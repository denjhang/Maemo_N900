# IMRUN 音频后端调查交接文档 v2(给能力强的执行型智能体)

> **⚠️ 本文档已归档。调查已完成，结论见底部"七、最终结论"。**
> 以下原始内容保留作为调查过程记录，但其中"设备 libao 是完整 0.8.8 框架"的核心假设**已被探针实测推翻**——设备 `/opt/libao/lib/libao.so.2` 实为 11KB mini 定制版，无 `ao_append_option`、硬编码 `snd_pcm_open("default")`。完整分析和修复见 `build/GLDEMO_IMGUI.md` v0.4.43 条目。

> **你是执行型调查员,不是顾问。** 本文档给足了你全部已知事实和工具。
> 你的产出必须是:①设备上跑通的三路探针实测结果;②一个改好的 IMRUN 后端补丁(经本窗口部署验证)。
> 不许只写分析和建议然后停手。不许在设备上留下任何循环发声的进程。不许动 ALSA 后端和 UI 的既有行为。

## 一、任务一句话

搞清楚 vgmplay-maemo 当年为什么随时打开都能顺畅播放,并把那条链在 IMRUN 的第二音频后端(AO)里复现到设备上出声。

## 二、环境与访问(全部实测有效)

- **目标设备**:N900 Maemo Fremantle,IP `192.168.101.108`,root/123456。
  用 python+paramiko(本机 Windows Git Bash 有 python 3.14 + paramiko):

```python
import warnings; warnings.filterwarnings('ignore')
import paramiko
c = paramiko.SSHClient(); c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect('192.168.101.108', username='root', password='123456', timeout=15)
i,o,e = c.exec_command("命令", timeout=30)
print(o.read().decode(), e.read().decode())
c.close()
```

  - 前台会挂的命令(音频探针、任何 while 循环)必须 `nohup ... > /tmp/x.out 2>&1 &` + sleep + cat;
  - **绝对禁止**在设备上留下循环播放声音的进程(上一个智能体留了个 NokiaTune 循环,用户当场炸了);
  - **测试音量要小**(amp ≤ 2500 级别)、时长 ≤ 3 秒。

- **交叉编译工具链(在 WSL 里,不在 Windows!)**:
  - 编译器:WSL 内 `arm-linux-gnueabi-gcc`(gcc-cross 11);进 WSL 用 `wsl -e bash -c "..."`
  - sysroot:WSL 内 `/opt/sysroot`(Maemo glibc 2.5);Windows 侧副本 `E:\working\zcode-projects\Maemo_N900\research\sysroot`
  - **链接配方**(普通 crt1 与 glibc 2.5 有代差,普通 main 链接必挂,必须用这个):

```bash
arm-linux-gnueabi-gcc --sysroot=/opt/sysroot -O1 \
  -nostartfiles -nodefaultlibs \
  -Wl,--dynamic-linker,/lib/ld-linux.so.3 \
  -o out probe.c \
  /mnt/e/working/zcode-projects/Maemo_N900/build/libdl.so.2 \
  -lm -lgcc -lgcc_s -lc
```

  - 源文件放 `E:\working\zcode-projects\Maemo_N900\build\`(WSL 路径 `/mnt/e/working/zcode-projects/Maemo_N900/build/`),编译在 WSL /tmp 做;
  - 探针 C 代码必须自带:`void *__attribute__((visibility("hidden"))) __dso_handle = (void *)&__dso_handle;`(无 crt);`setvbuf(stdout,NULL,_IONBF,0)` 且关键步骤打印到 stderr;
  - 二进制 sftp 传到设备 `/home/user/`(设备 /tmp 放不下大文件),`chmod 755` 后 `su - user -c 'DISPLAY=:0 /home/user/xxx'` 跑。

- **IMRUN 源码**:宿主 `E:\working\zcode-projects\Maemo_N900\build\gldemo_imgui.c`(音频后端全在这一个文件里,搜 `audio_backend`);插件 `research/apps/demo/demo.cpp`(TONEGEN 单选框)。部署由**主窗口**做,你不要自己部署主程序。

## 三、已实证事实(全部可信,别重复验证浪费时间)

1. IMRUN v0.4.42 三后端单选(DEMO→TONEGEN):**ALSA(plughw 直连,能响但 UI 卡顿时偶发 XRUN 风暴)/ AO(无声,探针卡死)/ SDL(无声,未深查)**。
2. 设备 libao:`/opt/libao/lib/libao.so.2`;插件目录 `/opt/libao/lib/ao/plugins-2/` **只有 libalsa09.so**;无 /etc/libao.conf。
3. pulseaudio **system-mode** 运行(`--system --high-priority`,pid~876,module-suspend-idle 空闲释放声卡)。
4. **AO 无声/卡死根因(上一窗口已基本锁定,你负责实证闭环)**:alsa09 插件按 "default" 打开 → ALSA 的 pulse 插件 → system-mode pulse 只认 native socket+cookie → **user(29999) 读 pulse cookie EACCES → 探针永久睡在 do_sys_poll**(带 pulse socket fd)。独立探针已复现。
5. **libcanberra→pulse 能出声**(旁证:曾触发 NokiaTune.aac 回放,用户亲耳听到)——pulse 路径本身活着,死的是 cookie 授权那条腿。
6. **上一窗口对 vgmplay 成功机制的最佳判断**:vgmplay 时代 pulse 是 per-user 模式(每个用户自己的 cookie/runtime socket),"default" 能通;现在是 system-mode,"default" 是死路。**唯一可复现路径 = 让 alsa09 绕过 pulse 直连 ALSA**:`ao_set_option` / `ao_option { "dev", "plughw:0,0" }`(读 `research/libao-src/libao-0.8.8/src/plugins/alsa09/` 源码确认选项键名和优先级)。
7. SDL 后端无声原因未查。怀疑同根(SDL 1.2 Maemo 的 audio driver 卡 pulse 连接),可用 `SDL_AUDIODRIVER=dsp` 环境变量对照,但 N900 无 /dev/dsp,预期此路不通——以实测为准。
8. ALSA 直连 plughw:0,0 **能出声**(探针和 IMRUN ALSA 后端都证明过)。

## 四、你的执行清单(按顺序)

1. **探针三连**(小音量、≤3 秒,全走 paramiko+nohup):
   a. ALSA plughw:0,0 直写(对照组,预期出声);
   b. ao_open_live + `ao_option{"dev","plughw:0,0"}`(alsa09 直连,验证 vgmplay 真路径);
   c. ao_open_live + 默认选项(复现卡死,给结论补最后一枪;用 `alarm(10)` 自杀)。
   读 libao 0.8.8 源码确认 alsa09 选项键名(可能是 "dev" 或 "dsp"),**源码为准**。
2. **改 IMRUN AO 后端**:`build/gldemo_imgui.c` 里搜 `ao_open_live`,把 ao_open_live 调用改为带 `{"dev","plughw:0,0"}` 选项(结构 `typedef struct { const char *name; const char *value; } ao_option_t;`,链表 next 字段按 libao 头文件真实布局——读 `research/libao-src/libao-0.8.8/include/ao/ao_private.h` 或 ao.h 确认,别猜)。改完**不要自己部署**,停在本地编译通过。
3. **SDL 后端**:`strings`/源码级确认 SDL 1.2 在 Maemo 的 audio driver 缺省值;若同样是 pulse 死路,在交接记录里写明结论即可,**不要求修**。
4. **汇报**:探针输出原文 + 改动 diff + 结论,交给主窗口部署验证。

## 五、红线

- ALSA 后端(backend 0)与 UI 逻辑一行不动;`audio_stop_full` 的空态短路、deadline 清理(v0.4.42 修的)不许回退;
- 不在设备上部署任何东西(探针二进制除外,放 /home/user,测试完 `rm` 掉);
- 测试进程必须 self-terminate(alarm/有限时长),离开前 `ps` 确认无残留、无声音循环;
- 设备上 pulse 服务、tonegend 等**系统进程不许杀**。

## 六、参考文件

- libao 0.8.8 完整源码:`research/libao-src/libao-0.8.8/`(audio_out.c=核心,plugins/alsa09/=插件)
- vgmplay-maemo 后端:`research/vgmplay-maemo/VGMplay for maemo 5/VGMPlay_040-9_src/Stream.c`(USE_LIBAO 路径)
- ppsspp 后端:`research/ppsspp_n900/ppsspp-0.9.6/native/base/PCMain.cpp`
- IMRUN 音频代码:`build/gldemo_imgui.c`(搜 audio_backend / ao_open_live / sdl_open)
- 开发日志:`build/GLDEMO_IMGUI.md`(搜 v0.4.38/40/41/42/43)

## 七、最终结论（调查完成，2026-10-15）

### AO 后端无声根因（推翻本文档第三·五节的假设）

本文档假设设备 `/opt/libao/lib/libao.so.2` 是"完整 0.8.8 框架 + dlopen alsa09 插件"。**探针实测推翻了这个假设**：

1. **设备 libao 是 11KB mini 定制版（`mini_libao.c`），不是完整 0.8.8**：只导出 8 个函数，**没有 `ao_append_option`/`ao_option`**，`ao_open_live` 硬编码 `snd_pcm_open("default")`。无法指定 `dev=plughw:0,0`。
2. **`/etc/asound.conf` 把 ALSA `default` 路由到 `type pulse`**：mini libao 的 `snd_pcm_open("default")` → pulse。路径不可控、绕不开 pulse。
3. **完整 libao 0.8.8 在 `research/libao-src/libao-0.8.8/src/.libs/libao.so.2.1.3`（61KB，带 dlopen/dlsym 引用，插件路径硬编码 `/opt/libao/lib/ao/plugins-2`）**。设备 alsa09 插件就在该路径下。
4. **build/libao.so.2（58KB）是残缺版**：编译时 `HAVE_DLOPEN` 未定义，只有静态 null/wav/raw/au 驱动，没有 alsa09。

### 修复

- `ao_open_live` 第三参数从 `NULL` 改为手建的 `{"dev","plughw:0,0",NULL}` 单节点链表（结构布局从 `ao.h` 实证）
- dlopen 搜索路径增加 `/home/user/imrun/audiolib/libao.so.2`（部署完整版 61KB libao 到这里）
- ALSA/SDL/UI 一行未动

### 探针实测（全部 user(29999) 模式）

| 探针 | 结果 |
|---|---|
| A: ALSA plughw:0,0 直写 | ✅ 出声 |
| B: build/libao(58KB)+dev | ❌ 残缺版无 dlopen 插件 |
| C: 设备 mini libao 默认 | ⚠️ 能 open 但路径不可控 |
| D: **完整 61KB libao+alsa 插件+dev=plughw** | ✅ **出声** |
| E: 完整 61KB libao+default_driver_id+dev | ✅ open OK |

### AOTEST 零断流验证

20 轮 PLAY/快速调音量(15档×2)/STOP/双STOP 循环：`hard_fail=0 max_stall=0 stalls=0 underruns=0`，`AO play FAIL`=0。

### SDL 后端无声结论

设备 `libSDL-1.2.so.0` ldd 依赖 `libpulse-simple.so.0`+`libpulse.so.0`——100% 走 pulse，与 AO 默认死路同根。N900 无 `/dev/dsp`。

### PPSSPP 音频架构研究

PPSSPP 的 SDL callback `mixaudio` → `NativeMix` → `PSPMixer::Mix` → `__AudioMix`，从 `outAudioQueue`（`FixedSizeQueue<s16, 512*16>` = 186ms 深缓冲）弹出采样。生产者（`__AudioUpdate`，每 64 样本/1.45ms 由 CoreTiming 触发）和消费者（SDL 音频线程，2048 样本/46ms）通过 186ms 队列解耦。IMRUN 的 SDL 后端无此队列，`sdl_mixaudio` 直接调 `gen_samples` 实时生成——无缓冲隔离，重 GPU 负载下直接 underrun。详见 `build/GLDEMO_IMGUI.md` v0.4.43 条目。
