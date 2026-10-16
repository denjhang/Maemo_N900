#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""N900 X touch/mouse injector via XTEST (Python 2.5, ctypes).

Uses libXtst to inject a pointer move+click into the X server - the same path
the main program listens on (XSelectInput ButtonPress/Release + MotionNotify).
This bypasses the static xorg.conf input-device binding that made uinput
synthetic devices invisible to Xorg.

Usage:  python xtest_tap.py <x> <y> [hold_ms] [move_only]
"""
import ctypes
import sys
import time


def main():
    if len(sys.argv) < 3:
        print("用法: xtest_tap.py <x> <y> [hold_ms] [move_only]")
        return 1
    x, y = int(sys.argv[1]), int(sys.argv[2])
    hold_ms = int(sys.argv[3]) if len(sys.argv) > 3 else 80
    move_only = len(sys.argv) > 4 and sys.argv[4] == "move_only"

    x11 = ctypes.CDLL("libX11.so.6")
    xtst = ctypes.CDLL("libXtst.so.6")

    x11.XOpenDisplay.restype = ctypes.c_void_p
    dpy = x11.XOpenDisplay(None)
    if not dpy:
        print("XOpenDisplay 失败")
        return 2

    # int XTestFakeMotionEvent(Display*, int screen, int x, int y, unsigned long delay)
    xtst.XTestFakeMotionEvent.argtypes = [ctypes.c_void_p, ctypes.c_int,
                                          ctypes.c_int, ctypes.c_int,
                                          ctypes.c_ulong]
    # int XTestFakeButtonEvent(Display*, unsigned int button, Bool is_press, unsigned long delay)
    xtst.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                          ctypes.c_int, ctypes.c_ulong]

    # move to (x,y) first so ButtonPress carries a fresh position
    xtst.XTestFakeMotionEvent(dpy, -1, x, y, 0)
    x11.XFlush(dpy)
    time.sleep(0.15)

    if not move_only:
        xtst.XTestFakeButtonEvent(dpy, 1, 1, 0)   # press
        x11.XFlush(dpy)
        time.sleep(hold_ms / 1000.0)
        xtst.XTestFakeButtonEvent(dpy, 1, 0, 0)   # release
        x11.XFlush(dpy)

    x11.XCloseDisplay(dpy)
    return 0


if __name__ == "__main__":
    sys.exit(main())
