/* gldemo_scene - N900 SGX530 GLES1.1 showcase
 * Scene 'u': multi-level 2D UI (title -> main menu -> settings -> confirm dialog),
 *            bitmap font, animated highlights, live 3D cube preview panel.
 * Scene 'g': 3D game flythrough (perspective + depth test, ground grid,
 *            orbiting cubes, enemy waves, starfield, HUD with score/FPS).
 * All graphics symbols resolved at runtime via dlopen/dlsym.
 * SGX quirk honored everywhere: color arrays are ALWAYS 4-component (RGBA).
 * Logs go to stderr; mode file is /home/user/gmode ('u' or 'g', default = both).
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
#define XA_ATOM 4
#define XA_INTEGER 12
#define PropModeReplace 0
#define SubstructureRedirectMask (1L<<20)
#define SubstructureNotifyMask (1L<<19)
/* Hildon fullscreen — verbatim recipe from Nokia's glmemperf native_x11.c:
   1) pre-map: _HILDON_NON_COMPOSITED_WINDOW=1 (disable compositor)
   2) pre-map: _NET_WM_STATE property = _NET_WM_STATE_FULLSCREEN
   3) AFTER XMapWindow + XFlush: _NET_WM_STATE_ADD ClientMessage to root
      with SubstructureNotifyMask — hildon-desktop then hides the
      statusbar/tasknavigator. Sending it pre-map does nothing. */
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
        ev.type = 33; /* ClientMessage */
        ev.window = w;
        ev.message_type = (int)a_state;
        ev.format = 32;
        ev.l[0] = 1;        /* _NET_WM_STATE_ADD */
        ev.l[1] = (long)a_fs;
        ev.l[2] = 0;
        root = pXRootWindow(xdpy, 0);
        pXSendEvent(xdpy, root, 0, SubstructureNotifyMask, &ev);
    }
}

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
#define EGL_BUFFER_SIZE 0x3020
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
typedef unsigned int GLenum;
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
    pXStoreName(xdpy, win, "GLDemo Scene");
    set_fullscreen_props(xdpy, win);
    pXMapWindow(xdpy, win);
    pXSync(xdpy, 0);
    send_fullscreen_msg(xdpy, win);  /* must come after map, per glmemperf */
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

/* ============ 3x5 bitmap font (rows top->bottom, bits: 4=left 2=mid 1=right) ============ */
static const unsigned short font3x5[] = {
    /* A*/ 2,5,7,5,5, /*B*/ 6,5,6,5,6, /*C*/ 3,4,4,4,3, /*D*/ 6,5,5,5,6,
    /*E*/ 7,4,6,4,7, /*F*/ 7,4,6,4,4, /*G*/ 3,4,5,5,3, /*H*/ 5,5,7,5,5,
    /*I*/ 7,2,2,2,7, /*J*/ 1,1,1,5,2, /*K*/ 5,5,6,5,5, /*L*/ 4,4,4,4,7,
    /*M*/ 5,7,5,5,5, /*N*/ 6,5,5,5,5, /*O*/ 2,5,5,5,2, /*P*/ 7,5,7,4,4,
    /*Q*/ 2,5,5,6,3, /*R*/ 6,5,6,5,5, /*S*/ 3,4,2,1,6, /*T*/ 7,2,2,2,2,
    /*U*/ 5,5,5,5,7, /*V*/ 5,5,5,5,2, /*W*/ 5,5,5,7,5, /*X*/ 5,5,2,5,5,
    /*Y*/ 5,5,2,2,2, /*Z*/ 7,1,2,4,7,
    /*0*/ 7,5,5,5,7, /*1*/ 2,6,2,2,7, /*2*/ 7,1,7,4,7, /*3*/ 7,1,7,1,7,
    /*4*/ 5,5,7,1,1, /*5*/ 7,4,7,1,7, /*6*/ 7,4,7,5,7, /*7*/ 7,1,2,2,2,
    /*8*/ 7,5,7,5,7, /*9*/ 7,5,7,1,7,
    /*:*/ 0,2,0,2,0, /*.*/ 0,0,0,0,2, /*-*/ 0,0,7,0,0, /*/*/ 1,1,2,4,4,
    /*(*/ 2,4,4,4,2, /*)*/ 2,1,1,1,2, /*%*/ 5,1,2,4,5, /*+*/ 0,2,7,2,0,
    /*space (offset 220)*/ 0,0,0,0,0
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
    return 220; /* space */
}

/* ============ dynamic 2D batch buffers ============ */
#define MAXQV 8192   /* quad verts (triangles) */
#define MAXPV 16384  /* point verts */
static GLfloat qv[MAXQV * 8], pv[MAXPV * 8];
static int nqv, npv;

static void batch_reset(void) { nqv = npv = 0; }

static void push_quad(float x, float y, float w, float h,
                      float r, float g, float b, float a) {
    if (nqv + 6 > MAXQV) return;
    GLfloat *p = qv + nqv * 8;
    float c0[4] = {r, g, b, a};
    float x1 = x + w, y1 = y + h;
    float xs[6] = {x, x1, x, x, x1, x1};
    float ys[6] = {y, y, y1, y1, y, y1};
    for (int i = 0; i < 6; i++) {
        p[i * 8] = xs[i]; p[i * 8 + 1] = ys[i]; p[i * 8 + 2] = 0;
        p[i * 8 + 3] = c0[0]; p[i * 8 + 4] = c0[1]; p[i * 8 + 5] = c0[2]; p[i * 8 + 6] = c0[3];
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

/* accumulate text as points; caller flushes */
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

/* ============ UI scene ============ */
#define W 800
#define H 480
static void draw_cube(float s, float t);
static int ui_screen(double t, double st, float fps) {
    /* st = seconds since screen start; returns next screen id when done */
    batch_reset();
    /* backdrop */
    push_quad(0, 0, W, H, 0.03f, 0.04f, 0.09f, 1);
    /* top bar */
    push_quad(0, 0, W, 34, 0.10f, 0.12f, 0.28f, 1);
    push_quad(0, 34, W, 2, 0.35f, 0.75f, 1.0f, 1);
    draw_text(12, 12, "MAEMO GL SCENE DEMO", 3, 0.55f, 0.85f, 1.0f, 1);
    {
        char l2[64];
        snprintf(l2, sizeof l2, "SGX530 - GLES 1.1 - FPS %.1f", fps);
        draw_text(560, 16, l2, 2, 0.45f, 0.65f, 0.9f, 1);
    }
    /* bottom bar */
    push_quad(0, H - 26, W, 26, 0.08f, 0.09f, 0.2f, 1);
    draw_text(12, H - 20, "NOKIA N900 - 2009 HARDWARE - 2026 TOOLCHAIN", 2, 0.4f, 0.5f, 0.75f, 1);

    int screen = (int)(st / 6) % 4; /* 4 sub-screens, 6s each */
    double ts = st - screen * 6;    /* time in sub-screen */
    float slide = ts < 0.4f ? (1.0f - ts / 0.4f) * 260.0f : 0.0f;

    if (screen == 0) {
        /* TITLE */
        float pulse = 0.6f + 0.4f * sinf(t * 3.0f);
        draw_text(150, 150, "GLDEMO SCENE", 12, 0.2f, pulse, 1.0f, 1);
        draw_text(230, 240, "SGX530 HARDWARE ACCELERATED", 4, 0.9f, 0.7f, 0.2f, 1);
        /* loading bar */
        float lw = 300.0f * fmodf(t, 3.0f) / 3.0f;
        push_quad(250, 300, 300, 14, 0.1f, 0.1f, 0.25f, 1);
        push_quad(250, 300, lw, 14, 0.2f, 0.9f, 0.4f, 1);
        draw_text(250, 330, "LOADING ASSETS", 2, 0.5f, 0.55f, 0.7f, 1);
    } else if (screen == 1 || screen == 3) {
        /* MAIN MENU (3 doubles as menu-under-dialog for confirm screen) */
        const char *items[5] = {"START GAME", "HIGH SCORES", "SETTINGS", "CREDITS", "QUIT"};
        int sel = (int)(t / 1.1f) % 5; /* auto-walking cursor */
        if (screen == 3) sel = 4;      /* dialog targets QUIT */
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
        /* live 3D cube preview panel (orth->frustum->orth round trip) */
        push_quad(430, 80, 320, 270, 0.05f, 0.07f, 0.16f, 1);
        push_quad(430, 80, 320, 2, 0.3f, 0.6f, 0.9f, 1);
        push_quad(430, 348, 320, 2, 0.3f, 0.6f, 0.9f, 1);
        draw_text(445, 320, "LIVE GPU PREVIEW", 2, 0.4f, 0.6f, 0.85f, 1);
        batch_flush();
        /* 3D preview inside panel */
        pglViewport(446, H - 342, 288, 250);
        pglClearColor(0.02f, 0.03f, 0.08f, 1);
        pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        pglMatrixMode(GL_PROJECTION);
        pglLoadIdentity();
        pglFrustumf(-1, 1, -0.87f, 0.87f, 1.5f, 20);
        pglMatrixMode(GL_MODELVIEW);
        pglLoadIdentity();
        pglTranslatef(0, 0, -5);
        pglRotatef(t * 40, 0.3f, 1, 0.2f);
        pglRotatef(t * 27, 0, 0, 1);
        draw_cube(1.6f, t);
        pglViewport(0, 0, W, H);
        pglMatrixMode(GL_PROJECTION);
        pglLoadIdentity();
        pglOrthof(0, W, H, 0, -1, 1);
        pglMatrixMode(GL_MODELVIEW);
        pglLoadIdentity();
        batch_reset();
    } else if (screen == 2) {
        /* SETTINGS with animated sliders */
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
        /* toggle row */
        float on = (fmodf(t, 4.0f) < 2.0f);
        draw_text(70, 360, "FULLSCREEN", 3, 0.6f, 0.7f, 0.9f, 1);
        push_quad(260, 352, 44, 24, 0.1f, 0.12f, 0.25f, 1);
        push_quad(on ? 282 : 262, 354, 20, 20, on ? 0.2f : 0.5f, on ? 0.95f : 0.3f, on ? 0.4f : 0.3f, 1);
        draw_text(320, 358, on ? "(ON)" : "(OFF)", 3, on ? 0.3f : 0.7f, on ? 0.9f : 0.5f, 0.4f, 1);
    }
    if (screen == 3) {
        /* confirmation dialog overlay */
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
    return screen;
}

/* ============ shared cube geometry ============ */
static GLfloat cube_v[36 * 8];
static void build_cube(void) {
    static const float F[6][4] = {
        {0.9f,0.25f,0.2f,1},{0.2f,0.9f,0.35f,1},{0.25f,0.45f,0.95f,1},
        {0.95f,0.85f,0.25f,1},{0.9f,0.4f,0.9f,1},{0.3f,0.9f,0.9f,1}
    };
    /* per face: 2 triangles of unit cube [-1,1] */
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
static void draw_cube(float s, float t) {
    pglScalef(s, s, s);
    pglEnableClientState(GL_VERTEX_ARRAY);
    pglEnableClientState(GL_COLOR_ARRAY);
    pglVertexPointer(3, GL_FLOAT, 32, cube_v);
    pglColorPointer(4, GL_FLOAT, 32, cube_v + 3);
    pglDrawArrays(GL_TRIANGLES, 0, 36);
    (void)t;
}

/* ============ 3D game scene ============ */
#define NENEMY 24
#define NSTAR 120
static GLfloat grid_v[84 * 2 * 8];
static int grid_n;
static GLfloat star_v[NSTAR * 8];
static void build_scene(void) {
    int n = 0;
    for (int i = -10; i <= 10; i += 2) {
        float c = 0.1f + 0.1f * (i == 0);
        float P[2][8] = {{i, 0, -50, 0.15f, 0.2f, c + 0.35f, 1},
                         {i, 0,  6, 0.15f, 0.2f, c + 0.35f, 1}};
        for (int k = 0; k < 2; k++) memcpy(grid_v + n * 8, P[k], 32), n++;
        float Q[2][8] = {{-20, 0, i * 2.6f - 30, 0.15f, 0.2f, c + 0.35f, 1},
                         { 20, 0, i * 2.6f - 30, 0.15f, 0.2f, c + 0.35f, 1}};
        for (int k = 0; k < 2; k++) memcpy(grid_v + n * 8, Q[k], 32), n++;
    }
    for (int i = 0; i < NSTAR; i++) {
        float x = (float)((i * 137) % 160 - 80);
        float y = 5 + (float)((i * 89) % 60);
        float z = -40 - (float)((i * 53) % 60);
        float b = 0.3f + 0.7f * ((i * 31 % 100) / 100.0f);
        GLfloat *p = star_v + i * 8;
        p[0] = x; p[1] = y; p[2] = z; p[3] = b; p[4] = b; p[5] = 0.9f; p[6] = 1;
    }
    grid_n = n;
}
static void scene_game(double t, float fps) {
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
    /* starfield + grid */
    pglEnableClientState(GL_VERTEX_ARRAY);
    pglEnableClientState(GL_COLOR_ARRAY);
    if (pglPointSize) pglPointSize(2);
    pglVertexPointer(3, GL_FLOAT, 32, star_v);
    pglColorPointer(4, GL_FLOAT, 32, star_v + 3);
    pglDrawArrays(GL_POINTS, 0, NSTAR);
    if (pglLineWidth) pglLineWidth(1.5f);
    pglVertexPointer(3, GL_FLOAT, 32, grid_v);
    pglColorPointer(4, GL_FLOAT, 32, grid_v + 3);
    pglDrawArrays(GL_LINES, 0, grid_n);
    /* orbiting cubes */
    for (int i = 0; i < 6; i++) {
        float a = t * (0.35f + i * 0.07f) + i * 1.05f;
        float r = 5 + 3.5f * sinf(t * 0.2f + i);
        pglMatrixMode(GL_MODELVIEW);
        pglLoadIdentity();
        pglTranslatef(0, -1.2f + 0.15f * sinf(t * 0.5f), 0);
        pglRotatef(4 * sinf(t * 0.3f), 0, 0, 1);
        pglTranslatef(r * cosf(a), 1.6f + 1.2f * sinf(t * 0.45f + i * 2), -14 - r * 0.5f + r * sinf(a) * 0.5f);
        pglRotatef(t * 50 + i * 60, 1, 0.6f, 0.2f);
        pglRotatef(t * 33, 0, 1, 0);
        draw_cube(0.9f, t);
    }
    /* enemy wave: spinning diamonds flying at the camera */
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
        /* diamond = 4 tris via 4 verts strip; use triangles: 2 tris */
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
    /* ---- HUD (2D overlay) ---- */
    pglDisable(GL_DEPTH_TEST);
    pglMatrixMode(GL_PROJECTION);
    pglLoadIdentity();
    pglOrthof(0, W, H, 0, -1, 1);
    pglMatrixMode(GL_MODELVIEW);
    pglLoadIdentity();
    batch_reset();
    /* crosshair */
    push_quad(W / 2 - 1, H / 2 - 12, 2, 24, 0.2f, 1, 0.3f, 1);
    push_quad(W / 2 - 12, H / 2 - 1, 24, 2, 0.2f, 1, 0.3f, 1);
    /* health + energy bars */
    push_quad(24, H - 60, 260, 12, 0.05f, 0.05f, 0.1f, 1);
    push_quad(24, H - 60, 260 * (0.6f + 0.35f * sinf(t)), 12, 0.9f, 0.2f, 0.15f, 1);
    push_quad(24, H - 42, 260, 8, 0.05f, 0.05f, 0.1f, 1);
    push_quad(24, H - 42, 260 * (0.5f + 0.45f * sinf(t * 1.7f)), 8, 0.2f, 0.5f, 1, 1);
    draw_text(24, H - 78, "HULL", 2, 1, 0.4f, 0.35f, 1);
    draw_text(24, H - 30, "SHIELD", 2, 0.4f, 0.6f, 1, 1);
    /* score + wave */
    {
        char b[96];
        snprintf(b, sizeof b, "SCORE %06d", (int)(t * 137) % 1000000);
        draw_text(24, 44, b, 4, 1, 0.9f, 0.3f, 1);
        snprintf(b, sizeof b, "WAVE %d - HOSTILES %d", 1 + (int)(t / 10) % 9, NENEMY);
        draw_text(24, 78, b, 3, 0.95f, 0.4f, 0.3f, 1);
        snprintf(b, sizeof b, "FPS %.1f", fps);
        draw_text(W - 150, 44, b, 3, 0.4f, 0.9f, 0.5f, 1);
    }
    batch_flush();
    pglEnable(GL_DEPTH_TEST);
}

/* ============ benchmark wrapper ============ */
static char bench_mode(void) {
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
    build_scene();
    char mode = bench_mode();
    double t0 = now();
    int frames = 0;
    float fps = 0;
    double t;
    double last_report = now();
    int report_frames = 0;
    /* run forever until killed; scene 'u'/'g' fixed, 'b' alternates every 10s */
    while (1) {
        double el = (t = now()) - t0;
        fps = frames / (t - t0);
        if (mode == 'u' || (mode == 'b' && (int)(el / 10) % 2 == 0)) {
            pglDisable(GL_DEPTH_TEST);
            pglViewport(0, 0, W, H);
            pglMatrixMode(GL_PROJECTION);
            pglLoadIdentity();
            pglOrthof(0, W, H, 0, -1, 1);
            pglMatrixMode(GL_MODELVIEW);
            pglLoadIdentity();
            pglClearColor(0, 0, 0, 1);
            pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            ui_screen(t - t0, t - t0, fps);
        } else {
            scene_game(t - t0, fps);
        }
        peglSwapBuffers(eglD, eglS);
        frames++;
        report_frames++;
        if (t - last_report >= 30.0) {
            printf("RUNNING %.0fs: %d frames, %.1f FPS\n",
                   t - last_report, report_frames, report_frames / (t - last_report));
            last_report = t;
            report_frames = 0;
        }
    }
}
