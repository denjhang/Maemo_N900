#!/bin/bash
cat > /usr/arm-linux-gnueabi/lib/libc.so <<'LIBC'
/* GNU ld script - Fremantle sysroot (forced mapping) */
GROUP ( =/lib/libc.so.6 =/usr/lib/libc_nonshared.a  AS_NEEDED ( =/lib/ld-linux.so.3 ) )
LIBC
echo patched
