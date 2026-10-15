# IMRUN 音频后端调查交接文档 v2(给能力强的执行型智能体)

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
- 开发日志:`build/GLDEMO_IMGUI.md`(搜 v0.4.38/40/41/42)
