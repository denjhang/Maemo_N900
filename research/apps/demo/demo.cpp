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

extern "C" void imapp_frame(double t) {
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
}
