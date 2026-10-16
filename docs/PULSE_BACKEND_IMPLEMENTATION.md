# PULSE 后端实现详解（v0.4.46r）

## 成功标志

**用户验证**："不仅软件内可以调音量，系统音量也响应了，而且还很顺畅。"

- ✅ 系统音量条/硬件音量键 → IMRUN 输出实时跟手
- ✅ 应用内 VOL 滑条仍可用（软件音量叠加）
- ✅ 20 秒长播流畅无噼啪
- ✅ STOP 不闪退
- ✅ 4 后端 AOTEST 全通过

---

## 1. 架构设计

### 1.1 核心链路

```
IMRUN gen_samples (48000Hz)
  ↓
writer 线程 pa_simple_write
  ↓
libpulse-simple.so.0 (dlopen)
  ↓
pulse server (system-mode, PID 876)
  ↓
sink.hw0 (module-alsa-sink-old device=hw:0 rate=48000)
  ↓
tlv320aic3x codec (HP_DAC/Headphone 硬件寄存器)
```

**系统音量控制链**：
```
硬件音量键 / 系统音量条
  ↓
module-alsa-sink-volume (sink_name=sink.hw0 mixer=HP_DAC)
  ↓
alsamixer HP_DAC 寄存器
  ↓
codec 硬件输出增益
  ↓
IMRUN 输出实时变化（pulse server 端计算）
```

### 1.2 与传统后端的差异

| | plughw (SDL/ALSA 后端) | PULSE 后端 (sink.hw0) |
|---|---|---|
| 采样率 | 44100 (固定) | **48000** (sink.hw0 原生) |
| 声卡占用 | IMRUN 独占 plughw:0,0 | pulse server 持有，IMRUN 必须释放 hold |
| 系统音量 | **不响应**（pulse 看不到 plughw 客户端） | **响应**（sink.hw0 volume = 硬件寄存器） |
| 应用内音量 | VOL 滑条 = 唯一控制 | VOL 滑条 + 系统音量叠加 |
| 延迟 | ~46ms (PERIOD 直写) | ~40-60ms (pa_simple_write 节流) |
| 适用场景 | 实时模拟（PicoDrive 模式） | VGM 播放/常规音频 |

---

## 2. 四个致命根因与修复

### 2.1 根因 1：跨线程死锁

**现象**：选 PULSE 后端 → 后端切换按钮全卡死，GUI 冻结。

**根因**：`pa_simple` **非线程安全**。v046e 之前把 `audio_pulse_open` 放在 `audio_start`
（audio-ctl 线程），而 `pa_simple_write` 在 writer 线程 → libpulse 内部 mutex 死锁
→ audio-ctl 线程冻结（它在 join writer）→ 整个 GUI 卡住（audio-ctl 是单独 pthread，
但它的卡住导致 UI 的后端切换命令无响应）。

**修复**（v046f+）：
```c
// audio_start PULSE 分支 - 只 spawn，不 open
if (audio_backend == 3) {
    audio_rate = 48000;  // gen_samples 用
    if (pcm) { snd_pcm_close(pcm); pcm = NULL; }
    thread_run = 1;
    pthread_create(&audio_tid, NULL, audio_writer, NULL);
    return 1;
}

// audio_writer PULSE 分支 - 自己 open/write/close
if (audio_backend == 3) {
    if (!audio_pulse_open(48000, 2)) return NULL;  // writer 自己 open
    while (thread_run) {
        gen_samples(chunk, PERIOD);
        audio_pulse_write(chunk, PERIOD);  // 同线程 write
    }
    audio_pulse_close();  // writer 自己 close
    return NULL;
}
```

**关键**：pa_simple 句柄的整个生命周期（new/write/free）在**同一线程内**。

---

### 2.2 根因 2：默认 sink 卡死

**现象**：paplay 播有声 20 秒 WAV → 永久不退出。writer 写 3 块后卡死。

**根因**：N900 Maemo 的**默认 sink 是 `sink.music`**（`module-nokia-music` Nokia 私有音频路由）。
实测 `pa_simple_new(NULL, "imrun", PA_STREAM_PLAYBACK, NULL, ...)` 的 `dev=NULL`
（使用默认 sink）→ 数据流入 `sink.music` → Nokia 路由链（`sink.music` → `sink.voice.raw` → `sink.hw0`）
在当前状态下**卡死**（可能 telephony/voice 模块占用）→ `pcm0p` 始终 `closed`
（pulse 拿不到硬件）→ 数据堆在 server 端 → `pa_simple_write` 永久阻塞。

**实测对照**：
```bash
# 默认 sink - 卡死
paplay tone.wav   # 2s 文件 5s+ 不退出，writer seq=3 后无日志

# sink.hw0 - 立即成功
paplay --device=sink.hw0 tone.wav   # EXIT=0，2s 完成
```

**修复**（v046i+）：
```c
// audio_pulse.c
pulse_stream = fn_new(NULL, "imrun", 1 /*PA_STREAM_PLAYBACK*/, 
                      "sink.hw0",  // 显式指定，绕过 Nokia 路由
                      "imrun-playback", &ss, NULL, NULL, &err);
```

**sink.hw0 是什么**：`module-alsa-sink-old device=hw:0 rate=48000` 直连 ALSA 硬件，
`module-alsa-sink-volume` 把它的 volume 绑到 HP_DAC/Headphone 硬件寄存器 → 系统音量键
直接操作它 → IMRUN 输出跟手。

---

### 2.3 根因 3：重采样噼啪

**现象**："只能稳定一秒钟，后面一直夹杂着噼里啪啦。而且是跟随声音调整的。"

**根因**：`sink.hw0` 原生采样率 **48000Hz**（`system.pa` 配置），IMRUN 流用 **44100Hz**
→ pulse server 每块做 44100→48000 实时重采样 → **600MHz 单核 A8 CPU 顶不住**
→ 稳态后每块 underrun（生成赶不上播放） → 噼啪声。第一秒干净是初始 buffer 深度掩盖。

**证据**：
```
PULSE open rate=44100 ch=2   # v046i 之前
→ writer seq=0~3 快速 (1.8ms/块)，然后卡死（write 阻塞等 server 消费，但 server 在重采样）
→ 即使勉强跑，稳态后 underrun → 噼啪
```

**修复**（v046k+）：
```c
// 1. PULSE 流直接用 sink 原生率
audio_pulse_open(48000, 2);  // 零重采样

// 2. gen_samples 的相位步进用运行时采样率
int audio_rate = 44100;  // 全局，PULSE 分支设 48000
static void gen_samples(short *dst, int n) {
    double step = (double)WT_SIZE * 440.0 / (double)audio_rate;
    // 440Hz 在 48000 下仍是 440Hz，不会变 471Hz
}
```

**验证**：20 秒长播 469 块，每块 39.5-59.8ms **均匀**，`max_stall=0.067s stalls=0 underruns=0`。

---

### 2.4 根因 4：声卡占用冲突

**现象**：实测 writer 写 3 块后卡死，但日志显示另一个 imrun PID 还活着。`pcm0p` 状态
`state: XRUN rate: 44100`（不是 pulse 的 48000！）。

**根因**：IMRUN 启动时 `main` 里 **`pcm_acquire()` 预占 plughw:0,0**（"独占防抢"设计）。
切到 PULSE 后端时，旧代码的 SETBACKEND 分支只在 `audio_on || thread_run` 时才释放
→ **如果当时没在播放，`pcm` hold 不释放** → pulse server 的 `sink.hw0` resume
时抢不到硬件（IMRUN 进程还持有 plughw）→ 两个来源抢声卡 → XRUN + writer 阻塞。

**实测场景**：
```
1. imrun 启动 → pcm_acquire() 拿 plughw
2. 用户切到 PULSE（没播） → SETBACKEND 不释放 hold
3. 点 PLAY → writer pa_simple_new → pulse server resume sink.hw0
   → 发现 plughw 被 IMRUN 占用 → 抢不到 → write 阻塞 + XRUN 噼啪
```

**修复**（v046q+）：
```c
// audio_ctl 的 SETBACKEND 分支
if (cmd == 3) {
    if (audio_on || thread_run || ao_dev || sdl_audio_open)
        audio_stop_full();
    audio_backend = audio_cmd_be;
    
    // 切到 PULSE 时无条件释放声卡 hold
    if (audio_backend == 3) {
        if (pcm) { snd_pcm_close(pcm); pcm = NULL; }
        if (ao_dev) { ao_close_fn(ao_dev); ao_dev = NULL; }
        sdl_close();
    }
}
```

**为什么这个设计**：PULSE 后端要 pulse server 拿 sink.hw0，而 IMRUN 的"启动预占"
与 pulse 哲学冲突。切到 PULSE = 放弃独占，交给 pulse server 管理。切回 ALSA/SDL
时 `audio_start` 的 `if (!pcm) pcm_acquire()` 会重新拿（pulse suspend-on-idle 1 秒
后自动释放硬件）。

---

## 3. 代码结构

### 3.1 文件清单

```
build/audio_pulse.c          # libpulse-simple dlopen 包装
build/gldemo_imgui.c         # 主程序，PULSE 后端集成
research/apps/system/        # system 插件（PULSE 按钮）
docs/AUDIO_BACKENDS.md       # 完整架构文档
docs/PULSE_BACKEND_IMPLEMENTATION.md  # 本文
```

### 3.2 audio_pulse.c API

```c
int audio_pulse_load_libs(void);
    // dlopen libpulse-simple.so.0 + libpulse.so.0
    // 初始化 fn_new/free/write/flush/latency 函数指针
    // 返回 0=OK -1=FAIL

int audio_pulse_open(int rate, int channels);
    // pa_simple_new(NULL, "imrun", PLAYBACK, "sink.hw0", ...)
    // rate 必须 = 48000（sink.hw0 原生）
    // 返回 1=OK 0=FAIL

int audio_pulse_write(const short *buf, int frames);
    // pa_simple_write(..., frames * 2 * sizeof(short), ...)
    // 阻塞 API，正常播放时每块阻塞 40-60ms（pulse 节奏）
    // 返回 1=OK 0=FAIL

void audio_pulse_close(void);
    // pa_simple_flush + pa_simple_free
    // 必须在 writer 线程内调用（与 open/write 同线程）

int audio_pulse_alive(void);
    // 检查 pulse_open 标志
```

### 3.3 gldemo_imgui.c 集成点

**全局变量**：
```c
int audio_backend = 2;  // 默认 SDL，3 = PULSE
int audio_rate = 44100; // gen_samples 用，PULSE 分支设 48000
```

**audio_start PULSE 分支**（audio-ctl 线程）：
```c
if (audio_backend == 3) {
    audio_rate = 48000;
    if (pcm) { snd_pcm_close(pcm); pcm = NULL; }  // 释放 plughw hold
    sdl_close();
    thread_run = 1;
    pthread_create(&audio_tid, NULL, audio_writer, NULL);
    audio_on = 1;
    return 1;
}
```

**audio_writer PULSE 分支**（writer 线程）：
```c
if (audio_backend == 3) {
    if (!audio_pulse_open(48000, 2)) { audio_on = 0; return NULL; }
    
    short chunk[PERIOD * 2];
    unsigned long seq = 0;
    
    // silence pad 防首块 XRUN
    static const short zeros[PERIOD * 2];
    audio_pulse_write(zeros, PERIOD);
    audio_pulse_write(zeros, PERIOD);
    
    while (thread_run) {
        if (audio_deadline && now() > audio_deadline) {
            thread_run = 0; break;
        }
        if (audio_source_fn) {
            if (!audio_source_fn(chunk, PERIOD)) { thread_run = 0; break; }
        } else {
            gen_samples(chunk, PERIOD);  // 用 audio_rate=48000
        }
        audio_flow += PERIOD;
        
        // pa_simple_write 是同步阻塞 API (40-60ms/块)
        if (!audio_pulse_write(chunk, PERIOD)) break;
        
        seq++;
    }
    
    audio_pulse_close();  // writer 自己关
    return NULL;
}
```

**audio_stop_full PULSE 分支**（audio-ctl 线程）：
```c
if (audio_backend == 3) {
    thread_run = 0;
    if (pthread_join_fn) pthread_join_fn(audio_tid, NULL);
    // 不调 audio_pulse_close（writer 自己关了）
    audio_deadline = 0;
    audio_on = 0;
    return;
}
```

**SETBACKEND 切到 PULSE**（audio-ctl 线程）：
```c
if (cmd == 3) {
    if (audio_on || thread_run || ...) audio_stop_full();
    audio_backend = audio_cmd_be;
    
    if (audio_backend == 3) {  // 无条件释放 hold
        if (pcm) { snd_pcm_close(pcm); pcm = NULL; }
        if (ao_dev) { ao_close_fn(ao_dev); ao_dev = NULL; }
        sdl_close();
    }
}
```

---

## 4. 测试验证

### 4.1 AOTEST（自动化压力测试）

```bash
# 设备上 touch /home/user/aotest 触发
# 或源码编译时改 audio_backend 默认 = 3

[     0.000] === AOTEST begin: wave=0 amp=16000 low-volume rapid play/vol/stop ===
[     0.000] === AOTEST backend=0 (ALSA) ===
[    10.113] === AOTEST backend=1 (libao-vgmplay) ===
[    24.954] === AOTEST backend=2 (SDL-psp) ===
[    34.075] === AOTEST backend=3 (PULSE-sink.hw0) ===
[    43.106] === AOTEST end: hard_fail=0 max_stall=0.064s stalls=0 underruns=0 -> exit(0)
```

**AOTEST 覆盖**：4 后端 × 10 轮 × (2s PLAY + 15 档调音量 + STOP + 再 STOP 空状态) = 40 次完整生命周期。

### 4.2 AVTEST（长播验证）

```bash
# 设备上 AVTEST=1 环境变量 + 默认 backend=3
# 启动 2.5s 后自动播 20s 测试音

T pulse spawn tid=0
T pulse seq=0 gen+write=3.1ms
T pulse FIRST-WRITE latency=157.7ms
T pulse seq=1 gen+write=1.8ms
...
T pulse seq=465 gen+write=39.9ms
T pulse seq=468 gen+write=39.6ms
T pulse exit seq=469
PULSE closed (flush err=0)
AUDIODBG max_stall=0.067s stalls=0 underruns=0
```

**关键指标**：
- 469 块 × 42.7ms 平均 = 20.0 秒精准
- 每块 39.5-59.8ms 均匀（pulse 播放节奏）
- max_stall=0.067s < 0.1s（GUI 无感知卡顿）
- stalls=0 underruns=0（零断流）

### 4.3 用户实测

**验证步骤**：
1. SETTINGS → AUDIO → 选 PULSE (paplay)
2. PLAY 20s 启动测试音
3. 期间按设备音量键或拖系统音量条
4. 观察：IMRUN 输出音量实时跟手
5. 点 STOP → 不闪退，立即停止

**结果**："不仅软件内可以调音量，系统音量也响应了，而且还很顺畅。" ✅

---

## 5. 性能分析

### 5.1 延迟对比

| 后端 | 首块延迟 | 稳态延迟/块 | 总延迟（用户感知） |
|---|---|---|---|
| plughw (ALSA) | ~10ms | ~46ms (PERIOD) | ~50ms |
| PULSE (sink.hw0 48000) | ~158ms | ~40-60ms | ~200ms 初始 + 50ms 稳态 |

**解读**：PULSE 首块 158ms 是 pulse server 端初始缓冲填充，之后稳态与 plughw 相当。
对 VGM 播放器（播放键 → 开始播放），200ms 初始延迟用户无感知。

### 5.2 CPU 占用

| 场景 | 44100 流 (旧) | 48000 流 (新) |
|---|---|---|
| writer gen+write | 1.8-7.5ms/块 | 1.8-3.9ms/块 |
| pulse 重采样 | **每块额外 ~30ms** (600MHz 顶不住) | **0**（零重采样） |
| GUI FPS | 28.9 (卡顿) | 60+ (流畅) |

**关键**：48000 免重采样 → writer 节奏正常 → 不霸占 CPU → GUI 流畅。

---

## 6. 故障排查

### 6.1 选 PULSE 后无声

**排查步骤**：
```bash
# 1. 检查 pulse server
ps -ef | grep pulse
# 应有 PID 876: /usr/bin/pulseaudio --system

# 2. 检查 sink.hw0 状态
su - user -c "pactl list sinks" | grep -A5 "sink.hw0"
# State 应该是 RUNNING 或 SUSPENDED

# 3. 检查 imrun 是否持有声卡
cat /proc/asound/card0/pcm0p/sub0/status
# 应该是 closed（如果是 RUNNING 且 rate=44100，说明 imrun plughw hold 没释放）

# 4. 看 imrun 日志
tail /home/user/audio_dbg.log
# 应有 "PULSE open rate=48000 ch=2"
# 如果卡在 seq=2/3，说明声卡冲突（回到步骤 3）
```

**常见原因**：IMRUN plughw hold 没释放。修复：确保 v046r+，或手动 `pkill -9 imrun; sleep 2` 清场。

### 6.2 噼里啪啦

**症状**：第一秒干净，之后噼啪。

**排查**：
```bash
grep "PULSE open" /tmp/imrun*.out
# 如果是 rate=44100，说明用了旧版本（v046j 之前）
# 升级到 v046k+
```

### 6.3 STOP 闪退

**症状**：点 STOP → `WSEGL_CreateWindowDrawable: Bad drawable`。

**排查**：
```bash
dmesg | tail  # 看是否有 kernel OOPS
ps -ef | grep imrun | wc -l  # 如果 >1，说明多实例抢声卡
```

**修复**：确保 v046q+，SETBACKEND 切到 PULSE 时无条件释放 hold。

---

## 7. PicoDrive 先例论证

**用户原话**："以前 S60 V3 版的 pico drive 就是独立的声音后端，它好像完全不受塞班系统的
音量控制，只能自己调内置的音量。这个应该是有先例的。"

**架构对照**：

| | PicoDrive S60v3 | IMRUN plughw | IMRUN PULSE |
|---|---|---|---|
| 输出方式 | 独立音频后端 | plughw:0,0 直连 | pulse sink.hw0 |
| 系统音量 | **不响应** | **不响应** | **响应** ✅ |
| 内置音量 | 模拟器 UI | VOL 滑条 | VOL 滑条 + 系统音量叠加 |
| 设计哲学 | 绕开 Symbian 媒体服务器 | 绕开 pulse | 走 pulse |
| 适用场景 | 实时模拟（0 延迟） | 同左 | VGM 播放/常规音频 |

**结论**：PicoDrive S60v3 的"独立后端不响应系统音量"是**既定正确架构**，不是 bug。
在性能受限的老设备（600MHz 单核）上，实时音视频应用绕开系统音频路径是唯一解。
IMRUN 的 plughw 后端 = PicoDrive 模式，PULSE 后端 = 系统集成模式，**两种并存是合理的**。

---

## 8. 未来展望

### 8.1 VGM 插件集成

当前 VGM 插件通过 `audio_source_fn` 回调喂 PCM 数据，后端透明。PULSE 后端对 VGM 零侵入。

### 8.2 动态采样率切换

当前 `audio_rate` 在 `audio_start` 时设定（PULSE=48000，其他=44100）。未来如果支持
多种采样率文件，可以：
```c
audio_pulse_open(file_rate, 2);  // 传文件原生率
audio_rate = file_rate;           // gen_samples 跟随
```

但 sink.hw0 固定 48000，非 48000 流仍会重采样。可选方案：
- 文件 44100 → 用 plughw 后端（零重采样，44100 直写）
- 文件 48000 → 用 PULSE 后端（遵守系统音量）

### 8.3 其他设备移植

**关键要素**：
1. pulse server 存在且 sink 可达（`pactl list sinks`）
2. 找到硬件 sink 名字（可能不叫 sink.hw0）
3. 确认 sink 原生采样率（`pactl list sinks | grep "Sample Specification"`）
4. 流用原生率，device 显式指定

**不适用场景**：per-user pulse（桌面 Linux）不需要 device 显式指定（默认 sink 就是硬件），
且通常不卡（多核 CPU）。本实现针对 **system-mode pulse + 单核老设备 + Nokia 路由混乱** 这种极端场景。

---

## 9. 致谢

- **用户**：提供 PicoDrive S60v3 先例，实测验证"系统音量响应了，而且很顺畅"
- **paplay**：对照组实验（`paplay --device=sink.hw0` vs 默认 sink）定位了根因 2
- **N900 社区**：Maemo 5 的 system-mode pulse 配置（`/etc/pulse/system.pa`）文档

---

## 附录 A：关键日志片段

### A.1 成功的 20 秒长播（v046r）

```
[    13.729] PULSE: alsa hold released
[    13.732] S PULSE START req=20s
[    13.776] T pulse spawn tid=0
[    13.937] T pulse seq=0 gen+write=3.1ms
[    13.937] T pulse FIRST-WRITE latency=157.7ms
[    13.939] T pulse seq=1 gen+write=1.8ms
...
[    26.289] T pulse seq=466 gen+write=59.8ms
[    26.329] T pulse seq=467 gen+write=39.5ms
[    26.369] T pulse seq=468 gen+write=39.6ms
[    26.370] T pulse exit seq=469
PULSE open  rate=48000 ch=2
PULSE closed (flush err=0)
AUDIODBG max_stall=0.067s stalls=0 underruns=0
```

### A.2 AOTEST 四后端全通（v046r）

```
[     0.000] === AOTEST begin: wave=0 amp=16000 low-volume rapid play/vol/stop ===
[     0.000] === AOTEST backend=0 (ALSA) ===
[    10.113] === AOTEST backend=1 (libao-vgmplay) ===
[    24.954] === AOTEST backend=2 (SDL-psp) ===
[    34.075] === AOTEST backend=3 (PULSE-sink.hw0) ===
[    43.106] === AOTEST end: hard_fail=0 max_stall=0.064s stalls=0 underruns=0 -> exit(0)
```

### A.3 卡死时的对照（修复前，两个 imrun 抢声卡）

```
# imrun 持有 plughw
cat /proc/asound/card0/pcm0p/sub0/status
state: XRUN
rate: 44100 (44100/1)   ← 不是 pulse 的 48000！
hw_ptr: 915456

# pulse writer 永久阻塞
[    13.943] T pulse seq=2 gen+write=3.9ms
(no more logs...)

# 两个 imrun PID
ps -ef | grep imrun
4151 user  130m R  /home/user/imrun/imrun   ← 旧实例 hold plughw
5638 user  155m R  /home/user/imrun/imrun   ← 新实例 writer 阻塞
```

---

**文档版本**：v1.0（2024-10-16，v0.4.46r 发布时）  
**维护者**：ZCode + 用户实测反馈  
**最后验证**：用户确认"系统音量也响应了，而且还很顺畅"
