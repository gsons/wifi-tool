#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <sys/wait.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_AT_LEN 256
#define MAX_OUTPUT_LEN 2048

// 执行AT命令并捕获输出
void execute_at_command(const char *at_command, char *output, int output_size) {
    char safe_command[MAX_AT_LEN + 10] = {0};
    int i, j = 0;
    
    // 过滤非字母数字和允许的特殊字符
    for (i = 0; at_command[i] && j < MAX_AT_LEN; i++) {
        if (isalnum(at_command[i]) || 
            at_command[i] == '+' || 
            at_command[i] == '-' || 
            at_command[i] == '=' || 
            at_command[i] == '?' || 
            at_command[i] == ' ' || 
            at_command[i] == ',' || 
            at_command[i] == '.' || 
            at_command[i] == '"' ||
            at_command[i] == '\r' ||
            at_command[i] == '\n') {
            safe_command[j++] = at_command[i];
        }
    }
    safe_command[j] = '\0';
    
    if (strlen(safe_command) == 0) {
        snprintf(output, output_size, "Invalid AT command");
        return;
    }

    // 创建管道
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        snprintf(output, output_size, "Failed to create pipe");
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        snprintf(output, output_size, "Failed to fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    if (pid == 0) { // 子进程
        close(pipefd[0]); // 关闭读端
        
        // 重定向标准输出和错误到管道
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        
        // 执行命令
        execlp("gsmtty", "gsmtty", safe_command, (char *)NULL);
        exit(EXIT_FAILURE); // 如果execlp失败
    } else { // 父进程
        close(pipefd[1]); // 关闭写端
        
        // 读取命令输出
        int total_read = 0;
        int n;
        while ((n = read(pipefd[0], output + total_read, output_size - total_read - 1)) > 0) {
            total_read += n;
            if (total_read >= output_size - 1) {
                break;
            }
        }
        output[total_read] = '\0';
        
        close(pipefd[0]);
        
        // 等待子进程结束
        int status;
        waitpid(pid, &status, 0);
        
        // 清理输出中的特殊字符
        for (i = 0; output[i]; i++) {
            if (output[i] == '\n') output[i] = ' ';
            if (output[i] == '\r') output[i] = ' ';
            if (output[i] == '"') output[i] = '\'';
        }
    }
}

// 优化后的URL解码函数，专门处理AT命令
void url_decode_for_at(const char *src, char *dest, size_t max_len) {
    size_t i = 0, j = 0;
    
    while (src[i] != '\0' && j < max_len - 1) {
        if (src[i] == '%' && isxdigit(src[i+1]) && isxdigit(src[i+2])) {
            // 处理%编码字符
            char hex[3] = {src[i+1], src[i+2], '\0'};
            unsigned char val = (unsigned char)strtol(hex, NULL, 16);
            
            // 只允许解码特定字符
            if (isalnum(val) || 
                val == '+' || 
                val == '-' || 
                val == '=' || 
                val == '?' || 
                val == ' ' || 
                val == ',' || 
                val == '.' || 
                val == '"' ||
                val == '\'' ||
                val == ';' ||
                val == ':') {
                dest[j++] = val;
            }
            i += 3;
        } else if (src[i] == '+') {
            // 关键修改：保留+号而不是转换为空格
            dest[j++] = '+';
            i++;
        } else {
            // 直接复制允许的字符
            if (isalnum(src[i]) || 
                src[i] == '+' || 
                src[i] == '-' || 
                src[i] == '=' || 
                src[i] == '?' || 
                src[i] == ' ' || 
                src[i] == ',' || 
                src[i] == '.' || 
                src[i] == '"' ||
                src[i] == '\'' ||
                src[i] == ';' ||
                src[i] == ':') {
                dest[j++] = src[i];
            }
            i++;
        }
    }
    dest[j] = '\0';
}


// 从POST数据中提取AT命令
int parse_post_at_command(const char *post_data, char *at_command) {
    //printf("start decoded:%s\n",post_data);
    // 查找"command="或"at="参数
    const char *pattern1 = "command=";
    const char *pattern2 = "at=";
    const char *start = NULL;
    
    if (start = strstr(post_data, pattern1)) {
        start += strlen(pattern1);
    } else if ((start = strstr(post_data, pattern2))) {
        start += strlen(pattern2);
    } else {
        start = post_data;
    }
    
    if (!start) return 0;
    
    // 提取到下一个&或字符串结束
    const char *end = strchr(start, '&');
    if (!end) end = start + strlen(start);
    
    // 使用新的URL解码函数
    char decoded[MAX_AT_LEN];
    url_decode_for_at(start, decoded, end - start + 1);

    //printf("end decoded:%s\n",decoded);
    
    // 验证是否是AT命令
    if (strncmp(decoded, "AT", 2) == 0) {
        strncpy(at_command, decoded, MAX_AT_LEN - 1);
        at_command[MAX_AT_LEN - 1] = '\0';
        return 1;
    }
    
    return 0;
}

#include <fcntl.h>
#include <sys/stat.h>

// 处理静态文件请求
void handle_static_file(int client_socket, const char *path) {
    char full_path[BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "/etc_ro/web%s", path);

    if (full_path[strlen(full_path) - 1] == '/') {
        strcat(full_path, "index.html");
    }

    int fd = open(full_path, O_RDONLY);
    if (fd == -1) {
        const char *resp = "HTTP/1.1 404 Not Found\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Content-Type: text/html\r\n"
                           "\r\n"
                           "<html><body><h1>404 Not Found</h1></body></html>";
        write(client_socket, resp, strlen(resp));
        return;
    }

    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1) {
        close(fd);
        const char *resp = "HTTP/1.1 500 Internal Server Error\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Content-Type: text/html\r\n"
                           "\r\n"
                           "<html><body><h1>500 Internal Server Error</h1></body></html>";
        write(client_socket, resp, strlen(resp));
        return;
    }

    char *content_type = "text/html";
    if (strstr(full_path, ".css")) {
        content_type = "text/css";
    } else if (strstr(full_path, ".js")) {
        content_type = "application/javascript";
    } else if (strstr(full_path, ".png")) {
        content_type = "image/png";
    } else if (strstr(full_path, ".jpg") || strstr(full_path, ".jpeg")) {
        content_type = "image/jpeg";
    }

    char header[BUFFER_SIZE];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "\r\n",
             content_type, (long)file_stat.st_size);
    write(client_socket, header, strlen(header));

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        write(client_socket, buffer, bytes_read);
    }

    close(fd);
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    char at_command[MAX_AT_LEN] = {0};
    char command_output[MAX_OUTPUT_LEN] = {0};
    char response[BUFFER_SIZE];
    int bytes_read, content_length = 0;
    char *post_data = NULL;
    
    // 读取请求头
    bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        close(client_socket);
        return;
    }
    buffer[bytes_read] = '\0';

    // 检查是否是OPTIONS预检请求
    int is_options = strstr(buffer, "OPTIONS") != NULL;
    
    if (is_options) {
        // 处理预检请求
        const char *resp = 
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "Access-Control-Max-Age: 86400\r\n"  // 24小时
            "Content-Length: 0\r\n"
            "\r\n";
        write(client_socket, resp, strlen(resp));
        close(client_socket);
        return;
    }

    // 检查是否是GET请求
    if (strstr(buffer, "GET") != NULL) {
        char *path_start = strchr(buffer, ' ');
        if (path_start) {
            path_start++;
            char *path_end = strchr(path_start, ' ');
            if (path_end) {
                *path_end = '\0';
                handle_static_file(client_socket, path_start);
            }
        }
        close(client_socket);
        return;
    }

    // 检查是否是POST请求
    if (strstr(buffer, "POST") == NULL) {
        const char *resp = "HTTP/1.1 405 Method Not Allowed\r\n"
                          "Access-Control-Allow-Origin: *\r\n"
                          "Content-Type: application/json\r\n"
                          "Allow: POST, GET\r\n"
                          "\r\n"
                          "{\"status\":\"error\",\"message\":\"Only POST and GET requests are supported\"}";
        write(client_socket, resp, strlen(resp));
        close(client_socket);
        return;
    }

    // 获取Content-Length
    char *cl_header = strstr(buffer, "Content-Length:");
    if (cl_header) {
        content_length = atoi(cl_header + 15);
        if (content_length <= 0 || content_length > 1024) {
            const char *resp = "HTTP/1.1 400 Bad Request\r\n"
                             "Content-Type: application/json\r\n"
                             "\r\n"
                             "{\"status\":\"error\",\"message\":\"Invalid Content-Length\"}";
            write(client_socket, resp, strlen(resp));
            close(client_socket);
            return;
        }
    } else {
        const char *resp = "HTTP/1.1 411 Length Required\r\n"
                         "Content-Type: application/json\r\n"
                         "\r\n"
                         "{\"status\":\"error\",\"message\":\"Content-Length header required\"}";
        write(client_socket, resp, strlen(resp));
        close(client_socket);
        return;
    }
    
    // 读取POST数据
    post_data = malloc(content_length + 1);
    if (!post_data) {
        const char *resp = "HTTP/1.1 500 Internal Server Error\r\n"
                         "Content-Type: application/json\r\n"
                         "\r\n"
                         "{\"status\":\"error\",\"message\":\"Server memory error\"}";
        write(client_socket, resp, strlen(resp));
        close(client_socket);
        return;
    }
    
    char *body_start = strstr(buffer, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        int header_len = body_start - buffer;
        int body_len = bytes_read - header_len;
        memcpy(post_data, body_start, body_len);
        
        if (body_len < content_length) {
            bytes_read = read(client_socket, post_data + body_len, content_length - body_len);
            if (bytes_read != content_length - body_len) {
                free(post_data);
                const char *resp = "HTTP/1.1 400 Bad Request\r\n"
                                 "Content-Type: application/json\r\n"
                                 "\r\n"
                                 "{\"status\":\"error\",\"message\":\"Incomplete POST data\"}";
                write(client_socket, resp, strlen(resp));
                close(client_socket);
                return;
            }
        }
    } else {
        bytes_read = read(client_socket, post_data, content_length);
        if (bytes_read != content_length) {
            free(post_data);
            const char *resp = "HTTP/1.1 400 Bad Request\r\n"
                             "Content-Type: application/json\r\n"
                             "\r\n"
                             "{\"status\":\"error\",\"message\":\"Incomplete POST data\"}";
            write(client_socket, resp, strlen(resp));
            close(client_socket);
            return;
        }
    }
    post_data[content_length] = '\0';

    printf("Received POST data: %s\n", post_data);
    
    // 解析和执行AT命令
    if (parse_post_at_command(post_data, at_command)) {
        printf("Executing AT command: %s\n", at_command);
        execute_at_command(at_command, command_output, MAX_OUTPUT_LEN);
        
        // 构建JSON响应
        snprintf(response, sizeof(response),
                "HTTP/1.1 200 OK\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Type: application/json\r\n"
                "\r\n"
                "{\"status\":\"success\",\"command\":\"%s\",\"output\":\"%s\"}", 
                at_command, command_output);
    } else {
        snprintf(response, sizeof(response),
                "HTTP/1.1 200 OK\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Type: application/json\r\n"
                "\r\n"
                "{\"status\":\"error\",\"message\":\"No valid AT command found. "
                "Send POST data like: command=AT+COMMAND or at=AT+COMMAND\"}");
    }
    
    write(client_socket, response, strlen(response));
    free(post_data);
    close(client_socket);
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("AT Command HTTP server (POST) running on port %d\n", PORT);
    printf("Send POST requests with AT commands in the body\n");
    printf("Example: curl -X POST -d \"command=AT+CGMR\" http://<ip>:8080\n");

    while (1) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        handle_client(client_socket);
    }

    return 0;
}
