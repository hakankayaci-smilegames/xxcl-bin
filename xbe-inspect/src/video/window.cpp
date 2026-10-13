#include "window.hpp"
#include "../kernel/apu.hpp"
#include "../kernel/nv2a.hpp"
#include <SDL2/SDL.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <string_view>

namespace xbe {
namespace video {

static SDL_Window* g_window = nullptr;
static SDL_Renderer* g_renderer = nullptr;
static auto g_last_frame = std::chrono::steady_clock::now();
static auto g_fps_sample_start = g_last_frame;
static uint32_t g_frames_in_sample = 0;
static float g_host_fps = 0.0f;
static float g_frame_time_ms = 0.0f;

namespace {

struct Glyph {
    char character;
    std::array<uint8_t, 7> rows;
};

constexpr std::array<Glyph, 39> glyphs = {{
    {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {15, 16, 16, 16, 16, 16, 15}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {15, 16, 16, 19, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {14, 4, 4, 4, 4, 4, 14}}, {'J', {7, 2, 2, 2, 2, 18, 12}},
    {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}}, {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}}, {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}}, {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}}, {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 1, 14}},
    {'.', {0, 0, 0, 0, 0, 4, 4}}, {'/', {1, 2, 2, 4, 8, 8, 16}},
    {':', {0, 4, 4, 0, 4, 4, 0}},
}};

void draw_text(int x, int y, std::string_view text, SDL_Color color) {
    constexpr int scale = 2;
    std::array<SDL_Rect, 768> pixels{};
    int pixel_count = 0;
    for (char character : text) {
        const Glyph* glyph = nullptr;
        for (const auto& candidate : glyphs) {
            if (candidate.character == character) {
                glyph = &candidate;
                break;
            }
        }
        if (glyph) {
            for (int row = 0; row < 7; ++row) {
                for (int column = 0; column < 5; ++column) {
                    if ((glyph->rows[row] & (1u << (4 - column))) == 0) continue;
                    pixels[pixel_count++] = {
                        x + column * scale, y + row * scale, scale, scale
                    };
                }
            }
        }
        x += 12;
    }
    SDL_SetRenderDrawColor(g_renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRects(g_renderer, pixels.data(), pixel_count);
}

void render_overlay() {
    if (!g_renderer) return;

    const auto now = std::chrono::steady_clock::now();
    g_frame_time_ms = std::chrono::duration<float, std::milli>(now - g_last_frame).count();
    g_last_frame = now;
    ++g_frames_in_sample;
    const auto sample_time = std::chrono::duration<float>(now - g_fps_sample_start).count();
    if (sample_time >= 1.0f) {
        g_host_fps = static_cast<float>(g_frames_in_sample) / sample_time;
        g_frames_in_sample = 0;
        g_fps_sample_start = now;
    }

    const auto dsp = xbe::kernel::apu_gp_dsp_debug_stats();
    const auto gpu = xbe::kernel::nv2a_debug_stats();
    char host_line[64];
    char dsp_line[64];
    char gpu_line[64];
    std::snprintf(host_line, sizeof(host_line), "HOST FPS %5.1f MS %5.1f",
                  g_host_fps, g_frame_time_ms);
    std::snprintf(dsp_line, sizeof(dsp_line), "DSP %s PC %04X INS %06llu",
                  dsp.worker_running ? "RUN" : "STOP",
                  dsp.program_counter,
                  static_cast<unsigned long long>(dsp.instructions_executed));
    std::snprintf(gpu_line, sizeof(gpu_line), "GPU %s GET %06X PUT %06X",
                  gpu.worker_running ? "RUN" : "OFF",
                  gpu.fifo_get, gpu.fifo_put);

    SDL_SetRenderDrawColor(g_renderer, 8, 14, 20, 255);
    SDL_RenderClear(g_renderer);
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 8, 18, 26, 232);
    SDL_Rect panel{16, 16, 384, 104};
    SDL_RenderFillRect(g_renderer, &panel);
    SDL_SetRenderDrawColor(g_renderer, 48, 104, 126, 255);
    SDL_RenderDrawRect(g_renderer, &panel);

    draw_text(28, 26, "XXCL MONITOR", {100, 220, 196, 255});
    const SDL_Color text_color{218, 230, 234, 255};
    draw_text(28, 46, host_line, text_color);
    draw_text(28, 66, dsp_line, text_color);
    draw_text(28, 86, gpu_line, text_color);
    SDL_RenderPresent(g_renderer);
}

} // namespace

bool video_init_window(int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("[VIDEO] SDL_Init hatasi: %s\n", SDL_GetError());
        return false;
    }

    g_window = SDL_CreateWindow(
        "XXCL - Sega GT 2002 [Running on Linux]",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_SHOWN
    );

    if (!g_window) {
        printf("[VIDEO] Pencere olusturulamadi: %s\n", SDL_GetError());
        return false;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_renderer) {
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!g_renderer) {
        printf("[VIDEO] SDL renderer olusturulamadi: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window);
        g_window = nullptr;
        SDL_Quit();
        return false;
    }
    SDL_SetRenderDrawColor(g_renderer, 10, 10, 10, 255); // Koyu gri / siyah
    g_last_frame = std::chrono::steady_clock::now();
    g_fps_sample_start = g_last_frame;

    printf("[VIDEO] *** TEBRIKLER! HOST PENCERESI (640x480) BASARIYLA ACILDI! ***\n");
    return true;
}

void video_poll_events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            xbe::kernel::apu_stop_worker();
            const auto stats = xbe::kernel::apu_gp_dsp_debug_stats();
            std::printf("[DEBUG] DSP instructions=%llu PC=0x%03X\n",
                        static_cast<unsigned long long>(stats.instructions_executed),
                        stats.program_counter);
            std::printf("[DEBUG] P[PC..PC+2]=%06X/%06X/%06X\n",
                        stats.program_words[0], stats.program_words[1],
                        stats.program_words[2]);
            const auto gpu = xbe::kernel::nv2a_debug_stats();
            std::printf("[DEBUG] GPU FIFO GET=%06X PUT=%06X\n",
                        gpu.fifo_get, gpu.fifo_put);
            printf("[VIDEO] Pencere kapatildi.\n");
            exit(0);
        }
    }
    render_overlay();
}

} // namespace video
} // namespace xbe
