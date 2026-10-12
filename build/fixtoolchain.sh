#!/bin/bash
set -e
HOSTLIB=/usr/arm-linux-gnueabi/lib
[ -f "$HOSTLIB/libc.so.orig" ] || cp "$HOSTLIB/libc.so" "$HOSTLIB/libc.so.orig"
for f in crt1.o crti.o crtn.o crtbegin.o crtend.o; do
  if [ -f "$HOSTLIB/$f" ] && [ ! -f "$HOSTLIB/$f.orig" ]; then cp "$HOSTLIB/$f" "$HOSTLIB/$f.orig"; fi
done
# 用 Fremantle 的 libc 链接脚本 + 启动文件顶替
cp /opt/sysroot/usr/lib/libc.so "$HOSTLIB/libc.so"
cp /opt/sysroot/usr/lib/crt1.o "$HOSTLIB/crt1.o"
cp /opt/sysroot/usr/lib/crti.o "$HOSTLIB/crti.o" 2>/dev/null || true
cp /opt/sysroot/usr/lib/crtn.o "$HOSTLIB/crtn.o" 2>/dev/null || true
echo "toolchain patched (orig backed up as .orig)"
