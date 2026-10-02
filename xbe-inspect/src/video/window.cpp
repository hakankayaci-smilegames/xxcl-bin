#include "window.hpp"
#include <SDL2/SDL.h>
#include <cstdio>

namespace xbe {
namespace video {

static SDL_Window* g_window = nullptr;
static SDL_Renderer* g_renderer = nullptr;

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
    SDL_SetRenderDrawColor(g_renderer, 10, 10, 10, 255); // Koyu gri / siyah
    SDL_RenderClear(g_renderer);
    SDL_RenderPresent(g_renderer);

    printf("[VIDEO] *** TEBRIKLER! HOST PENCERESI (640x480) BASARIYLA ACILDI! ***\n");
    return true;
}

void video_poll_events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            printf("[VIDEO] Pencere kapatildi.\n");
            exit(0);
        }
    }
}

} // namespace video
} // namespace xbe
