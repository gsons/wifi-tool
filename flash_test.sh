#!/bin/bash
# 功能：安全地将新固件刷入设备的 mtd4 分区
# 特性：
# 1. 自动验证镜像和分区大小
# 2. 刷写前自动备份原分区
# 3. 支持刷写后验证
# 4. 交互式确认流程

set -e  # 遇到错误立即退出

# 配置参数
MTD4_DEVICE="/dev/mtdblock4"
MTD4_NAME="mtd4"
MTD4_BIN="mtd4.new.bin"
BACKUP_BIN="mtd4_backup_$(date +%Y%m%d_%H%M%S).bin"
BUSYBOX="/tmp/busybox"
BLOCK_SIZE="64k"  # dd操作的块大小

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 检查ADB连接
check_adb() {
    if ! adb devices | grep -q "device$"; then
        echo -e "${RED}错误：未检测到ADB设备连接${NC}"
        exit 1
    fi
}

# 检查文件是否存在
check_file() {
    if [ ! -f "$1" ]; then
        echo -e "${RED}错误：未找到文件 $1${NC}"
        exit 1
    fi
}

# 交互确认
confirm_action() {
    local prompt="$1"
    while true; do
        read -p "$prompt [Y/N] " yn
        case $yn in
            [Yy]* ) return 0;;
            [Nn]* ) return 1;;
            * ) echo "请输入 Y 或 N";;
        esac
    done
}

# 获取MTD分区大小
get_mtd_size() {
    local size_hex=$(adb shell "cat /proc/mtd" | grep "$MTD4_NAME" | awk '{print $2}')
    if [ -z "$size_hex" ]; then
        echo -e "${RED}错误：无法获取 $MTD4_NAME 分区大小${NC}"
        exit 1
    fi
    echo $((0x$size_hex))
}

# 主流程
echo -e "${GREEN}=== MTD4 分区刷写工具 ===${NC}"
check_adb
check_file "$MTD4_BIN"
check_file "busybox"

# 获取大小信息
MTD4_SIZE=$(get_mtd_size)
BIN_SIZE=$(stat -c %s "$MTD4_BIN")

echo -e "\n${YELLOW}分区信息：${NC}"
echo -e "目标分区: $MTD4_DEVICE ($MTD4_NAME)"
echo -e "分区大小: $MTD4_SIZE 字节"
echo -e "镜像文件: $MTD4_BIN"
echo -e "镜像大小: $BIN_SIZE 字节"

# 大小验证
if [ "$BIN_SIZE" -gt "$MTD4_SIZE" ]; then
    echo -e "${RED}错误：镜像文件大小超过分区大小${NC}"
    echo -e "请检查镜像文件或尝试使用专业工具裁剪镜像"
    exit 1
fi

# 推送必要文件
echo -e "\n${YELLOW}推送文件到设备...${NC}"
adb push "$MTD4_BIN" /tmp/
adb push busybox "$BUSYBOX"
adb shell chmod +x "$BUSYBOX"

# 备份原分区
if confirm_action "是否要备份原分区内容?"; then
    echo -e "${YELLOW}正在备份分区...${NC}"
    adb shell "$BUSYBOX dd if=$MTD4_DEVICE of=/tmp/mtd4_backup.bin bs=$BLOCK_SIZE"
    adb pull /tmp/mtd4_backup.bin "$BACKUP_BIN"
    echo -e "${GREEN}备份已保存到: $BACKUP_BIN${NC}"
fi

# 刷写确认
if ! confirm_action "确认要刷写分区吗？此操作不可逆！"; then
    echo -e "${YELLOW}操作已取消${NC}"
    exit 0
fi

# 执行刷写
echo -e "\n${YELLOW}正在刷写分区...${NC}"
#adb shell "$BUSYBOX dd if=/tmp/$MTD4_BIN of=$MTD4_DEVICE bs=$BLOCK_SIZE"

# 验证刷写
if confirm_action "是否要验证刷写结果?"; then
    echo -e "${YELLOW}正在验证...${NC}"
    adb shell "$BUSYBOX dd if=$MTD4_DEVICE of=/tmp/mtd4_verify.bin bs=$BLOCK_SIZE"
    adb pull /tmp/mtd4_verify.bin /tmp/
    if ! cmp -s "/tmp/mtd4_verify.bin" "$MTD4_BIN"; then
        echo -e "${RED}验证失败：刷写内容与源镜像不匹配！${NC}"
        exit 1
    else
        echo -e "${GREEN}验证通过：分区刷写成功${NC}"
    fi
fi

echo -e "\n${GREEN}操作完成！建议重启设备使更改生效。${NC}"