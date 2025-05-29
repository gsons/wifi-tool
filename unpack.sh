#!/bin/bash

# 安全执行脚本，遇到错误立即退出
set -e

echo "开始执行命令序列..."

# 1. 执行Python脚本处理squashfs
echo "执行: python3 squashfs.py output mtd4"
python3 squashfs.py output mtd4

# 2. 删除squashfs-root目录（如果存在）
if [ -d "squashfs-root" ]; then
    echo "执行: rm -rf squashfs-root"
    rm -rf squashfs-root
else
    echo "squashfs-root目录不存在，跳过删除"
fi

# 3. 解压squashfs文件系统
echo "执行: unsquashfs mtd4_temp"
unsquashfs mtd4_temp

# 4. 删除临时文件
if [ -f "mtd4_temp" ]; then
    echo "执行: rm mtd4_temp"
    rm mtd4_temp
else
    echo "mtd4_temp文件不存在，跳过删除"
fi

echo "所有命令执行完成！"