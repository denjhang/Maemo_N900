# VGM 播放器插件开发交接文档（给下一个窗口）

> **你的任务**：在 IMRUN (N900) 里新建一个插件 APP「VGM 播放器」。完整 VGM/VGZ 文件兼容解析（所有芯片命令正确处理不脱 sync），当前只实现 AY8910 一个渲染核心。UI = 简单播放控件 + 文件列表。性能红线：AY8910 精简核心，压力极低。
> **当前状态：调查 100% 完成，代码 0% 写。所有事实已实证，直接按清单实现。**

## 一、三个权威参考源（用户指定）
1. **AY8910 精简核心**（照抄移植）：`E:\working\vscode-projects\STC_Chiptune\STC32G144K246\usb_cdc_test\src\ay8910.c` + `ay8910.h`（260 行，纯整数，曾在 72MHz 8051 跑通）
2. **VGM 解析权威**：`E:\working\vscode-projects\Reference_Project\vgm_libs\modizer-master-20260408\modizer-master\libs\libvgm\libvgm\` — `player/vgmplayer_cmdhandler.cpp`（`_CMD_INFO[0x100]` 完整命令长度表）、`player/vgmplayer.cpp`（header 解析 + ParseFile 主循环）、`emu/cores/ay8910.c`（MAME 完整版 1624 行重依赖，**不要用**，只做行为对拍）
3. **UI 参考**：`E:\working\vscode-projects\YM2163-Midi\Denjhang_Music_Player_v16\libvgm-modizer\chip_params.h`（`AY8910Params`：3 通道+16 寄存器+envelope+noise）

## 二、已实证关键事实（不用重查）

### AY8910 精简核心（STC 版）
- 接口仅 4 个：`ay_init()` / `ay_set_clock(u32)` / `ay_wr(u8 reg, u8 val)` / `ay_render()` 返回 `s16`（单声道，写 L=R）
- 依赖只有 stc.h 的 `u8/u16/u32/s16` typedef——移植时自带
- **采样率公式（关键，STC 原版硬编码 22050）**：`ay_base_incr = (u64)clock * (1<<24) / (8 * output_rate)`；clock=1789772@22050→170223307（=STC 已验证常量，吻合）；@44100→**85111653**（IMRUN 管线 44100Hz 用这个）。换成此公式后任意时钟/采样率皆准，弃用 STC 的 double 版
- `ay_regmsk[16]`/`ay_voltbl[32]` 表都在源文件里，直接抄

### VGM 格式（libvgm 实证）
- Header（小端 u32）：`"Vgm "`@0，`fileVer`@0x08，`eofOfs`(相对)@0x04，`gd3Ofs`(相对)@0x14，`numTicks`@0x18，`loopOfs`(相对)@0x1C，`loopTicks`@0x20，`dataOfs`(相对)@0x34（v1.50+，默认 0x40，最小 0x38）
- AY 时钟：@0x40（v1.50）或 @0x74（v1.70）双偏移读非零者；`chipFlags`@0x79 **bit4(0x10)=YM2149 /2 分频**
- **0xA0=AY8910 写**：`[0xA0][reg][data]` 共 3 字节；reg bit7=双芯片选择（只用 chip0，`reg&0x7F`）
- wait：`0x61`=N(2B LE)、`0x62`=735、`0x63`=882、`0x70-7F`=1+(b&0xF)；`0x66`=end；`0x67`=data block（`[0x67][0x66][type][size:4LE][data]`，**必须按 size 精确跳**）；`0x68`=PCM RAM write(12B)
- 渲染模型：`while(filePos<dataEnd && fileTick<=playTick){处理命令; filePos+=cmdLen;}` 后渲染本块

### N900 IMRUN 插件架构
- 插件在 `/home/user/imrun/apps/<id>/app.so`，`dlopen(RTLD_NOW)`，最多 8 个（imgui_gles1.cpp dyn_scan）
- ABI：`imapp_frame(double)` 必须；name/init/destroy/icon 可选
- 主程序 `--export-dynamic`（b_imgui_export.sh），插件 extern 即可解析主程序符号
- 插件编译旗标（抄 b_plugins2.sh）：`--sysroot=/opt/sysroot -O2 -fPIC -shared -nostdlib -fno-threadsafe-statics -I<build> -I<build>/imgui`，arm-linux-gnueabi-g++
- **关键缺口（本次要补的 hook）**：`gen_samples()` 是 static 硬编码 440Hz，被 `audio_writer()` 两个路径直接调（AO 每次 512 帧、ALSA 每次 PERIOD=2048 帧）。插件目前无喂采样入口
- 现有插件可用符号：`audio_ui_start(int seconds)`（自带 deadline 自动停）、`audio_ui_stop()`、`audio_ui_set_backend(int)`、`audio_on/audio_flow/dbg_underruns/dbg_stalls/audio_deadline/audio_volume/audio_wave/audio_backend/imrun_now()`
- VGM 时长=numTicks/44100，`audio_ui_start(numTicks/44100+2)` 自然收尾
- 死代码：`rb[RB_FRAMES*2]` 环形缓冲声明未用，勿被迷惑

### 测试素材
- `E:\working\vscode-projects\STC_Chiptune\vgm\ay8910\` 大量真实 AY8910 VGM，部署到设备用
- VGZ=gzip（magic `\x1f\x8b`）；设备 zlib 未验证——先 `dlopen("libz.so.1")+gzopen/gzread`，失败则跳过 vgz（miniz 后续加）

## 三、执行清单（按序）
1. **加 hook（gldemo_imgui.c 最小改动）**：加 `int (*audio_source_fn)(short *dst, int frames) = 0;`（返回 1=继续,0=EOF）；writer 两个路径 `if (audio_source_fn) keep=audio_source_fn(buf,n); else gen_samples(buf,n);`，keep==0 走既有 break 路径。**不许动三后端打开/关闭/切换逻辑（v0.4.44 刚修顺，commit e5c869a）**
2. **移植 AY8910 核心**：`research/apps/vgmplayer/ay8910.c/.h`，抄 STC 版+上文 44100 公式
3. **VGM 解析+插件 UI**：`research/apps/vgmplayer/vgm.cpp` — header 双偏移解析、完整命令长度表（libvgm _CMD_INFO 抄）、0xA0→ay_wr(reg&0x7F,data)、其余芯片命令正确跳过（=「所有芯片兼容解析」的含义）、渲染回调逐采样推进、loop 跳回 loopOfs、VGZ zlib dlopen、UI=opendir("/home/user/vgm") 文件列表+PLAY/STOP+GD3 曲名+进度+AY 三通道音量条
4. **部署**：测试 VGM → 设备 `/home/user/vgm/`；插件 → `/home/user/imrun/apps/vgmplayer/app.so`；主程序重编部署（先备份）
5. **实测**：开 VGM 播放器选曲播放，确认出声+不卡顿+能停能切；跑 AOTEST 回归

## 四、红线
- 三后端刚全通，writer 线程只许加 hook 分支不许重构；改完必跑 AOTEST（`touch /home/user/aotest`+启动）确认 `hard_fail=0 underruns=0`
- AOTEST 钩子、audio_dbg.log 追踪不许删；主程序部署前备份；测试进程自终止

## 五、环境速查
- 设备 192.168.101.108 root/123456，python+paramiko，前台命令 nohup+sleep+cat
- 交叉编译在 WSL：`wsl -e bash -c "..."`，sysroot /opt/sysroot
- 启动 GUI：`su - user -c 'export DISPLAY=:0; XAUTHORITY=/home/user/.Xauthority; DBUS_SESSION_BUS_ADDRESS=unix:abstract=/tmp/dbus-7Yk7lLxfhR,guid=f17a639b69a48bf06b03481400000013; nohup /home/user/imrun/imrun >/tmp/imrun_gui.out 2>&1 &'`
- 当前部署：主程序 md5 f817090d…（v0.4.44）、audiolib 完整版 libao 在 /home/user/imrun/audiolib/libao.so.2、备份 imrun.bak_v0.4.42