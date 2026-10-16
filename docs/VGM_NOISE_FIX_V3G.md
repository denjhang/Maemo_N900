# VGM 播放器开发记录 - 噪音通道修复之旅

## 问题背景

用户反馈：VGM 播放器相比 PC 原版播放器，"丢失了大部分噪音通道"。

测试文件：`d-fr4gmEnt-re4Lt-me-DiHalt-Lite-2019-2-.vgm`

---

## 调查过程

### 第一阶段：尝试完整移植 libvgm

**误判**：认为精简版 AY8910 核心实现有问题，需要用 libvgm 原版（1623 行）替换。

**尝试**：
1. 复制 libvgm ay8910.c/h 原版
2. 创建兼容层头文件处理依赖
3. 遇到大量依赖问题（DEVDEF_RWFUNC, AY8910_CFG 等）

**结果**：依赖太多，复杂度高，放弃此方案。

**用户建议**：不要推倒重来，应该保留当前精简版，只修复具体问题。

---

### 第二阶段：对比分析 VGM 文件

**分析工具**：Python 脚本解析 VGM 命令序列

**关键发现**：
```
[0092] REG6 (NOISE_PERIOD) = 0x00 (0)
[0095] REG7 (MIXER) = 0x30 | Tone:000 Noise:011 (0=EN)
...
[00AE] REG7 (MIXER) = 0x38 | Tone:000 Noise:111 (0=EN)
```

**观察**：
1. VGM 使用 `NOISE_PERIOD = 0`
2. 混合器在 `0x30`（噪音 ON）和 `0x38`（噪音 OFF）之间快速切换
3. CH0 音量快速变化（0x0F → 0x0E → ... → 0x07）

---

### 第三阶段：定位根本原因

**检查代码**（ay8910_v3.c 第 181 行）：
```c
if (ay_noise_freq > 0 && ay_noise_count >= ay_noise_freq) {
    // 更新噪声 LFSR
}
```

**问题**：
- 当 `ay_noise_freq = 0` 时，`> 0` 检查失败
- **噪声 LFSR 永远不更新**
- 噪音通道完全静音

**libvgm 原版逻辑**：
```c
psg->count_noise++;
if (psg->count_noise >= NOISE_PERIOD(psg)) {
    // 更新 LFSR
}
```
- 当 `NOISE_PERIOD = 0` 时，`>= 0` 永远为真
- **每个采样都更新 LFSR**
- 产生最高频率噪音

---

## 修复方案

### v3g 修复

**文件**：`research/apps/vgmplayer/ay8910_v3.c` 第 176-192 行

**修改前**：
```c
ay_noise_count += incr;
if (ay_noise_freq > 0 && ay_noise_count >= ay_noise_freq) {  // ❌
    ay_noise_rng ^= (((ay_noise_rng & 1) ^ ((ay_noise_rng >> 3) & 1)) << 17);
    ay_noise_rng >>= 1;
    ...
}
```

**修改后**：
```c
ay_noise_count += incr;
if (ay_noise_count >= ay_noise_freq) {  // ✅
    ay_noise_rng ^= (((ay_noise_rng & 1) ^ ((ay_noise_rng >> 3) & 1)) << 17);
    ay_noise_rng >>= 1;
    ...
}
```

**注释更新**：
```c
/* CRITICAL: period=0 时应该每次更新 (>=0 永远为真) */
```

---

## 测试结果

**v3f（修复前）**：
- 播放 d-fr4gmEnt VGM
- 噪音几乎完全丢失

**v3g（修复后）**：
- 播放同一文件
- ✅ 噪音完整输出
- 音质接近 PC 原版播放器

---

## 经验教训

### 1. 不要盲目推倒重来

**错误做法**：看到问题就想用"更完整"的实现替换
- 1623 行 libvgm 原版 vs 300 行精简版
- 引入大量不必要的依赖

**正确做法**：
- 保留当前可工作的代码
- 精确定位具体问题
- 只修复必要的部分

### 2. 数据驱动的调试

**有效方法**：
1. 分析实际 VGM 文件内容
2. 对比代码行为和预期行为
3. 找到具体的差异点

**无效方法**：
- 猜测可能的问题
- 反复微调代码试错
- 依赖"感觉"

### 3. 理解原版实现的关键细节

**关键点**：
- AY8910 的 `period = 0` 不是"禁用"
- 而是"最高频率"（每次触发）
- tone/noise period=0 的处理必须对齐 libvgm

**来源**：
- STC32 项目的精简版有 bug（检查了 `> 0`）
- 不能盲目复制，需要对照原版验证

---

## 版本历史

| 版本 | 问题 | 修复 | 结果 |
|---|---|---|---|
| v3 | 噪声 LFSR 可能有误 | 移植 libvgm 标准 17-bit LFSR | 部分改善 |
| v3f | tone period=0 处理 | 移除 `freq > 0` 检查 | 音调改善 |
| v3g | **noise period=0 处理** | **移除 `noise_freq > 0` 检查** | ✅ 噪音完整 |

---

## 当前实现特点

### AY8910 核心（ay8910_v3.c）

**规模**：~300 行（vs libvgm 1623 行）

**完整性**：
- ✅ 17-bit LFSR 标准实现
- ✅ 指数音量表
- ✅ 混合逻辑 `(tone_out | tone_dis) & (noise_out | noise_dis)`
- ✅ period=0 处理（tone + noise）
- ✅ 立体声支持（L/M/R per channel）

**性能**：
- N900 600MHz CPU 轻松运行
- 无需 libvgm 完整的电阻网络模型

---

## 部署信息

**文件**：`build/vgmplayer_v3g_noise_fix.so`
- MD5: `e477ede074b799695adf62af6b5c4460`
- 编译时间：2025-01-16
- 部署路径：`/home/user/imrun/apps/vgmplayer/app.so`

**验证**：
- 测试文件：d-fr4gmEnt-re4Lt-me-DiHalt-Lite-2019-2-.vgm
- 结果：噪音完整输出 ✅

---

## 下一步

- [x] 噪音通道修复
- [ ] 通道音量调整失效问题
- [ ] 立体声 UI 改进（单选按钮，与音量同行）
- [ ] 插件热重载功能
- [ ] PULSE 后端无声音修复

---

**总结**：一个 `> 0` 检查导致整个噪音通道失效。精确分析 VGM 数据，对比 libvgm 原版逻辑，最终用最小改动解决问题。
