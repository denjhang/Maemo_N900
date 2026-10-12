#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
extern void _start(void) __attribute__((noreturn));
__asm__(".global _start\n"
        "_start:\n"
        "    bl main\n"
        "    bl exit\n"
        "    b .\n");
int main(void) {
    snd_pcm_t *h = NULL;
    int err;
    printf("open default...\n");
    err = snd_pcm_open(&h, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) { printf("OPEN FAIL: %s\n", snd_strerror(err)); return 1; }
    printf("open OK\n");
    snd_pcm_hw_params_t *p = NULL;
    snd_pcm_hw_params_alloca(&p);
    snd_pcm_hw_params_any(h, p);
    err = snd_pcm_hw_params_set_access(h, p, SND_PCM_ACCESS_RW_INTERLEAVED);
    printf("set_access(RW): %d %s\n", err, err<0?snd_strerror(err):"");
    if (err < 0) { err = snd_pcm_hw_params_set_access(h, p, SND_PCM_ACCESS_MMAP_INTERLEAVED); printf("set_access(MMAP): %d\n", err); }
    unsigned int rate = 22050;
    err = snd_pcm_hw_params_set_format(h, p, SND_PCM_FORMAT_S16_LE);
    printf("set_format: %d\n", err);
    err = snd_pcm_hw_params_set_channels(h, p, 1);
    printf("set_channels: %d\n", err);
    err = snd_pcm_hw_params_set_rate_near(h, p, &rate, 0);
    printf("set_rate_near: %d -> %u\n", err, rate);
    err = snd_pcm_hw_params(h, p);
    printf("apply: %d %s\n", err, err<0?snd_strerror(err):"");
    snd_pcm_close(h);
    return 0;
}
