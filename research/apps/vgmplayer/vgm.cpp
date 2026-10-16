/* IMRUN plugin: VGM/VGZ player (AY8910 core v3b, full command table + stereo).
 * v3b: libvgm 对齐版 - 修复噪声 LFSR + 改进音量表 + 立体声支持
 * 全量移植 libvgm _CMD_INFO[0x100] 命令长度表 (vgmplayer_cmdhandler.cpp).
 * 渲染: 主程序 audio_source_fn hook 按块拉取 -> vgm_render().
 */
#include "imgui.h"
#include "ay8910_v3.c"   /* AY8910 v3 核心内联, 修复噪声+音量表+立体声 */
/* ay8910_v3.c 函数声明 (纯 C, vgm.cpp 是 C++, 直接用) */
extern void ay_set_stereo_mask(unsigned char ch, unsigned char mask);
extern void ay_render_stereo(short *out_L, short *out_R);
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>
#include <dlfcn.h>

/* ================= 主程序导出符号 (--export-dynamic) ================= */
extern "C" {
void audio_ui_start(int seconds);
void audio_ui_stop(void);
void audio_ui_set_backend(int be);
extern int audio_on;
extern double audio_deadline;
extern int audio_volume, audio_wave;
extern int (*audio_source_fn)(short *dst, int frames); /* 我们注册的喂采样入口 */
extern int audio_rate;  /* 主程序运行时采样率: 44100 (plughw/SDL/ALSA) 或 48000 (PULSE) */
double imrun_now(void);
}

/* ================= 全量 VGM 命令长度表 (libvgm _CMD_INFO 逐字节移植) =================
 * 表项: {chipType, cmdLength}. 第二列 = 该命令总字节长度 (含命令字节本身),
 * 是"所有芯片命令正确跳过不脱 sync"的唯一依据. 0x00 = 特殊处理或非法.
 * chipType 值 (libvgm enum): 0x00 SN76489, 0x01 YM2413, 0x02 YM2612,
 * 0x03 YM2151, 0x04 SegaPCM, 0x05 RF5C68, 0x06 YM2203, 0x07 YM2608,
 * 0x08 YM2610, 0x09 YM3812, 0x0A YM3526, 0x0B Y8950, 0x0C YMF262,
 * 0x0D YMF278B, 0x0E YMF271, 0x0F YMZ280, 0x10 RF5C164, 0x11 PWM,
 * 0x12 AY8910, 0x13 GameBoyDMG, 0x14 NES APU, 0x15 YMW258, 0x16 uPD7759,
 * 0x17 OKIM6258, 0x18 OKIM6295, 0x19 K051649, 0x1A K054539, 0x1B HuC6280,
 * 0x1C C140, 0x1D K053260, 0x1E Pokey, 0x1F QSound, 0x20 SCSP, 0x21 WonderSwan,
 * 0x22 VSU-VUE, 0x23 SAA1099, 0x24 ES5503, 0x25 ES5506, 0x26 X1-010,
 * 0x27 C352, 0x28 GA20, 0x29 Mikey, 0xFF 无芯片/未知 */
static const unsigned char VGM_CMD_LEN[0x100] = {
    /* 00-0F */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 10-1F */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 20-2F */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 30 SN76489(2nd)  31 AY stereo  3F GG stereo */
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    /* 40 Mikey  4F GG stereo */
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x02,
    /* 50 SN76489  51 YM2413  52-53 YM2612  54 YM2151  55 YM2203
       56-57 YM2608  58-59 YM2610  5A YM3812  5B YM3526  5C Y8950
       5D YMZ280  5E-5F YMF262 */
    0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    /* 60 invalid  61 wait N(2B)  62 wait735  63 wait882  66 end  67 dblk
       68 pcmRAM(12B)  70-7F wait1-16 */
    0x00, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    /* 80-8F YM2612 PCM delay (len1, 数据从 block0, 等待随低4位) */
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    /* 90 DAC setup  91 DAC data  92 DAC freq  93 DAC play-loc
       94 DAC stop   95 DAC play-blk */
    0x05, 0x05, 0x06, 0x0B, 0x02, 0x05, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* A0 AY8910  A1-AF 各芯片 2nd chip reg8/data8 */
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    /* B0 RF5C68  B1 RF5C164  B2 PWM  B3 GameBoy  B4 NES  B5 YMW258
       B6 uPD7759  B7 OKIM6258  B8 OKIM6295  B9 HuC6280  BA K053260
       BB Pokey  BC WonderSwan  BD SAA1099  BE ES5506  BF GA20 */
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    /* C0 SegaPCM mem  C1 RF5C68 mem  C2 RF5C164 mem  C3 YMW bank
       C4 QSound  C5 SCSP  C6 WonderSwan mem  C7 VSU-VUE
       C8 X1-010  C9-CF unknown */
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    /* D0 YMF278B  D1 YMF271  D2 K051649  D3 K054539  D4 C140
       D5 ES5503  D6 ES5506(16bit)  D7-DF unknown */
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    /* E0 YM2612PCM seek  E1 C352  E2-FF unknown/len5 */
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
};

/* ================= VGM 文件状态 ================= */
static unsigned char *vgm_data = NULL;   /* 整个文件(或解压后)载入内存 */
static long vgm_size = 0;
static long vgm_filepos = 0;             /* 当前命令流位置 */
static long vgm_dataofs = 0;             /* 数据起始(绝对) */
static long vgm_dataend = 0;             /* 数据结束(绝对) */
static long vgm_loopofs = 0;             /* 循环点(绝对) */
static long vgm_loopticks = 0;
static long vgm_numticks = 0;            /* 总 tick */
static long vgm_tick = 0;                /* 已消费 tick */
static unsigned long vgm_filever = 0;
static int vgm_ay_clock = 0;
static int vgm_ym2149_div2 = 0;
static char vgm_gd3_title[256] = "";
static int vgm_eof = 0;
static int vgm_loop_enable = 1;          /* 循环播放默认开 */
static int vgm_loops_done = 0;
static int vgm_rate_set = 0;  /* vgm_start 清零, vgm_render 第一次调用时设采样率 */

static int vgm_loaded = 0;
static char vgm_path[512] = "";
static long vgm_pending = 0;        /* 当前待等待的采样数 (渲染模型核心) */

/* ================= GD3 标签 (UTF-16LE) ================= */
static void gd3_read(const unsigned char *d, long ofs) {
    char *out[7] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    static char title[256], game[256], sysname[256], author[256], release[64];
    long p;
    int i;
    vgm_gd3_title[0] = 0;
    if (ofs <= 0 || ofs + 12 > vgm_size) return;
    if (memcmp(d + ofs, "Gd3 \x1a", 5) != 0) return;
    p = ofs + 12;   /* "Gd3 \x1a" + ver(4) + strings */
    for (i = 0; i < 7; i++) {
        char *dst;
        int maxlen = 64;
        int j = 0;
        switch (i) {
            case 0: dst = title;  maxlen = 255; break;
            case 1: dst = game;   maxlen = 255; break;
            case 2: dst = sysname;maxlen = 255; break;
            case 3: dst = author; maxlen = 255; break;
            case 4: dst = release;maxlen = 63;  break;
            default: dst = NULL; break;
        }
        while (p + 1 < vgm_size) {
            unsigned short c = d[p] | (d[p + 1] << 8);
            p += 2;
            if (c == 0) break;
            if (dst && j < maxlen - 1 && c < 0x7f) { dst[j++] = (char)c; dst[j] = 0; }
        }
        if (i == 0 && title[0]) strncpy(vgm_gd3_title, title, 255);
    }
}

/* ================= VGZ 解压 (zlib dlopen, 失败跳过) ================= */
static void *vgz_hz = NULL;
static void *(*gzopen_fn)(const char *, const char *);
static int (*gzread_fn)(void *, void *, unsigned);
static int (*gzclose_fn)(void *);

static int vgz_load(const char *path) {
    void *f;
    if (!vgz_hz) {
        vgz_hz = dlopen("libz.so.1", 2 /*RTLD_NOW*/);
        if (!vgz_hz) { fprintf(stderr, "vgz: libz.so.1 not found\n"); return 0; }
        gzopen_fn = (void *(*)(const char *, const char *))dlsym(vgz_hz, "gzopen");
        gzread_fn = (int (*)(void *, void *, unsigned))dlsym(vgz_hz, "gzread");
        gzclose_fn = (int (*)(void *))dlsym(vgz_hz, "gzclose");
        if (!gzopen_fn || !gzread_fn || !gzclose_fn) {
            fprintf(stderr, "vgz: missing zlib symbols\n"); return 0;
        }
    }
    f = gzopen_fn(path, "rb");
    if (!f) { fprintf(stderr, "vgz: gzopen fail\n"); return 0; }
    /* 先试读 64KB 估大小, 再按需增长 */
    vgm_size = 0;
    long cap = 1 << 20;   /* 1MB 起步 */
    unsigned char *buf = (unsigned char *)malloc(cap);
    if (!buf) { gzclose_fn(f); return 0; }
    for (;;) {
        if (vgm_size + 65536 > cap) {
            cap *= 2;
            unsigned char *nb = (unsigned char *)realloc(buf, cap);
            if (!nb) { free(buf); gzclose_fn(f); return 0; }
            buf = nb;
        }
        int r = gzread_fn(f, buf + vgm_size, 65536);
        if (r <= 0) break;
        vgm_size += r;
    }
    gzclose_fn(f);
    if (vgm_size == 0) { free(buf); return 0; }
    if (vgm_data) free(vgm_data);
    vgm_data = buf;
    return 1;
}

/* ================= VGM 载入 ================= */
static int vgm_load_file(const char *path) {
    FILE *f;
    long sz;
    unsigned char *buf;
    f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 0x40) { fclose(f); return 0; }
    buf = (unsigned char *)malloc(sz + 1);
    if (!buf) { fclose(f); return 0; }
    if (fread(buf, 1, sz, f) != (size_t)sz) { free(buf); fclose(f); return 0; }
    fclose(f);

    if (vgm_data) free(vgm_data);
    vgm_data = buf;
    vgm_size = sz;

    /* VGZ = gzip magic */
    if (sz >= 2 && buf[0] == 0x1f && buf[1] == 0x8b) {
        if (!vgz_load(path)) { /* 保留原样, 大概率解析失败 */ }
    }
    return 1;
}

/* 小端读 */
static unsigned long le32(const unsigned char *d, long ofs) {
    return (unsigned long)d[ofs] | ((unsigned long)d[ofs + 1] << 8) |
           ((unsigned long)d[ofs + 2] << 16) | ((unsigned long)d[ofs + 3] << 24);
}
static unsigned long le16(const unsigned char *d, long ofs) {
    return (unsigned long)d[ofs] | ((unsigned long)d[ofs + 1] << 8);
}

/* ================= 解析 header =================
 * VGM 所有偏移字段是"相对该字段位置"的偏移 (libvgm ReadRelOfs):
 *   实际绝对位置 = 字段所在字节位置 + 字段值
 * eofOfs@0x04 -> eof 绝对位置 = 0x04 + le32(0x04)
 * gd3Ofs@0x14 -> gd3 绝对位置 = 0x14 + le32(0x14)
 * loopOfs@0x1C-> loop 绝对位置 = 0x1C + le32(0x1C)
 * dataOfs@0x34-> data 绝对位置 = 0x34 + le32(0x34)  (v1.50+, 默认 0x40) */
static int vgm_parse_header(void) {
    const unsigned char *d = vgm_data;
    long eofabs, gd3abs, dataofs;
    if (vgm_size < 0x40) return 0;
    if (memcmp(d, "Vgm ", 4) != 0 && memcmp(d, "Ggm ", 4) != 0) return 0;

    vgm_filever = le32(d, 0x08);
    vgm_numticks = (long)le32(d, 0x18);
    vgm_loopticks= (long)le32(d, 0x20);

    /* eofOfs@0x04: 相对偏移, 绝对位置 = 0x04 + 值 (libvgm ReadRelOfs) */
    eofabs = le32(d, 0x04) ? 0x04L + (long)le32(d, 0x04) : vgm_size;
    /* gd3Ofs@0x14: 相对偏移 */
    gd3abs = le32(d, 0x14) ? 0x14L + (long)le32(d, 0x14) : 0;
    /* loopOfs@0x1C: 相对偏移 */
    vgm_loopofs = le32(d, 0x1C) ? 0x1CL + (long)le32(d, 0x1C) : 0;

    /* v1.50+: dataOfs@0x34 相对偏移, 默认 0x40, 最小 0x38 */
    dataofs = 0x40;
    if (vgm_filever >= 0x150 && vgm_size >= 0x38) {
        unsigned long raw = le32(d, 0x34);
        dataofs = raw ? 0x34L + (long)raw : 0x40;
    }
    if (dataofs < 0x38) dataofs = 0x40;
    if (dataofs > vgm_size) dataofs = vgm_size;

    /* AY 时钟: v1.50 @0x40, v1.70 @0x74, 双偏移读非零者 */
    vgm_ay_clock = 0;
    if (vgm_filever >= 0x150 && vgm_size >= 0x44) {
        unsigned long c = le32(d, 0x40);
        if (c) vgm_ay_clock = (int)c;
    }
    if (vgm_filever >= 0x170 && vgm_size >= 0x78) {
        unsigned long c = le32(d, 0x74);
        if (c) vgm_ay_clock = (int)c;
    }
    if (!vgm_ay_clock) vgm_ay_clock = 1789772; /* 默认 NTSC */

    /* chipFlags @0x79: bit4(0x10)=YM2149 /2 分频 */
    vgm_ym2149_div2 = 0;
    if (vgm_size >= 0x7A) {
        if (d[0x79] & 0x10) vgm_ym2149_div2 = 1;
    }

    vgm_dataofs = dataofs;
    vgm_dataend = eofabs;
    if (vgm_dataend > vgm_size) vgm_dataend = vgm_size;
    if (vgm_dataend < dataofs) vgm_dataend = vgm_size;
    if (vgm_loopofs < dataofs || vgm_loopofs >= vgm_dataend) vgm_loopofs = 0;

    gd3_read(d, gd3abs); /* gd3abs 已是绝对位置 */
    return 1;
}

/* ================= 播放状态 ================= */
static int vgm_playing = 0;
static int vgm_paused = 0;
static unsigned long vgm_play_start = 0;   /* imrun_now 秒 */
static float vgm_progress = 0.0f;          /* 0..1 */
static float vgm_vol[3] = { 1.0f, 1.0f, 1.0f }; /* AY 三通道音量条 */
static int vgm_need_reload = 0;

/* 重置到曲首 */
static void vgm_reset_stream(void) {
    vgm_filepos = vgm_dataofs;
    vgm_tick = 0;
    vgm_eof = 0;
    vgm_loops_done = 0;
    ay_init();
    if (vgm_ym2149_div2 && vgm_ay_clock > 0)
        ay_set_clock(vgm_ay_clock / 2);
    else
        ay_set_clock(vgm_ay_clock);
}

/* ================= 渲染核心 (主程序 writer 线程回调) =================
 * 模型 (对齐 libvgm): 先处理命令流直到累计到 pending 个等待采样,
 * 再逐采样递减 pending 并各渲染一个输出. wait 命令设 pending=N,
 * 寄存器写命令不消耗 pending. pending 耗尽后继续处理命令.
 * 返回 1=继续, 0=EOF/停止 */
static void vgm_advance_cmd(void) {
    /* 处理命令直到遇到一个非零 wait (设好 pending) 或 EOF.
     * 调用前保证 pending==0 且未到 EOF. */
    for (;;) {
        if (vgm_filepos >= vgm_dataend) { vgm_eof = 1; return; }
        unsigned char cmd = vgm_data[vgm_filepos];
        if (cmd == 0x66) { vgm_eof = 1; return; }                 /* end */
        if (cmd == 0x61) {                                        /* wait N (2B LE) */
            unsigned long n = vgm_data[vgm_filepos + 1] | ((unsigned long)vgm_data[vgm_filepos + 2] << 8);
            vgm_filepos += 3;
            vgm_pending = (long)n;
            return;
        }
        if (cmd == 0x62) { vgm_filepos += 1; vgm_pending = 735; return; }
        if (cmd == 0x63) { vgm_filepos += 1; vgm_pending = 882; return; }
        if (cmd >= 0x70 && cmd <= 0x7F) { vgm_filepos += 1; vgm_pending = 1 + (cmd & 0xF); return; }
        if (cmd == 0x67) { /* data block: 精确跳 */ if (vgm_filepos + 10 > vgm_dataend) { vgm_eof = 1; return; }
                           unsigned long bs = le32(vgm_data, vgm_filepos + 6); vgm_filepos += 10 + (long)bs; continue; }
        if (cmd == 0x68) { vgm_filepos += 12; continue; }         /* PCM RAM write */
        if (cmd == 0x31) {                                        /* AY8910 stereo mask */
            /* byte1 = stereo mask: bit0-1=ch0(L/R), bit2-3=ch1, bit4-5=ch2
             * 每通道 2-bit: 00=静音, 01=R only, 10=L only, 11=L+R(中央) */
            unsigned char mask = vgm_data[vgm_filepos + 1];
            vgm_filepos += 2;
            ay_set_stereo_mask(0, (mask >> 0) & 0x03);  /* ch0: bit0-1 */
            ay_set_stereo_mask(1, (mask >> 2) & 0x03);  /* ch1: bit2-3 */
            ay_set_stereo_mask(2, (mask >> 4) & 0x03);  /* ch2: bit4-5 */
            continue;
        }
        if (cmd == 0xA0) {                                        /* AY8910 write */
            unsigned char reg = vgm_data[vgm_filepos + 1];
            unsigned char dat = vgm_data[vgm_filepos + 2];
            vgm_filepos += 3;
            ay_wr(reg & 0x7F, dat);                               /* 只用 chip0 */
            continue;
        }
        {   /* 其余芯片命令: 按全量命令表长度精确跳 */
            unsigned char clen = VGM_CMD_LEN[cmd];
            if (clen == 0) { vgm_eof = 1; return; }               /* 非法 -> 保守 EOF */
            vgm_filepos += clen;
            continue;
        }
    }
}

extern "C" int vgm_render(short *dst, int frames) {
    int i;
    if (!vgm_loaded || !vgm_playing) return 0;
    if (vgm_eof) return 0;
    
    /* CRITICAL: 第一次渲染时设置采样率 (此时 audio_rate 已被 audio_start 设置)
     * 不能在 vgm_start 设置,因为那时 audio_rate 还是旧值 (44100).
     * audio_start (PULSE 分支) 会设 audio_rate=48000, 但在 vgm_start 之后. */
    if (!vgm_rate_set) {
        ay_set_sample_rate(audio_rate);
        vgm_rate_set = 1;
    }

    for (i = 0; i < frames; i++) {
        if (vgm_pending <= 0) {
            if (vgm_eof) break;
            vgm_advance_cmd();
            if (vgm_eof && vgm_pending <= 0) {
                /* 循环 */
                if (vgm_loop_enable && vgm_loopofs > 0 && vgm_loops_done < 100) {
                    vgm_filepos = vgm_loopofs;
                    vgm_tick = 0;
                    vgm_eof = 0;
                    vgm_loops_done++;
                    vgm_play_start = (unsigned long)imrun_now();
                    vgm_advance_cmd();
                } else {
                    vgm_playing = 0;
                    return 0;
                }
            }
        }
        if (vgm_eof && vgm_pending <= 0) { break; }
        /* 渲染一个采样: AY 三通道分离输出, 按 vgm_vol[] 和 stereo_mask 加权 */
        s16 ch[3];
        ay_render3(ch);
        
        /* 应用通道音量 + 立体声混合 */
        long L = 0, R = 0;
        for (int c = 0; c < 3; c++) {
            long sample = (long)ch[c] * (long)(vgm_vol[c] * 256.0f);
            sample >>= 8;  /* 恢复原始幅度 */
            /* stereo_mask: bit0=L, bit1=R (在 ay8910_v3.c 里设置) */
            extern u8 ay_stereo_mask[3];
            if (ay_stereo_mask[c] & 0x01) L += sample;
            if (ay_stereo_mask[c] & 0x02) R += sample;
        }
        
        /* 总音量缩放 */
        long vol_scale = 1600L * (long)audio_volume / 100L;
        long vL = L * vol_scale;
        long vR = R * vol_scale;
        if (vL > 32767) vL = 32767; if (vL < -32768) vL = -32768;
        if (vR > 32767) vR = 32767; if (vR < -32768) vR = -32768;
        dst[i * 2] = (short)vL;      /* L */
        dst[i * 2 + 1] = (short)vR;  /* R */
        vgm_pending--;
        if (vgm_pending < 0) vgm_pending = 0;
        vgm_tick++;
        /* 不因 tick>=numTicks 提前停: 循环文件 tick 会超, 靠 end/dataend(0x66) 终止 */
    }
    vgm_progress = vgm_numticks > 0 ? (float)vgm_tick / (float)vgm_numticks : 0.0f;
    if (vgm_progress > 1.0f) vgm_progress = 1.0f;
    return vgm_eof && vgm_pending <= 0 ? 0 : 1;
}

/* ================= 文件列表 ================= */
#define MAX_FILES 128
static char file_list[MAX_FILES][512];
static int file_count = 0;
static int file_sel = -1;

static void vgm_scan_dir(const char *dir) {
    DIR *dp;
    struct dirent *ep;
    file_count = 0;
    dp = opendir(dir);
    if (!dp) return;
    while ((ep = readdir(dp)) != NULL && file_count < MAX_FILES) {
        const char *n = ep->d_name;
        size_t len = strlen(n);
        if (len > 4 && (strcmp(n + len - 4, ".vgm") == 0 || strcmp(n + len - 4, ".vgz") == 0 ||
                        strcmp(n + len - 4, ".VGM") == 0 || strcmp(n + len - 4, ".VGZ") == 0))
            strncpy(file_list[file_count++], n, 511);
    }
    closedir(dp);
}

/* ================= UI 入口 ================= */
extern "C" const char *imapp_name(void) { return "VGM"; }

extern "C" void imapp_destroy(void) {
    /* CRITICAL: 摘钩顺序很重要
     * 1. 先停播放 (audio_ui_stop 会 join writer 线程)
     * 2. 再摘 audio_source_fn (此时 writer 已退出,不会再调 vgm_render)
     * 旧顺序 (先摘钩再停) 有竞态: writer 可能在 audio_source_fn=0 后仍调用 */
    vgm_playing = 0;  /* 让 vgm_render 立即返回 0 (EOF) */
    audio_ui_stop();  /* join writer 线程 */
    audio_source_fn = 0;  /* writer 已退出,安全摘钩 */
    if (vgm_data) { free(vgm_data); vgm_data = NULL; }
    vgm_loaded = 0;
}

static int vgm_start(const char *path) {
    char full[640];
    snprintf(full, sizeof(full), "/home/user/MyDocs/VGM/%s", path);
    if (!vgm_load_file(full)) return 0;
    if (!vgm_parse_header()) return 0;
    vgm_reset_stream();
    vgm_loaded = 1;
    vgm_playing = 1;
    vgm_paused = 0;
    vgm_play_start = (unsigned long)imrun_now();
    vgm_pending = 0;
    vgm_rate_set = 0;  /* 重置标志,让 vgm_render 第一次调用时重新设采样率 */
    
    audio_source_fn = vgm_render;
    audio_ui_start(vgm_numticks / 44100 + 2);  /* 时长计算暂用 44100 (粗略估算) */
    return 1;
}

extern "C" void imapp_frame(double t) {
    (void)t;
    if (ImGui::BeginTabBar("vgmtabs")) {
        if (ImGui::BeginTabItem("PLAYLIST")) {
            ImGui::TextUnformatted("/home/user/MyDocs/VGM");
            vgm_scan_dir("/home/user/MyDocs/VGM");
            for (int i = 0; i < file_count; i++) {
                bool sel = (i == file_sel);
                if (ImGui::Selectable(file_list[i], sel)) {
                    file_sel = i;
                    audio_ui_stop();
                    audio_source_fn = 0;
                    vgm_start(file_list[i]);
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("NOW PLAYING")) {
            if (vgm_loaded && vgm_playing) {
                ImGui::Text("TRACK: %s", vgm_gd3_title[0] ? vgm_gd3_title : (file_sel >= 0 ? file_list[file_sel] : "?"));
                ImGui::Text("TICKS: %ld / %ld", vgm_tick, vgm_numticks);
                ImGui::ProgressBar(vgm_progress, ImVec2(0, 0));
                ImGui::Separator();
                if (ImGui::Button("STOP", ImVec2(140, 48))) {
                    audio_ui_stop();
                    vgm_playing = 0;
                }
                ImGui::SameLine();
                if (ImGui::Button("RESTART", ImVec2(140, 48)))
                    vgm_reset_stream();
                ImGui::Separator();
                ImGui::TextUnformatted("AY8910 CHANNEL SETTINGS");
                
                static const char* pan_labels[] = {"MUTE", "R", "L", "M"};
                static int pan_ch0 = 3, pan_ch1 = 3, pan_ch2 = 3;  /* 默认 M(中央) */
                
                /* CH0: 音量 + 立体声同行 */
                ImGui::SliderFloat("CH0 VOL", &vgm_vol[0], 0.0f, 1.0f, "%.2f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(80);
                if (ImGui::Combo("##pan0", &pan_ch0, pan_labels, 4)) 
                    ay_set_stereo_mask(0, pan_ch0);
                
                /* CH1: 音量 + 立体声同行 */
                ImGui::SliderFloat("CH1 VOL", &vgm_vol[1], 0.0f, 1.0f, "%.2f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(80);
                if (ImGui::Combo("##pan1", &pan_ch1, pan_labels, 4)) 
                    ay_set_stereo_mask(1, pan_ch1);
                
                /* CH2: 音量 + 立体声同行 */
                ImGui::SliderFloat("CH2 VOL", &vgm_vol[2], 0.0f, 1.0f, "%.2f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(80);
                if (ImGui::Combo("##pan2", &pan_ch2, pan_labels, 4)) 
                    ay_set_stereo_mask(2, pan_ch2);
                
                ImGui::Separator();
                ImGui::Text("FILEVER: 0x%lx  AYCLK: %d  YM2149/2: %s",
                            vgm_filever, vgm_ay_clock, vgm_ym2149_div2 ? "YES" : "no");
            } else {
                ImGui::TextUnformatted("NO TRACK LOADED");
                ImGui::TextUnformatted("pick a file in PLAYLIST");
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

extern "C" void imapp_icon(void *dlv, float cx, float cy, float s) {
    ImDrawList *dl = (ImDrawList *)dlv;
    ImU32 ac = IM_COL32(255, 200, 60, 255);
    ImVec2 c = ImVec2(cx, cy);
    dl->AddCircleFilled(c, s, IM_COL32(60, 60, 70, 255), 24);
    dl->AddTriangleFilled(ImVec2(cx - s * 0.35f, cy - s * 0.45f),
                          ImVec2(cx + s * 0.5f, cy),
                          ImVec2(cx - s * 0.35f, cy + s * 0.45f), ac);
}
