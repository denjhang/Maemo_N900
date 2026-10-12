# gldemo2 闪退调试交接（2026-10-13）—— ✅ 已解决，见文末结论

## 结论（2026-10-13 当天修完）
1. **"日志零输出"的元凶：设备 /tmp 是 1MB tmpfs 且 100% 满**——所有写文件的输出（含 > 重定向）静默失败。日志请写 /home/user/。SEGV handler 从头到尾是清白的（但已删除，裸信号 + dmesg 更可靠）。
2. **闪退真凶：`glColorPointer(3, GL_FLOAT, ...)` 3 分量颜色数组**——SGX 驱动在带 3 分量 color array 的绘制调用里直接段错误。改成 4 分量 RGBA（alpha=1）后 e/q/v 全部通过。
3. 最终成绩（EXIT=0，DEMO COMPLETE）：triangles 61.8 FPS / sprites(300) 49-50 FPS（vsync 封顶）。
4. 教训：本次崩在 mode q/e 首个绘制调用 + /tmp 满，两条线叠加成了"88 但零日志"的迷雾。部署/编译坑：WSL /tmp 每次调用会被清空，cp 到 /tmp 再编译必须单条命令内完成；/mnt/e 偶发写 0 字节文件，部署前校验 md5。


## 背景与当前状态
- 程序：`build/gldemo2.c`，N900 GLES1.1 GPU 基准（三角形 + 300 精灵）
- 编译：WSL MaemoBuild，配方见本文件末尾；部署到设备 `/home/user/gldemo2`（root/123456，paramiko 2.12）
- **图形链路已经打通过**：EGL 初始化全 OK（config 匹配默认 visual 33、800×480、"warm-up draw OK"），旧构建连续渲染 ~310 帧无崩溃。**GPU 栈没问题，崩溃是最新补丁引入的。**
- 用户在手机上看到：先是有黑框窗口（旧版黑屏），最新版**秒闪退**。

## 已确证的事实（别重新推导）
1. 旧版黑屏根因 = **GL_QUADS 在 OpenGL ES 不存在**（图元被驱动丢弃，只剩 0.05,0.05,0.1 清屏色）。已换 GL_TRIANGLE_STRIP（tri 基准）+ 索引 DrawElements（精灵基准）。
2. 新构建闪退，退出码 **88** = 源码里 SIGSEGV 处理器的 `_exit(88)`（处理器只在 `bench_triangles()` 开头安装）。→ 崩溃发生在**进入三角形基准之后**。
3. 但日志**零输出**（即使 main 开头加了 `setvbuf(stdout,NULL,_IONBF,0)` ×2）——init 阶段的 printf 全没出现。矛盾点：没看到 init 输出却触发了 bench 里的处理器。**先解决这个矛盾再动代码。**
4. 无 DISPLAY 运行同一路径：干净打印 "XOpenDisplay FAIL" 退出 1 → 二进制、_start、libc、printf 全正常。
5. 设备 dmesg 反复出现 `pvr: gldemo2: cleaning up 139 unfreed resources` —— SGX 驱动层在给我们擦屁股，几十次崩溃后驱动可能已带病。
6. 参考代码在本地：PPSSPP EGL 流程 `research/ppsspp_n900/ppsspp-0.9.6/native/base/PCMain.cpp`（EGL_Open/EGL_Init + 主循环 while(true){PollEvent → NativeRender → eglSwapBuffers}）；N900 专属软渲染范本 `research/emulator_sources/cps2emu-0.6/src/emumain.c`（HAA 硬件合成作者试过失败弃用——别碰 HAA）。

## 头号嫌疑（按概率排序）
A. **段错误处理器自身在递归崩溃**：ucontext 偏移是手猜的（`unsigned long *u=(unsigned long*)uc; unsigned long *sc=u+5; sc[18]` 这些魔数在当前 build 上可能错位），handler 里 printf→_exit(88) 之前再触发一次 SEGV 的话行为不可预测。**EXIT=88 但零输出完全可能是 handler 第一条 printf 就炸了。**
B. sprites 静态数组/索引改动引入的野指针（`static GLushort sidx[NSPRITE*6]` 在函数内 static，应无问题，但 433-440 行补丁段值得重读）
C. SGX 驱动带病（历史上几十次崩溃后），eglInitialize 路径行为改变

## 调试步骤（按序执行，每步一个动作）
1. **换 fd 输出**：所有诊断 printf 改为 `fprintf(stderr, ...)`（stderr 默认无缓冲），运行 `DISPLAY=:0 /home/user/gldemo2 > /tmp/o.log 2> /tmp/e.log`。stderr 立即可见，一步排除"缓冲吞日志"。
2. **handler 去依赖**：SEGV 处理器内改用裸 `write(2, buf, n)` + `snprintf` 到栈缓冲，绝不 printf；或者干脆先删掉整个 handler（内核直接给你 139 + dmesg 一行），比 handler 猜偏移可靠。
3. **二分基准**：`echo n > /tmp/gmode` 跑 mode 'n'（只 clear+swap 不画）。活着 → 逐步加回 strip 绘制/精灵绘制，钉死引入点。
4. **查驱动状态**：`dmesg | grep pvr | tail`；如果 pvr 报错堆积，`/etc/init.d/pvrsrvlink stop/start` 或直接重启设备再跑一次——排除带病驱动。
5. **对照实验**：如果 SGX 路径怎么都 88，跑一个已知好的 GLES 程序验证驱动（PPSSPP deb 在 research/ppsspp_n900/ 有下载；或装 glmemperf——源码包 Wayback 直链在 research/garage_maemo/file_links.txt，pkgs/ 下可能已由子智能体抓回）。
6. 修好后**第一件事验证可见性**：跑 10 秒内手机屏幕上应出现旋转的彩色三角（红/绿/蓝顶点）。FPS 结果会打在日志 MODE 行。

## 编译部署配方（照抄）
```bash
cd "E:/working/zcode-projects/Maemo_N900/build" && wsl -d MaemoBuild -- bash -c 'cp /mnt/e/working/zcode-projects/Maemo_N900/build/gldemo2.c /tmp/gb.c && arm-linux-gnueabi-gcc --sysroot=/opt/sysroot -Wl,--sysroot=/opt/sysroot -nostartfiles -O2 -o /tmp/gb /tmp/gb.c -Wl,--no-as-needed /mnt/e/working/zcode-projects/Maemo_N900/build/libdl.so.2 -lm 2>&1 | head -30; cp /tmp/gb /mnt/e/working/zcode-projects/Maemo_N900/build/gldemo2bin && echo BUILD_OK'
```
- **必须先 cp 到 /tmp 再编译**（WSL 9p/drvfs 读 /mnt/e 会拿旧内容）
- 部署：Windows python + paramiko（192.168.101.111 root/123456，allow_agent=False, look_for_keys=False），sftp put 到 /home/user/gldemo2，chmod 755
- 运行建议前台阻塞式（`DISPLAY=:0 /home/user/gldemo2 > log 2>&1; echo EXIT=$?`），后台 nohup 会丢启动
- 改 C 源码用 Edit/Read 工具或 python 单行，**别用 bash heredoc 写含字符串字面量的 C**（反斜杠会被吃）

## 兜底方案
GPU 路线若 1 小时内钉不死：先按 cps2emu 范式出 **SDL 软渲染版 demo**（libSDL 1.2 设备现成，SkyRaid pygame 也证明窗口层通），保证"屏幕上有东西"，GPU 基准降级为后台任务继续磨。两条腿走路，别死磕。
