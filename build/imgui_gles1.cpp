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
    unsigned id = 0;
    gl.GenTextures(1, &id);
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

double now_double(void) { return (double)time(NULL); }

void ui_stress_toggle(void) {
    ImGuiStyle &st = ImGui::GetStyle();
    static float v = 1.0f;
    v = (v > 1.1f) ? 1.0f : 1.35f;
    st.FontScaleMain = v; /* forces full glyph rebake = heaviest UI op */
}

#include <dlfcn.h>
#include <dirent.h>

/* ============ hot-load: app.so dirs in /home/user/imrun/apps, no restart =====
   Main binary links with --export-dynamic: plugin .so files reference ImGui
   symbols directly, resolved from the exe at dlopen time. Rescan ~every 2s. */
struct DynApp {
    char id[40], name[40];
    void *h;
    void (*frame)(double t);          /* required: draw ImGui content */
    int (*init)(void);                /* optional */
    void (*destroy)(void);            /* optional */
    void (*icon)(void *dl, float cx, float cy, float s); /* optional: vector icon */
    bool open, mini, ever, cascade, closing;
    float anim; double anim_t0; double close_t0;
};
static DynApp g_dyn[8];
static int g_dyn_n = 0;
static int g_dyn_scan_cnt = 0;
extern "C" int g_wall_mode, g_wall_variant; /* gldemo wallpaper engine */

static void dyn_scan(void) {
    DIR *d = opendir("/home/user/imrun/apps");
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) && g_dyn_n < 8) {
        if (e->d_name[0] == '.') continue;
        bool have = false;
        for (int k = 0; k < g_dyn_n; k++)
            if (strcmp(g_dyn[k].id, e->d_name) == 0) { have = true; break; }
        if (have) continue;
        char so[128];
        snprintf(so, sizeof so, "/home/user/imrun/apps/%s/app.so", e->d_name);
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
        a.open = a.mini = a.ever = a.cascade = false;
        a.anim = 1; a.anim_t0 = 0;
        if (a.init) a.init();
    }
    closedir(d);
}

/* ============ IMRUN shell state (declared early: key path uses it) ======= */
struct ShellApp { const char *name; bool open; bool mini; bool cascade; bool ever;
                  bool closing; double close_t0;
                  float anim; double anim_t0;   /* open/restore transition */ };
static ShellApp g_apps[] = {
    {"SYSTEM", false, false, false, false, false, 0, 1, 0},
    {"APPMGR", false, false, false, false, false, 0, 1, 0},
};
/* desktop edit mode: long-press an icon -> red X badges (uninstall) */
static bool g_desk_edit = false;
static char g_desk_filter[32] = ""; /* desktop keyboard app filter */
static int g_desk_nav_sel = -1;   /* desktop manual nav selection */
static bool g_win_move = false;        /* Ctrl+M: arrow-key window move mode */
static float g_nudge_dx = 0, g_nudge_dy = 0;
static int g_dyn_dead = -1;         /* index pending dlclose+rm at frame start */
static const int NAPPS = (int)(sizeof g_apps / sizeof g_apps[0]);
static int g_focus = -1;          /* focused window, -1 = desktop only */
#define IMRUN_VER "0.3"
/* adjustable transparency (WALLPAP app): window bg / taskbar / desktop tint */
float g_a_win = 0.88f, g_a_bar = 0.80f, g_a_desk = 0.0f;
bool g_dock_covers_sysbar = false;  /* dock/maximize respects the 48px sysbar
                                       unless enabled in SYSTEM settings */
bool g_ctrl_down = false;         /* fed by igui_special_key / X keycode 37 */
static void shell_close(int i);       /* defined below */
static void shell_focus(int i);
static void shell_minimize(int i);
/* patched-ImGui native minimize hook: window title "NAME###appN" -> N */
void imrun_minimize_by_title(const char *title);
extern "C" void imrun_audio_stop_all(void); /* gldemo side: TONEGEN cleanup */
void igui_ctrl_combo(char c);         /* from X11 pump: ctrl+letter combos */

void igui_key_char(const char *utf8) {
    /* desktop focus + typing = app filter (like a launcher search) */
    if (g_focus < 0 && utf8 && utf8[0] >= 0x20 && !utf8[1] &&
        strlen(g_desk_filter) + 1 < sizeof g_desk_filter) {
        char c = utf8[0];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            size_t l = strlen(g_desk_filter);
            g_desk_filter[l] = c;
            g_desk_filter[l + 1] = 0;
            return;
        }
    }
    /* plain Ctrl+W/M arrives here as control bytes 0x17/0x0d - the printable
       filter in the pump drops them, combos go through igui_ctrl_combo */
    ImGui::GetIO().AddInputCharactersUTF8(utf8);
}

/* special hardware keys (Enter/Backspace/arrows/modifiers). Keysyms are raw
   X11 values (no X11 headers): 0xff08 BkSp, 0xff0d Return, 0xff1b Esc, ...
   Emulator evidence: N900 Fn key is standalone keycode evdev100/X108, and
   XKB composes Fn+key into the charsym - so only modifiers need wiring. */
void igui_special_key(unsigned ks, int down) {
    ImGuiIO &io = ImGui::GetIO();
    /* desktop manual nav: arrows move the selection, Enter opens */
    if (down && g_focus < 0 && !g_desk_edit) {
        int total = NAPPS + g_dyn_n;
        if (total > 0) {
            if (g_desk_nav_sel < 0) g_desk_nav_sel = 0;
            if (ks == 0xff51 && g_desk_nav_sel % 4 > 0) g_desk_nav_sel--;
            else if (ks == 0xff53 && g_desk_nav_sel % 4 < 3 && g_desk_nav_sel + 1 < total) g_desk_nav_sel++;
            else if (ks == 0xff52 && g_desk_nav_sel >= 4) g_desk_nav_sel -= 4;
            else if (ks == 0xff54 && g_desk_nav_sel + 4 < total) g_desk_nav_sel += 4;
            else if (ks == 0xff0d || ks == 0xff0a) {
                if (g_desk_nav_sel < NAPPS) shell_focus(g_desk_nav_sel);
                else {
                    DynApp &da = g_dyn[g_desk_nav_sel - NAPPS];
                    if (!da.ever) { da.cascade = true; da.ever = true; }
                    da.open = true; da.mini = false; da.anim = 0; da.anim_t0 = now_double();
                }
                g_desk_nav_sel = -1;
                return;
            }
        }
        if (ks == 0xff51 || ks == 0xff52 || ks == 0xff53 || ks == 0xff54)
            return; /* consumed by desktop nav */
    }
    /* Ctrl+M move mode: arrows nudge the focused window (24px steps) */
    if (down && g_win_move && g_focus >= 0) {
        if (ks == 0xff51) { g_nudge_dx -= 24; return; }
        if (ks == 0xff53) { g_nudge_dx += 24; return; }
        if (ks == 0xff52) { g_nudge_dy -= 24; return; }
        if (ks == 0xff54) { g_nudge_dy += 24; return; }
    }
    /* desktop filter control: BKSP=del, ENTER=open first match, ESC=clear */
    if (down && g_focus < 0 && g_desk_filter[0]) {
        if (ks == 0xff08) { /* BackSpace */
            size_t l = strlen(g_desk_filter);
            if (l) g_desk_filter[l - 1] = 0;
            return;
        }
        if (ks == 0xff0d || ks == 0xff0a) { /* Return: open first match */
            char uf[32];
            snprintf(uf, sizeof uf, "%s", g_desk_filter);
            for (char *q = uf; *q; q++) *q = (*q >= 'a' && *q <= 'z') ? *q - 32 : *q;
            for (int i = 0; i < NAPPS + g_dyn_n; i++) {
                const char *nm = i < NAPPS ? g_apps[i].name : g_dyn[i - NAPPS].name;
                char ub[40];
                snprintf(ub, sizeof ub, "%s", nm);
                for (char *q = ub; *q; q++) *q = (*q >= 'a' && *q <= 'z') ? *q - 32 : *q;
                if (strstr(ub, uf)) {
                    if (i < NAPPS) shell_focus(i);
                    else {
                        DynApp &da = g_dyn[i - NAPPS];
                        if (!da.ever) { da.cascade = true; da.ever = true; }
                        da.open = true; da.mini = false;
                        da.anim = 0; da.anim_t0 = now_double();
                    }
                    break;
                }
            }
            g_desk_filter[0] = 0;
            return;
        }
        if (ks == 0xff1b) { /* Escape */
            g_desk_filter[0] = 0;
            return;
        }
    }
    ImGuiKey k = ImGuiKey_None;
    switch (ks) {
    case 0xff08: k = ImGuiKey_Backspace; break;        /* BackSpace */
    case 0xff0a: case 0xff0d: k = ImGuiKey_Enter; break; /* Return/Linefeed */
    case 0xff1b: k = ImGuiKey_Escape; break;
    case 0xff09: k = ImGuiKey_Tab; break;
    case 0xff51: k = ImGuiKey_LeftArrow; break;
    case 0xff52: k = ImGuiKey_UpArrow; break;
    case 0xff53: k = ImGuiKey_RightArrow; break;
    case 0xff54: k = ImGuiKey_DownArrow; break;
    case 0xff55: k = ImGuiKey_PageUp; break;
    case 0xff56: k = ImGuiKey_PageDown; break;
    case 0xff57: k = ImGuiKey_End; break;
    case 0xff58: k = ImGuiKey_Home; break;
    case 0xff63: case 0xffff: k = ImGuiKey_Delete; break; /* Insert/Del */
    case 0xffe1: case 0xffe2: k = ImGuiMod_Shift; break;
    case 0xffe3: case 0xffe4: case 0xffe5: k = ImGuiMod_Ctrl; break;
    case 0xffe7: case 0xffe8: case 0xffe9: case 0xff7e: k = ImGuiMod_Alt; break;
    }
    if (k == ImGuiMod_Ctrl) g_ctrl_down = (down != 0);
    if (k != ImGuiKey_None)
        io.AddKeyEvent(k, down != 0);
}

void igui_mouse_pos(float x, float y) {
    ImGuiIO &io = ImGui::GetIO();
    io.AddMousePosEvent(x, y);
}
void igui_mouse_btn(int btn, int down) {
    ImGuiIO &io = ImGui::GetIO();
    io.AddMouseButtonEvent(btn, down != 0);
}

void igui_render(void) {
    ImGui::Render();
    // upload any textures the font system prepared this frame
    ImVector<ImTextureData *> &texs = ImGui::GetPlatformIO().Textures;
    for (int i = 0; i < texs.size(); i++) {
        ImTextureData *tex = texs[i];
        if (!tex) continue;
        if (tex->Status == ImTextureStatus_WantCreate ||
            tex->Status == ImTextureStatus_WantUpdates)
            upload_texture(tex);
        else if (tex->Status == ImTextureStatus_WantDestroy)
            tex->SetStatus(ImTextureStatus_Destroyed);
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
    gl.Orthof(0, fbw, fbh, 0, -1, 1); /* y-down: ImGui screen space */
    gl.MatrixMode(GL_MODELVIEW_M);
    gl.LoadIdentity();
    gl.BindTexture(GL_TEXTURE_2D, fontTex);
    for (int n = 0; n < dd->CmdListsCount; n++) {
        const ImDrawList *cmdl = dd->CmdLists[n];
        const ImDrawVert *vtx = cmdl->VtxBuffer.Data;
        const ImDrawIdx *idx = cmdl->IdxBuffer.Data;
        if (!vtx || !idx) continue;
        gl.EnableClientState(0x8074 /*GL_VERTEX_ARRAY*/);
        gl.EnableClientState(0x8076 /*GL_COLOR_ARRAY*/);
        gl.EnableClientState(0x8078 /*GL_TEXTURE_COORD_ARRAY*/);
        gl.VertexPointer(2, GL_FLOAT, sizeof(ImDrawVert), &vtx->pos);
        gl.TexCoordPointer(2, GL_FLOAT, sizeof(ImDrawVert), &vtx->uv);
        gl.ColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ImDrawVert), &vtx->col);
        for (int ci = 0; ci < cmdl->CmdBuffer.Size; ci++) {
            const ImDrawCmd *cmd = &cmdl->CmdBuffer[ci];
            if (cmd->UserCallback) continue;
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

// ---------------- demo UI ----------------
extern "C" {
extern int audio_on;
extern char last_key[64];
extern char hw[8][96];
extern int hw_n;
extern void audio_toggle(int on);
extern void audio_stop_full(void);
extern void vibra_ping(int ms);
extern void vibra_stop(void);
extern int audio_start(int seconds);
extern void audio_ui_start(int seconds); /* async: flags only, ~0us */
extern void audio_ui_stop(void);
double now_double();
}
static inline ImVec4 vec4f(float x, float y, float z, float w) { return ImVec4(x, y, z, w); }
#define ImVec2_to_imvec_helper vec4f
static float g_plot[64];
static float hue = 0.4f;

/* ============ IMRUN shell: desktop + bottom system bar + apps ============
   resistive screen = mouse logic: pure clicks, >=48px targets, no gestures */
/* app lifecycle: close = destroy. All static/app state resets to
   first-launch so reopening is a fresh instance - nothing survives a close.
   (Today the apps are in-process; when .imapp dlopen lands, this is where
   destroy()+dlclose goes. Same rule, bigger hammer.) */
static void shell_app_destroy(int i);
static void shell_close(int i) {   /* starts the fade-OUT, finalize at 0 */
    ShellApp &a = g_apps[i];
    if (!a.open) return;
    if (!a.closing) { a.closing = true; a.close_t0 = now_double(); }
    if (g_focus == i) g_focus = -1;
}
static void shell_minimize(int i) {
    g_apps[i].mini = true;
    if (g_focus == i) g_focus = -1;
}
void imrun_minimize_by_title(const char *title) {
    if (!title) return;
    const char *h = strstr(title, "###dyn");
    if (h) {
        int i = atoi(h + 6);
        if (i >= 0 && i < g_dyn_n) g_dyn[i].mini = true;
        return;
    }
    h = strstr(title, "###app");
    if (!h) return;
    int i = atoi(h + 6);
    if (i >= 0 && i < NAPPS) shell_minimize(i);
}
/* reset an app's static state = "freed". Add per-app reset blocks here;
   anything NOT reset here would survive close - keep this exhaustive. */
#include <string.h>
static void shell_app_destroy(int i) {
    (void)i; /* system apps hold no resources; plugins own their destroy() */
}

/* font handles for the FONTS plugin (g_*font are file-static) */
extern "C" ImFont *imrun_font_cn(void) { return g_cnfont; }
extern "C" ImFont *imrun_font_world(void) { return g_worldfont; }
extern "C" ImFont *imrun_font_kr(void) { return g_krfont; }
static void shell_minimize_all(void) {
    for (int i = 0; i < NAPPS; i++) g_apps[i].mini = true;
    for (int k = 0; k < g_dyn_n; k++) g_dyn[k].mini = true;
    g_focus = -1;
}
static int g_cascade_seq = 0;         /* new windows stack from top-left */
static bool g_uncollapse[16];  /* explicit open always un-collapses (stale
                                    ini Collapsed=1 made apps look dead) */
static void shell_focus(int i) {
    if (i < 0 || i >= NAPPS) return; /* out-of-range write = memory corruption */
    g_desk_filter[0] = 0;   /* any open action dismisses the filter */
    ShellApp &a = g_apps[i];
    if (i >= 0 && i < 16) g_uncollapse[i] = true;
    if (!a.ever) {                    /* cascade ONLY on first-ever open:
                                       afterwards imgui.ini restores the
                                       user's saved pos/size verbatim */
        a.cascade = true;
        a.ever = true;
    }
    a.open = true; a.mini = false;
    a.anim = 0; a.anim_t0 = now_double();
}
/* ctrl+letter combos (X11 pump path; N900 ctrl = X keycode 37) */
int igui_desk_nav_index(void) { return g_desk_nav_sel; }
void igui_ctrl_combo(char c) {
    printf("CTRL+%c (focus=%d)\n", c, g_focus); fflush(stdout);
    if (c == 'w' || c == 'W') { if (g_focus >= 0) shell_close(g_focus); }
    else if (c == 't' || c == 'T') { /* Ctrl+T: cycle focused window */
        int order[24], n = 0;
        for (int i = 0; i < NAPPS; i++)
            if (g_apps[i].open && !g_apps[i].mini && !g_apps[i].closing) order[n++] = i;
        for (int k = 0; k < g_dyn_n; k++)
            if (g_dyn[k].open && !g_dyn[k].mini && !g_dyn[k].closing) order[n++] = NAPPS + k;
        if (!n) return;
        int cur = -1;
        for (int k = 0; k < n; k++) if (order[k] == g_focus) cur = k;
        int nxt = order[(cur + 1) % n];
        if (nxt < NAPPS) shell_focus(nxt);
        else {
            DynApp &a = g_dyn[nxt - NAPPS];
            if (!a.ever) { a.cascade = true; a.ever = true; }
            a.open = true; a.mini = false; a.anim = 0; a.anim_t0 = now_double();
            g_focus = nxt;
        }
    }
    else if (c == 'm' || c == 'M') { /* Ctrl+M: toggle window move mode */
        g_win_move = !g_win_move;
        g_nudge_dx = g_nudge_dy = 0;
        vibra_ping(50);
    }
}
void igui_close_focused(void) { if (g_focus >= 0) shell_close(g_focus); }

/* device-verified paths: CPU temp = omap34xx_temp via hwmon0 (raw °C, NOT
   millidegrees); battery % = HAL/BME daemon, read by gldemo's batt_reader
   thread into g_batt_pct (no sysfs node for bq27200 on Fremantle) */
extern "C" int g_batt_pct;
static int g_temp = -1;
static void shell_read_sysfs(void) {
    static int cnt = 0;
    if (++cnt % 90) return;
    char line[64];
    FILE *f = fopen("/sys/class/hwmon/hwmon0/device/temp1_input", "r");
    if (f) { if (fgets(line, sizeof line, f)) g_temp = atoi(line); fclose(f); }
}

/* one app window: Begin with close + touch-sized buttons inside.
   open/restore transition: 220ms ease from the taskbar area + fade-in */
static ImVec2 CalcTextSizeLocal(ImFont *f, float fs, const char *s);
static bool shell_begin_app(int i, ImVec2 pos, ImVec2 size) {
    ShellApp &a = g_apps[i];
    if ((!a.open && !a.closing) || a.mini) return false;
    bool pushed_alpha = false;
    if (a.cascade) {
        /* first-ever open: cascade position + ALSO fades in (anim set by focus) */
        int n = g_cascade_seq++ % 6;
        a.cascade = false;
        ImVec2 cp = ImVec2(24 + n * 42, 20 + n * 34);
        ImVec2 cs = ImVec2(size.x < 400 ? 400 : size.x, size.y < 280 ? 280 : size.y);
        if (cp.x + cs.x > 790) cp.x = 790 - cs.x;
        if (cp.y + cs.y > 424) cp.y = 424 - cs.y;
        ImGui::SetNextWindowPos(cp, ImGuiCond_Always);
        ImGui::SetNextWindowSize(cs, ImGuiCond_Always);
    }
    if (a.closing) {
        /* fade-OUT close: keep rendering until gone, then finalize */
        float t = (float)(now_double() - a.close_t0) / 0.5f;
        if (t >= 1.0f) {
            a.open = false; a.closing = false; a.mini = false;
            shell_app_destroy(i);
            return false;
        }
        float e = t * t * (3 - 2 * t);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f - e);
        pushed_alpha = true;
    } else if (a.anim < 1.0f) {
        /* reopen/restore: ALPHA FADE ONLY. Never force pos/size here -
           SetNextWindowPos during the animation stomps the layout the user
           saved in imrun.ini (that was the bottom-left-reappear bug) */
        float t = (float)(now_double() - a.anim_t0) / 0.5f;
        if (t >= 1.0f) {
            a.anim = 1.0f;
        } else {
            a.anim = t;
            float e = t * t * (3 - 2 * t); /* smoothstep: born from nothing */
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, e);
            pushed_alpha = true;
        }
    }
    if (g_win_move && g_focus == i && (g_nudge_dx || g_nudge_dy)) {
        extern bool ImGui_GetWindowPosByName(const char *, float *, float *);
        float wx = 0, wy = 0;
        char tname[64];
        snprintf(tname, sizeof tname, "%s###app%d", a.name, i);
        if (ImGui_GetWindowPosByName(tname, &wx, &wy)) {
            ImGui::SetNextWindowPos(ImVec2(wx + g_nudge_dx, wy + g_nudge_dy),
                                    ImGuiCond_Always); /* Always => undocks */
            g_nudge_dx = g_nudge_dy = 0;
        }
    }
    if (!pushed_alpha && !a.cascade) {
        ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
    }
    char title[64];
    snprintf(title, sizeof title, "%s###app%d", a.name, i);
    if (i < 16 && g_uncollapse[i]) {
        g_uncollapse[i] = false;
        ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
    }
    /* semi-transparent app windows (alpha adjustable in WALLPAP) */
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                          ImVec4(0.09f, 0.11f, 0.16f, g_a_win));
    /* NATIVE title bar + native docking, untouched. p_open gives the X. */
    bool p_keep = a.open && !a.closing;
    bool shown = ImGui::Begin(title, &p_keep);
    if (!p_keep && !a.closing) { /* X pressed: fade out, don't vanish */
        a.open = true; a.closing = true; a.close_t0 = now_double();
    }
    if (pushed_alpha) ImGui::PopStyleVar();
    if (!shown) { ImGui::End(); ImGui::PopStyleColor(); if (!a.closing) shell_close(i); return false; }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) g_focus = i;
    /* minimize button inside the NATIVE title bar, left of the X */
    {
        ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
        float th = ImGui::GetFrameHeight();
        ImVec2 keep = ImGui::GetCursorPos();
        ImGui::SetCursorScreenPos(ImVec2(wp.x + ws.x - th * 2 - 6, wp.y + 2));
        ImGui::PushStyleColor(ImGuiCol_Button, vec4f(0.25f, 0.35f, 0.48f, 0.9f));
        if (ImGui::Button("_##min", ImVec2(th - 4, th - 4)))
            shell_minimize(i);
        ImGui::PopStyleColor();
        ImGui::SetCursorPos(keep);
    }
    return true;
}
static void shell_end_app(void) { ImGui::End(); ImGui::PopStyleColor(); }

/* ---- procedural vector icons (ImDrawList, no textures) ---- */
/* one 56x56 icon tile centered in rect: accent color + glyph per app */
static void shell_draw_icon(int i, ImVec2 c, float s, ImU32 accent) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImU32 line = IM_COL32(235, 240, 245, 255);
    switch (i) {
    case -1: { /* plugin (dyn) app: cube */
        ImVec2 t = c, r = ImVec2(c.x + s, c.y + s * 0.30f), b2 = ImVec2(c.x, c.y + s * 0.60f);
        ImVec2 l = ImVec2(c.x - s, c.y + s * 0.30f), bb = ImVec2(c.x, c.y + s * 1.05f);
        dl->AddTriangleFilled(l, b2, bb, IM_COL32(30, 80, 115, 255));
        dl->AddTriangleFilled(b2, r, bb, IM_COL32(45, 115, 165, 255));
        dl->AddTriangleFilled(t, r, b2, accent);
        dl->AddTriangleFilled(t, b2, l, IM_COL32(70, 150, 200, 255));
        break;
    }
    case 1: { /* WALLPAP: layered translucent squares */
        for (int k = 0; k < 3; k++) {
            float o = s * (0.45f - k * 0.18f);
            ImU32 cc = IM_COL32(60 + k * 50, 80 + k * 60, 120 + k * 60, 200 - k * 40);
            dl->AddRectFilled(ImVec2(c.x - o, c.y - o), ImVec2(c.x + o, c.y + o),
                              cc, 4.0f + k * 3.0f);
        }
        break;
    }
    case 2: { /* APPMGR: 2x2 app grid */
        for (int r = 0; r < 2; r++)
            for (int k = 0; k < 2; k++)
                dl->AddRectFilled(ImVec2(c.x - s * 0.7f + k * s * 0.8f, c.y - s * 0.7f + r * s * 0.8f),
                                  ImVec2(c.x - s * 0.7f + k * s * 0.8f + s * 0.55f,
                                         c.y - s * 0.7f + r * s * 0.8f + s * 0.55f),
                                  (r + k) % 2 ? IM_COL32(70, 160, 140, 255) : IM_COL32(90, 130, 180, 255), 4.0f);
        break;
    }
    default: { /* SYSINFO: chip with pins */
        dl->AddRectFilled(ImVec2(c.x - s * 0.55f, c.y - s * 0.55f),
                          ImVec2(c.x + s * 0.55f, c.y + s * 0.55f), accent, 4.0f);
        for (int k = 0; k < 4; k++) {
            float o = -s * 0.3f + k * s * 0.2f;
            dl->AddLine(ImVec2(c.x + o, c.y - s * 0.55f), ImVec2(c.x + o, c.y - s * 0.8f), line, 2.0f);
            dl->AddLine(ImVec2(c.x + o, c.y + s * 0.55f), ImVec2(c.x + o, c.y + s * 0.8f), line, 2.0f);
            dl->AddLine(ImVec2(c.x - s * 0.55f, c.y + o), ImVec2(c.x - s * 0.8f, c.y + o), line, 2.0f);
            dl->AddLine(ImVec2(c.x + s * 0.55f, c.y + o), ImVec2(c.x + s * 0.8f, c.y + o), line, 2.0f);
        }
        break;
    }
    }
}
static ImU32 g_app_accent[9] = {
    IM_COL32(70, 170, 220, 255), IM_COL32(230, 170, 60, 255),
    IM_COL32(90, 220, 140, 255), IM_COL32(220, 120, 220, 255),
    IM_COL32(200, 200, 210, 255), IM_COL32(240, 130, 110, 255),
    IM_COL32(120, 190, 240, 255), IM_COL32(170, 140, 230, 255),
    IM_COL32(120, 220, 190, 255),
};

/* clock string HH:MM */
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

void igui_build_ui(double t, float fps) {
    (void)t;
    for (int i = 0; i < 64; i++)
        g_plot[i] = 0.5f + 0.4f * sinf((float)t * 3 + i * 0.4f) * sinf((float)t + i * 0.11f);
    /* visual self-test: touch /home/user/gopen -> boot with 3 apps open */
    static bool s_selftest;
    if (!s_selftest) {
        s_selftest = true;
        if (access("/home/user/gopen", 0) == 0) {
            shell_focus(0);   /* bounds: NAPPS==2 now */
        }
    }
    /* dockspace host: fills the area ABOVE the 48px system bar so docked/
       maximized windows never cover it (toggle in SYSTEM settings) */
    {
        float dh = g_dock_covers_sysbar ? 480.0f : 432.0f;
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
                     ImGuiWindowFlags_NoBackground); /* else it blankets the desktop */
        ImGui::DockSpace(ImGui::GetID("DockSpace"), ImVec2(0, 0),
                         ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::End();
        ImGui::PopStyleVar(3);
    }
    shell_read_sysfs();
    if (++g_dyn_scan_cnt >= 120) { g_dyn_scan_cnt = 0; dyn_scan(); }

    /* ---- desktop: translucent grid launcher (3D cube background shows
       through - the cubes ARE the wallpaper) ---- */
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(800, 432));
    /* transparency spec: desktop fully transparent (wallpaper visible),
       only the frosted app cards themselves occlude */
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                          ImVec4(0.03f, 0.05f, 0.09f, g_a_desk));
    ImGui::Begin("###desktop", NULL,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | /* scrollbar allowed: vertical
                    scrolling desktop when apps exceed one screen */
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoNav); /* desktop nav handled manually (grid) */
    /* desktop keyboard filter: type letters while on desktop to find apps */
    if (g_desk_filter[0]) {
        ImDrawList *dl = ImGui::GetWindowDrawList();
        float fw = 240;
        dl->AddRectFilled(ImVec2(280, 6), ImVec2(280 + fw, 44),
                          IM_COL32(20, 26, 38, 235), 8.0f);
        dl->AddRect(ImVec2(280, 6), ImVec2(280 + fw, 44),
                    IM_COL32(90, 200, 240, 200), 8.0f, 0, 2.0f);
        char fl[72];
        snprintf(fl, sizeof fl, "> %s", g_desk_filter);
        dl->AddText(ImVec2(294, 6 + (38 - ImGui::GetFontSize()) * 0.5f),
                    IM_COL32(235, 240, 245, 255), fl);
        dl->AddText(ImVec2(280 + fw - 88, 6 + (38 - ImGui::GetFontSize()) * 0.5f),
                    IM_COL32(150, 160, 175, 200), "ENTER=open");
    }
    {
        ImDrawList *dl = ImGui::GetWindowDrawList();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, vec4f(1, 1, 1, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, vec4f(1, 1, 1, 0.16f));
        const int cols = 4, cellw = 190, cellh = 118;
        const int total = NAPPS + g_dyn_n;
        for (int i = 0; i < total; i++) {
            bool dyn = i >= NAPPS;
            int di = dyn ? i - NAPPS : -1;
            const char *nm = dyn ? g_dyn[di].name : g_apps[i].name;
            bool on = dyn ? (g_dyn[di].open && !g_dyn[di].mini)
                          : (g_apps[i].open && !g_apps[i].mini);
            int col = i % cols, row = i / cols;
            float bx = 14 + col * cellw, by = 12 + row * cellh;
            ImGui::SetCursorPos(ImVec2(bx, by));
            char hid[16];
            snprintf(hid, sizeof hid, "##desk%d", i);
            static double hold_t0[16] = {0};
            if (i == igui_desk_nav_index() && !g_desk_edit) { /* manual kb nav */
                dl->AddRect(ImVec2(bx - 3, by - 3), ImVec2(bx + cellw - 11, by + cellh - 9),
                            IM_COL32(120, 220, 255, 255), 12.0f, 0, 3.5f);
            }
            if (ImGui::InvisibleButton(hid, ImVec2(cellw - 14, cellh - 12))) {
                if (g_desk_edit) {
                    /* tap near the red X badge = uninstall NOW, no confirm */
                    ImVec2 mp = ImGui::GetIO().MousePos;
                    float cx = bx + cellw - 26, cy = by + 6;
                    if (dyn && (mp.x - cx) * (mp.x - cx) + (mp.y - cy) * (mp.y - cy) < 16 * 16) {
                        g_dyn_dead = di;
                        vibra_ping(120);
                    }
                } else if (dyn) {
                    DynApp &a = g_dyn[di];
                    if (!a.ever) { a.cascade = true; a.ever = true; }
                    a.open = true; a.mini = false; a.anim = 0; a.anim_t0 = -1;
                    a.anim = 0; a.anim_t0 = now_double();
                } else shell_focus(i);
            }
            /* long-press 0.5s -> edit (uninstall) mode */
            int hi = i < 16 ? i : 15;
            if (!g_desk_edit && ImGui::IsItemActive()) {
                if (hold_t0[hi] == 0) hold_t0[hi] = t;
                else if (t - hold_t0[hi] > 0.5) {
                    g_desk_edit = true;
                    vibra_ping(60);
                }
            } else hold_t0[hi] = 0;
            bool hot = ImGui::IsItemHovered();
            bool fmatch = true;
            if (g_desk_filter[0] && nm) {
                char ub[40], uf[40];
                snprintf(ub, sizeof ub, "%s", nm);
                for (char *q = ub; *q; q++) *q = (*q >= 'a' && *q <= 'z') ? *q - 32 : *q;
                snprintf(uf, sizeof uf, "%s", g_desk_filter);
                for (char *q = uf; *q; q++) *q = (*q >= 'a' && *q <= 'z') ? *q - 32 : *q;
                fmatch = strstr(ub, uf) != NULL;
            }
            /* tile: frosted card behind the icon */
            ImVec2 p0 = ImVec2(bx, by), p1 = ImVec2(bx + cellw - 14, by + cellh - 12);
            if (g_desk_filter[0] && !fmatch) {
                dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 60), 10.0f);
                dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 18), 10.0f, 0, 1.0f);
            } else {
                dl->AddRectFilled(p0, p1,
                                  IM_COL32(on ? 40 : 255, on ? 90 : 255, on ? 130 : 255,
                                           hot ? 66 : 38),
                                  10.0f);
                dl->AddRect(p0, p1,
                            (on || (g_desk_filter[0] && fmatch)) ? IM_COL32(90, 200, 240, 240)
                                                                 : IM_COL32(255, 255, 255, 45),
                            10.0f, 0, hot ? 2.5f : 1.5f);
            }
            if (dyn && g_dyn[di].icon)
                g_dyn[di].icon(ImGui::GetWindowDrawList(),
                               bx + (cellw - 14) * 0.5f, by + 34, 17.0f);
            else
                shell_draw_icon(dyn ? -1 : i, ImVec2(bx + (cellw - 14) * 0.5f, by + 34), 17.0f,
                                dyn ? IM_COL32(160, 120, 220, 255) : g_app_accent[i]);
            char label[40];
            snprintf(label, sizeof label, "%s%s", nm, on ? " *" : "");
            ImVec2 ts = ImGui::CalcTextSize(label);
            dl->AddText(ImVec2(bx + ((cellw - 14) - ts.x) * 0.5f, by + 76),
                        IM_COL32(235, 240, 245, 235), label);
        }
        ImGui::PopStyleColor(3);
    }
    if (g_desk_edit) {
        /* edit mode: red X-in-circle badges on uninstallable (dyn) tiles,
           lock glyph on builtin; DONE button exits */
        ImDrawList *dl = ImGui::GetWindowDrawList();
        for (int i = 0; i < NAPPS + g_dyn_n; i++) {
            bool dyn = i >= NAPPS;
            if (!dyn) continue;
            int di = i - NAPPS;
            int col = i % 4, row = i / 4;
            float bx = 14 + col * 190, by = 12 + row * 118;
            float cx = bx + 190 - 26, cy = by + 6;
            dl->AddCircleFilled(ImVec2(cx, cy), 11, IM_COL32(220, 50, 50, 255));
            dl->AddLine(ImVec2(cx - 5, cy - 5), ImVec2(cx + 5, cy + 5),
                        IM_COL32(255, 255, 255, 255), 2.5f);
            dl->AddLine(ImVec2(cx - 5, cy + 5), ImVec2(cx + 5, cy - 5),
                        IM_COL32(255, 255, 255, 255), 2.5f);
        }
        ImGui::SetCursorPos(ImVec2(700, 398));
        if (ImGui::Button("DONE", ImVec2(86, 30))) g_desk_edit = false;
        ImGui::SetCursorPos(ImVec2(10, 404));
        ImGui::TextColored(vec4f(0.9f, 0.5f, 0.4f, 0.9f), "EDIT MODE: tap red X to uninstall");
    } else {
        ImGui::SetCursorPos(ImVec2(10, 404));
        ImGui::TextColored(vec4f(0.55f, 0.62f, 0.72f, 0.85f), "IMRUN v" IMRUN_VER " - imgui %s  (type to filter, long-press to edit)",
                           ImGui::GetVersion());
    }
    /* process pending uninstall at safe point (never mid-frame) */
    if (g_dyn_dead >= 0) {
        int di = g_dyn_dead; g_dyn_dead = -1;
        DynApp &a = g_dyn[di];
        a.open = false;
        if (a.destroy) a.destroy();
        dlclose(a.h);
        char cmd[128];
        snprintf(cmd, sizeof cmd, "rm -rf /home/user/imrun/apps/%s", a.id);
        system(cmd);
        for (int k = di; k < g_dyn_n - 1; k++) g_dyn[k] = g_dyn[k + 1];
        g_dyn_n--;
        if (g_focus >= NAPPS + di) g_focus = -1;
    }
    ImGui::End();
    ImGui::PopStyleColor();

    /* ---- bottom system bar 48px: BACK HOME [running x] clock batt temp ---- */
    ImGui::SetNextWindowPos(ImVec2(0, 432));
    ImGui::SetNextWindowSize(ImVec2(800, 48));
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
        /* BACK/HOME: Android-style tap feedback - an elliptical flash that
           lights up on press and fades out over ~0.5s. No persistent state. */
        static float g_back_hl = 0, g_home_hl = 0;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        if (ImGui::InvisibleButton("##back", ImVec2(48, 42))) {
            g_back_hl = 1.0f;
            /* BACK semantics: collapse open popup menus (combo/context) one
               level at a time; only when none left, close the focused app */
            if (ImGui_OpenPopupCount() > 0)
                ImGui_CloseTopPopup();
            else
                igui_close_focused();
        }
        {
            ImVec2 c0 = ImGui::GetItemRectMin();
            ImVec2 c = ImVec2(c0.x + 24, c0.y + 21);
            if (g_back_hl > 0)
                dl->AddEllipseFilled(c, ImVec2(24, 17),
                                     IM_COL32(120, 200, 255, (int)(150 * g_back_hl)), 24);
            dl->AddTriangleFilled(ImVec2(c.x + 6, c.y - 12), ImVec2(c.x + 6, c.y + 12),
                                  ImVec2(c.x - 8, c.y), line);
        }
        ImGui::SameLine();
        if (ImGui::InvisibleButton("##home", ImVec2(48, 42)))
            { g_home_hl = 1.0f; shell_minimize_all(); }
        {
            ImVec2 c0 = ImGui::GetItemRectMin();
            ImVec2 c = ImVec2(c0.x + 24, c0.y + 21);
            if (g_home_hl > 0)
                dl->AddEllipseFilled(c, ImVec2(24, 17),
                                     IM_COL32(120, 200, 255, (int)(150 * g_home_hl)), 24);
            dl->AddCircle(c, 12.0f, line, 40, 3.0f);
        }
        /* fade-out: ~24 frames to full decay at 60fps (~0.4s) */
        g_back_hl *= 0.92f; if (g_back_hl < 0.02f) g_back_hl = 0;
        g_home_hl *= 0.92f; if (g_home_hl < 0.02f) g_home_hl = 0;
        ImGui::SameLine();
        /* running apps: name chip (click = focus/minimize toggle) */
        for (int i = 0; i < NAPPS + g_dyn_n; i++) {
            bool dyn = i >= NAPPS;
            int di = dyn ? i - NAPPS : -1;
            if (dyn ? !g_dyn[di].open : !g_apps[i].open) continue;
            const char *nm = dyn ? g_dyn[di].name : g_apps[i].name;
            bool mini = dyn ? g_dyn[di].mini : g_apps[i].mini;
            bool foc = (g_focus == i) && !mini;
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  foc ? vec4f(0.16f, 0.42f, 0.60f, 1)
                                      : vec4f(0.12f, 0.16f, 0.24f, mini ? 0.5f : 1));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, vec4f(0.16f, 0.60f, 0.32f, 1)); /* held = green */
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, vec4f(0.20f, 0.30f, 0.42f, 1));
            ImGui::PushStyleColor(ImGuiCol_Border, vec4f(0.45f, 0.55f, 0.68f, mini ? 0.35f : 0.8f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f); /* never a naked text chip */
            char tag[48];
            snprintf(tag, sizeof tag, "%s##run%d", nm, i);
            /* chip_t0 states: 0=idle, >0=pressing(t0), -1=long-press fired */
            static double chip_t0[24] = {0};
            int ci = i < 24 ? i : 23;
            if (ImGui::Button(tag, ImVec2(0, 42))) {
                if (chip_t0[ci] != -1) { /* quick tap only (long handled below) */
                    if (dyn) {
                        DynApp &a = g_dyn[di];
                        if (mini || !foc) { if(!a.ever){a.cascade=true;a.ever=true;} a.open=true; a.mini=false; a.anim=0; a.anim_t0=now_double(); }
                        else a.mini = true;
                    } else {
                        if (mini || !foc) shell_focus(i);
                        else shell_minimize(i);
                    }
                }
                chip_t0[ci] = 0;
            }
            if (ImGui::IsItemActive()) {
                if (chip_t0[ci] == 0) chip_t0[ci] = ImGui::GetTime();
                else if (chip_t0[ci] > 0 && ImGui::GetTime() - chip_t0[ci] > 0.5f) {
                    chip_t0[ci] = -1;   /* stays until release consumes it */
                    vibra_ping(80);
                    if (dyn && g_dyn[di].open && !g_dyn[di].closing) {
                        g_dyn[di].closing = true; g_dyn[di].close_t0 = now_double();
                    } else if (!dyn) shell_close(i);
                }
            } else if (chip_t0[ci] > 0) {
                chip_t0[ci] = 0;
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
            ImGui::SameLine();
        }
        /* right side: [thermo C] [battery %] [clock] - measured, evenly
           spaced groups, right-aligned (runtime text metrics, no guessing) */
        float wy = ImGui::GetWindowPos().y;   /* = 432 */
        float ytxt = wy + 21 - ImGui::GetFontSize() * 0.5f;
        char bat[8] = "--", tmp[8] = "--", clk[8] = "--:--";
        if (g_batt_pct >= 0) snprintf(bat, sizeof bat, "%d", g_batt_pct);
        if (g_temp >= 0) snprintf(tmp, sizeof tmp, "%d", g_temp);
        snprintf(clk, sizeof clk, "%s", shell_clock());
        ImFont *f = ImGui::GetFont();
        float fs = ImGui::GetFontSize();
        ImVec2 tc = CalcTextSizeLocal(f, fs, clk);
        ImVec2 tb = CalcTextSizeLocal(f, fs, bat);
        strcat(bat, "%"); tb = CalcTextSizeLocal(f, fs, bat);
        ImVec2 tt = CalcTextSizeLocal(f, fs, tmp);
        ImVec2 tC = CalcTextSizeLocal(f, fs, "C");
        const float GAP = 20.0f, ICON_TXT = 4.0f;
        /* clock: [watch] text        (right edge 792) */
        float clk_tx = 792 - tc.x;
        float clk_ix = clk_tx - ICON_TXT - 18;
        /* battery: [gauge] text      (gap 20 from clock icon) */
        float bat_tx = clk_ix - GAP - tb.x;
        float bat_ix = bat_tx - ICON_TXT - 34;
        /* temp: [thermo] num C       (gap 20 from battery icon) */
        float tmp_Cx = bat_ix - GAP - tC.x;
        float tmp_nx = tmp_Cx - 2 - tt.x;
        float tmp_ix = tmp_nx - ICON_TXT - 8;
        float yc = wy + 21;
        /* temperature */
        dl->AddCircleFilled(ImVec2(tmp_ix + 4, yc + 9), 4.0f, IM_COL32(235, 90, 80, 255));
        dl->AddLine(ImVec2(tmp_ix + 4, yc + 9), ImVec2(tmp_ix + 4, yc - 8),
                    IM_COL32(235, 90, 80, 255), 4.0f);
        dl->AddText(ImVec2(tmp_nx, ytxt), line, tmp);
        dl->AddText(ImVec2(tmp_Cx, ytxt), IM_COL32(180, 190, 200, 230), "C");
        /* battery gauge */
        dl->AddRect(ImVec2(bat_ix, yc - 7), ImVec2(bat_ix + 30, yc + 9), line, 3.0f, 0, 2.0f);
        dl->AddRectFilled(ImVec2(bat_ix + 30, yc - 3), ImVec2(bat_ix + 34, yc + 5), line, 1.0f);
        int pct = g_batt_pct < 0 ? 0 : g_batt_pct;
        ImU32 col = pct > 40 ? IM_COL32(110, 220, 130, 255)
                  : pct > 15 ? IM_COL32(235, 190, 80, 255)
                             : IM_COL32(235, 90, 80, 255);
        if (g_batt_pct >= 0)
            dl->AddRectFilled(ImVec2(bat_ix + 3, yc - 4),
                              ImVec2(bat_ix + 3 + 24 * pct / 100, yc + 6), col, 2.0f);
        dl->AddText(ImVec2(bat_tx, ytxt), line, bat);
        /* clock */
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

    /* ================= apps (each opened on demand) ================= */

    /* APP: SYSTEM - merged settings (LOOKS) + info (INFO) + about (ABOUT) */
    if (shell_begin_app(0, ImVec2(280, 60), ImVec2(460, 360))) {
        if (ImGui::BeginTabBar("systabs")) {
        if (ImGui::BeginTabItem("LOOKS")) {
            const char *modes[4] = {"3D DYNAMIC", "3D STATIC", "2D DYNAMIC", "2D STATIC"};
            int m = g_wall_mode;
            if (ImGui::Combo("MODE", &m, modes, 4)) g_wall_mode = m;
            const char *pals[4] = {"AZURE", "VIOLET", "EMERALD", "AMBER"};
            int v = g_wall_variant;
            if (ImGui::Combo("PALETTE", &v, pals, 4)) g_wall_variant = v;
            ImGui::Separator();
            ImGui::TextUnformatted("TRANSPARENCY");
            ImGui::SliderFloat("WINDOW", &g_a_win, 0.4f, 1.0f, "%.2f");
            ImGui::SliderFloat("TASKBAR", &g_a_bar, 0.3f, 1.0f, "%.2f");
            ImGui::SliderFloat("DESK TINT", &g_a_desk, 0.0f, 0.8f, "%.2f");
            ImGui::Separator();
            ImGui::Checkbox("DOCK MAY COVER SYSTEM BAR", &g_dock_covers_sysbar);
            ImGui::TextUnformatted("(off: docked/maximized windows stop above the bar)");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("INFO")) {
            for (int i = 0; i < hw_n; i++) ImGui::TextUnformatted(hw[i]);
            ImGui::Separator();
            ImGui::Text("Nokia N900 - 2009 - Cortex A8 + SGX530");
            ImGui::Text("imgui %s", ImGui::GetVersion());
            if (ImGui::BeginTable("hw", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("COMPONENT");
                ImGui::TableSetupColumn("VALUE");
                ImGui::TableHeadersRow();
                const char *rows[6][2] = {
                    {"CPU", "Cortex-A8 600MHz"}, {"GPU", "PowerVR SGX530"},
                    {"RAM", "256MB LPDDR"}, {"SCREEN", "800x480 AMOLED"},
                    {"GLES", "1.1 fixed-func"}, {"COMPOSITOR", "DISABLED"}};
                for (int i = 0; i < 6; i++) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(rows[i][0]);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(rows[i][1]);
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ABOUT")) {
            ImGui::Text("IMRUN v" IMRUN_VER);
            ImGui::TextUnformatted("hot-loadable ImGui runtime - bare X11+EGL+GLES1.1");
            ImGui::Separator();
            ImGui::TextUnformatted("CHANGELOG");
            ImGui::BulletText("v0.3 system apps merged (SYSTEM+APPMGR), dock respects "
                              "sysbar, scrolling desktop, keyboard app filter, version sync");
            ImGui::BulletText("v0.28 BACK=popup collapse then close; taskbar long-press kill; "
                              "auto-uncollapse on open; HOME minimizes all");
            ImGui::BulletText("v0.27 touch drag scrolling (core patch 3); plugin icon standard");
            ImGui::BulletText("v0.26 full modularization - only system apps builtin");
            ImGui::BulletText("v0.24/25 true hot reload, APPMGR, long-press uninstall");
            ImGui::BulletText("v0.9-v0.23 audio saga, shell, native minimize (core patches 1-2)");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
        }
        shell_end_app();
    }

    /* APP: APPMGR - install (filtered list) / manage / stats */
    if (shell_begin_app(1, ImVec2(60, 40), ImVec2(440, 340))) {
        static char filter[40] = "";
        if (ImGui::BeginTabBar("mgr")) {
        if (ImGui::BeginTabItem("INSTALL")) {
            ImGui::TextUnformatted("source: /home/user/MyDocs (drop .so or .imapp via USB)");
            static double scan_t = -99;
            if (ImGui::Button("RESCAN", ImVec2(-1, 40)) || ImGui::GetTime() - scan_t > 30) {
                scan_t = ImGui::GetTime();
            }
            ImGui::InputTextWithHint("##flt", "filter...", filter, sizeof filter);
            /* enumerate installables */
            DIR *d = opendir("/home/user/MyDocs");
            if (d) {
                struct dirent *e;
                while ((e = readdir(d))) {
                    const char *ext = strrchr(e->d_name, '.');
                    if (!ext || (strcmp(ext, ".so") && strcmp(ext, ".imapp"))) continue;
                    if (filter[0] && !strstr(e->d_name, filter)) continue;
                    char path[160];
                    snprintf(path, sizeof path, "/home/user/MyDocs/%s", e->d_name);
                    ImGui::BulletText("%s", e->d_name);
                    ImGui::SameLine(300);
                    char bid[80];
                    snprintf(bid, sizeof bid, "INSTALL##%s", e->d_name);
                    if (ImGui::Button(bid, ImVec2(110, 32))) {
                        char cmd[256], idname[40];
                        snprintf(idname, sizeof idname, "%.*s", (int)(ext - e->d_name), e->d_name);
                        if (!strcmp(ext, ".imapp"))
                            snprintf(cmd, sizeof cmd,
                                     "mkdir -p /home/user/imrun/apps && cd /home/user/imrun/apps && rm -rf %s && tar xzf %s",
                                     idname, path);
                        else
                            snprintf(cmd, sizeof cmd,
                                     "mkdir -p /home/user/imrun/apps/%s && cp %s /home/user/imrun/apps/%s/app.so",
                                     idname, path, idname);
                        system(cmd); /* picked up by the 2s hot scan */
                    }
                }
                closedir(d);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("MANAGE")) {
            ImGui::Text("builtin: %d", NAPPS);
            ImGui::Separator();
            for (int k = 0; k < g_dyn_n; k++) {
                DynApp &a = g_dyn[k];
                ImGui::Text("%s  [%s]", a.name, a.open ? (a.mini ? "background" : "running") : "stopped");
                ImGui::SameLine(280);
                char uid[48], kid[48];
                snprintf(uid, sizeof uid, "UNINSTALL##m%d", k);
                snprintf(kid, sizeof kid, "KILL##m%d", k);
                if (a.open && ImGui::Button(kid, ImVec2(60, 30))) a.open = false;
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, vec4f(0.55f, 0.15f, 0.15f, 1));
                if (ImGui::Button(uid, ImVec2(110, 30))) g_dyn_dead = k;
                ImGui::PopStyleColor();
            }
            ImGui::Separator();
            if (ImGui::Button("RESCAN NOW", ImVec2(-1, 40))) dyn_scan();
            ImGui::TextUnformatted("long-press a desktop icon for quick uninstall");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
        }
        shell_end_app();
    }

    /* ---- dynamic hot-loaded app windows (app.so from apps dir) ---- */
    for (int di = 0; di < g_dyn_n; di++) {
        DynApp &a = g_dyn[di];
        if ((!a.open && !a.closing) || a.mini) continue;
        bool pushed_alpha = false;
        if (a.closing) {
            float t = (float)(now_double() - a.close_t0) / 0.5f;
            if (t >= 1.0f) {
                a.open = false; a.closing = false; a.mini = false;
                if (a.destroy) a.destroy();
                continue;
            }
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f - (t * t * (3 - 2 * t)));
            pushed_alpha = true;
        } else if (a.cascade) {
            a.cascade = false;
            int n = g_cascade_seq++ % 6;
            ImVec2 cp = ImVec2(24 + n * 42, 20 + n * 34);
            if (cp.x + 420 > 790) cp.x = 790 - 420;
            if (cp.y + 300 > 424) cp.y = 424 - 300;
            ImGui::SetNextWindowPos(cp, ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(420, 300), ImGuiCond_Always);
        }
        if (!a.closing && a.anim < 1.0f) {
            float tt = (float)(now_double() - a.anim_t0) / 0.5f;
            if (tt >= 1.0f) a.anim = 1.0f;
            else {
                a.anim = tt;
                float e = tt * tt * (3 - 2 * tt);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, e);
                pushed_alpha = true;
            }
        }
        if (g_win_move && g_focus == NAPPS + di && (g_nudge_dx || g_nudge_dy)) {
            extern bool ImGui_GetWindowPosByName(const char *, float *, float *);
            float wx = 0, wy = 0;
            char tn[80];
            snprintf(tn, sizeof tn, "%s###dyn%d", a.name, di);
            if (ImGui_GetWindowPosByName(tn, &wx, &wy)) {
                ImGui::SetNextWindowPos(ImVec2(wx + g_nudge_dx, wy + g_nudge_dy),
                                        ImGuiCond_Always);
                g_nudge_dx = g_nudge_dy = 0;
            }
        }
        char title[80];
        snprintf(title, sizeof title, "%s###dyn%d", a.name, di);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.09f, 0.11f, 0.16f, g_a_win));
        bool shown = ImGui::Begin(title, &a.open);
        if (pushed_alpha) ImGui::PopStyleVar();
        if (shown) {
            a.frame(t);
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) g_focus = NAPPS + di;
        }
        ImGui::End();
        ImGui::PopStyleColor();
        if (!a.open && !a.closing) { /* title-bar X: fade, then destroy */
            a.open = true; a.closing = true; a.close_t0 = now_double();
        }
    }
}

} // extern "C"
