#!/bin/bash
set -e
cd /mnt/e/working/zcode-projects/Maemo_N900/build
arm-linux-gnueabi-gcc --sysroot=/opt/sysroot -Wl,--sysroot=/opt/sysroot -nostartfiles \
  -O2 -o gldemo gldemo.c -lX11 -lEGL -lGLES_CM -lm
echo BUILD_OK
