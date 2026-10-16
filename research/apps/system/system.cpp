/* IMRUN system plugin: SYSTEM (merged settings/info/about). Protected:
   imapp_system()=1 - cannot be uninstalled, otherwise a normal plugin. */
#include "imgui.h"
#include <math.h>

extern "C" {
extern int g_wall_mode, g_wall_variant;
extern float g_a_win, g_a_bar, g_a_desk;
extern bool g_dock_covers_sysbar;
extern char hw[8][96];
extern int hw_n;
}

/* 音频设置 (v0.4.45 起从 demo 插件迁入): 后端选择 + 测试音 (TONEGEN) + 流统计.
   后端切换 + 测试音输出使用与 demo 完全相同的宿主 API. */
extern "C" {
void audio_ui_start(int seconds);
void audio_ui_stop(void);
void audio_ui_set_backend(int be);
extern int audio_on;
extern int audio_backend;       /* 0=ALSA 1=libao 2=SDL(实际复用 ALSA plughw 路径) */
extern int audio_volume, audio_wave;
extern long audio_flow;
extern int dbg_underruns, dbg_stalls;
extern double audio_deadline;
double imrun_now(void);
}

extern "C" {
void imrun_ui_fontface(int);
void imrun_ui_fontsize(float);
void imrun_ui_scale(float);
}
extern "C" const char *imapp_name(void) { return "SETTINGS"; }
extern "C" int imapp_system(void) { return 1; }
extern "C" void imapp_icon(void *dlv, float cx, float cy, float s) {
    ImDrawList *dl = (ImDrawList *)dlv;
    for (int k = 0; k < 3; k++) {
        float o = s * (0.45f - k * 0.18f);
        ImU32 cc = IM_COL32(60 + k * 50, 80 + k * 60, 120 + k * 60, 200 - k * 40);
        dl->AddRectFilled(ImVec2(cx - o, cy - o), ImVec2(cx + o, cy + o), cc, 4.0f + k * 3.0f);
    }
}

extern "C" void imapp_frame(double t) {
    (void)t;
    if (ImGui::BeginTabBar("systabs")) {
        if (ImGui::BeginTabItem("WALLPAPER")) {
            const char *modes[4] = {"3D DYNAMIC", "3D STATIC", "2D DYNAMIC", "2D STATIC"};
            int m = g_wall_mode;
            if (ImGui::Combo("MODE", &m, modes, 4)) g_wall_mode = m;
            const char *pals[4] = {"AZURE", "VIOLET", "EMERALD", "AMBER"};
            int v = g_wall_variant;
            if (ImGui::Combo("PALETTE", &v, pals, 4)) g_wall_variant = v;
            ImGui::Separator();
            ImGui::TextUnformatted("TRANSPARENCY");
            ImGui::SliderFloat("WINDOW", &g_a_win, 0.4f, 1.0f, "%.2f");
            ImGui::SliderFloat("TASKBAR", &g_a_bar, 0.3f, 1.0f, "%.2f");
            ImGui::SliderFloat("DESK TINT", &g_a_desk, 0.0f, 0.8f, "%.2f");
            ImGui::Separator();
            ImGui::Checkbox("DOCK MAY COVER SYSTEM BAR", &g_dock_covers_sysbar);
            ImGui::TextUnformatted("(off: docked/maximized windows stop above the bar)");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("DISPLAY")) {
            static int face = 0;
            static const char *faces[2] = {"MSYH", "BITMAP"};
            if (ImGui::Combo("FACE", &face, faces, 2))
                imrun_ui_fontface(face);
            static float fsize = 20.0f;
            if (ImGui::SliderFloat("FONT SIZE", &fsize, 10.0f, 32.0f, "%.0f px",
                                   ImGuiSliderFlags_AlwaysClamp))
                imrun_ui_fontsize(fsize);
            static float uiscale = 1.0f;
            if (ImGui::SliderFloat("UI SCALE", &uiscale, 0.6f, 2.0f, "%.2fx",
                                   ImGuiSliderFlags_AlwaysClamp))
                imrun_ui_scale(uiscale);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("AUDIO")) {
            /* 后端选择 (v0.4.45 起默认 SDL; 切换走 audio-ctl 线程串行化, 不会漏设备) */
            ImGui::TextUnformatted("OUT BACKEND");
            if (ImGui::RadioButton("ALSA plughw",  audio_backend == 0)) audio_ui_set_backend(0);
            ImGui::SameLine();
            if (ImGui::RadioButton("libao(VGM)",   audio_backend == 1)) audio_ui_set_backend(1);
            ImGui::SameLine();
            if (ImGui::RadioButton("SDL(pulse)",   audio_backend == 2)) audio_ui_set_backend(2);
            ImGui::SameLine();
            if (ImGui::RadioButton("PULSE (paplay)", audio_backend == 3)) audio_ui_set_backend(3);
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                "v0.4.44+: SDL 复用 ALSA plughw 路径. v0.4.46+: PULSE 走 libpulse-simple (paplay 风格, 受系统音量控制)");
            ImGui::Separator();
            /* 测试音 (TONEGEN, 440Hz 内置发生器) */
            ImGui::TextUnformatted("TEST TONE");
            if (audio_on) {
                int left = (int)(audio_deadline - imrun_now());
                if (left < 0) left = 0;
                ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f),
                                   "PLAYING (%ds left)", left);
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "idle");
            }
            if (ImGui::Button("PLAY 20s", ImVec2(140, 48))) audio_ui_start(20);
            ImGui::SameLine();
            if (ImGui::Button("STOP", ImVec2(140, 48))) audio_ui_stop();
            const char *waves[4] = {"SINE", "TRIANGLE", "SQUARE", "SAW"};
            ImGui::Combo("WAVE", &audio_wave, waves, 4);
            ImGui::SliderInt("VOL", &audio_volume, 0, 1000, "%d%%");
            ImGui::Separator();
            /* 流统计 (实时反馈: flow=0=后端死了; underrun/stall=卡顿计数) */
            ImGui::Text("flow=%ld | underrun=%d | stall=%d",
                        audio_flow, dbg_underruns, dbg_stalls);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("INFO")) {
            for (int i = 0; i < hw_n; i++) ImGui::TextUnformatted(hw[i]);
            ImGui::Separator();
            ImGui::Text("Nokia N900 - 2009 - Cortex A8 + SGX530");
            ImGui::Text("imgui %s", ImGui::GetVersion());
            if (ImGui::BeginTable("hw", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("COMPONENT");
                ImGui::TableSetupColumn("VALUE");
                ImGui::TableHeadersRow();
                const char *rows[6][2] = {
                    {"CPU", "Cortex-A8 600MHz"}, {"GPU", "PowerVR SGX530"},
                    {"RAM", "256MB LPDDR"}, {"SCREEN", "800x480 AMOLED"},
                    {"GLES", "1.1 fixed-func"}, {"COMPOSITOR", "DISABLED"}};
                for (int i = 0; i < 6; i++) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(rows[i][0]);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(rows[i][1]);
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ABOUT")) {
            ImGui::Text("IMRUN v0.4");
            ImGui::TextUnformatted("hot-loadable ImGui runtime - bare X11+EGL+GLES1.1");
            ImGui::TextUnformatted("ALL apps are plugins (system = protected)");
            ImGui::Separator();
            ImGui::TextUnformatted("CHANGELOG");
            ImGui::BulletText("v0.4 unified plugin architecture - system apps are protected plugins");
            ImGui::BulletText("v0.3 dock respects sysbar, scrolling desktop, kb filter, fades");
            ImGui::BulletText("v0.2x touch scroll, app icons, APPMGR, long-press uninstall");
            ImGui::BulletText("v0.1x audio saga, shell, native minimize (core patches 1-2)");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
