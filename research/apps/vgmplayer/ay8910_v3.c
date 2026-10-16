/* AY-3-8910 / YM2149 PSG 核心 v3 - libvgm 对齐版
 * 基于 libvgm 原版修复噪声生成器 + 改进音量表
 * 保持精简版的性能（227 行 → ~300 行），只修复关键问题
 */
#include <string.h>

#define AY_CLK 1789772
#define AY_CHANS 3
#define AY_GETA_BITS 24

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;
typedef short s16;

static u8  ay_reg[16];
static u16 ay_count[AY_CHANS];

static u8  ay_freq_lo[AY_CHANS];
static u8  ay_freq_hi[AY_CHANS];
static u8  ay_edge[AY_CHANS];
static u8  ay_tmask[AY_CHANS];
static u8  ay_nmask[AY_CHANS];
static u16 ay_env_freq;
static u32 ay_env_count;
static u8  ay_env_step;
static u8  ay_env_attack;
static u8  ay_env_continue, ay_env_alternate, ay_env_hold, ay_env_pause;
static u32 ay_noise_rng;  /* 改名: seed -> rng, 对齐 libvgm */
static u8  ay_noise_count;
static u8  ay_noise_freq;
static u8  ay_volume[AY_CHANS];
static u32 ay_base_count;
static u32 ay_base_incr;
static u32 ay_sample_rate = 44100;
u8 ay_stereo_mask[AY_CHANS] = {0x03, 0x03, 0x03};  /* 每通道 L/R: bit0=L, bit1=R. 默认 0x03=L+R (中央). 非 static 供 vgm.cpp 访问 */

/* 音量表 v3: 改进精度（libvgm 用电阻网络计算，这里用近似指数曲线）
 * 原版 voltbl: 线性分段，音量差异不够平滑
 * v3: exp(i/2-7.5) 近似（pcmenc 风格），更接近真实 AY 芯片 */
static const u8 ay_voltbl_v3[32] = {
    /* 0-15: 固定音量档（每档重复一次以对齐 libvgm env_volume 5-bit） */
    0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x04,  
    0x05, 0x06, 0x08, 0x0A, 0x0E, 0x11, 0x15, 0x1C,
    0x22, 0x29, 0x33, 0x3F, 0x4E, 0x5D, 0x73, 0x8A,
    0xA6, 0xC3, 0xE6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static const u8 ay_regmsk[16] = {
    0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0x1f, 0x3f,
    0x1f, 0x1f, 0x1f, 0xff, 0xff, 0x0f, 0xff, 0xff
};

/* 前向声明 */
void ay_set_clock(u32 clock_hz);

void ay_init(void) {
    int i;
    for (i = 0; i < 16; i++) ay_reg[i] = 0;
    for (i = 0; i < AY_CHANS; i++) {
        ay_count[i] = 0; ay_freq_lo[i] = 0; ay_freq_hi[i] = 0;
        ay_edge[i] = 0;  ay_tmask[i] = 0;   ay_nmask[i] = 0;
        ay_volume[i] = 0;
    }
    ay_env_freq = 0; ay_env_count = 0; ay_env_step = 0; ay_env_attack = 0;
    ay_env_continue = 0; ay_env_alternate = 0; ay_env_hold = 0; ay_env_pause = 0;
    ay_noise_rng = 1;  /* libvgm 初始值 1, 不是 0 */
    ay_noise_count = 0; ay_noise_freq = 0;
    ay_base_count = 0;
    ay_set_clock(AY_CLK);
}

void ay_set_clock(u32 clock_hz) {
    u64 incr = (u64)clock_hz * (1UL << AY_GETA_BITS) / (8UL * ay_sample_rate);
    ay_base_incr = (u32)incr;
}

void ay_set_sample_rate(u32 rate) {
    ay_sample_rate = rate;
    ay_set_clock(AY_CLK);
}

/* 设置通道立体声位置
 * ch: 0-2 (通道编号)
 * mask: bit0=L, bit1=R. 0x01=L, 0x02=R, 0x03=L+R(中央), 0x00=静音 */
void ay_set_stereo_mask(u8 ch, u8 mask) {
    if (ch < AY_CHANS)
        ay_stereo_mask[ch] = mask & 0x03;
}

void ay_wr(u8 reg, u8 val) {
    u8 c;
    u16 freq;

    if (reg > 15) return;
    val &= ay_regmsk[reg];
    ay_reg[reg] = val;

    switch (reg) {
    case 0: case 2: case 4:
    case 1: case 3: case 5:
        c = reg >> 1;
        freq = ((u16)ay_reg[c * 2 + 1] & 0x0F) << 8;
        freq |= ay_reg[c * 2];
        ay_freq_lo[c] = freq & 0xFF;
        ay_freq_hi[c] = (freq >> 8) & 0x0F;
        break;
    case 6:
        ay_noise_freq = val & 0x1F;
        break;
    case 7:
        for (c = 0; c < AY_CHANS; c++) {
            ay_tmask[c] = (val >> c) & 1;
            ay_nmask[c] = (val >> (c + 3)) & 1;
        }
        break;
    case 8: case 9: case 10:
        c = reg - 8;
        ay_volume[c] = val & 0x1F;
        break;
    case 11:
        ay_env_freq = ((u16)ay_reg[12] << 8) | val;
        break;
    case 12:
        ay_env_freq = ((u16)val << 8) | ay_reg[11];
        break;
    case 13:
        /* envelope shape: bit3=continue, bit2=attack, bit1=alternate, bit0=hold
         * env_step 初始 0x0F, attack 决定 XOR 方向 */
        ay_env_continue = (val >> 3) & 1;
        ay_env_attack   = (val & 0x04) ? 0x0F : 0x00;
        ay_env_alternate= (val >> 1) & 1;
        ay_env_hold     = val & 1;
        ay_env_pause    = 0;
        ay_env_step     = 0x0F;
        break;
    }
}

/* 内部: 推进计数器 + 生成三通道输出
 * v3 修复: 噪声 LFSR 用 libvgm 标准实现 */
static void ay_advance(u16 ch_out[AY_CHANS]) {
    u8 incr, noise_out;
    u16 freq;
    int c;

    ay_base_count += ay_base_incr;
    incr = (u8)(ay_base_count >> AY_GETA_BITS);
    ay_base_count &= (1UL << AY_GETA_BITS) - 1;

    if (incr > 0) {
        /* 包络推进 */
        ay_env_count += incr;
        if (ay_env_freq > 0 && ay_env_count >= ay_env_freq) {
            if (!ay_env_pause) {
                ay_env_step--;
            }
            if (ay_env_step == 0xFF) {  /* 下溢 (0 -> 0xFF) */
                if (ay_env_hold) {
                    if (ay_env_alternate) ay_env_attack ^= 0x0F;
                    ay_env_pause = 1;
                    ay_env_step = 0;
                } else {
                    if (ay_env_alternate && (ay_env_step & 0x10))
                        ay_env_attack ^= 0x0F;
                    ay_env_step = 0x0F;
                }
            }
            if (ay_env_freq >= incr)
                ay_env_count -= ay_env_freq;
            else
                ay_env_count = 0;
        }

        /* 噪声推进 - libvgm 标准 17-bit LFSR
         * "The Random Number Generator of the 8910 is a 17-bit shift register.
         *  The input to the shift register is bit0 XOR bit3 (bit0 is the output).
         *  This was verified on AY-3-8910 and YM2149 chips."
         * 
         * CRITICAL: period=0 时应该每次更新 (>=0 永远为真) */
        ay_noise_count += incr;
        if (ay_noise_count >= ay_noise_freq) {
            /* XOR bit0 and bit3, put result at bit17, then shift right */
            ay_noise_rng ^= (((ay_noise_rng & 1) ^ ((ay_noise_rng >> 3) & 1)) << 17);
            ay_noise_rng >>= 1;
            
            if (ay_noise_freq >= incr)
                ay_noise_count -= ay_noise_freq;
            else
                ay_noise_count = 0;
        }
    }
    
    /* 噪声输出 = bit0 */
    noise_out = ay_noise_rng & 1;

    /* 三通道音调 + 混合 */
    for (c = 0; c < AY_CHANS; c++) {
        if (incr > 0) {
            freq = ((u16)ay_freq_hi[c] << 8) | ay_freq_lo[c];
            ay_count[c] += incr;
            /* libvgm: period=0 时每次都翻转 (>= 0 永远为真)
             * 这会产生最高频率方波 (采样率/2) */
            if (ay_count[c] >= freq) {
                ay_edge[c] = !ay_edge[c];
                if (freq >= incr) 
                    ay_count[c] -= freq; 
                else 
                    ay_count[c] = 0;
            }
        }
        
        ch_out[c] = 0;
        
        /* 混合逻辑 (libvgm 标准):
         * vol_enabled = (tone_output | tone_disabled) & (noise_output | noise_disabled)
         * 
         * CRITICAL: 当 tone 和 noise 都 disabled 时,输出=1 (不是0)
         *           可以通过音量调制 */
        u8 tone_on = ay_edge[c] | ay_tmask[c];  /* tone_output | tone_disabled */
        u8 noise_on = noise_out | ay_nmask[c];  /* noise_output | noise_disabled */
        
        if (tone_on && noise_on) {
            u8 vol_idx = ay_volume[c] & 0x0F;
            u8 vol_val;
            
            /* envelope mode: bit4=1 表示用包络 */
            if (ay_volume[c] & 0x10) {
                vol_idx = ay_env_step ^ ay_env_attack;
            }
            
            vol_val = ay_voltbl_v3[vol_idx];
            ch_out[c] = (u16)vol_val << 4;  /* << 4 to match 12-bit DAC range */
        }
    }
}

s16 ay_render(void) {
    u16 ch_out[AY_CHANS];
    s16 mix;
    ay_advance(ch_out);
    mix = 0;
    /* 单声道混合: 所有通道按 stereo_mask 加权（L+R 都算） */
    for (int c = 0; c < AY_CHANS; c++) {
        if (ay_stereo_mask[c])  /* 非 0x00 (非静音) */
            mix += (s16)((u16)ch_out[c] >> 4) - 8;
    }
    return mix;
}

void ay_render3(s16 out[3]) {
    u16 ch_out[AY_CHANS];
    int c;
    ay_advance(ch_out);
    for (c = 0; c < AY_CHANS; c++)
        out[c] = (s16)((u16)ch_out[c] >> 4) - 8;
}

/* 立体声渲染: 返回 L/R 分离
 * out_L, out_R: 输出左右声道 */
void ay_render_stereo(s16 *out_L, s16 *out_R) {
    u16 ch_out[AY_CHANS];
    s16 L = 0, R = 0;
    ay_advance(ch_out);
    for (int c = 0; c < AY_CHANS; c++) {
        s16 sample = (s16)((u16)ch_out[c] >> 4) - 8;
        if (ay_stereo_mask[c] & 0x01) L += sample;  /* bit0 = L */
        if (ay_stereo_mask[c] & 0x02) R += sample;  /* bit1 = R */
    }
    *out_L = L;
    *out_R = R;
}

u8 ay_channel_mask(void) {
    u8 mask = 0, i;
    for (i = 0; i < AY_CHANS; i++) {
        if (ay_volume[i]) mask |= (1 << i);
    }
    /* noise: reg7 任意通道 noise enabled */
    for (i = 0; i < AY_CHANS; i++) {
        if (!ay_nmask[i]) { mask |= 0x08; break; }
    }
    /* envelope: 任意通道使用包络 */
    for (i = 0; i < AY_CHANS; i++) {
        if (ay_volume[i] & 0x10) { mask |= 0x10; break; }
    }
    return mask;
}
