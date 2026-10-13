# GLDEMO_IMGUI — Dear ImGui 1.92.8 移植 N900（裸 X11+EGL+GLES1.1）笔记

2026-10-13 当天打通。ImGui 在 SGX530 上以 GLES1.1 固定管线全屏渲染 + 触摸交互 + 停靠布局，~62 FPS（vsync 封顶）。

## 架构
- `gldemo_imgui.c`（C，gcc 编译）：_start 直呼 main、dlopen 三件套、glmemperf 全屏 recipe（见 GLDEMO2_DEBUG.md）、X11 触摸事件泵、背景立方体
- `imgui_gles1.cpp`（C++，g++ 编译）：ImGui GLES1.1 自写后端（client arrays + 字体图集纹理 + scissor）+ demo UI + 停靠
- `imgui/`：核心 4 cpp + 头文件，来自 YM2163-Midi 播放器项目，**1.92.8 WIP docking 分支**（DockSpace 可用，白捡）
- 链接：`g++ -nostartfiles -nodefaultlibs` + 显式 `/opt/sysroot/usr/lib/libstdc++.so.6` + libdl shim + `-lm -lgcc -lgcc_s -lc`；**全部必须在同一条 wsl 命令内完成**（/tmp 易失）

## 编译 flags（缺一不可）
- `-fno-threadsafe-statics`（否则要 __cxa_guard_*，老 libstdc++ 有但何必）
- `-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0`（否则 __fread_chk，glibc 2.5 没有）
- `-DNDEBUG`（关 IM_ASSERT；正式版必关，触摸垃圾输入会直接 abort）
- **不要用宿主 libstdc++**：g++ 默认抓 gcc11 的 libstdc++.so（需要 glibc 2.35）→ 必须 -nodefaultlibs 显式链 sysroot 的 6.0.9（imgui 核心 STL-free，老版够用）

## 移植坑实录（按出现顺序）
1. `__isoc99_sscanf/__isoc99_vsscanf`：gcc11 头文件把 sscanf/vsscanf 重命名（__REDIRECT asm 层，#undef 无效）→ 源里写 shim `__isoc99_sscanf`，内部用 `dlsym(dlopen("libc.so.6"),"vsscanf")` 运行时解析
2. `__fread_chk`：fortify → 关 _FORTIFY_SOURCE
3. `__dso_handle` 未定义（-nostartfiles 丢 crtbegin）→ 手工 `void *__dso_handle = (void*)&__dso_handle;`
4. `__aeabi_unwind_cpp_pr0` → 链 libgcc_s
5. **GLES1 后端必须先 glEnableClientState 再 DrawElements**——数组关闭状态下调 DrawElements 是未定义行为，SGX 直接楔死渲染管线：CPU 62fps 空转 swap，画面冻在第一帧（"幻灯片"现象）。别问怎么知道的。
6. **epoch-float 精度**：动画时间别传 `now()`（1.79e9 秒），必须传相对时间。float 在 1.8e9 量级的精度间隔约 2048，每帧的旋转增量全被舍入吞掉 → 角度"冻住"、位置全挤在原点附近（"穿模叠一起"现象）。gldemo2/scene 传 `t-t0` 所以一直没事。
7. **1.92 动态字体系统**：旧式后端（不声明 RendererHasTextures）必须**每帧** `io.Fonts->GetTexDataAsRGBA32()` + 检查 TexData->Status 重新上传——任何新字形入图集都会把 TexIsBuilt 清回 false，只在 init 做一次构建会在首帧 assert 死。
8. **X11 触摸输入**：
   - XSelectInput: ButtonPress|ButtonRelease|PointerMotion|Exposure
   - 32 位 Xlib 事件布局：Motion 的 x/y 在偏移 32/36（int），ButtonEvent 的 button 在偏移 52（int）——手搓字节偏移读错会喂出天文数字按钮号
   - **按下瞬间必须先 AddMousePosEvent 再 AddMouseButtonEvent**：触摸屏手指不动就没有 Motion 事件，ImGui 拖拽手势（含解停靠）需要按下时刻的新鲜坐标
9. 停靠：本地 imgui 是 docking 分支；`io.ConfigFlags |= ImGuiConfigFlags_DockingEnable` + 每帧 `DockSpaceOverViewport(0, GetMainViewport(), PassthruCentralNode)`；窗口想可拖动就不要 `SetNextWindowPos(..., ImGuiCond_Always)`（每帧强制回位），用 FirstUseEver

## 验收记录（2026-10-13）
- 全屏 800x480、背景立方体旋转、两个 ImGui 窗口渲染、触摸拖动/点击、停靠/解停靠、拖 dock 分隔线调整大小——全部可用，~62 FPS（vsync 封顶），零 GL 错误
- 分隔线拖不动的解法：`io.MouseDragThreshold` 6px → 3px（触摸手指抖动大，默认阈值会把手势吞成点击），一处一行，立竿见影

## 音频 Saga 完整记录（2026-10-13，五版演进，最终根治）

**最终架构**：`AVTEST=1` 环境变量自测模式 + root 启动 → `sched_setscheduler(SCHED_RR,15)` → `setuid(29999)` 降权（RT 策略被音频线程继承）→ dlopen libasound → **`plughw:0,0` 直连硬件（绕过 PulseAudio）** → 独立写线程（pthread dlopen）自生成自写入，主循环零参与 → `audio_stop_full()` 先 join 线程再关 PCM（关序错误=闪退）。

**五版演进与数据**（AVTEST 压力：每 0.7s 强制 imgui 字体重烘焙 = 最重 UI 负载）：
1. 主循环直写 PCM（100ms 缓冲）：半秒即死——UI 卡一帧就断
2. 写线程 + 32768 帧环形缓冲（主循环生产）：UI 一卡断供 0.74s 缓冲
3. 写线程内嵌生成器（主循环零参与）+ 500ms ALSA 缓冲：仍断——`max_stall=0.349s × 26 次`
4. root 启动 SCHED_RR + 降权 + 1s 缓冲：仍断——`max_stall=0.477s × 69 次`
5. **plughw:0,0 绕过 pulse：根治——`max_stall=0.064s`（正常阻塞），饥饿 0 次**

**根因**：N900 的 PulseAudio 是 --system 模式守护进程，自己普通优先级。RT 写线程把数据交给 pulse 后，pulse 被主线程 GPU/字体负载挤兑，缓冲断供。RT 救不了别人家的进程——绕开它直连内核才是唯一解。

**其它已修**：
- 咔咔爆音 = snd_pcm_writei 部分写入未续写（丢帧即爆音）→ while 循环补写余量
- STOP 闪退 = glue 直接 audio_toggle(0) 关 PCM 而写线程还活着（use-after-free）→ STOP 走 audio_stop_full()（先 join 再 close），audio_start 先全停再开防竞态
- 音量小（正弦 2500/16000 幅度 + hw 音量）：待加软件音量条（下一版）

**参考实现结论**（vgmplay-maemo 亲手验证不卡的原因）：libao→pulse 阻塞写 + console UI 零重负载——它能用 pulse 是因为它没有 GPU 字体烘焙；我们这种重 GPU 应用在 N900 上必须绕开 pulse。pps2emu/deadbeef 等 SDL/default 派同理只在轻 UI 下成立。

**运行姿势**（三种都支持）：
- root 直接跑：`DISPLAY=:0 DBUS_SESSION_BUS_ADDRESS=unix:abstract=/tmp/dbus-XXX ./gldemo_imgui`（推荐，自动 RT+降权；地址从 `cat /home/user/.dbus/session-bus/*` 拿）
- su user 跑：无 RT，音频仍可用（plughw 不需要 pulse 权限）
- AVTEST=1：3 秒后自动 20 秒低音量测试音 + 周期性 UI 压力

## 已知待修
## 音频终版架构（第六版：启动持有制，vgmplay 生命周期模式）
- **启动时 snd_pcm_open("plughw:0,0") + 完整 hw/sw_params，全程持有到退出**。pulse 是按需占用（module-suspend-idle）：闲置时释放硬件、系统音效一响就抢回。之前每次 PLAY/STOP 开关设备 = 和 pulse 赛跑 → "时有时无"（EBUSY 静默失败）。持有制一劳永逸（vgmplay 的 libao 也是 open 一次用到底）
- PLAY = prepare + 起写线程；STOP = 停线程（PCM 不关）
- 音量：软件增益 VOL 0-1000%（100%=基准），±32767 硬限幅
- 波形：SINE/TRIANGLE/SQUARE/SAW 四选一（相位归一 0..1 查表式生成）
- **XSetInputFocus 必须延迟到窗口完全 mapped+fullscreen 之后**（主循环第 10 帧）；map 后立即调用会 BadMatch，Xlib 默认错误处理器直接杀进程（表现为"无声+程序消失"）
- 挂起焦点抓取后键盘事件恢复（XSelectInput KeyPress/KeyRelease + XLookupString）

### 已知尾巴（下一版修）
- PLAY 开始瞬间约 0.5s 杂音：DMA 重启时重放上次残留缓冲 → 需要 STOP 时 snd_pcm_drop 清缓冲
- STOP 后偶尔还有余音：停线程后缓冲内最多 0.74s 未播数据继续放完 → 同上，drop 解决

## 发布记录 v8（2026-10-13，tag: audio-release-v8）
- 二进制 md5: 4103c1fd79c04afd3931409d1ec9f77a（/home/user/gldemo_imgui）
- 发布包: research/releases/gldemo_imgui_release_v8.tar.gz（源码+二进制+部署脚本+文档+imgui 核心）
- 源码备份: research/releases/gldemo_imgui_src_backup_20261013.tar.gz
- 本版特性：全屏直通 81FPS 触摸 ImGui + 停靠 + 多语言 + 硬件面板 + 四波形音频发生器（drop-first 停止/静音垫启动/软件音量 0-1000%）
- **此版本为音频验收基线，后续改动必须与本版 A/B 对比，不准静默回退**

## 发布记录 v9（2026-10-13，Play 点击杂音根因修复）
- 二进制 md5: 186ff3eb53d93972ab04631b9d9b3491
- **根因（AUTOTEST 日志实证）**：STOP 的 snd_pcm_drop 之后，plug 插件层与硬件指针失同步——下一次 PLAY 的首次 writei 立即 XRUN（state=4, avail=-32），随后 8~16 个周期 -EPIPE 风暴，期间 DAC 重放 DMA 环内陈旧数据 = 用户听到的 0.5~1s 垃圾声音。prepare 单独救不回来（v8 实测交替复现）
- **修复**：drop 后重做完整 hw/sw_params 序列（pcm_resetup()，非仅 prepare）+ 写线程启动垫 2 周期静音（填住 DMA 重启窗口）→ AUTOTEST 6/6 循环零 XRUN 零 -EPIPE
- **附带发现**：v8 提交信息声称的"2 周期静音垫"实际从未进入代码（提交与实现脱节）——本版补上；hwp[512] 固定栈缓冲真实尺寸不够（snd_pcm_hw_params_sizeof()=604）→ 偶发 stack smashing，改 alloca(sizeof)
- 新调试设施：/home/user/audio_dbg.log 无缓冲音频追踪（每 writei 的返回/avail/state/耗时）；`touch /home/user/gautotest` + 启动 = 快速 Play/Stop 压力自测（env 变量经 nohup 传递不稳定，文件触发可靠）
- **此版本为音频验收新基线，后续改动必须与本版 A/B 对比，不准静默回退**

### v9 用户验收（2026-10-13）
- 真机快速反复 PLAY/STOP：Play 点击瞬间垃圾声音消失，四波形切换无杂音，音量 0-1000% 正常——**用户确认修复**
- v9 为音频最终基线（md5 186ff3eb），后续音频改动必须与它 A/B 对比
- 修复证据日志：audio_dbg4.log（AUTOTEST 6/6 循环，零 XRUN 零 -EPIPE，每次 STOP drop+resetup 后干净回 PREPARED）

## 发布记录 v10（2026-10-13，UI 零阻塞：音频控制全部异步化）
- 二进制 md5: 578e66d8c83926b85a7252ad9ab6d3d6
- **问题（用户实测）**：播放中快速连点 PLAY，系统触摸震动反馈被卡住——UI 线程每次 PLAY 重按同步执行 audio_stop_full()（pthread_join 实测 7~76ms + drop + pcm_resetup 实测 10~70ms ≈ 50~150ms 阻塞），600MHz 单核被打满，hildon 触摸震动被挤兑
- **修复（SDL/deadbeef 控制路径模式）**：常驻音频控制线程 audio_ctl，UI 的 PLAY/STOP 只登记意图（audio_ui_start/audio_ui_stop，微秒级）；join/drop/resetup/写线程生成全部在控制线程内执行。连点 N 次只是覆盖一个标志变量
- 顺带：vibra_ping/vibra_stop 的 system()（fork+exec dbus-send）挪到一次性线程——fork 在 UI 线程同样是阻塞点；附带修正 vibra 时长 ms/1000 截断（100ms 休眠变 sleep 0）
- AUTOTEST 回归：6/6 循环零 XRUN 零 -EPIPE（audio_dbg5.log），v9 音频行为无回退
- **此版本为 UI 响应性基线：UI 线程音频/震动路径耗时必须为 0（只允许置标志）**

### v10 里程碑验收（2026-10-13，用户暴力测试通过）
- 用户实测：播放中连点 PLAY/STOP 一百次，UI 完全不卡，触摸震动全程跟手——**里程碑通过**
- 发布包: research/releases/gldemo_imgui_release_v10.tar.gz（源码+二进制+构建/部署脚本+文档+imgui 核心）
- v10 = 音频(v9 基线) + UI 零阻塞 双基线版本

## 特殊键支持（2026-10-13，模拟器源码考古 + 实现 v11）

**考古结论**（证据在 dosbox_0.74-0maemo6 补丁 / hatari keymap.c / PPSSPP NKCodeFromSDL.h）：
- 所有 Maemo 模拟器都走 SDL 1.2 事件层（底层 X11），无人直读 /dev/input——我们直接吃 XEvent 是同层等价
- **Fn 键（蓝色斜箭头）不是 XKB modifier，是独立硬件键**：evdev keycode 100 / X keycode 108（dosbox 补丁 `mapper-extra-scancodes`: `sdlkey_map[0x64]=SDLK_BACKQUOTE` + `GetKeyCode` 的 `key-=8` 约定）。Fn+键的组合在 XKB 层已合成进字符——所以只要 XLookupString 正常出字，Fn 层自动可用
- **SYM 键模拟器全部忽略**（本地全树无任何处理代码），它归 Hildon 输入法管
- **PPSSPP 的 Maemo 特判**：N900 无 ESC → Backspace 兼任 Escape（NKCodeFromSDL.h:55-60 MaEMO 分支）
- hatari 用 `SDLK_BACKSPACE-0x0e / RETURN-0x1c / LSHIFT-0x2a / LCTRL-0x1d` 的 PC scancode 偏移启发式（keymap.c:373-405）——同一"Linux/X keycode 减 8 = evdev"约定

**实现（v11）**：
- 事件泵加 `XLookupKeysym`（dlsym），keysym 高字节 0xff00 的键走新增 `igui_special_key()`：Backspace/Enter/Esc/Tab/四方向/PgUp/PgDn/Home/End/Delete + Shift(L/R)/Ctrl(L/R)/Alt/Mode_switch(0xff7e，Fn 兜底映射 Alt) → ImGui `AddKeyEvent` 语义键
- 字符路径加可打印过滤（0x20..0xef），控制字节不再误入 UTF-8 文本流
- KEYS 面板显示 `ks=十六进制 keysym`——以后任何键失灵，屏幕上直接读 keysym，不用再猜

- 二进制 md5: a45c9ab234124ae72409b7b261709bde；AUTOTEST 回归 6/6 干净

## IMRUN 桌面集成（2026-10-13，运行时上桌面）
- **定名 IMRUN**（ImGui RUNtime / "I'm run"）——应用名从此固定，图标不再改
- 桌面入口：/usr/share/applications/hildon/imrun.desktop（Exec=/home/user/imrun，X-Osso-Type 同 SkyRaid/wgames 模式）
- 图标（自绘，深色窗口+青色立方体，已定稿）：/usr/share/icons/hicolor/{48x48,64x64,124x124}/hildon/imrun.png，源文件 packaging/desktop/
- 二进制：/home/user/imrun（=gldemo_imgui v11，md5 a45c9ab2）
- 安装姿势：SFTP 三件套（desktop+icons+bin）→ `killall hildon-desktop` 重启桌面壳刷新图标（X 客户端不受影响）
- 仓库资产：packaging/desktop/（imrun.desktop + 三尺寸 PNG + 二进制副本）

## IMRUN Shell v12（2026-10-13，运行时成型：桌面+系统栏+APP 窗口）
- 二进制 md5: 2e3b6c94f559cddbbb8e50d760e2b465（部署为 /home/user/imrun）
- **底部系统栏 48px**（Honeycomb 形态）：BACK（=关当前窗口）、HOME（回桌面）、运行中 APP 图标+红点 X 关闭、右侧时钟 HH:MM + 电量%（bq27200）+ CPU 温度（每 90 帧刷新 sysfs）
- **桌面网格**：7 个内置 APP（IMGUI/WIDGETS/TONEGEN/VIBRA/KEYS/FONTS/SYSINFO），4 列 72px 大按钮，已打开的带 * 标记
- **每个 APP = 独立窗口，标题栏自带 X + 窗口内 CLOSE 大按钮（44px）**
- **Ctrl+W 关闭当前焦点窗口**（N900 有实体 CTRL 键，PC 习惯直接搬来）；BACK 键同义
- 电阻屏纪律落地：全部点击式，目标 >=44px，零手势
- 实现注：sysfs 读数用 fgets+atoi（fscanf 被 gcc11 重定向成 __isoc99_fscanf，无 shim）
- AUTOTEST 回归 6/6 干净，音频基线无回退
- **v12a 修正**：右下角电量/温度真数据源（设备实测）——CPU 温度 `/sys/class/hwmon/hwmon0/device/temp1_input`（omap34xx_temp，裸摄氏度非毫度）；电量 Fremantle 无 sysfs（bq27200 归 BME 守护进程独占）→ 后台线程 30s 调 `hal-get-property --udi .../bme --key battery.charge_level.percentage`（fork 永不进 UI 线程）。md5 e98064688a51089ecd556f779abe4989

## IMRUN Shell v14（2026-10-13，桌面美化 + 最小化）
- 二进制 md5: 62c90c3c55843e17c33917922e857596（/home/user/imrun）
- **桌面半透明（alpha 0.42）**：背景 3D 方块动画直接当壁纸，磨砂卡片网格悬浮其上
- **每个 APP 手绘矢量图标**（ImDrawList 程序化绘制，零纹理）：IMGUI=立方体、WIDGETS=滑块组、TONEGEN=正弦波、VIBRA=手机+震动弧、KEYS=键盘点阵、FONTS=环形、SYSINFO=芯片引脚；每 APP 独立强调色，图标+文字标签一体的磨砂卡片（按下/悬停有反馈，运行中蓝边+"*"，最小化"-"）
- **窗口最小化**：每个窗口 MINIMIZE (Ctrl+M) + CLOSE (Ctrl+W) 双按钮；标题栏 X 仍可关；**HOME 圆圈 = 最小化全部**（安卓语义）；任务栏图标点击 = 最小化时恢复/前台时再按最小化
- **系统栏美化**：BACK/HOME 改为纯图标绘制（左三角/圆环，桌面态圆环高亮蓝），右下角 温度计+温度 / 电池描边+电量填充条(绿/橙/红)+百分比 / 表盘+时间，全部 drawlist 矢量 + 垂直居中
- **全局配色打磨**：8px 圆角全套、深蓝灰配色、标题栏/按钮/滑块统一强调色

## IMRUN Shell v15/v16（2026-10-13，过渡动画 + 组合键修复 + 系统栏修复）
- v16 二进制 md5: af6b6eadcce5e7977e603a8a30bac732（/home/user/imrun）
- **任务栏图标点击 = 切换显示/最小化**，打开/恢复有 220ms smoothstep 过渡（从任务栏位置飞出 + 透明度淡入，纯 2D，GPU 合成无压力）
- **组合键根因修复**：Ctrl+W/M 经 XLookupString 变成控制字节 0x17/0x0d，被可打印过滤器丢弃 → 组合键全死。改为 X11 泵里按键码/基 keysym 解析（设备实测 N900 键码：37=CTRL、25=W、22=退格、36=回车），igui_ctrl_combo 直通；退格/回车也加了键码兜底
- **系统栏右下角图标消失根因**：ImDrawList 坐标是屏幕系，我写成了窗口局部系（Y 差 432 全画到窗外被裁）——加窗口偏移修正；温度计/电池表/表盘恢复显示
- BACK/HOME 按下高亮加强（active 蓝 0.85 alpha，按下即亮松开即灭）
- 焦点唯一性：全局单 g_focus 变量天然互斥；任务栏当前焦点蓝色 chip 高亮

## IMRUN Shell v17（2026-10-14，级联布局 + 标题栏最小化 + 状态持久化 + 安卓高亮）
- 二进制 md5: 12839b86ddb341214f3bfaee5809c833（/home/user/imrun）
- **窗口状态持久化**：io.IniFilename=/home/user/imrun.ini，位置/大小/折叠全保存，重启即恢复——级联布局只在每个 APP 进程内首次打开时生效，之后绝不覆盖用户调好的布局
- **级联布局**：新 APP 从左上角 (24,20) 起每次偏移 (42,34) 错开堆叠，最小 400x280 不再小窗口扎堆
- **最小化入标题栏**：X 左侧 "_" 按钮（Windows 逻辑）；窗口底部只剩红色 CLOSE 大按钮（触摸友好保留）；Ctrl+M/Ctrl+W 不变
- **电量数据源修正**：HAL 的 percentage 属性是死的（设备实测恒 76），改用 charge_level.current/design 电量条计算（实测 7/8=87%，电压 4140mV 充电中），30s 后台线程不变
- **安卓式点击反馈**：BACK/HOME 点击瞬间椭圆高亮闪光，~0.4s 缓慢淡出；删掉 HOME 常亮环（那不是状态指示，是视觉 bug）
- 温度 38°C 是 omap34xx_temp SoC 区温度，真实数据，本身就稳定
- 系统栏右下角三组指示器间距收紧等距（温度 566 / 电量 648 / 时钟贴右边 8px）

## IMRUN Shell v18（2026-10-14，布局持久化修复）
- 二进制 md5: 19946d2b05e5eb83018bd9bc26e2265f（/home/user/imrun）
- **关闭再开跑到左下角一小坨的根因**：恢复动画在动画期间 SetNextWindowPos/Size(Always) 强制拽窗口——动画帧把位置定在了默认坐标，且窗口可能在动画帧首次创建（起点就是任务栏上方 200x100 的小块），直接覆盖 ini 布局。修复：**任何路径都不再强制位置/尺寸**——恢复动画只做透明度淡入；位置只有两个来源：ini 持久化 + 进程内首次级联
- 部署时清了一次被旧 bug 污染的 imrun.ini（里面存着 200x100 的坏布局），从干净级联默认重新开始
- 系统栏右下角指示器改为运行时实测文字宽度排布：右对齐一条线，组间距固定 20px，不再拍脑袋定坐标

## IMRUN Shell v19（2026-10-14，自绘窗口标题栏）
- 二进制 md5: cb3f0b755042aa364fa126040410cbaa（/home/user/imrun）
- **每个 APP 窗口自绘标题条**：左侧应用名，右侧 `_` 最小化 + 红色 `X` 关闭（Windows 顺序），焦点窗口标题条亮蓝、非焦点暗色；**拖标题条 = 移动窗口**（位置照常进 ini 持久化）
- 之前最小化按钮消失的根因：窗口停靠后原生标题栏被 dock 节点吞掉，画在上面的小部件整体不可见——所以改为自己绘制完整标题条（ImGuiWindowFlags_NoTitleBar），不再依赖原生 chrome；缩放边框/ini 持久化仍是原生的
- 任务栏红叉 X 已删（关闭走标题条 X / 底部 CLOSE / Ctrl+W 三个途径）；任务栏图标显示完整应用名，点击=聚焦/最小化切换
- 窗口底部红色 CLOSE 大按钮保留（电阻屏触摸友好）

## IMRUN Shell v20（2026-10-14，原生标题栏 + Docking 完整恢复）
- 二进制 md5: a3822eafdf1bd0a98e8932891c960c0b（/home/user/imrun）
- **标准 ImGui 原生标题栏恢复**（带原生 X 关闭、原生折叠、原生 dock 拖拽），**Docking 全量启用不残废**
- 最小化按钮 `_` 画在原生标题栏内、X 左侧（Windows 顺序）；停靠状态下标题栏归 dock 节点时该按钮被吞是 ImGui 的结构性限制，浮动状态一定可见，停靠时最小化仍有任务栏点击切换 + Ctrl+M 两条路
- 任务栏红叉已删；任务栏 chip 显示完整应用名
- 新增视觉自测钩子：touch /home/user/gopen 启动即开 3 个 APP（回归用）
- 窗口底部红色 CLOSE 大按钮保留

## IMRUN Shell v21（2026-10-14，ImGui 核心补丁：原生标题栏最小化 + 窗口生命周期）
- 二进制 md5: c710bf59e8487e55afa86929dc47d71a（/home/user/imrun）
- **直接改了 ImGui 源码**（用户授权"不能自己创新结构吗"）：
  - imgui_widgets.cpp 新增 `MinimizeButton(id,pos)`——CloseButton 同款交互/命中模型，画下划线字形
  - imgui.cpp 标题栏渲染处、Close 按钮左侧插入 MinimizeButton；导出 C 符号 `ImGui_g_MinimizeCallback(title)`，shell 注册回调按 `###appN` 解析目标窗口
  - **原生标题栏级别，停靠/浮动都渲染**（dock 节点吞按钮的问题从核心侧解决——标题栏绘制路径就带这个按钮）
- **窗口生命周期**：TONEGEN 关窗 = 音频停（shell_close(2) 调 imrun_audio_stop_all，走异步控制线程不阻塞 UI）；最小化不停播（后台继续响，合理默认）

## IMRUN Shell v22（2026-10-14，窗口生命周期：关闭=销毁）
- 二进制 md5: 4387bda2d14d62b72feb649ab97e94d9（/home/user/imrun）
- **shell_close 现在走 shell_app_destroy**：关闭 = 实例销毁——TONEGEN 停音频+波形/音量回默认值，VIBRA 停震动；重开 = 全新实例（静态 UI 状态由 ImGui ini 布局之外的窗口内容自动重建）
- 原则入档：**关掉的 APP 不占任何活资源**（256MB 机器的生存纪律）；将来 .imapp dlopen 版在同一钩子挂 destroy()+dlclose，语义不变
- 显示型 APP（WIDGETS/KEYS/FONTS/SYSINFO/IMGUI）无资源，关闭即销毁纯 UI 状态

## IMRUN Shell v23（2026-10-14，窗口控制收敛到标题栏）
- 二进制 md5: 23443b614155451af54646b38081db1b（/home/user/imrun）
- 窗口底部大红 CLOSE 全部移除——关闭途径收敛：标题栏 X / Ctrl+W /（TONEGEN 等资源型 APP 关窗即销毁资源）
- 最小化按钮 = v21 的 ImGui 核心补丁（原生标题栏绘制路径内、X 左侧），浮动/停靠均渲染
- 发布包: research/releases/imrun_release_v23.tar.gz（含 patched imgui 源码、shell/文档/图标）

## IMRUN Shell v24（2026-10-14，热重载 + 壁纸引擎 + 全局透明度）
- 二进制 md5: 35f2c17015149b7a71461afa0a500a1e（/home/user/imrun/imrun，**目录化布局**：imrun/ + apps/，desktop 入口同步更新）
- **真·热重载**：每 ~2s 扫 /home/user/imrun/apps/*/app.so → dlopen → 桌面自动出现新图标，**零重启**。主程序 --export-dynamic 链接，插件直接用 ImGui 符号（加载时从主程序解析）；插件 ABI：`imapp_name()` 可选、`imapp_frame(double t)` 必需、`imapp_init/imapp_destroy` 可选（-shared -fPIC -nostdlib，示例 research/apps/hello/hello.cpp，6KB）。已验证：/proc/<pid>/maps 里 app.so 在列、进程存活
- **壁纸引擎**（gldemo 侧 g_wall_mode/g_wall_variant）：3D动态（原方块）/3D静态（冻结）/2D动态（漂移柔色块 alpha 混合）/2D静态，四套配色（AZURE/VIOLET/EMERALD/AMBER）
- **WALLPAP 应用**（第 8 个内建）：模式+配色下拉，**三个透明度滑条**——窗口 / 任务栏 / 桌面色调，全部实时生效
- **透明度规范落地**（默认值）：桌面全透（0，壁纸直出）、应用窗口半透（0.88）、系统栏稍不透（0.80）、桌面卡片毛玻璃（悬停/按下发亮）
- 崩坑记录：注释里写 apps/*/app.so 会提前终止块注释（*/ 符号）；插件链接 -nostdlib 绕开 binutils BFD 断言

## IMRUN Shell v25（2026-10-14，APP 管理器 + 长按卸载，生态闭环）
- 二进制 md5: 7f6a9dd85bea83f49d04a27348c5a0eb（/home/user/imrun/imrun）
- **长按桌面图标 0.5s → 编辑模式**（震动反馈）：可卸载 APP 右上角红圈白叉，**点叉直接卸载零确认**（dlclose + rm -rf，在帧首安全点执行，绝不中途拆除）；DONE 退出编辑模式
- **APPMGR 应用**（第 9 个内建）：
  - INSTALL 页：扫 /home/user/MyDocs 的 .so/.imapp（USB/文件管理器扔进去即可），**过滤框**实时筛选，INSTALL 一键装（.imapp = tar 解包，.so = 拷入 apps/<id>/），2s 内热加载上桌面
  - MANAGE 页：已装动态 APP 列表 + 运行状态（running/background/stopped）+ KILL + UNINSTALL + RESCAN NOW
- 卸载管线统一走 g_dyn_dead 延迟处理（安全点 dlclose），APPMGR 和桌面长按共用
- 插件生态三件套齐了：hello 示例（SDK）、热加载（分发）、APPMGR（安装/卸载/管理）

## IMRUN Shell v26（2026-10-14，全面模块化 + 闪退修复 + 整条标题栏解停靠）
- 二进制 md5: 7ff2ce8ade89fe80e0b23b94f7199318（/home/user/imrun/imrun）
- **闪退根因修复**：shell_begin_app 折叠窗口路径（Begin 返回 false）push 了 WindowBg 透明色没 pop——每帧泄漏一层样式栈，32 层后 ImGui 断言杀进程。FONTS/KEYS 从 ini 恢复成折叠态所以一开就死
- **全面模块化**：系统程序仅 3 个（SYSINFO / WALLPAP / APPMGR，不可卸载——图标层天然保护：卸载徽章只画给动态 APP），其余 6 个（SETTINGS/WIDGETS/TONEGEN/VIBRA/KEYS/FONTS）全部拆为独立 .so 模块（research/apps/<id>/，各 ~6KB），已部署 apps/ 目录；主程序新增导出 imrun_now()、imrun_font_cn/world/kr()；插件生命周期自理（tonegen 的 imapp_destroy 停音频、vibra 的停震动）
- **整条标题栏解除停靠（第二次核心补丁）**：DockNodeUpdateTabBar 隐藏 tab 栏分支强制清除 HiddenTabBar——tab 条即完整标题栏，任意位置拖动即可解停靠（原版只留左上角小三角）
- 实测：7 个插件（含 hello）全部在线（/proc maps 28 条），进程稳定

## IMRUN Shell v27（2026-10-14，触摸滚动 + 插件图标标准）
- 二进制 md5: af7d06905409bca63335025160089939（/home/user/imrun/imrun）
- **核心补丁 #3：空白处触摸拖拽=滚动**（安卓式）——按下后无控件激活时，拖动直接滚动悬停窗口（ActiveId==MoveId 的空体点击也放行）；滚动条加粗到 20px + GrabMinSize 18px，指头能用
- **插件图标标准落地**：`imapp_icon(dl,cx,cy,s)` 向量绘制导出，7 个插件各画各的（齿轮/滑杆/正弦/震动/键盘/圆环/笑脸），不导出回退默认立方体；IMAPP_SPEC 升 v0.2（导出函数表 + 宿主符号表 + 生命周期）
- 实测 7 插件全在线（maps 28 条），进程稳定

## IMRUN Shell v28（2026-10-14，四个交互 bug 修复）
- 二进制 md5: 18a3460bbf913519be5aacc2d53a246f（/home/user/imrun/imrun）
- **SYSINFO"打不开"根因**：ini 里残留闪退时期的 Collapsed=1，点开只见折叠条——现在显式打开动作一律 SetNextWindowCollapsed(false) 自动展开（所有内建+插件同规则，一处修复全局生效）
- **HOME 键最小化全部**：原版漏了动态插件（只循环内建）——补上 g_dyn 循环，现在真的"全部"
- **BACK 键语义重定义**（安卓式）：先收起打开的弹出菜单（下拉框/右键菜单，一次一层，核心导出 ImGui_OpenPopupCount/CloseTopPopup），全部收完后才关闭当前聚焦应用
- **任务栏长按 0.5s = 关闭应用**（震动反馈；短按仍为聚焦/最小化切换；长按后释放不误触发切换）

## IMRUN v0.3（2026-10-14，六项 UI 体系升级）
- 二进制 md5: a56e919d0e94ec3da6f27ebc5486adec（/home/user/imrun/imrun）
- **Dock 停靠/最大化不再覆盖系统栏**：dockspace 宿主窗口固定在 432px 高（系统栏上方）；SYSTEM 设置里"DOCK MAY COVER SYSTEM BAR"可解锁全屏
- **SYSINFO+WALLPAP 合并为 SYSTEM**（LOOKS/INFO/ABOUT 三页），系统应用精简为 2 个：SYSTEM + APPMGR
- **桌面纵向卷轴**：图标超出 4x4 一屏后向下延伸，右侧出现触摸可拖的滚动条（配合 v0.27 触摸滚动补丁）
- **桌面键盘过滤**：桌面态直接打字母 → 顶部弹出过滤框，匹配图标亮边、不匹配变暗，ENTER 打开第一个匹配，BKSP 删字符，ESC 清空
- **开/关窗口动画**：打开从无到有淡入 0.5s smoothstep，关闭淡出 0.5s 后才真正销毁（destroy 钩子在动画结束时执行，资源类插件晚 0.5s 释放）
- **版本同步**：IMRUN_VER 宏统一版本号，左下角水印 v0.3；ABOUT 页内嵌完整 CHANGELOG（v0.9~v0.3）
- 任务栏长按关闭的震动反馈（v0.28 已有）保留

## IMRUN v0.3.1（2026-10-14，键盘导航全适配 + 灰雾修复）
- 二进制 md5: 8c63368a4af5abe44a391c0c17fa6a41（/home/user/imrun/imrun）
- **灰蒙蒙根因**：自建 dockhost 窗口漏了 NoBackground——整块不透明 ImGui 窗口背景盖住桌面，任务栏在区域外才清晰。已修
- **键盘导航**（ImGui 原生 NavEnableKeyboard）：上下左右在控件间移动聚焦（按钮/滑杆/下拉/桌面图标），回车=激活/折叠/拖动调节；桌面图标键盘聚焦时画亮蓝粗边
- **Ctrl+T**：在打开的窗口间循环切换聚焦
- **Ctrl+M 重定义**：窗口移动模式（原最小化改由标题栏按钮/任务栏承担）——开启后光标键 24px 步进移动窗口，第一次移动自动解除停靠（核心导出 ImGui_GetWindowPosByName 支持按名取位置）

## IMRUN v0.3.2（2026-10-14，五项 bug 修复）
- 二进制 md5: bd7ed8a3e2b2bcf7ee094bdf25d09f15（/home/user/imrun/imrun）
- **SYSTEM 闪退根因**：gopen 自测钩子残留旧索引 shell_focus(2)/(3)——系统应用只剩 2 个，越界写穿 g_apps 数组=内存破坏即崩；shell_focus 加边界保护，gopen 改开 app0（设备实测 SYSTEM 打开存活）
- **淡入淡出真正生效**：级联打开（首次）原先完全跳过淡入分支——现在级联只管定位，任何打开（含首次）都走 0.5s 从全透到不透明的淡入
- **Ctrl+W/T、BACK 失灵根因**：g_focus 焦点追踪用 ChildWindows 标志，停靠后焦点上报在 Root 窗口上→永远追踪失败→三个功能全指向 -1。改 RootAndChildWindows
- **任务栏 chip 三态**：聚焦=蓝色 / 按住=绿色 / 常态=永远带 1px 边框（不再出现光秃秃的文字）
- **桌面键盘导航改为手写网格逻辑**（左右±1/上下±4/回车打开，选中格亮蓝粗边）——ImGui 原生 nav 在自由布局的桌面上方向推算混乱（框卡死在 keys 上）
- igui_ctrl_combo 加了 CTRL+x 日志（o.log 可查组合键是否到达 shell 层）

## IMRUN v0.3.2a（2026-10-14，桌面闪退排查 + 黑匣子）
- 二进制 md5: 65a371b5414b8e9cfe68e4d01561077e（/home/user/imrun/imrun）
- **桌面启动闪退排查**：以 user 干净环境实测 218 秒稳定（15FPS）——程序本身没问题；真凶锁定为遗留的自测触发文件（gautotest/gopen 曾让启动直接进 AUTOTEST 自动退出模式 = 表现为闪退），已全部清除并修正钩子
- **黑匣子上机**：SIGSEGV/BUS/ABRT/FPE/ILL 全部捕获写 /home/user/audio_dbg.log（FATAL signal N）+ LIFECYCLE boot/exit 标记——以后任何闪退都有案底，不再盲猜

## IMRUN v0.4（2026-10-14，统一插件架构：系统应用也是插件）
- 二进制 md5: 20ffbdb655ae7e5823abf21ec3cc4594（/home/user/imrun/imrun）
- **按用户指示彻底重构**：SYSINFO+WALLPAP→SYSTEM 插件、APPMGR 插件（research/apps/{system,appmgr}），`imapp_system()=1` = 受保护（编辑模式无红叉、APPMGR 拒绝卸载），其余生命周期与普通插件完全一致——**所有 APP 只有一条窗口代码路径**
- shell 主程序瘦身为纯运行时：dockhost/桌面/系统栏/热扫描/窗口管理，不再包含任何应用内容
- 宿主新增导出：imrun_dyn_count/name/state/kill/uninstall、imrun_host_rescan、imrun_vibra_ping/stop（插件可调用）
- 实测：9 插件（7 常规+2 系统）全加载（maps 32 条），SYSTEM/APPMGR 打开存活
- 闪退案底结论（黑匣子实锤）：之前 v0.3.2 的"修复"没找到真凶——统一架构从根上消灭了内建应用的特殊路径

## IMRUN v0.4.1（2026-10-14，黑屏 bug：渲染后端被误删）
- 二进制 md5: 4ac847572ecac556db9351faa9d90a39（/home/user/imrun/imrun）
- **"只剩立方体飘"根因**：v0.4 重写 shell 时把 GL 提交后端（igui_render 的 DrawElements/字体纹理上传/裁剪循环）一并删掉，只剩 ImGui::Render() 构建列表——绘制数据从未送进 GPU。从 git 历史（6532303）找回完整后端恢复；fonts.so 的字体访问器（imrun_font_cn 等）同样被误删，一并恢复（它此前 dlopen 失败被静默跳过）
- 教训入档：重写大文件时边界必须按函数对齐，不能按注释标记切——本次 a/b 切点之间夹着与 shell 无关的后端代码

### v0.4.1 发布记录（2026-10-14）
- 发布包: research/releases/imrun_release_v0.4.1.tar.gz（主程序源码+二进制、patched imgui 全源、9 插件源码、图标资产、全套文档）
- v0.4.1 = 统一插件架构 + 渲染后端恢复；音频/键盘/生命周期基线全部继承

## IMRUN v0.4.2（2026-10-14，四项修复：动画覆盖内容 / 壁纸入口 / 可逆卸载 / 焦点跳变）
- 二进制 md5: 8faf6c5b4a0bf00b073675bbce8291e0（/home/user/imrun/imrun）
- **关闭动画覆盖内容**：停靠窗口内容画进宿主绘制列表、子窗 alpha 管不到（=只有窗体渐变）——关闭时先强制解除停靠变浮动再淡出，内容一并渐变；停靠态打开跳过淡入避免半截效果（核心导出 ImGui_IsWindowDocked/SetWindowFocusByName）
- **SYSTEM→SETTINGS，第一页更名 WALLPAPER**——壁纸设置入口显性化
- **本地应用商店**：/home/user/imrun/repo/ 预置 7 个插件；APPMGR 新 STORE 页列出可装应用（含 [installed] 标记）；**卸载=移回仓库**（mv 而非 rm），STORE 一键重装——彻底可逆
- **关窗后桌面图标高亮跳变根因**：键盘导航选中框（g_desk_nav_sel）残留，关窗焦点回桌面时旧框突然显现——任何窗口开关清空导航态，且高亮仅在按过方向键后显示（g_desk_nav_on）

## IMRUN v0.4.3（2026-10-14，交互架构修正：移动只认标题栏）
- 二进制 md5: 32f743a329e6d7061c3e3db0c55785b6（/home/user/imrun/imrun）
- **ConfigWindowsMoveFromTitleBarOnly=true**：窗口移动只认标题栏拖拽（+Ctrl+M 方向键模式）——窗口内空白区域的拖动动作彻底让给触摸滚动（翻页），两者不再互相打架；这是 ImGui 原生开关，配合核心补丁 #3（空体拖拽=滚动）形成完整触摸交互模型
