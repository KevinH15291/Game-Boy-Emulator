#include "PPU.h"
#include "bus.h"
#include <algorithm>

namespace GBC {
    void PPU::init_window() {
        SDL_Init(SDL_INIT_VIDEO);
        SDL_CreateWindowAndRenderer("(GBC) hello window", WINDOW_WIDTH*4, WINDOW_HEIGHT*4, SDL_WINDOW_RESIZABLE, &window, &renderer);
        SDL_SetRenderScale(renderer, 4, 4);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
    }

    void PPU::init_debug_window() {
        if (!debug_render) {
            debug_render = true;
        SDL_Init(SDL_INIT_VIDEO);
        SDL_CreateWindowAndRenderer("(GBC) hello tile window", 128*4, 192*4, SDL_WINDOW_RESIZABLE, &debug_tile_window, &debug_tile_renderer);
        SDL_SetRenderScale(debug_tile_renderer, 4, 4);
        SDL_SetRenderDrawColor(debug_tile_renderer, 255, 255, 255, 255);
        SDL_RenderClear(debug_tile_renderer);
        SDL_Init(SDL_INIT_VIDEO);
        SDL_CreateWindowAndRenderer("(GBC) hello background window", 256*2, 256*2, SDL_WINDOW_RESIZABLE, &debug_bg_window, &debug_bg_renderer);
        SDL_SetRenderScale(debug_bg_renderer, 2, 2);
        SDL_SetRenderDrawColor(debug_bg_renderer, 255, 255, 255, 255);
        SDL_RenderClear(debug_bg_renderer);
        SDL_CreateWindowAndRenderer("(GBC) hello window window", WINDOW_WIDTH*4, WINDOW_HEIGHT*4, SDL_WINDOW_RESIZABLE, &debug_window_window, &debug_window_renderer);
        SDL_SetRenderScale(debug_window_renderer, 4, 4);
        SDL_SetRenderDrawColor(debug_window_renderer, 255, 255, 255, 255);
        SDL_RenderClear(debug_window_renderer);
        SDL_Init(SDL_INIT_VIDEO);
        SDL_CreateWindowAndRenderer("(GBC) hello object window", WINDOW_WIDTH*4, WINDOW_HEIGHT*4, SDL_WINDOW_RESIZABLE, &debug_object_window, &debug_object_renderer);
        SDL_SetRenderScale(debug_object_renderer, 4, 4);
        SDL_SetRenderDrawColor(debug_object_renderer, 255, 255, 255, 255);
        SDL_RenderClear(debug_object_renderer);
        }
    }


    PPU::~PPU() {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    inline void PPU::update_register_cache() {
        cached_LCDC = bus->read(LCDC);
        cached_BGP = bus->read(BGP);
        cached_OBP0 = bus->read(OBP0);
        cached_OBP1 = bus->read(OBP1);
        cached_SCX = bus->read(SCX);
        cached_SCY = bus->read(SCY);
        cached_WX = bus->read(WX);
        cached_WY = bus->read(WY);
        cached_LYC = bus->read(LYC);
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

        if ((cached_LCDC&0x80) == 0) {
            bus->IOrange[STAT-IO_REGISTERS] = (bus->IOrange[STAT-IO_REGISTERS] & 0xF8) | 0 | (lyc_match << 2); 
            return;
        }
        bus->write(IF, bus->read(IF) & ~(1 << 1));
        bus->IOrange[LY-IO_REGISTERS] = lines;
        if (bus->read(STAT) & (1 << 6) && lyc_match) bus->write(IF, bus->read(IF) | (1 << 1));

        if (lines < 144) {
            if (dots < 80) {
                if (mode != OAMscan) {
                    objnum = 0;
                    mode = OAMscan; 
                    byte objsize = (cached_LCDC & (1 << 2)) ? 16 : 8;
                    
                    for (int16_t i = 0x00; i < 0x9F; i+=4) {
                        byte objy = bus->read_privledged(OAMaddress+i)-16,
                        objx = bus->read_privledged(OAMaddress+i+1),
                        index = bus->read_privledged(OAMaddress+i+2),
                        flags = bus->read_privledged(OAMaddress+i+3); 
                        if (objy <= lines && objy+objsize > lines) {
                            objbuffer[objnum].objx = objx,
                            objbuffer[objnum].objy = objy,
                            objbuffer[objnum].index = index,
                            objbuffer[objnum].flags = flags;
                            ++objnum;
                            
                        }
                        
                        if (objnum == 10) break;
                    }
                    std::sort(objbuffer, objbuffer+objnum, [](obj a, obj b){return a.objx < b.objx;});
                    bus->IOrange[STAT-IO_REGISTERS] = (bus->IOrange[STAT-IO_REGISTERS] & 0xF8) | 2 | (lyc_match << 2);  
                    if (bus->read(STAT) & (1 << 5)) bus->write(IF, bus->read(IF) | (1 << 1)); // TODO extract into function

                }

            } else if (dots < 252) {
                if (mode != draw) {
                    mode = draw;
                    wlyenabled = false;
#ifndef NDEBUG
                    debug_callback = true;
#endif
                    bus->IOrange[STAT-IO_REGISTERS] = (bus->IOrange[STAT-IO_REGISTERS] & 0xF8) | 3 | (lyc_match << 2);
                }

                draw_pixel();
            } else {
                if (mode != hblank) {
                    mode = hblank;
                    renderX = 0;
                    if (wlyenabled) {
                        ++wly;
                    }
                bus->IOrange[STAT-IO_REGISTERS] = (bus->IOrange[STAT-IO_REGISTERS] & 0xF8) | 0 | (lyc_match << 2);

                    if (bus->read(STAT) & (1 << 3)) bus->write(IF, bus->read(IF) | (1 << 1));
                }
                
            } 
        } else {
            if (mode != vblank) {
                mode = vblank;
                wly = 0;
                bus->write(IF, bus->read(IF) | 1);
                SDL_RenderPresent(renderer);
                if (bus->read(STAT) & (1 << 4)) bus->write(IF, bus->read(IF) | (1 << 1));
            bus->IOrange[STAT-IO_REGISTERS] = (bus->IOrange[STAT-IO_REGISTERS] & 0xF8) | 1 | (lyc_match << 2);

            }

        }
        // bus->lcd_mode = mode; TODO, activate this, has been buggy so far
        
        ++dots;
    }

    // TODO clean this up, and fix it
    void PPU::draw_pixel() {
        byte bgenable = cached_LCDC & 1,
             objenable = cached_LCDC & (1 << 1),
             windowenable = cached_LCDC & (1 << 5);

        byte wx = cached_WX,
             wy = cached_WY,
             scx = cached_SCX,
             scy = cached_SCY;

        if (windowenable && bgenable && !wlyenabled && ((renderX+1 >= wx) && (lines >= wy))) {
            wlyenabled = true;
        }
        
        half bg_tilex = (scx+renderX-6)%256, bg_tiley = (scy+lines)%256,
             window_tilex = renderX-wx+1, window_tiley = wly; 

        byte objpal = objFIFO(), 
             objpix = 0,
             winpal = windowFIFO(window_tilex, window_tiley),
             bgpal = bgFIFO(bg_tilex, bg_tiley),
             winbgpal = 0, 
             winbgpix = 0,
             pixel = 0,
             choice = 0;


        if (bgenable) {
            if (windowenable && ((renderX+1 >= wx) && (lines >= wy))){
                winbgpal = winpal;
            } else {
                winbgpal = bgpal;
            }   
        } else {
            winbgpal = 0;
        }
        
        objpix = ((((objpal >> 4) & 1) ? cached_OBP1 : cached_OBP0) >> ((objpal&03) * 2)) & 0x3;
        winbgpix = (cached_BGP >> (winbgpal*2))&0x3;

        if (objenable) {
            if (((((objpal&3) == 0) ||(objpal & 0x80)) && (winbgpal != 0)) || (objpal == (1 << 5))) {
                pixel = winbgpix;
            } else {
                pixel = objpix;
                if (objpix == (1 << 5)) pixel = 0;
            }
        } else {
            pixel = winbgpix;
        }

        if (debug_render) {
            objpal = (((objpal >> 4) & 1 ? cached_OBP1 : cached_OBP0) >> ((objpal&03) * 2)) & 0x3;
            SDL_SetRenderDrawColor(debug_object_renderer, 255-(255.0/3)*(objpal), 255-(255.0/3)*(objpal), 255-(255.0/3)*(objpal), 255);
            SDL_RenderPoint(debug_object_renderer,renderX-6, lines);    

            winpal = (cached_BGP >> (winpal*2))&0x3;
            SDL_SetRenderDrawColor(debug_window_renderer, 255-(255.0/3)*(winpal), 255-(255.0/3)*(winpal), 255-(255.0/3)*(winpal), 255);
            SDL_RenderPoint(debug_window_renderer,renderX-6, lines);    
        }
        
        SDL_SetRenderDrawColor(renderer, 255-(255.0/3)*(pixel), 255-(255.0/3)*(pixel), 255-(255.0/3)*(pixel), 255);
        SDL_RenderPoint(renderer,renderX++-6, lines);   
    }

    // TODO fix issues
    inline byte PPU::objFIFO() {
        byte tilei = 255;
        byte objsize = (cached_LCDC & (1 << 2)) ? 16 : 8;
        
        uint8_t tilex = 0;

        for (int i = (int)objnum-1; i >= 0; --i) {
            obj cand_tile = objbuffer[i];

            if (cand_tile.objx-2 <= renderX && 
                cand_tile.objx+6 > renderX) 
                tilei = i;
        }

        if (tilei == 255) return (1<<5);

        obj tile = objbuffer[tilei]; 

        byte objy = tile.objy,
             objx = tile.objx-2,
             flags = tile.flags,
             index = tile.index;
        

        if (objsize == 16) {
            index = (index & 0xFE);
        }
        
        byte palette = flags & (1 << 4),
             Xflip = flags & (1 << 5),
             Yflip = flags & (1 << 6),
             prio = flags & (1 << 7);


        half tile_address = VIDEO_RAM+index*16 + (Yflip ? (objsize-1-(lines-objy))*2 : (lines-objy)*2);

        byte tilelow = bus->read(tile_address),
            tilehigh = bus->read(tile_address+1);

        byte object_pixel = (((1 << (Xflip ? (renderX-objx) : 7-(renderX-objx))) & tilelow) != 0) | ((((1 << (Xflip ?  (renderX-objx) : 7-(renderX-objx))) & tilehigh) != 0) << 1);

        byte object_pixel_with_flags = object_pixel | prio | palette;
        return object_pixel_with_flags; 
    }

    inline byte PPU::bgFIFO(half tilex, half tiley) {
        byte bgenable = cached_LCDC & 1;

        half BG_tile_map = (cached_LCDC & (1 << 3)) ? 0x9C00 : 0x9800;
        byte data_area = (cached_LCDC & (1 << 4));

        
        half tile_index_index = (tilex/8)+((tiley)/8)*32 + BG_tile_map;

        byte tile_index = bus->read(tile_index_index);

        byte tilelow, tilehigh;
        uint8_t tiley_mod8 = tiley & 7;
        uint8_t tilex_mod8 = tilex & 7;

        if (data_area != 0) {
            tilelow = bus->read(tile_index*16+tiley_mod8*2+VIDEO_RAM);
            tilehigh = bus->read(tile_index*16+tiley_mod8*2+1+VIDEO_RAM);
        } else {
            tilelow = bus->read(tiley_mod8*2+0x9000+((int8_t)tile_index)*16);
            tilehigh = bus->read(tiley_mod8*2+1+0x9000+((int8_t)tile_index)*16);
        }

        byte final_pixel = (((1 << (7-tilex_mod8)) & tilelow) != 0) | ((((1 << (7-tilex_mod8)) & tilehigh) != 0) << 1);
        return final_pixel;
    }

    inline byte PPU::windowFIFO(half tilex, half tiley) {
        byte windowEnable = (cached_LCDC & (1 << 5));

        half w_tile_map = (cached_LCDC & (1 << 6)) ? 0x9C00 : 0x9800;  
        byte data_area = (cached_LCDC & (1 << 4));

        half tile_index_index = (tilex/8)+((tiley)/8)*32;

        byte tile_index = bus->read(tile_index_index+w_tile_map);

        byte tilelow, tilehigh;
        uint8_t tiley_mod8 = tiley & 7;
        uint8_t tilex_mod8 = tilex & 7;

        if (data_area) {
            tilelow = bus->read(tile_index*16+tiley_mod8*2+VIDEO_RAM);
            tilehigh = bus->read(tile_index*16+tiley_mod8*2+1+VIDEO_RAM);
        } else {
            if (tile_index <= 127) {
                tilelow = bus->read(tiley_mod8*2+0x9000+tile_index*16);
                tilehigh = bus->read(tiley_mod8*2+1+0x9000+tile_index*16);
            } else {
                tilelow = bus->read(tiley_mod8*2+0x9000+((int8_t)tile_index)*16);
                tilehigh = bus->read(tiley_mod8*2+1+0x9000+((int8_t)tile_index)*16);
            }
        }

        byte final_pixel = (((1 << (7-tilex_mod8)) & tilelow) != 0) | ((((1 << (7-tilex_mod8)) & tilehigh) != 0) << 1);

        return final_pixel; 
    }

    void PPU::render_debug() {
        if (!debug_render) 
            init_debug_window();
        // for (int i = 0; i < 382; ++i) {
        //     for (int j = 0; j < 8; ++j) {
        //         byte a = bus->read(0x8000+i*16+j*2), b = bus->read(0x8000+i*16+j*2+1);
        //         for (int k = 0; k < 8; ++k) {
        //             int temp = (((1 << (7-k)) & a) != 0) | ((((1 << (7-k)) & b) != 0) << 1);
        //             SDL_SetRenderDrawColor(debug_tile_renderer, 255-255.0/3*(temp), 255-255.0/3*(temp), 255-255.0/3*(temp), 255);
        //             SDL_RenderPoint(debug_tile_renderer, (i%16)*8+k, int(i/16)*8+j);
        //         }
        //     }
        // }
        // SDL_RenderPresent(debug_tile_renderer);  

        byte wx = bus->read(WX),
             wy = bus->read(WY),
             scx = bus->read(SCX),
             scy = bus->read(SCY);

        for (int i = 0; i < 256; ++i) {
            for (int j = 0; j < 256; ++j) {
                half bg_tilex = (scx+i-6)%256, bg_tiley = (scy+j)%256;
                int temp = bgFIFO(bg_tilex, bg_tiley);
                SDL_SetRenderDrawColor(debug_bg_renderer, 255-255.0/3*(temp), 255-255.0/3*(temp), 255-255.0/3*(temp), 255);
                SDL_RenderPoint(debug_bg_renderer, i, j);
            }
        }



        SDL_RenderPresent(debug_object_renderer); 
        SDL_RenderPresent(debug_window_renderer);        
        SDL_RenderPresent(debug_bg_renderer);        

    }

    void PPU::dump_info() {
        std::cerr << std::hex <<    "dots: " << dots << '\n';
        std::cerr << "lines: " << lines << '\n';
        std::cerr << "renderX: " << renderX << '\n';
        switch(mode) { 
            case hblank:
            std::cerr << "state: hblank\n";
            break;
            case vblank:
            std::cerr << "state: vblank\n";
            break;
            case OAMscan:
            std::cerr << "state: OAMscan\n";
            break;
            case draw:
            std::cerr << "state: draw\n";
            break;
        }
        std::cerr << std::endl;
    }

    void PPU::dump_vram() {
        for(int i = 0; i < 0x3FF; ++i) {
            if (i%16 == 0) std::cout << std::endl;
            std::cout << std::hex << (i+0x9900) << ": " << std::bitset<8>(bus->read(i+0x9900) & 0xFF) << " ";
        }
    }
} 