#!/bin/bash
set -e
cd /mnt/e/working/zcode-projects/Maemo_N900/build
arm-linux-gnueabi-gcc --sysroot=/opt/sysroot -shared -fPIC -Wl,-soname,libglxshim.so -o glxshim.so glxshim.c
echo SHIM_OK
