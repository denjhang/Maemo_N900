/* IMRUN plugin: Flappy Bird v3.1 (Safe)
 * 经典像素鸟游戏：点击屏幕让小鸟飞起，躲避管道
 * v3.1: 只改进图形（阴影、渐变、3D效果），暂时去掉音效避免崩溃
 */
#include "imgui.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

static float bird_y = 240.0f;
static float bird_vy = 0.0f;
static float scroll_x = 0.0f;
static int score = 0;
static int high_score = 0;
static bool game_over = false;
static bool started = false;
static float wing_anim = 0.0f;
static bool window_open = true;

struct Pipe {
    float x;
    float gap_y;
    bool scored;
};

static Pipe pipes[3];
static int pipe_count = 0;

#define BIRD_X 100.0f
#define BIRD_SIZE 28.0f
#define PIPE_WIDTH 70.0f
#define PIPE_GAP 150.0f
#define PIPE_SPACING 320.0f
#define GRAVITY 0.65f
#define JUMP_VELOCITY -11.0f
#define SCROLL_SPEED 3.5f

extern "C" const char *imapp_name(void) { return "FLAPPY"; }

extern "C" int imapp_init(void) {
    srand((unsigned)time(NULL));
    bird_y = 240.0f;
    bird_vy = 0.0f;
    wing_anim = 0.0f;
    scroll_x = 0.0f;
    score = 0;
    game_over = false;
    started = false;
    pipe_count = 0;
    window_open = true;
    
    for (int i = 0; i < 3; i++) {
        pipes[i].x = 800.0f + i * PIPE_SPACING;
        pipes[i].gap_y = 150.0f + (rand() % 180);
        pipes[i].scored = false;
    }
    pipe_count = 3;
    return 0;
}

extern "C" void imapp_icon(void *dlv, float cx, float cy, float s) {
    ImDrawList *dl = (ImDrawList *)dlv;
    // 改进的小鸟图标
    dl->AddCircleFilled(ImVec2(cx + 2, cy + 2), s * 0.6f, IM_COL32(0, 0, 0, 80));
    dl->AddCircleFilled(ImVec2(cx, cy), s * 0.6f, IM_COL32(255, 220, 0, 255));
    dl->AddCircleFilled(ImVec2(cx - s * 0.15f, cy + s * 0.1f), s * 0.35f, IM_COL32(255, 240, 100, 255));
    dl->AddCircleFilled(ImVec2(cx + s * 0.25f, cy - s * 0.15f), s * 0.18f, IM_COL32(255, 255, 255, 255));
    dl->AddCircleFilled(ImVec2(cx + s * 0.3f, cy - s * 0.1f), s * 0.12f, IM_COL32(0, 0, 0, 255));
    dl->AddTriangleFilled(
        ImVec2(cx + s * 0.5f, cy),
        ImVec2(cx + s * 0.75f, cy - s * 0.12f),
        ImVec2(cx + s * 0.75f, cy + s * 0.12f),
        IM_COL32(255, 140, 0, 255)
    );
}

extern "C" void imapp_frame(double t) {
    if (!window_open) return;
    
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    
    if (!ImGui::Begin("FLAPPY v3.1", &window_open, ImGuiWindowFlags_NoScrollbar)) {
        ImGui::End();
        return;
    }
    
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 canvas_p = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    
    if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
    if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
    
    // 天空渐变背景
    ImU32 sky_top = IM_COL32(115, 186, 235, 255);
    ImU32 sky_bottom = IM_COL32(180, 220, 250, 255);
    dl->AddRectFilledMultiColor(canvas_p, ImVec2(canvas_p.x + canvas_sz.x, canvas_p.y + canvas_sz.y),
                                 sky_top, sky_top, sky_bottom, sky_bottom);
    
    // 云朵装饰
    for (int i = 0; i < 3; i++) {
        float cx = fmod(scroll_x * 0.3f + i * 250.0f, canvas_sz.x + 100.0f) - 50.0f;
        float cy = 50.0f + i * 40.0f;
        dl->AddCircleFilled(ImVec2(canvas_p.x + cx, canvas_p.y + cy), 25.0f, IM_COL32(255, 255, 255, 180));
        dl->AddCircleFilled(ImVec2(canvas_p.x + cx + 20, canvas_p.y + cy - 5), 20.0f, IM_COL32(255, 255, 255, 180));
        dl->AddCircleFilled(ImVec2(canvas_p.x + cx + 35, canvas_p.y + cy), 22.0f, IM_COL32(255, 255, 255, 180));
    }
    
    if (!game_over && started) {
        wing_anim += 0.3f;
    }
    
    // 游戏逻辑
    if (!game_over && started) {
        bird_vy += GRAVITY;
        bird_y += bird_vy;
        scroll_x += SCROLL_SPEED;
        
        for (int i = 0; i < pipe_count; i++) {
            pipes[i].x -= SCROLL_SPEED;
            
            if (pipes[i].x + PIPE_WIDTH < 0) {
                pipes[i].x = pipes[(i + pipe_count - 1) % pipe_count].x + PIPE_SPACING;
                pipes[i].gap_y = 150.0f + (rand() % 180);
                pipes[i].scored = false;
            }
            
            if (!pipes[i].scored && pipes[i].x + PIPE_WIDTH < BIRD_X) {
                pipes[i].scored = true;
                score++;
                if (score > high_score) high_score = score;
            }
            
            float pipe_left = pipes[i].x;
            float pipe_right = pipes[i].x + PIPE_WIDTH;
            float bird_left = BIRD_X - BIRD_SIZE / 2;
            float bird_right = BIRD_X + BIRD_SIZE / 2;
            float bird_top = bird_y - BIRD_SIZE / 2;
            float bird_bottom = bird_y + BIRD_SIZE / 2;
            
            if (bird_right > pipe_left && bird_left < pipe_right) {
                float gap_top = pipes[i].gap_y - PIPE_GAP / 2;
                float gap_bottom = pipes[i].gap_y + PIPE_GAP / 2;
                
                if (bird_top < gap_top || bird_bottom > gap_bottom) {
                    game_over = true;
                }
            }
        }
        
        if (bird_y - BIRD_SIZE / 2 < 0 || bird_y + BIRD_SIZE / 2 > canvas_sz.y) {
            game_over = true;
        }
    }
    
    // 绘制管道（3D效果）
    for (int i = 0; i < pipe_count; i++) {
        float x = canvas_p.x + pipes[i].x;
        float gap_top = pipes[i].gap_y - PIPE_GAP / 2;
        float gap_bottom = pipes[i].gap_y + PIPE_GAP / 2;
        
        // 阴影
        dl->AddRectFilled(ImVec2(x + 3, canvas_p.y + 3),
                         ImVec2(x + PIPE_WIDTH + 3, canvas_p.y + gap_top + 3),
                         IM_COL32(0, 0, 0, 60));
        dl->AddRectFilled(ImVec2(x + 3, canvas_p.y + gap_bottom + 3),
                         ImVec2(x + PIPE_WIDTH + 3, canvas_p.y + canvas_sz.y + 3),
                         IM_COL32(0, 0, 0, 60));
        
        // 上管道
        dl->AddRectFilled(ImVec2(x, canvas_p.y),
                         ImVec2(x + PIPE_WIDTH, canvas_p.y + gap_top),
                         IM_COL32(60, 180, 60, 255));
        dl->AddRectFilled(ImVec2(x + 5, canvas_p.y),
                         ImVec2(x + 15, canvas_p.y + gap_top),
                         IM_COL32(90, 210, 90, 180));
        dl->AddRect(ImVec2(x, canvas_p.y),
                   ImVec2(x + PIPE_WIDTH, canvas_p.y + gap_top),
                   IM_COL32(30, 130, 30, 255), 0.0f, 0, 2.0f);
        dl->AddRectFilled(ImVec2(x - 5, canvas_p.y + gap_top - 20),
                         ImVec2(x + PIPE_WIDTH + 5, canvas_p.y + gap_top),
                         IM_COL32(50, 170, 50, 255));
        
        // 下管道
        dl->AddRectFilled(ImVec2(x, canvas_p.y + gap_bottom),
                         ImVec2(x + PIPE_WIDTH, canvas_p.y + canvas_sz.y),
                         IM_COL32(60, 180, 60, 255));
        dl->AddRectFilled(ImVec2(x + 5, canvas_p.y + gap_bottom),
                         ImVec2(x + 15, canvas_p.y + canvas_sz.y),
                         IM_COL32(90, 210, 90, 180));
        dl->AddRect(ImVec2(x, canvas_p.y + gap_bottom),
                   ImVec2(x + PIPE_WIDTH, canvas_p.y + canvas_sz.y),
                   IM_COL32(30, 130, 30, 255), 0.0f, 0, 2.0f);
        dl->AddRectFilled(ImVec2(x - 5, canvas_p.y + gap_bottom),
                         ImVec2(x + PIPE_WIDTH + 5, canvas_p.y + gap_bottom + 20),
                         IM_COL32(50, 170, 50, 255));
    }
    
    // 绘制小鸟（改进版）
    ImVec2 bird_center(canvas_p.x + BIRD_X, canvas_p.y + bird_y);
    
    dl->AddCircleFilled(ImVec2(bird_center.x + 3, bird_center.y + 3), BIRD_SIZE / 2, IM_COL32(0, 0, 0, 100));
    dl->AddCircleFilled(bird_center, BIRD_SIZE / 2, IM_COL32(255, 220, 0, 255));
    dl->AddCircleFilled(ImVec2(bird_center.x - 4, bird_center.y + 3), BIRD_SIZE / 3, IM_COL32(255, 245, 150, 255));
    
    // 翅膀动画
    float wing_offset = sin(wing_anim) * 8.0f;
    ImVec2 wing_tip(bird_center.x - 8, bird_center.y + wing_offset);
    dl->AddTriangleFilled(bird_center, wing_tip, ImVec2(wing_tip.x, bird_center.y + 10), IM_COL32(220, 180, 0, 255));
    
    dl->AddCircleFilled(ImVec2(bird_center.x + 8, bird_center.y - 4), 5, IM_COL32(255, 255, 255, 255));
    dl->AddCircleFilled(ImVec2(bird_center.x + 10, bird_center.y - 3), 3, IM_COL32(0, 0, 0, 255));
    
    ImVec2 beak1(bird_center.x + 12, bird_center.y + 2);
    ImVec2 beak2(bird_center.x + 20, bird_center.y - 1);
    ImVec2 beak3(bird_center.x + 20, bird_center.y + 5);
    dl->AddTriangleFilled(beak1, beak2, beak3, IM_COL32(255, 140, 0, 255));
    
    // 显示分数
    ImGui::SetCursorScreenPos(ImVec2(canvas_p.x + 10, canvas_p.y + 10));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    ImGui::Text("Score: %d", score);
    if (high_score > 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), " | Best: %d", high_score);
    }
    ImGui::PopStyleColor();
    
    // 游戏状态
    if (!started) {
        ImGui::SetCursorScreenPos(ImVec2(canvas_p.x + canvas_sz.x / 2 - 100, canvas_p.y + canvas_sz.y / 2 - 50));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        ImGui::Text("  FLAPPY v3.1 SAFE");
        ImGui::SetCursorScreenPos(ImVec2(canvas_p.x + canvas_sz.x / 2 - 80, canvas_p.y + canvas_sz.y / 2 - 20));
        ImGui::Text("  Click to Start!");
        ImGui::PopStyleColor();
        
        if (ImGui::IsMouseClicked(0)) {
            started = true;
            bird_vy = JUMP_VELOCITY;
        }
    }
    else if (!game_over) {
        if (ImGui::IsMouseClicked(0)) {
            bird_vy = JUMP_VELOCITY;
        }
    }
    else {
        ImGui::SetCursorScreenPos(ImVec2(canvas_p.x + canvas_sz.x / 2 - 70, canvas_p.y + canvas_sz.y / 2 - 50));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.2f, 0.2f, 1));
        ImGui::Text("  GAME OVER!");
        ImGui::PopStyleColor();
        
        ImGui::SetCursorScreenPos(ImVec2(canvas_p.x + canvas_sz.x / 2 - 60, canvas_p.y + canvas_sz.y / 2 - 10));
        ImGui::TextColored(ImVec4(1, 1, 1, 1), "Score: %d", score);
        
        ImGui::SetCursorScreenPos(ImVec2(canvas_p.x + canvas_sz.x / 2 - 80, canvas_p.y + canvas_sz.y / 2 + 20));
        ImGui::TextColored(ImVec4(1, 1, 1, 1), "Click to Restart");
        
        if (ImGui::IsMouseClicked(0)) {
            imapp_init();
            started = true;
            bird_vy = JUMP_VELOCITY;
        }
    }
    
    ImGui::End();
}
