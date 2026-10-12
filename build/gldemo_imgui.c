/* gldemo_imgui - Dear ImGui 1.92.8 on bare X11+EGL+GLES1.1, N900 fullscreen.
 * C scaffolding (runtime dlopen, glmemperf fullscreen recipe); ImGui rendering
 * is done by imgui_gles1.cpp (custom GLES1.1 fixed-function backend).
 * Infinite loop; /home/user/gmode: 'r' = spinning background cubes, else plain.
 */
#include <stdio.h>
#include <stdlib.h>
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

#include <stdarg.h>
/* gcc11 headers redirect sscanf -> __isoc99_sscanf; old sysroot glibc 2.5
   has no such symbol — forward it ourselves */
void *__dso_handle = (void *)&__dso_handle; /* -nostartfiles: no crtbegin */

typedef int (*vsscanf_fn)(const char *, const char *, va_list);
static vsscanf_fn real_vsscanf;
int __isoc99_sscanf(const char *s, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    if (!real_vsscanf)
        *(void **)&real_vsscanf = dlsym(dlopen("libc.so.6", 1), "vsscanf");
    int r = real_vsscanf(s, fmt, ap);
    va_end(ap);
    return r;
}

#include <sys/time.h>
static double now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static void *hX11, *hEGL, *hGLES;

static void *(*pXOpenDisplay)(const char *);
static int (*pXDefaultScreen)(void *);
static unsigned long (*pXRootWindow)(void *, int);
static unsigned long (*pXCreateSimpleWindow)(void *, unsigned long, int, int, int, int, int, unsigned long, unsigned long);
static int (*pXMapWindow)(void *, unsigned long);
static int (*pXStoreName)(void *, unsigned long, const char *);
static int (*pXSync)(void *, int);
static unsigned long (*pXInternAtom)(void *, const char *, int);
static int (*pXChangeProperty)(void *, unsigned long, unsigned long, unsigned long, int, int, const unsigned char *, int);
static int (*pXSendEvent)(void *, unsigned long, int, unsigned long, void *);
static int (*pXSelectInput)(void *, unsigned long, long);
static int (*pXPending)(void *);
static int (*pXNextEvent)(void *, void *);
static int (*pXFlush)(void *);

static void *eglD, *eglS, *eglC;
static unsigned (*peglInitialize)(void *, int *, int *);
static unsigned (*peglChooseConfig)(void *, const int *, void **, int, int *);
static void *(*peglCreateWindowSurface)(void *, void *, unsigned long, const int *);
static void *(*peglCreateContext)(void *, void *, void *, const int *);
static unsigned (*peglMakeCurrent)(void *, void *, void *, void *);
static unsigned (*peglSwapBuffers)(void *, void *);
static unsigned (*peglGetError)(void);

static void (*pglViewport)(int, int, int, int);
static void (*pglClearColor)(float, float, float, float);
static void (*pglClear)(unsigned);
static void (*pglMatrixMode)(unsigned);
static void (*pglLoadIdentity)(void);
static void (*pglFrustumf)(float, float, float, float, float, float);
static void (*pglOrthof)(float, float, float, float, float, float);
static void (*pglTranslatef)(float, float, float);
static void (*pglRotatef)(float, float, float, float);
static void (*pglEnable)(unsigned);
static void (*pglDisable)(unsigned);
static void (*pglVertexPointer)(int, unsigned, int, const void *);
static void (*pglColorPointer)(int, unsigned, int, const void *);
static void (*pglTexCoordPointer)(int, unsigned, int, const void *);
static void (*pglDrawArrays)(unsigned, int, int);
static void (*pglDrawElements)(unsigned, int, unsigned, const void *);
static void (*pglEnableClientState)(unsigned);
static void (*pglDisableClientState)(unsigned);
static void (*pglPointSize)(float);
static void (*pglGenTextures)(int, unsigned *);
static void (*pglBindTexture)(unsigned, unsigned);
static void (*pglTexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *);
static void (*pglTexParameteri)(unsigned, unsigned, int);
static void (*pglBlendFunc)(unsigned, unsigned);
static void (*pglScissor)(int, int, int, int);
static void (*pglGetIntegerv)(unsigned, int *);
static unsigned (*pglGetError)(void);

#define DLSYM(h, var, name) do { \
    *(void **)(&var) = dlsym(h, name); \
    if (!var) { printf("dlsym FAIL: %s\n", name); return 1; } \
} while (0)

typedef struct { void *visual; unsigned long visualid; int screen; unsigned int depth; int c_class; unsigned long red_mask, green_mask, blue_mask; int colormap_size; unsigned int bits_per_rgb; } XVisualInfo;
#define VisualIDMask 0x1
typedef unsigned long XColormap;
static void **(*pXGetVisualInfo)(void *, long, XVisualInfo *, int *);
static XColormap (*pXCreateColormap)(void *, unsigned long, void *, int);
static unsigned long (*pXCreateWindow)(void *, unsigned long, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, void *, unsigned long, void *);
#define EGL_NATIVE_VISUAL_ID 0x302E
#define EGL_SURFACE_TYPE 0x3033
#define EGL_WINDOW_BIT 0x0004
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_ES_BIT 0x0001
#define EGL_NONE 0x3038
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_DEPTH_SIZE 0x3025
#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_BLUE_SIZE 0x3022
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x100
#define GL_POINTS 0
#define GL_LINES 1
#define GL_TRIANGLES 4
#define GL_TRIANGLE_STRIP 5
typedef unsigned short GLushort;
typedef float GLfloat;
#define GL_PROJECTION 0x1701
#define GL_MODELVIEW 0x1700
#define GL_FLOAT 0x1406
#define GL_DEPTH_TEST 0x0B71
#define GL_VERTEX_ARRAY 0x8074
#define GL_COLOR_ARRAY 0x8076

#define XA_ATOM 4
#define XA_INTEGER 12
#define PropModeReplace 0
#define SubstructureNotifyMask (1L<<19)
/* glmemperf fullscreen recipe — see GLDEMO2_DEBUG.md */
static void set_fullscreen_props(void *xdpy, unsigned long w) {
    if (!pXInternAtom || !pXChangeProperty) return;
    unsigned long a_noncomp = pXInternAtom(xdpy, "_HILDON_NON_COMPOSITED_WINDOW", 0);
    unsigned long a_state = pXInternAtom(xdpy, "_NET_WM_STATE", 0);
    unsigned long a_fs = pXInternAtom(xdpy, "_NET_WM_STATE_FULLSCREEN", 0);
    int one = 1;
    if (a_noncomp)
        pXChangeProperty(xdpy, w, a_noncomp, XA_INTEGER, 32,
                         PropModeReplace, (unsigned char *)&one, 1);
    if (a_state && a_fs)
        pXChangeProperty(xdpy, w, a_state, XA_ATOM, 32, PropModeReplace,
                         (unsigned char *)&a_fs, 1);
}
static void send_fullscreen_msg(void *xdpy, unsigned long w) {
    unsigned long a_state, a_fs, root;
    if (!pXInternAtom || !pXSendEvent || !pXRootWindow) return;
    a_state = pXInternAtom(xdpy, "_NET_WM_STATE", 0);
    a_fs = pXInternAtom(xdpy, "_NET_WM_STATE_FULLSCREEN", 0);
    if (!a_state || !a_fs) return;
    {
        struct { int type; unsigned long serial, send_event, display, window;
                 int message_type, format;
                 long l[5]; } ev;
        memset(&ev, 0, sizeof ev);
        ev.type = 33;
        ev.window = w;
        ev.message_type = (int)a_state;
        ev.format = 32;
        ev.l[0] = 1;
        ev.l[1] = (long)a_fs;
        ev.l[2] = 0;
        root = pXRootWindow(xdpy, 0);
        pXSendEvent(xdpy, root, 0, SubstructureNotifyMask, &ev);
    }
}

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
    *(void **)(&pXInternAtom) = dlsym(hX11, "XInternAtom");
    *(void **)(&pXChangeProperty) = dlsym(hX11, "XChangeProperty");
    *(void **)(&pXSendEvent) = dlsym(hX11, "XSendEvent");
    *(void **)(&pXSelectInput) = dlsym(hX11, "XSelectInput");
    *(void **)(&pXPending) = dlsym(hX11, "XPending");
    *(void **)(&pXNextEvent) = dlsym(hX11, "XNextEvent");
    *(void **)(&pXFlush) = dlsym(hX11, "XFlush");
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
    DLSYM(hGLES, pglOrthof, "glOrthof");
    DLSYM(hGLES, pglTranslatef, "glTranslatef");
    DLSYM(hGLES, pglRotatef, "glRotatef");
    DLSYM(hGLES, pglEnable, "glEnable");
    DLSYM(hGLES, pglDisable, "glDisable");
    DLSYM(hGLES, pglVertexPointer, "glVertexPointer");
    DLSYM(hGLES, pglColorPointer, "glColorPointer");
    DLSYM(hGLES, pglTexCoordPointer, "glTexCoordPointer");
    DLSYM(hGLES, pglDrawArrays, "glDrawArrays");
    DLSYM(hGLES, pglDrawElements, "glDrawElements");
    DLSYM(hGLES, pglEnableClientState, "glEnableClientState");
    DLSYM(hGLES, pglDisableClientState, "glDisableClientState");
    *(void **)(&pglPointSize) = dlsym(hGLES, "glPointSize");
    DLSYM(hGLES, pglGenTextures, "glGenTextures");
    DLSYM(hGLES, pglBindTexture, "glBindTexture");
    DLSYM(hGLES, pglTexImage2D, "glTexImage2D");
    DLSYM(hGLES, pglTexParameteri, "glTexParameteri");
    DLSYM(hGLES, pglBlendFunc, "glBlendFunc");
    DLSYM(hGLES, pglScissor, "glScissor");
    DLSYM(hGLES, pglGetIntegerv, "glGetIntegerv");
    DLSYM(hGLES, pglGetError, "glGetError");
    return 0;
}

static unsigned long win;
static void *xdpy;

static int init_gfx(int w, int h) {
    if (load_syms()) return 1;
    xdpy = pXOpenDisplay(NULL);
    if (!xdpy) { printf("XOpenDisplay FAIL\n"); return 1; }
    int scr = pXDefaultScreen(xdpy);
    void *(*fnGetDisplay)(void *) = dlsym(hEGL, "eglGetDisplay");
    if (!fnGetDisplay) { printf("no eglGetDisplay\n"); return 1; }
    eglD = fnGetDisplay(NULL);
    if (!eglD) { printf("eglGetDisplay FAIL\n"); return 1; }
    if (!peglInitialize(eglD, NULL, NULL)) { printf("eglInitialize FAIL err=%x\n", peglGetError()); return 1; }
    static unsigned long (*pXVisualIDFromVisual)(void *);
    static void *(*pXDefaultVisual)(void *, int);
    if (!pXVisualIDFromVisual) {
        pXVisualIDFromVisual = dlsym(hX11, "XVisualIDFromVisual");
        pXDefaultVisual = dlsym(hX11, "XDefaultVisual");
    }
    unsigned long defvid = 0;
    if (pXVisualIDFromVisual && pXDefaultVisual)
        defvid = pXVisualIDFromVisual(pXDefaultVisual(xdpy, scr));
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
    unsigned (*fnGetConfigAttrib)(void*,void*,int,unsigned*) = dlsym(hEGL, "eglGetConfigAttrib");
    if (fnGetConfigAttrib) fnGetConfigAttrib(eglD, cfg, EGL_NATIVE_VISUAL_ID, (unsigned *)&visualid);
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
    if (got_vis && pvi) {
        XColormap cmap = pXCreateColormap(xdpy, pXRootWindow(xdpy, scr), pvi->visual, 0);
        unsigned long vmask = (1L<<13);
        unsigned long xattr[16] = {0};
        xattr[13] = (unsigned long)cmap;
        win = pXCreateWindow(xdpy, pXRootWindow(xdpy, scr), 0, 0, w, h, 0,
                             pvi->depth, 1, pvi->visual, vmask, xattr);
    } else {
        win = pXCreateSimpleWindow(xdpy, pXRootWindow(xdpy, scr), 0, 0, w, h, 0, 0, 0);
    }
    pXStoreName(xdpy, win, "GLDemo ImGui");
    set_fullscreen_props(xdpy, win);
    pXMapWindow(xdpy, win);
    pXSync(xdpy, 0);
    send_fullscreen_msg(xdpy, win);
    /* touchscreen = core X pointer: motion + button 1 */
    if (pXSelectInput)
        pXSelectInput(xdpy, win,
                      (1L<<2)/*ButtonPress*/ | (1L<<3)/*ButtonRelease*/ |
                      (1L<<6)/*PointerMotion*/ | (1L<<15)/*Exposure*/);
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
    printf("warm-up OK\n");
    return 0;
}

/* ============ imgui glue (defined in imgui_gles1.cpp) ============ */
typedef struct {
    void (*Enable)(unsigned);
    void (*Disable)(unsigned);
    void (*GenTextures)(int, unsigned *);
    void (*BindTexture)(unsigned, unsigned);
    void (*TexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *);
    void (*TexParameteri)(unsigned, unsigned, int);
    void (*BlendFunc)(unsigned, unsigned);
    void (*Scissor)(int, int, int, int);
    void (*Viewport)(int, int, int, int);
    void (*Orthof)(float, float, float, float, float, float);
    void (*MatrixMode)(unsigned);
    void (*LoadIdentity)(void);
    void (*VertexPointer)(int, unsigned, int, const void *);
    void (*TexCoordPointer)(int, unsigned, int, const void *);
    void (*ColorPointer)(int, unsigned, int, const void *);
    void (*DrawElements)(unsigned, int, unsigned, const void *);
    void (*DrawArrays)(unsigned, int, int);
    void (*GetIntegerv)(unsigned, int *);
    void (*EnableClientState)(unsigned);
    void (*GetErrorFn)(void);
} IGlfns;
extern int igui_init(int w, int h, IGlfns *fns);
extern void igui_newframe(float dt);
extern void igui_build_ui(double t, float fps);
extern void igui_render(void);
extern void igui_mouse_pos(float x, float y);
extern void igui_mouse_btn(int btn, int down);

/* ============ background: drifting colored cubes ============ */
#define W 800
#define H 480
static GLfloat cube_v[36 * 8];
static void build_cube(void) {
    static const float F[6][4] = {
        {0.5f,0.25f,0.55f,1},{0.25f,0.35f,0.6f,1},{0.3f,0.3f,0.5f,1},
        {0.45f,0.3f,0.5f,1},{0.35f,0.28f,0.55f,1},{0.3f,0.4f,0.62f,1}
    };
    static const float fc[6][4][3] = {
        {{-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1}},
        {{-1,-1,-1},{-1, 1,-1},{ 1, 1,-1},{ 1,-1,-1}},
        {{-1, 1,-1},{-1, 1, 1},{ 1, 1, 1},{ 1, 1,-1}},
        {{-1,-1,-1},{ 1,-1,-1},{ 1,-1, 1},{-1,-1, 1}},
        {{ 1,-1,-1},{ 1, 1,-1},{ 1, 1, 1},{ 1,-1, 1}},
        {{-1,-1,-1},{-1,-1, 1},{-1, 1, 1},{-1, 1,-1}}
    };
    for (int f = 0; f < 6; f++) {
        static const int ord[6] = {0, 1, 2, 0, 2, 3};
        for (int i = 0; i < 6; i++) {
            GLfloat *p = cube_v + (f * 6 + i) * 8;
            p[0] = fc[f][ord[i]][0]; p[1] = fc[f][ord[i]][1]; p[2] = fc[f][ord[i]][2];
            p[3] = F[f][0]; p[4] = F[f][1]; p[5] = F[f][2]; p[6] = F[f][3];
        }
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (init_gfx(W, H)) return 1;
    build_cube();
    IGlfns fns;
    memset(&fns, 0, sizeof fns);
    fns.Enable = (void (*)(unsigned))pglEnable;
    fns.Disable = (void (*)(unsigned))pglDisable;
    fns.GenTextures = pglGenTextures;
    fns.BindTexture = pglBindTexture;
    fns.TexImage2D = pglTexImage2D;
    fns.TexParameteri = pglTexParameteri;
    fns.BlendFunc = pglBlendFunc;
    fns.Scissor = pglScissor;
    fns.Viewport = pglViewport;
    fns.Orthof = pglOrthof;
    fns.MatrixMode = pglMatrixMode;
    fns.LoadIdentity = pglLoadIdentity;
    fns.VertexPointer = pglVertexPointer;
    fns.TexCoordPointer = pglTexCoordPointer;
    fns.ColorPointer = pglColorPointer;
    fns.DrawElements = pglDrawElements;
    fns.DrawArrays = pglDrawArrays;
    fns.GetIntegerv = pglGetIntegerv;
    fns.EnableClientState = pglEnableClientState;
    fns.GetErrorFn = pglGetError;
    if (igui_init(W, H, &fns)) return 1;
    printf("imgui init OK\n");

    double t0 = now(), t, last = t0;
    int frames = 0;
    double last_report = t0;
    int report_frames = 0;
    while (1) {
        /* pump X11 input -> ImGui */
        if (pXPending && pXNextEvent) {
            /* 32-bit Xlib event layout (core pointer events only) */
            typedef struct {
                int type; unsigned long serial, send_event, display;
                unsigned long window, root, subwindow, time;
                int x, y, x_root, y_root;
                unsigned int state, button;
            } XPtrEvent;
            XPtrEvent ev;
            while (pXPending(xdpy)) {
                pXNextEvent(xdpy, &ev);
                if (ev.type == 6 /*MotionNotify*/) {
                    igui_mouse_pos((float)ev.x, (float)ev.y);
                } else if (ev.type == 4 /*ButtonPress*/ || ev.type == 5 /*ButtonRelease*/) {
                    if (ev.button >= 1 && ev.button <= 3) {
                        /* press: report position FIRST — imgui's drag/undock
                           gesture needs a fresh pos at press time, and touch
                           sends no motion until the finger actually moves */
                        igui_mouse_pos((float)ev.x, (float)ev.y);
                        igui_mouse_btn((int)ev.button - 1, ev.type == 4);
                    }
                }
            }
        }
        double el = (t = now()) - t0;
        float fps = frames / el;
        float dt = (float)(t - last);
        if (dt > 0.25f || dt <= 0) dt = 0.033f;
        last = t;
        /* background: dark gradient clear + drifting cubes */
        pglDisable(GL_DEPTH_TEST);
        pglDisableClientState(GL_VERTEX_ARRAY);
        pglDisableClientState(GL_COLOR_ARRAY);
        pglClearColor(0.04f + 0.06f * (0.5f + 0.5f * sinf(el * 2.0f)),
                      0.05f, 0.10f, 1);
        pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        pglEnable(GL_DEPTH_TEST);
        pglMatrixMode(GL_PROJECTION);
        pglLoadIdentity();
        pglFrustumf(-1, 1, -0.6f, 0.6f, 1, 60);
        pglEnableClientState(GL_VERTEX_ARRAY);
        pglEnableClientState(GL_COLOR_ARRAY);
        pglVertexPointer(3, GL_FLOAT, 32, cube_v);
        pglColorPointer(4, GL_FLOAT, 32, cube_v + 3);
        for (int i = 0; i < 10; i++) {
            float a = el * 0.12f + i * 0.63f;
            float rr = 4 + 6 * ((i * 37 % 100) / 100.0f);
            pglMatrixMode(GL_MODELVIEW);
            pglLoadIdentity();
            pglTranslatef(rr * cosf(a), 2.5f * sinf(a * 1.7f + i), -6 - 14 * ((i * 53 % 100) / 100.0f));
            pglRotatef(el * 20 + i * 45, 1, 0.7f, 0.3f);
            pglDrawArrays(GL_TRIANGLES, 0, 36);
        }
        pglDisable(GL_DEPTH_TEST);
        pglDisableClientState(GL_VERTEX_ARRAY);
        pglDisableClientState(GL_COLOR_ARRAY);
        /* imgui pass: ortho y-down, identity modelview */
        pglMatrixMode(GL_PROJECTION);
        pglLoadIdentity();
        pglMatrixMode(GL_MODELVIEW);
        pglLoadIdentity();
        igui_newframe(dt);
        igui_build_ui(el, fps);
        igui_render();
        if (!peglSwapBuffers(eglD, eglS))
            printf("SWAP FAIL err=%x frame=%d\n", peglGetError(), frames);
        frames++;
        report_frames++;
        if (frames % 300 == 0) {
            unsigned gerr = pglGetError();
            printf("DBG f=%d t=%.2f el=%.2f sinf=%.3f cosf=%.3f gerr=%x\n",
                   frames, t, el, sinf((float)el), cosf((float)el), gerr);
        }
        if (t - last_report >= 30.0) {
            printf("RUNNING %.0fs: %d frames, %.1f FPS\n",
                   t - last_report, report_frames, report_frames / (t - last_report));
            last_report = t;
            report_frames = 0;
        }
    }
}
