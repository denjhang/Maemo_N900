# gldemo_showcase — N900 SGX530 全屏炫技 Demo 开发文档

2026-10-13 立项。目标：拍视频用的招牌 demo，把本项目摸通的全部图形本领在一个程序里循环展示。无限循环，永不退出（`pkill gldemo_showcase` 手动停）。

## 基础设施（全部继承自已验证的 gldemo2 / gldemo_scene）
- 裸 X11 + EGL + GLES1.1，运行时 dlopen，零链接期图形依赖
- **全屏三件套**（glmemperf recipe，见 GLDEMO2_DEBUG.md）：`_HILDON_NON_COMPOSITED_WINDOW=1` + `_NET_WM_STATE=FULLSCREEN` 属性 + **map 后**才发 ClientMessage。常量 XA_ATOM=4 / XA_INTEGER=12，绝不能再错
- 诊断 printf 宏重定向到 stderr；日志/模式文件在 /home/user/（/tmp 是 1MB 满的 tmpfs）
- 颜色数组**一律 4 分量 RGBA**（SGX 驱动 3 分量崩，见 GLDEMO2_DEBUG.md 结论）
- `_start` 直呼 main/exit，-nostartfiles，编译配方同 gldemo2（libdl.so.2 shim + -lm）
- 模式：默认 'b' 全场景循环；`/home/user/gmode` 单字符强制指定场景

## 场景表（每场 12 秒，'b' 模式循环；左上角常驻 FPS + 场景名）

| # | key | 场景 | 看点 / 技术点 |
|---|-----|------|--------------|
| 1 | logo | 3D LOGO 开机 | "N900" 用立方体笔画在 3D 中拼出并旋转，扫光加载条，粒子环绕 |
| 2 | ui | 多级菜单 UI | 标题屏→主菜单（光标游走+高亮动画）→Settings 滑条→确认对话框；菜单内嵌**实时 3D 预览窗**（ortho→frustum 往返） |
| 3 | tunnel | 隧道穿越 | 经典 GLES1 隧道：正弦半径的八边形环串成隧道，镜头钻行，颜色沿深度渐变，TRIANGLE_STRIP |
| 4 | terrain | 岩浆星球飞越 | 高度场网格（多正弦叠加），GL_LINES 线框地形，镜头低空掠过，高度→颜色映射（深蓝→橙红），远景星空 |
| 5 | particles | 粒子喷泉 | 1000 点粒子，重力抛物线，色相循环，地面碰撞反弹，GL_POINTS 批渲染 |
| 6 | spectrum | 频谱分析仪 | 32 根条形（多正弦伪 FFT 起舞）+ 地面倒影（Y 翻转暗色）+ 峰值下落点 |
| 7 | space | 星际战斗 | 星空 warp + 六立方体编队 + 敌方菱形波次俯冲 + HUD（准星/血条/SCORE/WAVE） |

## 渲染架构
- 2D 批处理：`push_quad`/`push_point`/`draw_text`（3x5 位图字体，GL_POINTS 渲染）→ 每帧一次 `batch_flush()`
- 3D：正交/透视投影按场景切换，每场景自管 clear；共享立方体几何（36 顶点 RGBA）
- 常驻 HUD：顶栏 FPS+场景名+分辨率，底栏 "N900 SGX530 GLES1.1 NON-COMPOSITED"
- 帧统计：每 30s 向 stderr 打一行 RUNNING 摘要（帧数/FPS）

## 验收标准
- EXIT 永不出现（无限循环）；stderr 无 Segmentation fault
- 全屏：状态栏消失、画面 800×480 满幅
- 每场景肉眼流畅（目标 vsync 上限 50-80 FPS）
- 手机上 `./gldemo_showcase` 一条命令即可开演

## 坑位检查清单（写代码时逐条过）
- [ ] 所有 glColorPointer 均 4 分量
- [ ] 静态顶点缓冲容量：quad 8192 verts / point 16384 verts / 场景各自的网格
- [ ] DrawArrays 数量与实际填充数一致（grid_n 教训）
- [ ] 全屏三件套顺序：属性→map→sync→消息
- [ ] 日志走 stderr，文件在 /home/user/
- [ ] gmode 读取 /home/user/gmode，缺省 'b'
- [ ] 编译必须 WSL 单条命令内 cp 到 /tmp 再编（/tmp 易失），产物 md5 校验后部署
