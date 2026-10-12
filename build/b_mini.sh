#!/bin/bash
set -e
cd /mnt/e/working/zcode-projects/Maemo_N900/build
arm-linux-gnueabi-gcc --sysroot=/opt/sysroot -Wl,--sysroot=/opt/sysroot -nostartfiles -shared \
  -Wl,-soname,libao.so.2 -Wl,--no-as-needed \
  -o mini_libao.so.2 mini_libao.c -lasound
arm-linux-gnueabi-readelf -d mini_libao.so.2 | grep -E 'NEEDED|SONAME'
echo BUILD_OK
