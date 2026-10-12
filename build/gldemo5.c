/* gldemo4 - v4: SetVideoMode combo probing */
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

static void *hSDL, *hSDLGLES, *hGLES;
static int (*pSDL_Init)(unsigned);
static void *(*pSDL_SetVideoMode)(int, int, int, unsigned);
static void *(*pSDL_GLES_SetVideoMode)(int, int, int, unsigned);
static int (*pSDL_GLES_Init)(int);
static int (*pSDL_GLES_LoadLibrary)(const char *);
static void *(*pSDL_GLES_CreateContext)(int);
static int (*pSDL_GLES_MakeCurrent)(void *);
static int (*pSDL_GLES_SwapBuffers)(void);

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
#define SDL_GLES_1_1 1
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

int main(void) {
    hSDL = dlopen("libSDL-1.2.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (!hSDL) { printf("dlopen SDL: %s\n", dlerror()); return 1; }
    hSDLGLES = dlopen("libSDL_gles-1.2.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (!hSDLGLES) { printf("dlopen SDL_gles: %s\n", dlerror()); return 1; }
    hGLES = dlopen("libGLES_CM.so", RTLD_NOW | RTLD_GLOBAL);
    if (!hGLES) { printf("dlopen GLES: %s\n", dlerror()); return 1; }

    DLSYM(hSDL, pSDL_Init, "SDL_Init");
    DLSYM(hSDLGLES, pSDL_GLES_Init, "SDL_GLES_Init");
    DLSYM(hSDLGLES, pSDL_GLES_LoadLibrary, "SDL_GLES_LoadLibrary");
    DLSYM(hSDL, pSDL_SetVideoMode, "SDL_SetVideoMode");
    DLSYM(hSDLGLES, pSDL_GLES_SetVideoMode, "SDL_GLES_SetVideoMode");
    DLSYM(hSDLGLES, pSDL_GLES_CreateContext, "SDL_GLES_CreateContext");
    DLSYM(hSDLGLES, pSDL_GLES_MakeCurrent, "SDL_GLES_MakeCurrent");
    DLSYM(hSDLGLES, pSDL_GLES_SwapBuffers, "SDL_GLES_SwapBuffers");
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
    if (pSDL_GLES_Init(SDL_GLES_1_1)) { printf("SDL_GLES_Init FAIL\n"); return 1; }
    pSDL_GLES_LoadLibrary(NULL);

    int w = 800, h = 480;
    unsigned flagv[] = { SDL_FULLSCREEN | SDL_OPENGL, SDL_FULLSCREEN,
                         SDL_FULLSCREEN | SDL_OPENGL | 0x1000000u, SDL_FULLSCREEN };
    unsigned bv[] = { 16, 16, 32, 32 };
    void *surf = NULL;
    for (int ci = 0; ci < 4 && !surf; ci++) {
        surf = pSDL_SetVideoMode(w, h, bv[ci], flagv[ci]);
        printf("combo %d flags=%08x bpp=%d -> %p\n", ci, flagv[ci], bv[ci], surf);
    }
    if (!surf) { printf("ALL SetVideoMode combos FAIL\n"); return 1; }

    void *ctx = pSDL_GLES_CreateContext(0);
    if (!ctx) { printf("CreateContext FAIL\n"); return 1; }
    if (pSDL_GLES_MakeCurrent(ctx)) { printf("MakeCurrent FAIL\n"); return 1; }

    pglViewport(0, 0, w, h);
    pglMatrixMode(GL_PROJECTION);
    pglLoadIdentity();
    pglFrustumf(-1.0f, 1.0f, -(float)h / w, (float)h / w, 1.0f, 100.0f);
    pglMatrixMode(GL_MODELVIEW);
    pglEnable(GL_DEPTH_TEST);

    if (1) {
        double t0 = now(), t;
        int frames = 0;
        float ang = 0;
        while ((t = now()) - t0 < 10.0) {
            ang += 2.0f;
            pglClearColor(0.05f, 0.05f, 0.1f, 1);
            pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            pglMatrixMode(GL_MODELVIEW);
            pglLoadIdentity();
            pglTranslatef(0, 0, -3.5f);
            pglRotatef(ang, 0, 1, 0);
            pglRotatef(ang * 0.7f, 0, 0, 1);
            GLfloat tv[9], tc[9];
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
            pSDL_GLES_SwapBuffers();
            frames++;
        }
        printf("MODE triangles: %d frames in %.2fs = %.1f FPS\n",
               frames, now() - t0, frames / (now() - t0));
    }
    if (1) {
        double t0 = now(), t;
        int frames = 0;
        float ang = 0;
        srand(1234);
        float px[300], py[300], ph[300];
        int i, v;
        for (i = 0; i < 300; i++) {
            px[i] = rand() % 2000 / 1000.0f - 1.0f;
            py[i] = rand() % 1200 / 1000.0f - 0.6f;
            ph[i] = rand() % 360 / 57.3f;
        }
        GLfloat sv[300 * 12], sc[300 * 12];
        while ((t = now()) - t0 < 10.0) {
            ang += 1.5f;
            pglClearColor(0.02f, 0.02f, 0.06f, 1);
            pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            pglMatrixMode(GL_MODELVIEW);
            pglLoadIdentity();
            pglTranslatef(0, 0, -3.0f);
            for (i = 0; i < 300; i++) {
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
            pglDrawArrays(GL_QUADS, 0, 300 * 4);
            pSDL_GLES_SwapBuffers();
            frames++;
        }
        printf("MODE sprites: %d frames in %.2fs = %.1f FPS\n",
               frames, now() - t0, frames / (now() - t0));
    }
    printf("DEMO COMPLETE\n");
    return 0;
}
