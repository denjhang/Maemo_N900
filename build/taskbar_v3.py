p='imgui_gles1.cpp'
s=open(p,encoding='utf-8').read()
a=s.index('''        /* running apps: dock-node-aware chips.''')
b=s.index('        char bat[12] = "--"', a)
new='''        /* running apps: dock-node-aware chips. ALL open windows have a chip
           (minimized ones stay visible, dim) - a minimized app/group must
           always be restorable from the taskbar.
           floating windows -> one chip each; windows docked into the same
           node -> ONE group chip (label = selected member +N). */
        {
            int grp_node[8] = {0};
            static int grp_mem[8][8];
            int grp_cnt = 0;
            int vis[8], vis_n = 0;
            static int grp_n[8] = {0};
            for (int i = 0; i < g_dyn_n; i++) {
                if (!g_dyn[i].open) continue;   /* mini included: restorable */
                char tn[80];
                snprintf(tn, sizeof tn, "%s###dyn%d", g_dyn[i].name, i);
                extern int ImGui_WindowDockNodeId(const char *);
                int nid = ImGui_WindowDockNodeId(tn);
                if (nid == 0) { vis[vis_n++] = i; continue; }
                int g = -1;
                for (int q = 0; q < grp_cnt; q++) if (grp_node[q] == nid) g = q;
                if (g < 0 && grp_cnt < 8) { g = grp_cnt++; grp_node[g] = nid; grp_n[g] = 0; }
                if (g >= 0 && grp_n[g] < 8) grp_mem[g][grp_n[g]++] = i;
            }
            /* shared chip renderer: members[] + count; click = focus-all /
               minimize-all; long-press = close-all. Color per state with lerp. */
            static float chip_rgb[16][3];
            auto draw_chip = [&](int *mem, int n, int uid, int node_id) {
                int sel = mem[0];
                for (int q = 0; q < n; q++) if (mem[q] == g_focus) sel = g_focus;
                bool all_mini = true;
                for (int q = 0; q < n; q++) if (!g_dyn[mem[q]].mini) all_mini = false;
                bool foc = (!all_mini && g_focus == sel);
                bool folded = (n == 1 && g_dyn[mem[0]].collapsed);
                char label[72];
                if (n > 1) snprintf(label, sizeof label, "%s+%d", g_dyn[sel].name, n - 1);
                else snprintf(label, sizeof label, "%s", g_dyn[sel].name);
                float tgt[3];
                if (folded)      { tgt[0]=0.45f; tgt[1]=0.35f; tgt[2]=0.68f; }
                else if (all_mini){ tgt[0]=0.06f; tgt[1]=0.08f; tgt[2]=0.12f; }
                else if (foc)    { tgt[0]=0.16f; tgt[1]=0.42f; tgt[2]=0.60f; }
                else             { tgt[0]=0.12f; tgt[1]=0.16f; tgt[2]=0.24f; }
                int cq = uid < 16 ? uid : 15;
                for (int q = 0; q < 3; q++)
                    chip_rgb[cq][q] += (tgt[q] - chip_rgb[cq][q]) * 0.12f;
                float a = all_mini ? 0.5f : 1.0f;
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      vec4f(chip_rgb[cq][0], chip_rgb[cq][1], chip_rgb[cq][2], a));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, vec4f(0.16f, 0.60f, 0.32f, 1));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, vec4f(0.20f, 0.30f, 0.42f, 1));
                ImGui::PushStyleColor(ImGuiCol_Border, vec4f(0.45f, 0.55f, 0.68f, all_mini ? 0.35f : 0.8f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
                char tag[96];
                snprintf(tag, sizeof tag, "%s##tb%d", label, uid);
                static double chip_t0[16] = {0};
                int ci = cq;
                if (ImGui::Button(tag, ImVec2(0, 42))) {
                    if (chip_t0[ci] != -1) {
                        if (all_mini) {
                            for (int q = 0; q < n; q++) dyn_focus(mem[q]);  /* restore all */
                            g_focus = sel;
                        }
                        else if (!foc) { for (int q = 0; q < n; q++) dyn_focus(mem[q]); g_focus = sel; }
                        else { for (int q = 0; q < n; q++) dyn_minimize(mem[q]); }
                    }
                    chip_t0[ci] = 0;
                }
                if (ImGui::IsItemActive()) {
                    if (chip_t0[ci] == 0) chip_t0[ci] = ImGui::GetTime();
                    else if (chip_t0[ci] > 0 && ImGui::GetTime() - chip_t0[ci] > 0.5f) {
                        chip_t0[ci] = -1;
                        imrun_vibra_ping(80);
                        for (int q = 0; q < n; q++) dyn_close(mem[q]);  /* close whole chip */
                    }
                } else if (chip_t0[ci] > 0) chip_t0[ci] = 0;
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(4);
                ImGui::SameLine();
            };
            /* floating chips */
            for (int q = 0; q < vis_n; q++) {
                int i = vis[q];
                draw_chip(&i, 1, i, 0);
            }
            /* group chips: one per dock node */
            for (int g = 0; g < grp_cnt; g++) {
                if (grp_n[g] == 0) continue;
                draw_chip(grp_mem[g], grp_n[g], 100 + g, grp_node[g]);
            }
        }
'''
s=s[:a]+new+s[b:]
open(p,'w',encoding='utf-8',newline='\n').write(s)
print('taskbar v3 ok')
