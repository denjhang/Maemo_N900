/* gldemo_showcase - N900 SGX530 GLES1.1 showcase, 7 scenes, infinite loop.
 * Scenes: logo / ui / tunnel / terrain / particles / spectrum / space
 * Mode: /home/user/gmode = scene key or 'b' (default, all scenes in a loop).
 * Runs forever until killed. Fullscreen via glmemperf recipe (see
 * GLDEMO2_DEBUG.md). All color arrays 4-component RGBA (SGX requirement).
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
static void (*pglScalef)(float, float, float);
static void (*pglEnable)(unsigned);
static void (*pglDisable)(unsigned);
static void (*pglVertexPointer)(int, unsigned, int, const void *);
static void (*pglColorPointer)(int, unsigned, int, const void *);
static void (*pglDrawArrays)(unsigned, int, int);
static void (*pglDrawElements)(unsigned, int, unsigned, const void *);
static void (*pglEnableClientState)(unsigned);
static void (*pglPointSize)(float);
static void (*pglLineWidth)(float);
static void (*pglPushMatrix)(void);
static void (*pglPopMatrix)(void);

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
#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_BLUE_SIZE 0x3022
#define EGL_BUFFER_SIZE 0x3020
#define EGL_DEPTH_SIZE 0x3025
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x100
#define GL_POINTS 0
#define GL_LINES 1
#define GL_TRIANGLES 4
#define GL_TRIANGLE_STRIP 5
typedef unsigned short GLushort;
typedef float GLfloat;
typedef unsigned int GLenum;
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
/* Hildon fullscreen — verbatim from Nokia glmemperf native_x11.c.
   XA_ATOM=4, XA_INTEGER=12 (wrong constants = silently ignored properties).
   ClientMessage must be sent AFTER XMapWindow. */
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
    DLSYM(hGLES, pglScalef, "glScalef");
    DLSYM(hGLES, pglEnable, "glEnable");
    DLSYM(hGLES, pglDisable, "glDisable");
    DLSYM(hGLES, pglVertexPointer, "glVertexPointer");
    DLSYM(hGLES, pglColorPointer, "glColorPointer");
    DLSYM(hGLES, pglDrawArrays, "glDrawArrays");
    *(void **)(&pglDrawElements) = dlsym(hGLES, "glDrawElements");
    DLSYM(hGLES, pglEnableClientState, "glEnableClientState");
    *(void **)(&pglPointSize) = dlsym(hGLES, "glPointSize");
    *(void **)(&pglLineWidth) = dlsym(hGLES, "glLineWidth");
    DLSYM(hGLES, pglPushMatrix, "glPushMatrix");
    DLSYM(hGLES, pglPopMatrix, "glPopMatrix");
    return 0;
}

static unsigned long win;

static int init_gfx(int w, int h) {
    if (load_syms()) return 1;
    void *xdpy = pXOpenDisplay(NULL);
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
        unsigned long vmask = (1L<<13) /*CWColormap*/;
        unsigned long xattr[16] = {0};
        xattr[13] = (unsigned long)cmap;
        win = pXCreateWindow(xdpy, pXRootWindow(xdpy, scr), 0, 0, w, h, 0,
                             pvi->depth, 1, pvi->visual, vmask, xattr);
    } else {
        win = pXCreateSimpleWindow(xdpy, pXRootWindow(xdpy, scr), 0, 0, w, h, 0, 0, 0);
    }
    pXStoreName(xdpy, win, "GLDemo Showcase");
    set_fullscreen_props(xdpy, win);
    pXMapWindow(xdpy, win);
    pXSync(xdpy, 0);
    send_fullscreen_msg(xdpy, win); /* must come after map (glmemperf) */
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

/* ============ 3x5 bitmap font ============ */
static const unsigned short font3x5[] = {
    2,5,7,5,5, 6,5,6,5,6, 3,4,4,4,3, 6,5,5,5,6,
    7,4,6,4,7, 7,4,6,4,4, 3,4,5,5,3, 5,5,7,5,5,
    7,2,2,2,7, 1,1,1,5,2, 5,5,6,5,5, 4,4,4,4,7,
    5,7,5,5,5, 6,5,5,5,5, 2,5,5,5,2, 7,5,7,4,4,
    2,5,5,6,3, 6,5,6,5,5, 3,4,2,1,6, 7,2,2,2,2,
    5,5,5,5,7, 5,5,5,5,2, 5,5,5,7,5, 5,5,2,5,5,
    5,5,2,2,2, 7,1,2,4,7,
    7,5,5,5,7, 2,6,2,2,7, 7,1,7,4,7, 7,1,7,1,7,
    5,5,7,1,1, 7,4,7,1,7, 7,4,7,5,7, 7,1,2,2,2,
    7,5,7,5,7, 7,5,7,1,7,
    0,2,0,2,0, 0,0,0,0,2, 0,0,7,0,0, 1,1,2,4,4,
    2,4,4,4,2, 2,1,1,1,2, 5,1,2,4,5, 0,2,7,2,0,
    0,0,0,0,0
};
static int font_idx(char c) {
    if (c >= 'A' && c <= 'Z') return (c - 'A') * 5;
    if (c >= 'a' && c <= 'z') return (c - 'a') * 5;
    if (c >= '0' && c <= '9') return 130 + (c - '0') * 5;
    if (c == ':') return 175;
    if (c == '.') return 180;
    if (c == '-') return 185;
    if (c == '/') return 190;
    if (c == '(') return 195;
    if (c == ')') return 200;
    if (c == '%') return 205;
    if (c == '+') return 210;
    return 220;
}

/* ============ dynamic 2D batch ============ */
#define MAXQV 12288
#define MAXPV 24576
#define W 800
#define H 480
static GLfloat qv[MAXQV * 8], pv[MAXPV * 8];
static int nqv, npv;
static void batch_reset(void) { nqv = npv = 0; }
static void push_quad(float x, float y, float w, float h,
                      float r, float g, float b, float a) {
    if (nqv + 6 > MAXQV) return;
    GLfloat *p = qv + nqv * 8;
    float x1 = x + w, y1 = y + h;
    float xs[6] = {x, x1, x, x, x1, x1};
    float ys[6] = {y, y, y1, y1, y, y1};
    for (int i = 0; i < 6; i++) {
        p[i * 8] = xs[i]; p[i * 8 + 1] = ys[i]; p[i * 8 + 2] = 0;
        p[i * 8 + 3] = r; p[i * 8 + 4] = g; p[i * 8 + 5] = b; p[i * 8 + 6] = a;
    }
    nqv += 6;
}
static void push_point(float x, float y, float z, float r, float g, float b, float a) {
    if (npv + 1 > MAXPV) return;
    GLfloat *p = pv + npv * 8;
    p[0] = x; p[1] = y; p[2] = z; p[3] = r; p[4] = g; p[5] = b; p[6] = a;
    npv++;
}
static void batch_flush(void) {
    if (nqv) {
        pglVertexPointer(3, GL_FLOAT, 32, qv);
        pglColorPointer(4, GL_FLOAT, 32, qv + 3);
        pglDrawArrays(GL_TRIANGLES, 0, nqv);
        nqv = 0;
    }
    if (npv) {
        if (pglPointSize) pglPointSize(2.0f);
        pglVertexPointer(3, GL_FLOAT, 32, pv);
        pglColorPointer(4, GL_FLOAT, 32, pv + 3);
        pglDrawArrays(GL_POINTS, 0, npv);
        npv = 0;
    }
}
static void draw_text(float x, float y, const char *s, float scale,
                      float r, float g, float b, float a) {
    float ox = x;
    for (; *s; s++) {
        if (*s == '\n') { ox = x; y += 6 * scale; continue; }
        const unsigned short *gl_ = font3x5 + font_idx(*s);
        for (int row = 0; row < 5; row++)
            for (int col = 0; col < 3; col++)
                if (gl_[row] & (4 >> col))
                    push_point(ox + col * scale + scale * 0.5f,
                               y + row * scale + scale * 0.5f, 0, r, g, b, a);
        ox += 4 * scale;
    }
}

/* ============ cube geometry ============ */
static GLfloat cube_v[36 * 8];
static void build_cube(void) {
    static const float F[6][4] = {
        {0.9f,0.25f,0.2f,1},{0.2f,0.9f,0.35f,1},{0.25f,0.45f,0.95f,1},
        {0.95f,0.85f,0.25f,1},{0.9f,0.4f,0.9f,1},{0.3f,0.9f,0.9f,1}
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
static void draw_cube(void) {
    pglEnableClientState(GL_VERTEX_ARRAY);
    pglEnableClientState(GL_COLOR_ARRAY);
    pglVertexPointer(3, GL_FLOAT, 32, cube_v);
    pglColorPointer(4, GL_FLOAT, 32, cube_v + 3);
    pglDrawArrays(GL_TRIANGLES, 0, 36);
}
/* stroked 3D box of half-extents (hx,hy,hz) at (x,y,z), color (r,g,b):
   recolor the shared cube in-place — cheap and keeps one geometry */
static GLfloat box_v[36 * 8];
static void draw_box(float x, float y, float z,
                     float hx, float hy, float hz,
                     float r, float g, float b) {
    memcpy(box_v, cube_v, sizeof box_v);
    for (int i = 0; i < 36; i++) {
        GLfloat *p = box_v + i * 8;
        p[0] *= hx; p[1] *= hy; p[2] *= hz;
        p[3] = r; p[4] = g; p[5] = b;
    }
    pglMatrixMode(GL_MODELVIEW);
    pglPushMatrix();
    pglTranslatef(x, y, z);
    pglEnableClientState(GL_VERTEX_ARRAY);
    pglEnableClientState(GL_COLOR_ARRAY);
    pglVertexPointer(3, GL_FLOAT, 32, box_v);
    pglColorPointer(4, GL_FLOAT, 32, box_v + 3);
    pglDrawArrays(GL_TRIANGLES, 0, 36);
    pglPopMatrix();
}

/* ============ scene 1: 3D N900 logo ============ */
/* LED-segment letters in the xz... use xy plane, unit cell = 1 */
/* each letter: list of boxes (cx, cy, w, h, rotz-deg) */
static const float LG_N[][5] = {{-1.0f,0,0.6f,4,0},{1.0f,0,0.6f,4,0},{0,0,3.2f,0.6f,-56}};
static const float LG_9[][5] = {{0,1.5f,2.4f,0.6f,0},{-1.2f,0.6f,0.6f,2.2f,0},{0,0,2.4f,0.6f,0},{1.2f,-0.9f,0.6f,2.2f,0},{0,-1.8f,2.4f,0.6f,0}};
static const float LG_0[][5] = {{0,1.5f,2.4f,0.6f,0},{-1.2f,0,0.6f,3.6f,0},{1.2f,0,0.6f,3.6f,0},{0,-1.5f,2.4f,0.6f,0}};
static void scene_logo(double t, float fps) {
    (void)fps;
    pglClearColor(0.01f, 0.01f, 0.03f, 1);
    pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    pglEnable(GL_DEPTH_TEST);
    pglMatrixMode(GL_PROJECTION);
    pglLoadIdentity();
    pglFrustumf(-1, 1, -0.6f, 0.6f, 1, 60);
    /* orbiting particles behind the logo (world space, drawn as points) */
    pglMatrixMode(GL_MODELVIEW);
    pglLoadIdentity();
    pglTranslatef(0, 0, -14);
    batch_reset();
    for (int i = 0; i < 160; i++) {
        float a = t * 0.5f + i * 0.39f;
        float rr = 7 + 3 * sinf(i * 1.7f);
        float zz = -6 + 10 * ((i * 61 % 97) / 97.0f);
        float hue = 0.5f + 0.5f * sinf(a * 2 + i);
        push_point(rr * cosf(a), rr * sinf(a) * 0.6f, zz,
                   0.2f + 0.3f * hue, 0.4f * hue, 0.9f, 1);
    }
    if (pglPointSize) pglPointSize(2);
    pglEnableClientState(GL_VERTEX_ARRAY);
    pglEnableClientState(GL_COLOR_ARRAY);
    batch_flush();
    /* logo letters: N 9 0 0, spinning in as a group */
    float spin = t * 40;
    float ease = t < 2 ? (t / 2) : 1;
    ease = ease * ease * (3 - 2 * ease); /* smoothstep */
    pglMatrixMode(GL_MODELVIEW);
    pglLoadIdentity();
    pglTranslatef(0, 0, -14 + 8 * ease);
    pglRotatef(spin * (1 - ease * 0.85f), 0, 1, 0);
    pglRotatef(10 * sinf(t * 0.7f), 0, 0, 1);
    float lx = -8.9f;
    float glow = 0.75f + 0.25f * sinf(t * 4);
    for (int i = 0; i < 3; i++)
            draw_box(lx + LG_N[i][0] * 1.0f, LG_N[i][1] * 1.0f, 0,
                     LG_N[i][2] * 0.5f, LG_N[i][3] * 0.5f, 0.35f,
                     0.2f, 0.85f * glow, 1);
    lx += 3.4f + 1.6f;
    for (int i = 0; i < 5; i++)
        draw_box(lx + LG_9[i][0], LG_9[i][1], 0,
                 LG_9[i][2] * 0.5f, LG_9[i][3] * 0.5f, 0.35f,
                 0.95f, 0.55f * glow, 0.15f);
    lx += 3.6f + 1.6f;
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < 4; i++)
            draw_box(lx + LG_0[i][0], LG_0[i][1], 0,
                     LG_0[i][2] * 0.5f, LG_0[i][3] * 0.5f, 0.35f,
                     0.2f, 0.9f * glow, 0.95f);
        lx += 3.6f + 1.6f;
    }
    pglDisable(GL_DEPTH_TEST);
    /* 2D overlay: scanline + title */
    pglMatrixMode(GL_PROJECTION);
    pglLoadIdentity();
    pglOrthof(0, W, H, 0, -1, 1);
    pglMatrixMode(GL_MODELVIEW);
    pglLoadIdentity();
    batch_reset();
    float sw = W * fmodf(t, 2.5f) / 2.5f;
    push_quad(0, 300, sw, 3, 0.2f, 0.9f, 1, 1);
    push_quad(sw - 40 > 0 ? sw - 40 : 0, 294, 40, 15, 0.2f, 0.9f, 1, 0.35f);
    {
        char b[64];
        snprintf(b, sizeof b, "BOOTING SGX530 - GPU POWERED - %d MS", (int)(t * 1000) % 10000);
        draw_text(20, 316, b, 2, 0.35f, 0.7f, 0.9f, 1);
    }
    batch_flush();
}

/* ============ scene 2: multi-level UI (from gldemo_scene) ============ */
static void ui_scene(double t, float fps);
static void draw_cube_centered(float s, double t) {
    pglMatrixMode(GL_MODELVIEW);
    pglLoadIdentity();
    pglTranslatef(0, 0, -5);
    pglRotatef(t * 40, 0.3f, 1, 0.2f);
    pglRotatef(t * 27, 0, 0, 1);
    pglScalef(s, s, s);
    draw_cube();
}

/* ============ scene 3: tunnel ============ */
#define NSEG 10
#define NRING 36
static GLfloat tun_v[NRING * NSEG * 6 * 8];
static void scene_tunnel(double t, float fps) {
    (void)fps;
    pglClearColor(0.02f, 0.01f, 0.04f, 1);
    pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    pglDisable(GL_DEPTH_TEST);
    pglMatrixMode(GL_PROJECTION);
    pglLoadIdentity();
    pglFrustumf(-0.7f, 0.7f, -0.42f, 0.42f, 0.5f, 90);
    pglMatrixMode(GL_MODELVIEW);
    pglLoadIdentity();
    pglEnableClientState(GL_VERTEX_ARRAY);
    pglEnableClientState(GL_COLOR_ARRAY);
    float scroll = fmodf(t * 6.0f, 3.0f);
    int n = 0;
    for (int i = 0; i < NRING; i++) {
        float z0 = -i * 3.0f + scroll;
        float z1 = z0 - 3.0f;
        float hue = fmodf((float)i / NRING + t * 0.15f, 1.0f);
        float r = 0.5f + 0.5f * sinf(hue * 6.28f);
        float g = 0.5f + 0.5f * sinf(hue * 6.28f + 2.1f);
        float b = 0.5f + 0.5f * sinf(hue * 6.28f + 4.2f);
        float dim0 = 1.0f - (float)i / NRING * 0.85f;
        float dim1 = 1.0f - ((float)i + 1) / NRING * 0.85f;
        float wob = 0.06f;
        for (int sgi = 0; sgi < NSEG; sgi++) {
            float a0 = (float)sgi / NSEG * 6.2832f + t * 0.6f;
            float a1 = (float)(sgi + 1) / NSEG * 6.2832f + t * 0.6f;
            float r00 = 2.6f + wob * 14 * sinf(a0 * 3 + z0 * 0.35f + t);
            float r10 = 2.6f + wob * 14 * sinf(a1 * 3 + z0 * 0.35f + t);
            float r01 = 2.6f + wob * 14 * sinf(a0 * 3 + z1 * 0.35f + t);
            float r11 = 2.6f + wob * 14 * sinf(a1 * 3 + z1 * 0.35f + t);
            float cx = 1.1f * sinf(z0 * 0.08f + t * 0.5f);
            float cy = 0.7f * cosf(z0 * 0.11f + t * 0.4f);
            float pts[4][2] = {
                {r00 * cosf(a0), r00 * sinf(a0)},
                {r10 * cosf(a1), r10 * sinf(a1)},
                {r11 * cosf(a1), r11 * sinf(a1)},
                {r01 * cosf(a0), r01 * sinf(a0)}
            };
            static const int ord[6] = {0, 1, 2, 0, 2, 3};
            for (int k = 0; k < 6; k++) {
                GLfloat *p = tun_v + n * 8;
                int v = ord[k];
                p[0] = pts[v][0] + cx;
                p[1] = pts[v][1] + cy;
                p[2] = (v < 2) ? z0 : z1;
                float d = (v == 0 || v == 1) ? dim0 : dim1;
                p[3] = r * d; p[4] = g * d; p[5] = b * d; p[6] = 1;
                n++;
            }
        }
    }
    pglVertexPointer(3, GL_FLOAT, 32, tun_v);
    pglColorPointer(4, GL_FLOAT, 32, tun_v + 3);
    pglDrawArrays(GL_TRIANGLES, 0, n);
}

/* ============ scene 4: lava planet flyover ============ */
#define TGX 44
#define TGN 30
static GLfloat terr_v[4096 * 8];
static float terr_h(float x, float z, double t) {
    return 1.1f * sinf(x * 0.5f + t * 0.7f)
         + 0.7f * sinf(z * 0.7f - t * 0.5f)
         + 0.45f * sinf((x + z) * 0.9f + t * 0.3f);
}
static void scene_terrain(double t, float fps) {
    (void)fps;
    pglClearColor(0.03f, 0.01f, 0.02f, 1);
    pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    pglEnable(GL_DEPTH_TEST);
    pglMatrixMode(GL_PROJECTION);
    pglLoadIdentity();
    pglFrustumf(-1, 1, -0.6f, 0.6f, 1, 140);
    pglMatrixMode(GL_MODELVIEW);
    pglLoadIdentity();
    pglRotatef(-24, 1, 0, 0);
    pglTranslatef(0, -2.4f, 0);
    pglEnableClientState(GL_VERTEX_ARRAY);
    pglEnableClientState(GL_COLOR_ARRAY);
    if (pglLineWidth) pglLineWidth(1.3f);
    int n = 0;
    for (int iz = 0; iz < TGN; iz++) {
        float z = -iz * 2.2f;
        for (int ix = 0; ix < TGX - 1; ix++) {
            float x0 = (ix - TGX / 2.0f) * 1.6f;
            float x1 = x0 + 1.6f;
            float h0 = terr_h(x0, z, t), h1 = terr_h(x1, z, t);
            GLfloat *p;
            p = terr_v + n * 8;
            p[0]=x0; p[1]=h0; p[2]=z;
            p[3]=0.9f; p[4]=0.25f+0.35f*h0; p[5]=0.1f+0.1f*h0; p[6]=1; n++;
            p = terr_v + n * 8;
            p[0]=x1; p[1]=h1; p[2]=z;
            p[3]=0.9f; p[4]=0.25f+0.35f*h1; p[5]=0.1f+0.1f*h1; p[6]=1; n++;
        }
    }
    for (int ix = 0; ix < TGX; ix += 3) {
        float x = (ix - TGX / 2.0f) * 1.6f;
        for (int iz = 0; iz < TGN - 1; iz++) {
            float z0 = -iz * 2.2f, z1 = -(iz + 1) * 2.2f;
            float h0 = terr_h(x, z0, t), h1 = terr_h(x, z1, t);
            GLfloat *p;
            p = terr_v + n * 8;
            p[0]=x; p[1]=h0; p[2]=z0;
            p[3]=0.5f; p[4]=0.2f+0.4f*h0; p[5]=0.6f; p[6]=1; n++;
            p = terr_v + n * 8;
            p[0]=x; p[1]=h1; p[2]=z1;
            p[3]=0.5f; p[4]=0.2f+0.4f*h1; p[5]=0.6f; p[6]=1; n++;
        }
    }
    pglVertexPointer(3, GL_FLOAT, 32, terr_v);
    pglColorPointer(4, GL_FLOAT, 32, terr_v + 3);
    pglDrawArrays(GL_LINES, 0, n);
    pglDisable(GL_DEPTH_TEST);
}

/* ============ scene 5: particle fountain ============ */
#define NPART 1000
static float px_[NPART], py_[NPART], pvx_[NPART], pvy_[NPART], page_[NPART];
static int pinit;
static void scene_particles(double t, float fps) {
    (void)fps;
    if (!pinit) {
        for (int i = 0; i < NPART; i++) {
            px_[i] = 0; py_[i] = 0;
            pvx_[i] = 0; pvy_[i] = 0; page_[i] = (float)(i % NPART) / NPART;
        }
        pinit = 1;
    }
    pglClearColor(0.01f, 0.02f, 0.05f, 1);
    pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    pglDisable(GL_DEPTH_TEST);
    /* ortho space: origin at bottom-center */
    pglMatrixMode(GL_PROJECTION);
    pglLoadIdentity();
    pglOrthof(-W / 2, W / 2, H, 0, -1, 1);
    pglMatrixMode(GL_MODELVIEW);
    pglLoadIdentity();
    static double last = 0;
    double now_ = t;
    float dt = (float)(now_ - last);
    if (dt > 0.1f || dt <= 0) dt = 0.033f;
    last = now_;
    batch_reset();
    for (int i = 0; i < NPART; i++) {
        page_[i] += dt * 0.5f;
        if (page_[i] >= 1) {
            /* respawn */
            page_[i] = 0;
            float a = -1.5708f + (((i * 137) % 100) / 100.0f - 0.5f) * 1.5f;
            float sp = 380 + ((i * 89) % 260);
            pvx_[i] = cosf(a) * sp;
            pvy_[i] = -sinf(a) * sp;
            px_[i] = 0; py_[i] = 0;
        }
        pvx_[i] *= 0.995f;
        pvy_[i] -= 320 * dt;
        px_[i] += pvx_[i] * dt;
        py_[i] += pvy_[i] * dt;
        if (py_[i] < 8) { py_[i] = 8; pvy_[i] = -pvy_[i] * 0.72f; }
        float hue = fmodf(page_[i] + t * 0.1f + (float)i / NPART, 1.0f);
        float r = 0.5f + 0.5f * sinf(hue * 6.28f);
        float g = 0.5f + 0.5f * sinf(hue * 6.28f + 2.1f);
        float b = 0.5f + 0.5f * sinf(hue * 6.28f + 4.2f);
        push_point(px_[i], py_[i], 0, r, g, b, 1);
    }
    if (pglPointSize) pglPointSize(2);
    pglEnableClientState(GL_VERTEX_ARRAY);
    pglEnableClientState(GL_COLOR_ARRAY);
    batch_flush();
    /* cannon barrel */
    batch_reset();
    push_quad(-16, 0, 32, 26, 0.35f, 0.38f, 0.45f, 1);
    batch_flush();
}

/* ============ scene 6: spectrum analyzer ============ */
#define NBARS 32
static float peaks[NBARS];
static void scene_spectrum(double t, float fps) {
    (void)fps;
    pglClearColor(0.02f, 0.02f, 0.04f, 1);
    pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    pglDisable(GL_DEPTH_TEST);
    pglMatrixMode(GL_PROJECTION);
    pglLoadIdentity();
    pglOrthof(0, W, H, 0, -1, 1);
    pglMatrixMode(GL_MODELVIEW);
    pglLoadIdentity();
    batch_reset();
    float bw = (W - 40.0f) / NBARS;
    float base = 300;
    for (int i = 0; i < NBARS; i++) {
        float x = 20 + i * bw;
        /* pseudo-FFT: sum of traveling sines + per-bar phase */
        float v = 0.5f + 0.28f * sinf(t * 2.3f + i * 0.55f)
                        + 0.18f * sinf(t * 3.7f - i * 0.31f)
                        + 0.12f * sinf(t * 5.1f + i * 0.83f);
        if (v < 0.05f) v = 0.05f;
        float bh = v * 190;
        float hue = (float)i / NBARS;
        float r = 0.5f + 0.5f * sinf(hue * 4.5f + t * 0.5f);
        float g = 0.5f + 0.5f * sinf(hue * 4.5f + 2.1f);
        float b = 0.5f + 0.5f * sinf(hue * 4.5f + 4.2f);
        push_quad(x + 1, base - bh, bw - 2, bh, r, g, b, 1);
        /* reflection */
        push_quad(x + 1, base + 4, bw - 2, bh * 0.35f, r * 0.25f, g * 0.25f, b * 0.25f, 1);
        /* falling peak marker */
        if (bh > peaks[i]) peaks[i] = bh; else peaks[i] -= 60.0f * 0.033f;
        if (peaks[i] < 0) peaks[i] = 0;
        push_quad(x + 1, base - peaks[i] - 5, bw - 2, 3, 1, 1, 1, 1);
    }
    /* baseline glow */
    push_quad(16, base, W - 32, 2, 0.4f, 0.8f, 1, 1);
    {
        char b2[64];
        snprintf(b2, sizeof b2, "SYNTH WAVES - 2 CH - %.0f BPM", 120 + 40 * sinf(t * 0.3f));
        draw_text(20, 322, b2, 2, 0.5f, 0.65f, 0.85f, 1);
    }
    batch_flush();
}

/* ============ scene 7: space battle (from gldemo_scene) ============ */
#define NENEMY 24
#define NSTAR 140
static GLfloat sgrid_v[84 * 2 * 8];
static int sgrid_n;
static GLfloat star_v[NSTAR * 8];
static int sinit;
static void build_space(void) {
    int n = 0;
    for (int i = -10; i <= 10; i += 2) {
        float c = 0.1f + 0.1f * (i == 0);
        GLfloat *p;
        p = sgrid_v + n * 8;
        p[0]=i; p[1]=0; p[2]=-50; p[3]=0.15f; p[4]=0.2f; p[5]=c+0.35f; p[6]=1; n++;
        p = sgrid_v + n * 8;
        p[0]=i; p[1]=0; p[2]=6;  p[3]=0.15f; p[4]=0.2f; p[5]=c+0.35f; p[6]=1; n++;
        p = sgrid_v + n * 8;
        p[0]=-20; p[1]=0; p[2]=i*2.6f-30; p[3]=0.15f; p[4]=0.2f; p[5]=c+0.35f; p[6]=1; n++;
        p = sgrid_v + n * 8;
        p[0]= 20; p[1]=0; p[2]=i*2.6f-30; p[3]=0.15f; p[4]=0.2f; p[5]=c+0.35f; p[6]=1; n++;
    }
    sgrid_n = n;
    for (int i = 0; i < NSTAR; i++) {
        GLfloat *p = star_v + i * 8;
        p[0] = (float)((i * 137) % 160 - 80);
        p[1] = 5 + (float)((i * 89) % 60);
        p[2] = -40 - (float)((i * 53) % 60);
        float b = 0.3f + 0.7f * ((i * 31 % 100) / 100.0f);
        p[3] = b; p[4] = b; p[5] = 0.9f; p[6] = 1;
    }
}
static void draw_ship_cube(float s, double t, int i) {
    pglRotatef(t * 50 + i * 60, 1, 0.6f, 0.2f);
    pglRotatef(t * 33, 0, 1, 0);
    pglScalef(s, s, s);
    draw_cube();
}
static void scene_space(double t, float fps) {
    pglClearColor(0.02f, 0.02f, 0.05f, 1);
    pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    pglEnable(GL_DEPTH_TEST);
    pglMatrixMode(GL_PROJECTION);
    pglLoadIdentity();
    pglFrustumf(-1, 1, -0.6f, 0.6f, 1, 120);
    pglMatrixMode(GL_MODELVIEW);
    pglLoadIdentity();
    pglTranslatef(0, -1.2f + 0.15f * sinf(t * 0.5f), 0);
    pglRotatef(4 * sinf(t * 0.3f), 0, 0, 1);
    pglEnableClientState(GL_VERTEX_ARRAY);
    pglEnableClientState(GL_COLOR_ARRAY);
    if (pglPointSize) pglPointSize(2);
    pglVertexPointer(3, GL_FLOAT, 32, star_v);
    pglColorPointer(4, GL_FLOAT, 32, star_v + 3);
    pglDrawArrays(GL_POINTS, 0, NSTAR);
    if (pglLineWidth) pglLineWidth(1.5f);
    pglVertexPointer(3, GL_FLOAT, 32, sgrid_v);
    pglColorPointer(4, GL_FLOAT, 32, sgrid_v + 3);
    pglDrawArrays(GL_LINES, 0, sgrid_n);
    for (int i = 0; i < 6; i++) {
        float a = t * (0.35f + i * 0.07f) + i * 1.05f;
        float r = 5 + 3.5f * sinf(t * 0.2f + i);
        pglMatrixMode(GL_MODELVIEW);
        pglLoadIdentity();
        pglTranslatef(0, -1.2f + 0.15f * sinf(t * 0.5f), 0);
        pglRotatef(4 * sinf(t * 0.3f), 0, 0, 1);
        pglTranslatef(r * cosf(a), 1.6f + 1.2f * sinf(t * 0.45f + i * 2), -14 - r * 0.5f + r * sinf(a) * 0.5f);
        draw_ship_cube(0.9f, t, i);
    }
    for (int i = 0; i < NENEMY; i++) {
        float ph = (float)i / NENEMY;
        float z = 8 + fmodf(t * (9 + 4 * ((i * 7 % 5) / 4.0f)) + ph * 70, 70);
        float x = 3.2f * sinf(i * 2.4f + t * 0.4f) * (z / 40.0f + 0.3f);
        float y = -0.6f + 1.4f * sinf(i * 1.7f + t * 0.6f);
        float gl_ = 0.5f + 0.5f * sinf(t * 5 + i);
        pglMatrixMode(GL_MODELVIEW);
        pglLoadIdentity();
        pglTranslatef(0, -1.2f, 0);
        pglTranslatef(x, y, -z);
        pglRotatef(t * 120 + i * 45, 0, 0, 1);
        GLfloat e[6 * 8];
        float c0[4] = {0.95f, 0.25f + 0.3f * gl_, 0.15f, 1};
        float vs[4][2] = {{0, 1.4f}, {1.4f, 0}, {0, -1.4f}, {-1.4f, 0}};
        static const int oi[6] = {0, 1, 2, 0, 2, 3};
        for (int k = 0; k < 6; k++) {
            e[k * 8] = vs[oi[k]][0]; e[k * 8 + 1] = vs[oi[k]][1]; e[k * 8 + 2] = 0;
            e[k * 8 + 3] = c0[0]; e[k * 8 + 4] = c0[1]; e[k * 8 + 5] = c0[2]; e[k * 8 + 6] = 1;
        }
        pglVertexPointer(3, GL_FLOAT, 32, e);
        pglColorPointer(4, GL_FLOAT, 32, e + 3);
        pglDrawArrays(GL_TRIANGLES, 0, 6);
    }
    /* HUD */
    pglDisable(GL_DEPTH_TEST);
    pglMatrixMode(GL_PROJECTION);
    pglLoadIdentity();
    pglOrthof(0, W, H, 0, -1, 1);
    pglMatrixMode(GL_MODELVIEW);
    pglLoadIdentity();
    batch_reset();
    push_quad(W / 2 - 1, H / 2 - 12, 2, 24, 0.2f, 1, 0.3f, 1);
    push_quad(W / 2 - 12, H / 2 - 1, 24, 2, 0.2f, 1, 0.3f, 1);
    push_quad(24, H - 60, 260, 12, 0.05f, 0.05f, 0.1f, 1);
    push_quad(24, H - 60, 260 * (0.6f + 0.35f * sinf(t)), 12, 0.9f, 0.2f, 0.15f, 1);
    push_quad(24, H - 42, 260, 8, 0.05f, 0.05f, 0.1f, 1);
    push_quad(24, H - 42, 260 * (0.5f + 0.45f * sinf(t * 1.7f)), 8, 0.2f, 0.5f, 1, 1);
    {
        char b[96];
        snprintf(b, sizeof b, "SCORE %06d", (int)(t * 137) % 1000000);
        draw_text(24, 44, b, 4, 1, 0.9f, 0.3f, 1);
        snprintf(b, sizeof b, "WAVE %d - HOSTILES %d", 1 + (int)(t / 10) % 9, NENEMY);
        draw_text(24, 78, b, 3, 0.95f, 0.4f, 0.3f, 1);
    }
    pglEnable(GL_DEPTH_TEST);
}

/* ============ scene 2 implementation ============ */
static void ui_scene(double t, float fps) {
    batch_reset();
    push_quad(0, 0, W, H, 0.03f, 0.04f, 0.09f, 1);
    push_quad(0, 0, W, 34, 0.10f, 0.12f, 0.28f, 1);
    push_quad(0, 34, W, 2, 0.35f, 0.75f, 1.0f, 1);
    draw_text(12, 12, "MAEMO GL SCENE DEMO", 3, 0.55f, 0.85f, 1.0f, 1);
    {
        char l2[64];
        snprintf(l2, sizeof l2, "SGX530 - GLES 1.1 - FPS %.1f", fps);
        draw_text(560, 16, l2, 2, 0.45f, 0.65f, 0.9f, 1);
    }
    push_quad(0, H - 26, W, 26, 0.08f, 0.09f, 0.2f, 1);
    draw_text(12, H - 20, "NOKIA N900 - 2009 HARDWARE - 2026 TOOLCHAIN", 2, 0.4f, 0.5f, 0.75f, 1);

    int screen = (int)(t / 6) % 4;
    double ts = t - screen * 6;
    float slide = ts < 0.4f ? (1.0f - ts / 0.4f) * 260.0f : 0.0f;

    if (screen == 0) {
        float pulse = 0.6f + 0.4f * sinf(t * 3.0f);
        draw_text(150, 150, "GLDEMO SCENE", 12, 0.2f, pulse, 1.0f, 1);
        draw_text(230, 240, "SGX530 HARDWARE ACCELERATED", 4, 0.9f, 0.7f, 0.2f, 1);
        float lw = 300.0f * fmodf(t, 3.0f) / 3.0f;
        push_quad(250, 300, 300, 14, 0.1f, 0.1f, 0.25f, 1);
        push_quad(250, 300, lw, 14, 0.2f, 0.9f, 0.4f, 1);
        draw_text(250, 330, "LOADING ASSETS", 2, 0.5f, 0.55f, 0.7f, 1);
    } else if (screen == 1 || screen == 3) {
        const char *items[5] = {"START GAME", "HIGH SCORES", "SETTINGS", "CREDITS", "QUIT"};
        int sel = (int)(t / 1.1f) % 5;
        if (screen == 3) sel = 4;
        for (int i = 0; i < 5; i++) {
            float y = 80 + i * 56 + (float)(i < sel ? slide : (i > sel ? -slide : 0));
            if (i == sel) {
                float gl_ = 0.5f + 0.3f * sinf(t * 6.0f);
                push_quad(60, y - 8, 300, 44, 0.15f, gl_, 0.45f, 1);
                push_quad(60, y - 8, 6, 44, 0.9f, 1.0f, 0.3f, 1);
                draw_text(84, y, items[i], 5, 0.05f, 0.08f, 0.12f, 1);
                draw_text(83, y - 2, items[i], 5, 1, 1, 0.4f, 1);
            } else {
                draw_text(84, y, items[i], 5, 0.55f, 0.65f, 0.85f, 1);
            }
        }
        push_quad(430, 80, 320, 270, 0.05f, 0.07f, 0.16f, 1);
        push_quad(430, 80, 320, 2, 0.3f, 0.6f, 0.9f, 1);
        push_quad(430, 348, 320, 2, 0.3f, 0.6f, 0.9f, 1);
        draw_text(445, 320, "LIVE GPU PREVIEW", 2, 0.4f, 0.6f, 0.85f, 1);
        batch_flush();
        pglViewport(446, H - 342, 288, 250);
        pglClearColor(0.02f, 0.03f, 0.08f, 1);
        pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        pglMatrixMode(GL_PROJECTION);
        pglLoadIdentity();
        pglFrustumf(-1, 1, -0.87f, 0.87f, 1.5f, 20);
        draw_cube_centered(1.6f, t);
        pglViewport(0, 0, W, H);
        pglMatrixMode(GL_PROJECTION);
        pglLoadIdentity();
        pglOrthof(0, W, H, 0, -1, 1);
        pglMatrixMode(GL_MODELVIEW);
        pglLoadIdentity();
        batch_reset();
    } else if (screen == 2) {
        draw_text(70, 60 + slide * 0.3f, "SETTINGS", 7, 0.9f, 0.75f, 0.25f, 1);
        const char *labels[3] = {"MUSIC VOL", "SFX VOL", "BRIGHTNESS"};
        for (int i = 0; i < 3; i++) {
            float y = 150 + i * 62;
            float v = 0.5f + 0.45f * sinf(t * (0.7f + i * 0.35f) + i * 2.1f);
            draw_text(70, y, labels[i], 3, 0.6f, 0.7f, 0.9f, 1);
            push_quad(260, y - 4, 360, 16, 0.08f, 0.09f, 0.2f, 1);
            push_quad(260, y - 4, 360 * v, 16, 0.25f + 0.5f * v, 0.8f, 0.35f, 1);
            push_quad(260 + 360 * v - 5, y - 9, 10, 26, 1, 1, 1, 1);
            {
                char b[16];
                snprintf(b, sizeof b, "%3d", (int)(v * 100));
                draw_text(650, y - 2, b, 3, 0.8f, 0.85f, 1, 1);
            }
        }
        float on = (fmodf(t, 4.0f) < 2.0f);
        draw_text(70, 360, "FULLSCREEN", 3, 0.6f, 0.7f, 0.9f, 1);
        push_quad(260, 352, 44, 24, 0.1f, 0.12f, 0.25f, 1);
        push_quad(on ? 282 : 262, 354, 20, 20, on ? 0.2f : 0.5f, on ? 0.95f : 0.3f, on ? 0.4f : 0.3f, 1);
        draw_text(320, 358, on ? "(ON)" : "(OFF)", 3, on ? 0.3f : 0.7f, on ? 0.9f : 0.5f, 0.4f, 1);
    }
    if (screen == 3) {
        push_quad(140, 130, 520, 220, 0, 0, 0, 1);
        push_quad(144, 134, 512, 212, 0.12f, 0.14f, 0.3f, 1);
        push_quad(144, 134, 512, 6, 0.95f, 0.4f, 0.2f, 1);
        draw_text(200, 170, "QUIT - ARE YOU SURE?", 5, 1, 0.85f, 0.3f, 1);
        draw_text(200, 230, "UNSAVED PROGRESS WILL BE LOST", 3, 0.7f, 0.7f, 0.8f, 1);
        int yes = (int)(t / 0.9f) % 2;
        push_quad(220, 280, 160, 44, yes ? 0.2f : 0.1f, yes ? 0.8f : 0.12f, yes ? 0.35f : 0.25f, 1);
        draw_text(268, 292, "YES", 4, 1, 1, 1, 1);
        push_quad(420, 280, 160, 44, yes ? 0.12f : 0.2f, yes ? 0.12f : 0.25f, yes ? 0.25f : 0.5f, 1);
        draw_text(472, 292, "NO", 4, 1, 1, 1, 1);
    }
    batch_flush();
}

/* ============ main loop ============ */
typedef void (*scene_fn)(double, float);
static const struct { char key; const char *name; scene_fn fn; } scenes[] = {
    {'l', "3D LOGO", scene_logo},
    {'u', "MENU UI", ui_scene},
    {'t', "TUNNEL", scene_tunnel},
    {'r', "TERRAIN", scene_terrain},
    {'p', "PARTICLES", scene_particles},
    {'s', "SPECTRUM", scene_spectrum},
    {'g', "SPACE", scene_space},
};
#define NSCENE 7
#define SCENE_SEC 12.0

static char read_mode(void) {
    char m = 'b';
    FILE *f = fopen("/home/user/gmode", "r");
    if (f) { m = (char)fgetc(f); fclose(f); }
    printf("mode=%c\n", m);
    return m;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (init_gfx(W, H)) return 1;
    build_cube();
    build_space();
    memset(peaks, 0, sizeof peaks);
    char mode = read_mode();
    int cur = 0;
    if (mode != 'b')
        for (int i = 0; i < NSCENE; i++) if (scenes[i].key == mode) cur = i;
    double t0 = now();
    double scene_t0 = t0;
    double last_report = t0;
    int frames = 0, report_frames = 0;
    double t;
    while (1) {
        double el = (t = now()) - t0;
        double st = t - scene_t0;
        if (st >= SCENE_SEC) {
            scene_t0 = t;
            st = 0;
            if (mode == 'b') cur = (cur + 1) % NSCENE;
        }
        float fps = frames / el;
        if (scenes[cur].fn == ui_scene || scenes[cur].fn == scene_space
            || scenes[cur].fn == scene_logo || scenes[cur].fn == scene_particles
            || scenes[cur].fn == scene_spectrum) {
            /* these manage their own projection; common setup here for ui/hud text */
        }
        scenes[cur].fn(st, fps);
        /* persistent HUD: top strip + scene name + FPS (2D ortho) */
        pglDisable(GL_DEPTH_TEST);
        pglMatrixMode(GL_PROJECTION);
        pglLoadIdentity();
        pglOrthof(0, W, H, 0, -1, 1);
        pglMatrixMode(GL_MODELVIEW);
        pglLoadIdentity();
        batch_reset();
        push_quad(0, 0, W, 22, 0.05f, 0.06f, 0.14f, 1);
        push_quad(0, 22, W, 1, 0.3f, 0.6f, 0.9f, 1);
        {
            char b[96];
            snprintf(b, sizeof b, "GLDEMO SHOWCASE - %s", scenes[cur].name);
            draw_text(10, 6, b, 2, 0.55f, 0.8f, 1.0f, 1);
            snprintf(b, sizeof b, "%.1F FPS", (double)fps);
            draw_text(W - 120, 6, b, 2, 0.4f, 0.9f, 0.5f, 1);
        }
        push_quad(0, H - 18, W, 18, 0.04f, 0.05f, 0.12f, 1);
        draw_text(10, H - 14, "N900 SGX530 GLES1.1 - NON-COMPOSITED FULLSCREEN", 2, 0.35f, 0.45f, 0.65f, 1);
        batch_flush();
        peglSwapBuffers(eglD, eglS);
        frames++;
        report_frames++;
        if (t - last_report >= 30.0) {
            printf("RUNNING %.0fs: %d frames, %.1f FPS, scene=%c\n",
                   t - last_report, report_frames, report_frames / (t - last_report),
                   scenes[cur].key);
            last_report = t;
            report_frames = 0;
        }
    }
}
