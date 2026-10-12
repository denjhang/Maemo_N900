#!/bin/bash
set -e
cd /mnt/e/working/zcode-projects/Maemo_N900/build
CC=arm-linux-gnueabi-gcc
$CC --sysroot=/opt/sysroot -Wl,--sysroot=/opt/sysroot -nostartfiles -o hello_ns hello_ns.c
echo "== GLIBC 版本引用:"
arm-linux-gnueabi-readelf -V hello_ns | grep -oE 'GLIBC_[0-9.]+' | sort -Vu
echo "== NEEDED:"; arm-linux-gnueabi-objdump -p hello_ns | grep NEEDED
echo "== INTERP:"; arm-linux-gnueabi-readelf -l hello_ns | grep interpreter
echo BUILD_OK
