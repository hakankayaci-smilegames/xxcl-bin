#include "video.hpp"
#include <SDL2/SDL.h>
#include <iostream>

namespace xbe {

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;
static SDL_Texture* texture = nullptr;
static bool running = false;
static uint32_t* framebuffer = nullptr;

bool init_video(const std::string& title) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "[VIDEO] SDL_Init basarisiz: " << SDL_GetError() << "\n";
        return false;
    }

    std::string win_title = "xbe-layer: " + title;
    window = SDL_CreateWindow(win_title.c_str(), 
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                              640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "[VIDEO] SDL_CreateWindow basarisiz: " << SDL_GetError() << "\n";
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "[VIDEO] SDL_CreateRenderer basarisiz: " << SDL_GetError() << "\n";
        return false;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, 
                                SDL_TEXTUREACCESS_STREAMING, 640, 480);
    if (!texture) {
        std::cerr << "[VIDEO] SDL_CreateTexture basarisiz: " << SDL_GetError() << "\n";
        return false;
    }
    
    framebuffer = new uint32_t[640 * 480];

    // Retro test deseni: Koyu Xbox yesili arka plan
    for (int i = 0; i < 640 * 480; ++i) {
        framebuffer[i] = 0xFF082008; 
    }

    // Ortaya parlak bir test karesi (Xbox yesili)
    for (int y = 200; y < 280; ++y) {
        for (int x = 280; x < 360; ++x) {
            framebuffer[y * 640 + x] = 0xFF00FF00;
        }
    }
    
    running = true;
    std::cout << "[VIDEO] SDL2 Sanal Ekrani basariyla olusturuldu (640x480).\n";
    return true;
}

void run_video_loop() {
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
            }
        }

        SDL_UpdateTexture(texture, nullptr, framebuffer, 640 * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
        
        SDL_Delay(16); // ~60 FPS cap
    }
}

void cleanup_video() {
    if (framebuffer) {
        delete[] framebuffer;
        framebuffer = nullptr;
    }
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    std::cout << "[VIDEO] SDL2 sistemi temizlendi.\n";
}

} // namespace xbe
