/* probe_sdl: standalone SDL 1.2 audio probe, runtime dlopen (no headers).
   Verifies SDL can produce sound via ALSA direct (bypass pulse) using the
   SDL_AUDIODRIVER=alsa + AUDIODEV=plughw:0,0 recipe. Callback generates a
   small-volume sine tone, self-terminates via alarm. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <dlfcn.h>

extern void _start(void) __attribute__((noreturn));
__asm__(".global _start\n"
        "_start:\n"
        "    bl main\n"
        "    bl exit\n"
        "    b .\n");

typedef struct { int freq; unsigned short format; unsigned char channels,
                 silence; unsigned short samples; unsigned int size;
                 void (*callback)(void *, unsigned char *, int);
                 void *userdata; } SDL_AudioSpec;
typedef struct { unsigned char dummy; } SDL_AudioSpec_obtained;

static void on_alarm(int s) {
    (void)s; fprintf(stderr, "[sdl] ALARM, aborting\n"); _exit(2);
}

static double phase = 0.0;
static int amp = 1500;
static volatile unsigned long cb_count = 0;
static volatile unsigned long cb_total_bytes = 0;
static void cb(void *ud, unsigned char *stream, int len) {
    (void)ud;
    cb_count++;
    cb_total_bytes += (unsigned long)len;
    short *s = (short *)stream;
    int n = len / 4; /* stereo 16-bit */
    for (int i = 0; i < n; i++) {
        float w = sinf(phase);
        short v = (short)(w * amp);
        s[i*2] = v; s[i*2+1] = v;
        phase += 2.0 * M_PI * 440.0 / 44100.0;
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    signal(SIGALRM, on_alarm);
    alarm(8);
    fprintf(stderr, "[sdl] start env: SDL_AUDIODRIVER=%s AUDIODEV=%s\n",
            getenv("SDL_AUDIODRIVER") ? getenv("SDL_AUDIODRIVER") : "(unset)",
            getenv("AUDIODEV") ? getenv("AUDIODEV") : "(unset)");

    void *h = dlopen("libSDL-1.2.so.0", 2 /*RTLD_NOW*/);
    if (!h) { fprintf(stderr, "[sdl] dlopen SDL FAIL\n"); return 1; }
    int (*Init)(unsigned) = dlsym(h, "SDL_Init");
    int (*Open)(SDL_AudioSpec*, void*) = dlsym(h, "SDL_OpenAudio");
    void (*Pause)(int) = dlsym(h, "SDL_PauseAudio");
    void (*Close)(void) = dlsym(h, "SDL_CloseAudio");
    void (*Delay)(unsigned) = dlsym(h, "SDL_Delay");
    const char *(*Err)(void) = dlsym(h, "SDL_GetError");
    if (!Init || !Open || !Pause || !Close || !Err) {
        fprintf(stderr, "[sdl] missing symbols\n"); return 1;
    }

    if (Init(0x10 /*SDL_INIT_AUDIO*/) < 0) {
        fprintf(stderr, "[sdl] SDL_Init FAIL: %s\n", Err());
        return 1;
    }
    fprintf(stderr, "[sdl] SDL_Init OK\n");

    SDL_AudioSpec fmt;
    memset(&fmt, 0, sizeof fmt);
    fmt.freq = 44100;
    fmt.format = 0x8010 /*AUDIO_S16SYS*/;
    fmt.channels = 2;
    fmt.samples = 2048;
    fmt.callback = cb;
    fmt.userdata = NULL;
    fprintf(stderr, "[sdl] calling SDL_OpenAudio(44100/S16/2ch/2048)...\n");
    if (Open(&fmt, NULL) < 0) {
        fprintf(stderr, "[sdl] SDL_OpenAudio FAIL: %s\n", Err());
        return 1;
    }
    fprintf(stderr, "[sdl] SDL_OpenAudio OK (obtained %dHz %dch)\n", fmt.freq, fmt.channels);
    Pause(0);
    fprintf(stderr, "[sdl] playing 3s tone...\n");
    if (Delay) Delay(3000);
    fprintf(stderr, "[sdl] cb invoked %lu times, %lu bytes (%.2fs of audio)\n",
            cb_count, cb_total_bytes, (double)cb_total_bytes / (44100.0*4.0));
    Pause(1);
    Close();
    fprintf(stderr, "[sdl] clean exit\n");
    return 0;
}