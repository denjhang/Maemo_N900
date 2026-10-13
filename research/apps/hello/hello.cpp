/* IMRUN sample hot-load app: drop this dir into /home/user/imrun/apps/hello/
   (as app.so) - it appears on the desktop within ~2s, no restart. */
#include "imgui.h"
#include <math.h>
extern "C" const char *imapp_name(void) { return "HELLO"; }
extern "C" void imapp_frame(double t) {
    ImGui::Text("hot-loaded at t=%.1f", t);
    ImGui::TextUnformatted("this .so was dlopen'd live - no restart");
    static float v = 0.5f;
    ImGui::SliderFloat("value", &v, 0.0f, 1.0f);
    static int n = 0;
    if (ImGui::Button("tap")) n++;
    ImGui::SameLine();
    ImGui::Text("count=%d", n);
    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.6f, 1), "IMRUN plugin SDK works");
}

extern "C" void imapp_icon(void *dlv, float cx, float cy, float s) {
    ImDrawList *dl = (ImDrawList *)dlv; ImU32 ac = IM_COL32(160,120,220,255), ln = IM_COL32(235,240,245,255);
    dl->AddCircle(ImVec2(cx, cy), s, ac, 40, 3.0f);
    dl->AddCircleFilled(ImVec2(cx - s * 0.35f, cy - s * 0.25f), s * 0.1f, ln);
    dl->AddCircleFilled(ImVec2(cx + s * 0.35f, cy - s * 0.25f), s * 0.1f, ln);
    dl->PathArcTo(ImVec2(cx, cy + s * 0.1f), s * 0.5f, 0.2f, 2.94f, 20); dl->PathStroke(ln, 0, 2.5f);
}
