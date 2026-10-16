/* IMRUN plugin: Flappy Bird v7 - 使用主程序的 PNG 加载器
 * 基于原版 Flappy Bird 素材（MIT License）
 * 素材来源: https://github.com/samuelcust/flappy-bird-assets
 */
#include "imgui.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

// 日志函数
extern "C" void alog_public(const char *fmt, ...);

// 主程序提供的 PNG 加载接口
struct ImTextureData;
extern "C" ImTextureData* imrun_load_png(const char *path);
extern "C" void imrun_register_texture(ImTextureData *tex);  // 注册（上传由渲染循环做，下帧读 GetTexID）

// 游戏状态
enum GameState { STATE_MENU, STATE_PLAYING, STATE_GAMEOVER };
static GameState game_state = STATE_MENU;

// 小鸟状态
static float bird_y = 240.0f;
static float bird_velocity = 0.0f;
static float bird_rotation = 0.0f;
static int bird_frame = 0;

// 管道
struct Pipe {
    float x;
    float gap_y;  // 缺口中心 Y 坐标
    bool scored;
};
static Pipe pipes[3];
static int pipe_count = 0;

// 游戏参数
static const float GRAVITY = 1200.0f;
static const float JUMP_VELOCITY = -400.0f;
static const float BIRD_X = 100.0f;
static const float PIPE_SPEED = 120.0f;
static const float PIPE_GAP = 140.0f;
static const float PIPE_WIDTH = 52.0f;
static const float BIRD_SIZE = 24.0f;

static int score = 0;
static int high_score = 0;
static float game_time = 0.0f;

// 纹理：保存 ImTextureData 句柄，渲染循环上传后再读 GetTexID() 得到 GL id
static ImTextureData *td_bird[3] = {nullptr};
static ImTextureData *td_pipe = nullptr;
static ImTextureData *td_background = nullptr;
static ImTextureData *td_base = nullptr;
static unsigned gl_bird[3] = {0};
static unsigned gl_pipe = 0;
static unsigned gl_background = 0;
static unsigned gl_base = 0;
static bool textures_loaded = false;

// 碰撞检测
static bool check_collision(float bx, float by, float bw, float bh, 
                            float px, float py, float pw, float ph) {
    return bx < px + pw && bx + bw > px && by < py + ph && by + bh > py;
}

extern "C" int imapp_init(void) {
    // 初始化游戏状态（纹理在第一帧加载）
    srand(time(nullptr));
    bird_y = 240.0f;
    bird_velocity = 0.0f;
    score = 0;
    
    // 初始化管道
    for (int i = 0; i < 3; i++) {
        pipes[i].x = 800.0f + i * 250.0f;
        pipes[i].gap_y = 150.0f + (rand() % 150);
        pipes[i].scored = false;
    }
    pipe_count = 3;
    
    return 0;
}

extern "C" void imapp_destroy(void) {
    // 纹理由主程序管理，插件不需要手动释放
    alog_public("FLAPPY v7: 销毁");
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

// 绘制纹理
// 绘制纹理（简化版：直接使用 TexID）
static void draw_texture(ImDrawList *dl, unsigned tex_id, ImVec2 pos, ImVec2 size, 
                         float rotation = 0.0f, bool flip_v = false) {
    if (tex_id == 0) return;
    
    ImVec2 uv0 = flip_v ? ImVec2(0, 1) : ImVec2(0, 0);
    ImVec2 uv1 = flip_v ? ImVec2(1, 0) : ImVec2(1, 1);
    
    /* GL ids are full 32-bit: cast unsigned -> ImTextureID (u64) must zero-extend.
       Going through intptr_t (signed 32-bit) sign-extends ids >= 0x80000000. */
    dl->AddImage((ImTextureID)tex_id, pos, 
                 ImVec2(pos.x + size.x, pos.y + size.y), uv0, uv1);
}

extern "C" void imapp_frame(double t) {
    static double last_t = t;
    float dt = (float)(t - last_t);
    last_t = t;
    if (dt > 0.1f) dt = 0.016f;  // 限制最大 dt
    
    game_time = (float)t;
    
    // 第一帧：加载并注册纹理（上传由渲染循环完成）
    if (!textures_loaded && !td_bird[0]) {
        const char* asset_base = "/home/user/imrun/apps/flappy/assets";
        char path[256];

        snprintf(path, sizeof(path), "%s/bluebird-midflap.png", asset_base);
        td_bird[0] = imrun_load_png(path);
        if (td_bird[0]) imrun_register_texture(td_bird[0]);

        snprintf(path, sizeof(path), "%s/bluebird-upflap.png", asset_base);
        td_bird[1] = imrun_load_png(path);
        if (td_bird[1]) imrun_register_texture(td_bird[1]);

        snprintf(path, sizeof(path), "%s/bluebird-downflap.png", asset_base);
        td_bird[2] = imrun_load_png(path);
        if (td_bird[2]) imrun_register_texture(td_bird[2]);

        snprintf(path, sizeof(path), "%s/pipe-green.png", asset_base);
        td_pipe = imrun_load_png(path);
        if (td_pipe) imrun_register_texture(td_pipe);

        snprintf(path, sizeof(path), "%s/background-day.png", asset_base);
        td_background = imrun_load_png(path);
        if (td_background) imrun_register_texture(td_background);

        snprintf(path, sizeof(path), "%s/base.png", asset_base);
        td_base = imrun_load_png(path);
        if (td_base) imrun_register_texture(td_base);

        alog_public("FLAPPY v7: 纹理已注册 (等待上传)");
    }

    // 之后每帧：渲染循环上传完，读回调 TexID（符号扩展已修，安全）
    if (!textures_loaded && td_bird[0] && td_pipe && td_background && td_base) {
        gl_bird[0] = (unsigned)td_bird[0]->GetTexID();
        gl_bird[1] = (unsigned)td_bird[1]->GetTexID();
        gl_bird[2] = (unsigned)td_bird[2]->GetTexID();
        gl_pipe = (unsigned)td_pipe->GetTexID();
        gl_background = (unsigned)td_background->GetTexID();
        gl_base = (unsigned)td_base->GetTexID();

        if (gl_bird[0] && gl_bird[1] && gl_bird[2] && gl_pipe && gl_background && gl_base) {
            textures_loaded = true;
            alog_public("FLAPPY v7: 纹理上传完成 bird=%u pipe=%u bg=%u base=%u",
                        gl_bird[0], gl_pipe, gl_background, gl_base);
        }
    }
    
    /* The host (shell_begin_dyn) already opened OUR window and will End() it
       after this call. Plugins must NEVER call ImGui::Begin/End themselves -
       doing so makes a rogue second window and leaves the host window empty. */
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();
    float frame_h = ImGui::GetFrameHeight();
    float title_h = ImGui::GetTextLineHeightWithSpacing();
    float top_pad = (frame_h > title_h ? frame_h : title_h);
    ImVec2 origin = ImVec2(wp.x, wp.y + top_pad);
    ImVec2 area = ImVec2(ws.x, ws.y - top_pad);
    if (area.x < 10 || area.y < 10) return;   /* not laid out yet */

    /* Game logic runs in a virtual 800x480 field; CR_W/CR_H scale it onto the
       window's inner rect so the bird/pipes/ground always fit inside. */
    const float CR_W = 800.0f, CR_H = 480.0f;
    float sx = area.x / CR_W, sy = area.y / CR_H;
    #define GX(v) (origin.x + (v) * sx)
    #define GY(v) (origin.y + (v) * sy)

    // 绘制背景（铺满内容区）；纹理未就绪时用纯色降级
    if (gl_background) {
        draw_texture(dl, gl_background, origin, area);
    } else {
        dl->AddRectFilled(origin, ImVec2(origin.x + area.x, origin.y + area.y),
                          IM_COL32(113, 197, 207, 255));
    }

    // 游戏逻辑
    if (game_state == STATE_PLAYING) {
        // 重力
        bird_velocity += GRAVITY * dt;
        bird_y += bird_velocity * dt;

        // 旋转角度
        bird_rotation = bird_velocity * 0.05f;
        if (bird_rotation > 25.0f) bird_rotation = 25.0f;
        if (bird_rotation < -25.0f) bird_rotation = -25.0f;

        // 移动管道
        for (int i = 0; i < pipe_count; i++) {
            pipes[i].x -= PIPE_SPEED * dt;

            // 计分
            if (!pipes[i].scored && pipes[i].x + PIPE_WIDTH < BIRD_X) {
                pipes[i].scored = true;
                score++;
                if (score > high_score) high_score = score;
            }

            // 重置管道
            if (pipes[i].x < -PIPE_WIDTH) {
                pipes[i].x = CR_W;
                pipes[i].gap_y = 150.0f + (rand() % 150);
                pipes[i].scored = false;
            }
        }

        // 碰撞检测
        if (bird_y < 0 || bird_y > 400) {
            game_state = STATE_GAMEOVER;
        }

        for (int i = 0; i < pipe_count; i++) {
            float pipe_top_h = pipes[i].gap_y - PIPE_GAP / 2;
            float pipe_bot_y = pipes[i].gap_y + PIPE_GAP / 2;

            if (check_collision(BIRD_X, bird_y, BIRD_SIZE, BIRD_SIZE,
                               pipes[i].x, 0, PIPE_WIDTH, pipe_top_h) ||
                check_collision(BIRD_X, bird_y, BIRD_SIZE, BIRD_SIZE,
                               pipes[i].x, pipe_bot_y, PIPE_WIDTH, CR_H - pipe_bot_y)) {
                game_state = STATE_GAMEOVER;
            }
        }

        // 小鸟动画
        bird_frame = (int)(game_time * 10) % 3;
    }

    // 绘制管道
    for (int i = 0; i < pipe_count; i++) {
        float pipe_top_h = pipes[i].gap_y - PIPE_GAP / 2;
        float pipe_bot_y = pipes[i].gap_y + PIPE_GAP / 2;
        float px = GX(pipes[i].x);

        if (gl_pipe) {
            // 上管道（从顶部到下沿，纹理需翻转）
            draw_texture(dl, gl_pipe, ImVec2(px, GY(0.0f)),
                         ImVec2(PIPE_WIDTH * sx, pipe_top_h * sy), 0.0f, true);
            // 下管道
            draw_texture(dl, gl_pipe, ImVec2(px, GY(pipe_bot_y)),
                         ImVec2(PIPE_WIDTH * sx, (CR_H - pipe_bot_y) * sy));
        } else {
            dl->AddRectFilled(ImVec2(px, GY(0.0f)),
                              ImVec2(px + PIPE_WIDTH * sx, GY(pipe_top_h)),
                              IM_COL32(94, 201, 82, 255));
            dl->AddRectFilled(ImVec2(px, GY(pipe_bot_y)),
                              ImVec2(px + PIPE_WIDTH * sx, GY(CR_H)),
                              IM_COL32(94, 201, 82, 255));
        }
    }

    // 绘制地面（底部一条）
    {
        float base_y = CR_H - 80.0f;
        if (gl_base) {
            draw_texture(dl, gl_base, ImVec2(origin.x, GY(base_y)),
                         ImVec2(area.x, area.y - GY(base_y) + origin.y));
        } else {
            dl->AddRectFilled(ImVec2(origin.x, GY(base_y)),
                              ImVec2(origin.x + area.x, origin.y + area.y),
                              IM_COL32(222, 216, 149, 255));
        }
    }

    // 绘制小鸟
    {
        float bx = GX(BIRD_X), by = GY(bird_y);
        float bs = BIRD_SIZE * sx;
        if (gl_bird[bird_frame]) {
            draw_texture(dl, gl_bird[bird_frame], ImVec2(bx, by), ImVec2(bs, bs));
        } else {
            dl->AddCircleFilled(ImVec2(bx + bs / 2, by + bs / 2), bs / 2,
                                IM_COL32(255, 219, 77, 255));
        }
    }

    // UI：Score 用窗口内光标（相对窗口，安全）
    ImGui::SetCursorPos(ImVec2(10, top_pad + 6));
    ImGui::Text("Score: %d  High: %d", score, high_score);

    if (!textures_loaded) {
        ImGui::SetCursorPos(ImVec2(10, top_pad + 30));
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "纹理加载失败");
    }

    // 状态提示（居中）；用 draw list 画，与游戏内容同层
    {
        const char *hint = nullptr;
        ImU32 col = IM_COL32(255, 255, 255, 255);
        float fs = ImGui::GetFontSize();
        if (game_state == STATE_MENU) {
            hint = "点击开始 / 空格";
            col = IM_COL32(255, 255, 255, 255);
        } else if (game_state == STATE_GAMEOVER) {
            hint = "Game Over — 点击重来";
            col = IM_COL32(255, 90, 90, 255);
        }
        if (hint) {
            float tw = ImGui::CalcTextSize(hint).x;
            ImVec2 tp = ImVec2(origin.x + (area.x - tw) * 0.5f,
                               origin.y + area.y * 0.5f - fs * 0.5f);
            // 半透明底衬，保证任何背景下都看得清
            dl->AddRectFilled(ImVec2(tp.x - 10, tp.y - 6),
                              ImVec2(tp.x + tw + 10, tp.y + fs + 6),
                              IM_COL32(0, 0, 0, 140), 6.0f);
            dl->AddText(tp, col, hint);
        }
    }

    /* ---- 输入：触摸/鼠标 + 空格 ----
       N900 触摸屏的点击会转成鼠标左键事件。用 IsWindowHovered() 限定
       只有在本窗口内容区内点击才生效（点标题栏/桌面/任务栏不误触发），
       这是早期版本验证过的做法。 */
    bool tapped = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
                  ImGui::IsMouseClicked(0);
    bool keyed = ImGui::IsKeyPressed(ImGuiKey_Space) ||
                 ImGui::IsKeyPressed(ImGuiKey_UpArrow) ||
                 ImGui::IsKeyPressed(ImGuiKey_Enter);
    bool action = tapped || keyed;

    if (game_state == STATE_MENU) {
        if (action) {
            game_state = STATE_PLAYING;
            bird_y = 240.0f;
            bird_velocity = JUMP_VELOCITY;
            score = 0;
        }
    } else if (game_state == STATE_PLAYING) {
        if (action) bird_velocity = JUMP_VELOCITY;
    } else if (game_state == STATE_GAMEOVER) {
        if (action) {
            game_state = STATE_PLAYING;
            bird_y = 240.0f;
            bird_velocity = JUMP_VELOCITY;
            score = 0;
            for (int i = 0; i < pipe_count; i++) {
                pipes[i].x = CR_W + i * 250.0f;
                pipes[i].gap_y = 150.0f + (rand() % 150);
                pipes[i].scored = false;
            }
        }
    }

    #undef GX
    #undef GY
    /* NO ImGui::End() - the host owns this window */
}

extern "C" const char* imapp_name(void) {
    return "Flappy";
}

/* 竖屏游戏：窄窗口。宿主读取此尺寸作为默认窗口大小。
   桌面可用高度 = 480 - 48(系统栏) = 432，留一点边距用 420。 */
extern "C" void imapp_pref_size(float *w, float *h) {
    *w = 400.0f;
    *h = 420.0f;
}
