# IMAPP 规范 v0.1（IMRUN 应用格式）

2026-10-13 定稿。IMRUN = N900 上的 ImGui 运行时（裸 X11+EGL+GLES1.1）；
IMAPP = 跑在 IMRUN 里的应用。目标形态：微信小程序式热插拔——点图标即启，
关掉即卸，运行时不重启，图形/音频/输入由运行时统一供给。

## 1. 包格式：单文件 `.imapp`

```
app.imapp  =  gzip 压缩的 tar，顶层目录名 = app id：
  <id>/
    manifest.ini    必有，UTF-8
    app.so          必有，ARM ELF 共享库（-shared 编译，非可执行）
    icon.png        必有，48x48（可附 64/124）
    data/           可选，应用私有数据（字体/资源）
```
- 选 tar.gz：Maemo 自带 tar，无新依赖；`dpkg-deb` 太重，规范不碰 deb
- 安装 = 解包到 `/home/user/imrun/apps/<id>/`；卸载 = 删目录。零注册表

## 2. manifest.ini

```ini
[imapp]
format = 1              ; 本规范版本，运行时不认识就不加载
id = n900.imrun.tonegen ; 反域名式，全局唯一，= 包顶层目录名
name = ToneGen          ; 桌面/任务栏显示名
version = 1.0.0
entry = app.so:imapp_main  ; 库文件:入口符号，入口可省略默认 imapp_main
single = 1              ; 1=单实例（重复打开=唤起已有实例）
[vibra]                 ; 权限段：声明才可用，运行时弹一次性授权
allowed = 0
[audio]
allowed = 1
```

## 3. ABI：一个函数表，三个生命周期函数

APP **禁止**自己 dlopen 任何图形/音频/X11 库——运行时把服务以 C 函数表
传入（沿用 IGlfns 模式，稳定、无符号膨胀、glibc 2.5 友好）：

```c
typedef struct IMAppAPI IMAppAPI;
struct IMAppAPI {
    int format;                      /* =1，运行时填 */

    /* --- 图形：每帧开始前调用合法，全部运行时已 make current --- */
    void (*imgui_frame_begin)(void); /* NewFrame，APP 之后直接画 ImGui */
    void (*imgui_frame_end)(void);   /* Render+Swap */
    /* GLES1.1 直通函数表（IGlfns，同 gldemo_imgui.c），供特效/3D 用 */

    /* --- 音频：运行时持有 PCM（v8 生命禁止 APP 碰）--- */
    int  (*audio_open)(int rate, int ch);       /* 成功后 PCM 归 APP 写 */
    long (*audio_write)(const short *buf, long frames); /* 返回已消化帧数 */
    void (*audio_close)(void);

    /* --- 输入（ImGui IO 已由运行时喂好，一般用不上）--- */
    int (*key_state)(int imgui_key);

    /* --- 系统 --- */
    void (*vibra)(int ms);           /* 异步，v10 线程化版 */
    double (*now)(void);             /* 相对秒（epoch-float 禁令内建）*/
    void (*log)(const char *fmt, ...);
};

/* 生命周期（全部 APP 侧实现，运行时单线程调用，无需加锁）：
   create  : 一次性初始化。返回非 0 = 拒绝加载
   frame   : 每帧调用。APP 只画自己的 ImGui 窗口；返回 1 = 请求退出
   destroy : 卸载前调用，之后运行时 dlclose */
typedef struct {
    int  (*create)(IMAppAPI *);
    int  (*frame)(void);
    void (*destroy)(void);
} IMAppExports;
```

## 4. 运行时铁律（APP 侧同样受约束）

1. **PCM 启动持有制**：audio_open/close 走 v8 架构（drop+resetup+静音垫），
   APP 绝不 snd_pcm_* —— 这是 v5~v9 五轮血的教训的直接产物
2. **UI 线程零阻塞**：一切 join/ioctl/system 在运行时工作线程（v10 铁律）
3. **相对时间**：API 里只有 now() 相对秒，杜绝 epoch-float 冻结动画
4. **4 分量颜色**：GLES1.1 客户端数组一律 RGBA（SGX530 三分量即楔死）
5. **特殊键**：XLookupKeysym 语义键路径由运行时负责（v11），APP 只看 ImGui
6. APP 崩溃 = `dlclose` + 图标变灰 + 运行时存活（伪 OS 的底线）

## 5. 目录布局（设备）

```
/home/user/imrun            运行时本体
/home/user/imrun/apps/      已安装 IMAPP
/home/user/imrun/imrun.log  运行时日志
```
任务栏：IMRUN 顶部条 = 已安装 APP 图标横排（后继版本实现）。

## 6. 版本承诺

format=1 的 ABI 函数表只增不改：新函数追加尾部，运行时按 APP 编译时的
format 值裁剪。图标、id、manifest 键名一经发布不改名（IMRUN 图标同规）。

---

# v0.2 增补（2026-10-14，实装后修订：插件模块标准）

热加载已实装（dlopen + --export-dynamic 符号解析），标准按实际落地修订：

## 模块目录标准（/home/user/imrun/apps/<id>/）
```
<id>/                 目录名 = app id（snake_case）
  app.so              必有。ARM ELF shared（-shared -fPIC -nostdlib）
  icon.png            可选（48x48，显示层未来支持）；向量图标优先见下
  manifest.ini        可选（name/version/author；缺省用 imapp_name()）
```

## app.so 导出函数标准
| 导出 | 必需性 | 签名 | 说明 |
|---|---|---|---|
| imapp_frame | **必需** | `void imapp_frame(double t)` | 每帧画 ImGui 内容（窗口本体由 shell 提供）；t = 相对秒 |
| imapp_name | 推荐 | `const char* imapp_name(void)` | 显示名；缺省用目录名 |
| imapp_icon | 推荐 | `void imapp_icon(void* dl, float cx, float cy, float s)` | **桌面图标（向量绘制）**：dl 是 ImDrawList*，以 (cx,cy) 为中心、s 为半径画；不导出则回退默认立方体（不推荐，千篇一律） |
| imapp_init | 可选 | `int imapp_init(void)` | 加载时调用一次 |
| imapp_destroy | 可选 | `void imapp_destroy(void)` | 卸载/窗口关闭时调用——**释放资源（停音频/震动）的唯一钩子** |
| imapp_pref_size | 可选 | `void imapp_pref_size(float* w, float* h)` | **声明首选窗口尺寸**。少数非 440×320 的应用（如竖屏游戏）用；宿主在应用首次打开时应用一次（即使 imrun.ini 存了旧尺寸也覆盖）。缺省或 0 = 用宿主默认 |

## 输入约定（触摸/键盘）
- 触摸屏点击 = 核心 X 指针（ButtonPress/Release），运行时已喂给 ImGui。
- 应用判断"点击自己窗口"用 `ImGui::IsWindowHovered(...) && ImGui::IsMouseClicked(0)`，
  **不要**直接读全局 `IsMouseClicked`——否则点标题栏/桌面/任务栏也会误触发。
- 键盘用 `ImGui::IsKeyPressed(ImGuiKey_Space/UpArrow/Enter)`。

## 宿主供给（可直接引用的符号，从主程序解析）
- 全部 ImGui 符号（Begin/Text/Button/...）
- `audio_ui_start/audio_ui_stop/audio_on/audio_deadline/audio_volume/audio_wave`
- `vibra_ping/vibra_stop`、`imrun_now()`（相对时钟，禁止自己取 epoch）
- `imrun_font_cn/world/kr()`（字体句柄）、`last_key[]`（按键记录）

## 生命周期
- 安装 = 放入 apps/（2s 内热加载）；卸载 = 长按桌面图标红叉 / APPMGR（dlclose + 删目录）
- 窗口关闭 = destroy() 调用；最小化 = 保活
- 系统程序（SYSINFO/WALLPAP/APPMGR）内建不可卸载
