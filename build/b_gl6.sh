#!/bin/bash
set -e
cd /mnt/e/working/zcode-projects/Maemo_N900/build
arm-linux-gnueabi-gcc --sysroot=/opt/sysroot -Wl,--sysroot=/opt/sysroot -nostartfiles -O2 -o gldemo6 gldemo6.c -lm -Wl,--no-as-needed ./libdl.so.2
echo BUILD_OK
