/* probe_e: full 61KB libao + ao_default_driver_id() + dev=plughw:0,0.
   Answers: under full libao, does ao_default_driver_id() as user return a
   usable live driver, or does test() open "default"->pulse and fail back
   to -1 (forcing the IMRUN patch to use ao_driver_id("alsa") explicitly)? */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ao/ao.h>

extern void _start(void) __attribute__((noreturn));
__asm__(".global _start\n"
        "_start:\n"
        "    bl main\n"
        "    bl exit\n"
        "    b .\n");

static void on_alarm(int s) {
    (void)s; fprintf(stderr, "[e] ALARM, aborting\n"); _exit(2);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    signal(SIGALRM, on_alarm);
    alarm(8);
    fprintf(stderr, "[e] start: full libao + ao_default_driver_id + dev option\n");
    ao_initialize();
    int def = ao_default_driver_id();
    fprintf(stderr, "[e] ao_default_driver_id() = %d\n", def);
    if (def < 0) { fprintf(stderr, "[e] default driver NOT found (test() skipped/failed)\n"); ai_shutdown: ao_shutdown(); return 1; }

    ao_info *inf = ao_driver_info(def);
    fprintf(stderr, "[e] default driver = '%s' (short '%s') prio=%d\n",
            inf ? inf->name : "?", inf ? inf->short_name : "?", inf ? inf->priority : -1);

    ao_sample_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.bits = 16; fmt.rate = 44100; fmt.channels = 2;
    fmt.byte_format = AO_FMT_NATIVE;
    ao_option *opt = NULL;
    ao_append_option(&opt, "dev", "plughw:0,0");
    fprintf(stderr, "[e] ao_open_live(default, dev=plughw:0,0) ...\n");
    ao_device *dev = ao_open_live(def, &fmt, opt);
    fprintf(stderr, "[e] returned %p\n", (void*)dev);
    if (!dev) { ao_free_options(opt); goto ai_shutdown; }
    ao_close(dev);
    ao_free_options(opt);
    ao_shutdown();
    fprintf(stderr, "[e] clean exit\n");
    return 0;
}