#pragma once

#ifdef __EMSCRIPTEN__
#include <SDL.h>
#include <SDL_render.h>
#include <SDL_video.h>
#else
#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#endif

#include <array>
#include <bitset>
#include <cstdint>
#include <utility>

#include "../DebugMacros.h"
#include "bus.h"
#include "enums.h"

namespace GBC {
constexpr uint32_t WINDOW_WIDTH = 160;
constexpr uint32_t WINDOW_HEIGHT = 144;

class GBC;
struct obj {
    byte objx, objy, index, flags;
};

class PPU {
   public:
    PPU(address_bus *bus, CgbConfig &config) : bus(bus), config(config) {}
    ~PPU();

    void execute_cycle();
    void draw_pixel();
#if GBC_PPU_DEBUG && !defined(__EMSCRIPTEN__)
    void dump_info();
    void dump_vram();
#endif

    void init_window();
    inline void io_write(half address, byte value);

#if GBC_PPU_DEBUG && !defined(__EMSCRIPTEN__)
    void init_debug_window();
    void render_debug();
#endif
    void mark_cache_dirty() { cache_dirty = true; }

   private:
    friend class GBC;
    friend class address_bus;

    RenderingState mode = RenderingState::hblank;
    address_bus *bus;
    CgbConfig &config;
    int dots = 0, lines = 0, renderX = 0, wly = 0;
    bool wlyenabled = false;
    int oam_scan_dot = 0;
    size_t oam_scan_index = 0;

    SDL_Event event{};
    SDL_Renderer *renderer = nullptr;
    SDL_Window *window = nullptr;
#if GBC_PPU_DEBUG && !defined(__EMSCRIPTEN__)
    SDL_Renderer *debug_tile_renderer = nullptr;
    SDL_Renderer *debug_window_renderer = nullptr;
    SDL_Renderer *debug_object_renderer = nullptr;
    SDL_Renderer *debug_bg_renderer = nullptr;
    SDL_Window *debug_tile_window = nullptr;
    SDL_Window *debug_window_window = nullptr;
    SDL_Window *debug_object_window = nullptr;
    SDL_Window *debug_bg_window = nullptr;
#endif
    SDL_Texture *texture = nullptr;

    std::array<uint32_t, WINDOW_WIDTH * WINDOW_HEIGHT> frame_rgb{};
    std::array<obj, 10> objbuffer;
    byte objnum = 0;
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

    inline byte &io_reg(VideoRegister reg);
    void update_stat_register();

    inline void update_register_cache();
    void perform_oam_scan_step();
    void scan_oam_entry();
    struct BgPixel {
        byte color = 0;
        byte palette = 0;
        bool priority = false;
    };

    enum class TileMapKind : uint8_t { Background, Window };

    struct ObjPixel {
        byte color = 0;
        byte palette = 0;
        bool priority = false;
        bool has_pixel = false;
    };

    [[nodiscard]] std::pair<byte, byte> fetch_tile_row(
        byte tile_index, byte row, bool unsigned_index, byte bank) const;
    inline BgPixel sample_bg_pixel(half tilex, half tiley);
    inline BgPixel sample_window_pixel(half tilex, half tiley);
    inline BgPixel sample_tile_map_pixel(TileMapKind kind, half tilex,
                                         half tiley);
    inline ObjPixel sample_object_pixel(int screen_x);
    void set_mode(RenderingState new_mode, bool allow_interrupt = true);
    void request_stat_interrupt(byte stat_bit);
    void upload_frame_to_texture();
#if GBC_PPU_DEBUG && !defined(__EMSCRIPTEN__)
    void render_debug_point(SDL_Renderer *target, uint32_t color, int x, int y);
#endif
};

inline void PPU::io_write(half address, byte value) {
    if (bus == nullptr) {
        return;
    }
    auto &io = bus->IOrange;
    byte &cell = io[address - addr(MemoryRegion::IO_REGISTERS)];
    switch (address) {
        case addr(VideoRegister::LCDC): {
            const byte old = reg_LCDC;
            reg_LCDC = value;
            cell = value;
            if (!isBitSet(reg_LCDC, 5) && isBitSet(old, 5)) {
                wly = 0;
                wlyenabled = false;
            }
            cache_dirty = true;
            break;
        }
        case addr(VideoRegister::STAT): {
            reg_STAT = static_cast<byte>((value & 0xF8) | (reg_STAT & 0x07));
            update_stat_register();
            cache_dirty = true;
            break;
        }
        case addr(VideoRegister::SCY):
            reg_SCY = value;
            cell = value;
            cache_dirty = true;
            break;
        case addr(VideoRegister::SCX):
            reg_SCX = value;
            cell = value;
            cache_dirty = true;
            break;
        case addr(VideoRegister::LY):
            cell = static_cast<byte>(lines);
            break;
        case addr(VideoRegister::LYC):
            reg_LYC = value;
            cell = value;
            cache_dirty = true;
            break;
        case addr(VideoRegister::BGP):
            reg_BGP = value;
            cell = value;
            cache_dirty = true;
            break;
        case addr(VideoRegister::OBP0):
            reg_OBP0 = value;
            cell = value;
            cache_dirty = true;
            break;
        case addr(VideoRegister::OBP1):
            reg_OBP1 = value;
            cell = value;
            cache_dirty = true;
            break;
        case addr(VideoRegister::WY): {
            const byte old = reg_WY;
            reg_WY = value;
            cell = value;
            if (reg_WY != old) {
                wly = 0;
                wlyenabled = false;
            }
            cache_dirty = true;
            break;
        }
        case addr(VideoRegister::WX):
            reg_WX = value;
            cell = value;
            cache_dirty = true;
            break;
        default:
            cell = value;
            break;
    }
}

}  // namespace GBC