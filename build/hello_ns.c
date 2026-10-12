#include <stdlib.h>
#include <stdio.h>
#include <time.h>

extern void _start(void) __attribute__((noreturn));
__asm__(
".global _start\n"
"_start:\n"
"    bl main\n"
"    bl exit\n"
"    b .\n"
);

int main(void) {
    printf("Hello from MaemoBuild cross toolchain!\n");
    printf("time(): %ld\n", (long)time(NULL));
    printf("malloc: %s\n", malloc(1024) ? "OK" : "FAIL");
    return 0;
}
