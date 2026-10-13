/* IMRUN plugin: vibra control (was builtin VIBRA) */
#include "imgui.h"
#include <math.h>
extern "C" { void vibra_ping(int ms); void vibra_stop(void); }
extern "C" const char *imapp_name(void) { return "VIBRA"; }
extern "C" void imapp_destroy(void) { vibra_stop(); }
extern "C" void imapp_frame(double t) {
    (void)t;
    static int ms = 1000;
    ImGui::SliderInt("DURATION", &ms, 100, 5000, "%d ms");
    if (ImGui::Button("VIBRATE", ImVec2(140, 48)))
        vibra_ping(ms);
    ImGui::SameLine();
    if (ImGui::Button("STOP ALL", ImVec2(140, 48)))
        vibra_stop();
}

extern "C" void imapp_icon(void *dlv, float cx, float cy, float s) {
    ImDrawList *dl = (ImDrawList *)dlv; ImU32 ac = IM_COL32(220,120,220,255), ln = IM_COL32(235,240,245,200);
    dl->AddRectFilled(ImVec2(cx - s * 0.35f, cy - s * 0.7f), ImVec2(cx + s * 0.35f, cy + s * 0.7f), ac, 6.0f);
    for (int k = 0; k < 2; k++) { float r = s * (0.55f + k * 0.25f);
        dl->PathArcTo(ImVec2(cx, cy), r, 0.75f * 3.14159f, 1.25f * 3.14159f, 20); dl->PathStroke(ln, 0, 2.5f);
        dl->PathArcTo(ImVec2(cx, cy), r, 1.75f * 3.14159f, 2.25f * 3.14159f, 20); dl->PathStroke(ln, 0, 2.5f); }
}
