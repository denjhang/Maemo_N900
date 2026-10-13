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
static int (*pXLookupString)(void *, char *, int, void **, void *);
static unsigned long (*pXLookupKeysym)(void *, int);
static int (*pXSetInputFocus)(void *, unsigned long, int, unsigned long);
/* ALSA: sound output without any audio framework */
typedef struct _snd_pcm snd_pcm_t;
static void *hALSA;
static int (*snd_pcm_open)(snd_pcm_t **, const char *, int, int);
static int (*snd_pcm_set_params)(snd_pcm_t *, int, int, unsigned, unsigned, int, unsigned);
static long (*snd_pcm_writei)(snd_pcm_t *, const void *, unsigned long);
static int (*snd_pcm_close)(snd_pcm_t *);
static int (*snd_pcm_prepare)(snd_pcm_t *);
static int (*snd_pcm_drop)(snd_pcm_t *);
typedef struct snd_pcm_hw_params snd_pcm_hw_params_t;
typedef struct snd_pcm_sw_params snd_pcm_sw_params_t;
static int (*snd_pcm_hw_params_any)(snd_pcm_t *, snd_pcm_hw_params_t *);
static int (*snd_pcm_hw_params_set_access)(snd_pcm_t *, snd_pcm_hw_params_t *, int);
static int (*snd_pcm_hw_params_set_format)(snd_pcm_t *, snd_pcm_hw_params_t *, int);
static int (*snd_pcm_hw_params_set_channels)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned);
static int (*snd_pcm_hw_params_set_rate_near)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned *, int *);
static int (*snd_pcm_hw_params_set_period_size_near)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned long *, int *);
static int (*snd_pcm_hw_params_set_buffer_size_near)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned long *);
static int (*snd_pcm_hw_params)(snd_pcm_t *, snd_pcm_hw_params_t *);
static int (*snd_pcm_sw_params_current)(snd_pcm_t *, snd_pcm_sw_params_t *);
static int (*snd_pcm_sw_params_set_start_threshold)(snd_pcm_t *, snd_pcm_sw_params_t *, unsigned long);
static int (*snd_pcm_sw_params)(snd_pcm_t *, snd_pcm_sw_params_t *);
static int (*snd_pcm_state_fn)(snd_pcm_t *);
static long (*snd_pcm_avail_fn)(snd_pcm_t *);
static size_t (*snd_pcm_hw_params_sizeof_fn)(void);
static size_t (*snd_pcm_sw_params_sizeof_fn)(void);
static snd_pcm_t *pcm;
int audio_on;
int audio_amp = 16000;
int dbg_underruns = 0;
int dbg_key_events = 0;
/* wallpaper: 0=3D dynamic cubes 1=3D static 2=2D dynamic 3=2D static 4=plain
   variant: color palette 0..3 */
int g_wall_mode = 0, g_wall_variant = 0;
int audio_volume = 100;
int audio_wave = 0; /* 0 sine 1 tri 2 square 3 saw */ /* 0..1000 software gain (100=unity); >320 clamps hard, >1000 undefined */
static float audio_phase;
double audio_deadline = 0;
double dbg_max_stall = 0;
int dbg_stalls = 0;

/* ---- fine-grained audio trace: /home/user/audio_dbg.log ---- */
static FILE *alogf;
static double alog_t0;
static void alog(const char *fmt, ...) {
    if (!alogf) {
        alogf = fopen("/home/user/audio_dbg.log", "a");
        if (!alogf) return;
        setvbuf(alogf, NULL, _IONBF, 0); /* survive hangs: flush every line */
    }
    unsigned long ms = (unsigned long)((now() - alog_t0) * 1000.0);
    va_list ap;
    va_start(ap, fmt);
    fprintf(alogf, "[%6lu.%03lu] ", ms / 1000, ms % 1000);
    vfprintf(alogf, fmt, ap);
    fputc('\n', alogf);
    va_end(ap);
}
static void *hPTHREAD;
static int (*pthread_create_fn)(unsigned long *, void *, void *(*)(void *), void *);
static int (*pthread_join_fn)(unsigned long, void **);
static int (*usleep_fn)(unsigned int);
static int (*sched_setscheduler_fn)(int, int, void *);
static int (*setgid_fn)(unsigned int);
static int (*setuid_fn)(unsigned int);
static void *hOSSO;
static void *osso_ctx;
static void *(*osso_initialize)(const char *, const char *, int, void *);
static int (*osso_display_blanking_pause)(void *);
char last_key[64];
char hw[8][96];
int hw_n;
/* ring-buffer audio (SDL callback style) */
#define RB_FRAMES 32768
#define PERIOD 2048
static short rb[RB_FRAMES * 2];
static volatile unsigned rb_r, rb_w;
static volatile int thread_run;
static unsigned long audio_tid;

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
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
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
    *(void **)(&pXLookupString) = dlsym(hX11, "XLookupString");
    *(void **)(&pXLookupKeysym) = dlsym(hX11, "XLookupKeysym");
    *(void **)(&pXSetInputFocus) = dlsym(hX11, "XSetInputFocus");
    hALSA = dlopen("libasound.so.2", RTLD_NOW);
    if (hALSA) {
        snd_pcm_open = dlsym(hALSA, "snd_pcm_open");
        snd_pcm_set_params = dlsym(hALSA, "snd_pcm_set_params");
        snd_pcm_writei = dlsym(hALSA, "snd_pcm_writei");
        snd_pcm_close = dlsym(hALSA, "snd_pcm_close");
        snd_pcm_prepare = dlsym(hALSA, "snd_pcm_prepare");
        snd_pcm_drop = dlsym(hALSA, "snd_pcm_drop");
        snd_pcm_hw_params_any = dlsym(hALSA, "snd_pcm_hw_params_any");
        snd_pcm_hw_params_set_access = dlsym(hALSA, "snd_pcm_hw_params_set_access");
        snd_pcm_hw_params_set_format = dlsym(hALSA, "snd_pcm_hw_params_set_format");
        snd_pcm_hw_params_set_channels = dlsym(hALSA, "snd_pcm_hw_params_set_channels");
        snd_pcm_hw_params_set_rate_near = dlsym(hALSA, "snd_pcm_hw_params_set_rate_near");
        snd_pcm_hw_params_set_period_size_near = dlsym(hALSA, "snd_pcm_hw_params_set_period_size_near");
        snd_pcm_hw_params_set_buffer_size_near = dlsym(hALSA, "snd_pcm_hw_params_set_buffer_size_near");
        snd_pcm_hw_params = dlsym(hALSA, "snd_pcm_hw_params");
        snd_pcm_sw_params_current = dlsym(hALSA, "snd_pcm_sw_params_current");
        snd_pcm_sw_params_set_start_threshold = dlsym(hALSA, "snd_pcm_sw_params_set_start_threshold");
        snd_pcm_sw_params = dlsym(hALSA, "snd_pcm_sw_params");
        snd_pcm_state_fn = dlsym(hALSA, "snd_pcm_state");
        snd_pcm_avail_fn = dlsym(hALSA, "snd_pcm_avail_update");
        snd_pcm_hw_params_sizeof_fn = dlsym(hALSA, "snd_pcm_hw_params_sizeof");
        snd_pcm_sw_params_sizeof_fn = dlsym(hALSA, "snd_pcm_sw_params_sizeof");
        printf("ALSA symbols OK\n");
    } else printf("libasound not found: %s\n", dlerror());
    hPTHREAD = dlopen("libpthread.so.0", RTLD_NOW);
    if (hPTHREAD) {
        pthread_create_fn = dlsym(hPTHREAD, "pthread_create");
        pthread_join_fn = dlsym(hPTHREAD, "pthread_join");
        usleep_fn = dlsym(hPTHREAD, "usleep");
        printf("PTHREAD symbols OK\n");
    } else printf("libpthread not found: %s\n", dlerror());
    hOSSO = dlopen("libosso.so.1", RTLD_NOW);
    if (!hOSSO) hOSSO = dlopen("libosso.so.2", RTLD_NOW);
    if (hOSSO) {
        osso_initialize = dlsym(hOSSO, "osso_initialize");
        osso_display_blanking_pause = dlsym(hOSSO, "osso_display_blanking_pause");
        if (osso_initialize)
            osso_ctx = osso_initialize("com.nokia.gldemo", "1.0", 0, NULL);
        printf("OSSO %s\n", osso_ctx ? "OK (blanking pause armed)" : "ctx FAIL");
    } else printf("libosso not found\n");
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

#define W 800
#define H 480

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
    /* keyboard focus is grabbed from main loop (frame 10) - calling it here,
       mid fullscreen transition, raises BadMatch and Xlib's default error
       handler kills the process */
    /* touchscreen = core X pointer: motion + button 1 */
    if (pXSelectInput)
        pXSelectInput(xdpy, win,
                      (1L<<2)/*ButtonPress*/ | (1L<<3)/*ButtonRelease*/ |
                      (1L<<6)/*PointerMotion*/ | (1L<<15)/*Exposure*/ |
                      (1L<<0)/*KeyPress*/ | (1L<<1)/*KeyRelease*/);
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
extern void igui_key_char(const char *utf8);
extern void igui_special_key(unsigned ks, int down);
extern void igui_ctrl_combo(char c);
extern double audio_deadline;
extern double dbg_max_stall;
extern int dbg_stalls;
extern int audio_start(int seconds);
extern void vibra_ping(int ms);
extern void vibra_stop(void);
extern void ui_stress_toggle(void);

extern void igui_key_char(const char *utf8);

static void hw_sample(void) {
    hw_n = 0;
    FILE *f;
    char line[256];
    long mt = -1, mf = -1;
    if ((f = fopen("/proc/meminfo", "r"))) {
        while (fgets(line, sizeof line, f)) {
            if (!strncmp(line, "MemTotal:", 9)) sscanf(line + 9, "%ld", &mt);
            if (!strncmp(line, "MemFree:", 8)) sscanf(line + 8, "%ld", &mf);
        }
        fclose(f);
    }
    if (mt > 0) snprintf(hw[hw_n++], 96, "MEM: %ld/%ld kB free", mf, mt);
    long khz = -1;
    if ((f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r"))) {
        if (fgets(line, sizeof line, f)) sscanf(line, "%ld", &khz);
        fclose(f);
    }
    if (khz > 0) snprintf(hw[hw_n++], 96, "CPU: %ld MHz", khz / 1000);
    /* battery: N900 bq27200, several possible node names */
    const char *batt_paths[] = {
        "/sys/class/power_supply/bq27200-0/capacity",
        "/sys/class/power_supply/rx51-battery/capacity",
        "/sys/class/power_supply/battery/capacity", NULL };
    for (int i = 0; batt_paths[i]; i++) {
        if ((f = fopen(batt_paths[i], "r"))) {
            int cap = -1;
            if (fgets(line, sizeof line, f)) cap = atoi(line);
            fclose(f);
            if (cap >= 0) { snprintf(hw[hw_n++], 96, "BATTERY: %d%%", cap); break; }
        }
    }
    const char *ifaces[] = {"wlan0", "gprs0", "usb0", "lo", NULL};
    for (int i = 0; ifaces[i]; i++) {
        char p[128];
        snprintf(p, sizeof p, "/sys/class/net/%s/operstate", ifaces[i]);
        if ((f = fopen(p, "r"))) {
            char st[32] = "?";
            if (fgets(st, sizeof st, f)) st[strcspn(st, "\r\n")] = 0;
            fclose(f);
            snprintf(hw[hw_n++], 96, "NET %s: %s", ifaces[i], st);
        }
    }
    if (hw_n == 0) snprintf(hw[hw_n++], 96, "hw sample failed");
}

/* fork+exec in the UI thread stalls touch input on the 600MHz core -
   dbus-send runs in a throwaway thread, UI returns instantly */
static void *vibra_thread(void *p) {
    system((char *)p);
    free(p);
    return NULL;
}
static void vibra_async(const char *cmd) {
    if (!pthread_create_fn) { system(cmd); return; }
    char *copy = strdup(cmd);
    unsigned long tid;
    if (pthread_create_fn(&tid, NULL, vibra_thread, copy) != 0) {
        system(cmd);
        free(copy);
    }
}

void vibra_ping(int ms) {
    /* one-shot, self-terminating pattern with explicit deactivate right
       after the requested duration - never loops forever like the call
       pattern did */
    char cmd[256];
    snprintf(cmd, sizeof cmd,
             "(dbus-send --system --type=method_call --dest=com.nokia.mce "
             "/com/nokia/mce/request com.nokia.mce.request.req_vibrator_"
             "pattern_activate string:PatternChatAndEmail;"
             " sleep %d.%03d;"
             " dbus-send --system --type=method_call --dest=com.nokia.mce "
             "/com/nokia/mce/request com.nokia.mce.request.req_vibrator_"
             "pattern_deactivate string:PatternChatAndEmail) 2>/dev/null &",
             ms / 1000, ms % 1000);
    vibra_async(cmd);
}

void vibra_stop(void) {
    vibra_async("dbus-send --system --type=method_call --dest=com.nokia.mce "
                "/com/nokia/mce/request com.nokia.mce.request.req_vibrator_"
                "pattern_deactivate string:PatternIncomingCall 2>/dev/null;"
                "dbus-send --system --type=method_call --dest=com.nokia.mce "
                "/com/nokia/mce/request com.nokia.mce.request.req_vibrator_"
                "pattern_deactivate string:PatternChatAndEmail 2>/dev/null &");
}

void audio_toggle(int on) {
    /* PCM is opened once at startup and never closed - this only flips
       the logical on/off used by the UI */
    audio_on = on && pcm != NULL;
}

/* full hw/sw params sequence - AUTOTEST proved prepare alone after drop
   leaves plug-layer/hw pointers desynced (instant XRUN on next PLAY);
   AIC34 needs the complete re-setup (same quirk as v7 per-session params) */
int pcm_resetup(void) {
    if (!pcm || !snd_pcm_hw_params_any) return -1;
    /* opaque structs are bigger than they look - fixed 512B stack buffer
       smashed the guard (device-verified). size via libasound itself */
    size_t hsz = snd_pcm_hw_params_sizeof_fn ? snd_pcm_hw_params_sizeof_fn() : 512;
    size_t ssz = snd_pcm_sw_params_sizeof_fn ? snd_pcm_sw_params_sizeof_fn() : 128;
    snd_pcm_hw_params_t *hwp = __builtin_alloca(hsz);
    snd_pcm_sw_params_t *swp = __builtin_alloca(ssz);
    memset(hwp, 0, hsz);
    memset(swp, 0, ssz);
    alog("R resetup hsz=%zu ssz=%zu", hsz, ssz);
    int er = 0;
    er |= snd_pcm_hw_params_any(pcm, (snd_pcm_hw_params_t *)hwp) < 0;
    er |= snd_pcm_hw_params_set_access(pcm, (snd_pcm_hw_params_t *)hwp, 3) < 0;
    er |= snd_pcm_hw_params_set_format(pcm, (snd_pcm_hw_params_t *)hwp, 2) < 0;
    er |= snd_pcm_hw_params_set_channels(pcm, (snd_pcm_hw_params_t *)hwp, 2) < 0;
    unsigned rate = 44100; int dir = 0;
    er |= snd_pcm_hw_params_set_rate_near(pcm, (snd_pcm_hw_params_t *)hwp, &rate, &dir) < 0;
    unsigned long ps = 1024;
    er |= snd_pcm_hw_params_set_period_size_near(pcm, (snd_pcm_hw_params_t *)hwp, &ps, &dir) < 0;
    unsigned long bs = 32768;
    er |= snd_pcm_hw_params_set_buffer_size_near(pcm, (snd_pcm_hw_params_t *)hwp, &bs) < 0;
    er |= snd_pcm_hw_params(pcm, (snd_pcm_hw_params_t *)hwp) < 0;
    if (!er) {
        snd_pcm_sw_params_current(pcm, (snd_pcm_sw_params_t *)swp);
        snd_pcm_sw_params_set_start_threshold(pcm, (snd_pcm_sw_params_t *)swp, 1024);
        snd_pcm_sw_params(pcm, (snd_pcm_sw_params_t *)swp);
    }
    return er;
}

/* SDL-callback-style: dedicated writer thread drains a ring buffer, so UI
   frame hiccups can never underrun the device (PPSSPP/deadbeef pattern) */
static void *audio_writer(void *arg) {
    (void)arg;
    short chunk[PERIOD * 2];
    /* generator lives IN the writer thread: UI can stall forever,
       audio never notices (deadbeef output-thread pattern) */
    unsigned long seq = 0;
    double spawn_t = now();
    alog("T spawn tid=%lu", (unsigned long)arg);
    /* silence pad: DMA restart after drop+prepare has a broken-pointer window
       (AUTOTEST log: first writei = instant XRUN, -EPIPE storm replays stale
       ring content = the Play-click garbage). Pad 2 periods so the DAC only
       ever sees silence there. */
    {
        static const short zeros[PERIOD * 2];
        long z1 = snd_pcm_writei(pcm, zeros, PERIOD);
        long z2 = snd_pcm_writei(pcm, zeros, PERIOD);
        alog("T silence pad z1=%ld z2=%ld state=%d", z1, z2,
             snd_pcm_state_fn ? snd_pcm_state_fn(pcm) : -1);
    }
    while (thread_run) {
        double t_before = now();
        if (audio_deadline && now() > audio_deadline)
            break;
        for (unsigned i = 0; i < PERIOD; i++) {
            float ph = audio_phase / 6.2831853f; ph -= (float)(int)ph; /* 0..1 */
            float w;
            switch (audio_wave) {
            case 1: w = ph < 0.5f ? ph * 4.0f - 1.0f : 3.0f - ph * 4.0f; break; /* triangle */
            case 2: w = ph < 0.5f ? 1.0f : -1.0f; break;                        /* square */
            case 3: w = ph * 2.0f - 1.0f; break;                                 /* saw */
            default: w = sinf(audio_phase); break;                               /* sine */
            }
            float v = (float)audio_amp * (audio_volume / 100.0f) * w;
            short s = (short)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
            chunk[i * 2] = s;
            chunk[i * 2 + 1] = s;
            audio_phase += 2.0f * 3.14159265f * 440.0f / 44100.0f;
        }
        long w = snd_pcm_writei(pcm, chunk, PERIOD);
        long done = 0;
        while (w > 0 && done + w < PERIOD) {
            done += w; /* partial write: push the rest, else lost frames = crackle */
            w = snd_pcm_writei(pcm, chunk + done * 2, PERIOD - done);
        }
        if (w < 0 && snd_pcm_prepare) {
            alog("T ERR seq=%lu w=%ld state=%d -> prepare", seq, w,
                 snd_pcm_state_fn ? snd_pcm_state_fn(pcm) : -1);
            snd_pcm_prepare(pcm);
            w = snd_pcm_writei(pcm, chunk, PERIOD);
            if (w < 0) dbg_underruns++;
        }
        {
            double stall = now() - t_before; /* one period = ~46ms nominal */
            if (stall > dbg_max_stall) dbg_max_stall = stall;
            if (stall > 0.1) dbg_stalls++;
            alog("T seq=%lu w=%ld gen+write=%.1fms avail=%ld state=%d",
                 seq, w, stall * 1000.0,
                 snd_pcm_avail_fn ? snd_pcm_avail_fn(pcm) : -1L,
                 snd_pcm_state_fn ? snd_pcm_state_fn(pcm) : -1);
            if (seq == 0)
                alog("T FIRST-WRITE latency=%.1fms", (t_before - spawn_t) * 1000.0);
            seq++;
        }
    }
    alog("T exit seq=%lu", seq);
    return NULL;
}

void audio_stop_full(void);
int audio_start(int seconds) {
    if (!pcm || !pthread_create_fn) return 0;
    alog("S START req=%ds state=%d avail=%ld was_run=%d", seconds,
         snd_pcm_state_fn ? snd_pcm_state_fn(pcm) : -1,
         snd_pcm_avail_fn ? snd_pcm_avail_fn(pcm) : -1L, thread_run);
    if (thread_run) audio_stop_full();
    int pr = snd_pcm_prepare ? snd_pcm_prepare(pcm) : -999; /* fresh start each time */
    alog("S prepare=%d avail_after=%ld state=%d", pr,
         snd_pcm_avail_fn ? snd_pcm_avail_fn(pcm) : -1L,
         snd_pcm_state_fn ? snd_pcm_state_fn(pcm) : -1);
    audio_phase = 0;
    audio_deadline = seconds ? now() + seconds : 0;
    thread_run = 1;
    if (pthread_create_fn(&audio_tid, NULL, audio_writer, NULL) != 0) {
        thread_run = 0;
        alog("S spawn FAIL");
        return 0;
    }
    alog("S spawned");
    return 1;
}

void audio_stop_full(void) {
    double t0 = now();
    alog("P STOP entry run=%d state=%d avail=%ld", thread_run,
         snd_pcm_state_fn ? snd_pcm_state_fn(pcm) : -1,
         snd_pcm_avail_fn ? snd_pcm_avail_fn(pcm) : -1L);
    if (thread_run) {
        thread_run = 0;
        if (pthread_join_fn) pthread_join_fn(audio_tid, NULL);
        alog("P joined in %.1fms", (now() - t0) * 1000.0);
    }
    if (pcm && snd_pcm_drop) {
        snd_pcm_drop(pcm); /* discard residual: instant silence, no stale replay */
        /* full re-setup, not just prepare: plug-layer/hw pointers desync
           after drop, plain prepare = instant XRUN on next PLAY (logged) */
        int er = pcm_resetup();
        alog("P dropped+resetup(er=%d) after %.1fms state=%d", er,
             (now() - t0) * 1000.0,
             snd_pcm_state_fn ? snd_pcm_state_fn(pcm) : -1);
    }
    audio_deadline = 0;
    audio_on = 0;
}

/* ---- async control path: UI never blocks on join/ioctls ----
   stop path costs 50-150ms (join + drop + full resetup) - measured. Running
   it in the UI thread starved hildon's touch-vibra on every PLAY re-tap.
   UI only registers intent; this resident thread does the heavy work. */
static volatile int audio_cmd, audio_cmd_sec; /* 0 none, 1 start, 2 stop */
void audio_ui_start(int seconds) { audio_cmd_sec = seconds; audio_cmd = 1; }
void audio_ui_stop(void)         { audio_cmd = 2; }
void imrun_audio_stop_all(void)     { audio_cmd = 2; } /* window closed = stop */
double imrun_now(void)             { return now(); } /* plugin clock (relative) */
static void *audio_ctl(void *arg) {
    (void)arg;
    while (1) {
        if (usleep_fn) usleep_fn(10000);
        int cmd = audio_cmd;
        if (!cmd) continue;
        audio_cmd = 0;
        if (cmd == 2) {
            alog("C ctl: STOP");
            audio_stop_full();
        } else if (cmd == 1) {
            alog("C ctl: START %ds", audio_cmd_sec);
            audio_start(audio_cmd_sec);
        }
    }
    return NULL;
}
static unsigned long audio_ctl_tid;

/* battery % via HAL/BME (bq27200 is owned by bme daemon, no sysfs exposed).
   NOTE: HAL's battery.charge_level.percentage property is STALE (stuck at 76
   on device) - compute from charge bars current/design instead, those move.
   popen forks - runs in own thread, 30s cadence */
int g_batt_pct = -1;
static int hal_int(const char *key) {
    char cmd[160], b[32] = {0};
    snprintf(cmd, sizeof cmd, "hal-get-property --udi /org/freedesktop/Hal/devices/bme "
                              "--key %s 2>/dev/null", key);
    FILE *p = popen(cmd, "r");
    if (!p) return -1;
    int v = -1;
    if (fgets(b, sizeof b, p)) v = atoi(b);
    pclose(p);
    return v;
}
static void *batt_reader(void *arg) {
    (void)arg;
    while (1) {
        int cur = hal_int("battery.charge_level.current");
        int des = hal_int("battery.charge_level.design");
        if (cur >= 0 && des > 0) g_batt_pct = cur * 100 / des;
        if (usleep_fn) usleep_fn(30000000);
    }
    return NULL;
}



/* ============ background: drifting colored cubes ============ */
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
    /* real-time audio pattern: start as root with SCHED_RR, then drop to
       'user' (uid 29999) before opening X/audio. RT policy is inherited by
       the audio writer thread created later. */
    if (geteuid() == 0) {
        void *libc = dlopen("libc.so.6", RTLD_NOW);
        if (libc) {
            sched_setscheduler_fn = dlsym(libc, "sched_setscheduler");
            setgid_fn = dlsym(libc, "setgid");
            setuid_fn = dlsym(libc, "setuid");
        }
        if (sched_setscheduler_fn) {
            struct { int sched_priority; } sp = { 15 };
            sched_setscheduler_fn(0, 2 /*SCHED_RR*/, &sp);
        }
        if (setgid_fn) setgid_fn(29999);
        if (setuid_fn) setuid_fn(29999);
        printf("privdrop uid=%d\n", (int)getuid());
    }
    if (init_gfx(W, H)) return 1;
    /* grab the sound card ONCE, hold for entire app lifetime - pulse is
       on-demand (module-suspend-idle): after idle it releases hw, we take
       it and never let go. No open/close racing, no EBUSY intermittency. */
    if (snd_pcm_open) {
        if (snd_pcm_open(&pcm, "plughw:0,0", 0 /*PLAYBACK*/, 0) == 0) {
            int er = pcm_resetup();
            printf("AUDIO: hw held from startup (%s)\n", er ? "PARAMS FAIL" : "OK");
        } else if (snd_pcm_open(&pcm, "default", 0, 0) == 0) {
            snd_pcm_set_params(pcm, 2, 3, 2, 44100, 1, 1000000);
            printf("AUDIO: via pulse default (hw busy at boot)\n");
        } else {
            printf("AUDIO: no device\n");
        }
    }


    /* resident audio-control thread: PLAY/STOP buttons only set flags */
    if (pthread_create_fn && pcm)
        pthread_create_fn(&audio_ctl_tid, NULL, audio_ctl, NULL);
    /* battery monitor thread (forks hal-get-property, never in UI thread) */
    if (pthread_create_fn) {
        unsigned long bt;
        pthread_create_fn(&bt, NULL, batt_reader, NULL);
    }

    /* AUTOTEST: reproduce rapid Play/Stop garbage, log everything, exit.
       Runs headless right after PCM grab - no GUI interaction needed. */
    printf("AUTOTEST env=%s pcm=%p\n", getenv("AUTOTEST") ? getenv("AUTOTEST") : "(null)", (void *)pcm);
    /* file trigger is the reliable path: env var proved flaky through nohup */
    if (getenv("AUTOTEST") || access("/home/user/gautotest", 0) == 0) {
        unlink("/home/user/gautotest");
    if (pcm) {
        alog_t0 = now();
        alog("=== AUTOTEST begin pcm_open=OK amp=%d ===", audio_amp);
        audio_amp = 2500;
        for (int cyc = 0; cyc < 6; cyc++) {
            alog("--- cycle %d: PLAY ---", cyc);
            audio_start(30);
            usleep_fn(400000);  /* brief play, like a user tapping */
            alog("--- cycle %d: STOP ---", cyc);
            audio_stop_full();
            usleep_fn(150000);  /* quick re-tap */
        }
        alog("=== AUTOTEST end ===");
        printf("AUTOTEST done, log at /home/user/audio_dbg.log\n");
        return 0;
    }
    }


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

    int avtest = getenv("AVTEST") != NULL;
    double av_t0 = 0;
    int av_started = 0;
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
                } else if (ev.type == 2 /*KeyPress*/ || ev.type == 3 /*KeyRelease*/) {
                    char kb[16] = {0};
                    if (pXLookupString)
                        pXLookupString(&ev, kb, sizeof kb - 1, NULL, NULL);
                    dbg_key_events++;
                    /* X keycodes are authoritative on the N900 keyboard
                       (device-verified): 37=CTRL, 25=W, 22=BKSP, 36=ENTER.
                       Ctrl+letter turns into control bytes via XLookupString,
                       which the printable filter drops - combos are resolved
                       here from the base keysym instead. */
                    unsigned long ks = pXLookupKeysym ? pXLookupKeysym(&ev, 0) : 0;
                    unsigned kc = ev.button; /* XKeyEvent.keycode field */
                    int down = (ev.type == 2);
                    static int x_ctrl = 0;
                    if (kc == 37 || ks == 0xffe3 || ks == 0xffe4 || ks == 0xffe5) {
                        x_ctrl = down;
                        igui_special_key(0xffe3, down);
                    } else if ((ks & 0xff00) == 0xff00 || kc == 22 || kc == 36) {
                        unsigned k = (unsigned)ks;
                        if (kc == 22) k = 0xff08; /* BackSpace */
                        if (kc == 36) k = 0xff0d; /* Return */
                        igui_special_key(k, down);
                    } else if (x_ctrl && down && ks >= 'a' && ks <= 'z') {
                        igui_ctrl_combo((char)ks);
                    }
                    if (kb[0] >= 0x20 && (unsigned char)kb[0] < 0xf0 && !x_ctrl)
                        if (ev.type == 2) igui_key_char(kb);
                    /* KEYS log: keysym hex beats guessing from empty strings */
                    snprintf(last_key, sizeof last_key, "KEY %u %s '%s' ks=%lx",
                             (unsigned)ev.button, ev.type == 2 ? "DOWN" : "UP", kb, ks);
                }
            }
        }
        if (frames % 300 == 0) {
            unsigned gerr = pglGetError();
            printf("AUDIODBG max_stall=%.3fs stalls=%d underruns=%d\n", dbg_max_stall, dbg_stalls, dbg_underruns);
        }
        if (frames % 60 == 0) hw_sample();
        if (osso_display_blanking_pause && frames % 1500 == 0)
            osso_display_blanking_pause(osso_ctx);
        if (frames == 10 && pXSetInputFocus) {
            /* window fully mapped + fullscreen by now: focus grab is safe */
            pXSetInputFocus(xdpy, win, 1 /*RevertToParent*/, 0 /*CurrentTime*/);
            pXSync(xdpy, 0);
        }
        double el = (t = now()) - t0;
        if (avtest && !av_started && el > 2.5) {
            audio_amp = 2500; /* quiet: don't disturb the human */
            audio_ui_start(20);
            av_started = 1;
            av_t0 = el;
            printf("AVTEST tone started (amp=%d)\n", audio_amp);
        }
        if (avtest && av_started && el - av_t0 < 15 && frames % 45 == 0)
            ui_stress_toggle(); /* heavy: force glyph rebake every ~0.7s */
        float fps = frames / el;
        float dt = (float)(t - last);
        if (dt > 0.25f || dt <= 0) dt = 0.033f;
        last = t;
        /* ---- wallpaper engine: 3D/2D x dynamic/static, 4 palettes ---- */
        {
        float wel = (g_wall_mode == 1 || g_wall_mode == 3) ? 5.0f : el;
        float hue = g_wall_variant * 0.23f;
        float r0 = 0.05f + 0.5f * hue, g0 = 0.06f + 0.35f * (1 - hue), b0 = 0.12f;
        pglDisable(GL_DEPTH_TEST);
        pglDisableClientState(GL_VERTEX_ARRAY);
        pglDisableClientState(GL_COLOR_ARRAY);
        pglClearColor(0.03f + 0.04f * r0, 0.04f + 0.04f * g0, 0.07f + 0.06f * b0, 1);
        pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (g_wall_mode < 2) {
            /* 3D cubes (dynamic or frozen) */
            pglEnable(GL_DEPTH_TEST);
            pglMatrixMode(GL_PROJECTION);
            pglLoadIdentity();
            pglFrustumf(-1, 1, -0.6f, 0.6f, 1, 60);
            pglEnableClientState(GL_VERTEX_ARRAY);
            pglEnableClientState(GL_COLOR_ARRAY);
            pglVertexPointer(3, GL_FLOAT, 32, cube_v);
            pglColorPointer(4, GL_FLOAT, 32, cube_v + 3);
            for (int i = 0; i < 10; i++) {
                float a = wel * 0.12f + i * 0.63f;
                float rr = 4 + 6 * ((i * 37 % 100) / 100.0f);
                pglMatrixMode(GL_MODELVIEW);
                pglLoadIdentity();
                pglTranslatef(rr * cosf(a), 2.5f * sinf(a * 1.7f + i), -6 - 14 * ((i * 53 % 100) / 100.0f));
                pglRotatef(wel * 20 + i * 45, 1, 0.7f, 0.3f);
                pglDrawArrays(GL_TRIANGLES, 0, 36);
            }
            pglDisable(GL_DEPTH_TEST);
            pglDisableClientState(GL_VERTEX_ARRAY);
            pglDisableClientState(GL_COLOR_ARRAY);
        } else {
            /* 2D: drifting soft color quads (dynamic or frozen) */
            static float quad[6 * 8]; /* xyz rgba stride 8 */
            pglMatrixMode(GL_PROJECTION);
            pglLoadIdentity();
            pglOrthof(0, 800, 480, 0, -1, 1);
            pglEnableClientState(GL_VERTEX_ARRAY);
            pglEnableClientState(GL_COLOR_ARRAY);
            pglEnable(GL_BLEND);
            pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            pglVertexPointer(3, GL_FLOAT, 32, quad);
            pglColorPointer(4, GL_FLOAT, 32, quad + 3);
            for (int q = 0; q < 7; q++) {
                float ph = wel * (0.25f + 0.1f * q) + q * 1.7f;
                float cx = 400 + 330 * sinf(ph * 0.7f + q);
                float cy = 240 + 190 * cosf(ph * 0.9f + q * 2.3f);
                float sz = 130 + 60 * sinf(q * 2.1f);
                float cr = 0.5f + 0.5f * sinf(hue * 6.28f + q);
                float cg = 0.5f + 0.5f * sinf(hue * 6.28f + q + 2.1f);
                float cb = 0.5f + 0.5f * sinf(hue * 6.28f + q + 4.2f);
                float c[6][8] = {
                    {cx - sz, cy - sz, 0, cr, cg, cb, 0.16f, 0},
                    {cx + sz, cy - sz, 0, cr, cg, cb, 0.16f, 0},
                    {cx + sz, cy + sz, 0, cr, cg, cb, 0.16f, 0},
                    {cx - sz, cy - sz, 0, cr, cg, cb, 0.16f, 0},
                    {cx + sz, cy + sz, 0, cr, cg, cb, 0.16f, 0},
                    {cx - sz, cy + sz, 0, cr, cg, cb, 0.16f, 0}};
                for (int k = 0; k < 48; k++) ((float *)quad)[k] = c[k / 8][k % 8];
                pglDrawArrays(GL_TRIANGLES, 0, 6);
            }
            pglDisable(GL_BLEND);
            pglDisableClientState(GL_VERTEX_ARRAY);
            pglDisableClientState(GL_COLOR_ARRAY);
        }
        }
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
