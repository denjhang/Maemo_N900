#!/bin/bash
set -e
CC=arm-linux-gnueabi-gcc
SYS=/opt/sysroot
echo "== gcc 默认 libc.so 位置:"
$CC --sysroot=$SYS -print-file-name=libc.so
echo "== 显式 -L 到 Fremantle multiarch:"
$CC --sysroot=$SYS -L$SYS/usr/lib/arm-linux-gnueabi -L$SYS/lib/arm-linux-gnueabi -o hello hello.c
arm-linux-gnueabi-readelf -V hello | grep -oE 'GLIBC_[0-9.]+' | sort -Vu | tail -2
echo "== 各库实际链接位置:"
arm-linux-gnueabi-readelf -l hello 2>/dev/null | grep -i interp || true
arm-linux-gnueabi-objdump -p hello | grep NEEDED
