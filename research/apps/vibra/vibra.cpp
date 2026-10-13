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
