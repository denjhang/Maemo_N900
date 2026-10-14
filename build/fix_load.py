p='imgui_gles1.cpp'
s=open(p,encoding='utf-8').read()
a=s.index('static void settings_load(void) {')
b=s.index('static void dyn_scan(void) {')
new='''static void settings_load(void) {
    FILE *f = fopen("/home/user/imrun/settings.ini", "r");
    if (!f) return;
    char line[96];
    int iv; float v;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "wall_mode=%d", &iv) == 1) { if (iv >= 0 && iv < 5) g_wall_mode = iv; }
        else if (sscanf(line, "wall_variant=%d", &iv) == 1) { if (iv >= 0 && iv < 4) g_wall_variant = iv; }
        else if (sscanf(line, "a_win=%f", &v) == 1) g_a_win = v;
        else if (sscanf(line, "a_bar=%f", &v) == 1) g_a_bar = v;
        else if (sscanf(line, "a_desk=%f", &v) == 1) g_a_desk = v;
        else if (sscanf(line, "dock_cover=%d", &iv) == 1) g_dock_covers_sysbar = iv > 0;
    }
    fclose(f);
}

'''
s=s[:a]+new+s[b:]
open(p,'w',encoding='utf-8',newline='\n').write(s)
print('load rewritten')
