# IMRUN 编译指南

## 概述

本文档详细说明如何从源码编译 IMRUN 主程序和插件。

**当前版本：** IMRUN v0.4.46r + 插件 (2026-10-16)

## 环境要求

### 工具链
- **WSL** (Windows Subsystem for Linux)
- **交叉编译器：** `arm-linux-gnueabi-g++` / `arm-linux-gnueabi-gcc`
- **Sysroot：** `/opt/sysroot` (N900 ARM rootfs)

### 必需的库（在 sysroot 中）
- `libstdc++.so.6`
- `libdl.so.2`
- `libpulse-simple.so` (符号链接到 `libpulse-simple.so.0`)
- `libpulse.so` (符号链接到 `libpulse.so.0`)
- `libpulsecommon-0.9.15.so`

### 验证环境
```bash
# 检查交叉编译器
wsl which arm-linux-gnueabi-g++

# 检查 sysroot
wsl ls /opt/sysroot/usr/lib/libstdc++.so.6

# 检查 PulseAudio 库
wsl ls -la /opt/sysroot/usr/lib/libpulse*.so
```

## 主程序编译

### 源文件位置
```
build/gldemo_imgui.c       # 主入口、X11/EGL 初始化、音频系统
build/audio_pulse.c         # PulseAudio 后端实现
build/imgui_gles1.cpp      # ImGui GLES1.1 后端 + 插件系统
build/imgui/*.cpp           # ImGui 核心库 (1.92.8 docking)
```

### 编译步骤

**方法1：使用编译脚本**
```bash
cd /mnt/e/working/zcode-projects/Maemo_N900/build
wsl bash b_imgui_export.sh
```

**方法2：手动编译（推荐，避免换行符问题）**
```bash
wsl bash << 'EOF'
set -e
cd /mnt/e/working/zcode-projects/Maemo_N900/build
S=/opt/sysroot
F="--sysroot=$S -O2 -fsigned-char -fno-threadsafe-statics -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -DNDEBUG -I. -Iimgui"

# 编译 C 源文件
arm-linux-gnueabi-gcc  $F -c gldemo_imgui.c -o /tmp/gi.o
arm-linux-gnueabi-gcc  $F -c audio_pulse.c -o /tmp/audio_pulse.o

# 编译 C++ 源文件
arm-linux-gnueabi-g++  $F -c imgui_gles1.cpp -o /tmp/ig.o

# 编译 ImGui 库
for f in imgui imgui_draw imgui_tables imgui_widgets imgui_demo; do
    arm-linux-gnueabi-g++ $F -c imgui/$f.cpp -o /tmp/$f.o
done

# 链接
arm-linux-gnueabi-g++ --sysroot=$S -nostartfiles -nodefaultlibs \
    -Wl,--dynamic-linker,/lib/ld-linux.so.3 -Wl,--export-dynamic \
    /tmp/gi.o /tmp/audio_pulse.o /tmp/ig.o \
    /tmp/imgui.o /tmp/imgui_draw.o /tmp/imgui_tables.o /tmp/imgui_widgets.o /tmp/imgui_demo.o \
    $S/usr/lib/libstdc++.so.6 libdl.so.2 \
    -lpulse-simple -lpulse -lm -lgcc -lgcc_s -lc \
    -o gldemo_imguibin

echo "BUILD_OK"
ls -lh gldemo_imguibin
md5sum gldemo_imguibin
EOF
```

### 关键编译选项说明

| 选项 | 说明 |
|------|------|
| `--sysroot=$S` | 指定目标系统根目录 |
| `-O2` | 优化级别（N900 CPU 性能有限） |
| `-fsigned-char` | 字符类型为有符号（ARM 默认） |
| `-fno-threadsafe-statics` | 避免 `__cxa_guard_*` 符号依赖 |
| `-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0` | 禁用 FORTIFY（老系统兼容） |
| `-DNDEBUG` | 禁用断言 |
| `--export-dynamic` | **关键！**导出符号供插件使用 |
| `-nostartfiles -nodefaultlibs` | 手动控制链接库 |

### 输出
- **文件：** `build/gldemo_imguibin`
- **大小：** ~1.4 MB
- **预期 md5：** `a05fe3b8d77eaf24c35c1fc7e906a274`

## 插件编译

### 插件列表

| 插件名 | 源文件 | 功能 |
|--------|--------|------|
| **demo** | `research/apps/demo/demo.cpp` | 演示应用（6个标签页：音频测试/振动/按键/字体等） |
| **vgmplayer** | `research/apps/vgmplayer/vgm.cpp` | VGM 音乐播放器（YM2612/AY8910 芯片模拟） |
| **appmgr** | `research/apps/appmgr/appmgr.cpp` | 应用管理器（安装/卸载/刷新插件） |
| **system** | `research/apps/system/system.cpp` | 系统设置（音量/亮度/字体等） |

### 编译命令

```bash
wsl bash << 'EOF'
set -e
cd /mnt/e/working/zcode-projects/Maemo_N900
S=/opt/sysroot
F="--sysroot=$S -O2 -fPIC -shared -nostdlib -fno-threadsafe-statics -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -DNDEBUG -I./build -I./build/imgui"

echo "编译插件..."

arm-linux-gnueabi-g++ $F research/apps/demo/demo.cpp -o build/demo.so
arm-linux-gnueabi-g++ $F research/apps/vgmplayer/vgm.cpp -o build/vgmplayer.so
arm-linux-gnueabi-g++ $F research/apps/appmgr/appmgr.cpp -o build/appmgr.so
arm-linux-gnueabi-g++ $F research/apps/system/system.cpp -o build/system.so

echo "BUILD_OK"
ls -lh build/*.so | grep -E "demo|vgmplayer|appmgr|system"
EOF
```

### 插件接口规范

每个插件必须导出以下符号：

```cpp
extern "C" {
    // 必需：每帧渲染函数
    void imapp_frame(double t);
    
    // 可选：应用名称
    const char *imapp_name(void);
    
    // 可选：初始化函数
    void imapp_init(void);
    
    // 可选：销毁函数
    void imapp_destroy(void);
    
    // 可选：系统信息
    const char *imapp_system(void);
}
```

### 关键插件编译选项

| 选项 | 说明 |
|------|------|
| `-fPIC` | 位置无关代码（共享库必需） |
| `-shared` | 生成共享库 |
| `-nostdlib` | 不链接标准库（由主程序提供） |

### 验证插件符号

```bash
# 检查是否导出 imapp_frame
wsl nm -D build/demo.so | grep imapp_frame
```

## 部署到 N900

### 文件结构

```
/home/user/
├── gldemo_imguibin_export          # 主程序
└── imrun/
    └── apps/
        ├── demo/
        │   └── app.so                # demo 插件
        ├── vgmplayer/
        │   └── app.so                # VGM 播放器插件
        ├── appmgr/
        │   └── app.so                # 应用管理器插件
        └── system/
            └── app.so                # 系统设置插件
```

### 部署脚本

```python
import paramiko

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect('192.168.101.108', username='root', password='123456',
          allow_agent=False, look_for_keys=False)

# 停止运行中的进程
c.exec_command('killall -9 gldemo_imguibin_export; sleep 1')

# 上传主程序
sftp = c.open_sftp()
sftp.put('build/gldemo_imguibin', '/home/user/gldemo_imguibin_export')
sftp.chmod('/home/user/gldemo_imguibin_export', 0o755)

# 上传插件
sftp.put('build/demo.so', '/home/user/imrun/apps/demo/app.so')
sftp.put('build/vgmplayer.so', '/home/user/imrun/apps/vgmplayer/app.so')
sftp.put('build/appmgr.so', '/home/user/imrun/apps/appmgr/app.so')
sftp.put('build/system.so', '/home/user/imrun/apps/system/app.so')
sftp.close()

# 启动
import time
c.exec_command(f'date -s @{int(time.time())}; export DISPLAY=:0 && nohup /home/user/gldemo_imguibin_export > /tmp/imrun.log 2>&1 &')

c.close()
```

## 故障排查

### 问题1：插件加载失败

**症状：** 日志显示 `BOOTORDER dyn_n < 4`

**排查步骤：**
```bash
# 检查插件文件是否存在
ssh root@192.168.101.108 'ls -la /home/user/imrun/apps/*/app.so'

# 检查插件是否有 imapp_frame 符号
ssh root@192.168.101.108 'nm -D /home/user/imrun/apps/demo/app.so | grep imapp_frame'

# 查看详细错误（如果主程序有 DYN_SCAN 日志）
ssh root@192.168.101.108 'grep DYN_SCAN /home/user/audio_dbg.log'
```

**常见原因：**
- 插件缺少 `imapp_frame` 符号
- 插件依赖主程序未导出的符号（主程序需要 `--export-dynamic`）
- 插件文件损坏

### 问题2：主程序启动失败

**症状：** 进程无法启动或立即退出

**排查步骤：**
```bash
# 查看启动日志
ssh root@192.168.101.108 'cat /tmp/imrun.log'

# 检查库依赖
ssh root@192.168.101.108 'ldd /home/user/gldemo_imguibin_export'

# 检查权限
ssh root@192.168.101.108 'ls -la /home/user/gldemo_imguibin_export'
```

**常见原因：**
- 缺少 PulseAudio 库（libpulse-simple.so、libpulse.so）
- 文件权限不正确（需要可执行权限）
- X11 DISPLAY 未设置

### 问题3：编译错误

**症状：** 链接时提示 `cannot find -lpulse-simple`

**解决方案：**
```bash
# 检查 sysroot 中的库
wsl ls -la /opt/sysroot/usr/lib/libpulse*.so

# 如果缺少，从 N900 复制
scp root@192.168.101.108:/usr/lib/libpulse-simple.so.0 /tmp/
scp root@192.168.101.108:/usr/lib/libpulse.so.0 /tmp/
wsl sudo cp /mnt/c/Users/*/AppData/Local/Temp/libpulse*.so.0 /opt/sysroot/usr/lib/
wsl sudo ln -sf /opt/sysroot/usr/lib/libpulse-simple.so.0 /opt/sysroot/usr/lib/libpulse-simple.so
wsl sudo ln -sf /opt/sysroot/usr/lib/libpulse.so.0 /opt/sysroot/usr/lib/libpulse.so
```

## 版本历史

### v0.4.46r (2026-10-16)
- ✅ PULSE 后端完整实现
- ✅ 4后端并存（SDL/plughw、PULSE、ALSA、libao）
- ✅ 系统音量实时响应
- ✅ 48000Hz 零重采样

### 文件清单
```
主程序: gldemo_imguibin (md5: a05fe3b8d77eaf24c35c1fc7e906a274)
插件:
  - demo.so (~12KB)
  - vgmplayer.so (~23KB)
  - appmgr.so (~11KB)
  - system.so (~12KB)
```

## 参考文档

- [热重载实现](./HOTRELOAD_IMPLEMENTATION.md) - 插件热重载机制
- [插件开发指南](./PLUGIN_DEV_GUIDE.md) - 插件 API 详细说明
- [PULSE 后端实现](./PULSE_BACKEND_IMPLEMENTATION.md) - 音频后端架构
- [主程序详细文档](../build/GLDEMO_IMGUI.md) - 完整技术细节
