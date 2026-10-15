#!/bin/bash
# build all three probes. -nostartfiles is REQUIRED to dodge BFD elf32-arm
# assertion in Ubuntu 11.4 ld against this Maemo-Fremantle sysroot's libasound.
set -e
cd /mnt/e/working/zcode-projects/Maemo_N900/build
INC_AO="-I/mmnt/e/working/zcode-projects/Maemo_N900/research/libao-src/libao-0.8.8/include"
# (the build dir's libao.so.2 already provides the impl; we only need headers)
INC_AO="-I/mnt/e/working/zcode-projects/Maemo_N900/research/libao-src/libao-0.8.8/include"
FLAGS="-O1 --sysroot=/opt/sysroot -Wl,--sysroot=/opt/sysroot -nostartfiles"
arm-linux-gnueabi-gcc $FLAGS -o probe_a_alsa  probe_a_alsa.c       -lasound -lm
arm-linux-gnueabi-gcc $FLAGS $INC_AO -o probe_b_ao_dev probe_b_ao_dev.c       ./libao.so.2 -lasound -lm
arm-linux-gnueabi-gcc $FLAGS $INC_AO -o probe_c_ao_default probe_c_ao_default.c ./libao.so.2 -lasound -lm
echo BUILD_OK
ls -la probe_a_alsa probe_b_ao_dev probe_c_ao_default
