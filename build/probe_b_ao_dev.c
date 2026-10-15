/* probe_b: libao alsa09 with dev=plughw:0,0 (the vgmplay-style fix path).
   Expected: opens fast, plays ~2s tone, exits cleanly. If this works, the
   IMRUN AO backend fix is confirmed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <ao/ao.h>

/* -nostartfiles needs a hand-rolled _start to actually call main() */
extern void _start(void) __attribute__((noreturn));
__asm__(".global _start\n"
        "_start:\n"
        "    bl main\n"
        "    bl exit\n"
        "    b .\n");

static void on_alarm(int s) {
    (void)s; fprintf(stderr, "[b] ALARM, aborting\n"); _exit(2);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    signal(SIGALRM, on_alarm);
    alarm(8);
    fprintf(stderr, "[b] start, libao+alsa09 dev=plughw:0,0\n");
    ao_initialize();
    int def = ao_default_driver_id();
    fprintf(stderr, "[b] default driver id = %d\n", def);
    ao_sample_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.bits = 16; fmt.rate = 44100; fmt.channels = 2;
    fmt.byte_format = AO_FMT_NATIVE;

    ao_option *opt = NULL;
    ao_append_option(&opt, "dev", "plughw:0,0");
    fprintf(stderr, "[b] calling ao_open_live(alsa09 path) ...\n");
    ao_device *dev = ao_open_live(def, &fmt, opt);
    fprintf(stderr, "[b] ao_open_live returned: %p\n", (void*)dev);
    if (!dev) {
        fprintf(stderr, "[b] OPEN FAIL\n");
        ao_free_options(opt); ao_shutdown(); return 1;
    }
    /* ~2s tone, small amp */
    const int frames = 44100 * 2;
    const int chunk = 1024;
    short *buf = calloc(chunk * 2, sizeof(short));
    if (!buf) { fprintf(stderr, "[b] OOM\n"); return 1; }
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
        if (!ao_play(dev, (char *)buf, chunk * 2 * sizeof(short))) {
            fprintf(stderr, "[b] ao_play FAIL at total=%d\n", total);
            break;
        }
        total += chunk;
    }
    fprintf(stderr, "[b] wrote %d frames (%.2fs)\n", total, total/44100.0);
    ao_close(dev);
    ao_free_options(opt);
    ao_shutdown();
    fprintf(stderr, "[b] clean exit\n");
    return 0;
}
