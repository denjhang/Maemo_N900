# IMRUN 音频后端调查交接文档

> 给下一个 ZCode 窗口:本文档是 N900 IMRUN 三音频后端调查的全部已知事实。
> 目标只有一个:**搞清楚 vgmplay-maemo 当年为什么随时打开都能顺畅播放,
> 而我们逐行复刻它的调用序列却无声/卡死。** 不要凭推理改代码,每个结论要设备实证。

## 设备访问

- N900 Maemo Fremantle,`192.168.101.108`,root/123456
- SSH CLI 需要 legacy 算法(`-oHostKeyAlgorithms=+ssh-rsa -oPubkeyAcceptedAlgorithms=+ssh-rsa -oStrictHostKeyChecking=no`),
  **推荐直接用 python + paramiko**(模板见下);前台可能挂住的命令会超时,用 `nohup ... > /tmp/x.out 2>&1 &` + sleep + cat
- IMRUN 主体在设备 `/home/user/imrun/imrun`(pidof imrun),源码 `build/gldemo_imgui.c` + `build/imgui_gles1.cpp`
- 插件源码 `research/apps/*/*.cpp`,装在 `/home/user/imrun/apps/*/app.so`

```python
import warnings; warnings.filterwarnings('ignore')
import paramiko
c = paramiko.SSHClient(); c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect('192.168.101.108', username='root', password='123456', timeout=15)
i,o,e = c.exec_command("命令", timeout=30)
print(o.read().decode(), e.read().decode())
c.close()
```

## 任务背景

IMRUN v0.4.40/41 增加了三个可选音频后端(DEMO → TONEGEN 里单选):

| 选项 | 实现 | 现状 |
|---|---|---|
| ALSA | plughw:0,0 常驻持有 + writei,自写 writer 线程 | 能出声,但 STOP/拖音量瞬间偶发秒级 XRUN 风暴(audio_dbg.log 里 223 条 `T ERR w=-32 state=4`,聚簇在 UI 卡顿 72~93ms 后;plain prepare 无法恢复这个 plughw) |
| AO(vgmplay) | dlopen `/opt/libao/lib/libao.so.2`,复刻 vgmplay Stream.c:ao_initialize → ao_default_driver_id → ao_open_live({16,44100,2,4}) → 阻塞 ao_play 2KB 循环 | **完全无声**;独立探针在设备上**卡死**(睡在 do_sys_poll,持有 pipes+socket = pulse 客户端特征) |
| SDL(ppsspp) | dlopen `/usr/lib/libSDL-1.2.so.0`,复刻 ppsspp PCMain.cpp:SDL_Init(AUDIO) → SDL_OpenAudio(44100/S16/2ch/2048, callback) → SDL_PauseAudio(0) | **无声**(未深查) |

## 已实证的设备事实(截至 2026-10-15 15:30)

1. **libao 真身**:`/opt/libao/lib/libao.so.2`;插件目录 `/opt/libao/lib/ao/plugins-2/` 里**只有 libalsa09.so**(无 pulse 插件、无 oss 插件);`/etc/libao.conf` 不存在。
2. **pulseaudio 以 system-mode 运行**:`/usr/bin/pulseaudio --system --high-priority`(pid ~876),module-suspend-idle(空闲释放声卡)。
3. **探针卡死位置**:睡在 `do_sys_poll`,fd 表有 pipes+socket(pulse native 协议客户端特征)——疑似 ALSA 的 pulse 插件连 system-mode pulse 时被卡(alive 但 15 秒无输出,alarm 自杀后无输出文件内容,stdout 全缓冲坑已用 setvbuf 规避过一次仍无输出,原因待查)。
4. **上一个被取消的智能体**用 `canberra-gtk-play`(libcanberra→pulse)测试,**手机响起了 NokiaTune.aac 循环**(`/usr/share/sounds/NokiaTune.aac` 存在,notification-daemon 回放)——这反证:**从 user 会话经 libcanberra→pulse 路径是能出声的**。它的探针产物在 /tmp/(ap*.out 全空、diag.sh 空、ao_probe2 存在)。该智能体被取消前没能回传结论。
5. vgmplay 二进制已不在设备上(`/usr/bin/VGMPlay_040-9_src/` 不存在),无法直接对照运行;本地源码 `research/vgmplay-maemo/VGMplay for maemo 5/VGMPlay_040-9_src/`(Makefile `USE_LIBAO=1` 默认,Stream.c 是后端)。
6. IMRUN 主程序是 **root 启动 → setuid(29999) 丢特权**;探针直接 su - user。这个差异从未被排除过。

## 核心谜题(新窗口的主线)

vgmplay 当时 = libao → 唯一插件 alsa09 → ALSA(哪个设备?)→ ??? → 出声顺畅。
我们 = 同样的调用序列 → 卡死/无声。

**必须回答的问题:**
1. ao_default_driver_id() 在这台机器上返回什么?ao_open_live 到底走到哪一步卡住(读 libao 0.8.8 源码 `research/libao-src/libao-0.8.8/src/audio_out.c` 的驱动加载与 conf 逻辑 + alsa09 插件源码,然后设备上验证)?
2. alsa09 插件打开的是 "default"(→pulse)还是 "hw"?如果是 default→pulse,为什么 libcanberra 经 pulse 能响而 alsa09 经 pulse 卡死(cookie?system-mode auth?X11 root-vs-user?)?
3. vgmplay 当年的运行身份/环境(run.sh 是用户跑的;但当年 pulse 状态、plughw 占用情况可能不同)。
4. SDL 无声的独立原因(SDL 1.2 的 audio driver 在 Maemo 是什么?pulse 插件?dsp?SDL_AUDIODRIVER 环境变量没设时 fallback 到哪?)。

## 已修好、与调查无关的前端 bug(v0.4.42,已部署)

- 负秒数计时:AO writer 到 deadline break 时没清 audio_on → 已清;SDL 由 audio_ctl 线程代为执行 deadline;demo 显示 clamp ≥0
- 闪退:`audio_stop_full` 在 pcm 已释放(NULL)时掉进 ALSA 分支解引用 NULL → 顶部加空态短路

## 工具链坑(复用必读)

- WSL 交叉编译主程序配方:`build/` 下 job3.sh(经 `wsl -e bash -c "$(cat job3.sh)" < srcs.b64`,数据走 stdin);
- 独立小程序链接配方(普通 crt1 与 glibc2.5 有代差,必须):
  `arm-linux-gnueabi-gcc --sysroot=/opt/sysroot -O1 -nostartfiles -nodefaultlibs -Wl,--dynamic-linker,/lib/ld-linux.so.3 -o out src.c /mnt/e/.../build/libdl.so.2 -lm -lgcc -lgcc_s -lc`
- 设备 /tmp 放不下大文件,sftp 传 /home/user;
- 探针 stdout 全缓冲:必须 setvbuf(stdout,NULL,_IONBF,0),输出走 stderr 更稳;
- 严禁留下无限循环出声的进程在设备上跑(上一个智能体留下了 NokiaTune 循环,用户当场炸了)。

## 底线

- ALSA 后端和 UI 一行都不要动(用户红线:v0.4.38 曾把播放改挂过一次);
- 每个结论必须有命令输出或源码行号支撑;
- 最终交付:vgmplay 成功的机制链 + 在 IMRUN 里复现它需要的条件清单(环境变量/驱动选择/打开顺序)。
