# VGM 插件紧急修复 - v2

## 问题描述（用户实测）

1. **音调升高** - PULSE 后端下播放 VGM 音高不对
2. **音质很差** - 有噼啪、失真
3. **STOP 崩溃** - 点击 STOP 按钮导致整个 IMRUN 界面卡死（signal 6 SIGABRT）

## 根因分析

### 问题 1 & 2：采样率时序错误

**错误的时序**：
```
1. 用户点 VGM 文件 → vgm_start()
2. vgm_start 读 audio_rate (此时还是默认 44100!)
3. vgm_start 调 ay_set_sample_rate(44100)
4. vgm_start 调 audio_ui_start()
5. audio_ui_start → audio_start (PULSE分支) 设 audio_rate=48000
6. 但 AY8910 已经用 44100 初始化了！
7. VGM 生成 44100 数据 → PULSE 以 48000 播放 → 音高升 8.8% + 音质扭曲
```

**证据**：
```
[ 22964.687] C ctl: STOP
[ 22964.703] T pulse exit seq=320
[ 22964.710] FATAL signal 6 (segv=11 bus=7 abort=6 fpe=8)
```

### 问题 3：STOP 崩溃（竞态）

**旧代码**（`imapp_destroy`）：
```cpp
audio_source_fn = 0;      // 先摘钩
audio_ui_stop();          // 再停播 (join writer)
```

**竞态窗口**：
- `audio_source_fn = 0` 后，writer 线程可能还在运行
- writer 调用 `audio_source_fn(chunk, PERIOD)` → NULL 指针？
- 或者 `pa_simple_free` 时 writer 还在 `pa_simple_write` → 死锁/崩溃

## 修复方案

### 修复 1：采样率时序 - 延迟到 vgm_render

**新增全局标志**：
```cpp
static int vgm_rate_set = 0;  // vgm_start 清零, vgm_render 第一次设
```

**vgm_render 第一次调用时设置**：
```cpp
extern "C" int vgm_render(short *dst, int frames) {
    if (!vgm_rate_set) {
        ay_set_sample_rate(audio_rate);  // 此时 audio_rate 已被 audio_start 设置
        vgm_rate_set = 1;
    }
    // ... 渲染逻辑
}
```

**vgm_start 重置标志**：
```cpp
vgm_rate_set = 0;  // 每次播放都重新读 audio_rate
```

**正确的时序**：
```
1. vgm_start() 设 vgm_rate_set=0, 挂 audio_source_fn
2. audio_ui_start() → audio_start (PULSE分支) 设 audio_rate=48000
3. writer 线程第一次调 vgm_render()
4. vgm_render 读 audio_rate (此时已是 48000!) → ay_set_sample_rate(48000)
5. VGM 生成 48000 数据 → PULSE 以 48000 播放 → 音高正确 ✅
```

### 修复 2：STOP 崩溃 - 先停后摘

**新代码**（`imapp_destroy`）：
```cpp
vgm_playing = 0;      // 1. 让 vgm_render 立即返回 0 (EOF)
audio_ui_stop();      // 2. join writer 线程 (此时 writer 已退出)
audio_source_fn = 0;  // 3. writer 已退出,安全摘钩
```

**关键**：`audio_ui_stop()` 会 `pthread_join(audio_tid)` 等待 writer 退出。
只有 writer 完全退出后，才摘 `audio_source_fn`，确保无竞态。

## 编译

**正确的编译命令**（单文件 `-nostdlib`）：
```bash
wsl -e bash -c '
S=/opt/sysroot
B=/mnt/e/working/zcode-projects/Maemo_N900
arm-linux-gnueabi-g++ \
  --sysroot=$S -O2 -fPIC -shared -nostdlib \
  -fno-threadsafe-statics -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -DNDEBUG \
  -I$B/build -I$B/build/imgui \
  $B/research/apps/vgmplayer/vgm.cpp \
  -o $B/build/vgmplayer_v2.so
'
```

**关键**：
- **`-nostdlib`**（不是 `-lz`）
- 单文件编译（`vgm.cpp` 里 `#include "ay8910.c"`）
- 参考：`docs/PLUGIN_DEV_GUIDE.md` 第 4 节

## 部署

```bash
# 1. 停旧 imrun
ssh root@192.168.101.108
pkill -9 -f "imrun/imrun"
rm -f /tmp/imrun.lock

# 2. 部署新插件
scp build/vgmplayer_v2.so root@192.168.101.108:/home/user/imrun/plugins/vgmplayer.so

# 3. 重启 imrun
su - user -c 'export DISPLAY=:0 XAUTHORITY=/home/user/.Xauthority DBUS_SESSION_BUS_ADDRESS=unix:abstract=/tmp/dbus-7Yk7lLxfhR,guid=f17a639b69a48bf06b03481400000013; nohup /home/user/imrun/imrun >/tmp/imrun_vgm_v2.out 2>&1 &'
exit
```

## 测试步骤

1. IMRUN 启动后，SETTINGS→AUDIO 选 **PULSE (paplay)**
2. VGM 标签页选一首 VGM 播放
3. 验证：
   - ✅ 音高正确（440Hz 不再变 480Hz）
   - ✅ 音质干净（无噼啪、无失真）
   - ✅ 系统音量键实时响应
4. 点 **STOP** 按钮
   - ✅ 立即停止，不崩溃
   - ✅ IMRUN 界面正常响应

5. 切回 **SDL** 后端，再播一次
   - ✅ 音高仍正确（ay_set_sample_rate 自动读 44100）

## 修改的文件

- `research/apps/vgmplayer/vgm.cpp`:
  - 新增 `vgm_rate_set` 标志
  - `vgm_render` 第一次调用时设采样率
  - `vgm_start` 重置标志
  - `imapp_destroy` 先停后摘

- `research/apps/vgmplayer/ay8910.c`:
  - 无变化（v1 版本已改 `ay_sample_rate` 变量化）

## 产物

- **文件**: `build/vgmplayer_v2.so`
- **MD5**: `15e473edec4f972e0500c2a5dee34b56`
- **大小**: ~18KB

---

**状态**: Ready to deploy
**优先级**: HIGH（修复崩溃 + 音高错误）
**风险**: LOW（只改时序和摘钩顺序，核心逻辑不变）
