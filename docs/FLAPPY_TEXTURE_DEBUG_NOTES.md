# Flappy Bird 纹理渲染调试手记（符号扩展 bug + 宿主窗口契约）

**日期**: 2026-10-17
**分支**: feature/flappy-bird
**状态**: 主程序/插件已修复并编译；截图自动化验证管道已打通

---

## 一句话结论

画面空白是两个独立 bug 叠加：
1. **符号扩展 bug**：GL 纹理 ID（32 位无符号）经过 `intptr_t`（32 位有符号）转 64 位 `ImTextureID` 时，ID ≥ 2^31 会被符号扩展成 `0xFFFFFFFFxxxxxxxx`，绑定无效纹理 → 画不出来。
2. **宿主窗口契约 bug**：插件在 `imapp_frame` 里自己调了 `ImGui::Begin/End`，开了个"流氓"第二窗口，宿主开的 Flappy 窗口反而空着。

---

## Bug 1：符号扩展（根本原因，日志直接命中）

### 现象
日志中纹理上传后立即读取 `GetTexID()`：
```
upload_texture: 生成新纹理 tex=0x1987f8 id=315638026 → GetTexID=315638026     ✓
upload_texture: 生成新纹理 tex=0x198898 id=2731963459 → GetTexID=18446744072146547779  ✗
```

前 3 个（小鸟）正常，后 3 个（管道/背景/地面）全变成接近 2^64 的怪数。

### 根因
`ImTextureID` = `ImU64`（64 位）。GL 返回的 `glGenTextures` ID 是 **32 位无符号**，且随机分布，一半概率 ≥ 2^31。
错误代码：
```cpp
tex->SetTexID((ImTextureID)(intptr_t)id);   // intptr_t 在 32 位 ARM 上是有符号 32 位！
dl->AddImage((ImTextureID)(intptr_t)tex_id, ...);  // 插件绘制同样中招
```
`id = 2731963459 = 0xA2D7F1C3` → `intptr_t` 变成负的 `-1561286337` → 赋给 u64 时符号扩展成 `0xFFFFFFFFA2D7F1C3` = `18446744072146547779`。与日志完全吻合（用 Python 验证过）。

### 修复
```cpp
tex->SetTexID((ImTextureID)id);          // unsigned → u64 直接零扩展
dl->AddImage((ImTextureID)tex_id, ...);  // 同上
```

涉及位置（主程序 imgui_gles1.cpp）：
- `upload_texture()` 里 `SetTexID`
- `igui_render()` 里 WantDestroy 的 `did` 提取
- 绘制循环里 `cmd->GetTexID()` 的 `ctid`
- `imrun_register_texture_and_get_id()` 里 `SetTexID`

插件 flappy_v7.cpp 的 `draw_texture()` 同样修复。

---

## Bug 2：插件误调 Begin/End（窗口空白主因）

### 现象
截图显示：
- 宿主开的 "Flappy" 窗口是空的（无背景、无管道、无小鸟）
- 另一个 "Flappy Bird" 流氓窗口有自己的标题栏（我的 Begin 开的）

### 根因
`IMAPP_SPEC.md` 明确规定："**imapp_frame 每帧画 ImGui 内容（窗口本体由 shell 提供）**"。宿主在 `igui_build_ui` 里做：
```cpp
if (!shell_begin_dyn(...)) continue;   // 这里已经 ImGui::Begin(宿主窗口)
a.frame(t);                             // 插件只需往里画
shell_end_dyn();                        // ImGui::End
```
插件自己 `ImGui::Begin("Flappy Bird")` 会开一个 **不属于宿主管理** 的第二窗口，宿主窗口没有内容。

### 修复
插件 `imapp_frame`：
- 删除 `ImGui::Begin/End`
- 直接用 `ImGui::GetWindowDrawList()`（宿主窗口的 draw list）
- 游戏逻辑跑在虚拟 800×480 坐标系，绘制时映射到窗口内容区（`origin + area`），**窗口缩放不溢出，永不盖任务栏**

---

## 开发方法：framebuffer 截图自动验证

N900 没有截图工具（无 xwd/import/scrot），但 **`/dev/fb0` 可读**（800×480，16bpp RGB565，stride 4096）。

流程：
1. SSH 执行 `dd if=/dev/fb0 bs=4096 count=480`
2. 直接读 stdout 通道拿原始字节（设备上没有 base64！）
3. 本地 numpy 解析 RGB565 → PIL 存 PNG
4. 分析像素：唯一色数 / 天空蓝占比 / 绿色(管道) / 黄色(小鸟)

脚本：`build/test_visual.py`
- `python test_visual.py` —— 部署主程序+插件、自动开应用、抓屏、分析、出结论
- `python test_visual.py --capture-only` —— 只抓当前屏幕

注意点：
- 设备 `/tmp` 很小（写 647KB 就满了），**必须走 SSH 通道流式读**，不能先落盘
- 设备没有 `base64` 命令，不能 base64 编码传输
- fb0 的 16bpp 是 RGB565，解析要按 stride 4096（不是 800*2）reshape

---

## 下一步（未完成）

- [x] 符号扩展修复（已编译）
- [x] 宿主窗口契约修复（已编译 flappy_v7_hostwin.so）
- [ ] 部署 + 截图验证纹理真的画出来（天空蓝背景 / 绿管道 / 黄小鸟）
- [ ] 修复后清理调试日志，出干净发布版
- [ ] 更新 docs/PNG_TEXTURE_LOADING.md（错误地宣传了 GetTexID 用法，应改为宿主窗口 + 零扩展规则）
