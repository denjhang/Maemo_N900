#!/bin/bash
# gldemo_imgui build recipe (GLDEMO_IMGUI.md): single WSL call, /tmp is volatile
set -e
cd /mnt/e/working/zcode-projects/Maemo_N900/build
S=/opt/sysroot
F="--sysroot=$S -O2 -fsigned-char -fno-threadsafe-statics -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -DNDEBUG -I. -Iimgui"
arm-linux-gnueabi-gcc  $F -c gldemo_imgui.c   -o /tmp/gi.o
arm-linux-gnueabi-g++  $F -c imgui_gles1.cpp  -o /tmp/ig.o
for f in imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/imgui_demo.cpp; do
    arm-linux-gnueabi-g++ $F -c $f -o /tmp/$(basename $f .cpp).o
done
arm-linux-gnueabi-g++ --sysroot=$S -nostartfiles -nodefaultlibs \
    -Wl,--dynamic-linker,/lib/ld-linux.so.3 \
    /tmp/gi.o /tmp/ig.o /tmp/imgui.o /tmp/imgui_draw.o /tmp/imgui_tables.o /tmp/imgui_widgets.o /tmp/imgui_demo.o \
    $S/usr/lib/libstdc++.so.6 ./libdl.so.2 -lm -lgcc -lgcc_s -lc \
    -o gldemo_imguibin
echo BUILD_OK
