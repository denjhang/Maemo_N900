/* IMRUN plugin: renderer/UI settings (was builtin IMGUI app) */
#include "imgui.h"
#include <math.h>
extern "C" const char *imapp_name(void) { return "SETTINGS"; }
extern "C" void imapp_frame(double t) {
    ImGuiIO &io = ImGui::GetIO();
    ImGuiStyle &st = ImGui::GetStyle();
    static int face = 0;
    static const char *faces[2] = {"MSYH", "BITMAP"};
    ImGui::Combo("FACE", &face, faces, 2);
    static float fsize = 20.0f;
    if (ImGui::SliderFloat("FONT SIZE", &fsize, 10.0f, 32.0f, "%.0f px",
                           ImGuiSliderFlags_AlwaysClamp)) {
        st.FontSizeBase = (float)(int)(fsize + 0.5f);
        st._NextFrameFontSizeBase = st.FontSizeBase;
    }
    static float uiscale = 1.0f;
    if (ImGui::SliderFloat("UI SCALE", &uiscale, 0.6f, 2.0f, "%.2fx",
                           ImGuiSliderFlags_AlwaysClamp))
        st.FontScaleMain = uiscale;
    static double last = 0; static float fps = 60;
    if (t > last) { fps = fps * 0.9f + (float)(1.0 / (t - last)) * 0.1f; last = t; }
    ImGui::Separator();
    ImGui::Text("FPS: %.1f  (%.2f ms)", fps, fps > 0 ? 1000.0f / fps : 0);
    ImGui::Text("SGX530 - GLES 1.1 - EGL - NON-COMPOSITED");
    ImGui::Text("Display: %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
    ImGui::Text("Vertices: %d  Windows: %d", io.MetricsRenderVertices, io.MetricsRenderWindows);
    float wave[64];
    for (int i = 0; i < 64; i++)
        wave[i] = 0.5f + 0.4f * sinf((float)t * 3 + i * 0.4f) * sinf((float)t + i * 0.11f);
    ImGui::PlotLines("Wave", wave, 64, 0, "SGX load", 0.0f, 1.0f, ImVec2(360, 60));
}

extern "C" void imapp_icon(void *dlv, float cx, float cy, float s) {
    ImDrawList *dl = (ImDrawList *)dlv; ImU32 ac = IM_COL32(70,170,220,255), ln = IM_COL32(235,240,245,255);
    dl->AddCircle(ImVec2(cx, cy), s * 0.8f, ln, 40, 3.0f);
    dl->AddCircleFilled(ImVec2(cx, cy), s * 0.3f, ac);
    for (int k = 0; k < 4; k++) { float a = k * 1.5708f; dl->AddLine(ImVec2(cx + cosf(a) * s * 0.55f, cy + sinf(a) * s * 0.55f), ImVec2(cx + cosf(a) * s, cy + sinf(a) * s), ac, 4.0f); }
}
