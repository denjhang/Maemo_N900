# IMRUN 插件开发指南

> 本文档基于 IMRUN 源码实证编写（`build/imgui_gles1.cpp` dyn_scan + `build/gldemo_imgui.c` 宿主符号 +
> `research/apps/` 下全部已验证插件），不是推测。每一条都标注了源码出处。
>
> **最后更新**: 2026-10-17（v0.4.48）—— 新增窗口尺寸（`imapp_pref_size`）、
> 输入约定（触摸/键盘）、PNG 纹理（含三个必踩的坑）、自动化验证方法。

## 1. 什么是一个 IMRUN 插件

一个 ARM ELF 共享库（`.so`），放到设备的 `/home/user/imrun/apps/<id>/app.so`，主程序启动时
`dlopen(RTLD_NOW)` 加载，桌面出现一个图标，点击打开一个 ImGui 窗口。不需要注册表、不需要
manifest——放进去就生效（启动时扫描，最多 8 个）。

```
/home/user/imrun/apps/<id>/     目录名 = 插件 id（snake_case，如 vgmplayer）
  app.so                        必须叫这个名字
```

源码出处：`imgui_gles1.cpp:241` `#define APPS_DIR "/home/user/imrun/apps"`，
`:564` `snprintf(so, ..., APPS_DIR "/%s/app.so", e->d_name)`。

## 2. 插件必须导出的函数

| 函数 | 必需性 | 签名 | 作用 |
|---|---|---|---|
| `imapp_frame` | **必需** | `void imapp_frame(double t)` | 每帧调用，画 ImGui 内容。`t` = 相对秒。不导出 = 不加载 |
| `imapp_name` | 推荐 | `const char* imapp_name(void)` | 返回显示名；缺省用目录名 |
| `imapp_icon` | 推荐 | `void imapp_icon(void* dl, float cx, float cy, float s)` | 桌面向量图标：`dl` 是 `ImDrawList*`，以 `(cx,cy)` 为中心、`s` 为半径画 |
| `imapp_init` | 可选 | `int imapp_init(void)` | 加载时调一次，返回非 0 = 拒绝加载 |
| `imapp_destroy` | 可选 | `void imapp_destroy(void)` | 关窗口/卸载时调用——**释放资源（停音频/震动）的唯一钩子** |
| `imapp_system` | 可选 | `int imapp_system(void)` | 返回 1 = 系统插件，不可卸载 |
| `imapp_pref_size` | 可选 | `void imapp_pref_size(float* w, float* h)` | 声明首选窗口尺寸（见 §4.5）。缺省 = 宿主默认 440×320 |

源码出处：`imgui_gles1.cpp` 的 `dyn_scan()`（dlopen 后逐个 dlsym 这些符号）。
`imapp_frame` 是硬门槛（缺了就 `dlclose` 跳过）。

### ⚠️ 窗口是宿主的，不要自己 Begin/End

`imapp_frame` **被调用时宿主已经 `ImGui::Begin()` 了你的窗口**，返回后宿主 `End()`。
所以：
- **不要**在 `imapp_frame` 里调 `ImGui::Begin/End`——那会开出一个"流氓"第二窗口，
  而宿主窗口是空的（v7 踩过这个坑，表现为"窗口一片空白"）。
- 直接用 `ImGui::GetWindowDrawList()` / `ImGui::GetWindowPos()` 画内容。
- 用 `ImGui::SetCursorPos(...)` 放 ImGui 控件（相对窗口内容区）。

## 3. 最小插件模板

```cpp
/* hello.cpp — IMRUN 最小插件 */
#include "imgui.h"

extern "C" const char *imapp_name(void) { return "HELLO"; }

extern "C" void imapp_frame(double t) {
    ImGui::Text("Hello, N900! t=%.1f", t);
    if (ImGui::Button("QUIT", ImVec2(120, 48)))
        ImGui::CloseCurrentTab(); /* 或者什么都不做，用户点 X 关 */
}

extern "C" void imapp_icon(void *dlv, float cx, float cy, float s) {
    ImDrawList *dl = (ImDrawList *)dlv;
    dl->AddCircleFilled(ImVec2(cx, cy), s, IM_COL32(60, 120, 200, 255), 24);
}
```

## 4. 编译——单条命令，不要手动链接

**这是最容易踩的坑。** 插件用单条 `arm-linux-gnueabi-g++` 命令编译，和
`b_plugins2.sh` 完全一致。不要拆成多文件分别编译再手动链接——手动链接用了主程序的
`-nostartfiles -nodefaultlibs` 旗标，产出的 `.so` 设备上 `dlopen` 会静默失败、图标不出现。

### 正确方式（单文件 / #include 合并）

```bash
# 在 WSL 里执行
S=/opt/sysroot
B=/mnt/e/working/zcode-projects/Maemo_N900
arm-linux-gnueabi-g++ \
  --sysroot=$S -O2 -fPIC -shared -nostdlib \
  -fno-threadsafe-statics -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -DNDEBUG \
  -I$B/build -I$B/build/imgui \
  $B/research/apps/<yourapp>/<yourapp>.cpp \
  -o /tmp/app.so
```

### 多文件：把 .c 内联进 .cpp

如果你的插件有多个源文件（比如核心引擎 `.c` + UI `.cpp`），**不要分别编译再手动链接**。
把引擎 `.c` 直接 `#include` 进主 `.cpp`：

```cpp
#include "imgui.h"
#include "engine.c"    /* 直接包含，单文件编译 */
// ... 你的 UI 代码
```

然后单条 g++ 命令编译这一个 `.cpp`。这是 VGM 播放器的实际做法（`vgm.cpp` 里
`#include "ay8910.c"`）。

### 旗标说明

| 旗标 | 作用 |
|---|---|
| `--sysroot=/opt/sysroot` | Fremantle glibc 2.5 运行库 |
| `-fPIC -shared` | 产出位置无关共享库 |
| `-nostdlib` | 不链接默认启动文件（主程序已有 `_start`） |
| `-fno-threadsafe-statics` | 避免 C++ static 初始化的 mutex 开销 |
| `-I$B/build -I$B/build/imgui` | 找到 `imgui.h` 和宿主头文件 |### 部署

```bash
# Windows 侧 Python（paramiko）
import paramiko
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('192.168.101.108', username='root', password='123456')
sftp = ssh.open_sftp()
sftp.put('build/app.so', '/home/user/imrun/apps/<id>/app.so')
sftp.close()
# 重启 imrun 让 dyn_scan 重新加载
ssh.exec_command('pkill -f imrun; sleep 2')
# 启动 GUI（见第 8 节环境速查）
```

## 4.5 窗口尺寸（可选）

默认窗口 440×320。非标准比例的应用（如竖屏游戏）导出：

```cpp
extern "C" void imapp_pref_size(float *w, float *h) {
    *w = 400.0f;   /* 竖屏：窄 */
    *h = 420.0f;   /* 桌面可用高 = 480 - 48(系统栏) = 432 */
}
```

宿主行为：
- `dyn_scan` 时 dlsym 读取，存进 `DynApp.pref_w/pref_h`
- 应用**首次打开**时用 `ImGuiCond_Always` 强制应用一次
- 能覆盖 `imrun.ini` 里存的旧尺寸（否则旧值靠 `FirstUseEver` 永久生效）
- 之后用户手动拖拽调整的尺寸仍会被 ini 记住

源码：`imgui_gles1.cpp` 的 `shell_begin_dyn()`。

## 4.6 输入（触摸 / 键盘）

触摸屏点击 = 核心 X 指针（ButtonPress/Release），运行时已喂给 ImGui。
**判断"点在我窗口上"必须用窗口 hover 限定**：

```cpp
bool tapped = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
              ImGui::IsMouseClicked(0);
bool keyed  = ImGui::IsKeyPressed(ImGuiKey_Space) ||
              ImGui::IsKeyPressed(ImGuiKey_UpArrow) ||
              ImGui::IsKeyPressed(ImGuiKey_Enter);
```

为什么不能只写 `ImGui::IsMouseClicked(0)`：那是全局状态，点标题栏、桌面、
任务栏都会误触发（早期版本踩过）。

N900 的电阻屏没有独立触摸事件，触摸 = 鼠标左键，所以以上对触摸和鼠标通用。

## 4.7 PNG 纹理（图片素材）

主程序集成了 stb_image，插件可加载外部 PNG 当纹理用。

```cpp
struct ImTextureData;
extern "C" ImTextureData* imrun_load_png(const char *path);
extern "C" void imrun_register_texture(ImTextureData *tex);

static ImTextureData *td = nullptr;
static unsigned gl_tex = 0;

/* 第一帧：加载 + 注册（此时 GL 上下文不一定 current，不能自己上传） */
if (!td) {
    td = imrun_load_png("/home/user/imrun/apps/myapp/assets/bg.png");
    if (td) imrun_register_texture(td);   /* 上传交给渲染循环 */
}
/* 下一帧起：读真实 GL id（渲染循环已上传） */
if (td && !gl_tex) gl_tex = (unsigned)td->GetTexID();
/* 绘制 */
if (gl_tex)
    ImGui::GetWindowDrawList()->AddImage((ImTextureID)gl_tex, p0, p1);
```

### 三个必须知道的坑

1. **必须 `imrun_register_texture()`**
   ImGui 的 `PlatformIO.Textures` **每帧**被 `UpdateTexturesEndFrame()` 清空重建。
   直接 `push_back` 进去的纹理会丢。必须走 `ImGui::RegisterUserTexture()`
   （`imrun_register_texture` 内部就是调它，加到持久的 `UserTextures`）。

2. **GL id 要零扩展，不能走 `intptr_t`**
   `ImTextureID` 是 64 位 `ImU64`，而 GL id 是 32 位无符号。
   `(ImTextureID)(intptr_t)id` 在 32 位 ARM 上会**符号扩展**：id ≥ 2^31 时
   变成 `0xFFFFFFFFxxxxxxxx`，绑定无效纹理（画不出来）。
   正确写法：`(ImTextureID)id` —— `unsigned` 直接转 u64 是零扩展。

3. **非二次幂（NPOT）纹理在 N900 上不可用**
   PowerVR SGX530 的 GLES 1.1 没有 `GL_OES_texture_npot`，
   `glTexImage2D(288×512)` 会被驱动静默拒绝 → 纹理采样为白色。
   主程序 `upload_texture()` 已自动把 NPOT 放大到下一个 POT，
   所以插件只管提供图，不用管尺寸。但**知道这点**很重要：
   看到"图全白、字体正常"就是 NPOT 症状（字体图集 512×128 是 POT）。

### 素材放哪

```
/home/user/imrun/apps/<id>/assets/*.png
```
插件内用绝对路径加载。参考实现：`research/apps/flappy/flappy_v7.cpp`。
技术细节与完整调试过程：[FLAPPY_TEXTURE_DEBUG_NOTES.md](./FLAPPY_TEXTURE_DEBUG_NOTES.md)。

## 5. 宿主供给的符号（主程序 --export-dynamic 导出）

主程序编译时带 `-Wl,--export-dynamic`（`b_imgui_export.sh`），全部全局符号对插件可见。
插件用 `extern "C"` 声明即可引用，不需要 dlopen 主程序。

### 音频后端现状（v0.4.45+）

| 值 | 后端 | 实际发声路径 | 默认？ |
|---|---|---|---|
| 0 | ALSA plughw | `snd_pcm_writei` 直写 `plughw:0,0` | 否 |
| 1 | libao (vgmplay) | 完整 61KB libao + alsa09 → `plughw:0,0` | 否 |
| 2 | SDL (ppsspp) | **实际复用 ALSA plughw 路径**（SDL 原生 pulse callback 已废） | **是** |
| 3 | PULSE (paplay) | **libpulse-simple → pulse sink**（v0.4.46+，受系统音量控制） | 否 |

`audio_backend` 默认 2，GUI 上「选 SDL」实测最稳定（用户验证：长播不断流）。
**PULSE 后端**（v0.4.46+）走 libpulse-simple，输出流入 pulse sink——系统栏/硬件键/amixer
改 sink vol 时 IMRUN 输出自动跟手（"遵守系统音量"的传统方法）。
插件不应该假设「SDL 就是 SDL 原生 callback」——SDL 音频路径实际是 plughw 直连。
详见 `docs/AUDIO_BACKENDS.md`。

### 全部 ImGui 符号

`ImGui::Begin/Text/Button/SliderFloat/...`、`ImDrawList::AddCircleFilled/...` 等全部 866 个
C++ mangled 符号都导出了。插件只需 `#include "imgui.h"`，链接时自动解析。

### 宿主函数

| 符号 | 签名 | 作用 | 出处 |
|---|---|---|---|
| `audio_ui_start` | `void audio_ui_start(int seconds)` | 启动音频播放，seconds=0=无限，>0=到时自动停 | `gldemo_imgui.c:1061` |
| `audio_ui_stop` | `void audio_ui_stop(void)` | 停止音频 | `:1062` |
| `audio_ui_set_backend` | `void audio_ui_set_backend(int be)` | 切换后端：0=ALSA 1=libao 2=SDL | `:1067` |
| `imrun_now` | `double imrun_now(void)` | 相对秒时钟（禁止自己取 epoch） | imgui_gles1.cpp |
| `vibra_ping` | `void vibra_ping(int ms)` | 震动 ms 毫秒 | `gldemo_imgui.c:633` |
| `vibra_stop` | `void vibra_stop(void)` | 停震动 | |
| `imrun_load_png` | `ImTextureData* imrun_load_png(const char*)` | 加载 PNG 为纹理（见 §4.7） | imgui_gles1.cpp |
| `imrun_register_texture` | `void imrun_register_texture(ImTextureData*)` | 注册纹理，上传交给渲染循环 | imgui_gles1.cpp |
| `alog_public` | `void alog_public(const char*, ...)` | 写 `/home/user/audio_dbg.log` | |

### 宿主变量

| 符号 | 类型 | 作用 |
|---|---|---|
| `audio_on` | `int` | 音频是否在播放（1=播放中） |
| `audio_deadline` | `double` | 自动停止时刻（`imrun_now()` 返回值） |
| `audio_volume` | `int` | 总音量（0..1000，100=unity） |
| `audio_wave` | `int` | 波形（0 sine 1 tri 2 square 3 saw），内置发生器用 |
| `audio_backend` | `int` | 当前后端编号 |
| `audio_flow` | `long` | 已渲染帧数计数 |
| `dbg_underruns` | `int` | ALSA underrun 计数 |
| `dbg_stalls` | `int` | 卡顿计数 |
| `audio_source_fn` | `int (*)(short*, int)` | **喂采样钩子**（见第 7 节） |

### 声明方式

```cpp
extern "C" {
void audio_ui_start(int seconds);
void audio_ui_stop(void);
extern int audio_on;
extern double audio_deadline;
extern int audio_volume, audio_wave;
double imrun_now(void);
void vibra_ping(int ms);
}
```

## 6. 使用内置音频（最简单）

如果你的插件只需要「播放一段声音」，用内置发生器：

```cpp
extern "C" void imapp_frame(double t) {
    if (audio_on) {
        int left = (int)(audio_deadline - imrun_now());
        ImGui::Text("PLAYING (%ds left)", left);
    } else {
        ImGui::Text("idle");
    }
    if (ImGui::Button("PLAY 20s", ImVec2(140, 48)))
        audio_ui_start(20);  // 内置 440Hz 正弦，20 秒后自动停
    ImGui::SameLine();
    if (ImGui::Button("STOP", ImVec2(140, 48)))
        audio_ui_stop();
}
extern "C" void imapp_destroy(void) {
    audio_ui_stop();  // 插件关闭时停音频
}
```

这是 `tonegen.cpp` 的完整模式。

## 7. 喂自定义采样（audio_source_fn 钩子）

内置发生器是 440Hz 正弦。要播自己的音频数据（如 VGM/解码器），注册一个采样回调：

```cpp
extern "C" {
extern int (*audio_source_fn)(short *dst, int frames);  // 主程序全局函数指针
}

/* 你的渲染函数：填 frames 个立体声帧 (dst[0..2*frames-1], L=R 交错)
   返回 1 = 继续播放，0 = EOF（主程序自动停） */
static int my_render(short *dst, int frames) {
    for (int i = 0; i < frames; i++) {
        short s = my_next_sample();
        dst[i * 2] = s;       // L
        dst[i * 2 + 1] = s;  // R
    }
    return my_eof ? 0 : 1;
}

static void start_playback() {
    audio_source_fn = my_render;                    // 先注册回调
    audio_ui_start(total_seconds);                  // 再启动（deadline 兜底）
}

static void stop_playback() {
    audio_source_fn = 0;                            // 先摘钩
    audio_ui_stop();
}
```

### 工作原理

主程序的 `audio_writer()` 线程在每个音频块（AO 后端 512 帧 / ALSA 后端 2048 帧）的渲染点：

```c
// gldemo_imgui.c:872 (AO 路径)
if (audio_source_fn) {
    if (!audio_source_fn(frag, 512)) { audio_on = 0; break; }  // EOF
} else gen_samples(frag, 512);  // 内置 440Hz

// gldemo_imgui.c:905 (ALSA 路径)
if (audio_source_fn) {
    if (!audio_source_fn(chunk, PERIOD)) { thread_run = 0; audio_on = 0; break; }
} else gen_samples(chunk, PERIOD);
```

`audio_source_fn` 非零时走插件回调，为零时走内置发生器。**三个后端（ALSA/libao/SDL）的
打开/关闭/切换逻辑完全不受影响**——钩子只在采样生成点分叉。

### 线程安全注意

- 渲染回调在 `audio_writer` 线程执行，`imapp_frame` 在 UI 线程执行
- 选曲/停止/加载是 UI 线程发起的，渲染是 writer 线程拉的——共享状态用 `volatile` 或
  简单标志，不要在回调里做文件 IO 或 malloc
- `imapp_destroy` 里**先 `audio_source_fn = 0` 再 `audio_ui_stop()`**，否则 writer 线程
  可能回调已卸载的函数

## 8. 环境速查

| 项 | 值 |
|---|---|
| 设备 IP | 192.168.101.108 |
| SSH | root / 123456（paramiko 兼容老算法） |
| 交叉编译 | WSL：`wsl -e bash -c "..."` |
| sysroot | /opt/sysroot |
| 编译器 | arm-linux-gnueabi-g++ 11.4（软浮点 armel） |
| 插件目录 | /home/user/imrun/apps/\<id\>/app.so |
| 最多插件数 | 8（`imgui_gles1.cpp:557` `g_dyn_n < 8`） |
| 音频采样率 | 44100Hz，立体声 s16 |
| GUI 启动 | `su - user -c 'export DISPLAY=:0; XAUTHORITY=/home/user/.Xauthority; DBUS_SESSION_BUS_ADDRESS=unix:abstract=/tmp/dbus-7Yk7lLxfhR,guid=f17a639b69a48bf06b03481400000013; nohup /home/user/imrun/imrun >/tmp/imrun_gui.out 2>&1 &'` |
| 主程序源码 | `build/gldemo_imgui.c`（C，音频+主循环）+ `build/imgui_gles1.cpp`（C++，ImGui shell+插件扫描） |
| 主程序编译 | `build/b_imgui_export.sh`（`--export-dynamic` 导出符号给插件解析） |
| 插件编译模板 | `build/b_plugins2.sh` |

## 9. 调试提示

- **dlopen 失败是静默的**：`dyn_scan`（`imgui_gles1.cpp:566`）的 `if (!h) continue` 不打
  任何日志。如果你的插件图标不出现，99% 是 dlopen 失败——检查编译方式（第 4 节）。
- **不要用 Python ctypes 测试 dlopen**：设备 Python 2.5 的 ctypes 不支持 `RTLD_NOW`，
  且不会像 C 的 dlopen 那样解析主程序导出符号，结果不可信。
- **AOTEST 标记文件**：如果 `/home/user/aotest` 存在，imrun 启动后立刻跑音频回归测试
  然后 `exit(0)` 闪退——桌面不会出现。测试完删掉它：`rm -f /home/user/aotest`。
- **/tmp 只有 1MB**：tmpfs 挂载，编译产物不要留在设备 /tmp。
- **画面空白 ≠ 代码没跑**：先分清三种情况——
  1. 窗口里 ImGui 控件（文字/按钮）能显示，但图/矩形不显示 → 你可能自己调了
     `ImGui::Begin/End`，内容画到流氓窗口去了（见 §2 警告）。
  2. 文字正常但图片全白 → NPOT 纹理问题（见 §4.7 坑 3）。
  3. 什么都不显示 → 检查 `imapp_frame` 是否真的被调用（`alog_public` 打一行）。

### 自动化验证（不用人眼看屏幕）

- **截图**：`/dev/fb0` 可直接读（800×480，16bpp RGB565，stride 4096）。
  设备没有 base64、/tmp 很小 → 必须走 SSH 通道流式读 `dd if=/dev/fb0`。
  现成脚本：`build/test_visual.py`。
- **注入点击**：用 XTEST（`libXtst.so.6` 的 `XTestFakeMotionEvent/ButtonEvent`）。
  **不要用 uinput 合成设备**——xorg.conf 静态绑定了 TSC2005，Xorg 不认新设备。
  现成脚本：`build/xtest_tap.py <x> <y> [hold_ms]`（需 `DISPLAY=:0`）。
- 完整方法论：[N900_AUTO_VISUAL_TEST.md](./N900_AUTO_VISUAL_TEST.md)。

## 10. 已验证的插件参考

| 插件 | 路径 | 功能 | 要点 |
|---|---|---|---|
| tonegen | `research/apps/tonegen/tonegen.cpp` | 440Hz 波形 | 最简音频插件，37 行 |
| demo | `research/apps/demo/demo.cpp` | ImGui 全控件演示 | 图标绘制范例 |
| system | `research/apps/system/system.cpp` | 设置（WALLPAPER/DISPLAY/AUDIO/INFO/ABOUT） | 系统插件（`imapp_system()=1` 不可卸载）；v0.4.45+ 含 AUDIO 标签（后端切换+TONEGEN） |
| appmgr | `research/apps/appmgr/appmgr.cpp` | 应用管理（安装/卸载/重排） | 调 `imrun_dyn_*` 系列函数 |
| vgmplayer | `research/apps/vgmplayer/vgm.cpp` | VGM/VGZ 播放器 | `audio_source_fn` 喂采样范例 |
| flappy | `research/apps/flappy/flappy_v7.cpp` | Flappy Bird 游戏 | **PNG 纹理范例** + 竖屏窗口（`imapp_pref_size`）+ 触摸输入 |

## 11. 完整构建脚本

```bash
#!/bin/bash
# b_plugin.sh — 通用插件编译脚本
# 用法: ./b_plugin.sh <插件目录名>
set -e
S=/opt/sysroot
B=/mnt/e/working/zcode-projects/Maemo_N900
F="--sysroot=$S -O2 -fPIC -shared -nostdlib -fno-threadsafe-statics -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -DNDEBUG -I$B/build -I$B/build/imgui"
APP=$1
if [ -z "$APP" ]; then echo "Usage: $0 <plugin_dir>"; exit 1; fi
# 找主源文件 (.cpp 优先, 否则 .c)
SRC=$(ls $B/research/apps/$APP/*.cpp 2>/dev/null | head -1)
[ -z "$SRC" ] && SRC=$(ls $B/research/apps/$APP/*.c 2>/dev/null | head -1)
[ -z "$SRC" ] && { echo "No source in research/apps/$APP/"; exit 1; }
arm-linux-gnueabi-g++ $F "$SRC" -o $B/build/$APP.so
echo "OK: $B/build/$APP.so"
md5sum $B/build/$APP.so
```
