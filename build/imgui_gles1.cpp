// imgui_gles1.cpp - Dear ImGui 1.92.8 rendering backend for OpenGL ES 1.1
// (fixed-function client arrays) + demo UI content for the N900 showcase.
// All GL entry points are handed in from the C side (runtime dlopen'd).
#include "imgui.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>

extern "C" {

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

extern "C" void (*pglDeleteTextures)(unsigned, const unsigned *);   /* gldemo dlsym's it */
static IGlfns gl;
static ImFont *g_fonts[4];
static ImFont *g_cnfont;
static ImFont *g_worldfont;
static ImFont *g_krfont;
static const char *g_fontnames[4] = {"MSYH", "MSYH", "MSYH", "BITMAP"};
static const float g_sizes[4] = {20.0f, 20.0f, 20.0f, 22.0f};
static int g_fontsel = 1;
static int scrW = 800, scrH = 480;
static unsigned fontTex = 0;

// GLES constants (values duplicated from C side; keep in sync)
#define GL_TEXTURE_2D      0x0DE1
#define GL_RGBA            0x1908
#define GL_UNSIGNED_BYTE   0x1401
#define GL_UNSIGNED_SHORT  0x1403
#define GL_FLOAT           0x1406
#define GL_TRIANGLES       4
#define GL_NEAREST         0x2600
#define GL_LINEAR          0x2601
#define GL_BLEND           0x0BE2
#define GL_SRC_ALPHA       0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_SCISSOR_TEST    0x0C11
#define GL_PROJECTION_M    0x1701
#define GL_MODELVIEW_M     0x1700
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S  0x2802
#define GL_TEXTURE_WRAP_T  0x2803
#define GL_CLAMP_TO_EDGE   0x812F

static void upload_texture(ImTextureData *tex) {
    unsigned id = (unsigned)(intptr_t)tex->TexID;
    if (id == 0) {
        gl.GenTextures(1, &id);
        tex->SetTexID((ImTextureID)(intptr_t)id);
    }
    gl.BindTexture(GL_TEXTURE_2D, id);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    int w = tex->Width, h = tex->Height;
    int pitch = tex->GetPitch();
    const unsigned char *px = (const unsigned char *)tex->GetPixels();
    if (pitch == w * 4) {
        gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    } else {
        unsigned char *tmp = new unsigned char[w * h * 4];
        for (int y = 0; y < h; y++)
            memcpy(tmp + y * w * 4, px + y * pitch, w * 4);
        gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tmp);
        delete[] tmp;
    }
    tex->SetTexID((ImTextureID)(intptr_t)id);
    tex->SetStatus(ImTextureStatus_OK);
    fontTex = id;
}

void imrun_minimize_by_title(const char *title);
extern void alog_public(const char *fmt, ...);
extern int ImGui_OpenPopupCount(void);   /* patched-core BACK support */
extern int igui_desk_nav_index(void);
extern void ImGui_CloseTopPopup(void);
int igui_init(int w, int h, IGlfns *fns) {
    gl = *fns;
    scrW = w; scrH = h;
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; /* arrows+Enter drive the UI */
    /* ONLY the title bar drags/moves a window - empty-space drags are
       reserved for touch scrolling (page turning), not window moving */
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures; /* dynamic font sizes */
    io.MouseDragThreshold = 3.0f; /* touch: 6px default eats finger gestures */
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = "/home/user/imrun.ini"; /* window pos/size persist forever */
    {   /* patched ImGui core passes the window title when "_" is hit */
        extern void (*ImGui_g_MinimizeCallback)(const char *title);
        ImGui_g_MinimizeCallback = [](const char *title) {
            imrun_minimize_by_title(title);
        };
    }
    ImGui::StyleColorsDark();
    {
        /* shell polish: rounded, soft; resistive-friendly paddings */
        ImGuiStyle &st = ImGui::GetStyle();
        st.WindowRounding = 8.0f;
        st.ChildRounding = 8.0f;
        st.FrameRounding = 6.0f;
        st.GrabRounding = 5.0f;
        st.PopupRounding = 6.0f;
        st.ScrollbarRounding = 8.0f;
        st.ScrollbarSize = 20.0f;   /* finger-sized, resistive screen */
        st.GrabMinSize = 18.0f;
        st.TabRounding = 6.0f;
        st.WindowBorderSize = 1.0f;
        st.FramePadding = ImVec2(10, 7);
        st.ItemSpacing = ImVec2(10, 9);
        ImVec4 *c = st.Colors;
        c[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.11f, 0.16f, 0.94f);
        c[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.25f, 0.36f, 1.0f);
        c[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.18f, 0.26f, 1.0f);
        c[ImGuiCol_Button] = ImVec4(0.18f, 0.24f, 0.34f, 1.0f);
        c[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.38f, 0.52f, 1.0f);
        c[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.50f, 0.68f, 1.0f);
        c[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.60f, 0.80f, 1.0f);
        c[ImGuiCol_CheckMark] = ImVec4(0.45f, 0.75f, 0.95f, 1.0f);
        c[ImGuiCol_Separator] = ImVec4(0.30f, 0.40f, 0.52f, 0.6f);
    }
#if IMGUI_VERSION_NUM >= 19200
    ImGui::GetStyle().FontScaleMain = 1.0f; /* no runtime scaling */
#else
    io.FontGlobalScale = 1.0f;
#endif
    /* pixel-perfect fonts, same recipe as the music player v16:
       pre-rasterize every choice at its FINAL pixel size + snap to pixels.
       msyh.ttc lives at /home/user/fonts/msyh.ttc. ASCII range only — the
       legacy preload path would blow the 2048px texture limit with Chinese
       ranges (SGX530 max texture size). */
    /* 1.92 draws at style.FontSizeBase (dynamic bake per requested size),
       NOT the AddFont SizePixels — so load each face ONCE and set
       FontSizeBase on selection. Sizes rasterize on demand, pixel-perfect. */
    ImFontConfig fcfg;
    fcfg.SizePixels = 20.0f;
    fcfg.PixelSnapH = true;
    g_fonts[0] = g_fonts[1] = g_fonts[2] = io.Fonts->AddFontFromFileTTF(
        "/home/user/fonts/msyh.ttc", 20.0f, &fcfg,
        io.Fonts->GetGlyphRangesDefault());
    {
        ImFontConfig bcfg;
        bcfg.SizePixels = 22.0f;
        bcfg.PixelSnapH = true;
        g_fonts[3] = io.Fonts->AddFontDefault(&bcfg);
    }
    {
        /* dynamic path bakes glyphs on demand: cjk/cyrillic ranges are fine */
        ImFontConfig ccfg;
        ccfg.SizePixels = 20.0f;
        ccfg.PixelSnapH = true;
        g_cnfont = io.Fonts->AddFontFromFileTTF("/home/user/fonts/msyh.ttc",
                                                20.0f, &ccfg,
                                                io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
        /* WORLD font: latin+accents, cyrillic, kana, han — one face */
        static ImWchar wranges[] = { 0x20, 0xFF, 0x400, 0x52F, 0x3040, 0x30FF,
                                     0x4E00, 0x9FFF, 0 };
        ImFontConfig wcfg;
        wcfg.SizePixels = 20.0f;
        wcfg.PixelSnapH = true;
        g_worldfont = io.Fonts->AddFontFromFileTTF("/home/user/fonts/msyh.ttc",
                                                   20.0f, &wcfg, wranges);
        /* Korean hangul: msyh has none, malgun does */
        ImFontConfig kcfg;
        kcfg.SizePixels = 20.0f;
        kcfg.PixelSnapH = true;
        g_krfont = io.Fonts->AddFontFromFileTTF("/home/user/fonts/malgun.ttf",
                                                20.0f, &kcfg,
                                                io.Fonts->GetGlyphRangesKorean());
    }
    g_fontsel = 1;
    io.FontDefault = g_fonts[1] ? g_fonts[1] : io.Fonts->Fonts[0];
    ImGui::GetStyle().FontSizeBase = g_sizes[1];
    // build default font atlas now (1.92 dynamic font system: mark wants-update)
    return 0;
}

void igui_newframe(float dt) {
    ImGuiIO &io = ImGui::GetIO();
    io.DeltaTime = dt > 0 ? dt : 1.0f / 60.0f;
    // RendererHasTextures backend: glyphs bake on demand at the requested
    // size; texture uploads happen in igui_render from PlatformIO().Textures.
    ImGui::NewFrame();
}



void ui_stress_toggle(void) {
    ImGuiStyle &st = ImGui::GetStyle();
    static float v = 1.0f;
    v = (v > 1.1f) ? 1.0f : 1.35f;
    st.FontScaleMain = v; /* forces full glyph rebake = heaviest UI op */
}

#include <dlfcn.h>
#include <dirent.h>

#include <dlfcn.h>
#include <dirent.h>
#include <string.h>
static inline ImVec4 vec4f(float x, float y, float z, float w) { return ImVec4(x, y, z, w); }
extern "C" double imrun_now(void);
extern "C" double imrun_now(void);
double now_double(void) { return imrun_now(); }

/* ============ IMRUN shell v0.4: EVERYTHING is a dyn plugin ==============
   System apps (SYSTEM/APPMGR) are plugins too - just marked protected
   (imapp_system() returns 1) so APPMGR/edit-mode refuse to uninstall them.
   One window code path for all apps: no special branches, no special bugs. */

#define IMRUN_VER "0.4"
#define APPS_DIR "/home/user/imrun/apps"
#define SYSBAR_H 48.0f

struct DynApp {
    char id[40], name[40];
    void *h;
    void (*frame)(double t);
    int (*init)(void);
    void (*destroy)(void);
    void (*icon)(void *dl, float cx, float cy, float s);
    int (*sys)(void);
    bool destroyed;
    bool open, mini, ever, cascade, closing;
    bool collapsed, uncoll;          /* native fold = third state */
    bool mfade; double mfade_t0;     /* minimize fade-out (0.5s) */
    float anim; double anim_t0, close_t0;
};
static DynApp g_dyn[8];
static int g_dyn_n = 0;
static int g_dyn_scan_cnt = 0;
static int g_dyn_dead = -1;
extern "C" { extern int g_wall_mode, g_wall_variant; }
extern "C" float g_a_win = 0.88f, g_a_bar = 0.80f, g_a_desk = 0.0f;
extern "C" bool g_dock_covers_sysbar = false;

/* ---- host services exported to plugins ---- */
extern "C" double imrun_now(void);
extern "C" void imrun_vibra_ping(int ms) { extern void vibra_ping(int); vibra_ping(ms); }
extern "C" void imrun_vibra_stop(void) { extern void vibra_stop(void); vibra_stop(); }
extern "C" int imrun_dyn_count(void) { return g_dyn_n; }
extern "C" const char *imrun_dyn_name(int i) { return (i>=0&&i<g_dyn_n)?g_dyn[i].name:""; }
extern "C" int imrun_dyn_state(int i) {
    if (i<0||i>=g_dyn_n) return 0;
    return g_dyn[i].open ? (g_dyn[i].mini?1:2) : 0;
}
extern "C" void imrun_dyn_kill(int i) { if (i>=0&&i<g_dyn_n&&g_dyn[i].open&&!g_dyn[i].closing){g_dyn[i].closing=true;g_dyn[i].close_t0=now_double();} }
extern "C" void imrun_dyn_uninstall(int i) { if (i>=0&&i<g_dyn_n&&!g_dyn[i].sys) g_dyn_dead = i; }
static bool g_scan_req = true;   /* initial scan + user-requested only */
extern "C" void imrun_host_rescan(void) { g_scan_req = true; }

static char g_desk_filter[32] = "";
static bool g_desk_edit = false;
static int g_desk_nav_sel = -1;
static bool g_desk_nav_on = false;  /* only show highlight after a keypress */
static int g_desk_last = -1;   /* last-clicked tile: ring persists */
/* ---- app ordering: system apps DEFAULT to top-left, user may move them.
   Order is kept BY ID (survives uninstall/reinstall); no forced re-pinning. */
static int g_order[8];
static int g_order_n = 0;
static char g_order_id[8][40];
static void order_sync(void) {
    int n = 0;
    for (int q = 0; q < 8; q++) g_order[q] = -1;   /* stale indices poison dup check */
    for (int q = 0; q < 8 && g_order_id[q][0]; q++) {
        for (int i = 0; i < g_dyn_n; i++) {
            if (strcmp(g_dyn[i].id, g_order_id[q]) == 0) {
                bool dup = false;
                for (int k = 0; k < n; k++) if (g_order[k] == i) dup = true;
                if (!dup) g_order[n++] = i;
                break;
            }
        }
    }
    if (n == 0) {
        /* first boot default: system apps top-left, then scan order */
        for (int i = 0; i < g_dyn_n && n < 8; i++)
            if (g_dyn[i].sys) g_order[n++] = i;
    }
    for (int i = 0; i < g_dyn_n && n < 8; i++) {
        bool dup = false;
        for (int k = 0; k < n; k++) if (g_order[k] == i) dup = true;
        if (!dup) g_order[n++] = i;
    }
    g_order_n = n;
    for (int q = 0; q < n; q++)
        snprintf(g_order_id[q], 40, "%s", g_dyn[g_order[q]].id);
}
static void order_swap(int a, int b) {
    if (a < 0 || b < 0 || a >= g_order_n || b >= g_order_n) return;
    int t = g_order[a];
    g_order[a] = g_order[b];
    g_order[b] = t;
    snprintf(g_order_id[a], 40, "%s", g_dyn[g_order[a]].id);
    snprintf(g_order_id[b], 40, "%s", g_dyn[g_order[b]].id);
}
/* animated tile display positions (-1 = snap on first frame) */
static float g_tile_x[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
static float g_tile_y[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
bool g_ctrl_down = false;
extern bool ImGui_IsWindowDocked(const char *name);
static bool g_win_move = false;
static float g_nudge_dx = 0, g_nudge_dy = 0;
static float g_back_hl = 0, g_home_hl = 0;

static int g_focus = -1;
static void dyn_focus(int di) {
    if (di < 0 || di >= g_dyn_n) return;
    DynApp &a = g_dyn[di];
    bool was_live = a.open && !a.closing && !a.mini;
    if (a.closing) a.closing = false;   /* reopen during fade = cancel */
    a.mfade = false;                    /* restore cancels minimize fade */
    if (a.destroyed && a.init) { a.init(); a.destroyed = false; }  /* fresh start */
    a.open = true; a.mini = false;
    a.uncoll = true;                 /* opening/focusing always expands */
    /* single-focus rule: re-clicking a live app ONLY switches focus -
       no multi-open, no animation replay */
    if (!was_live) {
        if (!a.ever) { a.cascade = true; a.ever = true; }
        a.anim = 0; a.anim_t0 = now_double();
    }
    g_desk_nav_sel = -1; g_desk_nav_on = false;
    g_focus = di;
}
static void dyn_minimize(int di) {
    /* minimize = 0.5s fade-out then hidden. For a DOCKED member this
       removes its tab from the node; minimizing ALL members of a node
       hides the merged window whole - ImGui remembers the dock layout
       by node ID, restore is identical */
    if (di<0||di>=g_dyn_n) return;
    DynApp &a = g_dyn[di];
    if (a.open && !a.mini && !a.closing && !a.mfade) {
        a.mfade = true; a.mfade_t0 = now_double();
    }
    if (g_focus == di) g_focus = -1;
}
static void dyn_minimize_all(void) {
    for (int k = 0; k < g_dyn_n; k++) g_dyn[k].mini = true;
    g_focus = -1;
}
extern void alog_public(const char *fmt, ...);
static void dyn_close(int di) {
    if (di>=0&&di<g_dyn_n) alog_public("DYNCLOSE di=%d name=%s", di, g_dyn[di].name);
    if (di<0||di>=g_dyn_n) return;
    if (g_dyn[di].open && !g_dyn[di].closing) {
        g_dyn[di].closing = true; g_dyn[di].close_t0 = now_double();
    }
    if (g_focus == di) g_focus = -1;
}
void igui_close_focused(void) { if (g_focus >= 0) dyn_close(g_focus); }

void igui_ctrl_combo(char c) {
    printf("CTRL+%c (focus=%d)\n", c, g_focus); fflush(stdout);
    if (c=='w'||c=='W') igui_close_focused();
    else if (c=='t'||c=='T') {
        int order[9], n = 0;
        for (int k = 0; k < g_dyn_n; k++)
            if (g_dyn[k].open && !g_dyn[k].mini && !g_dyn[k].closing) order[n++] = k;
        if (!n) return;
        int cur = -1;
        for (int k = 0; k < n; k++) if (order[k] == g_focus) cur = k;
        dyn_focus(order[(cur + 1) % n]);
    }
    else if (c=='m'||c=='M') {
        g_win_move = !g_win_move; g_nudge_dx = g_nudge_dy = 0;
        imrun_vibra_ping(50);
    }
}
void igui_key_char(const char *utf8) {
    if (g_focus < 0 && utf8 && utf8[0] >= 0x20 && !utf8[1] &&
        strlen(g_desk_filter) + 1 < sizeof g_desk_filter) {
        char c = utf8[0];
        if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')) {
            size_t l = strlen(g_desk_filter);
            g_desk_filter[l] = c; g_desk_filter[l+1] = 0;
            return;
        }
    }
    ImGui::GetIO().AddInputCharactersUTF8(utf8);
}
static void dyn_open_by_filter_match(void) {
    char uf[32];
    snprintf(uf, sizeof uf, "%s", g_desk_filter);
    for (char *q = uf; *q; q++) *q = (*q>='a'&&*q<='z') ? *q-32 : *q;
    for (int i = 0; i < g_dyn_n; i++) {
        char ub[40];
        snprintf(ub, sizeof ub, "%s", g_dyn[i].name);
        for (char *q = ub; *q; q++) *q = (*q>='a'&&*q<='z') ? *q-32 : *q;
        if (strstr(ub, uf)) { dyn_focus(i); g_desk_filter[0] = 0; return; }
    }
    g_desk_filter[0] = 0;
}
extern int ImGui_OpenPopupCount(void);
extern void ImGui_CloseTopPopup(void);
void igui_special_key(unsigned ks, int down) {
    ImGuiIO &io = ImGui::GetIO();
    if (down && g_focus < 0 && g_desk_filter[0]) {
        if (ks == 0xff08) { size_t l = strlen(g_desk_filter); if (l) g_desk_filter[l-1] = 0; return; }
        if (ks == 0xff0d || ks == 0xff0a) { dyn_open_by_filter_match(); return; }
        if (ks == 0xff1b) { g_desk_filter[0] = 0; return; }
    }
    if (down && g_focus < 0 && !g_desk_edit) {
        int total = g_dyn_n;
        if (total > 0) {
            if (ks == 0xff51 || ks == 0xff52 || ks == 0xff53 || ks == 0xff54)
                g_desk_nav_on = true;
            if (g_desk_nav_sel < 0) g_desk_nav_sel = 0;
            if (ks == 0xff51 && g_desk_nav_sel % 4 > 0) g_desk_nav_sel--;
            else if (ks == 0xff53 && g_desk_nav_sel % 4 < 3 && g_desk_nav_sel + 1 < total) g_desk_nav_sel++;
            else if (ks == 0xff52 && g_desk_nav_sel >= 4) g_desk_nav_sel -= 4;
            else if (ks == 0xff54 && g_desk_nav_sel + 4 < total) g_desk_nav_sel += 4;
            else if (ks == 0xff0d || ks == 0xff0a) { int sv = g_desk_nav_sel; g_desk_nav_sel = -1; g_desk_nav_on = false; dyn_focus(sv); return; }
        }
        if (ks == 0xff51 || ks == 0xff52 || ks == 0xff53 || ks == 0xff54) return;
    }
    if (down && g_win_move && g_focus >= 0) {
        if (ks == 0xff51) { g_nudge_dx -= 24; return; }
        if (ks == 0xff53) { g_nudge_dx += 24; return; }
        if (ks == 0xff52) { g_nudge_dy -= 24; return; }
        if (ks == 0xff54) { g_nudge_dy += 24; return; }
    }
    switch (ks) {
    case 0xff08: io.AddKeyEvent(ImGuiKey_Backspace, down != 0); break;
    case 0xff0a: case 0xff0d: io.AddKeyEvent(ImGuiKey_Enter, down != 0); break;
    case 0xff1b: io.AddKeyEvent(ImGuiKey_Escape, down != 0); break;
    case 0xff09: io.AddKeyEvent(ImGuiKey_Tab, down != 0); break;
    case 0xff51: io.AddKeyEvent(ImGuiKey_LeftArrow, down != 0); break;
    case 0xff52: io.AddKeyEvent(ImGuiKey_UpArrow, down != 0); break;
    case 0xff53: io.AddKeyEvent(ImGuiKey_RightArrow, down != 0); break;
    case 0xff54: io.AddKeyEvent(ImGuiKey_DownArrow, down != 0); break;
    case 0xff55: io.AddKeyEvent(ImGuiKey_PageUp, down != 0); break;
    case 0xff56: io.AddKeyEvent(ImGuiKey_PageDown, down != 0); break;
    case 0xff57: io.AddKeyEvent(ImGuiKey_End, down != 0); break;
    case 0xff58: io.AddKeyEvent(ImGuiKey_Home, down != 0); break;
    case 0xff63: case 0xffff: io.AddKeyEvent(ImGuiKey_Delete, down != 0); break;
    case 0xffe1: case 0xffe2: io.AddKeyEvent(ImGuiMod_Shift, down != 0); break;
    case 0xffe3: case 0xffe4: case 0xffe5: g_ctrl_down = (down != 0); io.AddKeyEvent(ImGuiMod_Ctrl, down != 0); break;
    case 0xffe7: case 0xffe8: case 0xffe9: case 0xff7e: io.AddKeyEvent(ImGuiMod_Alt, down != 0); break;
    }
}

/* ---- settings persistence: /home/user/imrun/settings.ini ---- */
extern "C" {
extern int g_wall_mode, g_wall_variant;
extern void alog_public(const char *fmt, ...);
}
static void settings_save(void) {
    FILE *f = fopen("/home/user/imrun_settings.ini", "w");
    alog_public("SETSAVE fopen=%p", f);
    if (!f) return;
    fprintf(f, "wall_mode=%d\nwall_variant=%d\na_win=%.2f\na_bar=%.2f\na_desk=%.2f\ndock_cover=%d\n",
            g_wall_mode, g_wall_variant, g_a_win, g_a_bar, g_a_desk,
            g_dock_covers_sysbar ? 1 : 0);
    for (int q = 0; q < g_order_n; q++)
        fprintf(f, "ord=%s\n", g_dyn[g_order[q]].id);
    fclose(f);
}
extern "C" void imrun_settings_touch(void) { settings_save(); }  /* order swap hook */
static void settings_load(void) {
    FILE *f = fopen("/home/user/imrun_settings.ini", "r");
    if (!f) return;
    char line[96];
    int iv; float v;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "wall_mode=%d", &iv) == 1) { if (iv >= 0 && iv < 5) g_wall_mode = iv; }
        else if (sscanf(line, "wall_variant=%d", &iv) == 1) { if (iv >= 0 && iv < 4) g_wall_variant = iv; }
        else if (sscanf(line, "a_win=%f", &v) == 1) g_a_win = v;
        else if (sscanf(line, "a_bar=%f", &v) == 1) g_a_bar = v;
        else if (sscanf(line, "a_desk=%f", &v) == 1) g_a_desk = v;
        else if (sscanf(line, "dock_cover=%d", &iv) == 1) g_dock_covers_sysbar = iv > 0;
        else if (strncmp(line, "ord=", 4) == 0) {
            char idv[40];
            if (sscanf(line + 4, "%39s", idv) == 1 && g_order_n < 8)
                snprintf(g_order_id[g_order_n++], 40, "%s", idv);
        }
    }
    fclose(f);
}

static void dyn_scan(void) {
    /* user directive: load ALL plugins at once at startup; afterwards scan
       ONLY when the user asks (APPMGR RESCAN / USB install) */
    DIR *d = opendir(APPS_DIR);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && g_dyn_n < 8) {
        if (e->d_name[0] == '.') continue;
        bool have = false;
        for (int k = 0; k < g_dyn_n; k++)
            if (strcmp(g_dyn[k].id, e->d_name) == 0) { have = true; break; }
        if (have) continue;
        char so[128];
        snprintf(so, sizeof so, APPS_DIR "/%s/app.so", e->d_name);
        void *h = dlopen(so, RTLD_NOW);
        if (!h) continue;
        void (*frame)(double) = (void (*)(double))dlsym(h, "imapp_frame");
        if (!frame) { dlclose(h); continue; }
        DynApp &a = g_dyn[g_dyn_n++];
        snprintf(a.id, sizeof a.id, "%s", e->d_name);
        const char *(*nm)(void) = (const char *(*)(void))dlsym(h, "imapp_name");
        snprintf(a.name, sizeof a.name, "%s", nm ? nm() : e->d_name);
        a.h = h; a.frame = frame;
        a.init = (int (*)(void))dlsym(h, "imapp_init");
        a.destroy = (void (*)(void))dlsym(h, "imapp_destroy");
        a.icon = (void (*)(void *, float, float, float))dlsym(h, "imapp_icon");
        a.sys = (int (*)(void))dlsym(h, "imapp_system");
        a.open = a.mini = a.ever = a.cascade = a.closing = a.destroyed = false;
        a.collapsed = a.uncoll = a.mfade = false;
        a.anim = 1; a.anim_t0 = a.close_t0 = 0;
        if (a.init) a.init();
    }
    closedir(d);
}

static void shell_draw_icon(int i, ImVec2 c, float s, ImU32 accent) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    if (i < 0) {
        ImVec2 t = c, r = ImVec2(c.x + s, c.y + s * 0.30f), b2 = ImVec2(c.x, c.y + s * 0.60f);
        ImVec2 l = ImVec2(c.x - s, c.y + s * 0.30f), bb = ImVec2(c.x, c.y + s * 1.05f);
        dl->AddTriangleFilled(l, b2, bb, IM_COL32(30, 80, 115, 255));
        dl->AddTriangleFilled(b2, r, bb, IM_COL32(45, 115, 165, 255));
        dl->AddTriangleFilled(t, r, b2, accent);
        dl->AddTriangleFilled(t, b2, l, IM_COL32(70, 150, 200, 255));
    } else {
        dl->AddRectFilled(ImVec2(c.x - s * 0.7f, c.y - s * 0.7f),
                          ImVec2(c.x + s * 0.7f, c.y + s * 0.7f), accent, 8.0f);
    }
}
static ImU32 g_app_accent[8] = {
    IM_COL32(120, 190, 240, 255), IM_COL32(120, 220, 190, 255),
    IM_COL32(160, 120, 220, 255), IM_COL32(230, 170, 60, 255),
    IM_COL32(90, 220, 140, 255), IM_COL32(220, 120, 220, 255),
    IM_COL32(200, 200, 210, 255), IM_COL32(240, 130, 110, 255),
};

/* ---- C-facing bridge (gldemo pump calls these) ---- */
void igui_mouse_pos(float x, float y) { ImGui::GetIO().AddMousePosEvent(x, y); }
void igui_mouse_btn(int btn, int down) { ImGui::GetIO().AddMouseButtonEvent(btn, down != 0); }
void igui_render(void) {
    ImGui::Render();
    ImVector<ImTextureData *> &texs = ImGui::GetPlatformIO().Textures;
    for (int i = 0; i < texs.size(); i++) {
        ImTextureData *tex = texs[i];
        if (!tex) continue;
        if (tex->Status == ImTextureStatus_WantCreate ||
            tex->Status == ImTextureStatus_WantUpdates)
            upload_texture(tex);
        else if (tex->Status == ImTextureStatus_WantDestroy) {
            unsigned did = (unsigned)(intptr_t)tex->TexID;
            if (did && pglDeleteTextures) pglDeleteTextures(1, &did);
            tex->SetStatus(ImTextureStatus_Destroyed);
        }
    }
    ImDrawData *dd = ImGui::GetDrawData();
    if (!dd) return;
    ImGuiIO &io = ImGui::GetIO();
    float fbw = io.DisplaySize.x, fbh = io.DisplaySize.y;
    if (dd->DisplaySize.x <= 0 || dd->DisplaySize.y <= 0) return;
    gl.Disable(GL_SCISSOR_TEST);
    gl.MatrixMode(GL_PROJECTION_M);
    gl.LoadIdentity();
    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl.Enable(GL_TEXTURE_2D);
    gl.Orthof(0, fbw, fbh, 0, -1, 1);
    gl.MatrixMode(GL_MODELVIEW_M);
    gl.LoadIdentity();
    for (int n = 0; n < dd->CmdListsCount; n++) {
        const ImDrawList *cmdl = dd->CmdLists[n];
        const ImDrawVert *vtx = cmdl->VtxBuffer.Data;
        const ImDrawIdx *idx = cmdl->IdxBuffer.Data;
        if (!vtx || !idx) continue;
        gl.EnableClientState(0x8074);
        gl.EnableClientState(0x8076);
        gl.EnableClientState(0x8078);
        gl.VertexPointer(2, GL_FLOAT, sizeof(ImDrawVert), &vtx->pos);
        gl.TexCoordPointer(2, GL_FLOAT, sizeof(ImDrawVert), &vtx->uv);
        gl.ColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ImDrawVert), &vtx->col);
        for (int ci = 0; ci < cmdl->CmdBuffer.Size; ci++) {
            const ImDrawCmd *cmd = &cmdl->CmdBuffer[ci];
            if (cmd->UserCallback) continue;
            unsigned ctid = (unsigned)(intptr_t)cmd->GetTexID();
            if (ctid) gl.BindTexture(GL_TEXTURE_2D, ctid);   /* per-cmd atlas */
            ImVec4 crs = cmd->ClipRect;
            if (crs.x < fbw && crs.y < fbh && crs.z > 0 && crs.w > 0) {
                gl.Scissor((int)crs.x, (int)(fbh - crs.w),
                           (int)(crs.z - crs.x), (int)(crs.w - crs.y));
                gl.Enable(GL_SCISSOR_TEST);
                gl.DrawElements(GL_TRIANGLES, (int)cmd->ElemCount, GL_UNSIGNED_SHORT,
                                (const void *)(idx + cmd->IdxOffset));
                gl.Disable(GL_SCISSOR_TEST);
            }
        }
    }
    gl.Disable(GL_BLEND);
    gl.Disable(GL_TEXTURE_2D);
}
void imrun_minimize_by_title(const char *title) {
    if (!title) return;
    const char *h = strstr(title, "###dyn");
    if (h) {
        int i = atoi(h + 6);
        if (i >= 0 && i < g_dyn_n) dyn_minimize(i);
    }
}
extern "C" ImFont *imrun_font_cn(void) { return g_cnfont; }
extern "C" ImFont *imrun_font_world(void) { return g_worldfont; }
extern "C" ImFont *imrun_font_kr(void) { return g_krfont; }
/* ---- UI display settings API (SETTINGS plugin) ---- */
extern "C" void imrun_ui_fontface(int idx) {   /* 0=MSYH 1=BITMAP (g_fonts 0/3) */
    if (idx == 0) ImGui::GetIO().FontDefault = g_fonts[0];
    if (idx == 1) ImGui::GetIO().FontDefault = g_fonts[3];
}
extern "C" void imrun_ui_fontsize(float px) {
    ImGuiStyle &st = ImGui::GetStyle();
    st.FontSizeBase = (float)(int)(px + 0.5f);
    st._NextFrameFontSizeBase = st.FontSizeBase;   /* per imgui_demo: BOTH */
}
extern "C" void imrun_ui_scale(float sc) {
    ImGui::GetStyle().FontScaleMain = sc;
}
static const char *shell_clock(void) {
    static char buf[8];
    time_t tt = time(NULL);
    struct tm *lt = localtime(&tt);
    if (lt) snprintf(buf, sizeof buf, "%02d:%02d", lt->tm_hour, lt->tm_min);
    else snprintf(buf, sizeof buf, "--:--");
    return buf;
}
static ImVec2 CalcTextSizeLocal(ImFont *f, float fs, const char *s) {
    return f->CalcTextSizeA(fs, 1000000.0f, 0.0f, s);
}
extern "C" int g_batt_pct;
extern "C" { extern int g_temp; }  /* defined in gldemo hw_thread */

static bool shell_begin_dyn(int di, ImVec2 pos, ImVec2 size) {
    DynApp &a = g_dyn[di];
    if (a.closing) { /* handled below: fade+destroy even when mini */ }
    else if (a.mfade) { /* handled below: fade-out to hidden */ }
    else if (!a.open || a.mini) return false;
    bool pushed_alpha = false;
    if (a.cascade) {
        a.cascade = false;
        static int casc_seq = 0;
        int n = casc_seq++ % 6;
        ImVec2 cp = ImVec2(24 + n * 42, 20 + n * 34);
        ImVec2 cs = ImVec2(size.x < 420 ? 420 : size.x, size.y < 300 ? 300 : size.y);
        if (cp.x + cs.x > 790) cp.x = 790 - cs.x;
        if (cp.y + cs.y > 424) cp.y = 424 - cs.y;
        ImGui::SetNextWindowPos(cp, ImGuiCond_Always);
        ImGui::SetNextWindowSize(cs, ImGuiCond_Always);
    }
    extern bool ImGui_IsWindowDocked(const char *);
    char ttl[80];
    snprintf(ttl, sizeof ttl, "%s###dyn%d", a.name, di);
    bool docked = ImGui_IsWindowDocked(ttl);
    if (a.closing) {
        if (docked) {  /* undock so the fade covers the CONTENT too
                          (docked content draws into the host's list) */
            extern bool ImGui_GetWindowPosByName(const char *, float *, float *);
            float wx, wy;
            if (ImGui_GetWindowPosByName(ttl, &wx, &wy))
                ImGui::SetNextWindowPos(ImVec2(wx, wy), ImGuiCond_Always);
            docked = false;
        }
        float t = (float)(now_double() - a.close_t0) / 0.5f;
        if (t >= 1.0f) {
            a.open = false; a.closing = false; a.mini = false;
            if (a.destroy && !a.destroyed) { a.destroy(); a.destroyed = true; }
            /* DECOUPLED: closing an app never touches desktop/UI focus -
               the desktop keeps whatever state the user left it in */
            return false;
        }
        float e = t * t * (3 - 2 * t);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f - e);
        pushed_alpha = true;
    } else if (a.anim < 1.0f && !docked) {
        float t = (float)(now_double() - a.anim_t0) / 0.5f;
        if (t >= 1.0f) a.anim = 1.0f;
        else {
            a.anim = t;
            float e = t * t * (3 - 2 * t);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, e);
            pushed_alpha = true;
        }
    }
    if (g_win_move && g_focus == di && (g_nudge_dx || g_nudge_dy)) {
        extern bool ImGui_GetWindowPosByName(const char *, float *, float *);
        float wx = 0, wy = 0;
        char tn[80];
        snprintf(tn, sizeof tn, "%s###dyn%d", a.name, di);
        if (ImGui_GetWindowPosByName(tn, &wx, &wy)) {
            ImGui::SetNextWindowPos(ImVec2(wx + g_nudge_dx, wy + g_nudge_dy), ImGuiCond_Always);
            g_nudge_dx = g_nudge_dy = 0;
        }
    }
    if (!pushed_alpha) {
        ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
    }
    if (a.uncoll) { ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always); a.uncoll = false; }
    char title[80];
    snprintf(title, sizeof title, "%s###dyn%d", a.name, di);
    bool p_keep = a.open && !a.closing;
    bool shown = ImGui::Begin(title, &p_keep);
    if (!p_keep && !a.closing) { a.open = true; a.closing = true; a.close_t0 = now_double(); }
    if (pushed_alpha) ImGui::PopStyleVar();
    /* Begin returns FALSE for COLLAPSED (and clipped) windows - that is NOT
       a close request. Only p_keep==false (the X button) closes. This was
       the collapse-vanish bug: collapsed -> Begin false -> dyn_close. */
    a.collapsed = !ImGui_IsWindowDocked(title) && ImGui::IsWindowCollapsed();
    if (!shown) { ImGui::End(); return false; }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) g_focus = di;
    return true;
}
static void shell_end_dyn(void) { ImGui::End(); }

void igui_build_ui(double t, float fps) {
    (void)fps;
    static bool s_loaded = false;
    if (!s_loaded) { s_loaded = true; settings_load(); }
    {   /* boot order: diag-heavy until proven */
        static bool s_ord_dbg = false;
        if (!s_ord_dbg && g_dyn_n > 0) {
            s_ord_dbg = true;
            alog_public("BOOTORDER order_n=%d dyn_n=%d", g_order_n, g_dyn_n);
        }
        if (g_order_n == 0 && g_dyn_n > 0) {
            order_sync();
            alog_public("BOOTORDER syncing+saving");
            settings_save();
        }
    }
    {   /* autosave ONLY when a setting actually changed (periodic writes
           caused the ~1s stutter: flash write in the UI thread) */
        static int s_last_sig = -999999;
        int sig = g_wall_mode + g_wall_variant * 8
                + (int)(g_a_win * 131) + (int)(g_a_bar * 197) + (int)(g_a_desk * 251)
                + (g_dock_covers_sysbar ? 10000 : 0);
        if (sig != s_last_sig) {
            settings_save();   /* incl. first frame: write defaults once */
            s_last_sig = sig;
        }
    }
    if (g_scan_req) {
        g_scan_req = false;
        int before = g_dyn_n;
        dyn_scan();
        if (g_dyn_n != before) order_sync();
    }

    {
        float dh = g_dock_covers_sysbar ? 480.0f : (480.0f - SYSBAR_H);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(800, dh));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("###dockhost", NULL,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBackground);
        ImGui::DockSpace(ImGui::GetID("DockSpace"), ImVec2(0, 0),
                         ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::End();
        ImGui::PopStyleVar(3);
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(800, 480 - SYSBAR_H));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03f, 0.05f, 0.09f, g_a_desk));
    ImGui::Begin("###desktop", NULL,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoNav);
    {
        ImDrawList *dl = ImGui::GetWindowDrawList();
        if (g_desk_filter[0]) {
            float fw = 240;
            dl->AddRectFilled(ImVec2(280, 6), ImVec2(280 + fw, 44), IM_COL32(20, 26, 38, 235), 8.0f);
            dl->AddRect(ImVec2(280, 6), ImVec2(280 + fw, 44), IM_COL32(90, 200, 240, 200), 8.0f, 0, 2.0f);
            char fl[72];
            snprintf(fl, sizeof fl, "> %s", g_desk_filter);
            float fy = 6 + (38 - ImGui::GetFontSize()) * 0.5f;
            dl->AddText(ImVec2(294, fy), IM_COL32(235, 240, 245, 255), fl);
            dl->AddText(ImVec2(280 + fw - 88, fy), IM_COL32(150, 160, 175, 200), "ENTER=open");
        }
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, vec4f(1, 1, 1, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, vec4f(1, 1, 1, 0.16f));
        const int cols = 4, cellw = 190, cellh = 118;
        static int drag_pos = -1;   /* edit mode: tile being dragged */
        for (int pos = 0; pos < g_order_n; pos++) {
            int i = g_order[pos];
            int col = pos % cols, row = pos / cols;
            float tx = 14 + col * cellw, ty = 12 + row * cellh;
            /* animated display position (snap on first appearance) */
            if (g_tile_x[i] < 0 || g_tile_y[i] < 0) { g_tile_x[i] = tx; g_tile_y[i] = ty; }
            g_tile_x[i] += (tx - g_tile_x[i]) * 0.28f;
            g_tile_y[i] += (ty - g_tile_y[i]) * 0.28f;
            float bx = g_tile_x[i], by = g_tile_y[i];
            if (g_desk_edit && drag_pos == pos) {  /* dragged tile follows finger */
                bx = ImGui::GetIO().MousePos.x - (cellw - 14) * 0.5f - ImGui::GetWindowPos().x;
                by = ImGui::GetIO().MousePos.y - (cellh - 12) * 0.5f - ImGui::GetWindowPos().y;
            }
            ImGui::SetCursorPos(ImVec2(bx, by));
            char hid[16];
            snprintf(hid, sizeof hid, "##desk%d", i);
            bool on = g_dyn[i].open && !g_dyn[i].mini;
            static double hold_t0[16] = {0};
            if (ImGui::InvisibleButton(hid, ImVec2(cellw - 14, cellh - 12))) {
                if (g_desk_edit) {
                    /* released a drag: drop on nearest slot -> swap */
                    if (drag_pos >= 0 && drag_pos != pos) {
                        order_swap(drag_pos, pos);
                        extern void imrun_settings_touch(void);
                        imrun_settings_touch();
                        imrun_vibra_ping(50);
                    }
                    drag_pos = -1;
                    if (!g_dyn[i].sys) {   /* plain tap (no drag) near X = uninstall */
                        ImVec2 mp = ImGui::GetIO().MousePos;
                        float cx = g_tile_x[i] + cellw - 26, cy = g_tile_y[i] + 6;
                        if ((mp.x-ImGui::GetWindowPos().x-cx)*(mp.x-ImGui::GetWindowPos().x-cx)+
                            (mp.y-ImGui::GetWindowPos().y-cy)*(mp.y-ImGui::GetWindowPos().y-cy) < 256) {
                            g_dyn_dead = i; imrun_vibra_ping(120);
                        }
                    }
                } else { g_desk_last = i; dyn_focus(i); }
            }
            if (g_desk_edit && ImGui::IsItemActive() && drag_pos < 0)
                drag_pos = pos;   /* pick up tile */
            /* LIVE gap insertion (Android/iOS): while dragging, the hovered
               slot becomes the drop slot - other tiles animate aside at once */
            if (g_desk_edit && drag_pos == pos && ImGui::IsItemActive()) {
                ImVec2 mp = ImGui::GetIO().MousePos;
                ImVec2 wo = ImGui::GetWindowPos();
                float mx = mp.x - wo.x, my = mp.y - wo.y;
                int best = -1; float bestd = 1e9f;
                for (int q = 0; q < g_order_n; q++) {
                    float cx = 14 + (q % 4) * 190 + 88, cy = 12 + (q / 4) * 118 + 53;
                    float d = (mx - cx) * (mx - cx) + (my - cy) * (my - cy);
                    if (d < bestd) { bestd = d; best = q; }
                }
                if (best >= 0 && best != drag_pos) {
                    order_swap(drag_pos, best);
                    imrun_settings_touch();
                    drag_pos = best;
                }
            }
            int hi = i < 16 ? i : 15;
            if (!g_desk_edit && ImGui::IsItemActive()) {
                if (hold_t0[hi] == 0) hold_t0[hi] = t;
                else if (t - hold_t0[hi] > 0.5) { g_desk_edit = true; imrun_vibra_ping(60); }
            } else hold_t0[hi] = 0;
            bool hot = ImGui::IsItemHovered();
            bool fmatch = true;
            if (g_desk_filter[0]) {
                char ub[40], uf[40];
                snprintf(ub, sizeof ub, "%s", g_dyn[i].name);
                for (char *q = ub; *q; q++) *q = (*q>='a'&&*q<='z') ? *q-32 : *q;
                snprintf(uf, sizeof uf, "%s", g_desk_filter);
                for (char *q = uf; *q; q++) *q = (*q>='a'&&*q<='z') ? *q-32 : *q;
                fmatch = strstr(ub, uf) != NULL;
            }
            ImVec2 p0 = ImVec2(bx, by), p1 = ImVec2(bx + cellw - 14, by + cellh - 12);
            if (g_desk_filter[0] && !fmatch) {
                dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 60), 10.0f);
                dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 18), 10.0f, 0, 1.0f);
            } else {
                dl->AddRectFilled(p0, p1,
                                  IM_COL32(on ? 40 : 255, on ? 90 : 255, on ? 130 : 255,
                                           hot ? 66 : 38), 10.0f);
                dl->AddRect(p0, p1,
                            (on || (g_desk_filter[0] && fmatch)) ? IM_COL32(90, 200, 240, 240)
                                                                 : IM_COL32(255, 255, 255, 45),
                            10.0f, 0, hot ? 2.5f : 1.5f);
            }
            if (i == g_focus && g_dyn[i].open) { /* THE focus ring (single) */
                dl->AddRect(ImVec2(bx - 3, by - 3), ImVec2(bx + cellw - 11, by + cellh - 9),
                            IM_COL32(120, 220, 255, 255), 12.0f, 0, 3.5f);
            }
            else if (i == g_desk_nav_sel && g_desk_nav_on && !g_desk_edit)
                dl->AddRect(ImVec2(bx - 3, by - 3), ImVec2(bx + cellw - 11, by + cellh - 9),
                            IM_COL32(120, 220, 255, 255), 12.0f, 0, 3.5f);
            if (g_dyn[i].icon)
                g_dyn[i].icon(dl, bx + (cellw - 14) * 0.5f, by + 34, 17.0f);
            else
                shell_draw_icon(-1, ImVec2(bx + (cellw - 14) * 0.5f, by + 34), 17.0f,
                                g_app_accent[i % 8]);
            char label[40];
            snprintf(label, sizeof label, "%s%s", g_dyn[i].name,
                     g_dyn[i].open ? (g_dyn[i].mini ? " -" : " *") : "");
            ImVec2 ts = ImGui::CalcTextSize(label);
            dl->AddText(ImVec2(bx + ((cellw - 14) - ts.x) * 0.5f, by + 76),
                        IM_COL32(235, 240, 245, 235), label);
            if (g_desk_edit && !g_dyn[i].sys) {
                float cx = bx + cellw - 26, cy = by + 6;
                dl->AddCircleFilled(ImVec2(cx, cy), 11, IM_COL32(220, 50, 50, 255));
                dl->AddLine(ImVec2(cx - 5, cy - 5), ImVec2(cx + 5, cy + 5), IM_COL32(255,255,255,255), 2.5f);
                dl->AddLine(ImVec2(cx - 5, cy + 5), ImVec2(cx + 5, cy - 5), IM_COL32(255,255,255,255), 2.5f);
            }
        }
        ImGui::PopStyleColor(3);
    }
    if (g_desk_edit) {
        ImGui::SetCursorPos(ImVec2(700, 398));
        if (ImGui::Button("DONE", ImVec2(86, 30))) g_desk_edit = false;
        ImGui::SetCursorPos(ImVec2(10, 404));
        ImGui::TextColored(vec4f(0.9f, 0.5f, 0.4f, 0.9f), "EDIT MODE: tap red X to uninstall");
    } else {
        ImGui::SetCursorPos(ImVec2(10, 404));
        ImGui::TextColored(vec4f(0.55f, 0.62f, 0.72f, 0.85f),
                           "IMRUN v" IMRUN_VER " - imgui %s  (type to filter, long-press to edit)",
                           ImGui::GetVersion());
    }
    if (g_dyn_dead >= 0) {
        int di = g_dyn_dead; g_dyn_dead = -1;
        DynApp &a = g_dyn[di];
        a.open = false;
        if (a.destroy && !a.destroyed) { a.destroy(); a.destroyed = true; }
        dlclose(a.h);
        /* REVERSIBLE uninstall: move to the local app repo, not rm -rf */
        char cmd[160];
        snprintf(cmd, sizeof cmd,
                 "mkdir -p /home/user/imrun/repo && rm -rf /home/user/imrun/repo/%s && mv "
                 APPS_DIR "/%s /home/user/imrun/repo/%s", a.id, a.id, a.id);
        system(cmd);
        for (int k = di; k < g_dyn_n - 1; k++) g_dyn[k] = g_dyn[k + 1];
        g_dyn_n--;
        if (g_focus >= di) g_focus = -1;
        g_desk_last = -1;
        order_sync();      /* stale indices poisoned grid + saved order */
        settings_save();
    }
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::SetNextWindowPos(ImVec2(0, 480 - SYSBAR_H));
    ImGui::SetNextWindowSize(ImVec2(800, SYSBAR_H));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3, 3));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, vec4f(0.08f, 0.10f, 0.15f, g_a_bar));
    ImGui::Begin("###sysbar", NULL,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);
    {
        ImDrawList *dl = ImGui::GetWindowDrawList();
        ImU32 line = IM_COL32(235, 240, 245, 255);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        if (ImGui::InvisibleButton("##back", ImVec2(48, 42))) {
            g_back_hl = 1.0f;
            if (ImGui_OpenPopupCount() > 0) ImGui_CloseTopPopup();
            else igui_close_focused();
        }
        {
            ImVec2 c0 = ImGui::GetItemRectMin();
            ImVec2 c = ImVec2(c0.x + 24, c0.y + 21);
            if (g_back_hl > 0)
                dl->AddEllipseFilled(c, ImVec2(24, 17), IM_COL32(120, 200, 255, (int)(150 * g_back_hl)), 24);
            dl->AddTriangleFilled(ImVec2(c.x + 6, c.y - 12), ImVec2(c.x + 6, c.y + 12),
                                  ImVec2(c.x - 8, c.y), line);
        }
        ImGui::SameLine();
        if (ImGui::InvisibleButton("##home", ImVec2(48, 42)))
            { g_home_hl = 1.0f; dyn_minimize_all(); }
        {
            ImVec2 c0 = ImGui::GetItemRectMin();
            ImVec2 c = ImVec2(c0.x + 24, c0.y + 21);
            if (g_home_hl > 0)
                dl->AddEllipseFilled(c, ImVec2(24, 17), IM_COL32(120, 200, 255, (int)(150 * g_home_hl)), 24);
            dl->AddCircle(c, 12.0f, line, 40, 3.0f);
        }
        g_back_hl *= 0.92f; if (g_back_hl < 0.02f) g_back_hl = 0;
        g_home_hl *= 0.92f; if (g_home_hl < 0.02f) g_home_hl = 0;
        ImGui::SameLine();
        /* running apps: dock-node-aware chips. ALL open windows have a chip
           (minimized ones stay visible, dim) - a minimized app/group must
           always be restorable from the taskbar.
           floating windows -> one chip each; windows docked into the same
           node -> ONE group chip (label = selected member +N). */
        {
            int grp_node[8] = {0};
            static int grp_mem[8][8];
            int grp_cnt = 0;
            int vis[8], vis_n = 0;
            static int grp_n[8] = {0};
            for (int i = 0; i < g_dyn_n; i++) {
                if (!g_dyn[i].open) continue;   /* mini included: restorable */
                char tn[80];
                snprintf(tn, sizeof tn, "%s###dyn%d", g_dyn[i].name, i);
                extern int ImGui_WindowDockNodeId(const char *);
                int nid = ImGui_WindowDockNodeId(tn);
                if (nid == 0) { vis[vis_n++] = i; continue; }
                int g = -1;
                for (int q = 0; q < grp_cnt; q++) if (grp_node[q] == nid) g = q;
                if (g < 0 && grp_cnt < 8) { g = grp_cnt++; grp_node[g] = nid; grp_n[g] = 0; }
                if (g >= 0 && grp_n[g] < 8) grp_mem[g][grp_n[g]++] = i;
            }
            /* shared chip renderer: members[] + count; click = focus-all /
               minimize-all; long-press = close-all. Color per state with lerp. */
            static float chip_rgb[16][3];
            auto draw_chip = [&](int *mem, int n, int uid, int node_id) {
                int sel = mem[0];
                for (int q = 0; q < n; q++) if (mem[q] == g_focus) sel = g_focus;
                bool all_mini = true;
                for (int q = 0; q < n; q++) if (!g_dyn[mem[q]].mini) all_mini = false;
                bool foc = (!all_mini && g_focus == sel);
                bool folded = (n == 1 && g_dyn[mem[0]].collapsed);
                char label[72];
                if (n > 1) snprintf(label, sizeof label, "%s+%d", g_dyn[sel].name, n - 1);
                else snprintf(label, sizeof label, "%s", g_dyn[sel].name);
                float tgt[3];
                if (folded)      { tgt[0]=0.45f; tgt[1]=0.35f; tgt[2]=0.68f; }
                else if (all_mini){ tgt[0]=0.06f; tgt[1]=0.08f; tgt[2]=0.12f; }
                else if (foc)    { tgt[0]=0.16f; tgt[1]=0.42f; tgt[2]=0.60f; }
                else             { tgt[0]=0.12f; tgt[1]=0.16f; tgt[2]=0.24f; }
                int cq = uid < 16 ? uid : 15;
                for (int q = 0; q < 3; q++)
                    chip_rgb[cq][q] += (tgt[q] - chip_rgb[cq][q]) * 0.12f;
                float a = all_mini ? 0.5f : 1.0f;
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      vec4f(chip_rgb[cq][0], chip_rgb[cq][1], chip_rgb[cq][2], a));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, vec4f(0.16f, 0.60f, 0.32f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, vec4f(0.20f, 0.30f, 0.42f, 1));
                ImGui::PushStyleColor(ImGuiCol_Border, vec4f(0.45f, 0.55f, 0.68f, all_mini ? 0.35f : 0.8f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
                char tag[96];
                snprintf(tag, sizeof tag, "%s##tb%d", label, uid);
                static double chip_t0[16] = {0};
                int ci = cq;
                if (ImGui::Button(tag, ImVec2(0, 42))) {
                    if (chip_t0[ci] != -1) {
                        if (all_mini) {
                            for (int q = 0; q < n; q++) dyn_focus(mem[q]);  /* restore all */
                            g_focus = sel;
                        }
                        else if (!foc) { for (int q = 0; q < n; q++) dyn_focus(mem[q]); g_focus = sel; }
                        else { for (int q = 0; q < n; q++) dyn_minimize(mem[q]); }
                    }
                    chip_t0[ci] = 0;
                }
                if (ImGui::IsItemActive()) {
                    if (chip_t0[ci] == 0) chip_t0[ci] = ImGui::GetTime();
                    else if (chip_t0[ci] > 0 && ImGui::GetTime() - chip_t0[ci] > 0.5f) {
                        chip_t0[ci] = -1;
                        imrun_vibra_ping(80);
                        for (int q = 0; q < n; q++) dyn_close(mem[q]);  /* close whole chip */
                    }
                } else if (chip_t0[ci] > 0) chip_t0[ci] = 0;
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(4);
                ImGui::SameLine();
            };
            /* floating chips */
            for (int q = 0; q < vis_n; q++) {
                int i = vis[q];
                draw_chip(&i, 1, i, 0);
            }
            /* group chips: one per dock node */
            for (int g = 0; g < grp_cnt; g++) {
                if (grp_n[g] == 0) continue;
                draw_chip(grp_mem[g], grp_n[g], 100 + g, grp_node[g]);
            }
        }
        char bat[12] = "--", tmp[8] = "--", clk[8] = "--:--";
        if (g_batt_pct >= 0) snprintf(bat, sizeof bat, "%d", g_batt_pct);
        if (g_temp >= 0) snprintf(tmp, sizeof tmp, "%d", g_temp);
        snprintf(clk, sizeof clk, "%s", shell_clock());
        ImFont *f = ImGui::GetFont();
        float fs = ImGui::GetFontSize();
        float wy = ImGui::GetWindowPos().y;
        float ytxt = wy + 21 - fs * 0.5f, yc = wy + 21;
        ImVec2 tc = CalcTextSizeLocal(f, fs, clk);
        if (g_batt_pct > 100) g_batt_pct = 100;
        snprintf(bat, sizeof bat, "%d%%", g_batt_pct < 0 ? 0 : g_batt_pct);
        ImVec2 tb = CalcTextSizeLocal(f, fs, bat);
        ImVec2 tt = CalcTextSizeLocal(f, fs, tmp);
        ImVec2 tC = CalcTextSizeLocal(f, fs, "C");
        const float GAP = 20.0f;
        float clk_tx = 792 - tc.x, clk_ix = clk_tx - 4 - 18;
        float bat_tx = clk_ix - GAP - tb.x, bat_ix = bat_tx - 4 - 34;
        float tmp_Cx = bat_ix - GAP - tC.x, tmp_nx = tmp_Cx - 2 - tt.x, tmp_ix = tmp_nx - 4 - 8;
        dl->AddCircleFilled(ImVec2(tmp_ix + 4, yc + 9), 4.0f, IM_COL32(235, 90, 80, 255));
        dl->AddLine(ImVec2(tmp_ix + 4, yc + 9), ImVec2(tmp_ix + 4, yc - 8), IM_COL32(235, 90, 80, 255), 4.0f);
        dl->AddText(ImVec2(tmp_nx, ytxt), line, tmp);
        dl->AddText(ImVec2(tmp_Cx, ytxt), IM_COL32(180, 190, 200, 230), "C");
        dl->AddRect(ImVec2(bat_ix, yc - 7), ImVec2(bat_ix + 30, yc + 9), line, 3.0f, 0, 2.0f);
        dl->AddRectFilled(ImVec2(bat_ix + 30, yc - 3), ImVec2(bat_ix + 34, yc + 5), line, 1.0f);
        int pct = g_batt_pct < 0 ? 0 : g_batt_pct;
        ImU32 col = pct > 40 ? IM_COL32(110, 220, 130, 255)
                  : pct > 15 ? IM_COL32(235, 190, 80, 255) : IM_COL32(235, 90, 80, 255);
        if (g_batt_pct >= 0)
            dl->AddRectFilled(ImVec2(bat_ix + 3, yc - 4), ImVec2(bat_ix + 3 + 24 * pct / 100, yc + 6), col, 2.0f);
        dl->AddText(ImVec2(bat_tx, ytxt), line, bat);
        dl->AddCircle(ImVec2(clk_ix + 9, yc), 9.0f, line, 32, 2.0f);
        dl->AddLine(ImVec2(clk_ix + 9, yc), ImVec2(clk_ix + 9, yc - 6), line, 2.0f);
        dl->AddLine(ImVec2(clk_ix + 9, yc), ImVec2(clk_ix + 13.5f, yc + 2), line, 2.0f);
        dl->AddText(ImVec2(clk_tx, ytxt), line, clk);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    for (int di = 0; di < g_dyn_n; di++) {
        DynApp &a = g_dyn[di];
        if (!shell_begin_dyn(di, ImVec2(60 + di * 30, 40 + di * 24), ImVec2(440, 320)))
            continue;
        a.frame(t);
        if (a.closing) {
            /* guaranteed content fade: scale EVERY vertex alpha in this
               window's draw list (text+widgets included - style alpha alone
               does not reach plugin content) */
            float e = (float)(now_double() - a.close_t0) / 0.5f;
            if (e > 1) e = 1;
            float k = 1.0f - (e * e * (3 - 2 * e));
            ImDrawList *dl = ImGui::GetWindowDrawList();
            for (int v = 0; v < dl->VtxBuffer.Size; v++) {
                unsigned c = dl->VtxBuffer[v].col;
                unsigned al = (unsigned)(((c >> 24) & 0xFF) * k);
                dl->VtxBuffer[v].col = (c & 0x00FFFFFF) | (al << 24);
            }
        }
        shell_end_dyn();
    }
}

} // extern "C"
