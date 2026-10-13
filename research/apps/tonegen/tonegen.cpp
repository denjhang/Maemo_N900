/* IMRUN plugin: ALSA waveform generator (was builtin TONEGEN) */
#include "imgui.h"
#include <math.h>
extern "C" {
void audio_ui_start(int seconds);
void audio_ui_stop(void);
extern int audio_on;
extern double audio_deadline;
extern int audio_volume, audio_wave;
double imrun_now(void);
void vibra_ping(int ms);
}
extern "C" const char *imapp_name(void) { return "TONEGEN"; }
extern "C" void imapp_destroy(void) { audio_ui_stop(); } /* audio dies with app */
extern "C" void imapp_frame(double t) {
    (void)t;
    if (audio_on) {
        int left = (int)(audio_deadline - imrun_now());
        ImGui::Text("ALSA PCM: PLAYING 440Hz (%ds left)", left);
    } else
        ImGui::Text("ALSA PCM: idle");
    if (ImGui::Button("PLAY 20s", ImVec2(140, 48)))
        audio_ui_start(20);
    ImGui::SameLine();
    if (ImGui::Button("STOP", ImVec2(140, 48)))
        audio_ui_stop();
    const char *waves[4] = {"SINE", "TRIANGLE", "SQUARE", "SAW"};
    ImGui::Combo("WAVE", &audio_wave, waves, 4);
    ImGui::SliderInt("VOL", &audio_volume, 0, 1000, "%d%%");
}

extern "C" void imapp_icon(void *dlv, float cx, float cy, float s) {
    ImDrawList *dl = (ImDrawList *)dlv; ImU32 ac = IM_COL32(90,220,140,255);
    ImVec2 p = ImVec2(cx - s, cy);
    for (int k = 1; k <= 24; k++) { float x = cx - s + 2 * s * k / 24.0f, y = cy - sinf(k * 0.4f) * s * 0.55f; dl->AddLine(p, ImVec2(x, y), ac, 3.5f); p = ImVec2(x, y); }
}
