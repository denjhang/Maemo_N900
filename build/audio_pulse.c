/* audio_pulse.c - libpulse-simple 包装 (v0.4.46 新后端, paplay 风格).
 * 通过 dlopen libpulse-simple.so.0 + libpulse.so.0 加载, 不直接 -lpulse-simple
 * 链接 (主程序已用 --export-dynamic, 插件用 -nostdlib).
 * 与 plughw 路径不同的关键: 输出流入 pulse sink, pulse server 自动应用
 * sink volume (Maemo 系统栏/硬件键/amixer 改 sink vol 时输出立即变).
 * AOTEST 已验证 pulse 路径不死锁, paplay 30s 流畅. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>

/* libpulse-simple 用到的类型 — 我们在运行时 dlopen, 不引头文件 (sysroot
 * 头已部署, 但 dlopen 路径不依赖编译时). 重新声明核心符号: */
typedef struct pa_simple pa_simple;

/* 函数指针类型 */
typedef pa_simple* (*pa_simple_new_fn)(const char *server, const char *name,
                                        int dir, const char *dev,
                                        const char *stream_name,
                                        const void *ss,    /* pa_sample_spec */
                                        const void *map,   /* pa_channel_map or NULL */
                                        const void *attr,  /* pa_buffer_attr or NULL */
                                        int *error);
typedef void (*pa_simple_free_fn)(pa_simple *s);
typedef int  (*pa_simple_write_fn)(pa_simple *s, const void *data, size_t bytes, int *error);
typedef int  (*pa_simple_drain_fn)(pa_simple *s, int *error);
typedef int  (*pa_simple_flush_fn)(pa_simple *s, int *error);
typedef unsigned long (*pa_simple_get_latency_fn)(pa_simple *s, int *error);

/* pa_sample_spec 布局 (必须与 libpulse 0.9.15 一致) */
typedef struct {
    int format;       /* PA_SAMPLE_S16LE = 3, PA_SAMPLE_S16BE = 4, etc */
    unsigned int rate;
    unsigned char channels;
} pa_sample_spec;

/* dlopen 的库句柄 */
static void *hpsimp = NULL, *hplib = NULL;
static pa_simple_new_fn  fn_new = NULL;
static pa_simple_free_fn fn_free = NULL;
static pa_simple_write_fn fn_write = NULL;
static pa_simple_drain_fn fn_drain = NULL;
static pa_simple_flush_fn fn_flush = NULL;
static pa_simple_get_latency_fn fn_latency = NULL;

static int pulse_lib_loaded = 0;

static int pulse_load_libs(void) {
    if (pulse_lib_loaded) return 0;
    hpsimp = dlopen("libpulse-simple.so.0", 2 /*RTLD_NOW*/);
    if (!hpsimp) { fprintf(stderr, "PULSE: libpulse-simple.so.0 not found: %s\n", dlerror()); return -1; }
    fn_new   = (pa_simple_new_fn)dlsym(hpsimp, "pa_simple_new");
    fn_free  = (pa_simple_free_fn)dlsym(hpsimp, "pa_simple_free");
    fn_write = (pa_simple_write_fn)dlsym(hpsimp, "pa_simple_write");
    fn_drain = (pa_simple_drain_fn)dlsym(hpsimp, "pa_simple_drain");
    fn_flush = (pa_simple_flush_fn)dlsym(hpsimp, "pa_simple_flush");
    fn_latency = (pa_simple_get_latency_fn)dlsym(hpsimp, "pa_simple_get_latency");
    if (!fn_new || !fn_free || !fn_write || !fn_drain || !fn_flush || !fn_latency) {
        fprintf(stderr, "PULSE: missing libpulse-simple symbols\n");
        dlclose(hpsimp); hpsimp = NULL;
        return -1;
    }
    hplib = dlopen("libpulse.so.0", 2 /*RTLD_NOW*/);
    if (!hplib) { fprintf(stderr, "PULSE: libpulse.so.0 not found: %s\n", dlerror()); return -1; }
    /* 验证符号, 即使不直接用 */
    pulse_lib_loaded = 1;
    fprintf(stderr, "PULSE: libpulse-simple + libpulse loaded\n");
    return 0;
}

/* 实例状态: 一个 active stream */
static pa_simple *pulse_stream = NULL;
static int pulse_open = 0;

int audio_pulse_open(int rate, int channels) {
    pa_sample_spec ss;
    int err = 0;
    if (pulse_open) return 1;
    if (pulse_load_libs() < 0) return 0;
    /* format 3 = PA_SAMPLE_S16LE (little-endian signed 16-bit) */
    ss.format = 3;
    ss.rate = (unsigned)rate;
    ss.channels = (unsigned char)channels;
    /* server=NULL (默认), name="imrun", dir=PA_STREAM_PLAYBACK=1,
       dev="sink.hw0" — N900 关键: 必须显式指定 sink.hw0 (直连 ALSA hw:0),
       不能用默认 sink (sink.music = Nokia music 模块路由, 实测卡死).
       sink.hw0 的 volume 由 module-alsa-sink-volume 控制 HP_DAC/Headphone
       硬件寄存器 => 系统音量条/硬件键调它就是调我们的输出. */
    pulse_stream = fn_new(NULL, "imrun", 1 /*PA_STREAM_PLAYBACK*/, "sink.hw0",
                          "imrun-playback", &ss, NULL, NULL, &err);
    if (!pulse_stream) {
        fprintf(stderr, "PULSE: pa_simple_new failed err=%d\n", err);
        return 0;
    }
    pulse_open = 1;
    fprintf(stderr, "PULSE open  rate=%d ch=%d\n", rate, channels);
    return 1;
}

int audio_pulse_write(const short *buf, int frames) {
    int err = 0;
    if (!pulse_open) return 0;
    /* pa_simple_write expects bytes = frames * channels * sizeof(sample) */
    if (fn_write(pulse_stream, buf, (size_t)frames * 2 * sizeof(short), &err) < 0) {
        fprintf(stderr, "PULSE write FAIL err=%d\n", err);
        return 0;
    }
    return 1;
}

/* 检查 pulse server 端是否有缓冲空间 (latency < 阈值 = 有空间).
   writer 在 write 前调用, 返回 0=缓冲满应该等, 1=可以 write.
   避免直接调 pa_simple_write 阻塞 (同步 API 无超时, 停播时死锁). */
int audio_pulse_can_write(int max_latency_us) {
    int err = 0;
    if (!pulse_open) return 0;
    unsigned long lat = fn_latency(pulse_stream, &err);
    if (err) return 1;  /* latency 查询失败, 保守放行 */
    /* lat 单位 us. < max_latency_us = 缓冲没满, 可以写. */
    return lat < (unsigned)max_latency_us ? 1 : 0;
}

/* 返回当前 server 端延迟 (us), 用于 UI 显示和 writer 节流. */
unsigned long audio_pulse_latency(void) {
    int err = 0;
    if (!pulse_open) return 0;
    return fn_latency(pulse_stream, &err);
}

void audio_pulse_close(void) {
    int err = 0;
    if (!pulse_open) return;
    /* 关键: 先 flush 丢弃 server 端缓冲, 否则 pa_simple_free 内部的 drain
       会等所有未播数据播完 (可达数百 ms), 期间若 writer 线程还在
       pa_simple_write 阻塞, 上层 pthread_join 会挂住 (AOTEST 死等). */
    fn_flush(pulse_stream, &err);   /* 丢弃缓冲, 让阻塞的 write 立即返回 */
    fn_free(pulse_stream);          /* 释放 stream (内部会断开连接) */
    pulse_stream = NULL;
    pulse_open = 0;
    fprintf(stderr, "PULSE closed (flush err=%d)\n", err);
}

int audio_pulse_alive(void) {
    return pulse_open;
}
