#!/bin/bash
set -e
CC=arm-linux-gnueabi-gcc
SYS=/opt/sysroot
$CC --sysroot=$SYS -Wl,--dynamic-linker,/lib/ld-linux.so.3 -o hello hello.c
arm-linux-gnueabi-readelf -V hello | grep -oE 'GLIBC_[0-9.]+' | sort -Vu
echo "== NEEDED:"
arm-linux-gnueabi-objdump -p hello | grep NEEDED
echo "== INTERP:"
arm-linux-gnueabi-readelf -l hello | grep interpreter
echo BUILD_OK
