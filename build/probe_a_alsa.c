/* probe_a: ALSA plughw:0,0 direct write (control group, expect audio out)
   Small volume, ~2s tone, must self-terminate. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <alsa/asoundlib.h>

/* -nostartfiles needs a hand-rolled _start to actually call main() */
extern void _start(void) __attribute__((noreturn));
__asm__(".global _start\n"
        "_start:\n"
        "    bl main\n"
        "    bl exit\n"
        "    b .\n");

static void on_alarm(int s) {
    (void)s; fprintf(stderr, "[a] ALARM, aborting\n"); _exit(2);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    signal(SIGALRM, on_alarm);
    alarm(8);
    fprintf(stderr, "[a] start, ALSA plughw:0,0 direct\n");
    snd_pcm_t *h = NULL;
    int err = snd_pcm_open(&h, "plughw:0,0", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) { fprintf(stderr, "[a] OPEN FAIL: %s\n", snd_strerror(err)); return 1; }
    fprintf(stderr, "[a] open OK\n");
    snd_pcm_hw_params_t *p; snd_pcm_hw_params_alloca(&p);
    snd_pcm_hw_params_any(h, p);
    snd_pcm_hw_params_set_access(h, p, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(h, p, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(h, p, 2);
    unsigned int rate = 44100;
    snd_pcm_hw_params_set_rate_near(h, p, &rate, 0);
    snd_pcm_uframes_t ps = 1024;
    int dir = 0;
    snd_pcm_hw_params_set_period_size_near(h, p, &ps, &dir);
    snd_pcm_hw_params(h, p);
    fprintf(stderr, "[a] hw params set, rate=%u\n", rate);

    /* 2s tone, small amp (~2500) */
    const int frames = 44100 * 2;
    const int chunk = 1024;
    short *buf = calloc(chunk * 2, sizeof(short));
    if (!buf) { fprintf(stderr, "[a] OOM\n"); return 1; }
    double ph = 0.0;
    int total = 0;
    while (total < frames) {
        for (int i = 0; i < chunk; i++) {
            float w = sinf(ph);
            short s = (short)(w * 2500.0f);
            buf[i*2]   = s;
            buf[i*2+1] = s;
            ph += 2.0 * M_PI * 440.0 / 44100.0;
        }
        long w = snd_pcm_writei(h, buf, chunk);
        if (w < 0) {
            fprintf(stderr, "[a] writei=%ld err=%s -> recover\n", w, snd_strerror(w));
            snd_pcm_recover(h, (int)w, 0);
            continue;
        }
        total += (int)w;
    }
    fprintf(stderr, "[a] wrote %d frames (%.2fs), state=%d\n", total, total/44100.0,
            snd_pcm_state(h));
    snd_pcm_drain(h);
    snd_pcm_close(h);
    free(buf);
    fprintf(stderr, "[a] clean exit\n");
    return 0;
}
