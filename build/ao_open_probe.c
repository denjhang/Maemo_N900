#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ao/ao.h>

static void on_alarm(int s) {
    fprintf(stderr, "[alarm] timed out, aborting\n");
    _exit(2);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    signal(SIGALRM, on_alarm);
    alarm(30);
    fprintf(stderr, "[t] start, argc=%d\n", argc);
    ao_initialize();
    fprintf(stderr, "[t] ao_initialize done\n");

    int def = ao_default_driver_id();
    fprintf(stderr, "[t] default driver id = %d\n", def);
    if (def >= 0) {
        ao_info *inf = ao_driver_info(def);
        fprintf(stderr, "[t] default driver: name='%s' short='%s' type=%d prio=%d\n",
            inf->name, inf->short_name, inf->type, inf->priority);
    }

    ao_sample_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.bits = 16;
    fmt.rate = 44100;
    fmt.channels = 2;
    fmt.byte_format = AO_FMT_NATIVE;

    ao_option *opt = NULL;
    if (argc > 1) {
        fprintf(stderr, "[t] using dev='%s'\n", argv[1]);
        ao_append_option(&opt, "dev", argv[1]);
    }
    if (argc > 2) {
        fprintf(stderr, "[t] using buffer_time=%s\n", argv[2]);
        ao_append_option(&opt, "buffer_time", argv[2]);
    }

    fprintf(stderr, "[t] calling ao_open_live ...\n");
    ao_device *dev = ao_open_live(def, &fmt, opt);
    fprintf(stderr, "[t] ao_open_live returned: %p\n", (void*)dev);
    if (!dev) {
        fprintf(stderr, "[t] FAILED to open live device\n");
        ao_free_options(opt);
        ao_shutdown();
        return 1;
    }

    fprintf(stderr, "[t] device opened. trying 1KB play...\n");
    /* 1KB of zeros: 1024/4 = 256 stereo frames ~ 5.8ms @44100 */
    char buf[1024];
    memset(buf, 0, sizeof(buf));
    int r = ao_play(dev, buf, sizeof(buf));
    fprintf(stderr, "[t] ao_play(1024) = %d\n", r);

    fprintf(stderr, "[t] sleeping 200ms then close\n");
    usleep(200*1000);
    ao_close(dev);
    ao_free_options(opt);
    ao_shutdown();
    fprintf(stderr, "[t] clean exit\n");
    return 0;
}
