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
/* ALSA: sound output without any audio framework */
typedef struct _snd_pcm snd_pcm_t;
static void *hALSA;
static int (*snd_pcm_open)(snd_pcm_t **, const char *, int, int);
static int (*snd_pcm_set_params)(snd_pcm_t *, int, int, unsigned, unsigned, int, unsigned);
static long (*snd_pcm_writei)(snd_pcm_t *, const void *, unsigned long);
static int (*snd_pcm_close)(snd_pcm_t *);
static int (*snd_pcm_prepare)(snd_pcm_t *);
static snd_pcm_t *pcm;
int audio_on;
static float audio_phase;
double audio_deadline = 0;
static void *hPTHREAD;
static int (*pthread_create_fn)(unsigned long *, void *, void *(*)(void *), void *);
static int (*pthread_join_fn)(unsigned long, void **);
static int (*usleep_fn)(unsigned int);
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
    hALSA = dlopen("libasound.so.2", RTLD_NOW);
    if (hALSA) {
        snd_pcm_open = dlsym(hALSA, "snd_pcm_open");
        snd_pcm_set_params = dlsym(hALSA, "snd_pcm_set_params");
        snd_pcm_writei = dlsym(hALSA, "snd_pcm_writei");
        snd_pcm_close = dlsym(hALSA, "snd_pcm_close");
        snd_pcm_prepare = dlsym(hALSA, "snd_pcm_prepare");
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
extern double audio_deadline;
extern int audio_start(int seconds);
extern void vibra_ping(void);
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

void vibra_ping(void) {
    int r = system("dbus-send --system --type=method_call --dest=com.nokia.mce "
                   "/com/nokia/mce/request com.nokia.mce.request.req_vibrator_"
                   "pattern_activate string:PatternIncomingCall 2>&1");
    (void)r;
}

void audio_toggle(int on) {
    if (!snd_pcm_open) return;
    if (on && !pcm) {
        if (snd_pcm_open(&pcm, "default", 0 /*PLAYBACK*/, 0) == 0) {
            /* S16_LE=2, RW_INTERLEAVED=3, 2ch, 44100Hz, 500ms buffer */
            snd_pcm_set_params(pcm, 2, 3, 2, 44100, 1, 500000);
        } else pcm = NULL;
    } else if (!on && pcm) {
        snd_pcm_close(pcm);
        pcm = NULL;
    }
    audio_on = on && pcm != NULL;
}

/* SDL-callback-style: dedicated writer thread drains a ring buffer, so UI
   frame hiccups can never underrun the device (PPSSPP/deadbeef pattern) */
static void *audio_writer(void *arg) {
    (void)arg;
    short chunk[PERIOD * 2];
    /* generator lives IN the writer thread: UI can stall forever,
       audio never notices (deadbeef output-thread pattern) */
    while (thread_run) {
        if (audio_deadline && now() > audio_deadline)
            break;
        for (unsigned i = 0; i < PERIOD; i++) {
            short s = (short)(sinf(audio_phase) * 16000);
            chunk[i * 2] = s;
            chunk[i * 2 + 1] = s;
            audio_phase += 2.0f * 3.14159265f * 440.0f / 44100.0f;
        }
        long w = snd_pcm_writei(pcm, chunk, PERIOD);
        if (w < 0 && snd_pcm_prepare) {
            snd_pcm_prepare(pcm);
            w = snd_pcm_writei(pcm, chunk, PERIOD);
        }
    }
    return NULL;
}

int audio_start(int seconds) {
    if (!snd_pcm_open || !pthread_create_fn) return 0;
    audio_toggle(0);
    audio_toggle(1);
    if (!audio_on) return 0;
    audio_phase = 0;
    audio_deadline = seconds ? now() + seconds : 0;
    thread_run = 1;
    if (pthread_create_fn(&audio_tid, NULL, audio_writer, NULL) != 0) {
        thread_run = 0;
        audio_toggle(0);
        return 0;
    }
    return 1;
}

static void audio_stop_full(void) {
    if (thread_run) {
        thread_run = 0;
        if (pthread_join_fn) pthread_join_fn(audio_tid, NULL);
    }
    audio_toggle(0);
    audio_deadline = 0;
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
                } else if (ev.type == 2 /*KeyPress*/ || ev.type == 3 /*KeyRelease*/) {
                    char kb[16] = {0};
                    if (pXLookupString)
                        pXLookupString(&ev, kb, sizeof kb - 1, NULL, NULL);
                    snprintf(last_key, sizeof last_key, "KEY %u %s '%s'",
                             (unsigned)ev.button, ev.type == 2 ? "DOWN" : "UP", kb);
                    if (ev.type == 2 && kb[0]) igui_key_char(kb);
                }
            }
        }
        if (frames % 60 == 0) hw_sample();
        if (osso_display_blanking_pause && frames % 1500 == 0)
            osso_display_blanking_pause(osso_ctx);
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
