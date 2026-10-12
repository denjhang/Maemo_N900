/* gldemo6 - pure path: SDL_SetVideoMode(OPENGL) + device internal GLES patch.
 * NO libSDL_gles, NO direct EGL. Swap via SDL_GL_SwapBuffers.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <dlfcn.h>
#include <sys/time.h>

extern void _start(void) __attribute__((noreturn));
__asm__(".global _start\n"
        "_start:\n"
        "    bl main\n"
        "    bl exit\n"
        "    b .\n");

static double now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

static void *hSDL, *hGLES;
static int (*pSDL_Init)(unsigned);
static void *(*pSDL_SetVideoMode)(int, int, int, unsigned);
static void (*pSDL_GL_SwapBuffers)(void);
static void (*pglViewport)(int, int, int, int);
static void (*pglClearColor)(float, float, float, float);
static void (*pglClear)(unsigned);
static void (*pglMatrixMode)(unsigned);
static void (*pglLoadIdentity)(void);
static void (*pglFrustumf)(float, float, float, float, float, float);
static void (*pglTranslatef)(float, float, float);
static void (*pglRotatef)(float, float, float, float);
static void (*pglEnable)(unsigned);
static void (*pglVertexPointer)(int, unsigned, int, const void *);
static void (*pglColorPointer)(int, unsigned, int, const void *);
static void (*pglDrawArrays)(unsigned, int, int);
static void (*pglEnableClientState)(unsigned);

#define DLSYM(h, var, name) do { \
    *(void **)(&var) = dlsym(h, name); \
    if (!var) { printf("dlsym FAIL: %s\n", name); return 1; } \
} while (0)

#define SDL_INIT_VIDEO 0x20u
#define SDL_FULLSCREEN 0x80000000u
#define SDL_OPENGL 0x00000002u
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x100
#define GL_TRIANGLES 4
#define GL_QUADS 7
#define GL_PROJECTION 0x1701
#define GL_MODELVIEW 0x1700
#define GL_FLOAT 0x1406
#define GL_DEPTH_TEST 0x0B71
#define GL_VERTEX_ARRAY 0x8074
#define GL_COLOR_ARRAY 0x8076
typedef float GLfloat;

static double bench_triangles(double dur) {
    double t0 = now(), t;
    int frames = 0;
    float ang = 0;
    GLfloat tv[9], tc[9];
    while ((t = now()) - t0 < dur) {
        ang += 2.0f;
        pglClearColor(0.05f, 0.05f, 0.1f, 1);
        pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        pglMatrixMode(GL_MODELVIEW);
        pglLoadIdentity();
        pglTranslatef(0, 0, -3.5f);
        pglRotatef(ang, 0, 1, 0);
        pglRotatef(ang * 0.7f, 0, 0, 1);
        tv[0] = 0; tv[1] = 1; tv[2] = 0;
        tv[3] = -1; tv[4] = -1; tv[5] = 0;
        tv[6] = 1; tv[7] = -1; tv[8] = 0;
        tc[0] = 1; tc[1] = 0.3f; tc[2] = 0.2f;
        tc[3] = 0.2f; tc[4] = 1; tc[5] = 0.3f;
        tc[6] = 0.3f; tc[7] = 0.3f; tc[8] = 1;
        pglEnableClientState(GL_VERTEX_ARRAY);
        pglEnableClientState(GL_COLOR_ARRAY);
        pglVertexPointer(3, GL_FLOAT, 0, tv);
        pglColorPointer(3, GL_FLOAT, 0, tc);
        pglDrawArrays(GL_TRIANGLES, 0, 3);
        pSDL_GL_SwapBuffers();
        frames++;
    }
    printf("MODE triangles: %d frames in %.2fs = %.1f FPS\n",
           frames, now() - t0, frames / (now() - t0));
    return now() - t0;
}

#define NSPRITE 300
static double bench_sprites(double dur) {
    double t0 = now(), t;
    int frames = 0;
    float ang = 0;
    srand(1234);
    float px[NSPRITE], py[NSPRITE], ph[NSPRITE];
    GLfloat sv[NSPRITE * 12], sc[NSPRITE * 12];
    int i, v;
    for (i = 0; i < NSPRITE; i++) {
        px[i] = rand() % 2000 / 1000.0f - 1.0f;
        py[i] = rand() % 1200 / 1000.0f - 0.6f;
        ph[i] = rand() % 360 / 57.3f;
    }
    while ((t = now()) - t0 < dur) {
        ang += 1.5f;
        pglClearColor(0.02f, 0.02f, 0.06f, 1);
        pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        pglMatrixMode(GL_MODELVIEW);
        pglLoadIdentity();
        pglTranslatef(0, 0, -3.0f);
        for (i = 0; i < NSPRITE; i++) {
            float ca = ph[i] + ang * 0.3f;
            float s = 0.08f;
            float x = px[i], y = py[i] + 0.3f * sinf(t * 0.5f + ph[i]);
            for (v = 0; v < 4; v++) {
                float vx = (v == 0 || v == 3) ? -1.0f : 1.0f;
                float vy = (v < 2) ? 1.0f : -1.0f;
                float rx = x + vx * s * cosf(ca) - vy * s * sinf(ca);
                float ry = y + vx * s * sinf(ca) + vy * s * cosf(ca);
                sv[i * 12 + v * 3] = rx;
                sv[i * 12 + v * 3 + 1] = ry;
                sv[i * 12 + v * 3 + 2] = -1.0f;
                float c = 0.5f + 0.5f * sinf(ph[i] + ang * 0.1f + i);
                sc[i * 12 + v * 3] = c;
                sc[i * 12 + v * 3 + 1] = 1.0f - c;
                sc[i * 12 + v * 3 + 2] = 0.5f;
            }
        }
        pglEnableClientState(GL_VERTEX_ARRAY);
        pglEnableClientState(GL_COLOR_ARRAY);
        pglVertexPointer(3, GL_FLOAT, 0, sv);
        pglColorPointer(3, GL_FLOAT, 0, sc);
        pglDrawArrays(GL_QUADS, 0, NSPRITE * 4);
        pSDL_GL_SwapBuffers();
        frames++;
    }
    printf("MODE sprites(%d): %d frames in %.2fs = %.1f FPS\n",
           NSPRITE, frames, now() - t0, frames / (now() - t0));
    return now() - t0;
}

int main(void) {
    hSDL = dlopen("libSDL-1.2.so.0", RTLD_LAZY | RTLD_GLOBAL);
    if (!hSDL) { printf("dlopen SDL: %s\n", dlerror()); return 1; }
    hGLES = dlopen("libGLES_CM.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!hGLES) { printf("dlopen GLES: %s\n", dlerror()); return 1; }

    DLSYM(hSDL, pSDL_Init, "SDL_Init");
    DLSYM(hSDL, pSDL_SetVideoMode, "SDL_SetVideoMode");
    DLSYM(hSDL, pSDL_GL_SwapBuffers, "SDL_GL_SwapBuffers");
    DLSYM(hGLES, pglViewport, "glViewport");
    DLSYM(hGLES, pglClearColor, "glClearColor");
    DLSYM(hGLES, pglClear, "glClear");
    DLSYM(hGLES, pglMatrixMode, "glMatrixMode");
    DLSYM(hGLES, pglLoadIdentity, "glLoadIdentity");
    DLSYM(hGLES, pglFrustumf, "glFrustumf");
    DLSYM(hGLES, pglTranslatef, "glTranslatef");
    DLSYM(hGLES, pglRotatef, "glRotatef");
    DLSYM(hGLES, pglEnable, "glEnable");
    DLSYM(hGLES, pglVertexPointer, "glVertexPointer");
    DLSYM(hGLES, pglColorPointer, "glColorPointer");
    DLSYM(hGLES, pglDrawArrays, "glDrawArrays");
    DLSYM(hGLES, pglEnableClientState, "glEnableClientState");

    if (pSDL_Init(SDL_INIT_VIDEO)) { printf("SDL_Init FAIL\n"); return 1; }
    void *surf = pSDL_SetVideoMode(800, 480, 16, SDL_FULLSCREEN | SDL_OPENGL);
    if (!surf) { printf("SDL_SetVideoMode FAIL: %s\n", dlerror()); return 1; }

    bench_triangles(10.0);
    bench_sprites(10.0);
    printf("DEMO COMPLETE\n");
    return 0;
}
