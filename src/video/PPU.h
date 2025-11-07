#pragma once

#ifdef __EMSCRIPTEN__
#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#else
#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#endif

#include <bitset>
#include <cstdint>

#include "bus.h"
#include "enums.h"

namespace GBC {
constexpr uint32_t WINDOW_WIDTH = 160;
constexpr uint32_t WINDOW_HEIGHT = 144;

struct obj {
    obj() {}
    obj(byte objx, byte objy, byte index, byte flags)
        : objx(objx), objy(objy), index(index), flags(flags) {}
    byte objx, objy, index, flags;
};

class PPU {
   public:
    PPU(address_bus *bus) : bus(bus) {}
    ~PPU();

    RenderingState mode = RenderingState::hblank;
    address_bus *bus;
    int dots = 0, lines = 0, renderX = 0, wly = 0;
    bool wlyenabled = 0;

    SDL_Event event;
    SDL_Renderer *renderer, *debug_tile_renderer, *debug_window_renderer,
        *debug_object_renderer, *debug_bg_renderer;
    SDL_Window *window, *debug_tile_window, *debug_window_window,
        *debug_object_window, *debug_bg_window;
    SDL_Texture *texture = nullptr;

    byte frame[160 * 144];
    bool debug_render = 0, debug_callback = 0;

    void execute_cycle();
    void draw_pixel();
    void dump_info();
    void dump_vram();

    void init_window();

    void init_debug_window();
    void render_debug();
    void mark_cache_dirty() { cache_dirty = true; }

    uint8_t read_register(uint16_t address) const;
    void write_register(uint16_t address, uint8_t value);

   private:
    obj objbuffer[10];
    uint8_t objnum = 0;
    byte cached_LCDC = 0;
    byte cached_BGP = 0;
    byte cached_OBP0 = 0;
    byte cached_OBP1 = 0;
    byte cached_SCX = 0;
    byte cached_SCY = 0;
    byte cached_WX = 0;
    byte cached_WY = 0;
    byte cached_LYC = 0;
    bool cache_dirty = true;
    bool lyc_match = false;
    uint8_t palette_lut[4] = {255, 170, 85, 0};
    uint8_t obp_lut[2][4] = {};
    uint8_t bg_tile_row_cache[256][2];
    uint8_t window_tile_row_cache[256][2];
    uint16_t bg_tile_cache_key[256] = {};
    bool bg_tile_cache_valid[256] = {};
    uint16_t window_tile_cache_key[256] = {};
    bool window_tile_cache_valid[256] = {};

    byte reg_LCDC = 0;
    byte reg_STAT = 0;
    byte reg_SCX = 0;
    byte reg_SCY = 0;
    byte reg_WX = 0;
    byte reg_WY = 0;
    byte reg_BGP = 0;
    byte reg_OBP0 = 0;
    byte reg_OBP1 = 0;
    byte reg_LYC = 0;

    inline void update_register_cache();
    inline void update_palette_luts();
    inline void cache_bg_tile_row(uint8_t tile_index, uint8_t tiley_mod8);
    inline void cache_window_tile_row(uint8_t tile_index, uint8_t tiley_mod8);
    inline byte objFIFO();
    inline byte bgFIFO(half tilex, half tiley);
    inline byte windowFIFO(half tilex, half tiley);
};
}  // namespace GBC