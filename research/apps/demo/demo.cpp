/* IMRUN demo plugin: all demos merged (widgets lab + hello + render stats).
   Icon: the classic IMRUN smiley. */
#include "imgui.h"
#include <math.h>

extern "C" const char *imapp_name(void) { return "DEMO"; }
extern "C" void imapp_icon(void *dlv, float cx, float cy, float s) {
    ImDrawList *dl = (ImDrawList *)dlv;
    ImU32 ac = IM_COL32(160, 120, 220, 255), ln = IM_COL32(235, 240, 245, 255);
    dl->AddCircle(ImVec2(cx, cy), s, ac, 40, 3.0f);
    dl->AddCircleFilled(ImVec2(cx - s * 0.35f, cy - s * 0.25f), s * 0.1f, ln);
    dl->AddCircleFilled(ImVec2(cx + s * 0.35f, cy - s * 0.25f), s * 0.1f, ln);
    dl->PathArcTo(ImVec2(cx, cy + s * 0.1f), s * 0.5f, 0.2f, 2.94f, 20);
    dl->PathStroke(ln, 0, 2.5f);
}

extern "C" {
void audio_ui_start(int seconds);
void audio_ui_stop(void);
void audio_ui_set_backend(int be);
extern int audio_on;
extern long audio_flow;
extern int dbg_underruns;
extern int dbg_stalls;
extern double audio_deadline;
extern int audio_volume, audio_wave;
extern int audio_backend; /* 0 ALSA 1 libao(vgmplay) 2 SDL(ppsspp) */
void vibra_ping(int ms);
void vibra_stop(void);
extern char last_key[64];
ImFont *imrun_font_cn(void);
ImFont *imrun_font_world(void);
ImFont *imrun_font_kr(void);
double imrun_now(void);
}
extern "C" void imapp_destroy(void) {
    audio_ui_stop();   /* tonegen tab: audio dies with the app */
    vibra_stop();
}

extern "C" void imapp_frame(double t) {
    if (ImGui::BeginTabBar("demotabs")) {
    if (ImGui::BeginTabItem("HELLO")) {
    /* --- hello --- */
    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.6f, 1.0f),
                       "hot-loaded plugin - dlopen'd live, no restart");
    static int hello_n = 0;
    if (ImGui::Button("TAP ME")) hello_n++;
    ImGui::SameLine();
    ImGui::Text("count=%d", hello_n);
    ImGui::Separator();

    /* --- widgets lab --- */
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

    /* --- render stats + wave --- */
    if (ImGui::TreeNode("RENDER STATS")) {
        ImGuiIO &io = ImGui::GetIO();
        ImGui::Text("Display: %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
        ImGui::Text("Vertices: %d  Windows: %d",
                    io.MetricsRenderVertices, io.MetricsRenderWindows);
        static double last = 0; static float fps = 60;
        if (t > last) { fps = fps * 0.9f + (float)(1.0 / (t - last)) * 0.1f; last = t; }
        ImGui::Text("FPS: %.1f", fps);
        ImGui::TreePop();
    }
    float wave[64];
    for (int i = 0; i < 64; i++)
        wave[i] = 0.5f + 0.4f * sinf((float)t * 3 + i * 0.4f) * sinf((float)t + i * 0.11f);
    ImGui::PlotLines("Wave", wave, 64, 0, "SGX load", 0.0f, 1.0f, ImVec2(360, 60));
    ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("TONEGEN")) {
        const char *bname = audio_backend == 0 ? "ALSA  " : audio_backend == 1 ? "AO(VGM)" : "SDL(PSP)";
        if (audio_on) {
            int left = (int)(audio_deadline - imrun_now());
            if (left < 0) left = 0;   /* deadline can lapse mid-drain */
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f),
                               "%s PLAYING 440Hz (%ds left)", bname, left);
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f),
                               "%s idle", bname);
        }
        /* live proof of stream: frames produced this session keeps climbing while
           a backend is actually delivering. zero = nothing is flowing. */
        ImGui::Text("flow=%ld | underrun=%d | stall=%d", audio_flow, dbg_underruns, dbg_stalls);

        if (ImGui::Button("PLAY 20s", ImVec2(140, 48)))
            audio_ui_start(20);
        ImGui::SameLine();
        if (ImGui::Button("STOP", ImVec2(140, 48)))
            audio_ui_stop();

        const char *waves[4] = {"SINE", "TRIANGLE", "SQUARE", "SAW"};
        ImGui::Combo("WAVE", &audio_wave, waves, 4);

        ImGui::Text("OUT BACKEND:");
        ImGui::SameLine();
        /* safe switch: serialized through the audio-control thread so the OLD
           backend is fully STOPped before the flag flips (no leaked device,
           no "switched then silent") */
        if (ImGui::RadioButton("ALSA", audio_backend == 0)) audio_ui_set_backend(0);
        ImGui::SameLine();
        if (ImGui::RadioButton("AO(VGM)", audio_backend == 1)) audio_ui_set_backend(1);
        ImGui::SameLine();
        if (ImGui::RadioButton("SDL(PSP)", audio_backend == 2)) audio_ui_set_backend(2);

        ImGui::SliderInt("VOL", &audio_volume, 0, 1000, "%d%%");
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("VIBRA")) {
        static int ms = 1000;
        ImGui::SliderInt("DURATION", &ms, 100, 5000, "%d ms");
        if (ImGui::Button("VIBRATE", ImVec2(140, 48)))
            vibra_ping(ms);
        ImGui::SameLine();
        if (ImGui::Button("STOP ALL", ImVec2(140, 48)))
            vibra_stop();
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("KEYS")) {
        ImGui::Text("last: %s", last_key[0] ? last_key : "(press slide-out keyboard)");
        ImGui::TextUnformatted("type into: ");
        ImGui::SameLine();
        static char textbuf[64] = "";
        ImGui::InputText("##kb", textbuf, sizeof textbuf);
        ImGui::Text("CTRL+W close / CTRL+M minimize");
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("FONTS")) {
        ImFont *cn = imrun_font_cn(), *wr = imrun_font_world(), *kr = imrun_font_kr();
        if (cn && ImGui::Button("USE CHINESE FONT")) ImGui::GetIO().FontDefault = cn;
        if (cn) { ImGui::PushFont(cn); ImGui::Text("诺基亚 N900 复活了"); ImGui::PopFont(); }
        ImGui::Text("FR: Bonjour! Ça marche très bien.");
        ImGui::Text("DE: Grüße — äöü ß.");
        ImGui::Text("ES: ¡Hola! El niño juega.");
        ImGui::Text("RU: Привет, мир!");
        if (wr) { ImGui::PushFont(wr); ImGui::Text("JP: こんにちは世界"); ImGui::Text("CN: 中文汉字动态烘焙。"); ImGui::PopFont(); }
        if (kr) { ImGui::PushFont(kr); ImGui::Text("KR: 안녕하세요"); ImGui::PopFont(); }
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
    }
}
