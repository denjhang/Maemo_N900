/* IMRUN plugin: widget lab */
#include "imgui.h"
#include <math.h>
extern "C" const char *imapp_name(void) { return "WIDGETS"; }
extern "C" void imapp_frame(double t) {
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
    float load[64];
    for (int i = 0; i < 64; i++)
        load[i] = 0.5f + 0.4f * sinf((float)t * 3 + i * 0.4f) * sinf((float)t + i * 0.11f);
    ImGui::PlotLines("LOAD", load, 64, 0, NULL, 0.0f, 1.0f, ImVec2(320, 50));
}
