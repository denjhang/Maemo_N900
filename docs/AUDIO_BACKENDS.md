# IMRUN 音频后端：实现、调试史、当前默认

> 本文档是 `docs/AUDIO_HANDOFF.md`（v0.4.42→v0.4.43 调查归档）之后的延续。
> 截至 v0.4.45（2026-10-16）三后端全通、SDL 为默认、`SETTINGS→AUDIO` 标签并入测试。
> 每条都标注源码出处，不靠推测。

## 0. 当前实装状态（v0.4.46）

| 后端 | 默认？ | 代码路径 | 实际发声 | 稳定性 |
|---|---|---|---|---|
| 0 = ALSA plughw | 否 | `gldemo_imgui.c:955-1004` `audio_start` → `snd_pcm_writei` 直写 `plughw:0,0` | 真实 ALSA | 良好 |
| 1 = libao (vgmplay) | 否 | 同上 0 路径分支 `if (audio_backend == 1)` → 走 `/home/user/imrun/audiolib/libao.so.2` (61KB 完整版) | 真实 libao + alsa09 | 良好（已修 pulse cookie 死路） |
| 2 = SDL (ppsspp) | **是** | `gldemo_imgui.c:981` 注释：`/* backend 0 (ALSA) and backend 2 (SDL/PSP) both go through ALSA plughw */` | **实际走 plughw 路径** | 最稳（用户验证：选 SDL 长播不断流） |
| 3 = PULSE (paplay) | 否 | `audio_pulse.c` dlopen `libpulse-simple.so.0` → `pa_simple_write` → pulse sink | **真实 pulse sink** | AOTEST 快速压力测不适用（pa_simple 同步阻塞），正常长播 OK（paplay 30s 流畅验证） |

**v0.4.46 新增 PULSE 后端**（backend=3）：paplay 风格，dlopen `libpulse-simple.so.0` + `libpulse.so.0`，
`pa_simple_new(NULL, "imrun", PA_STREAM_PLAYBACK, "sink.hw0", ...)` 连 pulse server，
writer 线程 `pa_simple_write` 喂字节流。**输出流入 pulse sink → 系统栏/硬件键/amixer 改 sink vol
时 IMRUN 输出自动跟手**（这是"遵守系统音量"的传统方法）。

**4 个关键根因（都踩过坑，务必记住）**：

1. **`pa_simple` 非线程安全** —— open/write/close 必须**全在同一个线程内**（writer）。
   之前把 `audio_pulse_open` 放在 `audio_start`（audio-ctl 线程）、`write` 放在 writer 线程
   → libpulse 内部死锁 → 整个 audio-ctl 线程冻结 → 后端切换按钮全卡死。
   修法：writer 线程自己 open/write/close，audio_start 只 `thread_run=1 + spawn`。

2. **必须显式指定 `device="sink.hw0"`，不能用默认 sink** —— N900 Maemo 的默认 sink 是
   `sink.music`（`module-nokia-music`，Nokia 私有音频路由模块）。**实测 `paplay` 播有声数据到
   默认 sink 会永久卡死**（`pcm0p` 始终 `closed`，数据堆在 server 端，`pa_simple_write` 永久阻塞）。
   而 `paplay --device=sink.hw0`（直连 ALSA hw:0）**立即成功、正常退出**。
   实测对比：
   ```
   paplay (默认 sink=sink.music)      -> 卡死 (2s 文件 5s+ 不退出)
   paplay --device=sink.hw0           -> EXIT=0 立即成功
   paplay --device=sink.null          -> EXIT=0 立即成功
   ```
   所以 PULSE 后端 `pa_simple_new` 的 dev 参数必须是 `"sink.hw0"`。

3. **流采样率必须 = sink 原生率 48000（免重采样）** —— `sink.hw0` 原生 48000
   （`system.pa` `module-alsa-sink-old ... rate=48000`）。如果流用 44100，pulse server 要做
   44100→48000 重采样，**600MHz A8 顶不住 → 稳态后每块 underrun → "噼里啪啦"**
   （用户实测："只能稳定一秒钟，后面一直夹杂噼里啪啦"——第一秒是 buffer 深度，之后开始抽）。
   修法：`audio_pulse_open(48000, 2)`，且 `gen_samples` 的相位步进用运行时的 `audio_rate`
   变量（PULSE=48000，其他后端=44100），否则音高会偏（440→471Hz）。

4. **切到 PULSE 时必须无条件释放声卡 hold** —— IMRUN 启动时 `main` 里 `pcm_acquire()`
   预占 plughw:0,0（"IMRUN 独占声卡防别人抢"的设计）。但 PULSE 后端要 pulse server 拿
   `sink.hw0`。**如果切到 PULSE 时没在播放，旧的 SETBACKEND 分支只在
   `audio_on||thread_run||...` 时才 `audio_stop_full()`，hold 不释放** → pulse server
   resume sink.hw0 抢不到硬件 → `pa_simple_write` 永久阻塞 + 已写块 XRUN 噼啪。
   修法：SETBACKEND 切到 3 时无条件 `snd_pcm_close(pcm); pcm=NULL`。

**这两个 bug 的现象与根因对照**：
| 用户看到的现象 | 真实根因 |
|---|---|
| "只能稳定一秒钟，后面噼里啪啦" | 44100 流被重采样到 48000，CPU 顶不住 → underrun |
| "跟随声音调整"（音量键有效） | pulse 链路本身是通的（volume 控制正常），只是数据流 underrun |
| "点击 stop 直接闪退" | 声卡被两个来源抢占（IMRUN hold + pulse sink），XRUN 后 GL/X 层崩 |

**sink.hw0 的 volume = 系统音量**：`module-alsa-sink-volume sink_name=sink.hw0 mixer=HP_DAC device=hw:0`
把 sink.hw0 的 volume 绑定到 tlv320aic3x 的 HP_DAC/Headphone 硬件寄存器。所以调 sink.hw0 的音量
= 调硬件 codec 寄存器 = 系统音量条/硬件音量键的效果。IMRUN 走 sink.hw0 就能被系统音量控制。

**验证结果**（v0.4.46r，修复后）：
- PULSE 48000 20s 长播（AVTEST）：469 块 × ~46ms 均匀，`max_stall=0.067s stalls=0 underruns=0`，
  `PULSE closed (flush err=0)` 干净关闭
- 4 后端 AOTEST 全通：`hard_fail=0 max_stall=0.064s stalls=0 underruns=0`
- 每块 `pa_simple_write` 阻塞 40-60ms（正常 pulse 播放节奏，不是死锁）

**AOTEST 覆盖四后端**（0/1/2/3）。

**关键事实**：SDL 后端当前**不是** SDL 原生 pulse callback，而是 v0.4.44 改成复用
ALSA plughw 路径——`audio_start` 末尾 `if (audio_backend == 0 || audio_backend == 2) { pcm_acquire(); snd_pcm_prepare; pthread_create(audio_writer); }`。
SDL 的 `sdl_open`/`sdl_close` 框架仍保留（dlopen libSDL-1.2.so.0 + 符号桩）但 callback
路径已废，因为实测 `sdl_cb_count=0`——SDL 的 pulse driver callback 线程根本不起来。

**所以「选 SDL 按钮 = 走 plughw 路径」，这就是它「最稳定」的本质**：plughw 直连 ALSA，
绕开 pulse，IOCTL 立即返回，没有 pulse server 调度延迟、没有 cookie 授权问题。

**AOTEST 验证**（v0.4.45b 部署后立即跑，3 后端×9 轮 PLAY/vol/STOP 循环）：
```
[     0.000] === AOTEST begin: wave=0 amp=16000 low-volume rapid play/vol/stop ===
[    34.609] === AOTEST end: hard_fail=0 max_stall=0.141s stalls=2 underruns=0 -> exit(0) ===
```
`hard_fail=0` + `underruns=0` 是核心指标（无断流、无 XRUN）。`stalls=2 max_stall=0.141s`
是 9 轮初始化阶段瞬时尖刺，不是回归。

## 1. 切换位置：SETTINGS → AUDIO 标签

v0.4.45 起，音频设置从 `demo.cpp` 迁入 `system.cpp`（系统插件，`imapp_system()=1` 不可卸载）。
原因：音频属于全局系统设置，应放在系统入口；demo 插件是控件演示，不该承担系统设置角色。

新增代码位置：`research/apps/system/system.cpp:14-30`（新增 `extern "C"` 块引入音频符号）
+ `:78-116`（新增 `AUDIO` 标签页），含：
- **后端选择**（3 个 RadioButton：`ALSA plughw` / `libao(VGM)` / `SDL(pulse)`）
- **测试音 TONEGEN**（PLAY 20s / STOP / WAVE 4 选 / VOL 滑条）
- **流统计**（`audio_flow` / `dbg_underruns` / `dbg_stalls` 实时显示）
- **v0.4.44+ SDL 复用 plughw 的说明**（灰色提示文本）

`demo.cpp` 中的音频标签（`:112-124`）可以保留作为冗余测试入口（v0.4.45 暂未删，
等你拍板）。

## 2. 切换机制：audio-ctl 线程串行化

切换后端不是直接改 `audio_backend` 全局变量——那会「旧的还在写声卡、新的已经开门
但资源没收」。v0.4.42 引入 `audio-ctl` 线程（`gldemo_imgui.c:1060-1096`）专门处理
异步命令：

```c
static volatile int audio_cmd, audio_cmd_sec, audio_cmd_be;  // 0 none, 1 start, 2 stop, 3 set-backend
void audio_ui_set_backend(int be) { audio_cmd_be = be; audio_cmd = 3; }   // 1067
```

audio-ctl 线程循环（`:1070-1096`）按 `audio_cmd` 处理：
1. `cmd==1`（start）→ 等到 `audio_on==0` 再 spawn writer
2. `cmd==2`（stop）→ `audio_stop_full()` 收尾
3. `cmd==3`（set-backend）→ **先 stop 旧后端完整收尾**（`audio_stop_full`），再 flip
   `audio_backend = audio_cmd_be`

关键日志（`audio_ctl` 串行化成功时打）：
```
C ctl: SETBACKEND 2->0  (切到 ALSA)
C ctl: SETBACKEND 0->2  (切到 SDL, 实际还是 plughw)
```
日志写到 `/home/user/audio_dbg.log`，可 `grep "SETBACKEND"` 验证切换路径。

## 3. 三后端调试史：从崩溃到全通

### 3.1 起点：单 ALSA 后端偶发 XRUN

v0.4.40 之前只有 ALSA 后端，UI 卡顿时 ALSA 写指针回卷报 XRUN、`-EPIPE` 风暴重放
stale 缓冲——「点击播放半秒静默然后杂音」（crackle/half-second then silence）。

### 3.2 错题 1：gen_samples 用 sinf() 软浮点

`gen_samples` 每采样调 `sinf(2πf*t)`，N900 没有硬浮点，软浮点 `sinf` 在 audio writer
线程热路径上**单次 0.5-1ms**——每块 2048 帧 = 2048 次 `sinf`，远超 46ms 周期
（`PERIOD/44100*1000 = 46.4ms`），必然 overrun。

**正解（v0.4.42）**：1024 表项的预计算 `wavetable_sine[]`，线性插值。热路径从「软浮点
`sinf`」变成「两次 float 乘加 + 一次加法」，整块 2048 帧在 1ms 内完成。
源码：`gldemo_imgui.c:825-832` 表初始化，`:840-856` 查表+插值。

### 3.3 错题 2：Audio writer 线程与 UI 共享同一线程

UI 拖滑条、加载字体、刷新纹理——任何停顿都会卡住 audio writer → XRUN。

**正解（v0.4.42，ppsspp 范本）**：`audio_writer` 跑在独立 pthread（`pthread_create_fn`），
UI 线程只调 `audio_ui_start` / `audio_ui_stop` 发命令给 audio-ctl 线程，writer 线程
从不阻塞等 UI。源码：`gldemo_imgui.c:862-934` `audio_writer`，`:1070-1096` `audio_ctl`。

### 3.4 错题 3：pulse cookie 授权死路

`/etc/asound.conf` 把 ALSA `default` 路由到 `type pulse`，`snd_pcm_open("default")`
连到 system-mode pulseaudio，user(29999) 读 `~/.pulse-cookie` EACCES → 永久 `do_sys_poll`。

**正解（v0.4.43）**：
- ALSA 后端：直接 `snd_pcm_open("plughw:0,0", 0)`，绕过 pulse。
- libao 后端：部署完整版 61KB libao（`/home/user/imrun/audiolib/libao.so.2`）取代
  设备 11KB mini 定制版，dlopen 加载 `alsa09` 插件，`ao_open_live` 传 `{"dev","plughw:0,0"}`
  选项，绕过 pulse 走 plughw。
- 兜底：plughw 失败时 fallback 到 `snd_pcm_open("default")`（pulse 死路时无声但
  不卡死）。

### 3.5 错题 4：SDL 原生 pulse callback 死锁

SDL 1.2 在 Maemo 上默认 `pulse` audio driver，callback 线程根本不起飞：
`audio_dbg.log` 显示 `SDL close cb_count=0`（一个回调都没跑），然后 `SDL_CloseAudio`
hang 住，冻结 audio-ctl 线程导致三后端 radio 按钮切换失灵。

**正解（v0.4.44）**：SDL 后端不再走 SDL 原生 callback，**复用 ALSA plughw 路径**：
```c
// gldemo_imgui.c:981
/* backend 0 (ALSA) and backend 2 (SDL/PSP) both go through ALSA plughw */
```
`audio_start` 末尾 `if (audio_backend == 0 || == 2)` 都进 plughw 分支。SDL 的
`sdl_open`/`sdl_close` dlopen 桩保留（万一以后 SDL 驱动修好可以切回），但当前
SDL button 实际是「ALSA plughw 换了个名字」。

### 3.6 错题 5：AOTEST 第一周期初始化尖刺

`pcm_acquire` 后立即 `snd_pcm_writei` 第一次有概率 XRUN（DMA 重启后 broken-pointer
窗口）。**正解**：`pcm_acquire` 后先写 2 个 PERIOD 静音垫缓冲（`:892-898`）：
```c
long z1 = snd_pcm_writei(pcm, zeros, PERIOD);
long z2 = snd_pcm_writei(pcm, zeros, PERIOD);
```
DAC 看到的第一帧永远是静音，没有 stale ring 内容重放。

### 3.7 错题 6：Play-click 杂音（drop+prepare 路径）

`snd_pcm_drop` 后 `snd_pcm_prepare` 不够——plug-layer 与硬件指针 desync，下一次
PLAY 立即 XRUN。**正解**（`audio_stop_full` 路径）：drop 后**完整 `pcm_resetup()`**
（`snd_pcm_hw_params` 重做），不留 desync 窗口。

## 4. 音量控制——为什么 IMRUN 自己乘，不调系统音量

**先纠正一个早期判断**：v0.4.45 之前的文档写过「我们绕过系统音量是因为 pulse 路径
堵死、pulse cookie EACCES」——这个解释**不准确**。实测（2026-10-16）：

```
# N900 声卡硬件 codec = tlv320aic3x（RX51）
# /etc/asound.conf 把 default 路由到 pulse, 但 plughw:0,0 / hw:0,0 不受影响
# 绕开 asound.conf 路由, 直接 amixer -c 0 调硬件 codec 控件:
$ amixer -c 0 sset PCM 30%
Simple mixer control 'PCM',0
  Front Left: Playback 39 [31%] [-44.00dB]    ← 物理寄存器写入了
  Front Right: Playback 39 [31%] [-44.00dB]
$ amixer -c 0 sset PCM 100%                     ← 还原
```

N900 声卡 tlv320aic3x 有完整硬件 mixer（PCM、Headphone、Line DAC、Speaker 等共 60+ 控件），
**任何走 plughw:0,0 的应用**写入的字节流都受 PCM 控件影响。技术上调系统音量**完全可行**。
我们没调，是**架构选择**不是技术限制。

### 4.1 现状——两层音量叠加

| 层 | 控制 | 范围 | 延迟 | 持久化 |
|---|---|---|---|---|
| **软件层**（IMRUN） | `audio_volume` 0..1000，乘 PCM 采样 | 0..100% | 0 帧 | 不持久 |
| **硬件层**（codec） | tlv320aic3x PCM 寄存器 | 0..127，-59.5..0dB | 1-2 帧 I2S | 不持久（设备重启后默认 100%） |

用户看到的结果 = 软件音量 × 硬件音量。当前两套都是手动控制：
- IMRUN VOL 滑条（SETTINGS→AUDIO）只动软件层
- 外部 `amixer -c 0 sset PCM N%` 动硬件层
- 两层**各自独立**，没有耦合

### 4.2 范本参照：vgmplay-maemo / PPSSPP 的处理

**vgmplay-maemo**（`Stream.c:311-327`）：
```c
ao_initialize();
dev_ao = ao_open_live(ao_default_driver_id(), &ao_fmt, NULL);  // 第三个参数 NULL
```
- **不调任何音量 API**——`FillBuffer` 直接拷贝采样给 `ao_play`
- 完全靠 libao → alsa09 → 系统 mixer（即 `alsamixer` GUI 或硬件音量键）
- 2014-2016 N900 Maemo 时代，pulse 是 **per-user 模式**，alsa09 默认走 plughw，
  pulse sink volume 接受 sink 设置，硬件音量键（dbus MCE）也正常联动
- **那个时代「系统音量」 = pulse sink volume = 硬件寄存器**——一条链

**PPSSPP**（`Core/HLE/__sceAudio.cpp:377-415`）：
```c
int __AudioMix(short *outstereo, int numFrames) {
    outAudioQueue.popPointers(numFrames * 2, &buf1, &sz1, &buf2, &sz2);
    memcpy(outstereo, buf1, sz1 * sizeof(s16));  // 没有任何音量乘法
    ...
}
```
- **同样不调音量**——`memcpy` 直拷采样给 SDL callback / Android AudioTrack
- 音量全靠宿主页（Android `setVolume()`、iOS 硬件音量键、Maemo 系统 pulse sink）
- 三个平台各自有「系统音量」层，PPSSPP 都不碰

**共同模式**：这两套都不写软件音量乘法——因为**它们有「系统」可调**。
IMRUN 的特殊性在于我们跑在 N900 Maemo 上，**这个 N900 的「系统」已经严重劣化**：
- pulse 是 system-mode 不是 per-user（vgmplay 时代的 per-user 没了）
- per-user cookie 不可读（user-mode 应用 EACCES）
- alsa09 默认走 plughw:0,0 OK，但 pulse sink volume 改了也不影响 plughw

### 4.3 为什么 IMRUN 不调系统音量——三个真实原因

**原因 A：Maemo 状态栏音量滑条不响应 plughw 路径**
- Maemo 状态栏 = pulse sink volume 滑条
- 我们走 plughw:0,0，不创建 pulse stream，pulse 看不到我们
- 状态栏动 = 改 pulse sink，**对我们的输出完全无效**（实测）
- 所以「系统音量键」这个 UI 通道对我们根本不存在

**原因 B：硬件音量键（dbus MCE）可调，但我们选择不监听**
- `com.nokia.mce` dbus 走 `req_set_volume`，pulse sink → alsa09 → 硬件
- 但 pulse 这条链对 plughw 同样无效（pulse 看不到 plughw 客户端）
- 要让硬件音量键工作，必须**走 pulse**（用 `libpulse-simple`），而 pulse cookie EACCES 死路
- **或者**直接监听 MCE dbus 信号 + `amixer -c 0 sset PCM N%`——但需要权限和线程

**原因 C：0 帧延迟需求**
- 实时游戏音频（AY8910 仿真、未来的 PSP VGM 等）任何音量变化延迟都会破坏沉浸感
- 软件层 `audio_volume` 直接乘采样 = 0 帧延迟，是最优选择
- 系统路径（即使走通）至少 1 帧 I2S + 1 帧 codec 处理 = 几毫秒

### 4.4 架构价值——「操作系统内操作系统」

PSP / IMRUN 都体现同一思想：**实时音视频子系统要保证 0 系统依赖**。

- PSP 自己的音频子系统不调 PS1 BIOS 系统调用（哪怕能调）
- IMRUN 不调 Maemo 桌面/系统（哪怕能调）
- **共同特征：实时音视频子系统需要 0 帧延迟、0 系统依赖**
- 各自有「操作系统内操作系统」——PPSSPP 的 PSP OS 是一例，IMRUN 的
  audio volume 直接乘采样是另一例

这才是 v0.4.45 之前判断的**真实价值**——不是因为「pulse 堵死」绕开，**而是即使
pulse 不堵死我们也要绕开**（实时子系统原则）。但具体到这个 N900 设备上，
pulse 确实堵死了，所以绕开是**两重因素叠加**：
1. 实时性原则（理论上的）—— 选 0 帧延迟的软件音量
2. N900 Maemo 现状（实际上的）—— pulse 路径不可用，硬件音量键不可调

### 4.5 未来选项（暂不做，记录在这里）

| 选项 | 工作量 | 价值 | 风险 |
|---|---|---|---|
| 监听 MCE 硬件音量键 + `amixer -c 0 sset PCM` | 中（dbus 线程 + codec 控件） | 硬件音量键可调硬件层 | 需要 root/CAP_DAC_OVERRIDE？需查证 |
| 取消软件音量，全靠硬件 PCM | 小（删 `audio_volume` 乘） | 实现简单 | 0 帧延迟丧失；任何音量跳变听到「咔嗒」 |
| 暴露 IMRUN VOL 滑条同时写硬件 PCM | 中 | IMRUN VOL 滑条直接控硬件 | 软件音量乘法可保留作 gain staging |
| 什么都不改（v0.4.45 现状） | 0 | IMRUN VOL 滑条软件音量 × 硬件音量 100% | 用户必须用 IMRUN 自己的滑条 |

**推荐**：维持现状。如果未来要做「硬件音量键联动」，按选项 1 做。

---

### 4.6 为什么开源软件都"能调音量"——实证审查

去翻 6 个真实项目的源码，得到一个**反直觉**的结论：**绝大多数开源模拟器
/ 播放器都不调系统音量，它们自己写软件音量**。

| 项目 | 实际怎么处理 | 源码位置 | 结论 |
|---|---|---|---|
| **DeaDBeeF 0.4.2** | **自己写软件音量**：dB→amp 转换，乘采样 | `volume.c:65-79` `volume_set_db()`; `alsa.c:584-586` `ivolume = volume_get_amp() * 1000; stream[i] = (int16_t)(stream[i] * ivolume / 1000)` | 双层音量：软件（app）+ 系统（pulse sink） |
| **vgmplay-maemo** | **0 软件音量**，`FillBuffer` 直拷采样给 `ao_play` | `Stream.c` 全文 0 个 volume 匹配 | 完全甩给 libao/alsa09/pulse |
| **PPSSPP 0.9.6** | **0 软件音量**，`__AudioMix` 直接 `memcpy` | `__sceAudio.cpp:377-399` | 完全甩给 SDL/Android AudioTrack/系统 |
| **CPS2 (MAME 衍生)** | GP2X 版：`/dev/mixer` + `ioctl(SOUND_MIXER_WRITE_PCM)`；**非 GP2X 平台为空函数** | `sndintrf.c:172-204` | 平台专用：直接调硬件 / 完全不管 |
| **gngeo (Neogeo)** | `EPOC_SetAudioVolume` 调 Symbian 系统 API | `symbian_s60.cpp:179-202` | 平台专用：调系统 API |
| **SDL_mixer 1.2** | **自己写软件音量**：`ADJUST_VOLUME(s,v) = s*s*v/SDL_MIX_MAXVOLUME` | `sdl-1.2.13/src/audio/SDL_mixer.c:90` | 几乎所有 SDL 模拟器 = 软件音量 |
| **libao 0.8.8** | **0 mixer 调用**（plugins/alsa/ao_alsa.c 0 匹配） | `ao_alsa.c` | 纯 PCM 输出，甩锅给 alsa09/pulse |
| **alsa09 (libao 驱动)** | **0 mixer 调用** | 同上 | 纯 PCM 输出 |

**统计**：8 个项目里 3 个自己写软件音量（DeaDBeeF、SDL_mixer、CPS2/GP2X 走硬件），
3 个完全不管音量（vgmplay、PPSSPP、libao 链），1 个平台特定（gngeo）。

**这些项目能"调音量"是因为**：
- DeaDBeeF 自己有 GUI 音量滑条，拖动改 `volume_db` 触发 `volume_set_db` → 自己乘
- SDL 模拟器 → SDL_mixer → `Mix_Volume` API → 内部调 `ADJUST_VOLUME`
- vgmplay/PPSSPP → 完全甩锅给 alsa09（libao）→ alsa09 转发给 pulse → per-user
  pulse 时代接受 sink volume 设置
- 任何 Maemo 状态栏音量条 = 改 pulse sink volume

**没有一个项目是"自己打开 /dev/mixer 设硬件寄存器"的现代写法**（GP2X 是 2004 年代的），
CPS2 GP2X 版是历史遗迹。**当代 Linux 上调音量都是 pulse / alsa-lib / snd_mixer_*
高层 API**，不直接动 codec 寄存器。

### 4.7 N900 现状——为什么我们和别人不一样

**重要先例：PicoDrive S60v3 的独立音频后端**（用户实证）——S60v3 版 PicoDrive
（Genesis/MegaDrive 模拟器）使用**完全独立的音频输出**，不受 Symbian 系统音量控制，
只能在模拟器内调内置音量。这与 IMRUN 的 plughw 独立路径**同构**：

| | PicoDrive S60v3 | IMRUN (ALSA/SDL 后端) |
|---|---|---|
| 输出方式 | 独立音频后端（绕开 Symbian 媒体服务器） | plughw:0,0 直连（绕开 pulse） |
| 系统音量 | **不响应**，只能用内置音量 | **不响应**（pulse sink 看不到 plughw 客户端） |
| 内置音量 | 模拟器 UI 内软件音量 | IMRUN VOL 滑条（软件乘法） |
| 为什么 | 实时模拟器需要 0 延迟 + 绕开系统调度 | 同左 |

**这是嵌入式模拟器/实时应用的既定先例**：在性能受限的老设备上，实时音视频子系统
为了 0 帧延迟，独立音频路径 + 应用内音量是**被验证过的正确架构**，不是 hack。
PPSSPP/CPS2/vgmplay 在各自平台"遵守系统音量"，是因为它们的宿主平台系统音频
（Android AudioTrack / per-user pulse / SDL）本身够快；**一旦宿主系统的音频路径
成为瓶颈（本 N900 的 system-mode pulse + Nokia 私有路由），独立路径就是唯一解**。

v0.4.46 的 PULSE 后端（sink.hw0）给出了第三条路：**在系统音频可用且不卡的前提下
（48000 原生率 + 独占声卡释放），可以兼得"系统音量控制"和"流畅"**。两后端并存：
- **默认 SDL/plughw（独立路径，PicoDrive 模式）**——最稳，音量在应用内
- **可选 PULSE（sink.hw0，系统路径）**——受系统音量控制，48000 下实测流畅

**别人的"系统音量"在我们的 N900 上都死了**：

1. **pulse 路径全死**
   - vgmplay/PPSSPP 时代 per-user pulse：sink volume 正常接受 user 设置
   - N900 现在 system-mode pulse：user 读 cookie EACCES → 永久 sleep
   - 任何 libao/SDL/pulse 客户端的「系统音量」API 都死路
2. **alsa09 不支持 `dev=` 选项**（设备 11KB mini libao，0 个 `ao_append_option`）
   - 不能强制走 plughw
   - 默认 "default" = pulse 路由 = 死
3. **完整 libao + `dev=plughw:0,0` 是绕开 pulse 的唯一办法**（v0.4.43 实装）
4. **绕开 pulse 后就**没有任何系统音量管理**了**
   - pulse sink 看不到 plughw 客户端
   - Maemo 状态栏滑条 = pulse sink volume = 改了我们输出不变
   - Maemo 硬件音量键 = MCE dbus → pulse sink → 同样无效

**所以"系统音量"在我们这台 N900 上是个死概念**。任何"开源软件都遵守规定"
的论断都隐含了一个前提：**那个"系统"是工作的**。我们的 N900 这个"系统"是坏的。

**这才是真正的"0 系统依赖"含义**：不是我们主动选 0 系统依赖，是**这个 N900
已经没有可用的"系统音量"让我们依赖**了。我们只能像 CPS2 GP2X 一样，直接动
硬件 codec 寄存器，或者像 DeaDBeeF 一样自己乘。

**实际可行的两条路**：
- **A. 直接动硬件**（CPS2 GP2X 路）：`amixer -c 0 sset PCM N%` 改 codec 寄存器
- **B. 自己乘采样**（DeaDBeeF / IMRUN v0.4.45 当前）：软件音量 + 硬件 100%

**两条路都"绕开系统音量"**——但**这不违反"开源都遵守规定"原则**，因为：
- 那个"系统"在这台 N900 上已经死了
- 我们选的实现方式（软件乘法 / 直接硬件 mixer）正是开源软件的两种主流选择
- 只是因为"系统音量"在 N900 上不可用，我们的两层音量降为单层（仅软件 / 仅硬件）

### 4.8 终极结论

> "为什么说走系统统一的音量管理一定会卡死"——这个命题**之前认为是普适的**，
> 实际上是**早期不严谨测试导致的错误论断**。**2026-10-16 实测推翻**：
>
> **pulse 路径不死锁。** `paplay /home/user/sw30.wav` 在 user(29999) 身份下
> 长播 30 秒，期间 4 次 `amixer -D default sset Master {30,80,50,100}%`
> **全部成功**，无任何 dbus 信号（amixer 不发 dbus），paplay 进程完整跑完
> 无错误无断流。
>
> 之前以为"pulse cookie EACCES 死路"是基于**单点不严谨测试**——实际
> 跑通 libao / paplay / system-mode pulse 之间的链路是**正常**的。
>
> 真正不死的路（实测确认）：
> 1. **直接动硬件 codec 寄存器**（`amixer -c 0` 路径）—— OK，不发 dbus
> 2. **自己写软件音量**（DeaDBeeF 模式）—— OK，与传统并存
> 3. **走 pulse**（paplay / libpulse-simple）—— **OK，30s 流畅不卡**
>
> "开源都遵守规定"是说**遵循一套跨平台的音量约定**——例如音频应用不应绕过
> 用户系统级的音量管理意图。我们的双层音量（应用 0..1 + 硬件 0..127）实际上
> 仍**尊重**这个意图，只是因为 IMRUN 当前 plughw 路径**没有创建 pulse stream**，
> pulse server 看不到 IMRUN 这个客户端，所以 Maemo 状态栏/硬件音量键改
> sink volume 时 IMRUN 不受控。

### 4.9 N900 Maemo 音量管理真相——基于实证 mce-dev 1.8.19 源码审查

**mce-dev 1.8.19 的 `dbus-names.h`（494 行）完全没有 volume/audiorouting/audio 接口。**

```
$ grep -iE "vol|aud|master|media" build/mce_dev/usr/include/mce/dbus-names.h
# (空 — 无匹配)
```

mce 1.8.19 公开的 dbus 接口只有：display, devicelock, tklock, callstate,
device_mode, inactivity, orientation, accelerometer, vibrator, led_pattern。

**这印证了 Maemo 5 音量管理的实际架构**：
- mce 捕获硬件音量键（keypad 模块）
- mce 转发事件给 **pulseaudio server**（通过 pulse 模块，与 audiorouting.so 配合）
- pulseaudio 修改 `Master` sink volume
- 所有 pulse 客户端（paplay、libao、libpulse）受 sink volume 约束
- **系统栏音量条 = 状态栏 widget 调 `pactl set-sink-volume`**（pulse native 协议）

**所以"系统音量"在 Maemo 5 = pulse sink volume**。任何调系统音量的应用都
必须订阅 pulse 事件，不能走 mce dbus（mce 根本不公开 volume 接口）。

### 4.10 传统、流畅、遵守系统音量的方案——**已实装 (v0.4.46)**

**libpulse-simple 后端**（backend=3）已实装。这是 paplay 走的路径：

- `audio_pulse.c` dlopen `libpulse-simple.so.0` + `libpulse.so.0`
- `pa_simple_new(NULL, "imrun", PA_STREAM_PLAYBACK, NULL, ...)` 连 pulse server（默认 sink）
- writer 线程 `pa_simple_write` 喂字节流到 pulse sink
- pulse server 自动让 stream 受 sink volume 控制
- **系统栏/硬件键/amixer 改 sink vol → IMRUN 输出实时跟手**（pulse server 端计算）

**延迟**：`pa_simple_write` 首写延迟 0.6ms，稳态 1.6ms/块（AOTEST 实测）。
paplay 30s 长播流畅不卡（实机验证）。

**关键实现细节**（`gldemo_imgui.c` audio_writer PULSE 分支）：
1. write 前用 `pa_simple_get_latency` 检查 server 端缓冲（满就 sleep 1ms 重试）
   ——避免 `pa_simple_write` 阻塞导致停播时 `pthread_join` 死锁
2. writer 线程自己 close（`pa_simple` 非线程安全，所有操作必须在同一线程内）
3. `audio_stop_full` PULSE 分支只设 `thread_run=0` + join（不调 pa_simple_*）

**AOTEST 不覆盖 PULSE**（只测 0/1/2 三后端）：pa_simple 同步阻塞 API 不适合
AOTEST 的快速 PLAY/STOP 压力循环（writer 在 pa_simple_write 阻塞时看不到
thread_run=0）。正常长播场景（VGM/TONEGEN 20s+）不受影响——用 GUI 实测验证。

### 4.11 用户实测指南

在设备上验证 PULSE 后端是否"遵守系统音量"：

1. IMRUN 已启动（v0.4.46+），桌面正常
2. 进 SETTINGS → AUDIO 标签
3. 选 **PULSE (paplay)** 后端按钮
4. 点 **PLAY 20s** 启动测试音
5. **期间用设备侧音量键**（或 `amixer -D default sset Master N%`）调音量
6. 观察：IMRUN 输出音量应**立即跟手变化**（不再是"调了没反应"）
7. 与 SDL 后端对比：SDL 后端（走 plughw）调系统音量**无效**，PULSE 后端**有效**

如果 PULSE 后端长播卡顿或 STOP 时挂住，回退到 SDL（backend=2，当前默认）。

---

## 5. 调试速查

- **后端死掉**：`audio_flow` 长时间不增长 → 看 `audio_backend` 值；切回 SDL(=plughw) 一般就活了
- **AOTEST 完整结果**：`grep "AOTEST end" /home/user/audio_dbg.log` → `hard_fail=0 underruns=0` 是核心
- **三后端切来切去日志**：`grep "SETBACKEND" /home/user/audio_dbg.log`
- **SDL callback 死锁（v0.4.44 前的问题）**：`grep "SDL close cb_count" /home/user/audio_dbg.log`——应=0
- **XRUN**：`grep "EPIPE\|XRUN" /home/user/audio_dbg.log`——应为空
- **writer 线程延迟**：`grep "gen+write" /home/user/audio_dbg.log`——>46.4ms 就有风险
- **硬件 codec 寄存器状态**：`amixer -c 0 sget PCM`——当前 PCM 控件值（N900 上 tlv320aic3x 寄存器）
- **直接设硬件音量**：`amixer -c 0 sset PCM 30%`（绕开 asound.conf 路由，plughw 写入仍受影响）

## 6. 相关文件

- 主程序音频源码：`build/gldemo_imgui.c`（1550 行，搜 `audio_backend` 全部逻辑）
- 插件扫描 + 后端切换 UI：`build/imgui_gles1.cpp:551-585` `dyn_scan`
- 旧 demo 音频 UI（已迁出但保留）：`research/apps/demo/demo.cpp:112-124`
- 新 system 音频 UI：`research/apps/system/system.cpp:14-30 + :78-116`
- 三后端完整版 libao：`/home/user/imrun/audiolib/libao.so.2`（61KB，dlopen 加载 alsa09）
- 调查归档（v0.4.43 之前的血路）：`docs/AUDIO_HANDOFF.md`
- 插件开发（怎么用 audio 钩子）：`docs/PLUGIN_DEV_GUIDE.md` §7
