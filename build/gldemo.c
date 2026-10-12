#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

/* ---- minimal X11 prototypes ---- */
typedef struct _XDisplay XDisplay;
typedef unsigned long XWindow;
extern XDisplay *XOpenDisplay(const char *);
extern int XCloseDisplay(XDisplay *);
extern int XDefaultScreen(XDisplay *);
extern XWindow XRootWindow(XDisplay *, int);
extern XWindow XCreateSimpleWindow(XDisplay *, XWindow, int, int, int, int, int, unsigned long, unsigned long);
extern int XMapWindow(XDisplay *, XWindow);
extern int XStoreName(XDisplay *, XWindow, const char *);
extern int XSync(XDisplay *, int);

/* ---- minimal EGL prototypes ---- */
typedef void *EGLDisplay;
typedef void *EGLSurface;
typedef void *EGLContext;
typedef void *EGLConfig;
typedef int EGLint;
#define EGL_DEFAULT_DISPLAY ((void *)0)
#define EGL_SURFACE_TYPE 0x3033
#define EGL_WINDOW_BIT 0x0004
#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_BLUE_SIZE 0x3022
#define EGL_ALPHA_SIZE 0x3021
#define EGL_DEPTH_SIZE 0x3025
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_ES_BIT 0x0001
#define EGL_NONE 0x3038
#define EGL_OPENGL_ES_API 0x3097
extern EGLDisplay eglGetDisplay(void *);
extern unsigned eglInitialize(EGLDisplay, EGLint *, EGLint *);
extern unsigned eglChooseConfig(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
extern EGLSurface eglCreateWindowSurface(EGLDisplay, EGLConfig, XWindow, const EGLint *);
extern EGLContext eglCreateContext(EGLDisplay, EGLConfig, EGLContext, const EGLint *);
extern unsigned eglMakeCurrent(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
extern unsigned eglSwapBuffers(EGLDisplay, EGLSurface);
extern unsigned eglGetError(void);

/* ---- minimal GLES1.1 prototypes ---- */
typedef float GLfloat;
typedef int GLint;
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
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
extern void glViewport(int, int, int, int);
extern void glClearColor(float, float, float, float);
extern void glClear(GLbitfield);
extern void glMatrixMode(GLenum);
extern void glLoadIdentity(void);
extern void glFrustumf(float, float, float, float, float, float);
extern void glTranslatef(float, float, float);
extern void glRotatef(float, float, float, float);
extern void glEnable(GLenum);
extern void glVertexPointer(int, GLenum, int, const void *);
extern void glColorPointer(int, GLenum, int, const void *);
extern void glDrawArrays(GLenum, int, int);
extern void glEnableClientState(GLenum);

extern void _start(void) __attribute__((noreturn));
__asm__(".global _start\n"
        "_start:\n"
        "    bl main\n"
        "    bl exit\n"
        "    b .\n");

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static EGLDisplay edpy;
static EGLSurface esurf;
static EGLContext ectx;
static XDisplay *xdpy;
static XWindow win;

static int init_gfx(int w, int h) {
    xdpy = XOpenDisplay(NULL);
    if (!xdpy) { printf("XOpenDisplay FAIL\n"); return 1; }
    int scr = XDefaultScreen(xdpy);
    win = XCreateSimpleWindow(xdpy, XRootWindow(xdpy, scr), 0, 0, w, h, 0, 0, 0);
    XStoreName(xdpy, win, "GLDemo");
    XMapWindow(xdpy, win);
    XSync(xdpy, 0);

    edpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglInitialize(edpy, NULL, NULL) != 1) {
        printf("eglInitialize FAIL err=%x\n", eglGetError());
        return 1;
    }
    EGLint cfgA[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 5,
                      EGL_GREEN_SIZE, 6, EGL_BLUE_SIZE, 5, EGL_DEPTH_SIZE, 16,
                      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT, EGL_NONE };
    EGLConfig cfg;
    EGLint ncfg = 0;
    if (eglChooseConfig(edpy, cfgA, &cfg, 1, &ncfg) != 1 || ncfg < 1) {
        printf("eglChooseConfig FAIL n=%d err=%x\n", ncfg, eglGetError());
        return 1;
    }
    esurf = eglCreateWindowSurface(edpy, cfg, win, NULL);
    if (!esurf) { printf("eglCreateWindowSurface FAIL err=%x\n", eglGetError()); return 1; }
    EGLint ctxA[] = { EGL_OPENGL_ES_API, EGL_NONE };
    ectx = eglCreateContext(edpy, cfg, (EGLContext)0, ctxA);
    if (!ectx) { printf("eglCreateContext FAIL err=%x\n", eglGetError()); return 1; }
    if (eglMakeCurrent(edpy, esurf, esurf, ectx) != 1) {
        printf("eglMakeCurrent FAIL err=%x\n", eglGetError());
        return 1;
    }
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustumf(-1.0f, 1.0f, -(float)h / w, (float)h / w, 1.0f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
    printf("GFX init OK %dx%d\n", w, h);
    return 0;
}

static GLfloat tri[9], tcol[9];
static void bench_triangles(double dur) {
    double t0 = now();
    int frames = 0;
    float ang = 0;
    double t = t0;
    while ((t = now()) - t0 < dur) {
        ang += 2.0f;
        glClearColor(0.05f, 0.05f, 0.1f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        glTranslatef(0, 0, -3.5f);
        glRotatef(ang, 0, 1, 0);
        glRotatef(ang * 0.7f, 0, 0, 1);
        tri[0] = 0; tri[1] = 1; tri[2] = 0;
        tri[3] = -1; tri[4] = -1; tri[5] = 0;
        tri[6] = 1; tri[7] = -1; tri[8] = 0;
        tcol[0] = 1; tcol[1] = 0.3f; tcol[2] = 0.2f;
        tcol[3] = 0.2f; tcol[4] = 1; tcol[5] = 0.3f;
        tcol[6] = 0.3f; tcol[7] = 0.3f; tcol[8] = 1;
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, tri);
        glColorPointer(3, GL_FLOAT, 0, tcol);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        eglSwapBuffers(edpy, esurf);
        frames++;
    }
    printf("MODE triangles: %d frames in %.2fs = %.1f FPS\n", frames, now() - t0, frames / (now() - t0));
}

#define NSPRITE 300
static GLfloat sv[NSPRITE * 12], sc[NSPRITE * 12];
static void bench_sprites(double dur) {
    double t0 = now();
    int frames = 0;
    float ang = 0;
    double t = t0;
    srand(1234);
    float px[NSPRITE], py[NSPRITE], ph[NSPRITE];
    int i;
    for (i = 0; i < NSPRITE; i++) {
        px[i] = rand() % 2000 / 1000.0f - 1.0f;
        py[i] = rand() % 1200 / 1000.0f - 0.6f;
        ph[i] = rand() % 360 / 57.3f;
    }
    while ((t = now()) - t0 < dur) {
        ang += 1.5f;
        glClearColor(0.02f, 0.02f, 0.06f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        glTranslatef(0, 0, -3.0f);
        for (i = 0; i < NSPRITE; i++) {
            float ca = ph[i] + ang * 0.3f;
            float s = 0.08f;
            float x = px[i], y = py[i] + 0.3f * sinf(t * 0.5f + ph[i]);
            int v;
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
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, sv);
        glColorPointer(3, GL_FLOAT, 0, sc);
        glDrawArrays(GL_QUADS, 0, NSPRITE * 4);
        eglSwapBuffers(edpy, esurf);
        frames++;
    }
    printf("MODE sprites(%d): %d frames in %.2fs = %.1f FPS\n", NSPRITE, frames, now() - t0, frames / (now() - t0));
}

extern void exit(int);
int main(void) {
    if (init_gfx(800, 480)) return 1;
    bench_triangles(10.0);
    bench_sprites(10.0);
    printf("DEMO COMPLETE\n");
    return 0;
}
