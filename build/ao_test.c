#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ao/ao.h>
extern void _start(void) __attribute__((noreturn));
__asm__(".global _start\n"
        "_start:\n"
        "    bl main\n"
        "    bl exit\n"
        "    b .\n");
int main(void) {
    ao_initialize();
    int def = ao_default_driver_id();
    printf("default driver id: %d\n", def);
    if (def >= 0) {
        ao_info *inf = ao_driver_info(def);
        printf("default driver: %s (%s) short=%s\n", inf->name, inf->comment, inf->short_name);
    }
    int n = ao_driver_id("alsa09");
    printf("alsa09 driver id: %d\n", n);
    int cnt = 0;
    for (int i = 0; i < 20; i++) {
        ao_info *inf2 = ao_driver_info(i);
        if (!inf2) break;
        printf("driver[%d] = %s\n", i, inf2->short_name);
        cnt++;
    }
    printf("total drivers: %d\n", cnt);
    ao_shutdown();
    return 0;
}
