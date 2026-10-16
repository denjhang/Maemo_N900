#!/bin/bash
# IMRUN Release 打包脚本
# 将主程序和所有插件打包为 release 压缩包

set -e

VERSION="v0.4.46r"
DATE=$(date +%Y%m%d)
RELEASE_NAME="imrun-${VERSION}-${DATE}"
RELEASE_DIR="release/${RELEASE_NAME}"

echo "=== IMRUN Release 打包 ==="
echo "版本: ${VERSION}"
echo "日期: ${DATE}"
echo ""

# 创建目录结构
mkdir -p ${RELEASE_DIR}/{bin,plugins,docs}

# 复制主程序
echo "复制主程序..."
cp build/gldemo_imguibin ${RELEASE_DIR}/bin/
chmod +x ${RELEASE_DIR}/bin/gldemo_imguibin

# 复制插件
echo "复制插件..."
cp build/demo.so ${RELEASE_DIR}/plugins/
cp build/vgmplayer.so ${RELEASE_DIR}/plugins/
cp build/appmgr.so ${RELEASE_DIR}/plugins/
cp build/system.so ${RELEASE_DIR}/plugins/

# 复制文档
echo "复制文档..."
cp docs/BUILD_GUIDE.md ${RELEASE_DIR}/docs/
cp docs/HOTRELOAD_IMPLEMENTATION.md ${RELEASE_DIR}/docs/
cp docs/PULSE_BACKEND_IMPLEMENTATION.md ${RELEASE_DIR}/docs/
cp README.md ${RELEASE_DIR}/

# 创建部署脚本
cat > ${RELEASE_DIR}/deploy.sh << 'EOF'
#!/bin/bash
# IMRUN 部署脚本
# 使用方法: ./deploy.sh <N900_IP>

if [ -z "$1" ]; then
    echo "用法: $0 <N900_IP>"
    echo "示例: $0 192.168.101.108"
    exit 1
fi

N900_IP=$1
N900_USER=root
N900_PASS=123456

echo "=== 部署 IMRUN 到 N900 ($N900_IP) ==="

# 停止运行中的进程
sshpass -p $N900_PASS ssh ${N900_USER}@${N900_IP} 'killall -9 gldemo_imguibin_export; sleep 1'

# 上传主程序
echo "上传主程序..."
sshpass -p $N900_PASS scp bin/gldemo_imguibin ${N900_USER}@${N900_IP}:/home/user/gldemo_imguibin_export
sshpass -p $N900_PASS ssh ${N900_USER}@${N900_IP} 'chmod +x /home/user/gldemo_imguibin_export'

# 上传插件
echo "上传插件..."
sshpass -p $N900_PASS scp plugins/demo.so ${N900_USER}@${N900_IP}:/home/user/imrun/apps/demo/app.so
sshpass -p $N900_PASS scp plugins/vgmplayer.so ${N900_USER}@${N900_IP}:/home/user/imrun/apps/vgmplayer/app.so
sshpass -p $N900_PASS scp plugins/appmgr.so ${N900_USER}@${N900_IP}:/home/user/imrun/apps/appmgr/app.so
sshpass -p $N900_PASS scp plugins/system.so ${N900_USER}@${N900_IP}:/home/user/imrun/apps/system/app.so

# 启动
echo "启动 IMRUN..."
sshpass -p $N900_PASS ssh ${N900_USER}@${N900_IP} "date -s @$(date +%s); export DISPLAY=:0 && nohup /home/user/gldemo_imguibin_export > /tmp/imrun.log 2>&1 &"

sleep 3

# 检查状态
echo ""
echo "=== 检查状态 ==="
sshpass -p $N900_PASS ssh ${N900_USER}@${N900_IP} 'pidof gldemo_imguibin_export && echo "✓ IMRUN 运行中" || echo "✗ IMRUN 未启动"'
sshpass -p $N900_PASS ssh ${N900_USER}@${N900_IP} 'grep BOOTORDER /home/user/audio_dbg.log | tail -1'

echo ""
echo "部署完成！"
EOF

chmod +x ${RELEASE_DIR}/deploy.sh

# 创建 README
cat > ${RELEASE_DIR}/RELEASE_README.md << 'EOF'
# IMRUN Release

Nokia N900 ImGui 运行时环境 + 插件系统

## 版本信息

- **版本**: v0.4.46r
- **编译日期**: 2026-10-16
- **架构**: ARM (N900)

## 包含内容

### 主程序
- `bin/gldemo_imguibin` - IMRUN 主程序 (~1.4MB)
  - ImGui 1.92.8 docking 分支
  - GLES1.1 渲染后端
  - 4音频后端（SDL/ALSA/AO/PULSE）
  - 插件热重载系统

### 插件
- `plugins/demo.so` - 演示应用（音频测试/振动/按键/字体）
- `plugins/vgmplayer.so` - VGM 音乐播放器
- `plugins/appmgr.so` - 应用管理器
- `plugins/system.so` - 系统设置

### 文档
- `docs/BUILD_GUIDE.md` - 完整编译指南
- `docs/HOTRELOAD_IMPLEMENTATION.md` - 热重载实现
- `docs/PULSE_BACKEND_IMPLEMENTATION.md` - 音频后端架构

## 快速部署

### 方法1：使用部署脚本（需要 sshpass）
```bash
./deploy.sh 192.168.101.108
```

### 方法2：手动部署
```bash
# 上传主程序
scp bin/gldemo_imguibin root@192.168.101.108:/home/user/gldemo_imguibin_export
ssh root@192.168.101.108 'chmod +x /home/user/gldemo_imguibin_export'

# 上传插件
scp plugins/demo.so root@192.168.101.108:/home/user/imrun/apps/demo/app.so
scp plugins/vgmplayer.so root@192.168.101.108:/home/user/imrun/apps/vgmplayer/app.so
scp plugins/appmgr.so root@192.168.101.108:/home/user/imrun/apps/appmgr/app.so
scp plugins/system.so root@192.168.101.108:/home/user/imrun/apps/system/app.so

# 启动
ssh root@192.168.101.108 'export DISPLAY=:0 && /home/user/gldemo_imguibin_export &'
```

## 验证

```bash
# 检查进程
ssh root@192.168.101.108 'pidof gldemo_imguibin_export'

# 检查日志
ssh root@192.168.101.108 'tail -20 /home/user/audio_dbg.log'

# 应该看到
[     1.xxx] BOOTORDER order_n=4 dyn_n=4
```

## 目标设备

- **设备**: Nokia N900
- **OS**: Maemo 5 (Fremantle)
- **CPU**: ARM Cortex-A8 600MHz
- **GPU**: PowerVR SGX530
- **RAM**: 256MB

## 功能特性

### 音频系统
- ✅ 4后端并存（SDL/plughw、PULSE、ALSA、libao）
- ✅ 系统音量实时响应
- ✅ 48000Hz 零重采样
- ✅ 无断流稳定播放

### 插件系统
- ✅ 热重载（无需重启主程序）
- ✅ 独立编译、独立更新
- ✅ 用户可安装自定义插件

### UI
- ✅ ImGui 停靠布局
- ✅ 触摸屏优化
- ✅ 最小化/恢复动画
- ✅ 任务栏管理

## 已知问题

- 无

## 更新日志

### v0.4.46r (2026-10-16)
- PULSE 后端完整实现
- 系统音量实时响应
- 4后端全通过测试

## 技术支持

详细文档请参阅 `docs/` 目录。
EOF

# 生成文件清单
echo "生成文件清单..."
cat > ${RELEASE_DIR}/MANIFEST.txt << EOF
IMRUN Release ${VERSION} (${DATE})

主程序:
$(md5sum ${RELEASE_DIR}/bin/gldemo_imguibin)

插件:
$(md5sum ${RELEASE_DIR}/plugins/*.so)

文件总数: $(find ${RELEASE_DIR} -type f | wc -l)
总大小: $(du -sh ${RELEASE_DIR} | cut -f1)
EOF

# 创建压缩包
echo ""
echo "创建压缩包..."
cd release
tar czf ${RELEASE_NAME}.tar.gz ${RELEASE_NAME}/
cd ..

echo ""
echo "=== 打包完成 ==="
echo "输出: release/${RELEASE_NAME}.tar.gz"
ls -lh release/${RELEASE_NAME}.tar.gz
echo ""
echo "内容清单:"
cat ${RELEASE_DIR}/MANIFEST.txt
