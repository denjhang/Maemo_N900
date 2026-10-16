# Maemo_N900

诺基亚 N900 (Maemo 5) 复古智能机开发档案馆 —— 让 2009 年的口袋 Linux 在 2026 年继续发热发光。

## 这是什么

一个围绕 Nokia N900 / S60 / J2ME 老派移动平台的完整开发档案：

- **真机可跑的 ImGui GLES1 应用运行时**（IMRUN v0.4.46r）—— 插件化 VGM 播放器 + 4 音频后端
- **PULSE 音频后端**（v0.4.46r 新增）—— 系统音量实时响应，48000Hz 零重采样流畅播放
- 自建 **Fremantle 交叉编译工具链**（无需任何 Nokia 官方 SDK）
- Symbian S60v3 生态考古：模拟器、补丁、闭源应用开膛分析
- J2ME 时代的音频输出路线研究（实时合成 / MIDI 转译 / 离线渲染）

## 核心成果（v0.4.46r）

### IMRUN —— ImGui Runtime on N900

**4 音频后端并存**：
- **SDL/plughw**（默认）—— PicoDrive 模式，独立后端 0 延迟，应用内音量
- **PULSE (sink.hw0)**（新增）—— 系统音量实时响应，硬件音量键 + 滑条双控
- **ALSA (plughw)**—— 直连 ALSA，调试后端
- **libao (vgmplay)**—— 兼容 vgmplay-maemo 路径

**用户验证**："不仅软件内可以调音量，系统音量也响应了，而且还很顺畅。" ✅

**技术亮点**：
- 解决 4 个致命根因（跨线程死锁、Nokia 路由卡死、重采样噼啪、声卡占用冲突）
- 48000Hz 零重采样 → 600MHz 单核流畅播放 20 秒无断流
- AOTEST 自动化压力测试：4 后端 × 10 轮 × 快速 PLAY/STOP 全通过

**完整文档**：
- `docs/PULSE_BACKEND_IMPLEMENTATION.md` —— 实现详解（根因分析 + 代码结构 + 故障排查）
- `docs/AUDIO_BACKENDS.md` —— 架构全貌（4 后端对比 + PicoDrive 先例论证）

## 设备档案

| 项目 | 状态 |
|---|---|
| 机型 | Nokia N900（RX-51，OMAP3430 A8@600MHz，SGX530，256MB） |
| 系统 | Maemo 5 + CSSU（glibc 2.5，内核 2.6.28-cssu3） |
| 连接 | SSH root@192.168.101.111（paramiko 兼容老算法） |
| 存储 | rootfs 228MB(慎用) / /home 5GB / MyDocs 24GB |

## 目录结构

```
game/               skyraid —— pygame 横版飞机大战（PC/N900 双端，真机可玩）
symbian/            PyS60 远程 agent（TCP 执行/截屏/文件通道）
j2me/               （规划中）S40 实时 PSG 播放器
build/              交叉编译工作区（hello/alsa_test/libao 等）
crossenv/           交叉编译工具链配方（已验证真机运行）
tools/              辅助脚本
research/           资料库（本地留存，不入库）：源码树/镜像/开发包
PROGRESS.md         全程开发笔记（廿五章，含全部踩坑实录与判决书）
```

## 交叉编译环境（核心成果）

**无需 Nokia 官方 SDK**，现代工具链直编 Fremantle：

```
WSL2 Ubuntu 22.04（F:\WSL\MaemoBuild）
+ gcc/g++-arm-linux-gnueabi 11.4（软浮点 armel）
+ Fremantle sysroot：277 个官方 -dev 包 + glibc 2.5 运行库
  （来源：plan9.de 镜像的 repository.maemo.org SDK 仓库存档）
```

### 三个关键空子（详见 crossenv/README.md）

1. libc.so 链接脚本用 `=/lib/libc.so.6` 强制 sysroot 映射
2. `-nostartfiles` + 自写 6 行 `_start`，绕开老 crt 与新 binutils 的属性断言
3. `-Wl,--no-as-needed` + 设备真身 libdl——宿主 glibc 2.35 的 libdl 是空壳

产物特征：`GLIBC_2.4` 唯一符号版本、interp=`/lib/ld-linux.so.3`——真机直跑。

## 已验证里程碑

- ✅ skyraid（pygame）真机 800×480 满屏运行
- ✅ **2023 年的 vgmplay 复活**：自研 150 行 mini-libao（直连 ALSA→pulse），
  YM3812 曲目 48kHz 实时播放（FM 核待换芯，见 PROGRESS 廿三实测）
- ✅ 交叉工具链产出真机可跑的 armel 二进制
- ✅ S60v3 模拟器 sis 开膛（picodrive/gpfce/AntSnes/fMSX 全部完整提取）
- ✅ ImGui+软渲染 UI 方案定案（GLES1.1 硬加速为功耗优先升级路）

## 硬件性能实测备忘

- 波表/PSG 类合成：600MHz A8 实时流畅 ✅
- MAME 系 OPL2 (YM3812) FM 合成：卡顿 ❌ → 待换精简整数核（对照实测已入库）
- SGX530 GPU：GLES1.1/2.0 可用，功耗优先的 UI 渲染首选

## 相关仓库

- [denjhang/vgm_ws_to_mid_full_version](https://github.com/denjhang/vgm_ws_to_mid_full_version) —— WonderSwan VGM→MIDI 转换器（本项目的 MIDI 转译路线原型）

## 笔记

全部开发过程、判决书、踩坑实录见 [PROGRESS.md](PROGRESS.md)（廿五章，持续追加）。
