#include "PPU.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <utility>

#include "bit_ops.h"
#include "bus.h"
#include "enums.h"

namespace GBC {
void PPU::init_window() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_SetHint("SDL_RENDERER_SCALE_QUALITY", "0");
    SDL_CreateWindowAndRenderer("(GBC) hello window", WINDOW_WIDTH,
                                WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &window,
                                &renderer);
    SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, WINDOW_WIDTH,
                                WINDOW_HEIGHT);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
}

PPU::~PPU() {
#if GBC_PPU_DEBUG && !defined(__EMSCRIPTEN__)
    if (debug_tile_texture) SDL_DestroyTexture(debug_tile_texture);
    if (debug_object_texture) SDL_DestroyTexture(debug_object_texture);
    if (debug_window_texture) SDL_DestroyTexture(debug_window_texture);
    if (debug_bg_texture) SDL_DestroyTexture(debug_bg_texture);
    SDL_DestroyRenderer(debug_tile_renderer);
    SDL_DestroyRenderer(debug_window_renderer);
    SDL_DestroyRenderer(debug_object_renderer);
    SDL_DestroyRenderer(debug_bg_renderer);
    SDL_DestroyWindow(debug_tile_window);
    SDL_DestroyWindow(debug_window_window);
    SDL_DestroyWindow(debug_object_window);
    SDL_DestroyWindow(debug_bg_window);
#endif
    if (texture) SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

inline byte& PPU::io_reg(VideoRegister reg) {
    return bus->IOrange[addr(reg) - addr(MemoryRegion::IO_REGISTERS)];
}

void PPU::update_stat_register() {
    if (bus == nullptr) {
        return;
    }
    const byte high = static_cast<byte>(reg_STAT & 0xF8);
    const byte coincidence = static_cast<byte>((lyc_match ? 1 : 0) << 2);
    const byte mode_bits = static_cast<byte>(static_cast<byte>(mode) & 0x03);
    reg_STAT = static_cast<byte>(high | coincidence | mode_bits);
    io_reg(VideoRegister::STAT) = reg_STAT;
    update_stat_line();
}

inline void PPU::update_register_cache() {
    cached_LCDC = reg_LCDC;
    cached_BGP = reg_BGP;
    cached_OBP0 = reg_OBP0;
    cached_OBP1 = reg_OBP1;
    cached_SCX = reg_SCX;
    cached_SCY = reg_SCY;
    cached_WX = reg_WX;
    cached_WY = reg_WY;
    cached_LYC = reg_LYC;
    lyc_match = (lines == cached_LYC);
    update_stat_register();
    cache_dirty = false;
}

void PPU::execute_cycle() {
    if (dots >= 456) {
        ++lines;
        lyc_match = (lines == cached_LYC);
        update_stat_register();
        hdma_done_this_line = false;  // Reset HDMA flag at start of new line
    }
    dots %= 456;
    lines %= 154;
    if (dots == 0 || cache_dirty) {
        update_register_cache();
    }
    if (dots == 0) {
        const bool lcd_on = isBitSet(cached_LCDC, 7);
        const bool window_enable = isBitSet(cached_LCDC, 5);

        if (!lcd_on || !window_enable || lines >= 144) {
            window_vert_active = false;
        } else {
            if (lines == cached_WY) {
                window_vert_active = true;
                wly = 0;
            } else if (window_vert_active) {
                ++wly;
            }
        }
    }
    if (!isBitSet(cached_LCDC, 7)) {
        lines = 0;
        dots = 0;
        wly = 0;
        hdma_done_this_line = false;
        lyc_match = (lines == cached_LYC);
        update_stat_register();
        if (bus != nullptr) {
            io_reg(VideoRegister::LY) = 0;
            auto& if_reg = bus->IOrange[addr(IORegister::IF) -
                                        addr(MemoryRegion::IO_REGISTERS)];
            if_reg = clearBit(if_reg, 1);
        }
        set_mode(RenderingState::hblank);
        window_vert_active = false;
        return;
    }
    if (bus != nullptr) {
        io_reg(VideoRegister::LY) = static_cast<byte>(lines);
    }

    if (lines < 144) {
        if (dots < 80) {
            if (mode != RenderingState::OAMscan) {
                objnum = 0;
                oam_scan_dot = 0;
                oam_scan_index = 0;
                scanline_cache.valid = false;
                set_mode(RenderingState::OAMscan);
            }
            perform_oam_scan_step();
        } else if (dots < 252) {
            if (mode != RenderingState::draw) {
                set_mode(RenderingState::draw);
                renderX = 0;
                prepare_scanline();
            }

            draw_pixel();
        } else {
            if (mode != RenderingState::hblank) {
                set_mode(RenderingState::hblank);
                renderX = 0;
            }

            if (!hdma_done_this_line && bus != nullptr) {
                bus->handle_hblank_hdma();
                hdma_done_this_line = true;
            }
        }
    } else {
        if (mode != RenderingState::vblank) {
            set_mode(RenderingState::vblank);
            wly = 0;
            auto& if_reg = bus->IOrange[addr(IORegister::IF) -
                                        addr(MemoryRegion::IO_REGISTERS)];
            if_reg = setBit(if_reg, 0);
            upload_frame_to_texture();
        }
    }

    ++dots;
}

void PPU::prepare_scanline() {
    scanline_cache.bg_enabled = isBitSet(cached_LCDC, 0);
    scanline_cache.obj_enabled = isBitSet(cached_LCDC, 1);
    scanline_cache.window_enabled = isBitSet(cached_LCDC, 5);
    scanline_cache.scx = cached_SCX;
    scanline_cache.scy = cached_SCY;
    scanline_cache.obj_size = isBitSet(cached_LCDC, 2) ? 16 : 8;
    scanline_cache.window_trigger_x = static_cast<int32_t>(cached_WX) - 7;
    scanline_cache.window_can_activate = scanline_cache.window_enabled &&
                                         (cached_WX <= 166) &&
                                         window_vert_active;
    scanline_cache.valid = true;
}

void PPU::draw_pixel() {
    const bool bg_enabled = scanline_cache.bg_enabled;
    const bool obj_enabled = scanline_cache.obj_enabled;

    const int32_t screen_x = static_cast<int32_t>(renderX);
    const bool window_active = scanline_cache.window_can_activate &&
                               (screen_x >= scanline_cache.window_trigger_x);

    const byte bg_tilex = static_cast<byte>(
        (static_cast<int32_t>(scanline_cache.scx) + screen_x) & 0xFF);
    const byte bg_tiley = static_cast<byte>(
        (static_cast<int32_t>(scanline_cache.scy) + lines) & 0xFF);
    byte window_tilex = 0;
    byte window_tiley = wly;
    if (window_active) {
        const int32_t relative_x = screen_x - scanline_cache.window_trigger_x;
        window_tilex = static_cast<byte>(relative_x & 0xFF);
    }

    BgPixel background_pixel{};
    if (bg_enabled || config.cgb_mode) {
        background_pixel = sample_bg_pixel(bg_tilex, bg_tiley);
    }
    BgPixel window_pixel{};
    const bool window_pixel_visible =
        window_active && (bg_enabled || config.cgb_mode);
    if (window_pixel_visible) {
        window_pixel = sample_window_pixel(window_tilex, window_tiley);
    }
    BgPixel bg_pixel = window_pixel_visible ? window_pixel : background_pixel;

    ObjPixel obj_pixel{};
    if (obj_enabled) {
        obj_pixel = sample_object_pixel(screen_x);
    }

    const uint32_t background_rgb =
        (bg_enabled || config.cgb_mode)
            ? bus->get_bg_color(background_pixel.palette,
                                background_pixel.color)
            : 0;
    const uint32_t window_rgb =
        window_pixel_visible
            ? bus->get_bg_color(window_pixel.palette, window_pixel.color)
            : 0;
    uint32_t bg_color_rgb =
        0xFF000000u | (window_pixel_visible ? window_rgb : background_rgb);
    uint32_t final_color = bg_color_rgb;
    const bool sprite_visible = obj_pixel.has_pixel && obj_pixel.color != 0;
    const bool bg_has_color = bg_pixel.color != 0;
    uint32_t obj_color_rgb = 0;
    bool obj_color_cached = false;
    const auto get_obj_color = [&]() -> uint32_t {
        if (!obj_color_cached) {
            obj_color_rgb = 0xFF000000u | bus->get_obj_color(obj_pixel.palette,
                                                             obj_pixel.color);
            obj_color_cached = true;
        }
        return obj_color_rgb;
    };

    bool obj_wins = false;
    if (config.cgb_mode) {
        if (bg_pixel.color == 0) {
            obj_wins = true;
        } else if (!isBitSet(cached_LCDC, 0)) {
            obj_wins = true;
        } else {
            const bool oam_bit7_clear = !obj_pixel.priority;
            const bool bg_bit7_clear = !bg_pixel.priority;
            obj_wins = oam_bit7_clear && bg_bit7_clear;
        }
    } else {
        if (obj_pixel.priority) {
            obj_wins = false;
        } else {
            const bool bg_priority =
                bg_pixel.priority || bus->bg_priority_over_obj();
            obj_wins = !bg_priority;
        }
    }

    if (obj_enabled && sprite_visible) {
        if (!bg_enabled || !bg_has_color) {
            final_color = get_obj_color() | 0xFF000000u;
        } else if (obj_wins) {
            final_color = get_obj_color() | 0xFF000000u;
        } else {
            final_color = bg_color_rgb;
        }
    }
    final_color |= 0xFF000000u;

    if (screen_x >= 0 && screen_x < static_cast<int32_t>(WINDOW_WIDTH) &&
        lines < WINDOW_HEIGHT) {
        const uint32_t frame_index =
            static_cast<uint32_t>(lines) * WINDOW_WIDTH + screen_x;
        if (frame_index < frame_rgb.size()) {
            frame_rgb[frame_index] = final_color;
#if GBC_PPU_DEBUG && !defined(__EMSCRIPTEN__)
            constexpr uint32_t opaque = 0xFF000000u;
            debug_obj_frame[frame_index] = (obj_enabled && sprite_visible)
                                               ? (opaque | get_obj_color())
                                               : 0;
#endif
        }
    }
    ++renderX;
}

inline PPU::ObjPixel PPU::sample_object_pixel(int screen_x) {
    ObjPixel result{};
    const byte objsize = scanline_cache.obj_size;

    for (int i = 0; i < static_cast<int>(objnum); ++i) {
        const obj sprite = objbuffer[i];
        const int sprite_x = static_cast<int>(sprite.objx) - 8;
        const int sprite_right = sprite_x + 8;
        if (screen_x < sprite_x || screen_x >= sprite_right) {
            continue;
        }

        byte tile_index = sprite.index;
        if (objsize == 16) {
            tile_index &= 0xFE;
        }

        const byte flags = sprite.flags;
        const bool yflip = isBitSet(flags, 6);
        const bool xflip = isBitSet(flags, 5);
        const bool sprite_priority =
            config.cgb_mode ? !isBitSet(flags, 7) : isBitSet(flags, 7);
        const byte palette = config.cgb_mode ? getBitRange(flags, 0, 3)
                                             : (isBitSet(flags, 4) ? 1 : 0);
        const byte tile_bank = (config.cgb_mode && isBitSet(flags, 3)) ? 1 : 0;

        int line = lines - sprite.objy;
        if (line < 0 || line >= objsize) {
            continue;
        }
        byte row = static_cast<byte>(line);
        if (yflip) {
            row = static_cast<byte>(objsize - 1 - row);
        }
        if (objsize == 16 && row >= 8) {
            row -= 8;
            tile_index = static_cast<byte>(tile_index + 1);
        }

        const auto [tilelow, tilehigh] =
            fetch_tile_row(tile_index, row, true, tile_bank);
        const int pixel_offset = screen_x - sprite_x;
        byte bit_index = xflip ? static_cast<byte>(pixel_offset & 0x7)
                               : static_cast<byte>(7 - (pixel_offset & 0x7));
        bit_index &= 0x7;
        const byte mask = static_cast<byte>(1u << bit_index);
        const byte color =
            ((tilelow & mask) ? 1 : 0) | ((tilehigh & mask) ? 2 : 0);
        if (color == 0) {
            continue;
        }

        result.color = color;
        result.palette = palette;
        result.priority = isBitSet(flags, 7);
        result.has_pixel = true;
        return result;
    }

    return result;
}

inline PPU::BgPixel PPU::sample_bg_pixel(half tilex, half tiley) {
    return sample_tile_map_pixel(TileMapKind::Background, tilex, tiley);
}

inline PPU::BgPixel PPU::sample_window_pixel(half tilex, half tiley) {
    return sample_tile_map_pixel(TileMapKind::Window, tilex, tiley);
}

inline PPU::BgPixel PPU::sample_tile_map_pixel(TileMapKind kind, half tilex,
                                               half tiley) {
    const bool window_map = (kind == TileMapKind::Window);
    const half tile_map = window_map
                              ? (isBitSet(cached_LCDC, 6) ? 0x9C00 : 0x9800)
                              : (isBitSet(cached_LCDC, 3) ? 0x9C00 : 0x9800);
    const bool unsigned_table = isBitSet(cached_LCDC, 4);
    const half tile_index_addr = static_cast<half>(
        ((tilex / 8) & 31) + (((tiley / 8) & 31) * 32) + tile_map);
    const half offset =
        static_cast<half>(tile_index_addr - addr(MemoryRegion::VIDEO_RAM));
    const byte tile_index = bus->read_vram(0, offset, true);
    byte attributes = 0;
    if (config.cgb_mode) {
        attributes = bus->read_vram(1, offset, true);
    }

    const byte tile_bank = (config.cgb_mode && isBitSet(attributes, 3)) ? 1 : 0;
    byte row = tiley & 0x07;
    if (config.cgb_mode && isBitSet(attributes, 6)) {
        row = static_cast<byte>(7 - row);
    }
    const auto [tilelow, tilehigh] =
        fetch_tile_row(tile_index, row, unsigned_table, tile_bank);

    byte bit_index = 7 - (tilex & 0x07);
    if (config.cgb_mode && isBitSet(attributes, 5)) {
        bit_index = tilex & 0x07;
    }
    const byte mask = static_cast<byte>(1u << bit_index);
    const byte color = ((tilelow & mask) ? 1 : 0) | ((tilehigh & mask) ? 2 : 0);

    BgPixel pixel{};
    pixel.color = color;
    pixel.palette = config.cgb_mode ? getBitRange(attributes, 0, 3) : 0;
    pixel.priority = config.cgb_mode && isBitSet(attributes, 7);
    return pixel;
}

std::pair<byte, byte> PPU::fetch_tile_row(
    byte tile_index, byte row, bool unsigned_index, byte bank) const {
    half address = 0;
    if (unsigned_index) {
        address = addr(MemoryRegion::VIDEO_RAM) + tile_index * 16 + row * 2;
    } else {
        address = static_cast<half>(
            0x9000 +
            static_cast<int16_t>(static_cast<int8_t>(tile_index)) * 16 +
            row * 2);
    }
    const half offset =
        static_cast<half>(address - addr(MemoryRegion::VIDEO_RAM));
    const byte tilelow = bus->read_vram(bank, offset, true);
    const byte tilehigh =
        bus->read_vram(bank, static_cast<half>((offset + 1) & 0x1FFF), true);
    return {tilelow, tilehigh};
}

void PPU::set_mode(RenderingState new_mode) {
    mode = new_mode;
    update_stat_register();
}

void PPU::update_stat_line() {
    if (bus == nullptr) return;

    if (!isBitSet(cached_LCDC, 7)) {
        stat_signal = false;
        return;
    }

    bool new_signal = false;

    // LYC Interrupt
    if (isBitSet(reg_STAT, 6) && lyc_match) {
        new_signal = true;
    }

    // Mode 2 Interrupt (OAM)
    if (isBitSet(reg_STAT, 5) && mode == RenderingState::OAMscan) {
        new_signal = true;
    }

    // Mode 1 Interrupt (VBlank)
    if (isBitSet(reg_STAT, 4) && mode == RenderingState::vblank) {
        new_signal = true;
    }

    // Mode 0 Interrupt (HBlank)
    if (isBitSet(reg_STAT, 3) && mode == RenderingState::hblank) {
        new_signal = true;
    }

    if (new_signal && !stat_signal) {
        auto& if_reg = bus->IOrange[addr(IORegister::IF) -
                                    addr(MemoryRegion::IO_REGISTERS)];
        if_reg = setBit(if_reg, 1);
    }

    stat_signal = new_signal;
}

void PPU::perform_oam_scan_step() {
    if (mode != RenderingState::OAMscan) {
        return;
    }
    if ((oam_scan_dot & 1) == 0) {
        scan_oam_entry();
    }
    ++oam_scan_dot;
}

void PPU::scan_oam_entry() {
    if (bus == nullptr) {
        return;
    }
    if (oam_scan_index >= OAM_SIZE) {
        return;
    }
    if (objnum >= static_cast<int>(objbuffer.size())) {
        oam_scan_index = std::min<size_t>(oam_scan_index + 4, OAM_SIZE);
        return;
    }

    const byte objsize = isBitSet(cached_LCDC, 2) ? 16 : 8;
    const byte objy = bus->OAM[oam_scan_index] - 16;
    const byte objx = bus->OAM[oam_scan_index + 1];
    const byte index = bus->OAM[oam_scan_index + 2];
    const byte flags = bus->OAM[oam_scan_index + 3];
    oam_scan_index = std::min<size_t>(oam_scan_index + 4, OAM_SIZE);

    if (objy <= lines && objy + objsize > lines) {
        objbuffer[objnum].objx = objx;
        objbuffer[objnum].objy = objy;
        objbuffer[objnum].index = index;
        objbuffer[objnum].flags = flags;
        ++objnum;
    }
}

void PPU::upload_frame_to_texture() {
    if (texture == nullptr) {
        return;
    }

    SDL_UpdateTexture(texture, nullptr, frame_rgb.data(),
                      WINDOW_WIDTH * sizeof(uint32_t));

#ifndef __EMSCRIPTEN__
    SDL_RenderTexture(renderer, texture, nullptr, nullptr);
#if GBC_PPU_DEBUG
    render_debug_layers();
#endif
#endif
}

void PPU::present() {
    if (renderer) {
#ifdef __EMSCRIPTEN__
        if (texture) {
            SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        }
#endif
        SDL_RenderPresent(renderer);
    }
}

#if GBC_PPU_DEBUG && !defined(__EMSCRIPTEN__)
void PPU::render_debug_layers() {
    init_debug_window();
    fill_debug_background_surface();
    fill_debug_window_surface();
    if (!debug_bg_surface.empty()) {
        update_debug_layer(debug_bg_renderer, debug_bg_texture,
                           debug_bg_surface.data(), debug_bg_surface_width,
                           debug_bg_surface_height);
    }
    if (!debug_window_surface.empty()) {
        update_debug_layer(debug_window_renderer, debug_window_texture,
                           debug_window_surface.data(),
                           debug_window_surface_width,
                           debug_window_surface_height);
    }
    update_debug_layer(debug_object_renderer, debug_object_texture,
                       debug_obj_frame.data(), WINDOW_WIDTH, WINDOW_HEIGHT);
    render_debug_tiles();
}

void PPU::update_debug_layer(SDL_Renderer* target_renderer,
                             SDL_Texture* target_texture,
                             const uint32_t* frame_data, int width,
                             int height) {
    if (target_renderer == nullptr || target_texture == nullptr ||
        frame_data == nullptr) {
        return;
    }
    void* pixels = nullptr;
    int pitch = 0;
    if (!SDL_LockTexture(target_texture, nullptr, &pixels, &pitch)) {
        return;
    }
    auto* dst = static_cast<std::uint8_t*>(pixels);
    const int row_bytes = width * static_cast<int>(sizeof(uint32_t));
    for (int y = 0; y < height; ++y) {
        std::memcpy(dst + y * pitch, frame_data + y * width, row_bytes);
    }
    SDL_UnlockTexture(target_texture);
    SDL_RenderTexture(target_renderer, target_texture, nullptr, nullptr);
    SDL_RenderPresent(target_renderer);
}

void PPU::fill_debug_background_surface() {
    if (bus == nullptr) {
        debug_bg_surface.clear();
        debug_bg_surface_width = 0;
        debug_bg_surface_height = 0;
        return;
    }
    debug_bg_surface_width = DEBUG_MAP_SIZE;
    debug_bg_surface_height = DEBUG_MAP_SIZE;
    debug_bg_surface.resize(static_cast<size_t>(DEBUG_MAP_SIZE) *
                            static_cast<size_t>(DEBUG_MAP_SIZE));
    const uint32_t opaque = 0xFF000000u;
    const byte scx = cached_SCX;
    const byte scy = cached_SCY;
    for (int y = 0; y < DEBUG_MAP_SIZE; ++y) {
        for (int x = 0; x < DEBUG_MAP_SIZE; ++x) {
            const half tilex =
                static_cast<byte>((static_cast<int>(scx) + x - 6) & 0xFF);
            const half tiley =
                static_cast<byte>((static_cast<int>(scy) + y) & 0xFF);
            const BgPixel pixel = sample_bg_pixel(tilex, tiley);
            const uint32_t color =
                bus->get_bg_color(pixel.palette, pixel.color);
            debug_bg_surface[y * DEBUG_MAP_SIZE + x] = opaque | color;
        }
    }
}

void PPU::fill_debug_window_surface() {
    if (bus == nullptr) {
        debug_window_surface.clear();
        debug_window_surface_width = 0;
        debug_window_surface_height = 0;
        return;
    }
    debug_window_surface_width = DEBUG_MAP_SIZE;
    debug_window_surface_height = DEBUG_MAP_SIZE;
    debug_window_surface.resize(static_cast<size_t>(DEBUG_MAP_SIZE) *
                                static_cast<size_t>(DEBUG_MAP_SIZE));
    const uint32_t opaque = 0xFF000000u;
    for (int y = 0; y < DEBUG_MAP_SIZE; ++y) {
        for (int x = 0; x < DEBUG_MAP_SIZE; ++x) {
            const BgPixel pixel =
                sample_window_pixel(static_cast<byte>(x), static_cast<byte>(y));
            const uint32_t color =
                bus->get_bg_color(pixel.palette, pixel.color);
            debug_window_surface[y * DEBUG_MAP_SIZE + x] = opaque | color;
        }
    }
}

void PPU::render_debug_tiles() {
    if (debug_tile_renderer == nullptr || debug_tile_texture == nullptr ||
        bus == nullptr || debug_tile_surface.empty()) {
        return;
    }
    const int banks = config.cgb_mode ? 2 : 1;
    const int tiles_per_row = 16;
    const int tile_size = 8;
    const int bank_width = tiles_per_row * tile_size;
    static constexpr std::array<uint32_t, 4> tile_palette{
        0xFFFFFFFFu, 0xFFC0C0C0u, 0xFF808080u, 0xFF202020u};
    std::fill(debug_tile_surface.begin(), debug_tile_surface.end(), 0u);
    for (int bank = 0; bank < banks; ++bank) {
        for (int tile = 0; tile < 384; ++tile) {
            const int tile_x = tile % tiles_per_row;
            const int tile_y = tile / tiles_per_row;
            const int base_x = bank * bank_width + tile_x * tile_size;
            const int base_y = tile_y * tile_size;
            for (int row = 0; row < 8; ++row) {
                const half tile_address =
                    addr(MemoryRegion::VIDEO_RAM) +
                    static_cast<half>(tile * 16 + row * 2);
                const half offset = static_cast<half>(
                    tile_address - addr(MemoryRegion::VIDEO_RAM));
                const byte tilelow =
                    bus->read_vram(static_cast<byte>(bank), offset, true);
                const byte tilehigh =
                    bus->read_vram(static_cast<byte>(bank),
                                   static_cast<half>((offset + 1) & 0x1FFF),
                                   true);
                for (int col = 0; col < 8; ++col) {
                    const byte mask = static_cast<byte>(1u << (7 - col));
                    const byte color =
                        static_cast<byte>(((tilelow & mask) ? 1 : 0) |
                                          ((tilehigh & mask) ? 2 : 0));
                    const uint32_t rgb = tile_palette[color & 0x3];
                    const int x = base_x + col;
                    const int y = base_y + row;
                    const size_t index =
                        static_cast<size_t>(y) *
                            static_cast<size_t>(debug_tile_surface_width) +
                        static_cast<size_t>(x);
                    if (index < debug_tile_surface.size()) {
                        debug_tile_surface[index] = rgb;
                    }
                }
            }
        }
    }
    void* pixels = nullptr;
    int pitch = 0;
    if (!SDL_LockTexture(debug_tile_texture, nullptr, &pixels, &pitch)) {
        return;
    }
    auto* dst = static_cast<std::uint8_t*>(pixels);
    const auto* src32 = debug_tile_surface.data();
    const int row_bytes =
        debug_tile_surface_width * static_cast<int>(sizeof(uint32_t));
    for (int y = 0; y < debug_tile_surface_height; ++y) {
        std::memcpy(dst + y * pitch, src32 + y * debug_tile_surface_width,
                    row_bytes);
    }
    SDL_UnlockTexture(debug_tile_texture);
    SDL_RenderTexture(debug_tile_renderer, debug_tile_texture, nullptr,
                      nullptr);
    SDL_RenderPresent(debug_tile_renderer);
}

void PPU::dump_info() {
    std::cerr << std::hex << "dots: " << dots << '\n';
    std::cerr << "lines: " << lines << '\n';
    std::cerr << "renderX: " << renderX << '\n';
    switch (mode) {
        case RenderingState::hblank:
            std::cerr << "state: hblank\n";
            break;
        case RenderingState::vblank:
            std::cerr << "state: vblank\n";
            break;
        case RenderingState::OAMscan:
            std::cerr << "state: OAMscan\n";
            break;
        case RenderingState::draw:
            std::cerr << "state: draw\n";
            break;
    }
    std::cerr << std::endl;
}

void PPU::dump_vram() {
    for (int i = 0; i < 0x3FF; ++i) {
        if (i % 16 == 0) std::cout << std::endl;
        std::cout << std::hex << (i + 0x9900) << ": "
                  << std::bitset<8>(bus->read(i + 0x9900) & 0xFF) << " ";
    }
}

void PPU::init_debug_window() {
    if (debug_tile_window != nullptr) {
        return;
    }
    SDL_Init(SDL_INIT_VIDEO);
    constexpr int tiles_per_row = 16;
    constexpr int tile_size = 8;
    const int tile_map_width = tiles_per_row * tile_size;  // 128 pixels
    const int tile_map_height =
        (384 / tiles_per_row) * tile_size;  // 192 pixels
    const int tile_window_width = (config.cgb_mode ? 2 : 1) * tile_map_width;
    const int tile_window_height = tile_map_height;
    debug_tile_window =
        SDL_CreateWindow("(GBC) VRAM tiles", tile_window_width * 4,
                         tile_window_height * 4, SDL_WINDOW_RESIZABLE);
    debug_tile_renderer = SDL_CreateRenderer(debug_tile_window, nullptr);
    SDL_SetRenderLogicalPresentation(debug_tile_renderer, tile_window_width,
                                     tile_window_height,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderDrawColor(debug_tile_renderer, 255, 255, 255, 255);
    SDL_RenderClear(debug_tile_renderer);

    debug_bg_window = SDL_CreateWindow("(GBC) background layer", 256 * 2,
                                       256 * 2, SDL_WINDOW_RESIZABLE);
    debug_bg_renderer = SDL_CreateRenderer(debug_bg_window, nullptr);
    SDL_SetRenderLogicalPresentation(debug_bg_renderer, 256, 256,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderDrawColor(debug_bg_renderer, 255, 255, 255, 255);
    SDL_RenderClear(debug_bg_renderer);

    debug_window_window =
        SDL_CreateWindow("(GBC) window layer", WINDOW_WIDTH * 4,
                         WINDOW_HEIGHT * 4, SDL_WINDOW_RESIZABLE);
    debug_window_renderer = SDL_CreateRenderer(debug_window_window, nullptr);
    SDL_SetRenderLogicalPresentation(debug_window_renderer, WINDOW_WIDTH,
                                     WINDOW_HEIGHT,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderDrawColor(debug_window_renderer, 255, 255, 255, 255);
    SDL_RenderClear(debug_window_renderer);

    debug_object_window =
        SDL_CreateWindow("(GBC) object layer", WINDOW_WIDTH * 4,
                         WINDOW_HEIGHT * 4, SDL_WINDOW_RESIZABLE);
    debug_object_renderer = SDL_CreateRenderer(debug_object_window, nullptr);
    SDL_SetRenderLogicalPresentation(debug_object_renderer, WINDOW_WIDTH,
                                     WINDOW_HEIGHT,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderDrawColor(debug_object_renderer, 255, 255, 255, 255);
    SDL_RenderClear(debug_object_renderer);

    const auto create_layer_texture = [&](SDL_Renderer* target, int width,
                                          int height) {
        if (target == nullptr) return static_cast<SDL_Texture*>(nullptr);
        SDL_Texture* tex =
            SDL_CreateTexture(target, SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING, width, height);
        if (tex != nullptr) {
            SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
        }
        return tex;
    };
    debug_bg_surface_width = DEBUG_MAP_SIZE;
    debug_bg_surface_height = DEBUG_MAP_SIZE;
    debug_bg_surface.resize(static_cast<size_t>(DEBUG_MAP_SIZE) *
                            static_cast<size_t>(DEBUG_MAP_SIZE));
    debug_bg_texture =
        create_layer_texture(debug_bg_renderer, DEBUG_MAP_SIZE, DEBUG_MAP_SIZE);

    debug_window_surface_width = DEBUG_MAP_SIZE;
    debug_window_surface_height = DEBUG_MAP_SIZE;
    debug_window_surface.resize(static_cast<size_t>(DEBUG_MAP_SIZE) *
                                static_cast<size_t>(DEBUG_MAP_SIZE));
    debug_window_texture = create_layer_texture(debug_window_renderer,
                                                DEBUG_MAP_SIZE, DEBUG_MAP_SIZE);

    debug_object_texture = create_layer_texture(debug_object_renderer,
                                                WINDOW_WIDTH, WINDOW_HEIGHT);

    debug_tile_surface_width = tile_window_width;
    debug_tile_surface_height = tile_window_height;
    debug_tile_surface.resize(static_cast<size_t>(debug_tile_surface_width) *
                              static_cast<size_t>(debug_tile_surface_height));
    if (debug_tile_renderer != nullptr) {
        debug_tile_texture =
            SDL_CreateTexture(debug_tile_renderer, SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING,
                              debug_tile_surface_width,
                              debug_tile_surface_height);
        if (debug_tile_texture != nullptr) {
            SDL_SetTextureScaleMode(debug_tile_texture, SDL_SCALEMODE_NEAREST);
        }
    }
}
#endif

}  // namespace GBC