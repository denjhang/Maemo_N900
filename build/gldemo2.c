/* gldemo - N900 SGX530 GLES1.1 performance test
 * All X11/EGL/GLES symbols resolved at runtime via dlopen/dlsym.
 * Zero link-time graphics dependencies.
 */
#include <stdio.h>
#include <stdlib.h>
/* debug: route all diagnostics to stderr (unbuffered) */
#define printf(...) fprintf(stderr, __VA_ARGS__)
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
static void (*pglDisable)(unsigned);
static void (*pglVertexPointer)(int, unsigned, int, const void *);
static void (*pglColorPointer)(int, unsigned, int, const void *);
static void (*pglDrawArrays)(unsigned, int, int);
static void (*pglDrawElements)(unsigned, int, unsigned, const void *);
static void (*pglEnableClientState)(unsigned);

#define DLSYM(h, var, name) do { \
    *(void **)(&var) = dlsym(h, name); \
    if (!var) { printf("dlsym FAIL: %s\n", name); return 1; } \
} while (0)


/* extra X11 — real XVisualInfo layout (Visual* first, visualid at +4, no ext_data) */
typedef struct { void *visual; unsigned long visualid; int screen; unsigned int depth; int c_class; unsigned long red_mask, green_mask, blue_mask; int colormap_size; unsigned int bits_per_rgb; } XVisualInfo;
#define VisualIDMask 0x1
typedef unsigned long XColormap;
extern XColormap XCreateColormap(void *, unsigned long, void *, int);
extern unsigned long XCreateWindow(void *, unsigned long, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, void *, unsigned long, void *);
#define TrueColor 4
#define AllocNone 0
#define CWColormap (1L<<13)
static void **(*pXGetVisualInfo)(void *, long, XVisualInfo *, int *);
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
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
/* GLES constants */
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x100
#define GL_TRIANGLES 4
#define GL_QUADS 7
#define GL_TRIANGLE_STRIP 5
typedef unsigned short GLushort;
#define GL_PROJECTION 0x1701
#define GL_MODELVIEW 0x1700
#define GL_FLOAT 0x1406
#define GL_UNSIGNED_SHORT 0x1403
#define GL_DEPTH_TEST 0x0B71
#define GL_VERTEX_ARRAY 0x8074
#define GL_COLOR_ARRAY 0x8076

static int load_syms(void) {
    printf("L: dlopen libX11\n");
    hX11 = dlopen("libX11.so.6", RTLD_NOW | RTLD_GLOBAL);
    if (!hX11) { printf("dlopen libX11: %s\n", dlerror()); return 1; }
    printf("L: dlopen libEGL\n");
    hEGL = dlopen("libEGL.so", RTLD_NOW | RTLD_GLOBAL);
    if (!hEGL) { printf("dlopen libEGL: %s\n", dlerror()); return 1; }
    printf("L: dlopen libGLES_CM\n");
    hGLES = dlopen("libGLES_CM.so", RTLD_NOW | RTLD_GLOBAL);
    if (!hGLES) { printf("dlopen libGLES_CM: %s\n", dlerror()); return 1; }
    printf("L: syms\n");
    DLSYM(hX11, pXOpenDisplay, "XOpenDisplay");
    DLSYM(hX11, pXDefaultScreen, "XDefaultScreen");
    DLSYM(hX11, pXRootWindow, "XRootWindow");
    DLSYM(hX11, pXCreateSimpleWindow, "XCreateSimpleWindow");
    DLSYM(hX11, pXMapWindow, "XMapWindow");
    DLSYM(hX11, pXStoreName, "XStoreName");
    DLSYM(hX11, pXSync, "XSync");
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
    DLSYM(hGLES, pglDisable, "glDisable");
    DLSYM(hGLES, pglVertexPointer, "glVertexPointer");
    DLSYM(hGLES, pglColorPointer, "glColorPointer");
    DLSYM(hGLES, pglDrawArrays, "glDrawArrays");
    *(void **)(&pglDrawElements) = dlsym(hGLES, "glDrawElements");
    DLSYM(hGLES, pglEnableClientState, "glEnableClientState");
    return 0;
}
typedef float GLfloat;
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;


static unsigned long win;

static int init_gfx(int w, int h) {
    printf("L: init_gfx enter\n");
    if (load_syms()) return 1;
    printf("L: XOpenDisplay\n");
    void *xdpy = pXOpenDisplay(NULL);
    if (!xdpy) { printf("XOpenDisplay FAIL\n"); return 1; }
    int scr = pXDefaultScreen(xdpy);

    eglD = dlsym(hEGL, "eglGetDisplay");  /* ensure symbol exists */
    void *(*fnGetDisplay)(void *) = dlsym(hEGL, "eglGetDisplay");
    if (!fnGetDisplay) { printf("no eglGetDisplay\n"); return 1; }
    eglD = fnGetDisplay(NULL);
    if (!eglD) { printf("eglGetDisplay FAIL\n"); return 1; }
    if (!peglInitialize(eglD, NULL, NULL)) { printf("eglInitialize FAIL err=%x\n", peglGetError()); return 1; }
    /* PPSSPP-maemo config: 16bpp buffer, 16-bit depth, GLES1 renderable */
#define EGL_BUFFER_SIZE 0x3020
#define EGL_DEPTH_SIZE 0x3025
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_ES_BIT 0x0001
#define EGL_SAMPLE_BUFFERS 0x3032
    int cfgA[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                   EGL_RED_SIZE, 5, EGL_GREEN_SIZE, 6, EGL_BLUE_SIZE, 5,
                   EGL_BUFFER_SIZE, 16, EGL_DEPTH_SIZE, 16,
                   EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
                   EGL_SAMPLE_BUFFERS, 0, EGL_NONE };
    /* find the X default visual id, ask EGL for a config matching it */
    static unsigned long (*pXVisualIDFromVisual)(void *);
    static void *(*pXDefaultVisual)(void *, int);
    if (!pXVisualIDFromVisual) {
        pXVisualIDFromVisual = dlsym(hX11, "XVisualIDFromVisual");
        pXDefaultVisual = dlsym(hX11, "XDefaultVisual");
    }
    unsigned long defvid = 0;
    if (pXVisualIDFromVisual && pXDefaultVisual)
        defvid = pXVisualIDFromVisual(pXDefaultVisual(xdpy, scr));
    printf("X default visual id: %lu\n", defvid);
    void *cfg = NULL;
    int ncfg = 0;
    int cfgA2[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
                    EGL_DEPTH_SIZE, 16,
                    EGL_NATIVE_VISUAL_ID, (int)defvid, EGL_NONE };
    if (defvid && peglChooseConfig(eglD, cfgA2, &cfg, 1, &ncfg) && ncfg >= 1) {
        printf("config matched default visual\n");
    } else {
        int cfgA[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                       EGL_RED_SIZE, 5, EGL_GREEN_SIZE, 6, EGL_BLUE_SIZE, 5, EGL_NONE };
        if (!peglChooseConfig(eglD, cfgA, &cfg, 1, &ncfg) || ncfg < 1) {
            printf("eglChooseConfig FAIL n=%d err=%x\n", ncfg, peglGetError());
            return 1;
        }
    }
    int visualid = 0;
    unsigned int dummy = 0;
    unsigned (*fnGetConfigAttrib)(void*,void*,int,unsigned*) = dlsym(hEGL, "eglGetConfigAttrib");
    if (fnGetConfigAttrib) fnGetConfigAttrib(eglD, cfg, EGL_NATIVE_VISUAL_ID, (unsigned *)&visualid);
    {
        unsigned rtype = 0, bsize = 0, dsize = 0;
        if (fnGetConfigAttrib) {
            fnGetConfigAttrib(eglD, cfg, EGL_RENDERABLE_TYPE, &rtype);
            fnGetConfigAttrib(eglD, cfg, EGL_BUFFER_SIZE, &bsize);
            fnGetConfigAttrib(eglD, cfg, EGL_DEPTH_SIZE, &dsize);
        }
        printf("config native visual id: %d rtype=%x bufsize=%u depth=%u\n",
               visualid, rtype, bsize, dsize);
    }

    /* window must use the EGL config's visual */
    if (!pXGetVisualInfo) pXGetVisualInfo = dlsym(hX11, "XGetVisualInfo");
    int got_vis = 0;
    XVisualInfo vitmpl, *pvi = NULL;
    int nvi = 0;
    memset(&vitmpl, 0, sizeof(vitmpl));
    vitmpl.visualid = (unsigned long)visualid;
    if (pXGetVisualInfo) {
        XVisualInfo *list = (XVisualInfo *)pXGetVisualInfo(xdpy, VisualIDMask, &vitmpl, &nvi);
        if (list && nvi > 0) { pvi = &list[0]; got_vis = 1; }
    }
    printf("window visual lookup: visualid=%d got=%d depth=%d\n",
           visualid, got_vis, got_vis ? pvi->depth : -1);
    if (got_vis && pvi) {
        XColormap cmap = pXCreateColormap(xdpy, pXRootWindow(xdpy, scr),
                                          pvi->visual, AllocNone);
        unsigned long vmask = CWColormap;
        unsigned long xattr[16] = {0};
        xattr[13] = (unsigned long)cmap; /* XSetWindowAttributes.colormap */
        win = pXCreateWindow(xdpy, pXRootWindow(xdpy, scr), 0, 0, w, h, 0,
                             pvi->depth, 1 /*InputOutput*/, pvi->visual, vmask, xattr);
    } else {
        win = pXCreateSimpleWindow(xdpy, pXRootWindow(xdpy, scr), 0, 0, w, h, 0, 0, 0);
    }
    pXStoreName(xdpy, win, "GLDemo");
    pXMapWindow(xdpy, win);
    pXSync(xdpy, 0);

    int ctxA[] = { EGL_CONTEXT_CLIENT_VERSION, 1, EGL_NONE };
    eglS = peglCreateWindowSurface(eglD, cfg, win, NULL);
    if (!eglS) { printf("eglCreateWindowSurface FAIL err=%x\n", peglGetError()); return 1; }
    eglC = peglCreateContext(eglD, cfg, NULL, ctxA);
    if (!eglC) { printf("eglCreateContext FAIL err=%x\n", peglGetError()); return 1; }
    if (!peglMakeCurrent(eglD, eglS, eglS, eglC)) {
        printf("eglMakeCurrent FAIL err=%x\n", peglGetError());
        return 1;
    }
    pglViewport(0, 0, w, h);
    pglMatrixMode(GL_PROJECTION);
    pglLoadIdentity();
    pglFrustumf(-1.0f, 1.0f, -(float)h / w, (float)h / w, 1.0f, 100.0f);
    pglMatrixMode(GL_MODELVIEW);
    pglEnable(GL_DEPTH_TEST);
    /* warm-up: vertex-only draw first — the Fremantle ES1 driver crashes
       (memcpy src=NULL) if the very first draw has color arrays enabled */
    {
        static GLfloat wv[3] = {0.f, 0.f, 0.f};
        pglDisable(GL_DEPTH_TEST);
        pglEnableClientState(GL_VERTEX_ARRAY);
        pglVertexPointer(3, GL_FLOAT, 0, wv);
        pglDrawArrays(GL_TRIANGLES, 0, 1);
        peglSwapBuffers(eglD, eglS);
        pglEnable(GL_DEPTH_TEST);
        printf("warm-up draw OK\n");
    }
    printf("GFX init OK %dx%d\n", w, h);
    return 0;
}

static GLfloat tri[12], tcol[16];
static unsigned short idx[3] = {0, 1, 2};
static char bench_mode(void) {
    char m = 'q';
    FILE *f = fopen("/home/user/gmode", "r"); /* /tmp tmpfs is 1MB and full */
    if (f) { m = (char)fgetc(f); fclose(f); }
    printf("mode=%c\n", m);
    return m;
}
#include <signal.h>
static void segv(int s, siginfo_t *si, void *uc) {
    /* async-signal-safe: snprintf to stack buffer + raw write(), no stdio,
       no /proc parsing (fopen inside a handler is a second-crash trap). */
    char buf[256];
    unsigned long *u = (unsigned long *)uc;
    unsigned long *sc = u + 5; /* 20-byte ucontext header, then sigcontext */
    int n;
    const char *nm = (s == SIGSEGV) ? "SIGSEGV" : "SIGBUS";
    if (!si || !uc) { n = snprintf(buf, sizeof buf, "%s: no context\n", nm); }
    else {
        unsigned long pc = (sc + 18 < u + 64) ? sc[18] : 0;
        unsigned long lr = (sc + 17 < u + 64) ? sc[17] : 0;
        n = snprintf(buf, sizeof buf,
            "%s addr=%p pc=%08lx lr=%08lx\n", nm, si->si_addr, pc, lr);
    }
    if (n > 0) { ssize_t r = write(2, buf, (size_t)n); (void)r; }
    _exit(88);
}
static void bench_triangles(double dur) {
    /* debug: handler removed — let kernel deliver raw signal (139 + dmesg) */
    char mode = bench_mode();
    if (mode != 'v') pglDisable(GL_DEPTH_TEST);
    double t0 = now(), t;
    int frames = 0;
    float ang = 0;
    printf("tri: enter loop\n");
    while ((t = now()) - t0 < dur) {
        if (frames == 0) {
            printf("tri: f0 clear\n"); pglClearColor(0.5f, 0.0f, 0.0f, 1);
            pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            printf("tri: f0 matrices\n");
        }
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
        tri[9] = 1; tri[10] = 1; tri[11] = 0;
        tcol[0] = 1; tcol[1] = 0.3f; tcol[2] = 0.2f; tcol[3] = 1;
        tcol[4] = 0.2f; tcol[5] = 1; tcol[6] = 0.3f; tcol[7] = 1;
        tcol[8] = 0.3f; tcol[9] = 0.3f; tcol[10] = 1; tcol[11] = 1;
        tcol[12] = 1; tcol[13] = 1; tcol[14] = 0.3f; tcol[15] = 1;
        pglEnableClientState(GL_VERTEX_ARRAY);
        pglVertexPointer(3, GL_FLOAT, 0, tri);
        if (mode == 'v') {
            if (frames == 0) printf("tri: f0 vertex-only draw\n");
            pglDrawArrays(GL_TRIANGLES, 0, 3);
        } else {
            pglEnableClientState(GL_COLOR_ARRAY);
            pglColorPointer(4, GL_FLOAT, 0, tcol);
            if (frames == 0) printf("tri: f0 draw fn=%p vertptr=%p colptr=%p\n",
                                    (void *)pglDrawArrays, tri, tcol);
            if (mode == 'e') {
                printf("tri: f0 drawElements\n");
                if (pglDrawElements) pglDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, idx);
            } else if (mode == 'q') {
                printf("tri: f0 colored QUADS x4\n");
                pglDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            } else if (mode != 'n') {
                pglDrawArrays(GL_TRIANGLES, 0, 3);
            }
        }
        if (frames == 0) printf("tri: f0 swap\n");
        peglSwapBuffers(eglD, eglS);
        frames++;
        if (frames == 1) printf("tri: frame 1 done\n");
    }
    printf("MODE triangles: %d frames in %.2fs = %.1f FPS\n",
           frames, now() - t0, frames / (now() - t0));
}

#define NSPRITE 300
static GLfloat sv[NSPRITE * 12], sc[NSPRITE * 16];
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
                sc[i * 16 + v * 4] = c;
                sc[i * 16 + v * 4 + 1] = 1.0f - c;
                sc[i * 16 + v * 4 + 2] = 0.5f;
                sc[i * 16 + v * 4 + 3] = 1.0f;
            }
        }
        pglEnableClientState(GL_VERTEX_ARRAY);
        pglEnableClientState(GL_COLOR_ARRAY);
        pglVertexPointer(3, GL_FLOAT, 0, sv);
        pglColorPointer(4, GL_FLOAT, 0, sc);
        {
            static GLushort sidx[NSPRITE * 6];
            static int sidx_init = 0;
            if (!sidx_init) {
                int q;
                for (q = 0; q < NSPRITE; q++) {
                    sidx[q * 6 + 0] = q * 4 + 0; sidx[q * 6 + 1] = q * 4 + 1;
                    sidx[q * 6 + 2] = q * 4 + 2; sidx[q * 6 + 3] = q * 4 + 0;
                    sidx[q * 6 + 4] = q * 4 + 2; sidx[q * 6 + 5] = q * 4 + 3;
                }
                sidx_init = 1;
            }
            pglDrawElements(GL_TRIANGLES, NSPRITE * 6, 0x1403 /*GL_UNSIGNED_SHORT*/, sidx);
        }
        peglSwapBuffers(eglD, eglS);
        frames++;
    }
    printf("MODE sprites(%d): %d frames in %.2fs = %.1f FPS\n",
           NSPRITE, frames, now() - t0, frames / (now() - t0));
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    if (init_gfx(800, 480)) return 1;
    bench_triangles(10.0);
    bench_sprites(10.0);
    printf("DEMO COMPLETE\n");
    return 0;
}
