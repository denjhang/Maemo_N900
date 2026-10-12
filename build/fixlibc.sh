#!/bin/bash
set -e
echo "== 原版 Fremantle libc.so 内容:"
cat /opt/sysroot/usr/lib/libc.so
cat > /usr/arm-linux-gnueabi/lib/libc.so <<'LIBC'
/* GNU ld script - Fremantle sysroot version */
GROUP ( /opt/sysroot/lib/libc.so.6 /opt/sysroot/usr/lib/libc_nonshared.a  AS_NEEDED ( /opt/sysroot/lib/ld-linux.so.3 ) )
LIBC
echo "== 已改写为 sysroot 相对版"
