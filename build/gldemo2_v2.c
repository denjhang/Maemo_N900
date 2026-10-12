/* gldemo - N900 SGX530 GLES1.1 performance test
 * All X11/EGL/GLES symbols resolved at runtime via dlopen/dlsym.
 * Zero link-time graphics dependencies.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <dlfcn.h>

extern void _start(void) __attribute__((noreturn));
__asm__(".global _start\n"
        "_start:\n"
        "    bl main\n"
        "    bl exit\n"
        "    b .\n");

#include <sys/time.h>
static double now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

/* ---- dynamic symbols ---- */
static void *hX11, *hEGL, *hGLES;

/* X11 */
static void *(*pXOpenDisplay)(const char *);
static int (*pXDefaultScreen)(void *);
static unsigned long (*pXRootWindow)(void *, int);
static unsigned long (*pXCreateSimpleWindow)(void *, unsigned long, int, int, int, int, int, unsigned long, unsigned long);
static int (*pXMapWindow)(void *, unsigned long);
static int (*pXStoreName)(void *, unsigned long, const char *);
static int (*pXSync)(void *, int);

/* EGL (handles as void*) */
static void *eglD, *eglS, *eglC;
static unsigned (*peglInitialize)(void *, int *, int *);
static unsigned (*peglChooseConfig)(void *, const int *, void **, int, int *);
static void *(*peglCreateWindowSurface)(void *, void *, unsigned long, const int *);
static void *(*peglCreateContext)(void *, void *, void *, const int *);
static unsigned (*peglMakeCurrent)(void *, void *, void *, void *);
static unsigned (*peglSwapBuffers)(void *, void *);
static unsigned (*peglGetError)(void);

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


/* extra X11 */
typedef struct { void *ext_data; unsigned long visualid; int screen; int depth; int c_class; unsigned long red_mask, green_mask, blue_mask; int colormap_size; int bits_per_rgb; } XVisualInfo;
extern int XMatchVisualInfo(void *, int, int, int, XVisualInfo *);
typedef unsigned long XColormap;
extern XColormap XCreateColormap(void *, unsigned long, void *, int);
extern unsigned long XCreateWindow(void *, unsigned long, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, void *, unsigned long, void *);
#define TrueColor 4
#define AllocNone 0
#define CWColormap (1L<<13)
static int (*pXMatchVisualInfo)(void *, int, int, int, void *);
static XColormap (*pXCreateColormap)(void *, unsigned long, void *, int);
static unsigned long (*pXCreateWindow)(void *, unsigned long, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, void *, unsigned long, void *);
#define EGL_NATIVE_VISUAL_ID 0x302E

/* EGL constants */
#define EGL_SURFACE_TYPE 0x3033
#define EGL_WINDOW_BIT 0x0004
#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_BLUE_SIZE 0x3022
#define EGL_DEPTH_SIZE 0x3025
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_ES_BIT 0x0001
#define EGL_NONE 0x3038
#define EGL_OPENGL_ES_API 0x3097
/* GLES constants */
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

static int load_syms(void) {
    hX11 = dlopen("libX11.so.6", RTLD_NOW | RTLD_GLOBAL);
    if (!hX11) { printf("dlopen libX11: %s\n", dlerror()); return 1; }
    hEGL = dlopen("libEGL.so", RTLD_NOW | RTLD_GLOBAL);
    if (!hEGL) { printf("dlopen libEGL: %s\n", dlerror()); return 1; }
    hGLES = dlopen("libGLES_CM.so", RTLD_NOW | RTLD_GLOBAL);
    if (!hGLES) { printf("dlopen libGLES_CM: %s\n", dlerror()); return 1; }
    DLSYM(hX11, pXOpenDisplay, "XOpenDisplay");
    DLSYM(hX11, pXDefaultScreen, "XDefaultScreen");
    DLSYM(hX11, pXRootWindow, "XRootWindow");
    DLSYM(hX11, pXCreateSimpleWindow, "XCreateSimpleWindow");
    DLSYM(hX11, pXMapWindow, "XMapWindow");
    DLSYM(hX11, pXStoreName, "XStoreName");
    DLSYM(hX11, pXSync, "XSync");
    DLSYM(hX11, pXMatchVisualInfo, "XMatchVisualInfo");
    DLSYM(hX11, pXCreateColormap, "XCreateColormap");
    DLSYM(hX11, pXCreateWindow, "XCreateWindow");
    DLSYM(hEGL, peglInitialize, "eglInitialize");
    DLSYM(hEGL, peglChooseConfig, "eglChooseConfig");
    DLSYM(hEGL, peglCreateWindowSurface, "eglCreateWindowSurface");
    DLSYM(hEGL, peglCreateContext, "eglCreateContext");
    DLSYM(hEGL, peglMakeCurrent, "eglMakeCurrent");
    DLSYM(hEGL, peglSwapBuffers, "eglSwapBuffers");
    DLSYM(hEGL, peglGetError, "eglGetError");
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
    return 0;
}
typedef float GLfloat;
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;


static unsigned long win;

static int init_gfx(int w, int h) {
    if (load_syms()) return 1;
    void *xdpy = pXOpenDisplay(NULL);
    if (!xdpy) { printf("XOpenDisplay FAIL\n"); return 1; }
    int scr = pXDefaultScreen(xdpy);

    eglD = dlsym(hEGL, "eglGetDisplay");  /* ensure symbol exists */
    void *(*fnGetDisplay)(void *) = dlsym(hEGL, "eglGetDisplay");
    if (!fnGetDisplay) { printf("no eglGetDisplay\n"); return 1; }
    eglD = fnGetDisplay(NULL);
    if (!eglD) { printf("eglGetDisplay FAIL\n"); return 1; }
    if (!peglInitialize(eglD, NULL, NULL)) { printf("eglInitialize FAIL err=%x\n", peglGetError()); return 1; }
    int cfgA[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 5,
                   EGL_GREEN_SIZE, 6, EGL_BLUE_SIZE, 5, EGL_NONE };
    void *cfg = NULL;
    int ncfg = 0;
    if (!peglChooseConfig(eglD, cfgA, &cfg, 1, &ncfg) || ncfg < 1) {
        printf("eglChooseConfig FAIL n=%d err=%x\n", ncfg, peglGetError());
        return 1;
    }
    int visualid = 0;
    unsigned int dummy = 0;
    peglGetConfigAttrib ? (void)0 : (void)0;
    unsigned (*fnGetConfigAttrib)(void*,void*,int,unsigned*) = dlsym(hEGL, "eglGetConfigAttrib");
    if (fnGetConfigAttrib) fnGetConfigAttrib(eglD, cfg, EGL_NATIVE_VISUAL_ID, (unsigned *)&visualid);
    printf("config native visual id: %d
", visualid);

    XVisualInfo vi;
    memset(&vi, 0, sizeof(vi));
    int have_vis = 0;
    if (visualid) {
        if (pXMatchVisualInfo(xdpy, scr, visualid, TrueColor, &vi)) have_vis = 1;
    }
    XColormap cmap;
    unsigned long xwin;
    if (have_vis) {
        cmap = pXCreateColormap(xdpy, pXRootWindow(xdpy, scr), (void *)vi.visual, AllocNone);
        unsigned long vmask = CWColormap;
        xwin = pXCreateWindow(xdpy, pXRootWindow(xdpy, scr), 0, 0, w, h, 0,
                              vi.depth, 1 /*InputOutput*/, (void *)vi.visual, vmask, &cmap);
    } else {
        cmap = pXCreateColormap(xdpy, pXRootWindow(xdpy, scr),
                                (void *)0, AllocNone); /* fallback */
        xwin = pXCreateSimpleWindow(xdpy, pXRootWindow(xdpy, scr), 0, 0, w, h, 0, 0, 0);
    }
    win = xwin;
    pXStoreName(xdpy, win, "GLDemo");
    pXMapWindow(xdpy, win);
    pXSync(xdpy, 0);

    int ctxA[] = { EGL_OPENGL_ES_API, EGL_NONE };
    eglS = peglCreateWindowSurface(eglD, cfg, win, NULL);
    if (!eglS) { printf("eglCreateWindowSurface FAIL err=%x
", peglGetError()); return 1; }
    eglC = peglCreateContext(eglD, cfg, NULL, ctxA);
    if (!eglC) { printf("eglCreateContext FAIL err=%x
", peglGetError()); return 1; }
    if (!peglMakeCurrent(eglD, eglS, eglS, eglC)) {
        printf("eglMakeCurrent FAIL err=%x
", peglGetError());
        return 1;
    }
    pglViewport(0, 0, w, h);
    pglMatrixMode(GL_PROJECTION);
    pglLoadIdentity();
    pglFrustumf(-1.0f, 1.0f, -(float)h / w, (float)h / w, 1.0f, 100.0f);
    pglMatrixMode(GL_MODELVIEW);
    pglEnable(GL_DEPTH_TEST);
    printf("GFX init OK %dx%d
", w, h);
    return 0;
}

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
        peglSwapBuffers(eglD, eglS);
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
        peglSwapBuffers(eglD, eglS);
        frames++;
    }
    printf("MODE sprites(%d): %d frames in %.2fs = %.1f FPS\n",
           NSPRITE, frames, now() - t0, frames / (now() - t0));
}

int main(void) {
    if (init_gfx(800, 480)) return 1;
    bench_triangles(10.0);
    bench_sprites(10.0);
    printf("DEMO COMPLETE\n");
    return 0;
}
