# N900 项目进度笔记

更新时间：2026-10-11

## 一、设备现状（已确认）

### 连接方式
- 内网地址：`192.168.101.111`
- SSH：`root / 123456`，端口 22
- **SSH 兼容性坑**：设备只支持老算法（ssh-rsa / ssh-dss 主机密钥，
  diffie-hellman-group14-sha1 / group1 密钥交换，3DES/AES-CBC 密码）。
  - Windows 自带 ssh 需加参数：
    `-o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedAlgorithms=+ssh-rsa`
    （但新版 ssh 无法交互输密码，非交互方案不可行）
  - **可用方案：Python + paramiko 2.12**（pip install "paramiko<3"），
    paramiko 4.x 已彻底移除老算法，不能用。
  - Samba (139/445) 也开着，可作文件传输通道。

### 系统信息
- 机型：Nokia N900（RX-51 主板），ARM Cortex-A8 (armv7l)
- 系统：Maemo 5，内核 2.6.28.10-cssu3（刷过 CSSU 社区更新，2012 年构建）
- 内存：235MB，free 只有 ~4MB（正常，Maemo 吃满缓存）
- 已开机运行正常，电池 47%（lshal 读数）

### 存储布局（32GB eMMC，三个分区）
| 分区 | 挂载点 | 大小 | 可用 | 文件系统 |
|---|---|---|---|---|
| ubi0:rootfs | `/` | 228MB | **仅 54MB** ⚠️ | ubifs |
| /dev/mmcblk0p2 | `/home`（/opt 链接至此） | 4.9GB | 3.1GB | ext3 |
| /dev/mmcblk0p1 | `/home/user/MyDocs` | 23.8GB | **21.7GB** | vfat |

- rootfs 极度紧张：装任何东西都不能进 `/`，必须进 `/home` 或 MyDocs
- MyDocs 是 vfat：**不支持符号链接、权限位**，适合放大文件（音频/ROM/备份），
  不适合装软件
- MyDocs 内容：`Android`（疑似旧 Nitdroid 残留）、`deb`（安装包）、
  `systemBackups`、`DCIM`、`Mac OS` 等（未细看）

### 已安装的可利用组件
- `python2.5`（/usr/bin/python2.5，pymaemo 链到 /opt）
- `mplayer`
- apt / dpkg（Maemo 5 包管理，extras 仓库现状待查——大部分 Maemo 源已死）

## 二、VGM 播放器方案分析（未动工，仅调研）

目标：在 N900 上播放 VGM（游戏芯片音乐，SN76489/YM2612/SPC 等芯片模拟）。

### ⚠️ 历史教训（2023 年实测，必读）
- 2023 年已在 N900 上编译跑过一次 VGM 播放：
  - **PCM 类芯片（SN76489 等）流畅**
  - **FM 类芯片（YM2612/YM2151 等）卡顿严重**
- 原因：FM 合成核心（尤其 Nuked 精度向核心）浮点运算量巨大，
  Cortex-A8 的 VFP 撑不住实时全精度模拟。不要默认性能"没问题"，
  一切以实测帧率为准。

### 修正后的方案：精简内核 + 压榨 A8（现行路线）

**核心选型原则：用速度向老核心，不用精度向新核心**
- YM2612：优先 **Gens 系（Gens KM / GENS 2.x 的 Sponge56 实现）**，
  它是整数运算为主的快速核心；弃用 Nuked OPN2（精度高但慢数倍）
- SN76489 / AY-3-8910 等PSG：成本本来就低，任意核心均可
- SPC700/SNES、GBDMG 等：选各自生态里经典的定点实现
- libvgm 支持按芯片选择核心（部分芯片有快/慢两套实现），
  编译时只编需要的芯片，砍掉用不到的模拟器减小体积和分支开销

**性能压榨手段（按优先级）**
1. 编译优化：`-O3 -march=armv7-a -mfpu=neon -mfloat-abi=softfp`，
   核心热点尝试 NEON 内联或定点化改造
2. 降采样：输出 22050Hz/16bit 起步（芯片音乐原始采样也才这么高，
   听感损失很小，CPU 直接减半）
3. 降精度：libvgm/VGMPlay 有精度开关（如 YM2612 的 DAC/包络精度选项）
4. 600MHz 若不够：可考虑 overclock 内核（CSSU 社区有 850/950MHz 内核），
   但这是最后手段，先做软件优化
5. 缓解措施：预读整个 vgz、播放线程 SCHED_FIFO 提权、绑核

**输出与打包（不变）**
- SDL 1.2 输出（Maemo 适配了 pulse 后端）；
  A8 上避免任何软件重采样——采样率让模拟核心直接产出
- 程序装 /home 分区，vgm 文件放 MyDocs，不碰 rootfs

### GUI 方案（已定）
- **PyGTK + Hildon 薄前端**：Maemo 5 自带 python2.5 + PyGTK + hildon 模块，
  零安装、零 rootfs 占用；Hildon 控件手指友好，可集成全屏/媒体键/任务切换
- **双进程架构**（吸取卡顿教训）：
  C 音频引擎（SCHED_FIFO）与 Python GUI 分离，FIFO/socket 通信；
  GUI 卡顿或退出不影响播放
- GUI 只做：文件列表（MyDocs）、播放/暂停/切曲、音量
- ~~备选：本地网页控制~~ —— 已否决：MicroB 是 Gecko 1.9（2009 年），
  无 HTML5/WebSocket，网页方案不可行且体验差
- 开发流程红线：**所有编辑、编译都在 PC 交叉环境完成，设备只收部署包**。
  严禁 2023 年那套"设备上装 gcc + 安卓手机改文本 + awk 拼前端"的流程

**方案 B：设备上直接编译**
- 需要装 gcc/binutils 到 /home（约 50-100MB，/home 还有 3.1GB 够用）
- 最大风险：**Maemo apt 源大部分已死**，gcc 能否装上待验证
  （repo.maemo.org 部分还在，CSSU 源可能活着）
- 优点：免去交叉工具链搭建；缺点：rootfs 有爆掉风险，编译慢

**方案 C：Python2.5 + 纯软模拟（不推荐做主力）**
- python2.5 写芯片模拟太慢，只适合做播放列表/前端 UI
- 可作为方案 A 的控制层：Hildon GUI 或终端 TUI 调用 vgmplay-cli

### 软件源与下载策略（国内无镜像，实测后修正）
- 实测：CSSU 镜像 merlin1991.at 从设备秒开（<1s）；
  repository.maemo.org 官方源基本已死——这才是当年 apt update 卡死的元凶
- **策略（按优先级）**：
  1. 首选：设备上完全不碰 apt。缺的头文件在 PC 端下载 dev 包解出，
     交叉编译后设备只链已有的 .so
  2. 真要装包：PC 下载 .deb → Samba/scp 推送 → 设备 `dpkg -i`
  3. 兜底：设备 apt 挂 PC 代理（`Acquire::http::Proxy "http://PC_IP:8888"`），
     PC 端带缓存
- 可选优化：注释掉源列表里已死的官方源
  （extras/extras-testing/Nokia SSU），只留 CSSU 镜像，apt update 即刻提速

### 待验证事项
1. ~~Maemo extras / CSSU apt 源是否还能连通~~（已实测：extras 死，CSSU 镜像活）
2. SDL 1.2 和 ALSA 头文件/库是否已装（`ls /usr/include/SDL` 等）
3. 音频输出路径：PulseAudio 是否在跑（`ps | grep pulse`）
4. `Android` 和 `systemBackups` 目录内容（可能与部署无关，仅清点）
5. MyDocs 在 vfat 上放 .vgm/.vgz 文件没问题（只读数据流），程序本体放 /home/opt

### 倾向的落地路线
1. 先验证 apt 源和 SDL/ALSA 环境（半小时内的探测）
2. 找 2023 年那次编译的残留（设备上或 PC 备份里）——如果旧工程还在，
   直接在其基础上换核心/调参数，比重起炉灶快得多
3. PC 端 armel 交叉编译：libvgm 精简配置（只编 PSG + 快速版 FM 核心），
   -O3 + NEON 参数，22050Hz 输出
4. 基准测试先行：单独跑 YM2612 核心测实时率（play time vs wall time），
   不过 1.0x 就继续砍精度/采样率，达标了再谈前端
5. 经 Samba/scp 推到设备，装到 `/opt/vgm`（落在 /home 分区）
6. 前端先做 CLI，满意后再考虑 Hildon GUI 包装

## 三、下一步
- [ ] 找 2023 年那次编译的工程残留（设备 MyDocs/deb 目录、PC 备份）
- [ ] 探测 apt 源可用性 + SDL/ALSA/Pulse 环境
- [ ] 搭 armel 交叉环境，先做 YM2612 快速核心的实时率基准测试

## 四、SkyRaid 飞机大战（已完成首个可玩版本）

### 环境（实测）
- PC：Python 3.14，pip 装 pygame 用 **pygame-ce**（官方 pygame 无 3.14 轮子），
  且 PyPI 直连被墙，需 `-i https://pypi.tuna.tsinghua.edu.cn/simple`
- N900：python2.5 + pygame 1.9.1 现成，零安装；
  设备上**没有 bash、没有 timeout**，远程跑用 sh + `sleep N; killall` 模式

### 游戏 skyraid.py（约 250 行，单文件，PC/设备双端兼容）
- 800x480 横屏，星空卷轴，三种敌机（血量/分值/弹幕不同），波次递增
- 操作：方向键/WASD + 空格，**触摸拖动**（N900 电阻屏映射为鼠标事件）
- 兼容性要点：python2/3 双兼容（无 f-string、注意整数除法）、
  用标准库 math 而非 pygame.math（1.9.1 没有）、SysFont 回退 Font
- **调试通道（已验证闭环）**：`--frames DIR` 每 30 帧导出 PNG →
  scp/sftp 拉回 PC → 直接看图。PC 端还能 SDL_VIDEODRIVER=dummy 无头自检
- 实测：PC 无头跑 300+ 帧正常、自动开火计分链路正常；
  真机跑 25 秒渲染约 450 帧（**约 18fps，纯 Python 基线，待优化**）

### 待办
- [ ] 性能：真机 18fps → 目标 30fps（降 star 数、减少 fill 调用、
      预渲染 sprite 到 Surface、降低 flip 开销）
- [ ] 打包成桌面图标（.desktop 文件）方便在设备上直接点开
- [ ] （可选）x11vnc 装上给用户远程围观真机画面

## 五、启动器注册（踩坑记录）
- Maemo 应用列表 = /usr/share/applications/hildon/*.desktop，但**有缓存**：
  装完必须 `killall hildon-home`（桌面进程自动复活重扫描）才出现在列表里
- Icon= 必须指向真实图标：放 /usr/share/icons/hicolor/48x48/hildon/<name>.png
  （参考本地样本：conky.desktop 教图标位置，osso-xterm.desktop 教完整字段）
- SkyRaid 已注册：skyraid.desktop + 48/124px 图标，点图标直接全屏启动

## 六、目录结构
- game/skyraid.py      游戏本体（PC/N900 双端）
- tools/test_autofire.py  无头自动开火冒烟测试
- assets/              启动器图标（48/124px）
- shots/               截图缓存（gitignore，不入库）
- PROGRESS.md          本笔记：设备档案 + 全部方案与踩坑
