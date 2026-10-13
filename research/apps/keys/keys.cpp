/* IMRUN plugin: hardware keyboard test (was builtin KEYS) */
#include "imgui.h"
#include <math.h>
extern "C" { extern char last_key[64]; }
extern "C" const char *imapp_name(void) { return "KEYS"; }
extern "C" void imapp_frame(double t) {
    (void)t;
    ImGui::Text("last: %s", last_key[0] ? last_key : "(press slide-out keyboard)");
    ImGui::TextUnformatted("type into: ");
    ImGui::SameLine();
    static char textbuf[64] = "";
    ImGui::InputText("##kb", textbuf, sizeof textbuf);
    ImGui::Text("CTRL+W close / CTRL+M minimize");
}

extern "C" void imapp_icon(void *dlv, float cx, float cy, float s) {
    ImDrawList *dl = (ImDrawList *)dlv; ImU32 ac = IM_COL32(200,200,210,255), ln = IM_COL32(235,240,245,255);
    dl->AddRect(ImVec2(cx - s, cy - s * 0.6f), ImVec2(cx + s, cy + s * 0.6f), ln, 4.0f, 0, 2.5f);
    for (int r = 0; r < 3; r++) for (int k = 0; k < 5; k++) dl->AddRectFilled(ImVec2(cx - s * 0.8f + k * s * 0.4f, cy - s * 0.42f + r * s * 0.34f), ImVec2(cx - s * 0.8f + k * s * 0.4f + s * 0.28f, cy - s * 0.42f + r * s * 0.34f + s * 0.2f), (r == 2 && k == 1) ? ac : IM_COL32(200,210,220,160), 2.0f);
}
