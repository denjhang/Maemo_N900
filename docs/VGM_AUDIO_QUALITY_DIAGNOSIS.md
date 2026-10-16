# VGM 插件音质问题诊断报告

## 用户反馈的问题
1. **音质差** - 跟原版相比
2. **丢失噪音通道** - 某些通道的噪音没有发出

## 排查结果

### 1. 采样率问题 ✅ 已排除
- **检查**：`ay_sample_rate` 是运行时可设变量（默认 44100）
- **结论**：不是 STC32 的 22050 锁死问题
- **现状**：`ay_set_sample_rate()` 正确调用，PULSE=48000, SDL=44100

### 2. AY8910 核心问题 🔧 已修复（v3）

#### 噪声生成器（17-bit LFSR）
**旧版（精简版）问题**：
```c
ay_noise_seed = (ay_noise_seed >> 1) | 
                ((ay_noise_seed ^ (ay_noise_seed >> 3)) & 1) << 16;
if (ay_noise_seed & 0x24000) // 什么逻辑？
```
- XOR 顺序可能有误
- 初始值为 0（应该是 1）
- `0x24000` 判断不明

**v3 修复（libvgm 标准）**：
```c
/* 标准 17-bit LFSR: bit0 XOR bit3 → bit17, then shift right */
ay_noise_rng ^= (((ay_noise_rng & 1) ^ ((ay_noise_rng >> 3) & 1)) << 17);
ay_noise_rng >>= 1;
noise_out = ay_noise_rng & 1;  // bit0 = 输出
```
- 初始值 1
- libvgm 验证过的 AY-3-8910/YM2149 标准实现

#### 音量表
**旧版**：线性分段（`ay_voltbl`）
```c
0x00, 0x00, 0x03, 0x03, 0x04, 0x04, 0x06, 0x06, ...
```

**v3 改进**：指数曲线（`ay_voltbl_v3`）
```c
/* exp(i/2-7.5) 近似，更接近真实 AY 芯片 */
0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x04,  
0x05, 0x06, 0x08, 0x0A, 0x0E, 0x11, 0x15, 0x1C, ...
```
- 音量过渡更平滑
- 接近 libvgm 电阻网络模型的输出

### 3. VGM 命令解析问题 🔧 已修复（v3b）

#### 遗漏的命令

| 命令 | 功能 | 旧版处理 | v3b 修复 |
|---|---|---|---|
| `0xA0` | AY8910 write | ✅ 正确 | ✅ 保持 |
| `0x31` | AY stereo mask | ❌ 被跳过 | ✅ 显式处理（暂单声道） |
| `0xA1` | Dual chip AY8910 | ❌ 被跳过 | ⚠️ 暂不支持（少见） |

**0x31 (stereo mask) 影响**：
- VGM 文件里三通道的立体声位置（L/R）被忽略
- 所有通道混到单声道 → **可能影响音质/空间感**
- v3b 加了显式跳过（TODO: 未来实现真立体声）

#### Clock 处理 ✅ 已有
```c
vgm_ay_clock = header[0x74-0x7B];  // 从头部读取
if (vgm_ym2149_div2)
    ay_set_clock(vgm_ay_clock / 2);  // YM2149 /2 分频
else
    ay_set_clock(vgm_ay_clock);
```

---

## 版本对比

| 版本 | 噪声 LFSR | 音量表 | 0x31 处理 | 状态 |
|---|---|---|---|---|
| v1 (精简) | 有误 | 线性 | 跳过 | 旧版 |
| v2 (时序修复) | 有误 | 线性 | 跳过 | 已部署，STOP 不崩 |
| v3 (核心升级) | ✅ libvgm 标准 | ✅ 指数曲线 | 跳过 | 已编译 |
| v3b | ✅ libvgm 标准 | ✅ 指数曲线 | ✅ 显式处理 | 推荐测试 |

---

## 测试建议

### v3b 测试步骤
1. 部署 `vgmplayer_v3b.so`
2. 用 **SDL 后端**播放（PULSE 还是无声）
3. 选有**打击乐/鼓点**的 VGM（使用噪音通道）
4. 对比点：
   - **噪音是否正常发出**（之前丢失）
   - **音质是否改善**（更平滑、更接近原版）
   - **整体音色**是否更接近 PC 上的 VGM 播放器

### 如果仍有问题
可能需要：
1. **实现真立体声输出**（0x31 stereo mask）
2. **移植 libvgm 完整音量表**（电阻网络模型，但计算量大）
3. **对比具体 VGM 文件的波形**（PC vs N900）

---

## 文件清单
- `research/apps/vgmplayer/ay8910_v3.c` - 改进的 AY8910 核心
- `research/apps/vgmplayer/vgm.cpp` - 加了 0x31 处理
- `build/vgmplayer_v3.so` (md5: a1008d01...) - v3 版本
- `build/vgmplayer_v3b.so` (md5: a2ff9a58...) - v3b 版本（推荐）

---

**下一步**：部署 v3b 到设备，用户实测音质改善程度。
