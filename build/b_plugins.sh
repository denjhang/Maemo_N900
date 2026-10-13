set -e
cd /tmp
S=/opt/sysroot
F="--sysroot=$S -O2 -fPIC -shared -nostdlib -fno-threadsafe-statics -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -DNDEBUG -I/mnt/e/working/zcode-projects/Maemo_N900/build -I/mnt/e/working/zcode-projects/Maemo_N900/build/imgui"
for a in imguiset widgets tonegen vibra keys fonts; do
    arm-linux-gnueabi-g++ $F /mnt/e/working/zcode-projects/Maemo_N900/research/apps/$a/$a.cpp -o $a.so
done
echo PLUGINS_OK
for a in imguiset widgets tonegen vibra keys fonts; do md5sum $a.so; done
for a in imguiset widgets tonegen vibra keys fonts; do echo "---$a"; base64 -w0 $a.so; echo; done
