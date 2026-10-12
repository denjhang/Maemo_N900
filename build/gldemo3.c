/* gldemo3 - N900 GLES1.1 perf test via SDL + SDL_gles
 * dlopen-only: no link-time graphics deps.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <dlfcn.h>

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

/* SDL */
static void *hSDL, *hSDLGLES, *hGLES;
static int (*pSDL_Init)(unsigned);
static void *(*pSDL_SetVideoMode)(int, int, int, unsigned);
static void *(*pSDL_GLES_SetVideoMode)(int, int, int, unsigned);
typedef void SDL_Surface_t;

/* SDL_gles */
static int (*pSDL_GLES_Init)(int);
static int (*pSDL_GLES_LoadLibrary)(const char *);
static void *(*pSDL_GLES_CreateContext)(void *);
static int (*pSDL_GLES_MakeCurrent)(void *);
static int (*pSDL_GLES_SwapBuffers)(void);

/* GLES1.1 */
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

static int load_and_init(int w, int h) {
    hSDL = dlopen("libSDL-1.2.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (!hSDL) { printf("dlopen SDL: %s\n", dlerror()); return 1; }
    DLSYM(hSDL, pSDL_Init, "SDL_Init");
    DLSYM(hSDL, pSDL_SetVideoMode, "SDL_SetVideoMode");

    hSDLGLES = dlopen("libSDL_gles-1.2.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (!hSDLGLES) { printf("dlopen SDL_gles: %s\n", dlerror()); return 1; }
    DLSYM(hSDLGLES, pSDL_GLES_Init, "SDL_GLES_Init");
    DLSYM(hSDLGLES, pSDL_GLES_LoadLibrary, "SDL_GLES_LoadLibrary");
    DLSYM(hSDLGLES, pSDL_GLES_CreateContext, "SDL_GLES_CreateContext");
    DLSYM(hSDLGLES, pSDL_GLES_MakeCurrent, "SDL_GLES_MakeCurrent");
    DLSYM(hSDLGLES, pSDL_GLES_SwapBuffers, "SDL_GLES_SwapBuffers");
    DLSYM(hSDLGLES, pSDL_GLES_SetVideoMode, "SDL_GLES_SetVideoMode");

    if (pSDL_Init(SDL_INIT_VIDEO)) { printf("SDL_Init FAIL\n"); return 1; }
    if (pSDL_GLES_Init(SDL_GLES_1_1)) { printf("SDL_GLES_Init FAIL\n"); return 1; }
    pSDL_GLES_LoadLibrary(NULL);

    hGLES = dlopen("libGLES_CM.so", RTLD_NOW | RTLD_GLOBAL);
    if (!hGLES) { printf("dlopen GLES: %s\n", dlerror()); return 1; }
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

    void *surf = pSDL_GLES_SetVideoMode(w, h, 16, SDL_FULLSCREEN | SDL_OPENGL);
    if (!surf) { printf("SDL_SetVideoMode FAIL: %s\n", dlerror()); return 1; }
    void *ctx = pSDL_GLES_CreateContext(0);
    if (!ctx) { printf("CreateContext FAIL\n"); return 1; }
    if (pSDL_GLES_MakeCurrent(ctx)) { printf("MakeCurrent FAIL\n"); return 1; }

    pglViewport(0, 0, w, h);
    pglMatrixMode(GL_PROJECTION);
    pglLoadIdentity();
    pglFrustumf(-1.0f, 1.0f, -(float)h / w, (float)h / w, 1.0f, 100.0f);
    pglMatrixMode(GL_MODELVIEW);
    pglEnable(GL_DEPTH_TEST);
    printf("GFX init OK %dx%d\n", w, h);
    return 0;
}

typedef float GLfloat;
typedef unsigned int GLbitfield;
static GLfloat tri[9], tcol[9];
static void bench_triangles(double dur) {
    double t0 = now(), t;
    int frames = 0;
    float ang = 0;
    while ((t = now()) - t0 < dur) {
        ang += 2.0f;
        pglClearColor(0.05f, 0.05f, 0.1f, 1);
        pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        pglMatrixMode(GL_MODELVIEW);
        pglLoadIdentity();
        pglTranslatef(0, 0, -3.5f);
        pglRotatef(ang, 0, 1, 0);
        pglRotatef(ang * 0.7f, 0, 0, 1);
        tri[0] = 0; tri[1] = 1; tri[2] = 0;
        tri[3] = -1; tri[4] = -1; tri[5] = 0;
        tri[6] = 1; tri[7] = -1; tri[8] = 0;
        tcol[0] = 1; tcol[1] = 0.3f; tcol[2] = 0.2f;
        tcol[3] = 0.2f; tcol[4] = 1; tcol[5] = 0.3f;
        tcol[6] = 0.3f; tcol[7] = 0.3f; tcol[8] = 1;
        pglEnableClientState(GL_VERTEX_ARRAY);
        pglEnableClientState(GL_COLOR_ARRAY);
        pglVertexPointer(3, GL_FLOAT, 0, tri);
        pglColorPointer(3, GL_FLOAT, 0, tcol);
        pglDrawArrays(GL_TRIANGLES, 0, 3);
        pSDL_GLES_SwapBuffers();
        frames++;
    }
    printf("MODE triangles: %d frames in %.2fs = %.1f FPS\n",
           frames, now() - t0, frames / (now() - t0));
}

#define NSPRITE 300
static GLfloat sv[NSPRITE * 12], sc[NSPRITE * 12];
static void bench_sprites(double dur) {
    double t0 = now(), t;
    int frames = 0;
    float ang = 0;
    srand(1234);
    float px[NSPRITE], py[NSPRITE], ph[NSPRITE];
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
        pSDL_GLES_SwapBuffers();
        frames++;
    }
    printf("MODE sprites(%d): %d frames in %.2fs = %.1f FPS\n",
           NSPRITE, frames, now() - t0, frames / (now() - t0));
}

int main(void) {
    if (load_and_init(800, 480)) return 1;
    bench_triangles(10.0);
    bench_sprites(10.0);
    printf("DEMO COMPLETE\n");
    return 0;
}
