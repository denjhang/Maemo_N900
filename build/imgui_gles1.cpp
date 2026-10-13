// imgui_gles1.cpp - Dear ImGui 1.92.8 rendering backend for OpenGL ES 1.1
// (fixed-function client arrays) + demo UI content for the N900 showcase.
// All GL entry points are handed in from the C side (runtime dlopen'd).
#include "imgui.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
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

int igui_init(int w, int h, IGlfns *fns) {
    gl = *fns;
    scrW = w; scrH = h;
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures; /* dynamic font sizes */
    io.MouseDragThreshold = 3.0f; /* touch: 6px default eats finger gestures */
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = "/home/user/imrun.ini"; /* window pos/size persist forever */
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

/* ============ IMRUN shell state (declared early: key path uses it) ======= */
struct ShellApp { const char *name; bool open; bool mini; bool cascade; bool ever;
                  float anim; double anim_t0;   /* open/restore transition */ };
static ShellApp g_apps[] = {
    {"IMGUI", false, false, false, false, 1, 0}, {"WIDGETS", false, false, false, false, 1, 0},
    {"TONEGEN", false, false, false, false, 1, 0}, {"VIBRA", false, false, false, false, 1, 0},
    {"KEYS", false, false, false, false, 1, 0}, {"FONTS", false, false, false, false, 1, 0},
    {"SYSINFO", false, false, false, false, 1, 0},
};
static const int NAPPS = (int)(sizeof g_apps / sizeof g_apps[0]);
static int g_focus = -1;          /* focused window, -1 = desktop only */
bool g_ctrl_down = false;         /* fed by igui_special_key / X keycode 37 */
static void shell_close(int i);       /* defined below */
static void shell_minimize(int i);
void igui_ctrl_combo(char c);         /* from X11 pump: ctrl+letter combos */

void igui_key_char(const char *utf8) {
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
static void shell_close(int i) {
    g_apps[i].open = false;
    g_apps[i].mini = false;
    if (g_focus == i) g_focus = -1;
}
static void shell_minimize(int i) {
    g_apps[i].mini = true;
    if (g_focus == i) g_focus = -1;
}
static void shell_minimize_all(void) {
    for (int i = 0; i < NAPPS; i++) g_apps[i].mini = true;
    g_focus = -1;
}
static int g_cascade_seq = 0;         /* new windows stack from top-left */
static void shell_focus(int i) {
    ShellApp &a = g_apps[i];
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
void igui_ctrl_combo(char c) {
    if (c == 'w' || c == 'W') { if (g_focus >= 0) shell_close(g_focus); }
    else if (c == 'm' || c == 'M') { if (g_focus >= 0) shell_minimize(g_focus); }
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
static bool shell_begin_app(int i, ImVec2 pos, ImVec2 size) {
    ShellApp &a = g_apps[i];
    if (!a.open || a.mini) return false;
    bool pushed_alpha = false;
    if (a.cascade) {
        /* fresh open: cascade from top-left, never tiny (Windows style) */
        int n = g_cascade_seq++ % 6;
        a.cascade = false;
        ImVec2 cp = ImVec2(24 + n * 42, 20 + n * 34);
        ImVec2 cs = ImVec2(size.x < 400 ? 400 : size.x, size.y < 280 ? 280 : size.y);
        if (cp.x + cs.x > 790) cp.x = 790 - cs.x;
        if (cp.y + cs.y > 424) cp.y = 424 - cs.y;
        ImGui::SetNextWindowPos(cp, ImGuiCond_Always);
        ImGui::SetNextWindowSize(cs, ImGuiCond_Always);
    } else if (a.anim < 1.0f) {
        /* reopen/restore: ALPHA FADE ONLY. Never force pos/size here -
           SetNextWindowPos during the animation stomps the layout the user
           saved in imrun.ini (that was the bottom-left-reappear bug) */
        float t = (float)(now_double() - a.anim_t0) / 0.22f;
        if (t >= 1.0f) {
            a.anim = 1.0f;
        } else {
            a.anim = t;
            float e = t * t * (3 - 2 * t); /* smoothstep */
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.2f + 0.8f * e);
            pushed_alpha = true;
        }
    }
    if (!pushed_alpha && !a.cascade) {
        ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
    }
    char title[64];
    snprintf(title, sizeof title, "%s###app%d", a.name, i);
    bool shown = ImGui::Begin(title, &a.open);
    if (pushed_alpha) ImGui::PopStyleVar();
    if (!shown) { ImGui::End(); shell_close(i); return false; }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) g_focus = i;
    /* minimize button in the TITLE BAR, left of the X (Windows logic) */
    {
        ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
        float th = ImGui::GetFrameHeight();
        ImVec2 keep = ImGui::GetCursorPos();
        ImGui::SetCursorScreenPos(ImVec2(wp.x + ws.x - th * 2 - 4, wp.y + 2));
        ImGui::PushStyleColor(ImGuiCol_Button, vec4f(0.25f, 0.35f, 0.48f, 0.9f));
        if (ImGui::Button("_###min", ImVec2(th - 4, th - 4)))
            shell_minimize(i);
        ImGui::PopStyleColor();
        ImGui::SetCursorPos(keep);
    }
    return true;
}
static void shell_end_app(void) { ImGui::End(); }

/* window footer: touch-sized CLOSE (minimize lives in the title bar) */
static void shell_window_buttons(int i) {
    ImGui::PushStyleColor(ImGuiCol_Button, vec4f(0.55f, 0.15f, 0.15f, 1));
    if (ImGui::Button("CLOSE (Ctrl+W)", ImVec2(-1, 44)))
        shell_close(i);
    ImGui::PopStyleColor();
}

/* ---- procedural vector icons (ImDrawList, no textures) ---- */
/* one 56x56 icon tile centered in rect: accent color + glyph per app */
static void shell_draw_icon(int i, ImVec2 c, float s, ImU32 accent) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImU32 line = IM_COL32(235, 240, 245, 255);
    switch (i) {
    case 0: { /* IMGUI: cube */
        ImVec2 t = c, r = ImVec2(c.x + s, c.y + s * 0.30f), b = ImVec2(c.x, c.y + s * 0.60f);
        ImVec2 l = ImVec2(c.x - s, c.y + s * 0.30f), bb = ImVec2(c.x, c.y + s * 1.05f);
        dl->AddTriangleFilled(l, b, bb, IM_COL32(30, 80, 115, 255));
        dl->AddTriangleFilled(b, r, bb, IM_COL32(45, 115, 165, 255));
        dl->AddTriangleFilled(t, r, b, accent);
        dl->AddTriangleFilled(t, b, l, IM_COL32(70, 150, 200, 255));
        break;
    }
    case 1: { /* WIDGETS: three sliders */
        for (int k = 0; k < 3; k++) {
            float y = c.y - s * 0.5f + k * s * 0.5f;
            dl->AddLine(ImVec2(c.x - s, y), ImVec2(c.x + s, y), line, 3.0f);
            float kx = c.x - s * 0.4f + k * s * 0.55f;
            dl->AddCircleFilled(ImVec2(kx, y), s * 0.16f, accent);
        }
        break;
    }
    case 2: { /* TONEGEN: sine wave */
        ImVec2 prev = ImVec2(c.x - s, c.y);
        for (int k = 1; k <= 24; k++) {
            float x = c.x - s + (2.0f * s) * k / 24.0f;
            float y = c.y - sinf(k * 6.2831f * 1.5f / 24.0f) * s * 0.55f;
            dl->AddLine(prev, ImVec2(x, y), accent, 3.5f);
            prev = ImVec2(x, y);
        }
        break;
    }
    case 3: { /* VIBRA: phone + shake arcs */
        dl->AddRectFilled(ImVec2(c.x - s * 0.35f, c.y - s * 0.7f),
                          ImVec2(c.x + s * 0.35f, c.y + s * 0.7f), accent, 6.0f);
        for (int k = 0; k < 2; k++) {
            float rr = s * (0.55f + k * 0.25f);
            dl->PathArcTo(c, rr, 0.75f * 3.14159f, 1.25f * 3.14159f, 20);
            dl->PathStroke(IM_COL32(235, 240, 245, 200), 0, 2.5f);
            dl->PathArcTo(c, rr, 1.75f * 3.14159f, 2.25f * 3.14159f, 20);
            dl->PathStroke(IM_COL32(235, 240, 245, 200), 0, 2.5f);
        }
        break;
    }
    case 4: { /* KEYS: keyboard */
        dl->AddRect(ImVec2(c.x - s, c.y - s * 0.6f), ImVec2(c.x + s, c.y + s * 0.6f),
                    line, 4.0f, 0, 2.5f);
        for (int r = 0; r < 3; r++)
            for (int k = 0; k < 5; k++)
                dl->AddRectFilled(
                    ImVec2(c.x - s * 0.8f + k * s * 0.4f, c.y - s * 0.42f + r * s * 0.34f),
                    ImVec2(c.x - s * 0.8f + k * s * 0.4f + s * 0.28f,
                           c.y - s * 0.42f + r * s * 0.34f + s * 0.2f),
                    (r == 2 && k == 1) ? accent : IM_COL32(200, 210, 220, 160), 2.0f);
        break;
    }
    case 5: { /* FONTS: big A + 文 handled as text by caller; fallback glyph */
        dl->AddCircle(c, s * 0.7f, accent, 40, 3.0f);
        dl->AddLine(ImVec2(c.x - s * 0.35f, c.y), ImVec2(c.x + s * 0.35f, c.y), line, 2.5f);
        dl->AddLine(ImVec2(c.x, c.y - s * 0.35f), ImVec2(c.x, c.y + s * 0.35f), line, 2.5f);
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
static ImU32 g_app_accent[7] = {
    IM_COL32(70, 170, 220, 255), IM_COL32(230, 170, 60, 255),
    IM_COL32(90, 220, 140, 255), IM_COL32(220, 120, 220, 255),
    IM_COL32(200, 200, 210, 255), IM_COL32(240, 130, 110, 255),
    IM_COL32(120, 190, 240, 255),
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
    // full-screen dockspace: drag windows to screen edges to dock
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                 ImGuiDockNodeFlags_PassthruCentralNode);
    shell_read_sysfs();

    /* ---- desktop: translucent grid launcher (3D cube background shows
       through - the cubes ARE the wallpaper) ---- */
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(800, 432));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, vec4f(0.03f, 0.05f, 0.09f, 0.42f));
    ImGui::Begin("###desktop", NULL,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoNav);
    {
        ImDrawList *dl = ImGui::GetWindowDrawList();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, vec4f(1, 1, 1, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, vec4f(1, 1, 1, 0.16f));
        const int cols = 4, cellw = 190, cellh = 118;
        for (int i = 0; i < NAPPS; i++) {
            int col = i % cols, row = i / cols;
            float bx = 14 + col * cellw, by = 12 + row * cellh;
            ImGui::SetCursorPos(ImVec2(bx, by));
            char hid[16];
            snprintf(hid, sizeof hid, "##desk%d", i);
            bool on = g_apps[i].open && !g_apps[i].mini;
            if (ImGui::InvisibleButton(hid, ImVec2(cellw - 14, cellh - 12))) {
                shell_focus(i);
            }
            bool hot = ImGui::IsItemHovered();
            /* tile: frosted card behind the icon */
            ImVec2 p0 = ImVec2(bx, by), p1 = ImVec2(bx + cellw - 14, by + cellh - 12);
            dl->AddRectFilled(p0, p1,
                              IM_COL32(on ? 40 : 255, on ? 90 : 255, on ? 130 : 255,
                                       hot ? 66 : 38),
                              10.0f);
            dl->AddRect(p0, p1,
                        on ? IM_COL32(90, 200, 240, 220) : IM_COL32(255, 255, 255, 45),
                        10.0f, 0, hot ? 2.5f : 1.5f);
            shell_draw_icon(i, ImVec2(bx + (cellw - 14) * 0.5f, by + 34), 17.0f,
                            g_app_accent[i]);
            char label[32];
            snprintf(label, sizeof label, "%s%s", g_apps[i].name,
                     g_apps[i].open ? (g_apps[i].mini ? " -" : " *") : "");
            ImVec2 ts = ImGui::CalcTextSize(label);
            dl->AddText(ImVec2(bx + ((cellw - 14) - ts.x) * 0.5f, by + 76),
                        IM_COL32(235, 240, 245, 235), label);
        }
        ImGui::PopStyleColor(3);
    }
    ImGui::SetCursorPos(ImVec2(10, 404));
    ImGui::TextColored(vec4f(0.55f, 0.62f, 0.72f, 0.85f), "IMRUN shell v0.1 - imgui %s",
                       ImGui::GetVersion());
    ImGui::End();
    ImGui::PopStyleColor();

    /* ---- bottom system bar 48px: BACK HOME [running x] clock batt temp ---- */
    ImGui::SetNextWindowPos(ImVec2(0, 432));
    ImGui::SetNextWindowSize(ImVec2(800, 48));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3, 3));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, vec4f(0.08f, 0.10f, 0.15f, 0.82f));
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
        if (ImGui::InvisibleButton("##back", ImVec2(48, 42))) { /* Ctrl+W */
            g_back_hl = 1.0f;
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
        /* running apps: name chip + red X */
        for (int i = 0; i < NAPPS; i++) {
            if (!g_apps[i].open) continue;
            bool foc = (g_focus == i && !g_apps[i].mini);
            bool mini = g_apps[i].mini;
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  foc ? vec4f(0.16f, 0.42f, 0.60f, 1)
                                      : vec4f(0.12f, 0.16f, 0.24f, mini ? 0.5f : 1));
            char tag[16];
            snprintf(tag, sizeof tag, "%.4s##run%d", g_apps[i].name, i);
            if (ImGui::Button(tag, ImVec2(56, 42))) {
                if (mini || !foc) shell_focus(i);
                else shell_minimize(i);
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, vec4f(0.62f, 0.14f, 0.14f, 1));
            ImGui::PushStyleColor(ImGuiCol_Text, vec4f(1, 1, 1, 1));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            char xt[12];
            snprintf(xt, sizeof xt, "x##kill%d", i);
            if (ImGui::Button(xt, ImVec2(18, 42)))
                shell_close(i);
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
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

    /* APP: IMGUI - runtime settings + render stats */
    if (shell_begin_app(0, ImVec2(20, 20), ImVec2(400, 380))) {
        ImGuiIO &io = ImGui::GetIO();
        ImGuiStyle &st = ImGui::GetStyle();
        static int face = 0;
        static const char *faces[2] = {"MSYH", "BITMAP"};
        static const int face_font[2] = {0, 3};
        static float fsize = 20.0f;
        if (ImGui::Combo("FACE", &face, faces, 2))
            if (g_fonts[face_font[face]]) io.FontDefault = g_fonts[face_font[face]];
        if (ImGui::SliderFloat("FONT SIZE", &fsize, 10.0f, 32.0f, "%.0f px",
                               ImGuiSliderFlags_AlwaysClamp)) {
            /* per official imgui_demo.cpp:8595 — NewFrame resets FontSizeBase
               from _NextFrameFontSizeBase every frame, must set BOTH */
            st.FontSizeBase = (float)(int)(fsize + 0.5f);
            st._NextFrameFontSizeBase = st.FontSizeBase;
        }
        static float uiscale = 1.0f;
        if (ImGui::SliderFloat("UI SCALE", &uiscale, 0.6f, 2.0f, "%.2fx",
                               ImGuiSliderFlags_AlwaysClamp))
            st.FontScaleMain = uiscale;
        ImGui::Separator();
        ImGui::Text("FPS: %.1f  (%.2f ms)", fps, fps > 0 ? 1000.0 / fps : 0);
        ImGui::Text("SGX530 - GLES 1.1 - EGL - NON-COMPOSITED");
        static float v1 = 0.6f, v2 = 0.35f;
        ImGui::SliderFloat("Engine load", &v1, 0, 1);
        ImGui::SliderFloat("Color hue", &v2, 0, 1);
        static float tint[4] = {0.3f, 0.6f, 1.0f, 1.0f};
        tint[0] = v2; tint[2] = 1.0f - v2;
        ImGui::ColorEdit4("Tint", tint, ImGuiColorEditFlags_NoInputs);
        ImGui::ProgressBar(fps / 85.0f, ImVec2(-1, 0), "VSYNC");
        if (ImGui::TreeNode("Render stats")) {
            ImGui::Text("Display: %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
            ImGui::Text("FontScaleMain: %.2f", ImGui::GetStyle().FontScaleMain);
            ImGui::Text("Vertices: %d", ImGui::GetIO().MetricsRenderVertices);
            ImGui::Text("Indices: %d", ImGui::GetIO().MetricsRenderIndices);
            ImGui::Text("Windows: %d", ImGui::GetIO().MetricsRenderWindows);
            ImGui::TreePop();
        }
        float wave[64];
        for (int i = 0; i < 64; i++)
            wave[i] = 0.5f + 0.4f * sinf((float)t * 3 + i * 0.4f) * sinf((float)t + i * 0.11f);
        ImGui::PlotLines("Wave", wave, 64, 0, "SGX load", 0.0f, 1.0f, ImVec2(360, 60));
        shell_window_buttons(0);
        shell_end_app();
    }

    /* APP: WIDGETS - imgui widget lab */
    if (shell_begin_app(1, ImVec2(60, 60), ImVec2(360, 240))) {
        static int clicks = 0;
        if (ImGui::Button("PUSH ME")) clicks++;
        ImGui::SameLine();
        ImGui::Text("count=%d", clicks);
        static bool chk = true;
        ImGui::Checkbox("enable", &chk);
        static int radio = 0;
        ImGui::RadioButton("sd", &radio, 0); ImGui::SameLine();
        ImGui::RadioButton("mmc", &radio, 1); ImGui::SameLine();
        ImGui::RadioButton("usb", &radio, 2);
        static float prog = 0.4f;
        prog = 0.5f + 0.45f * sinf((float)t);
        ImGui::ProgressBar(prog, ImVec2(-1, 0), "COPY");
        static int listitem = 1;
        const char *list[] = {"PPSSPP 0.9.6", "cps2emu 0.6", "drnoksnes 1.3", "gldemo 2026"};
        ImGui::ListBox("APPS", &listitem, list, 4);
        ImGui::PlotLines("LOAD", g_plot, 64, 0, NULL, 0.0f, 1.0f, ImVec2(320, 50));
        shell_window_buttons(1);
        shell_end_app();
    }

    /* APP: TONEGEN - ALSA waveform generator */
    if (shell_begin_app(2, ImVec2(120, 100), ImVec2(320, 220))) {
        extern double audio_deadline;
        if (audio_on) {
            int left = (int)(audio_deadline - now_double());
            ImGui::Text("ALSA PCM: PLAYING 440Hz (%ds left)", left);
        } else
            ImGui::Text("ALSA PCM: idle");
        if (ImGui::Button("PLAY 20s", ImVec2(140, 48)))
            audio_ui_start(20);
        ImGui::SameLine();
        if (ImGui::Button("STOP", ImVec2(140, 48)))
            audio_ui_stop(); /* async: never join/ioctl in UI thread */
        {
            extern int audio_volume;
            extern int audio_wave;
            const char *waves[4] = {"SINE", "TRIANGLE", "SQUARE", "SAW"};
            ImGui::Combo("WAVE", &audio_wave, waves, 4);
            ImGui::SliderInt("VOL", &audio_volume, 0, 1000, "%d%%");
        }
        shell_window_buttons(2);
        shell_end_app();
    }

    /* APP: VIBRA */
    if (shell_begin_app(3, ImVec2(160, 140), ImVec2(320, 180))) {
        static int ms = 1000;
        ImGui::SliderInt("DURATION", &ms, 100, 5000, "%d ms");
        if (ImGui::Button("VIBRATE", ImVec2(140, 48)))
            vibra_ping(ms);
        ImGui::SameLine();
        if (ImGui::Button("STOP ALL", ImVec2(140, 48)))
            vibra_stop();
        shell_window_buttons(3);
        shell_end_app();
    }

    /* APP: KEYS - hardware keyboard test */
    if (shell_begin_app(4, ImVec2(200, 80), ImVec2(360, 200))) {
        ImGui::Text("last: %s", last_key[0] ? last_key : "(press slide-out keyboard)");
        ImGui::TextUnformatted("type into: ");
        ImGui::SameLine();
        static char textbuf[64] = "";
        ImGui::InputText("##kb", textbuf, sizeof textbuf);
        ImGui::Text("CTRL+W closes the focused window");
        shell_window_buttons(4);
        shell_end_app();
    }

    /* APP: FONTS - multilingual */
    if (shell_begin_app(5, ImVec2(240, 40), ImVec2(420, 320))) {
        if (g_cnfont && ImGui::Button("USE CHINESE FONT"))
            ImGui::GetIO().FontDefault = g_cnfont;
        if (g_cnfont) {
            ImGui::PushFont(g_cnfont);
            ImGui::Text("诺基亚 N900 复活了");
            ImGui::PopFont();
        }
        ImGui::Text("FR: Bonjour! Ça marche très bien.");
        ImGui::Text("DE: Grüße — äöü ß.");
        ImGui::Text("ES: ¡Hola! El niño juega.");
        ImGui::Text("RU: Привет, мир!");
        if (g_worldfont) {
            ImGui::PushFont(g_worldfont);
            ImGui::Text("JP: こんにちは世界");
            ImGui::Text("CN: 中文汉字动态烘焙。");
            ImGui::PopFont();
        }
        if (g_krfont) {
            ImGui::PushFont(g_krfont);
            ImGui::Text("KR: 안녕하세요");
            ImGui::PopFont();
        }
        shell_window_buttons(5);
        shell_end_app();
    }

    /* APP: SYSINFO - hardware + version */
    if (shell_begin_app(6, ImVec2(300, 120), ImVec2(380, 260))) {
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
        shell_window_buttons(6);
        shell_end_app();
    }
}

} // extern "C"
