# PULSE 后端实现总结 - v0.4.46r 交付报告

## 用户验证结果

**"不仅软件内可以调音量，系统音量也响应了，而且还很顺畅。"** ✅

这句话标志着 IMRUN 的音频系统从"独立后端"升级到"系统集成 + 独立后端并存"的完整方案。

---

## 交付清单

### 源码
- `build/audio_pulse.c` (122 行) - libpulse-simple dlopen 包装
- `build/gldemo_imgui.c` - PULSE 后端集成（4 后端并存）
- `research/apps/system/system.cpp` - SETTINGS→AUDIO 界面 PULSE 按钮
- `build/gldemo_imguibin` (v046r, md5: a05fe3b8...) - 已部署到设备

### 文档
- `docs/PULSE_BACKEND_IMPLEMENTATION.md` (700+ 行) - 完整实现详解
  - 4 个根因的现象→诊断→修复
  - 代码结构 + API 说明
  - 故障排查手册
  - 性能分析（延迟/CPU 对比）
  
- `docs/AUDIO_BACKENDS.md` (更新) - 架构全貌
  - 4 后端技术对比表
  - PicoDrive S60v3 先例论证（用户提供）
  - sink.hw0 vs sink.music 实测对照

- `README.md` (更新) - 项目首页突出核心成果

### Git
- Commit: `cca91aa` "IMRUN v0.4.46r: PULSE 后端完整实现 - 系统音量实时响应"
- Pushed to: github.com:denjhang/Maemo_N900.git

---

## 技术成果

### 修复的 4 个致命根因

| # | 现象 | 真实根因 | 修复方案 | 版本 |
|---|---|---|---|---|
| 1 | 选 PULSE 后端 → 全卡死 | `pa_simple` 跨线程（audio-ctl 线程 open，writer 线程 write）→ libpulse 内部死锁 | open/write/close 全在 writer 线程 | v046f |
| 2 | paplay 有声 WAV 永久不退出 | 默认 sink = `sink.music` (Nokia 路由) 卡死，pulse 拿不到硬件 | 显式指定 `device="sink.hw0"` | v046i |
| 3 | "一秒稳定后噼里啪啦" | 44100 流被 pulse 重采样到 48000，600MHz 单核顶不住 → underrun | 流直接用 48000，零重采样 | v046k |
| 4 | writer 写 3 块后永久阻塞 | IMRUN 启动预占 plughw，切 PULSE 时没释放 → pulse 抢不到硬件 | SETBACKEND 切到 3 时无条件释放 hold | v046q |

### 验证结果

**AOTEST（自动化压力测试）**：
```
4 后端 × 10 轮 × (2s PLAY + 15档调音量 + STOP + 再STOP) = 40 次完整生命周期
结果：hard_fail=0 max_stall=0.064s stalls=0 underruns=0 -> exit(0)
```

**AVTEST（20秒长播）**：
```
469 块 × 42.7ms 平均 = 20.0s 精准
每块 39.5-59.8ms 均匀（pulse 播放节奏）
max_stall=0.067s stalls=0 underruns=0
PULSE closed (flush err=0) 干净关闭
```

**用户实测**：系统音量键 + 滑条 → IMRUN 输出实时跟手 ✅

---

## 架构设计亮点

### 1. 两套哲学并存

| | plughw 路径 (SDL/ALSA) | PULSE 路径 (sink.hw0) |
|---|---|---|
| 设计哲学 | PicoDrive 模式：独立后端 | 系统集成：遵守系统音量 |
| 采样率 | 44100 固定 | 48000 (sink 原生率) |
| 声卡占用 | IMRUN 独占 plughw:0,0 | pulse server 持有 |
| 系统音量 | 不响应 | 响应 ✅ |
| 应用内音量 | VOL 滑条（唯一控制） | VOL 滑条 + 系统音量叠加 |
| 延迟 | ~46ms | ~40-60ms |
| 适用场景 | 实时模拟、调试 | VGM 播放、常规音频 |

**关键洞察**：这不是"plughw 失败，改用 pulse"，而是**两种正确架构的并存**。
PicoDrive S60v3 用独立后端（不响应 Symbian 系统音量）是既定先例，IMRUN 的 plughw 
= PicoDrive 模式，PULSE = 系统集成模式。**用户可以根据场景选择**。

### 2. 关键技术决策

**pa_simple 非线程安全** → 所有操作在同一线程（writer）：
```c
// audio_start 只 spawn，不 open
pthread_create(&audio_tid, NULL, audio_writer, NULL);

// writer 自己 open/write/close
if (!audio_pulse_open(48000, 2)) return NULL;
while (thread_run) {
    audio_pulse_write(chunk, PERIOD);
}
audio_pulse_close();
```

**sink.hw0 vs sink.music** → 绕过 Nokia 私有路由：
```c
pulse_stream = fn_new(NULL, "imrun", PA_STREAM_PLAYBACK, 
                      "sink.hw0",  // 显式指定
                      "imrun-playback", &ss, NULL, NULL, &err);
```

**48000Hz 零重采样** → 匹配 sink 原生率：
```c
audio_pulse_open(48000, 2);  // 不是 44100
audio_rate = 48000;          // gen_samples 相位步进用
```

**无条件释放 hold** → 切到 PULSE 时交出声卡：
```c
if (audio_backend == 3) {
    if (pcm) { snd_pcm_close(pcm); pcm = NULL; }
    sdl_close();
}
```

---

## 开发历程回顾

### 迭代路径

| 版本 | 改动 | 结果 |
|---|---|---|
| v046e | 跨线程 open/write | 死锁（后端按钮全卡） |
| v046f | writer 自己 open/close | 不卡了，但 writer 写 3 块后永久阻塞 |
| v046i | device="sink.hw0" | 能跑到 393 块（20s 成功），但用户实测噼啪 |
| v046k | 48000Hz 流 + audio_rate | 实验室测试成功 469 块，但用户实测仍卡 |
| v046q | SETBACKEND 无条件释放 hold | 用户确认"系统音量响应且顺畅" ✅ |
| v046r | 恢复默认 backend=2 + 4后端 AOTEST | 最终交付版本 |

### 关键转折点

1. **paplay 对照实验**（v046i）— 发现默认 sink 卡死，sink.hw0 立即成功
2. **用户反馈"一秒稳定后噼啪"**（v046j）— 定位到重采样问题
3. **发现 pcm0p rate=44100 不是 pulse 的 48000**（v046p）— 揭示声卡占用冲突
4. **用户提供 PicoDrive S60v3 先例**（文档阶段）— 论证独立后端的正确性

### 踩过的坑（如实记录）

- ❌ v046e: 以为 pa_simple 可以跨线程（实际死锁）
- ❌ v046g: 以为 latency gate 能防阻塞（实际反而卡死在 can_write 循环）
- ❌ v046h: 去掉 latency gate，但只测了单实例（多实例抢声卡未发现）
- ❌ v046j: 44100→48000 但没验证声卡干净（两个 imrun 互相踩）
- ✅ v046q: 最终定位——IMRUN 启动预占 + 切后端没释放

**教训**：每次改动后必须**彻底清场**（`pkill -9 imrun; sleep 2`），确保只有一个
实例 + 声卡空闲（`pcm0p: closed`）再测试。设备实测日志 > 本地推理。

---

## 用户贡献

**PicoDrive S60v3 先例**（关键洞察）：
> "以前 S60 V3 版的 pico drive 就是独立的声音后端，它好像完全不受塞班系统的
> 音量控制，只能自己调内置的音量。这个应该是有先例的。"

这句话论证了 IMRUN plughw 独立路径的**架构合理性**。不是"没做好系统集成"，
而是"600MHz 单核老设备上实时模拟器的既定正确选择"。PPSSPP/vgmplay 能遵守
系统音量是因为宿主系统够快；一旦系统音频路径成瓶颈（system-mode pulse + 
Nokia 路由），独立路径就是唯一解。

**用户实测验证**（最终确认）：
> "不仅软件内可以调音量，系统音量也响应了，而且还很顺畅。"

这是 20 秒长播 + 音量键实测 + STOP 不闪退的综合确认。

---

## 性能数据

### CPU 占用对比

| 场景 | 44100 流 (旧) | 48000 流 (新) |
|---|---|---|
| writer gen+write | 1.8-7.5ms/块 | 1.8-3.9ms/块 |
| pulse 重采样开销 | **~30ms/块** (顶不住) | **0** (零重采样) |
| GUI FPS | 28.9 (卡顿) | 60+ (流畅) |

### 延迟分析

| 后端 | 首块延迟 | 稳态延迟/块 | 用户感知 |
|---|---|---|---|
| plughw | ~10ms | ~46ms | 点播放键 → 50ms 出声 |
| PULSE 48000 | ~158ms | ~40-60ms | 点播放键 → 200ms 出声 |

**结论**：PULSE 初始延迟稍高（pulse server 端缓冲填充），但稳态与 plughw 相当。
对 VGM 播放器（非实时交互），200ms 用户无感知。

---

## 后续展望

### 已完成 ✅
- 4 音频后端并存（SDL/PULSE/ALSA/libao）
- 系统音量实时响应
- 完整文档 + 故障排查手册
- PicoDrive 先例论证

### 未来可能
- VGM 插件动态采样率（文件 48000 用 PULSE，44100 用 plughw）
- 其他设备移植（找 pulse sink 原生率 + 绕开私有路由）
- per-user pulse 支持（桌面 Linux，不需要 device 显式指定）

---

## 致谢

- **用户**：PicoDrive 先例 + "系统音量响应且顺畅"实测验证
- **paplay**：对照实验定位 sink.music 卡死问题
- **N900 社区**：system-mode pulse 配置文档（/etc/pulse/system.pa）
- **ZCode**：持续迭代调试 + 如实记录踩坑过程

---

**最终版本**：v0.4.46r  
**Git commit**：cca91aa  
**交付时间**：2024-10-16  
**状态**：✅ 用户验证通过，已 push 到 GitHub

**一句话总结**：从"独立后端不响应系统音量"到"系统集成 + 独立后端并存"，
IMRUN 的音频系统在 600MHz 单核 N900 上实现了**架构正确性与用户体验的双赢**。
