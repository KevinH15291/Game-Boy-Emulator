#pragma once

#ifdef __EMSCRIPTEN__
#include <SDL2/SDL.h>
#else
#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#endif

#include <cstddef>
#include <cstdint>

#include "bus.h"

namespace GBC {
constexpr size_t WINDOW_WIDTH = 160;
constexpr size_t WINDOW_HEIGHT = 144;

class PPU {
   public:
    PPU(const PPU &) = delete;
    PPU(PPU &&) = delete;
    PPU &operator=(const PPU &) = delete;
    PPU &operator=(PPU &&) = delete;
    PPU(address_bus *bus);
    ~PPU();

    void execute_cycle();
    void draw_pixel();
    void dump_info();
    void dump_vram();

    void init_window();
    void init_canvas();

    void init_debug_window();
    void render_debug();

   private:
    struct obj {
        obj() = default;
        obj(byte objx, byte objy, byte index, byte flags)
            : objx(objx), objy(objy), index(index), flags(flags) {}
        obj(const obj &) = default;
        obj(obj &&) = default;
        obj &operator=(const obj &) = default;
        obj &operator=(obj &&) = default;
        ~obj() = default;

        byte objx, objy, index, flags;
    };

    address_bus *bus;
    RenderingState mode = RenderingState::hblank;

    byte objFIFO();
    byte bgFIFO(half tilex, half tiley);
    byte windowFIFO(half tilex, half tiley);

    SDL_Event event{};
    SDL_Renderer *renderer{};
    SDL_Window *window{};

    std::array<obj, 10> objbuffer{};
    std::array<byte, WINDOW_WIDTH * WINDOW_HEIGHT> frame{};

    uint32_t dots = 0;
    uint32_t lines = 0;
    uint32_t renderX = 0;
    uint32_t wly = 0;

    bool debug_render = false;
    bool debug_callback = false;
    bool wlyenabled = false;

#ifdef _DEBUG
    SDL_Window *debug_tile_window, *debug_window_window, *debug_object_window,
        *debug_bg_window;
    SDL_Renderer *debug_tile_renderer, *debug_window_renderer,
        *debug_object_renderer, *debug_bg_renderer;
#endif
    uint8_t objnum = 0;

    friend class GBC;
};

}  // namespace GBC