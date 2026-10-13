/* IMRUN system plugin: APPMGR (install/manage). Protected plugin. */
#include "imgui.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
int imrun_dyn_count(void);
const char *imrun_dyn_name(int i);
int imrun_dyn_state(int i);            /* 0 stop 1 bg 2 run */
void imrun_dyn_kill(int i);
void imrun_dyn_uninstall(int i);
void imrun_host_rescan(void);
}

extern "C" const char *imapp_name(void) { return "APPMGR"; }
extern "C" int imapp_system(void) { return 1; }
extern "C" void imapp_icon(void *dlv, float cx, float cy, float s) {
    ImDrawList *dl = (ImDrawList *)dlv;
    for (int r = 0; r < 2; r++)
        for (int k = 0; k < 2; k++)
            dl->AddRectFilled(ImVec2(cx - s * 0.7f + k * s * 0.8f, cy - s * 0.7f + r * s * 0.8f),
                              ImVec2(cx - s * 0.7f + k * s * 0.8f + s * 0.55f,
                                     cy - s * 0.7f + r * s * 0.8f + s * 0.55f),
                              (r + k) % 2 ? IM_COL32(70, 160, 140, 255) : IM_COL32(90, 130, 180, 255), 4.0f);
}

extern "C" void imapp_frame(double t) {
    (void)t;
    static char filter[40] = "";
    if (ImGui::BeginTabBar("mgr")) {
        if (ImGui::BeginTabItem("INSTALL")) {
            ImGui::TextUnformatted("source: /home/user/MyDocs (.so / .imapp via USB)");
            if (ImGui::Button("RESCAN NOW", ImVec2(-1, 40)))
                imrun_host_rescan();
            ImGui::InputTextWithHint("##flt", "filter...", filter, sizeof filter);
            DIR *d = opendir("/home/user/MyDocs");
            if (d) {
                struct dirent *e;
                while ((e = readdir(d))) {
                    const char *ext = strrchr(e->d_name, '.');
                    if (!ext || (strcmp(ext, ".so") && strcmp(ext, ".imapp"))) continue;
                    if (filter[0] && !strstr(e->d_name, filter)) continue;
                    char path[160];
                    snprintf(path, sizeof path, "/home/user/MyDocs/%s", e->d_name);
                    ImGui::BulletText("%s", e->d_name);
                    ImGui::SameLine(300);
                    char bid[80];
                    snprintf(bid, sizeof bid, "INSTALL##%s", e->d_name);
                    if (ImGui::Button(bid, ImVec2(110, 32))) {
                        char cmd[256], idname[40];
                        snprintf(idname, sizeof idname, "%.*s", (int)(ext - e->d_name), e->d_name);
                        if (!strcmp(ext, ".imapp"))
                            snprintf(cmd, sizeof cmd,
                                     "mkdir -p /home/user/imrun/apps && cd /home/user/imrun/apps && rm -rf %s && tar xzf %s",
                                     idname, path);
                        else
                            snprintf(cmd, sizeof cmd,
                                     "mkdir -p /home/user/imrun/apps/%s && cp %s /home/user/imrun/apps/%s/app.so",
                                     idname, path, idname);
                        system(cmd);
                    }
                }
                closedir(d);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("MANAGE")) {
            int n = imrun_dyn_count();
            ImGui::Text("installed plugins: %d", n);
            ImGui::Separator();
            for (int k = 0; k < n; k++) {
                const char *nm = imrun_dyn_name(k);
                int st = imrun_dyn_state(k);
                ImGui::Text("%s  [%s]", nm,
                            st == 2 ? "running" : st == 1 ? "background" : "stopped");
                ImGui::SameLine(280);
                char kid[48], uid[48];
                snprintf(kid, sizeof kid, "KILL##m%d", k);
                snprintf(uid, sizeof uid, "UNINSTALL##m%d", k);
                if (st && ImGui::Button(kid, ImVec2(60, 30)))
                    imrun_dyn_kill(k);
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.15f, 0.15f, 1));
                if (ImGui::Button(uid, ImVec2(110, 30)))
                    imrun_dyn_uninstall(k);
                ImGui::PopStyleColor();
            }
            ImGui::Separator();
            ImGui::TextUnformatted("long-press a desktop icon for quick uninstall");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
