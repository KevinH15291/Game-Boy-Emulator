#include "PPU.h"

#include <algorithm>
#include <ios>
#include <iostream>

#include "bus.h"

namespace GBC {
void PPU::init_window() {
#ifdef __EMSCRIPTEN__
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    window = SDL_CreateWindow("(GBC) hello window", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH * 2,
                              WINDOW_HEIGHT * 2, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
#else
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_CreateWindowAndRenderer("(GBC) hello window", WINDOW_WIDTH * 4,
                                WINDOW_HEIGHT * 4, SDL_WINDOW_RESIZABLE,
                                &window, &renderer);
    SDL_SetRenderScale(renderer, 4, 4);
#endif
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
}

void PPU::init_canvas() {
#ifdef __EMSCRIPTEN__
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    window =
        SDL_CreateWindow("CGB", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);
#else
    init_window();
#endif
}

PPU::~PPU() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void PPU::execute_cycle() {
    if (dots >= 456) ++lines;
    dots %= 456;
    lines %= 154;

    if ((bus->read(addr(VideoRegister::LCDC)) & 0x80) == 0) {
        bus->IOrange[addr(VideoRegister::STAT) -
                     addr(MemoryRegion::IO_REGISTERS)] =
            (bus->IOrange[addr(VideoRegister::STAT) -
                          addr(MemoryRegion::IO_REGISTERS)] &
             0xF8) |
            0 | ((lines == bus->read(addr(VideoRegister::LYC))) << 2);
        return;
    }
    bus->write(addr(IORegister::IF),
               bus->read(addr(IORegister::IF)) & ~(1 << 1));
    bus->IOrange[addr(VideoRegister::LY) - addr(MemoryRegion::IO_REGISTERS)] =
        lines;
    if (bus->read(addr(VideoRegister::STAT)) & (1 << 6) &&
        (lines == bus->read(addr(VideoRegister::LYC))))
        bus->write(addr(IORegister::IF),
                   bus->read(addr(IORegister::IF)) | (1 << 1));

    if (lines < 144) {
        if ((dots % 456) < 80) {
            if (mode != RenderingState::OAMscan) {
                objnum = 0;
                mode = RenderingState::OAMscan;
                byte objsize =
                    (bus->read(addr(VideoRegister::LCDC)) & (1 << 2)) ? 16 : 8;

                for (int16_t i = 0x00; i < 0x9F; i += 4) {
                    byte objy = bus->read_privileged(0xFE00 + i) - 16,
                         objx = bus->read_privileged(0xFE00 + i + 1),
                         index = bus->read_privileged(0xFE00 + i + 2),
                         flags = bus->read_privileged(0xFE00 + i + 3);
                    if (objy <= lines && objy + objsize > lines) {
                        objbuffer[objnum].objx = objx,
                        objbuffer[objnum].objy = objy,
                        objbuffer[objnum].index = index,
                        objbuffer[objnum].flags = flags;
                        ++objnum;
                    }

                    if (objnum == 10) break;
                }
                std::stable_sort(objbuffer.begin(),
                                 objbuffer.begin() + static_cast<int>(objnum),
                                 [](obj a, obj b) { return a.objx < b.objx; });
                bus->IOrange[addr(VideoRegister::STAT) -
                             addr(MemoryRegion::IO_REGISTERS)] =
                    (bus->IOrange[addr(VideoRegister::STAT) -
                                  addr(MemoryRegion::IO_REGISTERS)] &
                     0xF8) |
                    2 | ((lines == bus->read(addr(VideoRegister::LYC))) << 2);
                if (bus->read(addr(VideoRegister::STAT)) & (1 << 5))
                    bus->write(addr(IORegister::IF),
                               bus->read(addr(IORegister::IF)) |
                                   (1 << 1));  // TODO extract into function
            }

        } else if ((dots % 456) < 252) {
            if (mode != RenderingState::draw) {
                mode = RenderingState::draw;
                wlyenabled = false;
                debug_callback = true;
                bus->IOrange[addr(VideoRegister::STAT) -
                             addr(MemoryRegion::IO_REGISTERS)] =
                    (bus->IOrange[addr(VideoRegister::STAT) -
                                  addr(MemoryRegion::IO_REGISTERS)] &
                     0xF8) |
                    3 | ((lines == bus->read(addr(VideoRegister::LYC))) << 2);
            }

            draw_pixel();
        } else {
            if (mode != RenderingState::hblank) {
                mode = RenderingState::hblank;
                renderX = 0;
                if (wlyenabled) {
                    ++wly;
                }
                bus->IOrange[addr(VideoRegister::STAT) -
                             addr(MemoryRegion::IO_REGISTERS)] =
                    (bus->IOrange[addr(VideoRegister::STAT) -
                                  addr(MemoryRegion::IO_REGISTERS)] &
                     0xF8) |
                    0 | ((lines == bus->read(addr(VideoRegister::LYC))) << 2);

                if (bus->read(addr(VideoRegister::STAT)) & (1 << 3))
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
            SDL_RenderPresent(renderer);
            if (bus->read(addr(VideoRegister::STAT)) & (1 << 4))
                bus->write(addr(IORegister::IF),
                           bus->read(addr(IORegister::IF)) | (1 << 1));
            bus->IOrange[addr(VideoRegister::STAT) -
                         addr(MemoryRegion::IO_REGISTERS)] =
                (bus->IOrange[addr(VideoRegister::STAT) -
                              addr(MemoryRegion::IO_REGISTERS)] &
                 0xF8) |
                1 | ((lines == bus->read(addr(VideoRegister::LYC))) << 2);
        }
    }
    // bus->lcd_mode = mode; TODO, activate this, has been buggy so far

    ++dots;
}

// TODO clean this up, and fix it
void PPU::draw_pixel() {
    byte lcdc = bus->read(addr(VideoRegister::LCDC));
    byte bgenable = lcdc & 1;
    byte objenable = lcdc & (1 << 1);
    byte windowenable = lcdc & (1 << 5);

    byte wx = bus->read(addr(VideoRegister::WX));
    byte wy = bus->read(addr(VideoRegister::WY));
    byte scx = bus->read(addr(VideoRegister::SCX));
    byte scy = bus->read(addr(VideoRegister::SCY));

    if (windowenable && bgenable && !wlyenabled &&
        ((renderX + 1 >= wx) && (lines >= wy))) {
        wlyenabled = true;
    }

    half bg_tilex = (scx + renderX - 6) % 256;
    half bg_tiley = (scy + lines) % 256;
    half window_tilex = renderX - wx + 1;
    half window_tiley = wly;

    byte objpix = objFIFO();
    byte winpix = windowFIFO(window_tilex, window_tiley);
    byte bgpix = bgFIFO(bg_tilex, bg_tiley);
    byte winbg = 0;
    byte pixel = 0;

    if (bgenable) {
        if (windowenable && ((renderX + 1 >= wx) && (lines >= wy))) {
            winbg = (bus->read(addr(VideoRegister::BGP)) >> (winpix * 2)) & 0x3;
        } else {
            winbg = (bus->read(addr(VideoRegister::BGP)) >> (bgpix * 2)) & 0x3;
        }
    } else {
        winbg = 0;
    }

    if (objenable) {
        if ((objpix == 0 || (objpix & 0x80)) && (winbg != 0)) {
            pixel = winbg;
        } else {
            pixel = (((objpix >> 4) & 1)
                         ? bus->read_privileged(addr(VideoRegister::OBP1))
                         : bus->read_privileged(addr(VideoRegister::OBP0))) >>
                        ((objpix & 0x03) * 2) &
                    0x3;
        }
    } else {
        pixel = winbg;
    }

#ifdef _DEBUG
    byte objpal_debug =
        (((objpix >> 4) & 1
              ? bus->read_privileged(addr(VideoRegister::OBP1))
              : bus->read_privileged(addr(VideoRegister::OBP0))) >>
         ((objpix & 0x03) * 2)) &
        0x3;
    SDL_SetRenderDrawColor(debug_object_renderer,
                           255 - (255.0 / 3) * (objpal_debug),
                           255 - (255.0 / 3) * (objpal_debug),
                           255 - (255.0 / 3) * (objpal_debug), 255);
    SDL_RenderPoint(debug_object_renderer, renderX - 6, lines);

    byte winpal_debug =
        (bus->read(addr(VideoRegister::BGP)) >> (winpix * 2)) & 0x3;
    SDL_SetRenderDrawColor(debug_window_renderer,
                           255 - (255.0 / 3) * (winpal_debug),
                           255 - (255.0 / 3) * (winpal_debug),
                           255 - (255.0 / 3) * (winpal_debug), 255);
    SDL_RenderPoint(debug_window_renderer, renderX - 6, lines);
#endif

    SDL_SetRenderDrawColor(renderer, 255 - (255.0 / 3) * (pixel),
                           255 - (255.0 / 3) * (pixel),
                           255 - (255.0 / 3) * (pixel), 255);
    SDL_RenderDrawPoint(renderer, renderX++ - 6, lines);
}

// TODO fix issues
inline byte PPU::objFIFO() {
    byte tilei = 255;
    byte objsize = (bus->read(addr(VideoRegister::LCDC)) & (1 << 2)) ? 16 : 8;

    for (int i = static_cast<int>(objnum) - 1; i >= 0; --i) {
        const obj cand_tile = objbuffer[i];

        if (cand_tile.objx - 2 <= renderX && cand_tile.objx + 6 > renderX)
            tilei = i;
    }

    if (tilei == 255) return 0;

    obj tile = objbuffer[tilei];

    byte objy = tile.objy;
    byte objx = tile.objx - 2;
    byte flags = tile.flags;
    byte index = tile.index;

    if (objsize == 16) {
        index &= 0xFE;
    }

    byte palette = flags & (1 << 4);
    byte Xflip = flags & (1 << 5);
    byte Yflip = flags & (1 << 6);
    byte prio = flags & (1 << 7);

    half tile_address =
        0x8000 + index * 16 +
        (Yflip ? (objsize - 1 - (lines - objy)) * 2 : (lines - objy) * 2);

    byte tilelow = bus->read(tile_address);
    byte tilehigh = bus->read(tile_address + 1);

    byte shift =
        static_cast<byte>(Xflip ? (renderX - objx) : 7 - (renderX - objx));
    byte object_pixel = (((1 << shift) & tilelow) != 0) |
                        ((((1 << shift) & tilehigh) != 0) << 1);

    byte object_pixel_with_flags = object_pixel | prio | palette;
    return object_pixel_with_flags;
}

inline byte PPU::bgFIFO(half tilex, half tiley) {
    byte lcdc = bus->read(addr(VideoRegister::LCDC));

    half BG_tile_map = (lcdc & (1 << 3)) ? 0x9C00 : 0x9800;
    byte data_area = lcdc & (1 << 4);

    half tile_index_index = (tilex / 8) + (tiley / 8) * 32 + BG_tile_map;

    byte tile_index = bus->read(tile_index_index);

    byte tilelow;
    byte tilehigh;

    if (data_area) {
        tilelow = bus->read(tile_index * 16 + (tiley % 8) * 2 + 0x8000);
        tilehigh = bus->read(tile_index * 16 + (tiley % 8) * 2 + 1 + 0x8000);
    } else {
        tilelow = bus->read((tiley % 8) * 2 + 0x9000 +
                            static_cast<int8_t>(tile_index) * 16);
        tilehigh = bus->read((tiley % 8) * 2 + 1 + 0x9000 +
                             static_cast<int8_t>(tile_index) * 16);
    }

    byte bit = static_cast<byte>(7 - tilex % 8);
    byte final_pixel =
        (((1 << bit) & tilelow) != 0) | ((((1 << bit) & tilehigh) != 0) << 1);
    return final_pixel;
}

inline byte PPU::windowFIFO(half tilex, half tiley) {
    byte lcdc = bus->read(addr(VideoRegister::LCDC));

    half w_tile_map = (lcdc & (1 << 6)) ? 0x9C00 : 0x9800;
    byte data_area = lcdc & (1 << 4);

    half tile_index_index = (tilex / 8) + (tiley / 8) * 32;

    byte tile_index = bus->read(tile_index_index + w_tile_map);

    byte tilelow;
    byte tilehigh;

    if (data_area) {
        tilelow = bus->read(tile_index * 16 + (tiley % 8) * 2 + 0x8000);
        tilehigh = bus->read(tile_index * 16 + (tiley % 8) * 2 + 1 + 0x8000);
    } else {
        if (tile_index <= 127) {
            tilelow = bus->read((tiley % 8) * 2 + 0x9000 + tile_index * 16);
            tilehigh =
                bus->read((tiley % 8) * 2 + 1 + 0x9000 + tile_index * 16);
        } else {
            tilelow = bus->read((tiley % 8) * 2 + 0x9000 +
                                static_cast<int8_t>(tile_index) * 16);
            tilehigh = bus->read((tiley % 8) * 2 + 1 + 0x9000 +
                                 static_cast<int8_t>(tile_index) * 16);
        }
    }

    byte bit = static_cast<byte>(7 - tilex % 8);
    byte final_pixel =
        (((1 << bit) & tilelow) != 0) | ((((1 << bit) & tilehigh) != 0) << 1);

    return final_pixel;
}
#ifdef _DEBUG
void PPU::render_debug() {
    if (!debug_render) init_debug_window();
    // for (int i = 0; i < 382; ++i) {
    //     for (int j = 0; j < 8; ++j) {
    //         byte a = bus->read(0x8000+i*16+j*2), b =
    //         bus->read(0x8000+i*16+j*2+1); for (int k = 0; k < 8; ++k) {
    //             int temp = (((1 << (7-k)) & a) != 0) | ((((1 << (7-k)) & b)
    //             != 0) << 1); SDL_SetRenderDrawColor(debug_tile_renderer,
    //             255-255.0/3*(temp), 255-255.0/3*(temp), 255-255.0/3*(temp),
    //             255); SDL_RenderPoint(debug_tile_renderer, (i%16)*8+k,
    //             int(i/16)*8+j);
    //         }
    //     }
    // }
    // SDL_RenderPresent(debug_tile_renderer);

    byte wx = bus->read(addr(VideoRegister::WX)),
         wy = bus->read(addr(VideoRegister::WY)),
         scx = bus->read(addr(VideoRegister::SCX)),
         scy = bus->read(addr(VideoRegister::SCY));

    for (int i = 0; i < 256; ++i) {
        for (int j = 0; j < 256; ++j) {
            half bg_tilex = (scx + i - 6) % 256, bg_tiley = (scy + j) % 256;
            int temp = bgFIFO(bg_tilex, bg_tiley);
            SDL_SetRenderDrawColor(debug_bg_renderer, 255 - 255.0 / 3 * (temp),
                                   255 - 255.0 / 3 * (temp),
                                   255 - 255.0 / 3 * (temp), 255);
            SDL_RenderPoint(debug_bg_renderer, i, j);
        }
    }

    SDL_RenderPresent(debug_object_renderer);
    SDL_RenderPresent(debug_window_renderer);
    SDL_RenderPresent(debug_bg_renderer);
}
#endif

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

PPU::PPU(address_bus *bus) : bus(bus) {}
}  // namespace GBC