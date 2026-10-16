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
__attribute__((visibility("hidden"))) void *__dso_handle = (void *)&__dso_handle; /* -nostartfiles: no crtbegin */

typedef int (*vsscanf_fn)(const char *, const char *, va_list);
static vsscanf_fn real_vsscanf;
int __isoc99_sscanf(const char *s, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    if (!real_vsscanf)
        *(void **)&real_vsscanf = dlsym(dlopen("libc.so.6", 1), "vsscanf");
    if (!real_vsscanf) { va_end(ap); return 0; }   /* never deref NULL */
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
static int (*pXSetErrorHandler)(int (*)(void *, void *), int (*)(void *, void *));
static void alog(const char *fmt, ...);
static int imrun_xerror(void *disp, void *ev) {   /* Xlib exit()s by default */
    alog("[XERROR] X protocol error swallowed (default handler would exit)");
    return 0;
}

void imrun_atexit_log(void) {
    alog("LIFECYCLE exit() called (no signal) - XERROR lines above = Xlib death");
}
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
long audio_flow = 0;   /* frames the active backend has actually produced this session;
                          the TONEGEN panel shows this ticking up = sound is flowing */
/* wallpaper: 0=3D dynamic cubes 1=3D static 2=2D dynamic 3=2D static 4=plain
   variant: color palette 0..3 */
int g_wall_mode = 0, g_wall_variant = 0;
int audio_volume = 100;
int audio_wave = 0; /* 0 sine 1 tri 2 square 3 saw */ /* 0..1000 software gain (100=unity); >320 clamps hard, >1000 undefined */
int audio_backend = 2; /* 默认 SDL (走 ALSA plughw, 稳定). 0=ALSA 1=libao 2=SDL 3=PULSE(paplay/sink.hw0) */
int (*audio_source_fn)(short *dst, int frames) = 0; /* plugin VGM feed: ret 1=continue, 0=EOF */
int audio_rate = 44100; /* gen_samples 相位步进用; PULSE 后端设 48000 (sink.hw0 原生率) */
/* libpulse-simple 后端 (v0.4.46+, paplay 风格) - 见 audio_pulse.c */
extern int  audio_pulse_open(int rate, int channels);
extern int  audio_pulse_write(const short *buf, int frames);
extern int  audio_pulse_can_write(int max_latency_us);
extern unsigned long audio_pulse_latency(void);
extern int  audio_pulse_alive(void);
extern void audio_pulse_close(void);
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
void alog_public(const char *fmt, ...) {
    if (!alogf) {
        alogf = fopen("/home/user/audio_dbg.log", "a");
        if (!alogf) return;
        setvbuf(alogf, NULL, _IONBF, 0);
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
int g_temp = -1;   /* exported to shell sysbar (thread-fed) */
double g_wall_t = 0;   /* wallpaper clock: fed by own thread, monotonic -
                          UI stalls and wall-clock jumps can't touch it */
static double mono_now(void) {
    static int (*cgt)(int, void *);
    if (!cgt) {
        void *h = dlopen("librt.so.1", 1);
        if (!h) h = dlopen("libc.so.6", 1);
        if (h) *(void **)(&cgt) = dlsym(h, "clock_gettime");
    }
    if (cgt) {
        struct { long sec, nsec; } ts;
        if (cgt(1 /*CLOCK_MONOTONIC*/, &ts) == 0)
            return (double)ts.sec + (double)ts.nsec / 1e9;
    }
    return now(); /* fallback */
}
static void *wall_thread(void *arg) {
    (void)arg;
    for (;;) {
        g_wall_t = mono_now();
        if (usleep_fn) usleep_fn(4000);   /* 250Hz: smooth, ~zero cost */
    }
    return NULL;
}
static void *hw_thread(void *arg) {
    (void)arg;
    void (*sam)(void) = (void (*)(void))arg;
    while (1) {
        sam();  /* /proc+/sys reads: blocking flash I/O, NEVER in UI thread */
        {
            FILE *f = fopen("/sys/class/hwmon/hwmon0/device/temp1_input", "r");
            if (f) { char ln[64]; if (fgets(ln, sizeof ln, f)) g_temp = atoi(ln); fclose(f); }
        }
        if (usleep_fn) usleep_fn(10000000);
    }
    return NULL;
}
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
void (*pglDeleteTextures)(unsigned, const unsigned *);
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
    *(void **)(&pXSetErrorHandler) = dlsym(hX11, "XSetErrorHandler");
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
    DLSYM(hGLES, pglDeleteTextures, "glDeleteTextures");
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

/* ================= libao backend (ported from vgmplay-maemo) ==============
   vgmplay-maemo builds with USE_LIBAO=1: ao_open_live(default driver) +
   blocking ao_play. On this device that lands on pulseaudio, whose internal
   queue is ~seconds deep - that is WHY whole tracks played without a hiccup
   while our direct plughw path XRUNs on every UI stall. Verbatim call
   sequence, runtime dlopen (zero link-time deps). */
typedef struct { int bits; int rate; int channels; int byte_format; } ao_fmt_t;
/* libao ao_option is a 3-pointer linked list: {const char *key; const char
   *value; struct ao_option *next} (see research/libao-src .../include/ao/ao.h).
   We hand-build it to avoid depending on ao_append_option (the device-stock
   11KB mini libao does NOT export it). */
typedef struct { const char *key; const char *value; void *next; } ao_option_link_t;
/* alsa09 routes "default" through /etc/asound.conf -> type pulse -> system-mode
   pulse that refuses user(29999). Bypass it with a direct ALSA hw device, the
   vgmplay-era path that always worked. Single-node list: libao walks
   options->next until NULL, and each node's key MUST be non-NULL (alsa09 does
   strcmp(key,"dev") with no guard) - a dummy {NULL,...} sentinel would crash it. */
static ao_option_link_t ao_dev_opt_plughw = { "dev", "plughw:0,0", NULL };
static void *hAO;
static int (*ao_initialize_fn)(void);
static int (*ao_default_driver_id_fn)(void);
static void *(*ao_open_live_fn)(int, ao_fmt_t *, void *);
static int (*ao_play_fn)(void *, char *, unsigned int);
static void (*ao_close_fn)(void *);
static void *ao_dev;
static int ao_open(void) {
    if (!hAO) {
        /* audiolib/ is the deployment slot for the full 61KB dlopen-enabled
           libao (research/libao-src .../src/.libs/libao.so.2.1.3) + its
           plugins dir /opt/libao/lib/ao/plugins-2. The device-stock
           /opt/libao/lib/libao.so.2 is an 11KB mini libao with NO alsa plugin
           loader and NO ao_append_option - do not rely on it. */
        static const char *libs[] = { "/home/user/imrun/audiolib/libao.so.2",
                                      "/home/user/imrun/lib/libao.so.2",
                                      "/opt/libao/lib/libao.so.2", "libao.so.2",
                                      "/usr/lib/libao.so.2", NULL };
        for (int i = 0; libs[i] && !hAO; i++)
            hAO = dlopen(libs[i], 2 /*RTLD_NOW*/);
        if (!hAO) { alog("AO: libao.so.2 not found"); return 0; }
        ao_initialize_fn = dlsym(hAO, "ao_initialize");
        ao_default_driver_id_fn = dlsym(hAO, "ao_default_driver_id");
        ao_open_live_fn = dlsym(hAO, "ao_open_live");
        ao_play_fn = dlsym(hAO, "ao_play");
        ao_close_fn = dlsym(hAO, "ao_close");
        if (!ao_initialize_fn || !ao_default_driver_id_fn || !ao_open_live_fn ||
            !ao_play_fn || !ao_close_fn) {
            alog("AO: missing symbols"); hAO = NULL; return 0;
        }
        ao_initialize_fn();
    }
    ao_fmt_t fmt = { 16, 44100, 2, 4 /*AO_FMT_NATIVE little-endian*/ };
    ao_dev = ao_open_live_fn(ao_default_driver_id_fn(), &fmt, &ao_dev_opt_plughw);
    alog("AO open_live dev=%p %s", ao_dev, ao_dev ? "OK" : "FAIL");
    return ao_dev != NULL;
}
/* ================= SDL 1.2 audio backend (ported from ppsspp-n900) ========
   PPSSPP native/base/PCMain.cpp, verbatim: SDL_Init(AUDIO) ->
   SDL_OpenAudio(44100/S16/2ch/samples=2048, callback) -> SDL_PauseAudio(0).
   SDL's internal thread paces the callback; on Maemo SDL's audio driver
   talks to the same pulse layer. mixaudio = NativeMix analog = gen_samples. */
static void gen_samples(short *dst, int n);   /* fwd: def sits below */
typedef struct { int freq; unsigned short format; unsigned char channels,
                 silence; unsigned short samples; unsigned int size;
                 void (*callback)(void *, unsigned char *, int);
                 void *userdata; } SDL_AudioSpec_t;
static void *hSDL;
static int (*SDL_Init_fn)(unsigned);
static int (*SDL_OpenAudio_fn)(SDL_AudioSpec_t *, SDL_AudioSpec_t *);
static void (*SDL_PauseAudio_fn)(int);
static void (*SDL_CloseAudio_fn)(void);
static int sdl_audio_open;
static unsigned long sdl_cb_count;   /* SDL internal-thread callback count: proof the tone is actually being pulled */
static void sdl_mixaudio(void *userdata, unsigned char *stream, int len) {
    (void)userdata;
    sdl_cb_count++;
    audio_flow += len / 4;   /* stereo 16-bit frames produced */
    gen_samples((short *)stream, len / 4);   /* == NativeMix(stream, len/4) */
}
static int sdl_open(void) {
    if (!hSDL) {
        hSDL = dlopen("libSDL-1.2.so.0", 2 /*RTLD_NOW*/);
        if (!hSDL) { alog("SDL: libSDL-1.2.so.0 not found"); return 0; }
        SDL_Init_fn = dlsym(hSDL, "SDL_Init");
        SDL_OpenAudio_fn = dlsym(hSDL, "SDL_OpenAudio");
        SDL_PauseAudio_fn = dlsym(hSDL, "SDL_PauseAudio");
        SDL_CloseAudio_fn = dlsym(hSDL, "SDL_CloseAudio");
        if (!SDL_Init_fn || !SDL_OpenAudio_fn || !SDL_PauseAudio_fn ||
            !SDL_CloseAudio_fn) { alog("SDL: missing symbols"); hSDL = NULL; return 0; }
    }
    /* Pin the pulse driver. The ALSA driver opens "default" which /etc/asound.conf
       routes to pulse anyway, but SDL's own pulse driver keeps the server-side
       deep queue that absorbs UI stalls (the whole point of this backend). If
       we leave SDL to auto-pick it could silently fall to a different driver on
       a device-state change and go mute - pin it explicitly. */
    setenv("SDL_AUDIODRIVER", "pulse", 1);

    if (SDL_Init_fn(0x10 /*SDL_INIT_AUDIO*/) < 0) {
        alog("SDL_Init AUDIO fail"); return 0;
    }
    SDL_AudioSpec_t fmt;
    fmt.freq = 44100;
    fmt.format = 0x8010 /*AUDIO_S16SYS*/;
    fmt.channels = 2;
    fmt.samples = 2048;
    fmt.callback = &sdl_mixaudio;
    fmt.userdata = (void *)0;
    if (SDL_OpenAudio_fn(&fmt, NULL) < 0) {
        alog("SDL_OpenAudio fail"); return 0;
    }
    /* Audio must be unpaused after init (PPSSPP comment, kept) */
    SDL_PauseAudio_fn(0);
    sdl_audio_open = 1;
    alog("SDL audio opened 44100/S16/2ch/2048");
    return 1;
}
static void sdl_close(void) {
    if (!sdl_audio_open) return;
    alog("SDL close cb_count=%lu", sdl_cb_count);
    SDL_PauseAudio_fn(1);
    SDL_CloseAudio_fn();
    sdl_audio_open = 0;
    alog("SDL audio closed");
}
/* shared tone generator (both backends render the same samples).
   sinf() is soft-float on the N900 and far too slow inside the hot loop
   (log: gen+write 40-70ms > 46ms period -> EPIPE/XRUN storm = the
   "crackle/half-second then silence"). Pre-computed wavetable kills that. */
#define WT_SIZE 1024
static float wavetable_sine[WT_SIZE];
static int wavetable_ready = 0;
static void wavetable_init(void) {
    for (int i = 0; i < WT_SIZE; i++)
        wavetable_sine[i] = sinf(2.0f * 3.14159265f * (float)i / (float)WT_SIZE);
    wavetable_ready = 1;
}
static void gen_samples(short *dst, int n) {
    if (!wavetable_ready) wavetable_init();
    /* phase step in wavetable slots per sample: 440Hz over the ACTIVE rate.
       audio_rate = 44100 (三后端) or 48000 (PULSE sink.hw0 原生, 免重采样) */
    double step = (double)WT_SIZE * 440.0 / (double)audio_rate;
    for (int i = 0; i < n; i++) {
        int idx = (int)audio_phase;
        float f = audio_phase - (float)idx;
        float w = wavetable_sine[idx & (WT_SIZE - 1)] * (1.0f - f) +
                  wavetable_sine[(idx + 1) & (WT_SIZE - 1)] * f;
        if (audio_wave == 1) {          /* triangle */
            float p = (float)(idx & (WT_SIZE - 1)) / (float)WT_SIZE;
            w = p < 0.5f ? p * 4.0f - 1.0f : 3.0f - p * 4.0f;
        } else if (audio_wave == 2) {   /* square */
            w = (idx & (WT_SIZE >> 1)) ? 1.0f : -1.0f;
        } else if (audio_wave == 3) {   /* saw */
            float p = (float)(idx & (WT_SIZE - 1)) / (float)WT_SIZE;
            w = p * 2.0f - 1.0f;
        }
        float v = (float)audio_amp * (audio_volume / 100.0f) * w;
        short s = (short)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
        dst[i * 2] = s;
        dst[i * 2 + 1] = s;
        audio_phase += (float)step;
        if (audio_phase >= (float)WT_SIZE) audio_phase -= (float)WT_SIZE;
    }
}

/* SDL-callback-style: dedicated writer thread drains a ring buffer, so UI
   frame hiccups can never underrun the device (PPSSPP/deadbeef pattern) */
static void *audio_writer(void *arg) {
    (void)arg;
    if (audio_backend == 1) {
        /* vgmplay-maemo path: blocking ao_play, the server-side queue does
           ALL the buffering. Zero error recovery (vgmplay has none). */
        short frag[512 * 2];
        unsigned long seq = 0;
        while (thread_run) {
            if (audio_deadline && now() > audio_deadline) { audio_on = 0; break; }
            if (audio_source_fn) {
                if (!audio_source_fn(frag, 512)) { audio_on = 0; break; } /* EOF */
            } else gen_samples(frag, 512);
            audio_flow += 512;
            if (!ao_play_fn(ao_dev, (char *)frag, sizeof(frag))) {
                alog("AO play FAIL seq=%lu", seq);
                break;
            }
            seq++;
        }
        alog("T ao exit seq=%lu", seq);
        return NULL;
    }
    if (audio_backend == 3) {
        /* libpulse-simple path (v0.4.46+): paplay 风格, 字节流入 pulse sink.
           pulse server 自动应用 sink volume (Maemo 系统栏/硬件键/amixer
           改 sink vol 时输出立即变). pa_simple_write 是阻塞的, 与
           snd_pcm_writei 类似; 块大小 PERIOD=2048 帧 ~46ms. */
        short chunk[PERIOD * 2];
        unsigned long seq = 0;
        double spawn_t = now();
        alog("T pulse spawn tid=%lu", (unsigned long)arg);
        /* pa_simple 非线程安全: open/write/close 全在 writer 线程内.
           audio_start 不碰 pa_simple, 只 spawn 这里.
           rate=48000: sink.hw0 原生 48000 (system.pa rate=48000), 零重采样.
           44100 流会被 pulse server 重采样, 600MHz CPU 顶不住 -> 噼啪声. */
        if (!audio_pulse_open(48000, 2)) {
            alog("T pulse open FAIL");
            audio_on = 0;
            return NULL;
        }
        /* silence pad: 与 plughw 路径同样的"首块 XRUN"防御 */
        static const short zeros[PERIOD * 2];
        if (!audio_pulse_write(zeros, PERIOD)) {
            alog("T pulse silence pad FAIL");
            audio_on = 0;
            return NULL;
        }
        if (!audio_pulse_write(zeros, PERIOD)) {
            alog("T pulse silence pad2 FAIL");
            audio_on = 0;
            return NULL;
        }
        while (thread_run) {
            double t_before = now();
            if (audio_deadline && now() > audio_deadline) {
                thread_run = 0; audio_on = 0;
                break;
            }
            if (!thread_run) break;  /* STOP 信号: 不再 write, 直接退出 */
            if (audio_source_fn) {
                if (!audio_source_fn(chunk, PERIOD)) { thread_run = 0; audio_on = 0; break; }
            } else gen_samples(chunk, PERIOD);
            audio_flow += PERIOD;
            /* pa_simple_write 是同步阻塞 API: 正常播放时每块阻塞 ~46ms
               (正好是播放节奏). 初始填充时第一块可能阻塞 ~1.4s (server
               端缓冲填满后等消费). 这是 pa_simple 的固有行为, 不是死锁.
               STOP 时最多多等一个 write 周期 (~46ms 稳态 / ~1.4s 初始). */
            if (!audio_pulse_write(chunk, PERIOD)) {
                alog("T pulse write FAIL seq=%lu", seq);
                break;
            }
            {
                double stall = now() - t_before;
                if (stall > dbg_max_stall) dbg_max_stall = stall;
                if (stall > 0.1) dbg_stalls++;
                alog("T pulse seq=%lu gen+write=%.1fms", seq, stall * 1000.0);
                if (seq == 0)
                    alog("T pulse FIRST-WRITE latency=%.1fms", (t_before - spawn_t) * 1000.0);
                seq++;
            }
        }
pulse_exit:
        alog("T pulse exit seq=%lu", seq);
        audio_pulse_close();   /* writer 自己关: pa_simple 非线程安全, 所有操作
                                  必须在同一线程内. 避免 audio-ctl 并发 flush/free
                                  导致 libpulse 内部死锁. */
        return NULL;
    }
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
        if (audio_deadline && now() > audio_deadline) {
            thread_run = 0; audio_on = 0;   /* truthful state for UI */
            break;
        }
        if (audio_source_fn) {
            if (!audio_source_fn(chunk, PERIOD)) { thread_run = 0; audio_on = 0; break; } /* EOF */
        } else gen_samples(chunk, PERIOD);
        audio_flow += PERIOD;
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
/* (re)acquire the ALSA hw hold - boot, and every time we come back from
   the OSS backend which had to release it */
static int pcm_acquire(void) {
    if (pcm || !snd_pcm_open) return pcm != NULL;
    if (snd_pcm_open(&pcm, "plughw:0,0", 0 /*PLAYBACK*/, 0) == 0) {
        int er = pcm_resetup();
        printf("AUDIO: hw held (%s)\n", er ? "PARAMS FAIL" : "OK");
        return 1;
    }
    if (snd_pcm_open(&pcm, "default", 0, 0) == 0) {
        snd_pcm_set_params(pcm, 2, 3, 2, 44100, 1, 1000000);
        printf("AUDIO: via pulse default (hw busy)\n");
        return 1;
    }
    printf("AUDIO: no device\n");
    return 0;
}
int audio_start(int seconds) {
    if (!pthread_create_fn) return 0;
    /* SDL(ppsspp) backend reuses the proven ALSA plughw writer path. Its
       native pulse callback path is broken on this device under load:
       audio_dbg.log shows "SDL close cb_count=0" (callback thread never ran)
       then SDL_CloseAudio hangs, freezing the resident control thread so the
       radio buttons can't switch. The dlopen/符号 skeleton below stays for
       reference, but sound goes through plughw:0,0 like the ALSA backend. */
    if (audio_backend == 1) {
        /* libao backend: release the ALSA hw hold so the ao server path
           (pulse on this device) can wake up and take the card */
        if (pcm) { snd_pcm_close(pcm); pcm = NULL; alog("AO: alsa hold released"); }
        if (ao_dev) { if (ao_close_fn) ao_close_fn(ao_dev); ao_dev = NULL; }
        sdl_close();   /* leaving SDL backend? its device must not linger */
        if (!ao_dev && !ao_open()) return 0;
        alog("S AO START req=%ds", seconds);
        audio_phase = 0;
        audio_deadline = seconds ? now() + seconds : 0;
        thread_run = 1;
        if (pthread_create_fn(&audio_tid, NULL, audio_writer, NULL) != 0) {
            thread_run = 0;
            return 0;
        }
        audio_on = 1;
        return 1;
    }
    /* backend 0 (ALSA) and backend 2 (SDL/PSP) both go through ALSA plughw */
    if (audio_backend == 3) {
        /* libpulse-simple (v0.4.46+, paplay 风格).
           pa_simple 非线程安全: open/write/close 必须全在 writer 线程内.
           audio_start 只释放 ALSA hold + spawn writer, 不碰 pa_simple.
           audio_rate=48000: sink.hw0 原生率, gen_samples 相位步进用. */
        audio_rate = 48000;
        if (pcm) { snd_pcm_close(pcm); pcm = NULL; alog("PULSE: alsa hold released"); }
        if (ao_dev) { if (ao_close_fn) ao_close_fn(ao_dev); ao_dev = NULL; }
        sdl_close();
        alog("S PULSE START req=%ds", seconds);
        audio_phase = 0;
        audio_deadline = seconds ? now() + seconds : 0;
        thread_run = 1;
        if (pthread_create_fn(&audio_tid, NULL, audio_writer, NULL) != 0) {
            thread_run = 0;
            alog("S PULSE spawn FAIL");
            return 0;
        }
        audio_on = 1;
        return 1;
    }
    if (!pcm) pcm_acquire();   /* returning from AO backend: take hw back */
    if (!pcm) return 0;
    sdl_close();   /* leaving SDL backend? its device must not linger */
    audio_rate = 44100;   /* plughw 路径固定 44100 */
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
    audio_on = 1;
    return 1;
}

/* libpulse-simple 后端 (v0.4.46+): paplay 风格 - 见 audio_pulse.c */

void audio_stop_full(void) {
    double t0 = now();
    if (audio_backend == 3) {
        /* libpulse-simple: 只 join writer, writer 自己 close.
           pa_simple 非线程安全: 任何线程都不能在 writer 还在 pa_simple_write
           时调 flush/free (libpulse 内部死锁). thread_run=0 让 writer 下一轮
           循环退出并自行 audio_pulse_close(). */
        thread_run = 0;
        if (pthread_join_fn) pthread_join_fn(audio_tid, NULL);
        alog("P pulse stop joined in %.1fms", (now() - t0) * 1000.0);
        audio_deadline = 0;
        audio_on = 0;
        return;
    }
    if (audio_backend == 2 && sdl_audio_open) {
        /* pause + close: SDL joins its own thread internally */
        thread_run = 0;
        sdl_close();
        alog("P sdl stop in %.1fms", (now() - t0) * 1000.0);
        audio_deadline = 0;
        audio_on = 0;
        return;
    }
    /* nothing active on the non-ALSA paths? then pcm may be NULL (released
       when a server backend took over) - dereferencing it segfaults */
    if (!thread_run && !ao_dev && !sdl_audio_open && !pcm) {
        audio_deadline = 0; audio_on = 0;
        return;
    }
    if (audio_backend == 1 && ao_dev) {
        /* join is fast: ao_play hands bytes to the server-side queue and
           returns - the server does the pacing, nothing blocks for long */
        thread_run = 0;
        if (pthread_join_fn) pthread_join_fn(audio_tid, NULL);
        if (ao_close_fn) ao_close_fn(ao_dev);
        ao_dev = NULL;
        alog("P ao stop in %.1fms", (now() - t0) * 1000.0);
        audio_deadline = 0;
        audio_on = 0;
        return;
    }
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
static volatile int audio_cmd, audio_cmd_sec, audio_cmd_be; /* 0 none, 1 start, 2 stop, 3 set-backend */
void audio_ui_start(int seconds) { audio_cmd_sec = seconds; audio_cmd = 1; }
void audio_ui_stop(void)         { audio_cmd = 2; }
/* backend switch must be serialized through the ctl thread: STOP the OLD
   backend first (using the THEN-current audio_backend), then flip. Doing the
   flip inline while audio is playing leaves the old device/thread leaked and
   the next STOP targets the wrong path (= the "switched then silent" bug). */
void audio_ui_set_backend(int be) { audio_cmd_be = be; audio_cmd = 3; }
void imrun_audio_stop_all(void)     { audio_cmd = 2; } /* plugin-facing */
double imrun_now(void)             { return now(); } /* plugin clock (relative) */
static void *audio_ctl(void *arg) {
    (void)arg;
    while (1) {
        if (usleep_fn) usleep_fn(10000);
        int cmd = audio_cmd;
        if (!cmd && audio_backend == 2 && sdl_audio_open &&
            audio_deadline && now() > audio_deadline)
            audio_cmd = cmd = 2;   /* SDL has no writer thread: ctl enforces */
        if (!cmd) continue;
        audio_cmd = 0;
        if (cmd == 3) {
            alog("C ctl: SETBACKEND %d->%d", audio_backend, audio_cmd_be);
            if (audio_on || thread_run || ao_dev || sdl_audio_open)
                audio_stop_full();   /* stops the CURRENT (old) backend */
            audio_backend = audio_cmd_be;
            /* 切到 PULSE (3) 时必须无条件释放声卡 hold:
               PULSE 后端要 pulse server 拿 sink.hw0, 但 IMRUN 启动时 pcm_acquire()
               预占了 plughw. 若没在播, 上面 audio_stop_full 不会跑, hold 还在
               -> pulse server resume sink.hw0 抢不到硬件 -> pa_simple_write 永久
               阻塞 + 已写块 XRUN 噼啪. 这是 PULSE 后端的致命前提. */
            if (audio_backend == 3) {
                if (pcm) { snd_pcm_close(pcm); pcm = NULL; alog("PULSE: hold released (setbackend)"); }
                if (ao_dev) { if (ao_close_fn) ao_close_fn(ao_dev); ao_dev = NULL; }
                sdl_close();
            }
            alog("C ctl: backend now %d", audio_backend);
        } else if (cmd == 2) {
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

#include <signal.h>
static void fatal_sig(int sig) {
    alog("FATAL signal %d (segv=11 bus=7 abort=6 fpe=8)", sig);
    signal(sig, SIG_DFL);
    raise(sig);
}
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    alog_t0 = now();
    alog("LIFECYCLE boot");
    {   /* single instance: second launch from the desktop icon exits at once */
        FILE *lf = fopen("/tmp/imrun.lock", "r");
        if (lf) {
            int oldpid = 0;
            { char lb[32]; if (fgets(lb, sizeof lb, lf)) oldpid = atoi(lb); }
            fclose(lf);
            char pc[64];
            snprintf(pc, sizeof pc, "/proc/%d/cmdline", oldpid);
            FILE *pf = fopen(pc, "r");
            if (pf) {
                char cb[128] = {0};
                fread(cb, 1, sizeof cb - 1, pf);
                fclose(pf);
                if (strstr(cb, "imrun")) {
                    alog("LIFECYCLE second instance blocked (pid %d alive)", oldpid);
                    printf("imrun already running (pid %d)\n", oldpid);
                    return 0;
                }
            }
        }
        FILE *wl = fopen("/tmp/imrun.lock", "w");
        if (wl) { fprintf(wl, "%d\n", (int)getpid()); fclose(wl); }
    }
    {
        extern void imrun_atexit_log(void);
        imrun_atexit_log();
    }
    signal(SIGSEGV, fatal_sig);
    signal(SIGBUS, fatal_sig);
    signal(SIGABRT, fatal_sig);
    signal(SIGFPE, fatal_sig);
    signal(SIGILL, fatal_sig);
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
    if (pXSetErrorHandler) pXSetErrorHandler(imrun_xerror, NULL);
    /* grab the sound card ONCE, hold until the OSS backend needs the hw -
       pulse is on-demand (module-suspend-idle): after idle it releases hw,
       we take it and never let go. No open/close racing, no EBUSY. */
    pcm_acquire();

    /* resident audio-control thread: PLAY/STOP buttons only set flags
       (spawn even without pcm: the OSS backend needs no ALSA handle) */
    if (pthread_create_fn)
        pthread_create_fn(&audio_ctl_tid, NULL, audio_ctl, NULL);
    /* battery monitor thread (forks hal-get-property, never in UI thread) */
    if (pthread_create_fn) {
        unsigned long bt;
        pthread_create_fn(&bt, NULL, batt_reader, NULL);
        unsigned long ht, wt;
        pthread_create_fn(&ht, NULL, hw_thread, (void *)hw_sample);
        pthread_create_fn(&wt, NULL, wall_thread, NULL);
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
        alog("=== AUTOTEST end -> exit(0) (this is the \"flash quit\" if gautotest existed) ===");
        printf("AUTOTEST done, log at /home/user/audio_dbg.log\n");
        return 0;
    }
    }

    /* AOTEST: audio backend stability probe, low volume, headless + file
       triggered (mirrors AUTOTEST's reliable file path). Rapid PLAY/volume
       jitter/STOP/double-STOP cycles. Detects stream dropouts in BOTH
       backends: libao (plughw:0,0 direct) and SDL (pulse server queue).
       All markers in /home/user/audio_dbg.log. */
    if (access("/home/user/aotest", 0) == 0) {
        unlink("/home/user/aotest");
        alog_t0 = now();
        alog("=== AOTEST begin: wave=%d amp=%d low-volume rapid play/vol/stop ===",
             audio_wave, audio_amp);
        audio_wave    = 0;   /* sine */
        audio_amp     = 1500;/* low volume: peak ~1500*vol/100, stays quiet */
        audio_volume  = 100;
        int vols[] = { 30, 200, 80, 15, 250, 120, 40, 300, 60, 180, 90, 20, 260, 140, 50 };
        int nvols = (int)(sizeof vols / sizeof vols[0]);
        int hard_fail = 0;
        int backends[4] = { 0, 1, 2, 3 };   /* 0=ALSA 1=libao 2=SDL 3=PULSE(sink.hw0).
                                            PULSE 必须 device=sink.hw0 (默认 sink
                                            = Nokia music 路由, 实测卡死). */
        for (int bi = 0; bi < 4 && !hard_fail; bi++) {
            int be = backends[bi];
            audio_backend = be;
            alog("=== AOTEST backend=%d (%s) ===", be,
                 be == 0 ? "ALSA" : be == 1 ? "libao-vgmplay" : be == 2 ? "SDL-psp" : "PULSE-sink.hw0");
            for (int cyc = 0; cyc < 10; cyc++) {
                alog("--- AOTEST be%d cyc %d: PLAY(2s) ---", be, cyc);
                if (!audio_start(2)) {
                    alog("AOTEST be%d cyc %d: audio_start FAILED (open/acquire dropped)", be, cyc);
                    hard_fail = 1;
                    break;
                }
                for (int rep = 0; rep < 2; rep++) {
                    for (int k = 0; k < nvols; k++) {
                        audio_volume = vols[k];
                        alog("AOTEST be%d cyc %d vol=%d on=%d ald=%d ao=%p", be, cyc,
                             audio_volume, audio_on, sdl_audio_open, ao_dev);
                        usleep_fn(25000); /* rapid: user dragging the VOL slider */
                    }
                }
                alog("--- AOTEST be%d cyc %d: STOP (sdl_cb=%lu) ---", be, cyc, sdl_cb_count);
                audio_stop_full();
                usleep_fn(40000);
                alog("--- AOTEST be%d cyc %d: STOP again (empty-state short-circuit) ---", be, cyc);
                audio_stop_full();
                usleep_fn(60000);
            }
        }
        alog("=== AOTEST end: hard_fail=%d max_stall=%.3fs stalls=%d underruns=%d -> exit(0) ===",
             hard_fail, dbg_max_stall, dbg_stalls, dbg_underruns);
        printf("AOTEST done, log at /home/user/audio_dbg.log\n");
        return 0;
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
        double ph0 = now();
        /* pump X11 input -> ImGui */
        if (pXPending && pXNextEvent) {
            /* 32-bit Xlib event layout (core pointer events only) */
            /* full XEvent is ~96B on 32-bit ARM - a partial struct let
               XNextEvent overwrite ~24B of adjacent stack EVERY event */
            typedef struct {
                int type; unsigned long serial, send_event, display;
                unsigned long window, root, subwindow, time;
                int x, y, x_root, y_root;
                unsigned int state, button;
                char pad[64];   /* union tail: full XEvent ~96B */
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
        if (osso_display_blanking_pause && frames % 1500 == 0)
            osso_display_blanking_pause(osso_ctx);
        if (frames == 10 && pXSetInputFocus) {
            /* window fully mapped + fullscreen by now: focus grab is safe */
            pXSetInputFocus(xdpy, win, 1 /*RevertToParent*/, 0 /*CurrentTime*/);
            pXSync(xdpy, 0);
        }
        double el = (t = now()) - t0;
        /* N900 has no RTC battery: the clock jumps (2009 firmware default ->
           NTP 2026) while we run; el explodes and float precision freezes ALL
           animation. Detect any jump > 1h (or backwards) and rebase. */
        if (el > 3600.0 || el < 0) {
            t0 = t;
            el = 0;
            alog("CLOCK jumped - animation time rebased");
        }
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
        float wel = (g_wall_mode == 1 || g_wall_mode == 3) ? 5.0f
                                                              : (float)g_wall_t;
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
        double ph1 = now();
        igui_newframe(dt);
        igui_build_ui(el, fps);
        double ph2 = now();
        igui_render();
        double ph3 = now();
        if (!peglSwapBuffers(eglD, eglS))
            printf("SWAP FAIL err=%x frame=%d\n", peglGetError(), frames);
        frames++;
        report_frames++;
        {   /* frame profiler: where does the time go? (every 180 frames) */
            static double p_max[3] = {0,0,0}, p_sum[3] = {0,0,0}, p_t0 = 0;
            static int p_n = 0, p_stall = 0;
            double ph4 = now();
            double d[3] = { (ph1 - ph0) * 1000.0, (ph3 - ph2) * 1000.0, (ph4 - ph3) * 1000.0 };
            double tot = (ph4 - ph0) * 1000.0;
            for (int q = 0; q < 3; q++) {
                if (d[q] > p_max[q]) p_max[q] = d[q];
                p_sum[q] += d[q];
            }
            if (tot > 50.0) p_stall++;
            if (p_n == 0) p_t0 = ph4;
            if (++p_n >= 180) {
                double fps = 180.0 / (ph4 - p_t0);
                alog("PROF fps=%.1f pump=%.1f(max%.1f) ui=%.1f(max%.1f) swap=%.1f(max%.1f) stalls50ms=%d",
                     fps, p_sum[0]/p_n, p_max[0], p_sum[1]/p_n, p_max[1],
                     p_sum[2]/p_n, p_max[2], p_stall);
                for (int q = 0; q < 3; q++) { p_max[q] = 0; p_sum[q] = 0; }
                p_n = 0; p_stall = 0;
            }
        }
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
