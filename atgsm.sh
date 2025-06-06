#!/bin/sh

# 获取当前分钟数的个位数（0~9）
current_minute=$(date +%M | grep -o '.$')

# 检查条件：分钟个位数 <5 且 /bin/atgsm 存在
if [ "$current_minute" -lt 5 ] && [ -f "/bin/atgsm" ]; then
    echo "Waiting 30 seconds before starting atgsm..."
    sleep 30  # 休眠 30 秒
    echo "Starting atgsm..."
    /bin/atgsm &  # 后台执行
else
    echo "Skipping atgsm execution (minute=$current_minute or file missing)."
fi