#!/bin/bash

arm-linux-gnueabi-gcc -static -o atgsm http.c

adb shell "mount -o remount,rw /"

adb push atgsm /bin

#adb push www/at.html /etc_ro/web

adb shell "atgsm"



