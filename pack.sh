#!/bin/bash

# 安全执行脚本，遇到错误立即退出
set -e

echo "开始打包和修改SquashFS文件系统..."

# 1. 打包squashfs-root目录为new.squashfs
echo "执行: mksquashfs squashfs-root/ new.squashfs -comp xz"
mksquashfs squashfs-root/ new.squashfs -comp xz

# 检查是否打包成功
if [ ! -f "new.squashfs" ]; then
    echo "错误: new.squashfs文件创建失败!"
    exit 1
fi

echo -e "\nSquashFS打包完成，文件大小: $(du -h new.squashfs | cut -f1)"

# 2. 使用Python脚本处理新的squashfs文件
echo -e "\n执行: python3 squashfs.py input mtd4 new.squashfs 0"
python3 squashfs.py input mtd4 new.squashfs 0

# 检查Python脚本是否执行成功
if [ $? -ne 0 ]; then
    echo "错误: python3脚本执行失败!"
    exit 1
fi


# 3. 删除临时文件new.squashfs
echo -e "\n执行: rm new.squashfs"
rm new.squashfs

echo -e "\n所有操作成功完成!"
echo "生成的文件: mtd4.new.bin"