/* IMRUN plugin: Flappy Bird v6 - 使用原版图片纹理
 * 基于原版 Flappy Bird 素材（MIT License）
 * 素材来源: https://github.com/samuelcust/flappy-bird-assets
 */
#include "imgui.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <dlfcn.h>

// OpenGL ES 1.1 类型和常量
typedef unsigned int GLuint;
typedef int GLint;
typedef unsigned int GLenum;
typedef unsigned char GLubyte;

#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_LINEAR 0x2601

// OpenGL ES 函数指针（动态加载）
static void (*gl_GenTextures)(GLint n, GLuint *textures) = nullptr;
static void (*gl_BindTexture)(GLenum target, GLuint texture) = nullptr;
static void (*gl_TexParameteri)(GLenum target, GLenum pname, GLint param) = nullptr;
static void (*gl_TexImage2D)(GLenum target, GLint level, GLint internalformat,
                              GLint width, GLint height, GLint border,
                              GLenum format, GLenum type, const void *pixels) = nullptr;
static void (*gl_DeleteTextures)(GLint n, const GLuint *textures) = nullptr;

static void *gl_handle = nullptr;

// 日志函数
extern "C" void alog_public(const char *fmt, ...);

// 加载 OpenGL ES 函数
static bool load_gl_functions() {
    if (gl_handle) return true;
    
    alog_public("FLAPPY: 尝试加载 OpenGL ES 库");
    
    // N900 使用 libGLES_CM.so
    gl_handle = dlopen("libGLES_CM.so", RTLD_NOW);
    if (!gl_handle) {
        alog_public("FLAPPY: libGLES_CM.so 失败: %s", dlerror());
        gl_handle = dlopen("/usr/lib/libGLES_CM.so", RTLD_NOW);
    }
    if (!gl_handle) {
        alog_public("FLAPPY: /usr/lib/libGLES_CM.so 失败: %s", dlerror());
        return false;
    }
    
    alog_public("FLAPPY: OpenGL 库加载成功");
    
    gl_GenTextures = (void (*)(GLint, GLuint*))dlsym(gl_handle, "glGenTextures");
    gl_BindTexture = (void (*)(GLenum, GLuint))dlsym(gl_handle, "glBindTexture");
    gl_TexParameteri = (void (*)(GLenum, GLenum, GLint))dlsym(gl_handle, "glTexParameteri");
    gl_TexImage2D = (void (*)(GLenum, GLint, GLint, GLint, GLint, GLint, GLenum, GLenum, const void*))dlsym(gl_handle, "glTexImage2D");
    gl_DeleteTextures = (void (*)(GLint, const GLuint*))dlsym(gl_handle, "glDeleteTextures");
    
    bool ok = (gl_GenTextures && gl_BindTexture && gl_TexParameteri && 
               gl_TexImage2D && gl_DeleteTextures);
    
    alog_public("FLAPPY: OpenGL 函数解析: %s", ok ? "成功" : "失败");
    return ok;
}

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

// 游戏状态
static float bird_y = 240.0f;
static float bird_vy = 0.0f;
static float scroll_x = 0.0f;
static float base_x = 0.0f;
static int score = 0;
static int high_score = 0;
static bool game_over = false;
static bool started = false;
static float bird_anim_time = 0.0f;

// 纹理ID
static GLuint tex_bird[3];       // 3帧鸟动画
static GLuint tex_pipe;
static GLuint tex_background;
static GLuint tex_base;
static bool textures_loaded = false;

// 管道定义
struct Pipe {
    float x;
    float gap_y;
    bool scored;
};

static Pipe pipes[3];
static int pipe_count = 0;

// 游戏常量（基于原版）
#define BIRD_W 34.0f
#define BIRD_H 24.0f
#define BIRD_X 100.0f
#define PIPE_W 52.0f
#define PIPE_H 320.0f
#define PIPE_GAP 100.0f
#define PIPE_SPACING 180.0f
#define BASE_H 112.0f
#define GRAVITY 0.6f
#define JUMP_VELOCITY -9.0f
#define SCROLL_SPEED 2.0f

extern "C" const char *imapp_name(void) { return "FLAPPY"; }

// 加载PNG为OpenGL纹理
static GLuint load_texture(const char *path) {
    alog_public("FLAPPY: 尝试加载纹理: %s", path);
    
    int w, h, channels;
    unsigned char *data = stbi_load(path, &w, &h, &channels, 4);
    if (!data) {
        alog_public("FLAPPY: stbi_load 失败: %s", path);
        return 0;
    }
    
    alog_public("FLAPPY: 图片加载成功 %dx%d channels=%d", w, h, channels);
    
    GLuint tex;
    gl_GenTextures(1, &tex);
    gl_BindTexture(GL_TEXTURE_2D, tex);
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    alog_public("FLAPPY: 纹理上传成功 tex=%u", tex);
    
    stbi_image_free(data);
    return tex;
}

// 加载所有纹理
static void load_all_textures() {
    if (textures_loaded) return;
    
    alog_public("FLAPPY: 开始加载纹理");
    
    // 先加载 OpenGL 函数
    if (!load_gl_functions()) {
        alog_public("FLAPPY: OpenGL 函数加载失败，无法使用纹理");
        return;
    }
    
    const char *base_path = "/home/user/imrun/apps/flappy/sprites";
    
    char path[256];
    snprintf(path, sizeof path, "%s/yellowbird-midflap.png", base_path);
    tex_bird[0] = load_texture(path);
    
    snprintf(path, sizeof path, "%s/yellowbird-upflap.png", base_path);
    tex_bird[1] = load_texture(path);
    
    snprintf(path, sizeof path, "%s/yellowbird-downflap.png", base_path);
    tex_bird[2] = load_texture(path);
    
    snprintf(path, sizeof path, "%s/pipe-green.png", base_path);
    tex_pipe = load_texture(path);
    
    snprintf(path, sizeof path, "%s/background-day.png", base_path);
    tex_background = load_texture(path);
    
    snprintf(path, sizeof path, "%s/base.png", base_path);
    tex_base = load_texture(path);
    
    textures_loaded = (tex_bird[0] && tex_bird[1] && tex_bird[2] && 
                       tex_pipe && tex_background && tex_base);
    
    alog_public("FLAPPY: 纹理加载完成 bird=%u/%u/%u pipe=%u bg=%u base=%u loaded=%d",
                tex_bird[0], tex_bird[1], tex_bird[2], tex_pipe, tex_background, tex_base, textures_loaded);
}

extern "C" int imapp_init(void) {
    srand((unsigned)time(NULL));
    
    load_all_textures();
    
    bird_y = 240.0f;
    bird_vy = 0.0f;
    bird_anim_time = 0.0f;
    scroll_x = 0.0f;
    base_x = 0.0f;
    score = 0;
    game_over = false;
    started = false;
    pipe_count = 0;
    
    for (int i = 0; i < 3; i++) {
        pipes[i].x = 400.0f + i * PIPE_SPACING;
        pipes[i].gap_y = 150.0f + (rand() % 150);
        pipes[i].scored = false;
    }
    pipe_count = 3;
    
    return 0;
}

extern "C" void imapp_destroy(void) {
    if (textures_loaded && gl_DeleteTextures) {
        gl_DeleteTextures(3, tex_bird);
        gl_DeleteTextures(1, &tex_pipe);
        gl_DeleteTextures(1, &tex_background);
        gl_DeleteTextures(1, &tex_base);
        textures_loaded = false;
    }
    
    if (gl_handle) {
        dlclose(gl_handle);
        gl_handle = nullptr;
    }
}

extern "C" void imapp_icon(void *dlv, float cx, float cy, float s) {
    ImDrawList *dl = (ImDrawList *)dlv;
    // 原版风格小鸟图标
    dl->AddCircleFilled(ImVec2(cx + 2, cy + 2), s * 0.5f, IM_COL32(0, 0, 0, 60));
    dl->AddCircleFilled(ImVec2(cx, cy), s * 0.5f, IM_COL32(255, 219, 77, 255));
    dl->AddCircle(ImVec2(cx, cy), s * 0.5f, IM_COL32(214, 158, 0, 255), 16, 2.0f);
    dl->AddCircleFilled(ImVec2(cx + s * 0.2f, cy - s * 0.1f), s * 0.15f, IM_COL32(255, 255, 255, 255));
    dl->AddCircleFilled(ImVec2(cx + s * 0.25f, cy - s * 0.05f), s * 0.08f, IM_COL32(0, 0, 0, 255));
}

// 使用ImGui绘制OpenGL纹理
static void draw_texture(ImDrawList *dl, GLuint tex, ImVec2 pos, ImVec2 size, bool flip_v = false) {
    if (!tex) return;
    ImVec2 uv0 = flip_v ? ImVec2(0, 1) : ImVec2(0, 0);
    ImVec2 uv1 = flip_v ? ImVec2(1, 0) : ImVec2(1, 1);
    dl->AddImage((ImTextureID)(intptr_t)tex, pos, ImVec2(pos.x + size.x, pos.y + size.y), uv0, uv1);
}

extern "C" void imapp_frame(double t) {
    // 主程序已创建窗口，直接绘制内容
    
    static bool first_frame = true;
    if (first_frame) {
        alog_public("FLAPPY: imapp_frame 首次调用 textures_loaded=%d", textures_loaded);
        first_frame = false;
    }
    
    if (!textures_loaded) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load textures!");
        ImGui::Text("Expected path: /home/user/imrun/apps/flappy/sprites/");
        alog_public("FLAPPY: 纹理未加载，显示错误信息");
        return;
    }
    
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 canvas_p = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    
    static int frame_count = 0;
    if (frame_count == 0) {
        alog_public("FLAPPY: 画布信息 pos=(%.1f,%.1f) size=(%.1f,%.1f)", 
                    canvas_p.x, canvas_p.y, canvas_sz.x, canvas_sz.y);
    }
    frame_count++;
    
    if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
    if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
    
    // 绘制背景（平铺）
    float bg_w = 288.0f;
    float bg_h = 512.0f;
    float bg_scale = canvas_sz.y / bg_h;
    for (float x = 0; x < canvas_sz.x; x += bg_w * bg_scale) {
        draw_texture(dl, tex_background, ImVec2(canvas_p.x + x, canvas_p.y), 
                    ImVec2(bg_w * bg_scale, canvas_sz.y));
    }
    
    // 游戏逻辑
    if (!game_over && started) {
        bird_vy += GRAVITY;
        bird_y += bird_vy;
        scroll_x += SCROLL_SPEED;
        base_x -= SCROLL_SPEED;
        if (base_x <= -48.0f) base_x = 0.0f;
        
        bird_anim_time += 0.1f;
        
        for (int i = 0; i < pipe_count; i++) {
            pipes[i].x -= SCROLL_SPEED;
            
            if (pipes[i].x + PIPE_W < 0) {
                pipes[i].x = pipes[(i + pipe_count - 1) % pipe_count].x + PIPE_SPACING;
                pipes[i].gap_y = 150.0f + (rand() % 150);
                pipes[i].scored = false;
            }
            
            if (!pipes[i].scored && pipes[i].x + PIPE_W < BIRD_X) {
                pipes[i].scored = true;
                score++;
                if (score > high_score) high_score = score;
            }
            
            // 碰撞检测
            float bird_l = BIRD_X - BIRD_W/2;
            float bird_r = BIRD_X + BIRD_W/2;
            float bird_t = bird_y - BIRD_H/2;
            float bird_b = bird_y + BIRD_H/2;
            
            if (bird_r > pipes[i].x && bird_l < pipes[i].x + PIPE_W) {
                float gap_t = pipes[i].gap_y - PIPE_GAP/2;
                float gap_b = pipes[i].gap_y + PIPE_GAP/2;
                if (bird_t < gap_t || bird_b > gap_b) {
                    game_over = true;
                }
            }
        }
        
        if (bird_y - BIRD_H/2 < 0 || bird_y + BIRD_H/2 > canvas_sz.y - BASE_H) {
            game_over = true;
        }
    }
    
    // 绘制管道
    for (int i = 0; i < pipe_count; i++) {
        float x = canvas_p.x + pipes[i].x;
        float gap_y = pipes[i].gap_y;
        
        // 上管道（翻转）
        draw_texture(dl, tex_pipe, ImVec2(x, canvas_p.y + gap_y - PIPE_GAP/2 - PIPE_H),
                    ImVec2(PIPE_W, PIPE_H), true);
        
        // 下管道
        draw_texture(dl, tex_pipe, ImVec2(x, canvas_p.y + gap_y + PIPE_GAP/2),
                    ImVec2(PIPE_W, PIPE_H), false);
    }
    
    // 绘制地面（平铺）
    float base_w = 336.0f;
    for (float x = base_x; x < canvas_sz.x; x += base_w) {
        draw_texture(dl, tex_base, 
                    ImVec2(canvas_p.x + x, canvas_p.y + canvas_sz.y - BASE_H),
                    ImVec2(base_w, BASE_H));
    }
    
    // 绘制小鸟（动画）
    int bird_frame = ((int)bird_anim_time % 3);
    ImVec2 bird_pos(canvas_p.x + BIRD_X - BIRD_W/2, canvas_p.y + bird_y - BIRD_H/2);
    draw_texture(dl, tex_bird[bird_frame], bird_pos, ImVec2(BIRD_W, BIRD_H));
    
    // UI
    ImGui::SetCursorScreenPos(ImVec2(canvas_p.x + 10, canvas_p.y + 10));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    ImGui::Text("Score: %d", score);
    if (high_score > 0) {
        ImGui::SameLine();
        ImGui::Text(" | Best: %d", high_score);
    }
    ImGui::PopStyleColor();
    
    // 交互
    bool window_hovered = ImGui::IsWindowHovered();
    bool mouse_clicked = ImGui::IsMouseClicked(0);
    
    if (!started) {
        ImGui::SetCursorScreenPos(ImVec2(canvas_p.x + canvas_sz.x/2 - 80, canvas_p.y + canvas_sz.y/2 - 30));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        ImGui::Text("  Click to Start!");
        ImGui::PopStyleColor();
        
        if (window_hovered && mouse_clicked) {
            started = true;
            bird_vy = JUMP_VELOCITY;
        }
    }
    else if (!game_over) {
        if (window_hovered && mouse_clicked) {
            bird_vy = JUMP_VELOCITY;
        }
    }
    else {
        ImGui::SetCursorScreenPos(ImVec2(canvas_p.x + canvas_sz.x/2 - 60, canvas_p.y + canvas_sz.y/2 - 40));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
        ImGui::Text("GAME OVER!");
        ImGui::PopStyleColor();
        
        ImGui::SetCursorScreenPos(ImVec2(canvas_p.x + canvas_sz.x/2 - 50, canvas_p.y + canvas_sz.y/2 - 10));
        ImGui::Text("Score: %d", score);
        
        ImGui::SetCursorScreenPos(ImVec2(canvas_p.x + canvas_sz.x/2 - 70, canvas_p.y + canvas_sz.y/2 + 20));
        ImGui::Text("Click to Restart");
        
        if (window_hovered && mouse_clicked) {
            imapp_init();
            started = true;
            bird_vy = JUMP_VELOCITY;
        }
    }
}
