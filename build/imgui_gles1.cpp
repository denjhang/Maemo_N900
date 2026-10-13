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

void igui_key_char(const char *utf8) {
    ImGui::GetIO().AddInputCharactersUTF8(utf8);
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
extern void vibra_ping(void);
extern int audio_start(int seconds);
double now_double();
}
static inline ImVec4 vec4f(float x, float y, float z, float w) { return ImVec4(x, y, z, w); }
#define ImVec2_to_imvec_helper vec4f
static float g_plot[64];
static float hue = 0.4f;
void igui_build_ui(double t, float fps) {
    (void)t;
    for (int i = 0; i < 64; i++)
        g_plot[i] = 0.5f + 0.4f * sinf((float)t * 3 + i * 0.4f) * sinf((float)t + i * 0.11f);
    // full-screen dockspace: drag windows to screen edges to dock
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                 ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::SetNextWindowPos(ImVec2(20, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 380), ImGuiCond_FirstUseEver);
    ImGui::Begin("N900 GLES1.1 IMGUI");
    {
        /* legacy preload bakes glyphs ONLY at AddFont size — runtime font-size
           changes are a no-op here, so faces are fixed-size by design */
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
    }
    ImGui::Separator();
    ImGui::Text("FPS: %.1f  (%.2f ms)", fps, fps > 0 ? 1000.0 / fps : 0);
    ImGui::Text("SGX530 - GLES 1.1 - EGL - NON-COMPOSITED");
    ImGui::Separator();
    static float v1 = 0.6f, v2 = 0.35f;
    static float tint[4] = {0.3f, 0.6f, 1.0f, 1.0f};
    ImGui::SliderFloat("Engine load", &v1, 0, 1);
    ImGui::SliderFloat("Color hue", &v2, 0, 1);
    tint[0] = v2; tint[2] = 1.0f - v2;
    ImGui::ColorEdit4("Tint", tint, ImGuiColorEditFlags_NoInputs);
    static int item = 1;
    const char *items[] = {"LOW 240P", "MED 480P", "FULL 800x480"};
    ImGui::Combo("Quality", &item, items, 3);
    ImGui::ProgressBar(fps / 85.0f, ImVec2(-1, 0), "VSYNC");
    if (ImGui::TreeNode("Render stats")) {
        ImGuiIO &io = ImGui::GetIO();
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
    ImGui::End();

    // second window bottom-right: system badge
    /* ---- widgets lab ---- */
    ImGui::SetNextWindowPos(ImVec2(20, 436), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 240), ImGuiCond_FirstUseEver);
    ImGui::Begin("WIDGETS LAB");
    if (ImGui::BeginTabBar("labtabs")) {
        if (ImGui::BeginTabItem("BASIC")) {
            static int clicks = 0;
            if (ImGui::Button("PUSH ME"))
                clicks++;
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
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("TABLE")) {
            if (ImGui::BeginTable("hw", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
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
        if (ImGui::BeginTabItem("INPUT")) {
            static int iv = 600;
            static float fv = 1.25f;
            ImGui::InputInt("CLOCK MHZ", &iv);
            ImGui::InputFloat("VOLT", &fv, 0.05f, 0.5f, "%.2f V");
            static int listitem = 1;
            const char *list[] = {"PPSSPP 0.9.6", "cps2emu 0.6", "drnoksnes 1.3", "gldemo 2026"};
            ImGui::ListBox("APPS", &listitem, list, 4);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("TIPS")) {
            ImGui::TextUnformatted("hover me:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec2_to_imvec_helper(0.3f, 0.9f, 1.0f, 1.0f), "TOUCH TARGETS");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Touch = left click.\nDrag title bar to dock.\nDrag borders to resize.");
            }
            static float col[3] = {0.3f, 0.6f, 1.0f};
            ImGui::ColorEdit3("LED", col, ImGuiColorEditFlags_NoInputs);
            ImGui::PlotLines("LOAD", g_plot, 64, 0, NULL, 0.0f, 1.0f, ImVec2(320, 50));
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    /* ---- system: hw / audio / vibra / keys / chinese ---- */
    ImGui::SetNextWindowPos(ImVec2(400, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 260), ImGuiCond_FirstUseEver);
    ImGui::Begin("SYSTEM");
    if (ImGui::BeginTabBar("systabs")) {
        if (ImGui::BeginTabItem("HW")) {
            for (int i = 0; i < hw_n; i++) ImGui::TextUnformatted(hw[i]);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("AUDIO")) {
            extern double audio_deadline;
            if (audio_on) {
                int left = (int)(audio_deadline - now_double());
                ImGui::Text("ALSA PCM: PLAYING 440Hz (%ds left)", left);
            } else
                ImGui::Text("ALSA PCM: idle");
            if (ImGui::Button("PLAY 20s"))
                audio_start(20);
            ImGui::SameLine();
            if (ImGui::Button("STOP"))
                audio_stop_full(); /* join writer thread BEFORE closing PCM */
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("VIBRA")) {
            ImGui::TextUnformatted("MCE vibrator via dbus (system bus)");
            if (ImGui::Button("PING VIBRA"))
                vibra_ping();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("KEYS")) {
            ImGui::Text("last: %s", last_key[0] ? last_key : "(press slide-out keyboard)");
            ImGui::TextUnformatted("type into: ");
            ImGui::SameLine();
            static char textbuf[64] = "";
            ImGui::InputText("##kb", textbuf, sizeof textbuf);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("CN")) {
            if (g_cnfont && ImGui::Button("USE CHINESE FONT"))
                ImGui::GetIO().FontDefault = g_cnfont;
            if (g_cnfont) {
                ImGui::PushFont(g_cnfont);
                ImGui::Text("诺基亚 N900 复活了");
                ImGui::Text("SGX530 图形直通，音频直连。");
                ImGui::Text("中文动态烘焙：按需光栅化。");
                ImGui::PopFont();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("WORLD")) {
            if (g_worldfont && ImGui::Button("USE WORLD FONT"))
                ImGui::GetIO().FontDefault = g_worldfont;
            ImGui::Text("FR: Bonjour le monde! Ça marche très bien.");
            ImGui::Text("DE: Grüße aus 2009 — Umlaute: äöü ß.");
            ImGui::Text("ES: ¡Hola! El niño juega con el pingüino.");
            ImGui::Text("RU: Привет, мир! Nokia N900 жив.");
            if (g_worldfont) {
                ImGui::PushFont(g_worldfont);
                ImGui::Text("JP: こんにちは世界、日本語も大丈夫。");
                ImGui::Text("CN: 中文汉字渲染：动态烘焙按需光栅化。");
                ImGui::PopFont();
            }
            if (g_krfont) {
                ImGui::PushFont(g_krfont);
                ImGui::Text("KR: 안녕하세요, 말군 고딕입니다.");
                ImGui::PopFont();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(560, 396), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(228, 74), ImGuiCond_FirstUseEver);
    ImGui::Begin("SYS", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    ImGui::Text("Nokia N900 - 2009");
    ImGui::Text("Cortex A8 + SGX530");
    ImGui::Text("imgui %s", ImGui::GetVersion());
    static bool show_demo = false;
    ImGui::Checkbox("OFFICIAL DEMO", &show_demo);
    if (show_demo) ImGui::ShowDemoWindow(&show_demo);
    ImGui::End();
}

} // extern "C"
