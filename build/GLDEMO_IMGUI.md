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
