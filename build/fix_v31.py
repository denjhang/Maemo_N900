import io
p='imgui_gles1.cpp'
s=open(p,encoding='utf-8').read()

# 1. FADE ALWAYS: cascade branch separates positioning from fade
old='''    if (a.cascade) {
        /* fresh open: cascade from top-left, never tiny (Windows style) */
        int n = g_cascade_seq++ % 6;
        a.cascade = false;
        ImVec2 cp = ImVec2(24 + n * 42, 20 + n * 34);
        ImVec2 cs = ImVec2(size.x < 400 ? 400 : size.x, size.y < 280 ? 280 : size.y);
        if (cp.x + cs.x > 790) cp.x = 790 - cs.x;
        if (cp.y + cs.y > 424) cp.y = 424 - cs.y;
        ImGui::SetNextWindowPos(cp, ImGuiCond_Always);
        ImGui::SetNextWindowSize(cs, ImGuiCond_Always);
    } else if (a.closing) {'''
new='''    if (a.cascade) {
        /* first-ever open: cascade position + ALSO fades in (anim set by focus) */
        int n = g_cascade_seq++ % 6;
        a.cascade = false;
        ImVec2 cp = ImVec2(24 + n * 42, 20 + n * 34);
        ImVec2 cs = ImVec2(size.x < 400 ? 400 : size.x, size.y < 280 ? 280 : size.y);
        if (cp.x + cs.x > 790) cp.x = 790 - cs.x;
        if (cp.y + cs.y > 424) cp.y = 424 - cs.y;
        ImGui::SetNextWindowPos(cp, ImGuiCond_Always);
        ImGui::SetNextWindowSize(cs, ImGuiCond_Always);
    }
    if (a.closing) {'''
assert s.count(old)==1, 'A1'
s=s.replace(old,new,1)

old2='''        } else if (a.cascade) {
            a.cascade = false;
            int n = g_cascade_seq++ % 6;
            ImVec2 cp = ImVec2(24 + n * 42, 20 + n * 34);
            if (cp.x + 420 > 790) cp.x = 790 - 420;
            if (cp.y + 300 > 424) cp.y = 424 - 300;
            ImGui::SetNextWindowPos(cp, ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(420, 300), ImGuiCond_Always);
        } else if (a.anim < 1.0f) {'''
new2='''        } else if (a.cascade) {
            a.cascade = false;
            int n = g_cascade_seq++ % 6;
            ImVec2 cp = ImVec2(24 + n * 42, 20 + n * 34);
            if (cp.x + 420 > 790) cp.x = 790 - 420;
            if (cp.y + 300 > 424) cp.y = 424 - 300;
            ImGui::SetNextWindowPos(cp, ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(420, 300), ImGuiCond_Always);
        }
        if (!a.closing && a.anim < 1.0f) {'''
assert s.count(old2)==1, 'A2'
s=s.replace(old2,new2,1)

s=s.replace('if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) g_focus = i;',
            'if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) g_focus = i;',1)
s=s.replace('if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) g_focus = NAPPS + di;',
            'if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) g_focus = NAPPS + di;',1)

old3='void igui_ctrl_combo(char c) {\n    if (c == \'w\''
new3=('static int g_desk_nav_sel = -1;   /* desktop manual nav selection */\n'
      'int igui_desk_nav_index(void) { return g_desk_nav_sel; }\n'
      'void igui_ctrl_combo(char c) {\n'
      '    printf("CTRL+%c (focus=%d)\\n", c, g_focus); fflush(stdout);\n'
      '    if (c == \'w\'')
assert s.count(old3)==1, 'A3'
s=s.replace(old3,new3,1)

old4='''            ImGui::PushStyleColor(ImGuiCol_Button,
                                  foc ? vec4f(0.16f, 0.42f, 0.60f, 1)
                                      : vec4f(0.12f, 0.16f, 0.24f, mini ? 0.5f : 1));'''
new4='''            ImGui::PushStyleColor(ImGuiCol_Button,
                                  foc ? vec4f(0.16f, 0.42f, 0.60f, 1)
                                      : vec4f(0.12f, 0.16f, 0.24f, mini ? 0.5f : 1));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, vec4f(0.16f, 0.60f, 0.32f, 1)); /* held = green */
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, vec4f(0.20f, 0.30f, 0.42f, 1));
            ImGui::PushStyleColor(ImGuiCol_Border, vec4f(0.45f, 0.55f, 0.68f, mini ? 0.35f : 0.8f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f); /* never a naked text chip */'''
assert s.count(old4)==1, 'A4'
s=s.replace(old4,new4,1)

old5='''            ImGui::PopStyleColor();
            ImGui::SameLine();
        }
        /* right side:'''
new5='''            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
            ImGui::SameLine();
        }
        /* right side:'''
assert s.count(old5)==1, 'A5'
s=s.replace(old5,new5,1)

old6='''                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    /* desktop keyboard filter'''
new6='''                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoNav); /* desktop nav handled manually (grid) */
    /* desktop keyboard filter'''
assert s.count(old6)==1, 'A6'
s=s.replace(old6,new6,1)

old7='''    /* Ctrl+M move mode: arrows nudge the focused window (24px steps) */
    if (down && g_win_move && g_focus >= 0) {'''
new7='''    /* desktop manual nav: arrows move the selection, Enter opens */
    if (down && g_focus < 0 && !g_desk_edit) {
        int total = NAPPS + g_dyn_n;
        if (total > 0) {
            if (g_desk_nav_sel < 0) g_desk_nav_sel = 0;
            if (ks == 0xff51 && g_desk_nav_sel % 4 > 0) g_desk_nav_sel--;
            else if (ks == 0xff53 && g_desk_nav_sel % 4 < 3 && g_desk_nav_sel + 1 < total) g_desk_nav_sel++;
            else if (ks == 0xff52 && g_desk_nav_sel >= 4) g_desk_nav_sel -= 4;
            else if (ks == 0xff54 && g_desk_nav_sel + 4 < total) g_desk_nav_sel += 4;
            else if (ks == 0xff0d || ks == 0xff0a) {
                if (g_desk_nav_sel < NAPPS) shell_focus(g_desk_nav_sel);
                else {
                    DynApp &da = g_dyn[g_desk_nav_sel - NAPPS];
                    if (!da.ever) { da.cascade = true; da.ever = true; }
                    da.open = true; da.mini = false; da.anim = 0; da.anim_t0 = now_double();
                }
                g_desk_nav_sel = -1;
                return;
            }
        }
        if (ks == 0xff51 || ks == 0xff52 || ks == 0xff53 || ks == 0xff54)
            return; /* consumed by desktop nav */
    }
    /* Ctrl+M move mode: arrows nudge the focused window (24px steps) */
    if (down && g_win_move && g_focus >= 0) {'''
assert s.count(old7)==1, 'A7'
s=s.replace(old7,new7,1)

old8='''            if (ImGui::IsItemFocused() && !g_desk_edit) { /* keyboard nav highlight */
                dl->AddRect(ImVec2(bx - 3, by - 3), ImVec2(bx + cellw - 11, by + cellh - 9),
                            IM_COL32(120, 220, 255, 255), 12.0f, 0, 3.5f);
            }'''
new8='''            if (i == igui_desk_nav_index() && !g_desk_edit) { /* manual kb nav */
                dl->AddRect(ImVec2(bx - 3, by - 3), ImVec2(bx + cellw - 11, by + cellh - 9),
                            IM_COL32(120, 220, 255, 255), 12.0f, 0, 3.5f);
            }'''
assert s.count(old8)==1, 'A8'
s=s.replace(old8,new8,1)

# decl for igui_desk_nav_index (used before def)
anchor='extern int ImGui_OpenPopupCount(void);   /* patched-core BACK support */'
add=anchor+'\nextern int igui_desk_nav_index(void);'
assert s.count(anchor)==1, 'A9'
s=s.replace(anchor,add,1)
open(p,'w',encoding='utf-8',newline='\n').write(s)
print('ALL OK')
