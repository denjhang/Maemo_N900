#!/bin/bash
set -e
cd /mnt/e/working/zcode-projects/Maemo_N900/build
arm-linux-gnueabi-gcc --sysroot=/opt/sysroot -Wl,--sysroot=/opt/sysroot -nostartfiles \
  -I/mnt/e/working/zcode-projects/Maemo_N900/research/libao-src/libao-0.8.8/include \
  -o ao_test ao_test.c ./libao.so.2
echo BUILD_OK
