set -e
cd /tmp
S=/opt/sysroot
B=/mnt/e/working/zcode-projects/Maemo_N900/build
F="--sysroot=$S -O2 -fsigned-char -fno-threadsafe-statics -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -DNDEBUG -I$B -I$B/imgui"
arm-linux-gnueabi-gcc  $F -c gldemo_imgui.c   -o gi.o
arm-linux-gnueabi-g++  $F -c imgui_gles1.cpp  -o ig.o
for f in imgui imgui_draw imgui_tables imgui_widgets imgui_demo; do
    src=$B/imgui/$f.cpp; [ -f /tmp/$f.cpp ] && src=/tmp/$f.cpp
    arm-linux-gnueabi-g++ $F -c $src -o $f.o
done
arm-linux-gnueabi-g++ --sysroot=$S -nostartfiles -nodefaultlibs     -Wl,--dynamic-linker,/lib/ld-linux.so.3 -Wl,--export-dynamic     gi.o ig.o imgui.o imgui_draw.o imgui_tables.o imgui_widgets.o imgui_demo.o     $S/usr/lib/libstdc++.so.6 $B/libdl.so.2 -lm -lgcc -lgcc_s -lc     -o gldemo_imguibin
echo BUILD_OK
md5sum gldemo_imguibin
