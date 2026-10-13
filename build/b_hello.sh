set -e
cd /tmp
arm-linux-gnueabi-g++ --sysroot=/opt/sysroot -O2 -fPIC -shared -nostdlib \
  -fno-threadsafe-statics -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -DNDEBUG \
  -I/mnt/e/working/zcode-projects/Maemo_N900/build -I/mnt/e/working/zcode-projects/Maemo_N900/build/imgui \
  /mnt/e/working/zcode-projects/Maemo_N900/research/apps/hello/hello.cpp -o hello_app.so
echo HELLO_BUILD_OK
md5sum hello_app.so
