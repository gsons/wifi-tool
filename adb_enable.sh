#!/bin/bash

# 定义常量
ip="192.168.100.1"
#password="MM888@Qiruizhilian20241202"
password="159258@Qiruizhilian20241202"

wait_time_after_curl=5


# 执行curl请求
echo "正在执行curl请求..."
response=$(curl -s -X POST "http://$ip/reqproc/proc_post" \
  -H "cache-control: no-cache" \
  -H "content-type: application/x-www-form-urlencoded; charset=UTF-8" \
  -H "pragma: no-cache" \
  -H "x-requested-with: XMLHttpRequest" \
  --referer "http://$ip/index.html" \
  --data-raw "goformId=SET_DEVICE_MODE&debug_enable=1&password=$password&valid_without_reboot=1")

# 输出响应结果
echo "请求响应结果:"
echo "$response"

# 暂停5秒
echo "脚本将暂停 $wait_time_after_curl 秒..."
sleep $wait_time_after_curl

echo "$wait_time_after_curl 秒已过，脚本执行完毕"