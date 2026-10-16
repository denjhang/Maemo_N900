#!/bin/bash
# IMRUN Release 打包脚本 v0.4.47
# 主程序 + 5 插件（含 Flappy Bird + PNG 纹理素材）
set -e

VERSION="v0.4.47"
DATE=$(date +%Y%m%d)
RELEASE_NAME="imrun-${VERSION}-${DATE}"
RELEASE_DIR="release/${RELEASE_NAME}"
ROOT="$(cd "$(dirname "$0")" && pwd)"

echo "=== IMRUN Release 打包 ==="
echo "版本: ${VERSION}"
echo "日期: ${DATE}"
echo ""

rm -rf "${RELEASE_DIR}"
mkdir -p "${RELEASE_DIR}"/{bin,plugins,docs}

# --- 主程序 ---
echo "复制主程序..."
cp "${ROOT}/build/gldemo_v047" "${RELEASE_DIR}/bin/gldemo_imguibin"
chmod +x "${RELEASE_DIR}/bin/gldemo_imguibin"

# --- 插件 ---
echo "复制插件..."
cp "${ROOT}/build/demo.so"      "${RELEASE_DIR}/plugins/"
cp "${ROOT}/build/vgmplayer.so" "${RELEASE_DIR}/plugins/"
cp "${ROOT}/build/appmgr.so"    "${RELEASE_DIR}/plugins/"
cp "${ROOT}/build/system.so"    "${RELEASE_DIR}/plugins/"

# --- Flappy Bird（插件 + 纹理素材）---
echo "复制 Flappy Bird 插件 + 素材..."
cp "${ROOT}/build/flappy_v7.so" "${RELEASE_DIR}/plugins/flappy.so"
mkdir -p "${RELEASE_DIR}/plugins/flappy_assets"
cp "${ROOT}/assets/flappy-bird-assets/sprites/"*.png "${RELEASE_DIR}/plugins/flappy_assets/"

# --- 文档 ---
echo "复制文档..."
cp "${ROOT}/docs/BUILD_GUIDE.md"                    "${RELEASE_DIR}/docs/"
cp "${ROOT}/docs/FLAPPY_TEXTURE_DEBUG_NOTES.md"     "${RELEASE_DIR}/docs/"
cp "${ROOT}/docs/SESSION_20261017.md"               "${RELEASE_DIR}/docs/"
cp "${ROOT}/README.md"                              "${RELEASE_DIR}/"

# --- 部署脚本 ---
cat > "${RELEASE_DIR}/deploy.sh" << 'DEPLOY_EOF'
#!/bin/bash
# IMRUN 部署脚本  用法: ./deploy.sh <N900_IP> [ssh_password]
set -e
IP="${1:?用法: $0 <N900_IP> [password]}"
PASS="${2:-123456}"
SSH="sshpass -p $PASS ssh -o StrictHostKeyChecking=no root@$IP"
SCP="sshpass -p $PASS scp -o StrictHostKeyChecking=no"

echo "=== 部署 IMRUN $IP ==="
$SSH 'killall -9 gldemo_imguibin_export; sleep 1' || true

echo "上传主程序..."
$SCP bin/gldemo_imguibin "root@$IP:/home/user/gldemo_imguibin_export"
$SSH 'chmod +x /home/user/gldemo_imguibin_export'

echo "上传插件..."
for p in demo vgmplayer appmgr system; do
    $SCP "plugins/$p.so" "root@$IP:/home/user/imrun/apps/$p/app.so"
done

echo "上传 Flappy Bird + 素材..."
$SSH 'mkdir -p /home/user/imrun/apps/flappy/assets'
$SCP plugins/flappy.so "root@$IP:/home/user/imrun/apps/flappy/app.so"
$SCP plugins/flappy_assets/*.png "root@$IP:/home/user/imrun/apps/flappy/assets/"

echo "启动..."
$SSH "date -s @$(date +%s); export DISPLAY=:0 && nohup /home/user/gldemo_imguibin_export > /tmp/imrun.log 2>&1 &"
sleep 3

echo ""
echo "=== 状态 ==="
$SSH 'pidof gldemo_imguibin_export && echo "✓ 运行中" || echo "✗ 未启动"'
$SSH 'grep BOOTORDER /home/user/audio_dbg.log | tail -1'
echo "部署完成"
DEPLOY_EOF
chmod +x "${RELEASE_DIR}/deploy.sh"

# --- RELEASE README ---
cat > "${RELEASE_DIR}/RELEASE_README.md" << 'README_EOF'
# IMRUN Release v0.4.47

Nokia N900 ImGui 运行时 + 插件系统（含 PNG 纹理支持与 Flappy Bird）

## 版本信息
- **版本**: v0.4.47
- **架构**: ARM (Nokia N900 / Maemo 5)
- **主程序**: ImGui 1.92.8 docking + GLES1.1

## 本版新增（相对 v0.4.46r）

### PNG 纹理支持
- 主程序集成 stb_image，新增 API：
  - `imrun_load_png(path)` — 解码 PNG 为纹理
  - `imrun_register_texture(tex)` — 注册（上传由渲染循环完成）
- 插件可加载外部 PNG 素材并绘制

### Flappy Bird 游戏（示例插件）
- 原版素材纹理（背景/管道/小鸟）
- 小窗口模式，不遮挡任务栏
- 空格跳跃

## 本版修复的三个渲染 bug

1. **符号扩展**：GL 纹理 ID ≥ 2^31 时 `(intptr_t)` 转换符号扩展成无效值 → 改用零扩展
2. **宿主窗口契约**：插件必须画进宿主窗口，不能自己 `ImGui::Begin/End`
3. **NPOT 纹理**：N900 PowerVR SGX530 不支持非二次幂纹理 → 上传时放大到 POT

详见 `docs/FLAPPY_TEXTURE_DEBUG_NOTES.md`。

## 包含内容

### 主程序
- `bin/gldemo_imguibin` — IMRUN 主程序（~1.4MB）

### 插件
- `plugins/demo.so` — 演示应用
- `plugins/vgmplayer.so` — VGM 音乐播放器
- `plugins/appmgr.so` — 应用管理器
- `plugins/system.so` — 系统设置
- `plugins/flappy.so` + `plugins/flappy_assets/` — Flappy Bird + 纹理素材

### 文档
- `docs/BUILD_GUIDE.md` — 编译指南
- `docs/FLAPPY_TEXTURE_DEBUG_NOTES.md` — 纹理调试手记
- `docs/SESSION_20261017.md` — 会话记录

## 部署

```bash
./deploy.sh 192.168.101.108          # 需要 sshpass
```

## 验证

```bash
ssh root@192.168.101.108 'pidof gldemo_imguibin_export'
ssh root@192.168.101.108 'grep BOOTORDER /home/user/audio_dbg.log | tail -1'
# 应显示：BOOTORDER order_n=5 dyn_n=5
```

## 目标设备
- Nokia N900 / Maemo 5 (Fremantle)
- ARM Cortex-A8 600MHz / PowerVR SGX530 / 256MB RAM

## 更新日志

### v0.4.47
- PNG 纹理加载系统
- Flappy Bird 示例插件
- 修复 3 个渲染 bug（符号扩展 / 宿主窗口 / NPOT）
- 新增 framebuffer 截图自动验证脚本

### v0.4.46r
- PULSE 后端完整实现
- 系统音量实时响应
README_EOF

# --- 清单 ---
echo "生成文件清单..."
{
    echo "IMRUN Release ${VERSION} (${DATE})"
    echo ""
    echo "主程序:"
    md5sum "${RELEASE_DIR}/bin/gldemo_imguibin"
    echo ""
    echo "插件:"
    (cd "${RELEASE_DIR}/plugins" && md5sum *.so)
    echo ""
    echo "Flappy 素材: $(ls ${RELEASE_DIR}/plugins/flappy_assets/*.png | wc -l) 个 PNG"
    echo "文件总数: $(find ${RELEASE_DIR} -type f | wc -l)"
    echo "总大小: $(du -sh ${RELEASE_DIR} | cut -f1)"
} > "${RELEASE_DIR}/MANIFEST.txt"

# --- 压缩 ---
echo ""
echo "创建压缩包..."
( cd release && tar czf "${RELEASE_NAME}.tar.gz" "${RELEASE_NAME}/" )

echo ""
echo "=== 打包完成 ==="
ls -lh "release/${RELEASE_NAME}.tar.gz"
echo ""
cat "${RELEASE_DIR}/MANIFEST.txt"
