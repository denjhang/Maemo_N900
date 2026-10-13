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
