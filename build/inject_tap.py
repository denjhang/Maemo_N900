#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""N900 touch injector via uinput (Python 2.5, no external deps).

Creates a synthetic absolute-positioning touchscreen, then injects a tap at
(x, y) in screen coordinates (0..800, 0..480). Used by the automated test to
verify tap handling without a human finger.

Usage:  python inject_tap.py <x> <y> [hold_ms]
"""
import ctypes
import struct
import sys
import time
import fcntl
import os

# --- uinput / input event ABI (Linux) ---
UI_SET_EVBIT   = 0x40045564
UI_SET_KEYBIT  = 0x40045565
UI_SET_ABSBIT  = 0x40045567
UI_DEV_CREATE  = 0x5501
UI_DEV_DESTROY = 0x5502

EV_SYN = 0x00
EV_KEY = 0x01
EV_ABS = 0x03
SYN_REPORT = 0

BTN_TOUCH = 0x14a
ABS_X = 0x00
ABS_Y = 0x01

# N900 touchscreen is 800x480 (matches framebuffer)
SCREEN_W, SCREEN_H = 800, 480


def emit(fd, ev_type, code, value):
    # struct input_event on 32-bit ARM: tv_sec, tv_usec, type, code, value
    os.write(fd, struct.pack("iiHHi", 0, 0, ev_type, code, value))


def main():
    if len(sys.argv) < 3:
        print("用法: inject_tap.py <x> <y> [hold_ms]")
        return 1
    x, y = int(sys.argv[1]), int(sys.argv[2])
    hold_ms = int(sys.argv[3]) if len(sys.argv) > 3 else 60

    fd = os.open("/dev/input/uinput", os.O_WRONLY | os.O_NONBLOCK)
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_ABS)
    fcntl.ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH)
    fcntl.ioctl(fd, UI_SET_ABSBIT, ABS_X)
    fcntl.ioctl(fd, UI_SET_ABSBIT, ABS_Y)

    # struct uinput_user_dev (kernel order):
    #   char name[80]; input_id{bus,vend,prod,ver}; u32 ff_effects_max;
    #   s32 absmax[64]; absmin[64]; absfuzz[64]; absflat[64];
    name = "imrun-test-touch"
    dev = struct.pack("80s", name)
    dev += struct.pack("HHHH", 0x03, 0x1234, 0x5678, 1)   # input_id
    dev += struct.pack("I", 0)                             # ff_effects_max
    absmax = [0] * 64
    absmin = [0] * 64
    absfuzz = [0] * 64
    absflat = [0] * 64
    absmax[ABS_X] = SCREEN_W
    absmax[ABS_Y] = SCREEN_H
    dev += struct.pack("64i", *absmax)
    dev += struct.pack("64i", *absmin)
    dev += struct.pack("64i", *absfuzz)
    dev += struct.pack("64i", *absflat)
    os.write(fd, dev)
    fcntl.ioctl(fd, UI_DEV_CREATE)
    time.sleep(1)   # let udev/X register the device

    # press
    emit(fd, EV_ABS, ABS_X, x)
    emit(fd, EV_ABS, ABS_Y, y)
    emit(fd, EV_KEY, BTN_TOUCH, 1)
    emit(fd, EV_SYN, SYN_REPORT, 0)
    time.sleep(hold_ms / 1000.0)
    # release
    emit(fd, EV_KEY, BTN_TOUCH, 0)
    emit(fd, EV_SYN, SYN_REPORT, 0)
    time.sleep(0.1)

    fcntl.ioctl(fd, UI_DEV_DESTROY)
    os.close(fd)
    return 0


if __name__ == "__main__":
    sys.exit(main())
