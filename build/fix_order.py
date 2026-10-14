p='imgui_gles1.cpp'
s=open(p,encoding='utf-8').read()
old='''            g_dock_covers_sysbar ? 1 : 0);
    fclose(f);
}'''
new='''            g_dock_covers_sysbar ? 1 : 0);
    for (int q = 0; q < g_order_n; q++)
        fprintf(f, "ord=%s\\n", g_dyn[g_order[q]].id);
    fclose(f);
}
extern "C" void imrun_settings_touch(void) { settings_save(); }  /* order swap hook */'''
assert s.count(old)==1
s=s.replace(old,new,1)
old3='''        else if (sscanf(line, "dock_cover=%d", &iv) == 1) g_dock_covers_sysbar = iv > 0;
    }'''
new3='''        else if (sscanf(line, "dock_cover=%d", &iv) == 1) g_dock_covers_sysbar = iv > 0;
        else if (strncmp(line, "ord=", 4) == 0) {
            char idv[40];
            if (sscanf(line + 4, "%39s", idv) == 1 && g_order_n < 8)
                snprintf(g_order_id[g_order_n++], 40, "%s", idv);
        }
    }'''
assert s.count(old3)==1
s=s.replace(old3,new3,1)
# initial order_sync after first scan (settings_load fills g_order_id first)
old4='''    static bool s_loaded = false;
    if (!s_loaded) { s_loaded = true; settings_load(); }'''
new4='''    static bool s_loaded = false;
    if (!s_loaded) { s_loaded = true; settings_load(); }
    if (g_order_n == 0 && g_dyn_n > 0) order_sync();   /* first boot default */'''
assert s.count(old4)==1
s=s.replace(old4,new4,1)
open(p,'w',encoding='utf-8',newline='\n').write(s)
print('all ok')
