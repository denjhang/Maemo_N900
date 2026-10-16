#!/usr/bin/env python3
"""Automated visual test for N900 Flappy Bird.

Deploys main program + plugin, auto-opens the app, captures the framebuffer,
and analyzes pixels to verify the image actually rendered (not blank/black).

Usage:
    python test_visual.py                    # deploy + capture + analyze
    python test_visual.py --capture-only     # just grab current screen
"""
import base64
import io
import os
import sys
import time

import paramiko
import numpy as np
from PIL import Image

HOST = '192.168.101.108'
USER = 'root'
PASS = '123456'

FB_W, FB_H, FB_STRIDE = 800, 480, 4096   # /sys/class/graphics/fb0: 16bpp, stride 4096

# Reference colors from the Flappy Bird sprite set (approx, post-RGB565)
REF_BG_BLUE = (113, 197, 207)     # sky/background fallback + background-day dominant


def connect():
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username=USER, password=PASS,
              allow_agent=False, look_for_keys=False)
    return c


def grab_framebuffer(c):
    """Read /dev/fb0 as raw RGB565 bytes over the SSH channel."""
    _, o, _ = c.exec_command('dd if=/dev/fb0 bs=4096 count=480 2>/dev/null')
    o.channel.settimeout(30)
    data = b''
    while True:
        chunk = o.read(65536)
        if not chunk:
            break
        data += chunk
    if len(data) < FB_H * FB_STRIDE:
        raise RuntimeError('short framebuffer read: %d bytes' % len(data))
    arr = np.frombuffer(data, dtype=np.uint16).reshape(FB_H, FB_STRIDE // 2)[:, :FB_W]
    return arr


def rgb565_to_rgb(arr):
    r = ((arr >> 11) & 0x1F) * 255 // 31
    g = ((arr >> 5) & 0x3F) * 255 // 63
    b = (arr & 0x1F) * 255 // 31
    return np.dstack([r, g, b]).astype(np.uint8)


def analyze(arr, label='screen'):
    """Report whether the screen looks like a rendered game vs blank/black."""
    rgb = rgb565_to_rgb(arr)
    uniq = len(np.unique(arr))
    nonzero = float((arr != 0).mean())
    mean = float(arr.mean())

    # Count pixels close to the sky-blue background
    d = np.abs(rgb.astype(int) - np.array(REF_BG_BLUE)).sum(axis=2)
    sky = float((d < 60).mean())

    # Count strongly saturated green (pipe) and yellow (bird)
    r, g, b = rgb[:, :, 0].astype(int), rgb[:, :, 1].astype(int), rgb[:, :, 2].astype(int)
    green = float(((g > 120) & (g > r + 30) & (g > b + 30)).mean())
    yellow = float(((r > 180) & (g > 150) & (b < 120)).mean())

    print(f'--- {label} ---')
    print(f'  unique colors : {uniq}')
    print(f'  nonzero ratio : {nonzero:.3f}')
    print(f'  mean value    : {mean:.1f}')
    print(f'  sky-blue px   : {sky:.3f}')
    print(f'  green (pipe)  : {green:.4f}')
    print(f'  yellow (bird) : {yellow:.4f}')

    verdict = 'UNKNOWN'
    if nonzero < 0.5:
        verdict = 'BLANK/BLACK'
    elif sky > 0.15 and uniq > 200:
        verdict = 'RENDERED (texture background visible)'
    elif uniq > 100:
        verdict = 'PARTIAL (UI drawn, texture missing?)'
    return verdict, dict(uniq=uniq, nonzero=nonzero, sky=sky, green=green, yellow=yellow)


def deploy_and_run(c, main_bin, plugin_so):
    sftp = c.open_sftp()
    print('[*] stopping old process')
    c.exec_command('killall -9 gldemo_imguibin_export; sleep 1')
    time.sleep(2)
    c.exec_command('rm -f /home/user/gldemo_imguibin_export')
    time.sleep(1)

    print('[*] uploading main program:', main_bin)
    sftp.put(main_bin, '/home/user/gldemo_imguibin_export')
    sftp.chmod('/home/user/gldemo_imguibin_export', 0o755)

    print('[*] uploading plugin:', plugin_so)
    sftp.put(plugin_so, '/home/user/imrun/apps/flappy/app.so')

    print('[*] arming auto-open')
    f = sftp.open('/home/user/auto_open_app', 'w')
    f.write('flappy\n')
    f.close()
    sftp.close()

    print('[*] launching')
    c.exec_command('echo "" > /home/user/audio_dbg.log')
    _, o, _ = c.exec_command(
        'date -s @%d; export DISPLAY=:0 && nohup /home/user/gldemo_imguibin_export '
        '> /tmp/imrun.log 2>&1 &' % int(time.time()))
    o.channel.settimeout(10)
    try:
        o.read()
    except Exception:
        pass
    time.sleep(6)
    _, o, _ = c.exec_command('pidof gldemo_imguibin_export')
    pid = o.read().decode().strip()
    print('[*] pid =', pid or '(NOT RUNNING)')
    return pid


def main():
    capture_only = '--capture-only' in sys.argv
    c = connect()

    if not capture_only:
        here = os.path.dirname(os.path.abspath(__file__))
        deploy_and_run(c,
                       os.path.join(here, 'gldemo_v047_fixed'),
                       os.path.join(here, 'flappy_v7_fixed.so'))
        time.sleep(4)   # let the menu settle / textures upload

    print('[*] capturing framebuffer')
    arr = grab_framebuffer(c)
    rgb = rgb565_to_rgb(arr)
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, 'fb_capture.png')
    Image.fromarray(rgb).save(out)
    print('[*] saved', out)

    verdict, stats = analyze(arr)
    print()
    print('VERDICT:', verdict)

    # plugin-side log grep
    _, o, _ = c.exec_command('grep -E "FLAPPY|AUTO_OPEN|BOOTORDER" /home/user/audio_dbg.log | tail -15')
    print('\n--- device log ---')
    print(o.read().decode())
    c.close()
    return 0 if 'RENDERED' in verdict else 1


if __name__ == '__main__':
    sys.exit(main())
