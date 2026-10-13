/* IMRUN plugin: multilingual fonts (was builtin FONTS) */
#include "imgui.h"
#include <math.h>
extern "C" { ImFont *imrun_font_cn(void); ImFont *imrun_font_world(void); ImFont *imrun_font_kr(void); }
extern "C" const char *imapp_name(void) { return "FONTS"; }
extern "C" void imapp_frame(double t) {
    (void)t;
    ImFont *cn = imrun_font_cn(), *wr = imrun_font_world(), *kr = imrun_font_kr();
    if (cn && ImGui::Button("USE CHINESE FONT")) ImGui::GetIO().FontDefault = cn;
    if (cn) { ImGui::PushFont(cn); ImGui::Text("诺基亚 N900 复活了"); ImGui::PopFont(); }
    ImGui::Text("FR: Bonjour! Ça marche très bien.");
    ImGui::Text("DE: Grüße — äöü ß.");
    ImGui::Text("ES: ¡Hola! El niño juega.");
    ImGui::Text("RU: Привет, мир!");
    if (wr) { ImGui::PushFont(wr); ImGui::Text("JP: こんにちは世界"); ImGui::Text("CN: 中文汉字动态烘焙。"); ImGui::PopFont(); }
    if (kr) { ImGui::PushFont(kr); ImGui::Text("KR: 안녕하세요"); ImGui::PopFont(); }
}

extern "C" void imapp_icon(void *dlv, float cx, float cy, float s) {
    ImDrawList *dl = (ImDrawList *)dlv; ImU32 ac = IM_COL32(240,130,110,255), ln = IM_COL32(235,240,245,255);
    dl->AddCircle(ImVec2(cx, cy), s * 0.8f, ac, 40, 3.0f);
    dl->AddLine(ImVec2(cx - s * 0.35f, cy), ImVec2(cx + s * 0.35f, cy), ln, 2.5f);
    dl->AddLine(ImVec2(cx, cy - s * 0.35f), ImVec2(cx, cy + s * 0.35f), ln, 2.5f);
}
