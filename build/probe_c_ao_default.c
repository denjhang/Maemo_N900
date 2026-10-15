/* probe_c: libao with NO option (default -> pulse -> cookie EACCES death).
   Expected to HANG in ao_open_live; alarm(10) kills it. The point of this
   probe is to nail shut the previous diagnosis: in system-mode pulse the
   "default" path is unwalkable for user (29999). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ao/ao.h>

/* -nostartfiles needs a hand-rolled _start to actually call main() */
extern void _start(void) __attribute__((noreturn));
__asm__(".global _start\n"
        "_start:\n"
        "    bl main\n"
        "    bl exit\n"
        "    b .\n");

static void on_alarm(int s) {
    (void)s;
    fprintf(stderr, "[c] ALARM 10s reached, confirming hang in ao_open_live\n");
    _exit(3);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    signal(SIGALRM, on_alarm);
    alarm(10);
    fprintf(stderr, "[c] start, libao default (no dev override)\n");
    ao_initialize();
    int def = ao_default_driver_id();
    fprintf(stderr, "[c] default driver id = %d\n", def);
    ao_sample_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.bits = 16; fmt.rate = 44100; fmt.channels = 2;
    fmt.byte_format = AO_FMT_NATIVE;

    fprintf(stderr, "[c] calling ao_open_live(default, NULL) ...\n");
    ao_device *dev = ao_open_live(def, &fmt, NULL);
    fprintf(stderr, "[c] ao_open_live returned: %p\n", (void*)dev);
    if (dev) {
        fprintf(stderr, "[c] (UNEXPECTED) opened with default - playing 1s then exit\n");
        char buf[1024]; memset(buf, 0, sizeof(buf));
        ao_play(dev, buf, sizeof(buf));
        sleep(1);
        ao_close(dev);
        ao_shutdown();
        return 0;
    }
    fprintf(stderr, "[c] OPEN FAIL (NULL returned) - this is also acceptable, "
                    "the alarm will fire if open_live hangs instead\n");
    ao_shutdown();
    return 0;
}
