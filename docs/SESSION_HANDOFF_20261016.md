# 会话交接文档 - 2026-10-16

## 当前系统状态

### ✅ N900 运行正常
- **主程序**: gldemo_imguibin_export (PID 1849)
- **版本**: v0.4.46r (md5: a05fe3b8d77eaf24c35c1fc7e906a274)
- **插件加载**: BOOTORDER order_n=4 dyn_n=4 ✓
- **桌面显示**: 4个图标正常
  1. DEMO - 演示应用
  2. VGM - 音乐播放器
  3. APPMGR - 应用管理器
  4. SETTINGS - 系统设置

### ✅ Git 仓库状态
- **最新提交**: ea65366 "RELEASE v0.4.46r: 完整编译文档 + Release打包"
- **分支**: main
- **已推送**: origin/main (同步完成)

## 本次会话完成的工作

### 1. 问题诊断与解决

**初始问题**：
- 尝试开发 Flappy Bird 游戏（纹理版本）
- 添加远程控制 API 失败
- 主程序被损坏，只显示1-3个应用图标

**根本原因**：
- git 仓库中的主程序二进制文件 (md5: f817090d) 是旧版本
- N900 上的插件文件也是旧版本，缺少 `imapp_frame` 符号
- 版本不匹配导致插件加载失败

**解决方案**：
1. 从当前源码重新编译主程序
2. 重新编译所有4个插件
3. 部署到 N900 验证通过

### 2. 创建的文档和工具

#### docs/BUILD_GUIDE.md
完整的编译指南，包含：
- 主程序编译步骤（包含 PulseAudio 支持）
- 4个插件编译步骤
- 部署脚本
- 故障排查指南
- 编译选项详细说明

#### make_release.sh
自动化 release 打包脚本：
- 创建完整的 release 目录结构
- 打包主程序和所有插件
- 生成部署脚本 (deploy.sh)
- 创建文件清单 (MANIFEST.txt)
- 生成 tar.gz 压缩包

#### release/imrun-v0.4.46r-20261016.tar.gz
完整的 release 包 (695KB)：
- 主程序和4个插件
- 完整文档
- 一键部署脚本

### 3. 回退的功能

**Flappy Bird 游戏**：
- 开发了 v5 矢量图形版本（工作正常）
- 尝试了 v6 纹理版本（纹理加载成功，但无法验证渲染）
- 最终决定回退，删除 flappy 插件

**远程控制 API**：
- 在主程序中添加了 `imrun_remote_open()` 等函数
- 但无法从外部简单调用（需要 gdb 等工具）
- 最终回退到原版本

## 关键技术发现

### 主程序编译要点

1. **必须包含 audio_pulse.c**
   ```bash
   arm-linux-gnueabi-gcc $F -c audio_pulse.c -o /tmp/audio_pulse.o
   ```

2. **必须链接 PulseAudio 库**
   ```bash
   -lpulse-simple -lpulse
   ```

3. **必须使用 --export-dynamic**
   ```bash
   -Wl,--export-dynamic
   ```
   否则插件无法解析主程序的符号

4. **sysroot 中需要的库**
   - libpulse-simple.so → libpulse-simple.so.0
   - libpulse.so → libpulse.so.0
   - libpulsecommon-0.9.15.so

### 插件编译要点

1. **必须导出 imapp_frame 符号**
   ```cpp
   extern "C" void imapp_frame(double t);
   ```

2. **编译选项**
   ```bash
   -fPIC -shared -nostdlib
   ```

3. **验证插件**
   ```bash
   nm -D plugin.so | grep imapp_frame
   ```

### 部署验证

```bash
# 检查进程
ssh root@192.168.101.108 'pidof gldemo_imguibin_export'

# 检查插件加载
ssh root@192.168.101.108 'grep BOOTORDER /home/user/audio_dbg.log'
# 应该看到: BOOTORDER order_n=4 dyn_n=4
```

## 下一步建议

### 如果需要继续开发

1. **开发新插件**
   - 参考 `research/apps/demo/demo.cpp`
   - 使用 BUILD_GUIDE.md 中的编译命令
   - 测试后部署到 N900

2. **修改现有插件**
   - 修改源码（如 `research/apps/vgmplayer/vgm.cpp`）
   - 编译后通过 APPMGR 热重载
   - 或直接 scp 到 N900

3. **更新主程序**
   - 修改 `build/imgui_gles1.cpp` 或 `build/gldemo_imgui.c`
   - 按 BUILD_GUIDE.md 重新编译
   - 部署到 N900（需要重启）

### 如果需要添加 Flappy Bird

**方案1：使用矢量图形版本**
- 源码在 `git stash` 中（如需恢复：`git stash pop`）
- 文件：`research/apps/flappy/flappy_v5_vector.cpp`
- 编译后部署到 `/home/user/imrun/apps/flappy/app.so`

**方案2：使用纹理版本**
- 需要解决远程测试问题（如何自动打开窗口）
- 或手动在 N900 上测试
- 源码：`research/apps/flappy/flappy_v6_textured.cpp`

### 如果需要远程控制

**当前主程序中已有的 API**（在 imgui_gles1.cpp）：
```cpp
extern "C" int imrun_remote_open(const char *app_name);
extern "C" int imrun_remote_open_by_index(int index);
extern "C" int imrun_remote_list_apps(char *buffer, int bufsize);
```

**调用方法**：
- 需要编写 C 程序通过 dlopen 调用
- 或使用 gdb 注入（需要在 N900 上安装 gdb）
- 或修改主程序添加信号/文件触发机制

## 文件位置快速参考

### 本地（Windows）
```
E:\working\zcode-projects\Maemo_N900\
├── build/
│   ├── gldemo_imguibin           # 主程序二进制（git tracked）
│   ├── gldemo_imguibin_fixed     # 本次编译的版本
│   ├── demo.so                   # 编译好的插件
│   ├── vgmplayer.so
│   ├── appmgr.so
│   └── system.so
├── research/apps/
│   ├── demo/demo.cpp             # 插件源码
│   ├── vgmplayer/vgm.cpp
│   ├── appmgr/appmgr.cpp
│   ├── system/system.cpp
│   └── flappy/                   # Flappy Bird 源码（已删除）
├── docs/
│   ├── BUILD_GUIDE.md            # 编译指南 ★
│   ├── HOTRELOAD_IMPLEMENTATION.md
│   └── SESSION_HANDOFF_20261016.md  # 本文档
└── release/
    └── imrun-v0.4.46r-20261016.tar.gz  # Release 包
```

### N900
```
/home/user/
├── gldemo_imguibin_export        # 运行的主程序
├── imrun/
│   └── apps/
│       ├── demo/app.so
│       ├── vgmplayer/app.so
│       ├── appmgr/app.so
│       └── system/app.so
├── audio_dbg.log                 # 主日志文件
└── /tmp/imrun.log                # 启动日志
```

## 快速命令参考

### 编译主程序
```bash
wsl bash << 'EOF'
set -e
cd /mnt/e/working/zcode-projects/Maemo_N900/build
S=/opt/sysroot
F="--sysroot=$S -O2 -fsigned-char -fno-threadsafe-statics -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -DNDEBUG -I. -Iimgui"

arm-linux-gnueabi-gcc  $F -c gldemo_imgui.c -o /tmp/gi.o
arm-linux-gnueabi-gcc  $F -c audio_pulse.c -o /tmp/audio_pulse.o
arm-linux-gnueabi-g++  $F -c imgui_gles1.cpp -o /tmp/ig.o
for f in imgui imgui_draw imgui_tables imgui_widgets imgui_demo; do
    arm-linux-gnueabi-g++ $F -c imgui/$f.cpp -o /tmp/$f.o
done

arm-linux-gnueabi-g++ --sysroot=$S -nostartfiles -nodefaultlibs \
    -Wl,--dynamic-linker,/lib/ld-linux.so.3 -Wl,--export-dynamic \
    /tmp/gi.o /tmp/audio_pulse.o /tmp/ig.o \
    /tmp/imgui.o /tmp/imgui_draw.o /tmp/imgui_tables.o /tmp/imgui_widgets.o /tmp/imgui_demo.o \
    $S/usr/lib/libstdc++.so.6 libdl.so.2 \
    -lpulse-simple -lpulse -lm -lgcc -lgcc_s -lc \
    -o gldemo_imguibin

md5sum gldemo_imguibin
EOF
```

### 编译插件
```bash
wsl bash << 'EOF'
cd /mnt/e/working/zcode-projects/Maemo_N900
S=/opt/sysroot
F="--sysroot=$S -O2 -fPIC -shared -nostdlib -fno-threadsafe-statics -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -DNDEBUG -I./build -I./build/imgui"

arm-linux-gnueabi-g++ $F research/apps/demo/demo.cpp -o build/demo.so
arm-linux-gnueabi-g++ $F research/apps/vgmplayer/vgm.cpp -o build/vgmplayer.so
arm-linux-gnueabi-g++ $F research/apps/appmgr/appmgr.cpp -o build/appmgr.so
arm-linux-gnueabi-g++ $F research/apps/system/system.cpp -o build/system.so

ls -lh build/*.so | tail -4
EOF
```

### 部署到 N900
```python
import paramiko
import time

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect('192.168.101.108', username='root', password='123456',
          allow_agent=False, look_for_keys=False)

c.exec_command('killall -9 gldemo_imguibin_export; sleep 1')

sftp = c.open_sftp()
sftp.put('build/gldemo_imguibin_fixed', '/home/user/gldemo_imguibin_export')
sftp.chmod('/home/user/gldemo_imguibin_export', 0o755)
sftp.put('build/demo.so', '/home/user/imrun/apps/demo/app.so')
sftp.put('build/vgmplayer.so', '/home/user/imrun/apps/vgmplayer/app.so')
sftp.put('build/appmgr.so', '/home/user/imrun/apps/appmgr/app.so')
sftp.put('build/system.so', '/home/user/imrun/apps/system/app.so')
sftp.close()

c.exec_command(f'date -s @{int(time.time())}; export DISPLAY=:0 && nohup /home/user/gldemo_imguibin_export > /tmp/imrun.log 2>&1 &')
c.close()
```

### 验证
```bash
ssh root@192.168.101.108 'grep BOOTORDER /home/user/audio_dbg.log | tail -1'
# 应该输出: [     1.xxx] BOOTORDER order_n=4 dyn_n=4
```

## 重要提醒

1. **主程序和插件版本必须匹配**
   - 如果重新编译主程序，必须重新编译所有插件
   - 否则会出现符号不匹配问题

2. **git 中的二进制文件可能过期**
   - 当前 git 中的 `build/gldemo_imguibin` (md5: f817090d) 是旧版本
   - 最新编译的版本 (md5: a05fe3b8) 在 `build/gldemo_imguibin_fixed`
   - 如果要更新 git，需要覆盖 `build/gldemo_imguibin`

3. **stash 中有未提交的修改**
   - Flappy Bird 相关代码
   - 远程控制 API 相关代码
   - 如需恢复：`git stash pop`

4. **Release 包已就绪**
   - 可以直接使用 `release/imrun-v0.4.46r-20261016.tar.gz`
   - 解压后使用 `deploy.sh` 一键部署
   - 或使用 `make_release.sh` 重新打包

## 联系与支持

- **项目仓库**: https://github.com/denjhang/Maemo_N900
- **最新提交**: ea65366
- **Release 标签**: v0.4.46r-20261016

---

**会话时间**: 2026-10-16 21:00 - 23:56  
**会话 ID**: #sess_20261016  
**状态**: ✅ 系统正常运行，所有文档已完成，已推送到远端
