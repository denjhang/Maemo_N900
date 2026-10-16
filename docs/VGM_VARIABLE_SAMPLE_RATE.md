# VGM 插件支持可变采样率修改方案

## 问题
当前 VGM 插件硬编码 44100Hz 采样率，而 PULSE 后端要求 48000Hz。这导致：
- 音高偏高 8.8%（440Hz → 480Hz）
- 播放速度不匹配

## 修改点

### 1. ay8910.c - 采样率变量化

**当前（第 63 行）**：
```c
#define AY_RATE 44100UL
```

**改为**：
```c
static u32 ay_sample_rate = 44100;  // 默认 44100，运行时可设
```

**ay_set_clock 函数（第 65 行）改用变量**：
```c
void ay_set_clock(u32 clock_hz) {
    u64 incr = (u64)clock_hz * (1UL << AY_GETA_BITS) / (8UL * ay_sample_rate);  // 改这里
    ay_base_incr = (u32)(incr & 0xFFFFFFFF);
}
```

**添加新函数设置采样率**：
```c
/* 设置输出采样率（必须在 ay8910_reset 前调用）
 * rate: 44100 (plughw 后端) 或 48000 (PULSE 后端) */
void ay_set_sample_rate(u32 rate) {
    ay_sample_rate = rate;
    ay_set_clock(AY_CLK);  // 重算 base_incr
}
```

### 2. vgm.cpp - 从主程序读取采样率

**在文件开头添加外部符号声明（第 24 行后）**：
```cpp
extern "C" {
void audio_ui_start(int seconds);
void audio_ui_stop(void);
void audio_ui_set_backend(int be);
extern int audio_on;
extern double audio_deadline;
extern int audio_volume, audio_wave;
extern int (*audio_source_fn)(short *dst, int frames);
extern int audio_rate;  // 新增：主程序的运行时采样率（44100 或 48000）
double imrun_now(void);
}
```

**在 ay8910.c 开头添加函数声明（第 6 行后）**：
```cpp
#include "imgui.h"
#include "ay8910.c"
extern "C" void ay_set_sample_rate(u32 rate);  // 新增
```

**vgm_start 函数改用运行时采样率（第 437 行）**：

**当前**：
```cpp
static int vgm_start(const char *path) {
    // ... 省略加载逻辑 ...
    audio_source_fn = vgm_render;
    audio_ui_start(vgm_numticks / 44100 + 2);  // 硬编码 44100
    return 1;
}
```

**改为**：
```cpp
static int vgm_start(const char *path) {
    char full[640];
    snprintf(full, sizeof(full), "/home/user/MyDocs/VGM/%s", path);
    if (!vgm_load_file(full)) return 0;
    if (!vgm_parse_header()) return 0;
    vgm_reset_stream();
    vgm_loaded = 1;
    vgm_playing = 1;
    vgm_paused = 0;
    vgm_play_start = (unsigned long)imrun_now();
    vgm_pending = 0;
    
    // 新增：设置 AY8910 采样率为主程序当前后端的采样率
    ay_set_sample_rate(audio_rate);  // 44100 (plughw) 或 48000 (PULSE)
    
    audio_source_fn = vgm_render;
    // 时长计算也用运行时采样率
    audio_ui_start(vgm_numticks / audio_rate + 2);
    return 1;
}
```

### 3. 验证点

修改后编译 vgmplayer.so，测试：

**plughw 后端（默认）**：
```
audio_rate = 44100
→ ay_set_sample_rate(44100)
→ VGM 音高正确（A4 = 440Hz）
```

**PULSE 后端**：
```
audio_rate = 48000
→ ay_set_sample_rate(48000)
→ VGM 音高正确（A4 = 440Hz，不再偏高到 480Hz）
→ 系统音量键响应 ✅
```

## 编译命令（参考 b_plugins2.sh）

```bash
# WSL 交叉编译
S=/opt/sysroot
B=/mnt/e/working/zcode-projects/Maemo_N900
F="--sysroot=$S -O2 -fsigned-char -fno-threadsafe-statics -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -DNDEBUG -I$B/build -I$B/build/imgui"

arm-linux-gnueabi-g++ $F -fPIC -shared \
    $B/research/apps/vgmplayer/vgm.cpp \
    -o $B/build/vgmplayer.so \
    -lz  # VGZ 解压需要

# 部署到设备
scp build/vgmplayer.so root@192.168.101.108:/home/user/imrun/plugins/
```

## 测试步骤

1. 修改 ay8910.c 和 vgm.cpp
2. 重编 vgmplayer.so
3. 部署到设备 `/home/user/imrun/plugins/`
4. 启动 IMRUN → SETTINGS→AUDIO 选 **PULSE (paplay)**
5. VGM 标签页加载一首 VGM
6. 点 PLAY：
   - 音高应该正确（不再偏高 8.8%）
   - 系统音量键应该实时响应
7. 切回 SDL 后端，再播一次：
   - 音高仍正确（ay_set_sample_rate 自动切回 44100）

## 注意事项

- `ay_set_sample_rate` 必须在 `audio_source_fn = vgm_render` 之前调用
- 如果 VGM 播放期间切换后端，需要重新 STOP + PLAY（当前 IMRUN 设计）
- `audio_rate` 变量由主程序在 `audio_start` 时设置（PULSE=48000，其他=44100）

## 预期效果

修改后，VGM 插件会自动适配主程序的后端：
- plughw/SDL/ALSA → 44100Hz 生成 → 音高正确，独立音量
- PULSE → 48000Hz 生成 → 音高正确，系统音量响应 ✅

---

**文件清单**：
- `research/apps/vgmplayer/ay8910.c` - 3 处改动
- `research/apps/vgmplayer/vgm.cpp` - 3 处改动
- 总共约 10 行新增/修改

**难度**：简单（不涉及 VGM 命令解析，只改采样率配置）
