#!/bin/bash
# 功能：将新固件刷入设备的 mtd4 分区

set -e  # 遇到错误立即退出

MTD4_DEVICE="/dev/mtdblock4" 

MTD4_BIN="mtd4.new.bin"

# 检查镜像文件是否存在
if [ ! -f "$MTD4_BIN" ]; then
    echo "错误：未找到镜像文件 $MTD4_BIN"
    exit 1
fi

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


# 推送镜像文件到设备
echo "推送 $MTD4_BIN 到设备临时目录..."
adb push "$MTD4_BIN" /tmp/

# 推送附带DD命令的busybox到设备
echo "推送 busybox 到设备临时目录..."
adb push busybox /tmp/

adb shell "chmod +x /tmp/busybox"

DD_CMD="/tmp/busybox dd if=/tmp/$MTD4_BIN of=$MTD4_DEVICE";


if confirm_action "是否确认刷入分区？将执行命令$DD_CMD"; then
    # 刷写分区（使用dd）
    echo "正在刷写 $MTD4_DEVICE..."
    adb shell "/tmp/busybox dd if=/tmp/$MTD4_BIN of=$MTD4_DEVICE"
else
    # 刷写分区（使用dd）
    echo "取消刷新操作"
    exit 1
fi


echo "刷写完成...."

echo "请等待设备重启，如果设备未自动重启,请手动重启设备以生效。"