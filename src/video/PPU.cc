#include "PPU.h"

#include <algorithm>
#include <iostream>
#include <utility>

#include "bit_ops.h"
#include "bus.h"
#include "enums.h"

namespace GBC {
void PPU::init_window() {
    SDL_Init(SDL_INIT_VIDEO);
#ifdef __EMSCRIPTEN__
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    window = SDL_CreateWindow("(GBC) hello window", SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH * 8,
                              WINDOW_HEIGHT * 8, SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH * 8, WINDOW_HEIGHT * 8);
#else
    SDL_CreateWindowAndRenderer("(GBC) hello window", WINDOW_WIDTH * 4,
                                WINDOW_HEIGHT * 4, SDL_WINDOW_RESIZABLE,
                                &window, &renderer);
    SDL_SetRenderScale(renderer, 4, 4);
#endif
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24,
                                SDL_TEXTUREACCESS_STREAMING, WINDOW_WIDTH,
                                WINDOW_HEIGHT);
#ifndef __EMSCRIPTEN__
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
#endif
}

#if GBC_PPU_DEBUG && !defined(__EMSCRIPTEN__)
void PPU::init_debug_window() {
    if (debug_tile_window != nullptr) {
        return;
    }
    SDL_Init(SDL_INIT_VIDEO);
    debug_tile_window = SDL_CreateWindow("(GBC) hello tile window", 128 * 4,
                                         192 * 4, SDL_WINDOW_RESIZABLE);
    debug_tile_renderer = SDL_CreateRenderer(debug_tile_window, nullptr);
    SDL_SetRenderLogicalPresentation(debug_tile_renderer, 128, 192,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderDrawColor(debug_tile_renderer, 255, 255, 255, 255);
    SDL_RenderClear(debug_tile_renderer);

    debug_bg_window = SDL_CreateWindow("(GBC) hello background window", 256 * 2,
                                       256 * 2, SDL_WINDOW_RESIZABLE);
    debug_bg_renderer = SDL_CreateRenderer(debug_bg_window, nullptr);
    SDL_SetRenderLogicalPresentation(debug_bg_renderer, 256, 256,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderDrawColor(debug_bg_renderer, 255, 255, 255, 255);
    SDL_RenderClear(debug_bg_renderer);

    debug_window_window =
        SDL_CreateWindow("(GBC) hello window window", WINDOW_WIDTH * 4,
                         WINDOW_HEIGHT * 4, SDL_WINDOW_RESIZABLE);
    debug_window_renderer = SDL_CreateRenderer(debug_window_window, nullptr);
    SDL_SetRenderLogicalPresentation(debug_window_renderer, WINDOW_WIDTH,
                                     WINDOW_HEIGHT,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderDrawColor(debug_window_renderer, 255, 255, 255, 255);
    SDL_RenderClear(debug_window_renderer);

    debug_object_window =
        SDL_CreateWindow("(GBC) hello object window", WINDOW_WIDTH * 4,
                         WINDOW_HEIGHT * 4, SDL_WINDOW_RESIZABLE);
    debug_object_renderer = SDL_CreateRenderer(debug_object_window, nullptr);
    SDL_SetRenderLogicalPresentation(debug_object_renderer, WINDOW_WIDTH,
                                     WINDOW_HEIGHT,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderDrawColor(debug_object_renderer, 255, 255, 255, 255);
    SDL_RenderClear(debug_object_renderer);
}
#endif

PPU::~PPU() {
    if (texture) SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
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
    cache_dirty = false;
}

void PPU::execute_cycle() {
    if (dots >= 456) {
        ++lines;
        lyc_match = (lines == cached_LYC);
    }
    dots %= 456;
    lines %= 154;

    if (dots == 0 || cache_dirty) {
        update_register_cache();
    }

    if (!isBitSet(cached_LCDC, 7)) {
        set_mode(RenderingState::hblank, false);
        auto& if_reg = bus->IOrange[addr(IORegister::IF) -
                                    addr(MemoryRegion::IO_REGISTERS)];
        if_reg = clearBit(if_reg, 1);
        return;
    }
    auto& if_reg =
        bus->IOrange[addr(IORegister::IF) - addr(MemoryRegion::IO_REGISTERS)];
    if_reg = clearBit(if_reg, 1);
    if (isBitSet(reg_STAT, 6) && lyc_match) {
        if_reg = setBit(if_reg, 1);
    }

    if (lines < 144) {
        if (dots < 80) {
            if (mode != RenderingState::OAMscan) {
                objnum = 0;
                set_mode(RenderingState::OAMscan);
                byte objsize = isBitSet(cached_LCDC, 2) ? 16 : 8;

                for (int16_t i = 0x00; i < 0x9F; i += 4) {
                    byte objy = bus->OAM[i] - 16, objx = bus->OAM[i + 1],
                         index = bus->OAM[i + 2], flags = bus->OAM[i + 3];
                    if (objy <= lines && objy + objsize > lines) {
                        objbuffer[objnum].objx = objx,
                        objbuffer[objnum].objy = objy,
                        objbuffer[objnum].index = index,
                        objbuffer[objnum].flags = flags;
                        ++objnum;
                    }

                    if (objnum == 10) break;
                }
            }

        } else if (dots < 252) {
            if (mode != RenderingState::draw) {
                set_mode(RenderingState::draw, false);
                renderX = 0;  // Reset renderX at start of draw phase
                wlyenabled = false;
            }

            draw_pixel();
        } else {
            if (mode != RenderingState::hblank) {
                set_mode(RenderingState::hblank);
                if (bus != nullptr) {
                    bus->handle_hblank_hdma();
                }
                renderX = 0;
                if (wlyenabled) {
                    ++wly;
                }
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

void PPU::draw_pixel() {
    const bool bg_enabled = isBitSet(cached_LCDC, 0);
    const bool obj_enabled = isBitSet(cached_LCDC, 1);
    const bool window_enabled = isBitSet(cached_LCDC, 5);

    const byte wx = cached_WX;
    const byte wy = cached_WY;
    const byte scx = cached_SCX;
    const byte scy = cached_SCY;

    const int32_t pipeline_x = renderX >= 6 ? renderX - 6 : 0;
    const int32_t screen_x = pipeline_x;

    const int32_t window_trigger_x = static_cast<int32_t>(wx) - 7;
    const bool window_trigger_valid = wx <= 166;
    const bool window_active = window_enabled && window_trigger_valid &&
                               (lines >= wy) && (screen_x >= window_trigger_x);

    if (window_active && !wlyenabled) {
        wlyenabled = true;
    }

    const byte bg_tilex =
        static_cast<byte>((static_cast<int32_t>(scx) + screen_x) & 0xFF);
    const byte bg_tiley =
        static_cast<byte>((static_cast<int32_t>(scy) + lines) & 0xFF);
    byte window_tilex = 0;
    byte window_tiley = wly;
    if (window_active) {
        const int32_t relative_x = screen_x - window_trigger_x;
        window_tilex = static_cast<byte>(relative_x & 0xFF);
    }

    BgPixel bg_pixel{};
    if (bg_enabled) {
        bg_pixel = window_active
                       ? sample_window_pixel(window_tilex, window_tiley)
                       : sample_bg_pixel(bg_tilex, bg_tiley);
    }

    ObjPixel obj_pixel{};
    if (obj_enabled) {
        obj_pixel = sample_object_pixel(screen_x);
    }

    const uint32_t bg_color_rgb =
        bus->get_bg_color(bg_pixel.palette, bg_pixel.color);
    uint32_t final_color = bg_color_rgb;
    const bool sprite_visible = obj_pixel.has_pixel && obj_pixel.color != 0;
    const bool bg_has_color = bg_pixel.color != 0;
    uint32_t obj_color_rgb = 0;
    bool obj_color_cached = false;
    const auto get_obj_color = [&]() -> uint32_t {
        if (!obj_color_cached) {
            obj_color_rgb =
                bus->get_obj_color(obj_pixel.palette, obj_pixel.color);
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
            final_color = get_obj_color();
        } else if (obj_wins) {
            final_color = get_obj_color();
        } else {
            final_color = bg_color_rgb;
        }
    }

#if GBC_PPU_DEBUG && !defined(__EMSCRIPTEN__)
    init_debug_window();
    const uint32_t obj_debug_color = obj_pixel.has_pixel ? get_obj_color() : 0;
    render_debug_point(debug_object_renderer, obj_debug_color, pipeline_x,
                       lines);
    render_debug_point(debug_window_renderer, bg_color_rgb, pipeline_x, lines);
#endif

    if (screen_x >= 0 && screen_x < static_cast<int32_t>(WINDOW_WIDTH) &&
        lines < WINDOW_HEIGHT) {
        const uint32_t frame_index =
            static_cast<uint32_t>(lines) * WINDOW_WIDTH + screen_x;
        if (frame_index < frame_rgb.size()) {
            frame_rgb[frame_index] = final_color;
        }
    }
    ++renderX;
}

inline PPU::ObjPixel PPU::sample_object_pixel(int screen_x) {
    ObjPixel result{};
    const byte objsize = isBitSet(cached_LCDC, 2) ? 16 : 8;

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
    if (config.cgb_mode && isBitSet(attributes, 5)) {
        row = static_cast<byte>(7 - row);
    }
    const auto [tilelow, tilehigh] =
        fetch_tile_row(tile_index, row, unsigned_table, tile_bank);

    byte bit_index = 7 - (tilex & 0x07);
    if (config.cgb_mode && isBitSet(attributes, 4)) {
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

void PPU::set_mode(RenderingState new_mode, bool allow_interrupt) {
    mode = new_mode;
    reg_STAT = static_cast<byte>(
        (reg_STAT & 0xF8) | static_cast<byte>(new_mode) | (lyc_match << 2));
    if (!allow_interrupt) {
        return;
    }
    switch (new_mode) {
        case RenderingState::hblank:
            request_stat_interrupt(3);
            break;
        case RenderingState::vblank:
            request_stat_interrupt(4);
            break;
        case RenderingState::OAMscan:
            request_stat_interrupt(5);
            break;
        case RenderingState::draw:
            break;
    }
}

void PPU::request_stat_interrupt(byte stat_bit) {
    if (!isBitSet(reg_STAT, stat_bit) || bus == nullptr) {
        return;
    }
    auto& if_reg =
        bus->IOrange[addr(IORegister::IF) - addr(MemoryRegion::IO_REGISTERS)];
    if_reg = setBit(if_reg, 1);
}

void PPU::upload_frame_to_texture() {
    if (renderer == nullptr) {
        return;
    }
    if (texture == nullptr) {
        SDL_RenderPresent(renderer);
        return;
    }

    void* pixels = nullptr;
    int pitch = 0;
#ifdef __EMSCRIPTEN__
    if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) != 0) {
        return;
    }
#else
    if (!SDL_LockTexture(texture, nullptr, &pixels, &pitch)) {
        return;
    }
#endif
    auto* pixel_data = static_cast<byte*>(pixels);
    for (int y = 0; y < static_cast<int>(WINDOW_HEIGHT); ++y) {
        for (int x = 0; x < static_cast<int>(WINDOW_WIDTH); ++x) {
            const uint32_t color = frame_rgb[y * WINDOW_WIDTH + x];
            const int index = y * pitch + x * 3;
            pixel_data[index] = static_cast<byte>(getBitRange(color, 16, 8));
            pixel_data[index + 1] = static_cast<byte>(getBitRange(color, 8, 8));
            pixel_data[index + 2] = static_cast<byte>(getBitRange(color, 0, 8));
        }
    }
    SDL_UnlockTexture(texture);
#ifdef __EMSCRIPTEN__
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
#else
    SDL_RenderTexture(renderer, texture, nullptr, nullptr);
#endif
    SDL_RenderPresent(renderer);
}

#if GBC_PPU_DEBUG && !defined(__EMSCRIPTEN__)
void PPU::render_debug_point(SDL_Renderer* target, uint32_t color, int x,
                             int y) {
    if (target == nullptr) {
        return;
    }
    SDL_SetRenderDrawColor(target, static_cast<byte>(getBitRange(color, 16, 8)),
                           static_cast<byte>(getBitRange(color, 8, 8)),
                           static_cast<byte>(getBitRange(color, 0, 8)), 255);
    SDL_RenderPoint(target, x, y);
}

void PPU::render_debug() {
    init_debug_window();

    byte wx = cached_WX, wy = cached_WY, scx = cached_SCX, scy = cached_SCY;

    for (int i = 0; i < 256; ++i) {
        for (int j = 0; j < 256; ++j) {
            half bg_tilex = (scx + i - 6) % 256, bg_tiley = (scy + j) % 256;
            const BgPixel pixel = sample_bg_pixel(bg_tilex, bg_tiley);
            const uint32_t color =
                bus->get_bg_color(pixel.palette, pixel.color);
            SDL_SetRenderDrawColor(debug_bg_renderer,
                                   static_cast<byte>(getBitRange(color, 16, 8)),
                                   static_cast<byte>(getBitRange(color, 8, 8)),
                                   static_cast<byte>(getBitRange(color, 0, 8)),
                                   255);
            SDL_RenderPoint(debug_bg_renderer, i, j);
        }
    }

    SDL_RenderPresent(debug_object_renderer);
    SDL_RenderPresent(debug_window_renderer);
    SDL_RenderPresent(debug_bg_renderer);
}
#endif

#if GBC_PPU_DEBUG && !defined(__EMSCRIPTEN__)
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
#endif

byte PPU::read_register(half address) const {
    switch (address) {
        case addr(VideoRegister::LCDC):
            return reg_LCDC;
        case addr(VideoRegister::STAT):
            return reg_STAT | (lyc_match << 2);
        case addr(VideoRegister::SCY):
            return reg_SCY;
        case addr(VideoRegister::SCX):
            return reg_SCX;
        case addr(VideoRegister::LY):
            return lines;
        case addr(VideoRegister::LYC):
            return reg_LYC;
        case addr(VideoRegister::BGP):
            return reg_BGP;
        case addr(VideoRegister::OBP0):
            return reg_OBP0;
        case addr(VideoRegister::OBP1):
            return reg_OBP1;
        case addr(VideoRegister::WY):
            return reg_WY;
        case addr(VideoRegister::WX):
            return reg_WX;
        default:
            return 0xFF;
    }
}

void PPU::write_register(half address, byte value) {
    switch (address) {
        case addr(VideoRegister::LCDC):
            if (!isBitSet(value, 5) && isBitSet(reg_LCDC, 5)) {
                wly = 0;
                wlyenabled = false;
            }
            reg_LCDC = value;
            cache_dirty = true;
            break;
        case addr(VideoRegister::STAT):
            reg_STAT = (value & 0xF8) | (reg_STAT & 0x07);
            break;
        case addr(VideoRegister::SCY):
            reg_SCY = value;
            cache_dirty = true;
            break;
        case addr(VideoRegister::SCX):
            reg_SCX = value;
            cache_dirty = true;
            break;
        case addr(VideoRegister::LY):
            break;
        case addr(VideoRegister::LYC):
            reg_LYC = value;
            cache_dirty = true;
            break;
        case addr(VideoRegister::BGP):
            reg_BGP = value;
            cache_dirty = true;
            break;
        case addr(VideoRegister::OBP0):
            reg_OBP0 = value;
            cache_dirty = true;
            break;
        case addr(VideoRegister::OBP1):
            reg_OBP1 = value;
            cache_dirty = true;
            break;
        case addr(VideoRegister::WY):
            reg_WY = value;
            wly = 0;
            wlyenabled = false;
            cache_dirty = true;
            break;
        case addr(VideoRegister::WX):
            reg_WX = value;
            cache_dirty = true;
            break;
    }
}
}  // namespace GBC