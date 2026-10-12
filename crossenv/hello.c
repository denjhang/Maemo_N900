#include <stdio.h>
#include <stdlib.h>
#include <time.h>
int main(void) {
    printf("Hello from MaemoBuild cross toolchain!\n");
    printf("time(): %ld\n", (long)time(NULL));
    printf("malloc/free: %s\n", malloc(1024) ? "OK" : "FAIL");
    return 0;
}
