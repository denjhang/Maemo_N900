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
