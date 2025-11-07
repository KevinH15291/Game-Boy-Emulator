#include "PPU.h"

#include <algorithm>
#include <iostream>

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

void PPU::init_debug_window() {
#ifdef __EMSCRIPTEN__
    // Debug windows disabled for web build
    debug_render = false;
#else
    if (!debug_render) {
        debug_render = true;
        SDL_Init(SDL_INIT_VIDEO);
        SDL_CreateWindowAndRenderer("(GBC) hello tile window", 128 * 4, 192 * 4,
                                    SDL_WINDOW_RESIZABLE, &debug_tile_window,
                                    &debug_tile_renderer);
        SDL_SetRenderScale(debug_tile_renderer, 4, 4);
        SDL_SetRenderDrawColor(debug_tile_renderer, 255, 255, 255, 255);
        SDL_RenderClear(debug_tile_renderer);
        SDL_Init(SDL_INIT_VIDEO);
        SDL_CreateWindowAndRenderer("(GBC) hello background window", 256 * 2,
                                    256 * 2, SDL_WINDOW_RESIZABLE,
                                    &debug_bg_window, &debug_bg_renderer);
        SDL_SetRenderScale(debug_bg_renderer, 2, 2);
        SDL_SetRenderDrawColor(debug_bg_renderer, 255, 255, 255, 255);
        SDL_RenderClear(debug_bg_renderer);
        SDL_CreateWindowAndRenderer("(GBC) hello window window",
                                    WINDOW_WIDTH * 4, WINDOW_HEIGHT * 4,
                                    SDL_WINDOW_RESIZABLE, &debug_window_window,
                                    &debug_window_renderer);
        SDL_SetRenderScale(debug_window_renderer, 4, 4);
        SDL_SetRenderDrawColor(debug_window_renderer, 255, 255, 255, 255);
        SDL_RenderClear(debug_window_renderer);
        SDL_Init(SDL_INIT_VIDEO);
        SDL_CreateWindowAndRenderer("(GBC) hello object window",
                                    WINDOW_WIDTH * 4, WINDOW_HEIGHT * 4,
                                    SDL_WINDOW_RESIZABLE, &debug_object_window,
                                    &debug_object_renderer);
        SDL_SetRenderScale(debug_object_renderer, 4, 4);
        SDL_SetRenderDrawColor(debug_object_renderer, 255, 255, 255, 255);
        SDL_RenderClear(debug_object_renderer);
    }
#endif
}

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
    update_palette_luts();
}

inline void PPU::update_palette_luts() {
    for (int i = 0; i < 4; ++i) {
        obp_lut[0][i] = (cached_OBP0 >> (i * 2)) & 0x3;
        obp_lut[1][i] = (cached_OBP1 >> (i * 2)) & 0x3;
    }
}

inline void PPU::cache_bg_tile_row(uint8_t tile_index, uint8_t tiley_mod8) {
    uint16_t cache_key_full =
        (static_cast<uint16_t>(tile_index) << 3) | (tiley_mod8 & 0x7);
    uint8_t cache_key = cache_key_full & 0xFF;

    if (bg_tile_cache_valid[cache_key] &&
        bg_tile_cache_key[cache_key] == cache_key_full) {
        return;
    }

    byte data_area = (cached_LCDC & (1 << 4));
    if (data_area != 0) {
        bg_tile_row_cache[cache_key][0] = bus->read(
            tile_index * 16 + tiley_mod8 * 2 + addr(MemoryRegion::VIDEO_RAM));
        bg_tile_row_cache[cache_key][1] =
            bus->read(tile_index * 16 + tiley_mod8 * 2 + 1 +
                      addr(MemoryRegion::VIDEO_RAM));
        bg_tile_cache_key[cache_key] = cache_key_full;
        bg_tile_cache_valid[cache_key] = true;
    } else {
        bg_tile_row_cache[cache_key][0] =
            bus->read(tiley_mod8 * 2 + 0x9000 + ((int8_t)tile_index) * 16);
        bg_tile_row_cache[cache_key][1] =
            bus->read(tiley_mod8 * 2 + 1 + 0x9000 + ((int8_t)tile_index) * 16);
        bg_tile_cache_key[cache_key] = cache_key_full;
        bg_tile_cache_valid[cache_key] = true;
    }
}

inline void PPU::cache_window_tile_row(uint8_t tile_index, uint8_t tiley_mod8) {
    uint16_t cache_key_full =
        (static_cast<uint16_t>(tile_index) << 3) | (tiley_mod8 & 0x7);
    uint8_t cache_key = cache_key_full & 0xFF;

    if (window_tile_cache_valid[cache_key] &&
        window_tile_cache_key[cache_key] == cache_key_full) {
        return;
    }

    byte data_area = (cached_LCDC & (1 << 4));
    if (data_area) {
        window_tile_row_cache[cache_key][0] = bus->read(
            tile_index * 16 + tiley_mod8 * 2 + addr(MemoryRegion::VIDEO_RAM));
        window_tile_row_cache[cache_key][1] =
            bus->read(tile_index * 16 + tiley_mod8 * 2 + 1 +
                      addr(MemoryRegion::VIDEO_RAM));
        window_tile_cache_key[cache_key] = cache_key_full;
        window_tile_cache_valid[cache_key] = true;
    } else {
        window_tile_row_cache[cache_key][0] =
            bus->read(tiley_mod8 * 2 + 0x9000 + ((int8_t)tile_index) * 16);
        window_tile_row_cache[cache_key][1] =
            bus->read(tiley_mod8 * 2 + 1 + 0x9000 + ((int8_t)tile_index) * 16);
        window_tile_cache_key[cache_key] = cache_key_full;
        window_tile_cache_valid[cache_key] = true;
    }
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

    if ((cached_LCDC & 0x80) == 0) {
        reg_STAT = (reg_STAT & 0xF8) | 0 | (lyc_match << 2);
        return;
    }
    bus->write(addr(IORegister::IF),
               bus->read(addr(IORegister::IF)) & ~(1 << 1));
    if (reg_STAT & (1 << 6) && lyc_match)
        bus->write(addr(IORegister::IF),
                   bus->read(addr(IORegister::IF)) | (1 << 1));

    if (lines < 144) {
        if (dots < 80) {
            if (mode != RenderingState::OAMscan) {
                objnum = 0;
                mode = RenderingState::OAMscan;
                byte objsize = (cached_LCDC & (1 << 2)) ? 16 : 8;

                for (int16_t i = 0x00; i < 0x9F; i += 4) {
                    byte objy = bus->read_privileged(
                                    addr(MemoryRegion::OAMaddress) + i) -
                                16,
                         objx = bus->read_privileged(
                             addr(MemoryRegion::OAMaddress) + i + 1),
                         index = bus->read_privileged(
                             addr(MemoryRegion::OAMaddress) + i + 2),
                         flags = bus->read_privileged(
                             addr(MemoryRegion::OAMaddress) + i + 3);
                    if (objy <= lines && objy + objsize > lines) {
                        objbuffer[objnum].objx = objx,
                        objbuffer[objnum].objy = objy,
                        objbuffer[objnum].index = index,
                        objbuffer[objnum].flags = flags;
                        ++objnum;
                    }

                    if (objnum == 10) break;
                }
                std::sort(objbuffer, objbuffer + objnum,
                          [](obj a, obj b) { return a.objx < b.objx; });
                reg_STAT = (reg_STAT & 0xF8) | 2 | (lyc_match << 2);
                if (reg_STAT & (1 << 5))
                    bus->write(addr(IORegister::IF),
                               bus->read(addr(IORegister::IF)) | (1 << 1));
            }

        } else if (dots < 252) {
            if (mode != RenderingState::draw) {
                mode = RenderingState::draw;
                renderX = 0;  // Reset renderX at start of draw phase
                wlyenabled = false;
                for (int i = 0; i < 256; ++i) {
                    bg_tile_cache_valid[i] = false;
                    bg_tile_cache_key[i] = 0;
                    window_tile_cache_valid[i] = false;
                    window_tile_cache_key[i] = 0;
                }
#ifndef NDEBUG
                debug_callback = true;
#endif
                reg_STAT = (reg_STAT & 0xF8) | 3 | (lyc_match << 2);
            }

            draw_pixel();
        } else {
            if (mode != RenderingState::hblank) {
                mode = RenderingState::hblank;
                renderX = 0;
                if (wlyenabled) {
                    ++wly;
                }
                reg_STAT = (reg_STAT & 0xF8) | 0 | (lyc_match << 2);

                if (reg_STAT & (1 << 3))
                    bus->write(addr(IORegister::IF),
                               bus->read(addr(IORegister::IF)) | (1 << 1));
            }
        }
    } else {
        if (mode != RenderingState::vblank) {
            mode = RenderingState::vblank;
            wly = 0;
            bus->write(addr(IORegister::IF),
                       bus->read(addr(IORegister::IF)) | 1);
            if (texture) {
                void* pixels;
                int pitch;
                SDL_LockTexture(texture, nullptr, &pixels, &pitch);
                uint8_t* pixel_data = static_cast<uint8_t*>(pixels);
                for (int y = 0; y < WINDOW_HEIGHT; ++y) {
                    for (int x = 0; x < WINDOW_WIDTH; ++x) {
                        uint8_t gray = frame[y * WINDOW_WIDTH + x];
                        pixel_data[y * pitch + x * 3] = gray;
                        pixel_data[y * pitch + x * 3 + 1] = gray;
                        pixel_data[y * pitch + x * 3 + 2] = gray;
                    }
                }
                SDL_UnlockTexture(texture);
#ifdef __EMSCRIPTEN__
                SDL_RenderCopy(renderer, texture, nullptr, nullptr);
#else
                SDL_RenderTexture(renderer, texture, nullptr, nullptr);
#endif
                SDL_RenderPresent(renderer);
            } else {
                SDL_RenderPresent(renderer);
            }
            if (reg_STAT & (1 << 4))
                bus->write(addr(IORegister::IF),
                           bus->read(addr(IORegister::IF)) | (1 << 1));
            reg_STAT = (reg_STAT & 0xF8) | 1 | (lyc_match << 2);
        }
    }

    ++dots;
}

// TODO clean this up, and fix it
void PPU::draw_pixel() {
    byte bgenable = cached_LCDC & 1, objenable = cached_LCDC & (1 << 1),
         windowenable = cached_LCDC & (1 << 5);

    byte wx = cached_WX, wy = cached_WY, scx = cached_SCX, scy = cached_SCY;

    if (windowenable && bgenable && !wlyenabled &&
        ((renderX + 1 >= wx) && (lines >= wy))) {
        wlyenabled = true;
    }

    half bg_tilex = (scx + renderX - 6) % 256, bg_tiley = (scy + lines) % 256,
         window_tilex = renderX - wx + 1, window_tiley = wly;

    byte objpal = objFIFO(), objpix = 0,
         winpal = windowFIFO(window_tilex, window_tiley),
         bgpal = bgFIFO(bg_tilex, bg_tiley), winbgpal = 0, winbgpix = 0,
         pixel = 0, choice = 0;

    if (bgenable) {
        if (windowenable && ((renderX + 1 >= wx) && (lines >= wy))) {
            winbgpal = winpal;
        } else {
            winbgpal = bgpal;
        }
    } else {
        winbgpal = 0;
    }

    objpix = obp_lut[(objpal >> 4) & 1][objpal & 0x3];
    winbgpix = (cached_BGP >> (winbgpal * 2)) & 0x3;

    if (objenable) {
        bool has_sprite = (objpal != (1 << 5));
        bool sprite_transparent = ((objpal & 0x3) == 0);
        bool sprite_priority = ((objpal & 0x80) != 0);
        bool bg_color_zero = (winbgpix == 0);

        if (!has_sprite || sprite_transparent) {
            pixel = winbgpix;
        } else if (!bgenable) {
            pixel = objpix;
        } else if (sprite_priority && !bg_color_zero) {
            pixel = winbgpix;
        } else {
            pixel = objpix;
        }
    } else {
        pixel = winbgpix;
    }

    if (debug_render) {
        uint8_t objpal_color = obp_lut[(objpal >> 4) & 1][objpal & 0x3];
        SDL_SetRenderDrawColor(debug_object_renderer, palette_lut[objpal_color],
                               palette_lut[objpal_color],
                               palette_lut[objpal_color], 255);
#ifdef __EMSCRIPTEN__
        SDL_RenderDrawPoint(debug_object_renderer, renderX - 6, lines);
#else
        SDL_RenderPoint(debug_object_renderer, renderX - 6, lines);
#endif

        uint8_t winpal_color = (cached_BGP >> (winpal * 2)) & 0x3;
        SDL_SetRenderDrawColor(debug_window_renderer, palette_lut[winpal_color],
                               palette_lut[winpal_color],
                               palette_lut[winpal_color], 255);
#ifdef __EMSCRIPTEN__
        SDL_RenderDrawPoint(debug_window_renderer, renderX - 6, lines);
#else
        SDL_RenderPoint(debug_window_renderer, renderX - 6, lines);
#endif
    }

    // Game Boy has a 6-pixel pipeline delay, so renderX starts at 0 but first
    // visible pixel is at renderX=6 We write pixels starting from renderX=6
    // (which maps to x=0 on screen)
    int32_t screen_x = renderX - 6;
    if (screen_x >= 0 && screen_x < static_cast<int32_t>(WINDOW_WIDTH) &&
        lines < WINDOW_HEIGHT) {
        uint32_t frame_index = lines * WINDOW_WIDTH + screen_x;
        if (frame_index < WINDOW_WIDTH * WINDOW_HEIGHT) {
            frame[frame_index] = palette_lut[pixel];
        }
    }
    ++renderX;
}

// TODO fix issues
inline byte PPU::objFIFO() {
    byte tilei = 255;
    byte objsize = (cached_LCDC & (1 << 2)) ? 16 : 8;

    uint8_t tilex = 0;

    for (int i = (int)objnum - 1; i >= 0; --i) {
        obj cand_tile = objbuffer[i];

        if (cand_tile.objx - 2 <= renderX && cand_tile.objx + 6 > renderX)
            tilei = i;
    }

    if (tilei == 255) return (1 << 5);

    obj tile = objbuffer[tilei];

    byte objy = tile.objy, objx = tile.objx - 2, flags = tile.flags,
         index = tile.index;

    if (objsize == 16) {
        index = (index & 0xFE);
    }

    byte palette = flags & (1 << 4), Xflip = flags & (1 << 5),
         Yflip = flags & (1 << 6), prio = flags & (1 << 7);

    half tile_address =
        addr(MemoryRegion::VIDEO_RAM) + index * 16 +
        (Yflip ? (objsize - 1 - (lines - objy)) * 2 : (lines - objy) * 2);

    byte tilelow = bus->read(tile_address),
         tilehigh = bus->read(tile_address + 1);

    byte object_pixel =
        (((1 << (Xflip ? (renderX - objx) : 7 - (renderX - objx))) & tilelow) !=
         0) |
        ((((1 << (Xflip ? (renderX - objx) : 7 - (renderX - objx))) &
           tilehigh) != 0)
         << 1);

    byte object_pixel_with_flags = object_pixel | prio | palette;
    return object_pixel_with_flags;
}

inline byte PPU::bgFIFO(half tilex, half tiley) {
    byte bgenable = cached_LCDC & 1;

    half BG_tile_map = (cached_LCDC & (1 << 3)) ? 0x9C00 : 0x9800;
    byte data_area = (cached_LCDC & (1 << 4));

    half tile_index_index =
        ((tilex / 8) & 31) + (((tiley / 8) & 31) * 32) + BG_tile_map;

    byte tile_index = bus->read(tile_index_index);

    uint8_t tiley_mod8 = tiley & 7;
    uint8_t tilex_mod8 = tilex & 7;

    cache_bg_tile_row(tile_index, tiley_mod8);
    uint16_t cache_key_full =
        (static_cast<uint16_t>(tile_index) << 3) | tiley_mod8;
    uint8_t cache_key = cache_key_full & 0xFF;

    byte tilelow, tilehigh;
    if (bg_tile_cache_valid[cache_key] &&
        bg_tile_cache_key[cache_key] == cache_key_full) {
        tilelow = bg_tile_row_cache[cache_key][0];
        tilehigh = bg_tile_row_cache[cache_key][1];
    } else {
        byte data_area = (cached_LCDC & (1 << 4));
        if (data_area != 0) {
            tilelow = bus->read(tile_index * 16 + tiley_mod8 * 2 +
                                addr(MemoryRegion::VIDEO_RAM));
            tilehigh = bus->read(tile_index * 16 + tiley_mod8 * 2 + 1 +
                                 addr(MemoryRegion::VIDEO_RAM));
        } else {
            tilelow =
                bus->read(tiley_mod8 * 2 + 0x9000 + ((int8_t)tile_index) * 16);
            tilehigh = bus->read(tiley_mod8 * 2 + 1 + 0x9000 +
                                 ((int8_t)tile_index) * 16);
        }
    }

    byte final_pixel = (((1 << (7 - tilex_mod8)) & tilelow) != 0) |
                       ((((1 << (7 - tilex_mod8)) & tilehigh) != 0) << 1);
    return final_pixel;
}

inline byte PPU::windowFIFO(half tilex, half tiley) {
    half w_tile_map = (cached_LCDC & (1 << 6)) ? 0x9C00 : 0x9800;
    byte data_area = (cached_LCDC & (1 << 4));

    half tile_index_index = ((tilex / 8) & 31) + (((tiley / 8) & 31) * 32);

    byte tile_index = bus->read(tile_index_index + w_tile_map);

    uint8_t tiley_mod8 = tiley & 7;
    uint8_t tilex_mod8 = tilex & 7;

    cache_window_tile_row(tile_index, tiley_mod8);
    uint16_t cache_key_full =
        (static_cast<uint16_t>(tile_index) << 3) | tiley_mod8;
    uint8_t cache_key = cache_key_full & 0xFF;

    byte tilelow, tilehigh;
    if (window_tile_cache_valid[cache_key] &&
        window_tile_cache_key[cache_key] == cache_key_full) {
        tilelow = window_tile_row_cache[cache_key][0];
        tilehigh = window_tile_row_cache[cache_key][1];
    } else {
        byte data_area = (cached_LCDC & (1 << 4));
        if (data_area) {
            tilelow = bus->read(tile_index * 16 + tiley_mod8 * 2 +
                                addr(MemoryRegion::VIDEO_RAM));
            tilehigh = bus->read(tile_index * 16 + tiley_mod8 * 2 + 1 +
                                 addr(MemoryRegion::VIDEO_RAM));
        } else {
            tilelow =
                bus->read(tiley_mod8 * 2 + 0x9000 + ((int8_t)tile_index) * 16);
            tilehigh = bus->read(tiley_mod8 * 2 + 1 + 0x9000 +
                                 ((int8_t)tile_index) * 16);
        }
    }

    byte final_pixel = (((1 << (7 - tilex_mod8)) & tilelow) != 0) |
                       ((((1 << (7 - tilex_mod8)) & tilehigh) != 0) << 1);

    return final_pixel;
}

void PPU::render_debug() {
    if (!debug_render) init_debug_window();

    byte wx = cached_WX, wy = cached_WY, scx = cached_SCX, scy = cached_SCY;

    for (int i = 0; i < 256; ++i) {
        for (int j = 0; j < 256; ++j) {
            half bg_tilex = (scx + i - 6) % 256, bg_tiley = (scy + j) % 256;
            int temp = bgFIFO(bg_tilex, bg_tiley);
            uint8_t temp_color = (cached_BGP >> (temp * 2)) & 0x3;
            SDL_SetRenderDrawColor(debug_bg_renderer, palette_lut[temp_color],
                                   palette_lut[temp_color],
                                   palette_lut[temp_color], 255);
#ifdef __EMSCRIPTEN__
            SDL_RenderDrawPoint(debug_bg_renderer, i, j);
#else
            SDL_RenderPoint(debug_bg_renderer, i, j);
#endif
        }
    }

    SDL_RenderPresent(debug_object_renderer);
    SDL_RenderPresent(debug_window_renderer);
    SDL_RenderPresent(debug_bg_renderer);
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

uint8_t PPU::read_register(uint16_t address) const {
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

void PPU::write_register(uint16_t address, uint8_t value) {
    switch (address) {
        case addr(VideoRegister::LCDC):
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
            cache_dirty = true;
            break;
        case addr(VideoRegister::WX):
            reg_WX = value;
            cache_dirty = true;
            break;
    }
}
}  // namespace GBC