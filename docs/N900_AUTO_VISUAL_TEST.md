# 自动化视觉验证与输入注入（N900）

**用途**: 无需人工看屏幕/点触摸，自动验证插件渲染与交互。

---

## 为什么需要

N900 没有截图工具（无 xwd/import/scrot），也没有 xdotool。反复"人肉看屏幕+手点"
调试效率极低。本目录下两个脚本把这两件事自动化。

---

## 1. 截图：读取 framebuffer

N900 的 `/dev/fb0` 可直接读：800×480，16bpp RGB565，stride 4096。

关键点：
- **设备无 `base64` 命令**，且 `/tmp` 很小（写 647KB 就满）→ 必须走 SSH 通道
  流式读 `dd if=/dev/fb0`，不能在设备上落盘再取。
- 本地用 numpy 解析 RGB565，按 stride 4096 reshape（不是 800×2）。

```python
i, o, e = ssh.exec_command('dd if=/dev/fb0 bs=4096 count=480 2>/dev/null')
o.channel.settimeout(30)
data = b''
while True:
    ch = o.read(65536)
    if not ch: break
    data += ch
arr = np.frombuffer(data, dtype=np.uint16).reshape(480, 2048)[:, :800]
```

脚本：`test_visual.py`（抓屏 + 像素分析 + 判定）

## 2. 输入注入：XTEST

主程序从 **X11 窗口事件**读触摸（`XSelectInput` 收 ButtonPress/Release + MotionNotify，
见 `gldemo_imgui.c`）。

- **uinput 合成的设备 Xorg 不认**：xorg.conf 静态绑定了 TSC2005(event3)，Xorg 不
  热插拔新输入设备。所以 uinput 那条路（`inject_tap.py`）走不通——留作记录。
- **正解是 XTEST**：`libXtst.so.6` 在 Maemo 上可用（`nm`/`strings` 看不到符号，
  但 ctypes 能 `getattr` 到 `XTestFakeMotionEvent`/`XTestFakeButtonEvent`）。

```python
x11  = ctypes.CDLL("libX11.so.6");  xtst = ctypes.CDLL("libXtst.so.6")
dpy  = x11.XOpenDisplay(None)
xtst.XTestFakeMotionEvent(dpy, -1, x, y, 0)   # 先移动，保证 press 带新坐标
xtst.XTestFakeButtonEvent(dpy, 1, 1, 0)       # press
xtst.XTestFakeButtonEvent(dpy, 1, 0, 0)       # release
```

脚本：`xtest_tap.py <x> <y> [hold_ms]`（需 `DISPLAY=:0`）

## 3. 判定"状态改变"的方法

不能只看"画面有差异"——桌面背景动画每帧都在变。要针对**游戏状态的特征**判断：
- MENU/GAMEOVER：窗口中央有半透明黑底衬的提示文字 → 该区域偏暗
- PLAYING：无提示 → 中央为天空蓝/内容
- 管道：统计"绿色且绿分量显著高于红/蓝"的像素数

---

## 完整验证流程

```bash
# 1. 部署（test_visual.py 内含）
cd build && python test_visual.py

# 2. 注入点击（设备上）
ssh root@192.168.101.108 'cd /home/user && DISPLAY=:0 python xtest_tap.py 430 300 80'

# 3. 再次抓屏对比
cd build && python test_visual.py --capture-only
```

---

## 已知限制

- XTEST 注入的是**指针移动+点击**，等价于触摸（主程序两者都当 core X pointer）。
- 截图是整屏（含桌面/任务栏），分析时要按窗口位置裁剪区域。
- `/dev/fb0` 是 16bpp；若换设备需重新确认 bpp/stride。
