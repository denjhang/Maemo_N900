# IMRUN Shell 设计（2026-10-13）

目标：全屏 ImGui 运行时 = 伪 OS。有桌面、任务栏（含时钟）、启动器、
应用切换，全部自带，不依赖 hildon。800x480 竖滑横置触摸屏。

## 1. 屏幕分区（固定 24px 任务栏 + 全屏内容区）

```
+--------------------------------------------------------------+
| [home] [icon][icon][icon]...      [back] 12:34  78% 42C     | <- 24px 任务栏
+--------------------------------------------------------------+
|                                                              |
|                       内容区（456px）                        |
|                 桌面 或 当前 APP 的 ImGui 窗口                |
|                                                              |
+--------------------------------------------------------------+
```

- **任务栏常驻顶部 24px**，任何状态下都在最上层，APP 内容区从 y=24 起
- 左起：**HOME 键**（IMRUN 立方体 logo，高亮=桌面态）→ 正在运行的 APP
  图标（切换器，点=切前台，长按=关掉该 APP）
- 右侧：**BACK 键**（当前 APP 全屏时点它回桌面，等价 Android back）、
  **时钟 HH:MM**（1 分钟刷新）、电量 %、CPU 温度（已有 hw_sample 供给）

## 2. 三个系统态（状态机，互斥）

1. **DESKTOP**：桌面 = 已安装 APP 网格（4 列 x 6 行，图标 64px+名字），
   点图标启动 APP；空白处显示 IMRUN 版本水印。数据源 =
   扫描 /home/user/imrun/apps/*/manifest.ini
2. **APP**：某 APP 独占内容区。HOME 或 BACK → 回 DESKTOP（APP 转后台
   保活，frame() 停调，内存留着）；再点任务栏图标 → 前台恢复
3. **SWITCHER**：HOME 键长按或任务栏空白处上滑 → 后台 APP 卡片列表，
   点卡片切换，卡片上 X 关闭（走 destroy+dlclose）

状态转换全部 <1 帧完成（只是 ImGui 窗口显隐 + 是否调 APP 的 frame）。

## 3. 交互细节（触摸优先）

- **手势**：内容区顶边下滑 8px = 呼出任务栏（APP 全屏沉浸模式下任务栏
  自动隐藏时）；现在 v0 任务栏常驻，手势后继加
- **时钟**：time.h 本地时间，不碰 osso-datetime；格式 HH:MM，任务栏右侧
- **任务栏渲染**：属于运行时自己的一层 ImGui window（NoScrollbar、固定
  pos、AlwaysAutoResize），先于 APP 窗口绘制，focus 不抢占
- **字体**：任务栏/桌面用 20px msyh.ttc（已上传 /home/user/fonts/），
  NEAREST 过滤——字体清晰度教训已入档

## 4. 存储布局（放 /home/user，文件管理器可见）

```
/home/user/imrun/            运行时 + 全部系统数据（MyDocs 是 FAT，无执行位，不能用！）
  imrun                      运行时二进制（现有 /home/user/imrun 迁入）
  apps/                      已安装 IMAPP 解包处
    n900.imrun.tonegen/
      manifest.ini  app.so  icon.png  data/
  desk.cfg                   桌面配置（图标排序/壁纸，v0 可无）
  imrun.log                  日志
```

- **为什么不放 /home/user/MyDocs**：那是 vfat 分区，无执行权限、无符号
  连接、大小写不敏感——app.so 根本没法 dlopen。代码在 rootfs，用户数据
  (app 的 data/) 也在上面布局里；只有用户手动导出的大文件才建议挪 MyDocs
- **空间**：rootfs /home/user 分区 ~2GB 可用，对 .so 级别的 APP 绰绰有余；
  manifest 里 `[size]` 段可声明配额，后继强制

## 5. 与系统的关系（不做的事）

- 不取代 hildon 状态栏（信号/蓝牙那些还是 Maemo 的）——IMRUN 任务栏只管
  自己这一亩三分地：HOME/切换/时钟/电量
- 不注册 IMAPP 到 hildon 桌面：只有 IMRUN 本体有 .desktop 图标，IMAPP 的
  图标只出现在 IMRUN 桌面网格里——单入口，符合"伪 OS 是一个 APP"的定位
- 锁屏/休眠：继续用 osso_display_blanking_pause 保活，不自己做锁屏

## 6. 实施顺序

1. shell 层抽离：gldemo_imgui.c 拆出 runtime（任务栏/桌面/状态机/时钟），
   ToneGen（现有音频面板）改造成第一个 .imapp 验证 ABI（IMAPP_SPEC v0.1）
2. APP 安装器 v0：桌面加"安装"入口 = 选 .imapp 路径解包（触摸选文件用
   文件浏览器路径输入框即可，不做图形文件选择器）
3. SWITCHER 卡片与手势（体验层，逻辑先跑通）
