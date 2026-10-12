#!/bin/bash
set -e
cp /opt/sysroot/usr/lib/libc.so /usr/arm-linux-gnueabi/lib/libc.so
CC=arm-linux-gnueabi-gcc
$CC --sysroot=/opt/sysroot -o hello hello.c
echo "== GLIBC 版本引用（应全部 ≤2.4）:"
arm-linux-gnueabi-readelf -V hello | grep -oE 'GLIBC_[0-9.]+' | sort -Vu
echo "== NEEDED:"; arm-linux-gnueabi-objdump -p hello | grep NEEDED
echo "== INTERP:"; arm-linux-gnueabi-readelf -l hello | grep interpreter
echo BUILD_OK
