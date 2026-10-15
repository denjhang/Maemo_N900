/* probe_d: full libao 0.8.8 + EXPLICIT alsa09 driver id + dev=plughw:0,0.
   This sidesteps ao_default_driver_id()'s test() (which opens "default"
   -> pulse and fails under user). This is the DECISIVE check: does the
   full libao + alsa09 + plughw:0,0 path produce sound on this device? */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <ao/ao.h>

extern void _start(void) __attribute__((noreturn));
__asm__(".global _start\n"
        "_start:\n"
        "    bl main\n"
        "    bl exit\n"
        "    b .\n");

static void on_alarm(int s) {
    (void)s; fprintf(stderr, "[d] ALARM, aborting\n"); _exit(2);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    signal(SIGALRM, on_alarm);
    alarm(8);
    fprintf(stderr, "[d] start: full libao + alsa09 + dev=plughw:0,0\n");
    ao_initialize();

    int ndrv = 0;
    ao_info **list = ao_driver_info_list(&ndrv);
    fprintf(stderr, "[d] total drivers = %d\n", ndrv);
    for (int i = 0; i < ndrv; i++) {
        fprintf(stderr, "[d]   driver[%d] = '%s' (short '%s') type=%d prio=%d\n",
                i, list[i]->name, list[i]->short_name, list[i]->type, list[i]->priority);
    }
    int id = ao_driver_id("alsa");
    if (id < 0) id = ao_driver_id("alsa09");
    fprintf(stderr, "[d] ao_driver_id(alsa/alsa09) = %d\n", id);
    if (id < 0) {
        /* fall back to the first LIVE driver (driver[0] = alsa here) */
        for (int i = 0; i < ndrv; i++)
            if (list[i]->type == AO_TYPE_LIVE) { id = i; break; }
        fprintf(stderr, "[d] fallback first LIVE driver id = %d\n", id);
    }
    if (id < 0) { fprintf(stderr, "[d] no live driver\n"); ao_shutdown(); return 1; }

    ao_sample_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.bits = 16; fmt.rate = 44100; fmt.channels = 2;
    fmt.byte_format = AO_FMT_NATIVE;

    ao_option *opt = NULL;
    ao_append_option(&opt, "dev", "plughw:0,0");
    fprintf(stderr, "[d] ao_open_live(id=%d, dev=plughw:0,0) ...\n", id);
    ao_device *dev = ao_open_live(id, &fmt, opt);
    fprintf(stderr, "[d] ao_open_live returned: %p\n", (void*)dev);
    if (!dev) {
        fprintf(stderr, "[d] OPEN FAIL\n");
        ao_free_options(opt); ao_shutdown(); return 1;
    }

    /* ~2s tone, small amp */
    const int frames = 44100 * 2;
    const int chunk = 1024;
    short *buf = calloc(chunk * 2, sizeof(short));
    if (!buf) { fprintf(stderr, "[d] OOM\n"); return 1; }
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
            fprintf(stderr, "[d] ao_play FAIL at total=%d\n", total);
            break;
        }
        total += chunk;
    }
    fprintf(stderr, "[d] wrote %d frames (%.2fs)\n", total, total/44100.0);
    ao_close(dev);
    ao_free_options(opt);
    ao_shutdown();
    fprintf(stderr, "[d] clean exit\n");
    return 0;
}