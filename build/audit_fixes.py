import re
results=[]

# ---------- imgui_gles1.cpp ----------
p='imgui_gles1.cpp'
s=open(p,encoding='utf-8').read()

def rep(s, old, new, tag):
    if s.count(old)!=1:
        results.append(('FAIL',tag,s.count(old)))
        return s
    results.append(('OK',tag,''))
    return s.replace(old,new,1)

# T1 upload reuse
s=rep(s,'''static void upload_texture(ImTextureData *tex) {
    unsigned id = 0;
    gl.GenTextures(1, &id);
    gl.BindTexture(GL_TEXTURE_2D, id);''',
'''static void upload_texture(ImTextureData *tex) {
    unsigned id = (unsigned)(intptr_t)tex->TexID;
    if (id == 0) {
        gl.GenTextures(1, &id);
        tex->SetTexID((ImTextureID)(intptr_t)id);
    }
    gl.BindTexture(GL_TEXTURE_2D, id);''','T1 upload-reuse')

# T2 destroy deletes
s=rep(s,'''        else if (tex->Status == ImTextureStatus_WantDestroy)
            tex->SetStatus(ImTextureStatus_Destroyed);''',
'''        else if (tex->Status == ImTextureStatus_WantDestroy) {
            unsigned did = (unsigned)(intptr_t)tex->TexID;
            if (did && pglDeleteTextures) pglDeleteTextures(1, &did);
            tex->SetStatus(ImTextureStatus_Destroyed);
        }''','T2 destroy-delete')

# T3 per-cmd bind
s=rep(s,'''    gl.BindTexture(GL_TEXTURE_2D, fontTex);
    for (int n = 0; n < dd->CmdListsCount; n++) {''',
'''    for (int n = 0; n < dd->CmdListsCount; n++) {''','T3 drop global bind')

# T4 per-cmd texture
s=rep(s,'''            if (cmd->UserCallback) continue;
            ImVec4 crs = cmd->ClipRect;''',
'''            if (cmd->UserCallback) continue;
            unsigned ctid = (unsigned)(intptr_t)cmd->GetTexID();
            if (ctid) gl.BindTexture(GL_TEXTURE_2D, ctid);   /* per-cmd atlas */
            ImVec4 crs = cmd->ClipRect;''','T4 per-cmd tex')

# T5 dlsym decl
s=rep(s,'static IGlfns gl;',
'''static void (*pglDeleteTextures)(unsigned, const unsigned *);
static IGlfns gl;''','T5 decl')
s=rep(s,'    DLSYM(hGLES, pglClearColor, "glClearColor");',
'''    DLSYM(hGLES, pglClearColor, "glClearColor");
    DLSYM(hGLES, pglDeleteTextures, "glDeleteTextures");''','T6 dlsym')

# S1 (高): closing processed before mini guard
s=rep(s,'''    DynApp &a = g_dyn[di];
    if ((!a.open && !a.closing) || a.mini) return false;''',
'''    DynApp &a = g_dyn[di];
    if (a.closing) { /* handled below: fade+destroy even when mini */ }
    else if (!a.open || a.mini) return false;''','S1 closing-before-mini')

# S2 (高): uninstall syncs order
s=rep(s,'''        for (int k = di; k < g_dyn_n - 1; k++) g_dyn[k] = g_dyn[k + 1];
        g_dyn_n--;
        if (g_focus >= di) g_focus = -1;''',
'''        for (int k = di; k < g_dyn_n - 1; k++) g_dyn[k] = g_dyn[k + 1];
        g_dyn_n--;
        if (g_focus >= di) g_focus = -1;
        g_desk_last = -1;
        order_sync();      /* stale indices poisoned grid + saved order */
        settings_save();''','S2 uninstall-order')

# S3 (高): order_swap id write-back
s=rep(s,'''    snprintf(g_order_id[a], 40, "%s", g_dyn[t].id);
    snprintf(g_order_id[b], 40, "%s", g_dyn[g_order[a]].id);''',
'''    snprintf(g_order_id[a], 40, "%s", g_dyn[g_order[a]].id);
    snprintf(g_order_id[b], 40, "%s", g_dyn[g_order[b]].id);''','S3 swap-ids')

# S4 (中): reopen during fade cancels close
s=rep(s,'''static void dyn_focus(int di) {
    if (di < 0 || di >= g_dyn_n) return;
    DynApp &a = g_dyn[di];
    if (!a.ever) { a.cascade = true; a.ever = true; }
    a.open = true; a.mini = false;''',
'''static void dyn_focus(int di) {
    if (di < 0 || di >= g_dyn_n) return;
    DynApp &a = g_dyn[di];
    if (!a.ever) { a.cascade = true; a.ever = true; }
    if (a.closing) a.closing = false;   /* reopen during fade = cancel */
    a.open = true; a.mini = false;''','S4 cancel-close')

# S5 (中): destroyed flag, re-init on reopen, guard double destroy
s=rep(s,'''    int (*sys)(void);
    bool open, mini, ever, cascade, closing;''',
'''    int (*sys)(void);
    bool destroyed;
    bool open, mini, ever, cascade, closing;''','S5a field')
s=rep(s,'''        a.open = a.mini = a.ever = a.cascade = a.closing = false;''',
'''        a.open = a.mini = a.ever = a.cascade = a.closing = a.destroyed = false;''','S5b init')
s=rep(s,'''        if (t >= 1.0f) {
            a.open = false; a.closing = false; a.mini = false;
            if (a.destroy) a.destroy();
            return false;
        }''',
'''        if (t >= 1.0f) {
            a.open = false; a.closing = false; a.mini = false;
            if (a.destroy && !a.destroyed) { a.destroy(); a.destroyed = true; }
            return false;
        }''','S5c destroy-once')
s=rep(s,'''    a.open = true; a.mini = false;
    a.uncoll = true;                 /* opening/focusing always expands */''',
'''    if (a.destroyed && a.init) { a.init(); a.destroyed = false; }  /* fresh start */
    a.open = true; a.mini = false;
    a.uncoll = true;                 /* opening/focusing always expands */''','S5d reinit')
s=rep(s,'''        a.open = false;
        if (a.destroy) a.destroy();
        dlclose(a.h);''',
'''        a.open = false;
        if (a.destroy && !a.destroyed) { a.destroy(); a.destroyed = true; }
        dlclose(a.h);''','S5e uninstall-guard')
open(p,'w',encoding='utf-8',newline='\n').write(s)

# ---------- gldemo_imgui.c ----------
p2='gldemo_imgui.c'
s2=open(p2,encoding='utf-8').read()
def rep2(old,new,tag):
    global s2
    if s2.count(old)!=1:
        results.append(('FAIL',tag,s2.count(old)))
        return
    results.append(('OK',tag,''))
    s2=s2.replace(old,new,1)

rep2('''        if (audio_deadline && now() > audio_deadline) {
            thread_run = 0;   /* keep state truthful for the next start */
            break;
        }''','''        if (audio_deadline && now() > audio_deadline) {
            thread_run = 0; audio_on = 0;   /* truthful state for UI */
            break;
        }''','G6 audio-deadline-state')
open(p2,'w',encoding='utf-8',newline='\n').write(s2)

for r in results: print(r)
