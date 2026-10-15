#!/bin/bash
# imrun build: AO backend patch + AOTEST hook + --export-dynamic (demo.so
# plugin resolves audio_volume/audio_wave/etc from the main binary).
# Runs from the build dir; absolute source paths so cwd doesn't matter.
set -e
cd /mnt/e/working/zcode-projects/Maemo_N900/build
S=/opt/sysroot
F="--sysroot=$S -O2 -fsigned-char -fno-threadsafe-statics -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -DNDEBUG -I. -Iimgui"
B=/mnt/e/working/zcode-projects/Maemo_N900/build
arm-linux-gnueabi-gcc  $F -c $B/gldemo_imgui.c -o /tmp/gi.o
arm-linux-gnueabi-g++  $F -c $B/imgui_gles1.cpp -o /tmp/ig.o
for f in imgui imgui_draw imgui_tables imgui_widgets imgui_demo; do
    arm-linux-gnueabi-g++ $F -c $B/imgui/$f.cpp -o /tmp/$f.o
done
arm-linux-gnueabi-g++ --sysroot=$S -nostartfiles -nodefaultlibs \
    -Wl,--dynamic-linker,/lib/ld-linux.so.3 -Wl,--export-dynamic \
    /tmp/gi.o /tmp/ig.o /tmp/imgui.o /tmp/imgui_draw.o /tmp/imgui_tables.o /tmp/imgui_widgets.o /tmp/imgui_demo.o \
    $S/usr/lib/libstdc++.so.6 $B/libdl.so.2 -lm -lgcc -lgcc_s -lc \
    -o $B/gldemo_imguibin
echo BUILD_OK
md5sum $B/gldemo_imguibin