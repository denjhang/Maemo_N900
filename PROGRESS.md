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

## 七、GitHub 建仓/Push 流程（本机实测）
- 建仓：从 Git 凭据管理器取 token（`git credential fill`）→
  `POST https://api.github.com/user/repos`，无需 gh CLI
- push：直连被重置、7890 不通，**必须走 `http.proxy=http://127.0.0.1:7892`**
  （github-accel 技能记载的 push 专线）
- 远程：https://github.com/denjhang/Maemo_N900 (main)

## 八、PYMO 考证（2026-10-11 付费搜索查证）
- **PYMO** = "手机上的 GalGame 引擎"，官网 pymogames.com
- 本质：用 **Python（PyS60）写的 ONScripter 解释器**——不是原生 C 移植！
  基于 skydark 的 eustia-test0.04 / 枫笛 1.5 版 ONS 移植脚本发展而来
- 平台：塞班 S60v3/v5（PyS60）+ Android，跨平台；能完整输出语音和立绘
- 意义：证明 PyS60 在 S60v3 300-400MHz ARM11 上跑得动 ONS 文字游戏，
  塞班"Python 上限很高"的用户记忆属实
- 相关：原生 C 路线的 ONScripter 官方移植覆盖 Win/Linux/Android/iOS/PSP 等
  （GPL，AUR 有 onscripter-en 包）；KiriKiri 系有 Kirikiroid 血统延续至安卓
- 启示：N900（A8 + 真 Linux）跑 ONScripter 引擎同理没有障碍

## 九、PYMO 引擎解包分析（源码级，2026-10-11）
- 源码已落地 research/pymo-master（gh-proxy 加速下载 master.zip，36MB）
- **引擎本体 = symbian/app.py 单文件 4968 行 Python 2**（PC 版 pc/main.py 5095 行 pygame）
- 仓库 symbian/PythonForS60/ 里**直接捆着 PyS60 2.0.0 + ScriptShell 的官方 sis**
  ——塞班端"引擎安装包"问题就此解决，免签名官方包
- 依赖全是 PyS60 原生模块：appuifw/graphics/e32/audio，无任何第三方 C 扩展
- 功能实况（远超预期）：逐字符文字渲染+字体颜色/速度可调、立绘+遮罩、
  CG 鉴赏、存档槽、设置菜单、MP3 BGM/语音、WAV 音效、**视频播放**、
  自定义 pak 资源包（u32 数量 + [GBK文件名32B + u32偏移 + u32长度] 条目，
  与 120Y 的 bg.pak 实测头完全吻合）
- 性能真相：纯解释执行跑在 300-434MHz ARM11 上，靠"静态背景+对话框节奏"
  的 galgame 天然适配才流畅；文字是逐字符画+redraw，这是它吃 CPU 的地方
- 游戏包实测（120日元，国人 chen_xin_ming 移植、GGM 论坛汉化）：
  s60v3 版 320x240/字号16，s60v5 版 540x360/字号26——同引擎按平台出双资源包
- 手头游戏资源：F:\下载\120日元（120日元全集）、F:\下载\pymo（eden/Ever17/
  把所有的歌献给未来的你，移植者：黑之契约者/墨水管等，百度盘存档）
- 启示：N900 的 python2.5 + pygame 比 PyS60 的 appuifw 快一个量级以上，
  PYMO PC 版（pygame）理论上稍加移植即可在 N900 跑 120日元——待验证

## 十、pymogames.com 官网镜像（2026-10-12）
- 已整站镜像至 research/pymogames.com/（173 文件/40MB，链接已本地化，离线可开）
- 内容：首页游戏图鉴（120日元/3days/Ever17 等截图齐全）、install.html 安装指南、
  changelog（引擎版本史）、FAQ、**《PYMO教程》PDF 2.5MB**（中文，含脚本命令文档）
- 站内无直接 sis/zip 包：引擎与游戏分发全走百度网盘（官网留有两处 pan 链接：
  s/1oiuhDUrveNeCDzK9gdAhag、s/1tojpORlo6SLbZI221beNpQ，需手动转存）
- 但引擎本体已不依赖网盘：research/pymo-master 内含 symbian/app.py 源码 +
  PyS60 2.0.0 全套官方 sis（见第八节）

## 十一、DeaDBeeF for N900 —— 正经播放器的参考答案（2026-10-12）
- 来源：https://23.fi/deadbeef-n900/（petteri 的移植页），
  deadbeef_0.4.2-2_armel.deb 已下载解包（research/deadbeef-n900/）
- 源码：github.com/petteri/DeaDBeef（待克隆）；deb 内含 deadbeef.h 插件 API 头文件
- **架构范本**：核心 + 动态插件（.so），解码器/输出/UI 全是插件：
  - 解码插件：ffmpeg、flac、vorbis、mpgmad、musepack、sndfile、tta、shn、
    ffap(猴子猴)、dumb/mod、**adplug(OPL 芯片)**、**sid**、**vtx(AY/YM 芯片)**、
    **gme(游戏音乐全家桶：VGM/NSF/SPC...)**、wildmidi
  - 输出插件：**pulse**(N900 原生)、ao；UI：gtkui；另附 supereq 均衡器、
    artwork 封面、lastfm、vfs_curl 网络流、hotkeys、shellexec
- **重磅**：gme 插件意味着装上这个 deb 的 N900 理论上已能直接播 VGM/NSF/SPC
  ——我们的 VGM 播放器可先装它听为快，自研播放器则学它的
  "核心+插件"分层（解码器独立于输出与 UI，正是精简内核路线要的形态）
- 安装位置 /opt/deadbeef，rootfs 无需变动，附 deadbeef.desktop
- 源码补充：petteri 的 maemo 分支已在 GitHub 失传（链接 404、Wayback 无存档）。
  已改取官方 DeaDBeeF-Player/deadbeef 的 **0.4.2 标签源码**（research/deadbeef-n900/
  deadbeef-0.4.2/），与 deb 内二进制严格同版本：plugins/gme（VGM/NSF/SPC 解码，
  我们最直接的参考）、plugins/pulse（PCM 输出）、plugins/adplug+vtx+sid（芯片音乐）、
  deadbeef.h（插件 API）。petteri 的 hildon 适配差异可日后用 deb 内 .so 逆向比对。

## 十二、Maemo 5 正经开发栈（参考清单，2026-10-12 归档）
来自 Copilot 调研 + 本项目实测双重印证。做"正经 Maemo 应用"就是这套组合：
- UI：Hildon（libhildon，Maemo 原生 UI，建于 GTK+ 2 之上）+ GTK+ 2.x
- 系统集成：libosso（应用生命周期/系统服务）、D-Bus + dbus-glib（IPC/通知）
- 多媒体：GStreamer 0.10、ALSA/PulseAudio；图形加速：OpenGL ES/EGL（SGX530）
- 电话/网络：oFono、Telepathy、NetworkManager/connman
- 语言路线：入门 python-gtk2/python-hildon/python-osso → 深入 C+GTK+Hildon+libosso
  → C++/Qt4（maemo-qt4）
- 工具链：Maemo Fremantle SDK + Scratchbox2（交叉编译）、sbrsh/qemu 仿真、
  dpkg-buildpackage 打 .deb、gdbserver 交叉调试
- 代表性开源应用（学习素材）：MicroB（Mozilla Fennec 移植浏览器，最复杂范例，
  wiki.maemo.org/MicroB、hg.mozilla.org/projects/maemo）、GPodder、Maemo Mapper
- Copilot 推荐的"最强单件"是 MicroB；但按我们的目标（音频/播放器），
  DeaDBeeF N900 移植是更贴身的范本（见第十一节）
- 本仓库已有对应实证：skyraid 验证了 Python+SDL 快速迭代路线；
  deadbeef 解包验证了 C+GTK2/Hildon+pulse 的正经路线；二者拼出完整地图

## 十三、Maemo 软件源存活地图（2026-10-12 并行探针实测）
**活的（金矿）：**
- plan9.de SDK 源 ✅✅✅ 最高价值：
  https://maemo.plan9.de/apt-mirror/mirror/repository.maemo.org/
  dists/fremantle/{sdk,tools}/free/non-free 全部有 Packages.gz（armel，
  sdk/free 索引 283KB 实测 200）——**官方 SDK 仓库复活了**，交叉编译要的
  libhildon-dev/libosso-dev 等头文件库全在这
- cloud-7.de ✅ https://maemo.cloud-7.de/maemo5/
  离线安装包仓库（opt/usr/sbin/patches_n_tools/HAM-catalogs），
  含 muarf_all.install 等目录配置器
- merlin1991.at ✅ 仅 CSSU（community-thumb/devel）
**死的：** repository.maemo.org 全线、downloads.maemo.nokia.com、
merlin /extras 路径、maemo.muarf.org（直连 000/代理 502，仅存于
cloud-7 的 catalog 配置文件里）、viniciuspaes（403 全站）、
coderus.openrepos.net（301 转 https 后未验）
**注意**：plan9 是 https，设备上 busybox wget 不一定能用；
正确姿势 = PC 端下 deb → 推到设备 dpkg -i（见第七节源策略）

## 十四、S60v3 模拟器"性能天花板"样本（2026-10-12 收藏）
inbox/s60v3-emulators/ 四件套（E72 等 v3 机型实测可跑）：
- picodrive 1.51 s60v3 —— MD 模拟器，Cyclone 68000 手写 ARM dynarec，
  220MHz N73 全速跑 MD，"效率优化牛逼"的教科书本体
- fMSX 3.5 s60e3 —— MSX 模拟器（配 inbox/msx-aleste 的 Aleste 2 ROM）
- gpfce 0.3.3 —— NES 模拟器
- AntSnes 0.6.3 —— Summeli 的 SNES（与 research/Summeli/AntSnes 源码对应）
学习点：这些在无 GPU、220-434MHz v3 机器上全速的移植，核心技巧=
手写 ARM 动态重编译 + 定点数 + 精简显存路径。对 N900 上的
C/GLES 版游戏是降维参照；对 VGM 的 FM 核心（Gens 系快速整数实现）同理。

## 十五、前端选型决议 + 硬件加速真相（2026-10-12 讨论定案）
### 前端选型（用户拍板）
- **SDL 是正桥，GTK 是古董——不碰**。Qt 只在贡献 NIKINIKI 时才学
- 候补现代方案：Dear ImGui 风格 immediate-mode 手绘（叠加在 SDL1.2+GLES 上）；
  TUI/ncurses 保底
- 音乐播放器形态：VGM 核心编成解码模块 + SDL 画界面 + pulse 出声，
  键盘快捷键（空格暂停/方向键切曲）。furnace tracker 移植 = 自己找罪受，放弃

### N900 硬件加速家底（设备实测）
- SGX530 全套在位：libGLES_CM(ES1.1)、libGLESv2(ES2.0)、libEGL、
  **libSDL_gles**（SDL 的 GLES 上下文，已装）、Clutter eglx
- 加速表：SDL1.2 普通 ✗ | SDL_gles ✓ | Clutter ✓ | Qt4 半残 | C 直写 EGL 完全体
- **pygame 卡顿根治判断**：卡顿 = 画图开销(大头,GPU可根治) + Python解释开销
  (小头,GPU管不着)。pygame 无 GLES 后端 → Python 只能治一半；
  **根治 = C + GLES1.1 固定管线重写渲染**，逻辑照抄 pygame 版
- 决议：skyraid C 版直接 GLES 渲染，跳过 SDL 软绘

### 塞班分世代图形路线
- S^3/Anna/Belle：Qt4.7+/QML（NIKINIKI 模式），GPU=BCM2727 VideoCore III
  （GLES1.1/2.0+OpenVG），健康度最高
- S60v5：**多数机型无 3D GPU**（5800/N97），正解 = Avkon C++ / Open C / J2ME；
  Qt4.6 可装
- S60v3：分裂——N95/N82 有 PowerVR MBX(GLES1.1 固定管线)，海量中低端无 GPU；
  **J2ME 是 v3 游戏绝对王者**（MIDP2），原生走 AVKON/Open C
- **统一战线**：C/SDL 核心一份 → N900 直编 + 塞班走 SDL_Symbian(Summeli 移植层，
  ONScripter 同款路线) + Leste N9 还是 SDL。三平台各配薄渲染壳

### 安卓双系统为何比 Maemo 流畅（架构代差，非优化强）
1. 图形：安卓 2.x SurfaceFlinger+SGX 硬件合成 vs Maemo GTK+X11 纯 CPU 软绘
2. 事件：安卓为触摸而生 vs Maemo 背 X11+GTK 桌面 legacy
3. 内存：安卓激进 LMK 保前台 vs Maemo 万物常驻摊薄 235MB
4. 结论：Maemo=桌面 Linux 缩小塞手机，安卓=围手机从头造。
   SDL 路线 = 绕开桌面遗产、最接近安卓式的 Maemo 开发

### N9/MeeGo 备忘
- N9 开发 = Qt4.7+QML，aegis 沙箱；NIKINIKI 的 Qt 功夫直接迁移
- 历史：2011 燃烧平台转向 WP，Qt 战略作废；若全力押 Qt，塞班^3 与 MeeGo
  同技能栈平滑过渡。N9 = 发布即葬礼的绝唱；社区续作为 Maemo Leste

## 十六、PyS60 能力全谱（给塞班 agent 用的 API 底牌）
- 系统级：完整文件系统(os/shutil 直达 C:/E:)、e32.drive_list、set_home_time、
  sysinfo(电量/信号/IMEI)；官方发过 PythonScriptShell_2.0.0_high_capas.sis
  高权限变体 → "Python 清扫系统目录"坐实
- 通信：inbox 读短信、messaging 发短信/彩信、telephone 拨号、logs 通话记录、
  contacts 通讯录、calendar 日历
- 硬件：camera 拍照、sensor 加速度计、location 基站定位、positioning GPS、
  topwindow 悬浮层、keycapture 全局按键、profile 情景模式、audio 录音
- 网络：socket/ssl、蓝牙 RFCOMM/OBEX（官方 BT console）、红外、e32db
- 上限判断：GUI=Canvas 手绘(简陋但能用)；游戏=2D 软绘 10-15fps；
  VGM 实时合成=不可能(无裸 PCM 输出+FM 算不动) → 塞班播放器只能放预转文件

## 十七、SIS 解包工具链（自研，已验证）
- 工具：GitHub zixing131/sisinfo（Python 库）+ 自研 extract_sis2.py
- 两处 py2 兼容补丁（unichr/ord-int），配对算法 = 按 SISFileDescription 的
  uncompressedLength 与数据单元解压长度精确匹配（op=8 为空操作不装）
- 战果：4 个 S60v3 模拟器 sis 全部 10/10、14/14、8/8、5/10 提取成功，
  sys/bin/*.exe 均为 E32Image（magic 'z\x00\x00\x10'）：
  picodrive.exe 485KB、gpfce.exe 339KB、AntSnes.exe + AntOpenGLES.dll(GLES!)、
  fMSX 全套含 MS2EXT.ROM/DEFAULT.FNT
- 工具留存 research/sistools/{sisinfo-master,extract_sis2.py}，可复用于任意 sis

## 十八、S60 开源项目收藏（Copilot 清单 + 实测分支）
正在下载 research/s60-repos/（gh-proxy 并行）：
- PSP-Archive/PicoDrive-notaz(main, 1.5MB) —— S60 移植构建文件所在地
- cowboyh4t/JTube —— 真 S60 Java(MIDlet) 应用
- Hexadecinull/QRScan(main) —— symbian/ 目录 = S60 3rd ECam+ZXing
- Klozz/Symbian-Open-Source —— 塞班生态资源汇总
- wayfinder/Wayfinder-CppCore-v2(12MB) —— 大型真实 S60 C++ 应用（含 S60 头文件用法）
- yomcube/ClassiCube(38MB) —— 含 Symbian S60 编译章节
- libwbxml/libwbxml —— 含 Nokia S60 SDK 构建流程
### Symbian SDK 考据（Copilot 调研归档）
- SDK 本体不在 GitHub；版本对应表：S60 3.1=Symbian 9.2, 3.2=9.3, 5.0=9.4,
  5.3=S^3, 5.5=Anna/Belle
- Qt for Symbian 关键路径：EPOCROOT/epoc32/{include,release/armv5,release/winscw,tools}
- Qt SDK for Symbian 4.7.3（Symbian^1）路径范式：C:/QtSDK/Symbian/SDKs/Symbian1Qt473/epoc32
- NIKINIKI docs/archive/wiliwiliforsymbian3 = 现代可用的完整构建/签名/CODA 调试文档

## 十五、前端选型决议 + 硬件加速真相（2026-10-12 定案）
- 用户拍板：SDL 是正桥，GTK 古董不碰；Qt 仅在贡献 NIKINIKI 时学；
  候补 = ImGui 风格 immediate-mode 手绘 / TUI 保底
- 播放器形态：VGM 解码模块 + SDL 界面 + pulse 出声 + 键盘快捷键；
  furnace tracker 移植 = 自找罪受，放弃
- N900 加速家底（实测）：SGX530 全套在位（GLES1.1/2.0+EGL+libSDL_gles+Clutter eglx）
- 加速表：SDL1.2 普通✗ | SDL_gles ✓ | Clutter ✓ | Qt4 半残 | C 直写 EGL 完全体
- pygame 卡顿 = 画图开销(大头,GPU根治) + 解释开销(小头,GPU管不着)；
  pygame 无 GLES 后端 → Python 只治一半；根治 = C+GLES1.1 固定管线重写渲染。
  决议：skyraid C 版直接 GLES，跳过 SDL 软绘

## 十六、塞班分世代图形路线
- S^3/Anna/Belle：Qt4.7+/QML（NIKINIKI 模式），GPU=BCM2727（GLES+OpenVG）
- S60v5：多数无 3D GPU（5800/N97），正解 = Avkon C++/Open C/J2ME；Qt4.6 可装
- S60v3：N95/N82 有 PowerVR MBX(GLES1.1)，中低端无；J2ME 是 v3 游戏王者
- 统一战线：C/SDL 核心一份 → N900 直编 + 塞班 SDL_Symbian + Leste N9，三平台薄壳

## 十七、安卓双系统为何比 Maemo 流畅（架构代差，非优化强）
- SurfaceFlinger+SGX 硬件合成 vs GTK+X11 纯 CPU 软绘（每帧十倍像素工作量差）
- 触摸原生事件 vs X11+GTK 桌面 legacy；激进 LMK vs 万物常驻摊薄 235MB
- SDL 路线 = 绕开桌面遗产，最接近"安卓式"的 Maemo 开发

## 十八、PyS60 能力全谱（塞班 agent 的 API 底牌）
- 系统：完整文件系统直达 C:/E:、set_home_time、sysinfo；
  官方发过 high_capas 高权限变体 → "Python 清扫系统"坐实
- 通信：inbox/messaging/telephone/logs/contacts/calendar
- 硬件：camera/sensor 加速度/location 基站/positioning GPS/topwindow 悬浮/
  keycapture 全局键/profile/audio 录音
- 网络：socket/ssl/蓝牙 RFCOMM/红外/e32db
- 上限：GUI=Canvas 手绘；游戏 2D 软绘 10-15fps；VGM 实时合成不可能
  （无裸 PCM 输出+FM 算不动）→ 塞班只能放预转音频

## 十九、SIS 解包工具链（自研已验证）
- zixing131/sisinfo（Python 库）+ 自研 extract_sis2.py；两处 py2 兼容补丁
- 配对算法 = FileDescription.uncompressedLength 与数据单元解压长度精确匹配
  （op=8 空操作不装；extract_sis.py 初版按序配对会错位，弃用）
- 战果：4 个 S60v3 模拟器 sis 全部完整提取，sys/bin/*.exe 均 E32Image：
  picodrive.exe 485KB / gpfce.exe 339KB / AntSnes.exe+AntOpenGLES.dll(GLES) /
  fMSX 含 MS2EXT.ROM。工具在 research/sistools/，可复用任意 sis

## 二十、S60 开源仓库收藏（7/7 到手，research/s60-repos/）
- PicoDrive-notaz(main,7.6MB)：platform/s60 = 塞班版构建文件
- JTube(592KB)：真 S60 Java MIDlet 应用
- QRScan(655KB)：symbian/ = S60 3rd ECam+ZXing
- Symbian-Open-Source(2.5MB)：塞班生态资源汇总
- Wayfinder-CppCore-v2(46MB)：大型真实 S60 C++ 应用，含全套 S60 头文件用法
- ClassiCube(19MB)：src/Window_Symbian.cpp 等塞班后端
- libwbxml(2.3MB)：symbian/ = Nokia S60 SDK 构建流程
### Symbian SDK 考据（Copilot 调研归档）
- SDK 本体不在 GitHub；版本表：S60 3.1=Symbian 9.2、3.2=9.3、5.0=9.4、5.3=S^3、5.5=Anna/Belle
- Qt for Symbian 路径范式：EPOCROOT/epoc32/{include,release/armv5,release/winscw,tools}
- Qt SDK 4.7.3 范式：C:/QtSDK/Symbian/SDKs/Symbian1Qt473/epoc32
- NIKINIKI docs/archive/wiliwiliforsymbian3 = 现代可用的完整构建/签名/CODA 调试文档

## 廿一、全平台 VGM 播放器可行性研究（2026-10-12 大调研）

### 硬件/开发栈总表
| 平台 | SoC/CPU | RAM | GPU | 语言 | 前端 | 音频API |
|---|---|---|---|---|---|---|
| N900 Maemo5 | OMAP3430 A8@600 | 256MB | SGX530 | C/C++, Py2.5 | GTK/Hildon, SDL, Qt4.6 | pulse/ALSA ✓裸PCM |
| N9 MeeGo | OMAP3630 A8@1G | 1GB | SGX530 | C/C++, Qt4.7/QML | QML/SDL(社区) | pulse ✓ |
| S^3(N8/701/808) | ARM11@680→1.3G | 256MB | BCM2727 | Symbian C++, Qt4.7/QML, J2ME, PyS60 | Qt/QML, Avkon | DevSound ✓裸PCM |
| S60v5(5800/N97) | ARM11@434 | 70-128MB | 无 | Symbian C++, OpenC, J2ME, PyS60, Qt4.6 | Avkon | DevSound ✓ |
| S60v3(N73/N95/E71) | ARM9@220-369 | 64-128MB | 仅N95等MBX | Symbian C++, OpenC(9.2+), J2ME, PyS60 1.4 | Avkon | DevSound ✓ |
| S60v2(6600/N70) | ARM9@123-220 | 10-32MB | 无 | Symbian C++(无OpenC), J2ME, PyS60 1.x | Avkon前奏 | CMdaAudioOutputStream |
| S60v1(7650/N-Gage) | ARM9@~123 | 4-8MB | 无 | Symbian C++(最原始), J2ME MIDP1 | 无框架 | 流式PCM(挤) |
| S40v3/v5/v6(6300/5310) | ARM9@200-460 | Java堆1-5MB | 无 | **仅J2ME** | MIDP UI | MMAPI(wav/mp3流) |
| Asha全触(305/311) | ARM11@1G | 128MB+ | 无 | 仅J2ME+Asha UI | Asha Full Touch | MMAPI ✓ |
| Asha平台(501等) | 1G | 4GB存储 | 无 | 仅J2ME+Asha UI | Asha UI | MMAPI ✓ |

### VGM 播放器落地判定（VGM=SN76489/YM2413/YM2612/YM2151/AY等）
- **N900 ★★★★★**：C+Gens系FM核心+pulse，全芯片全实时。deadbeef+gme 已证明；
  自研播放器是"顺便的事"。基线参考：我们手里 2023 vgmplay ARM 二进制
- **N9 ★★★★★**：1GHz+1GB，最舒服；Qt/QML 前端+SDL 引擎，全芯片实时无压力
- **S^3 ★★★★**：原生 C++ 引擎(整数FM核)+DevSound：YM2612 实时可保(参照Gens
  在 133-233MHz 奔腾全速史)；Qt/QML 做界面；808(1.3G) 更宽裕
- **S60v5 ★★★☆**：434MHz 跑 Gens 系整数 FM 核≈实时临界但可过(降采样32000Hz
  保险)；PSG/YM2413 宽裕。J2ME 版=PSG 可、FM 不可(Java合成太慢) → J2ME 只做
  预转播放器(把VGM离线转WAV再MMAPI播)
- **S60v3 ★★★**：330MHz 级(FP1+ 9.2)同上策略；220MHz(N73) 只保 PSG+YM2413，
  YM2612 建议预转。OpenC 让 C 引擎能编译(v3.2+)；v3.0/9.1 只能纯 Symbian C++
- **S60v2 ★★**：123-220MHz+10-32MB：只做 PSG/YM2413 播放器(手写8bit合成核)，
  FM 全预转。CMdaAudioOutputStream 16bit 流式可行
- **S60v1 ★**: 123MHz+4MB：英雄模式 PSG 单芯片播放器；或干脆只做"VGM信息查看器"
- **S40全系/Asha ★★**：**只能 J2ME**。结论分两档：
  ① 实时 PSG 合成播放器(参照 vNes/MeBoy 在 S40 全速跑模拟器的实证，SN76489
  的Java合成比NES整机模拟便宜一个量级，必然实时)
  ② FM 类 = J2ME 写离线转换器(VGM→WAV)再 MMAPI 播放，"格式支持"保住，实时性放弃

### 总路线图
1. N900：主线，C+SDL/GLES 前端实验场（gme 基线 + 自研精简核心）
2. 引擎与 UI 分离：核心输出 PCM 环形缓冲，前端可以是 SDL/QML/Avkon/Canvas
3. 移植顺序：N900 → S^3(同为C++) → v5/v3(OpenC) → J2ME 版(PSG实时+FM预转)
4. 各平台音频共性：全部要 16bit 小端 PCM 环形缓冲 + 独立解码线程/定时器

### 廿一节勘误与源码实证（读 PicoDrive-notaz platform/s60 + uiq3 之后）
**"E72 全速完整声音"的源码级解释（PicoDrive S60V3）：**
- CPU核 = **Cyclone**（手写 ARM 68000 dynarec）+ **DrZ80**（手写 ARM Z80），
  以预编译 .o 链接（cyclone.o/DRZ80.o/ym2612_helper.o 直接进 mmp）
- YM2612/SN76489 核心来自 MAME（整数实现，非高精度核）
- 声音配置（install.txt 原文）：YM2612+SN76489 默认全开，
  **采样率 8/11/16/22kHz 四档，默认 8kHz**（不是 44.1k！），自动跳帧
- 所以"全速完整声音"= FM+PSG 全开 @ 8-22kHz + 跳帧，这正是
  "精简整数核+降采样"路线的实战背书——我们的 VGM FM 策略与之一致

**塞班音频输出参考实现（platform/uiq3/engine/audio_mediaserver.cpp，293行）：**
- 输出 API = CMdaAudioOutputStream（MEDIACLIENTAUDIOSTREAM.LIB），
  TMdaAudioDataSettings 支持 8000/11025/16000/22050/44100Hz 五档
- 双缓冲（iSoundBuffers）+ KMaxLag 欠载阈值 + 连续 underflow 计数保护（50次熔断）
- 这 293 行就是塞班 VGM 播放器音频层的完整模板：我们只需
  把它的喂料端从 Pico 的 mix 接到 VGM 核心即可
- mmp 链接姿势：LIBRARY mediaclientaudiostream.LIB + 静态库 cyclone.o 等
**勘误**：原表"S60v3 FM 实时可保"成立，但注明"声音完整=8-22kHz 降采样+可跳帧"，
非 44.1kHz 满配。

## 廿二、引擎血统：STC_Chiptune（VGM 挑战的底气，2026-10-12 收录）
- 位置：E:\working\vscode-projects\STC_Chiptune（Denjhang 架构+硬件 × Claude 固件）
- **STC32G12K128：C251 核 38MHz IRC，4KB SRAM+8KB XRAM**，六音源共享
  17640Hz ISR（每中断预算 56.7µs，Keil OPTIMIZE(8,SPEED) 压线达标）：
  AY8910 / SN76489 / 自定义2-Op FM(16voice) / YM2413 OPLL(VGM寄存器级兼容,
  9ch+rhythm) / Gigatron TTL / WT 波表 / ADPCM / BRR(SNES SPC DSP bit-exact)
- 8-bit PWM=权衡后的最优解（10-bit 载波 37kHz 落入可听段出底噪，38MHz IRC
  锁不住 PLL）；新板 STC32G144K246 实跑 **72MHz HPLL**（代码实锤
  usb_cdc_test/src/main.c MAIN_Fosc=72000000L，HPLLPDIV=5）——超频立项，
  配 I2S+TLV320AIC23B codec+纯源码 USB CDC
- **正视差距**：超频到 72MHz 的 C251 也够不着最便宜的 S40（200-460MHz ARM9+cache）。
  MCU 项目=极限工程（每微秒每字节预算内采购），手机=生态工程（瓶颈在架构不在硅）
- **VGM 主线已通**：tools/vgm_player.py PC 端解析 VGM 命令流走 UART 喂芯片
  实时合成（AY8910/SN76489/OPLL 曲目库在 vgm/），VGM_COMMAND_TABLE_libvgm.md 命令表齐备
- **对 N900 引擎的定论**：精简内核不用学别人——ay8910.c/sn76489.c/fm.c 就是
  蓝本。移植到 600MHz A8+256MB 后约束全部松绑十倍以上：44.1kHz 立体声、
  加 YM2612(MAME整数核,deadbeef 参考)二引擎、混响等全部解锁
- 血统谱系：74 门电路 PSG/WSG → C251 全速 YM2413 → STC32G 六核博物馆 →
  N900 VGM 引擎（架构同源，镣铐逐代放宽）

### 廿二补充：YM2413 重写规模 + SID + USB 声卡化（144K246 最新状态）
- **YM2413 核心几乎全重写：359 行**（MAME 原版 ~2000 行 / emu2413 ~900 行）
  核心手法 = `conv_vol[32][64]` 预计算表把 sin×电平融为一次查表，**运行时零乘法**；
  包络轮询分摊（每 tick 一通道）；feedback 移位代替除法——C251 查表法的体系化
- **C64 SID 核已上板**：usb_cdc_test/src/sid.c + sid_filter_table.h +
  sid_tables_code.h，配 SID_HANDOFF/INTEGRATION_STATUS/REFERENCE_COMPARE 三文档
  （模拟滤波器查表化）
- **出声形态升级**：22050Hz + USB CDC 纯源码声卡（无 LIB 依赖）——
  72MHz 超频的 144K246 现在是 USB 声卡形态的八芯片合成器
- 结论强化：N900 引擎的合成核 = 直接沿用这套"查表融乘"范式；
  STC 版核心可移植性=纯 C，几乎免改造

### 廿二补充：J2ME 参考项目落实（MIDPlay）
- **MIDPlay**（phd051199, ★80, J2ME 在线音乐播放器）已收至 research/midplay/
- 关键实证：`Manager.createPlayer(InputStream, contentType)` 流式播放范式，
  生产级细节全有：content-type 候选协商、创建重试+瞬态失败标记、
  分片创建防卡死、自定义 BufferedInputStream、URL/InputStream 双模式
- **J2ME PSG 播放器 = STC 计数器范式 PSG 核 + MIDPlay MediaResolver 骨架**：
  contentType 换 audio/x-wav，喂料换 PSG render()；content-type 协商降级
  探测照抄它的候选表思路
- MeBoy-Asha 源码实证 = MIDI 作弊（MIDI_DEVICE_LOCATOR+shortMidiEvent），
  反面教材，与我们无关；GitHub 无 J2ME 真软件合成先例 → 我们写即参考实现

### 廿二再补充：FPlay J2ME（carlosrafaelgn）——J2ME 播放器工程完整体
- 已收至 research/fplay/（86 个 java 文件，目标机型 Nokia 3120c 弱 S40，MIT）
- 播放=URL locator 三件套：file:///（本地）、**capture://radio?f=...M（把手机
  FM 调谐器硬件包成 Player 源，白嫖收音机功能）**、getEncodedURL（转义）
- EQ=JSR-234 amms EqualizerControl 全局控制+能力探测降级；
  可视化=getMediaTime 轮询；触屏+键盘双输入完整 UI
- J2ME 输出路线图齐了：FPlay 的 URL 壳+JSR-234 EQ + MIDPlay 的
  InputStream 流式骨架 + 我们的 PSG 核（伪 WAV 流/文件轮换降级矩阵）

### 廿二再补充2：加载器世界判决（FreeJ2ME 源码实锤）
- 证据：flashgame/j2me_ref/freej2me-web-main LibmediaPlayer.doPrefetch() =
  `MediaBridge.playerLoad(handle, readBytesAndClose(stream), type)`
  —— **InputStream 被一次性全读，无流式**（j2me-web 同源码）
- 因此伪 WAV 无限流方案在 FreeJ2ME/J2ME Loader 上死路；真机三档矩阵照旧
- **加载器模式**：PSG 离线渲染完整 WAV（3min@22050 单声道≈8MB，安卓堆无压力，
  渲染 1s 音频≈0.1-0.3s CPU）→ createPlayer(ByteArrayInputStream,"audio/x-wav")
  —— 1 秒内渲完全曲，体验=加载曲子
- 附：FreeJ2ME 模拟了 Nokia Sound/Samsung AudioClip/Vodafone SoundPlayer 等
  厂商私有 API，MIDI 作弊路径也兼容；真 PCM x-wav 通道反而是最干净的路线
- 双世界策略：同一 PSG 核，真机走实时流，加载器走离线渲染，配置项切换
