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
    ImGui::StyleColorsDark();
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
struct ShellApp { const char *name; bool open; };
static ShellApp g_apps[] = {
    {"IMGUI", false}, {"WIDGETS", false}, {"TONEGEN", false},
    {"VIBRA", false}, {"KEYS", false}, {"FONTS", false}, {"SYSINFO", false},
};
static const int NAPPS = (int)(sizeof g_apps / sizeof g_apps[0]);
static int g_focus = -1;          /* focused window, -1 = desktop only */
bool g_ctrl_down = false;         /* fed by igui_special_key */

void igui_key_char(const char *utf8) {
    /* Ctrl+W = close focused window (N900 has a real CTRL key; same as PC) */
    if (g_ctrl_down && utf8 && (utf8[0] == 'w' || utf8[0] == 'W') && !utf8[1]) {
        if (g_focus >= 0) g_apps[g_focus].open = false;
        return;
    }
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
    if (g_focus == i) g_focus = -1;
}
static void shell_focus(int i) { g_focus = i; g_apps[i].open = true; }
void igui_close_focused(void) { if (g_focus >= 0) shell_close(g_focus); }

/* sysfs reads are cached: battery + cpu temp, refresh every 90 frames
   (fgets+sscanf: fscanf is __isoc99_fscanf here, no shim for it) */
static int g_batt = -1, g_temp = -1;
static void shell_read_sysfs(void) {
    static int cnt = 0;
    if (++cnt % 90) return;
    char line[64];
    FILE *f = fopen("/sys/class/power_supply/bq27200-0/capacity", "r");
    if (f) { if (fgets(line, sizeof line, f)) g_batt = atoi(line); fclose(f); }
    f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (f) { if (fgets(line, sizeof line, f)) g_temp = atoi(line) / 1000; fclose(f); }
}

/* one app window: Begin with close + touch-sized CLOSE button inside */
static bool shell_begin_app(int i, ImVec2 pos, ImVec2 size) {
    if (!g_apps[i].open) return false;
    ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
    char title[64];
    snprintf(title, sizeof title, "%s###app%d", g_apps[i].name, i);
    bool shown = ImGui::Begin(title, &g_apps[i].open);
    if (!shown) { ImGui::End(); shell_close(i); return false; }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) g_focus = i;
    return true;
}
static void shell_end_app(void) { ImGui::End(); }

/* clock string HH:MM */
static const char *shell_clock(void) {
    static char buf[8];
    time_t tt = time(NULL);
    struct tm *lt = localtime(&tt);
    if (lt) snprintf(buf, sizeof buf, "%02d:%02d", lt->tm_hour, lt->tm_min);
    else snprintf(buf, sizeof buf, "--:--");
    return buf;
}

void igui_build_ui(double t, float fps) {
    (void)t;
    for (int i = 0; i < 64; i++)
        g_plot[i] = 0.5f + 0.4f * sinf((float)t * 3 + i * 0.4f) * sinf((float)t + i * 0.11f);
    // full-screen dockspace: drag windows to screen edges to dock
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                 ImGuiDockNodeFlags_PassthruCentralNode);
    shell_read_sysfs();

    /* ---- desktop: fullscreen grid launcher (behind app windows) ---- */
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(800, 432));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, vec4f(0.05f, 0.07f, 0.11f, 1.0f));
    ImGui::Begin("###desktop", NULL,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoNav);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 26));
    if (ImGui::BeginTable("desk", 4, ImGuiTableFlags_SizingStretchSame)) {
        for (int i = 0; i < NAPPS; i++) {
            ImGui::TableNextColumn();
            bool on = g_apps[i].open;
            if (on) ImGui::PushStyleColor(ImGuiCol_Button, vec4f(0.15f, 0.35f, 0.5f, 1));
            char label[48];
            snprintf(label, sizeof label, "%s%s", g_apps[i].name, on ? " *" : "");
            if (ImGui::Button(label, ImVec2(-1, 72)))
                shell_focus(i);
            if (on) ImGui::PopStyleColor();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::SetCursorPosY(400);
    ImGui::TextColored(vec4f(0.4f, 0.5f, 0.6f, 1), "IMRUN shell v0.1 - Nokia N900 - imgui %s",
                       ImGui::GetVersion());
    ImGui::End();
    ImGui::PopStyleColor();

    /* ---- bottom system bar 48px: BACK HOME [running x] clock batt temp ---- */
    ImGui::SetNextWindowPos(ImVec2(0, 432));
    ImGui::SetNextWindowSize(ImVec2(800, 48));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
    ImGui::Begin("###sysbar", NULL,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);
    if (ImGui::Button("<", ImVec2(48, 42)))   /* BACK = close focused (Ctrl+W) */
        igui_close_focused();
    ImGui::SameLine();
    if (ImGui::Button("O", ImVec2(48, 42)))   /* HOME = desktop */
        g_focus = -1;
    ImGui::SameLine();
    for (int i = 0; i < NAPPS; i++) {
        if (!g_apps[i].open) continue;
        bool foc = (g_focus == i);
        if (foc) ImGui::PushStyleColor(ImGuiCol_Button, vec4f(0.2f, 0.5f, 0.7f, 1));
        char tag[16];
        snprintf(tag, sizeof tag, "%.3s##run%d", g_apps[i].name, i);
        if (ImGui::Button(tag, ImVec2(44, 42)))
            g_focus = i;
        if (foc) ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, vec4f(0.55f, 0.1f, 0.1f, 1));
        ImGui::PushStyleColor(ImGuiCol_Text, vec4f(1, 1, 1, 1));
        char xt[12];
        snprintf(xt, sizeof xt, "x##kill%d", i);
        if (ImGui::Button(xt, ImVec2(16, 42)))
            shell_close(i);
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
    }
    /* right side: clock / battery / temp (read-only, no touch target) */
    float rightx = 800;
    const char *clock = shell_clock();
    ImGui::SetCursorPos(ImVec2(rightx - 190, 11));
    ImGui::Text("%s", clock);
    ImGui::SetCursorPos(ImVec2(rightx - 130, 11));
    if (g_batt >= 0) ImGui::Text("%d%%", g_batt); else ImGui::Text("--%%");
    ImGui::SetCursorPos(ImVec2(rightx - 76, 11));
    if (g_temp >= 0) ImGui::Text("%dC", g_temp); else ImGui::Text("--C");
    ImGui::End();
    ImGui::PopStyleVar();

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
        if (ImGui::Button("CLOSE (Ctrl+W)", ImVec2(-1, 44))) shell_close(0);
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
        if (ImGui::Button("CLOSE (Ctrl+W)", ImVec2(-1, 44))) shell_close(1);
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
        if (ImGui::Button("CLOSE (Ctrl+W)", ImVec2(-1, 44))) shell_close(2);
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
        if (ImGui::Button("CLOSE (Ctrl+W)", ImVec2(-1, 44))) shell_close(3);
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
        if (ImGui::Button("CLOSE (Ctrl+W)", ImVec2(-1, 44))) shell_close(4);
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
        if (ImGui::Button("CLOSE (Ctrl+W)", ImVec2(-1, 44))) shell_close(5);
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
        if (ImGui::Button("CLOSE (Ctrl+W)", ImVec2(-1, 44))) shell_close(6);
        shell_end_app();
    }
}

} // extern "C"
