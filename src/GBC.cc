#include "GBC.h"
#include "bus.h"
#include "cycles.h"
#include <bitset>

namespace GBC {
    GBC::GBC() : cpu(&addresses), ppu(&addresses) {
        start();
        std::ofstream("log.txt") << "";
    } 

    void GBC::start() {
        // ppu.init_debug_window();
        ppu.init_window();

        addresses.booting = false;
        addresses.IOrange[JOYP-IO_REGISTERS] = 0xFF;
        addresses.write(OBP0, 0b11100100);
        addresses.write(OBP1, 0b11100100);

    }

    // This function is a mess, I'm using it to debug stuff right now
    void GBC::run() {
        if (addresses.booting == 0) {
            cpu.RA = 0x01;
            cpu.RB = 0xFF;
            cpu.RC = 0x13;
            cpu.RD = 0x00;
            cpu.RE = 0xC1;
            cpu.RH = 0x84;
            cpu.RL = 0x03;
            cpu.pc = 0x100;
            cpu.sp = 0xFFFE;
        } else {
            cpu.pc = 0;
        }
        bool breakflag = 0;
        while(true) {
            ++frame;

            // frame, TODO move out into function
            for (int i = 0; i < 70224; ) {
                int batch_size = 100;
                for (int j = 0; j < batch_size && i < 70224; ++j, ++i) {
                    execute_cycle();
                }
                if (ppu.mode == vblank) {
                    break;
                }
            } 

            while (SDL_PollEvent(&ppu.event)) {
                if (ppu.event.type == SDL_EVENT_QUIT) breakflag = true;
            }   
            // ppu.render_debug();

            for (int j = 0; j < 4560; ++j) {
                execute_cycle();
            }

            if (breakflag) break;
            // std::this_thread::sleep_for(10ms); // TODO make sleep based on elapsed frame execution time
        }
    }

    inline void GBC::handle_input() {
#ifndef __EMSCRIPTEN__
        byte input_s = 0xF0, input_d = 0xF0;

        const uint8_t* keyboard_state = SDL_GetKeyboardState(nullptr);
        if (keyboard_state[SDL_SCANCODE_A]) {
            input_s |= 1;
        }
        if (keyboard_state[SDL_SCANCODE_S]) {
            input_s |= (1 << 1);
        }
        if (keyboard_state[SDL_SCANCODE_Z]) {
            input_s |= (1 << 2);
        }
        if (keyboard_state[SDL_SCANCODE_X]) {
            input_s |= (1 << 3);
        }

        if (keyboard_state[SDL_SCANCODE_RIGHT]) {
            input_d |= 1;
        }
        if (keyboard_state[SDL_SCANCODE_LEFT]) {
            input_d |= (1 << 1);
        }
        if (keyboard_state[SDL_SCANCODE_UP]) {
            input_d |= (1 << 2);
        }
        if (keyboard_state[SDL_SCANCODE_DOWN]) {
            input_d |= (1 << 3); 
        }

        if (((input_d & addresses.input_d ) ^ input_d) | ((input_s & addresses.input_s) ^ input_s)) {
            addresses.write(IF, addresses.read(IF) | (1 << 4));
        }

        addresses.input_d = ~input_d;
        addresses.input_s = ~input_s;
#endif
    }

    void GBC::execute_frame() {
        handle_input();
        for (int i = 0; i < 70224; ++i) {
            ppu.execute_cycle();
            cpu.execute();
            ++cycle_count;
            if ((i % 100 == 0) && ppu.mode == vblank && i > 0) {
                break;
            }
        }
    }

    inline void GBC::execute_cycle() {
        ppu.execute_cycle();
        cpu.execute();

        ++cycle_count;

#ifndef __EMSCRIPTEN__
        if (cycle_count % 100 == 0) handle_input();
#endif
    }

    inline void GBC::debug_execute_cycle(uint32_t freq) {
        execute_cycle();

#ifndef __EMSCRIPTEN__
        if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_D]) {
            debug_flag = 1;
        }
        if (debug_flag && cpu.cycles == 0) { 
            dump_stuff();
        }
#endif
    }

    inline void GBC::dump_stuff() {
        std::ofstream("log.txt", std::ofstream::app) 
        << "_________\n"
        << "cycles: " << cycle_count << '\n'
        << "frame: " << frame << '\n'
        << "pc: " << std::hex << cpu.pc << '\n'
        << "cycles: " << std::hex << (int)cpu.cycles << '\n'
        << "opcode: " << std::hex << (int)cpu.opcode << '\n'
        << "instr: " << std::hex << (cpu.opcode > 0xFF ? CBinstructions[cpu.opcode&0xFF] : instructions[cpu.opcode&0xFF]) << '\n'
        << "rom bank: " << std::hex << (int)addresses.rom_bank << '\n'
        << "sp: " << std::hex << cpu.sp << '\n'
        << "top of stack: " << std::hex << ((uint16_t)addresses.read(cpu.sp) | 
         ((uint16_t)addresses.read(cpu.sp+1) << 8)) << '\n'
        << "flags: " << std::bitset<8>(cpu.RF) << '\n'
        << "HL: " << std::hex << cpu.getHL() << '\n'
        << "renderX: " << ppu.renderX << '\n'
        << "lines: " << ppu.lines << '\n'
        << "stat: " << std::bitset<8>(addresses.read(STAT)) << '\n'
        << "LCDC: " << std::bitset<8>(addresses.read(LCDC)) << '\n'
        << "lyc: " << std::hex << (int)addresses.read(LYC) << '\n'
        << "IE: " << std::hex << (int)addresses.read(IE) << '\n'
        << "IF: " << std::hex << (int)addresses.read(IF) << '\n'
        << "IME: " << std::hex << (int)cpu.IME << '\n'
        << "bg tile map: " << std::hex << (addresses.read(LCDC) & (1 << 3) ? 0x9C00 : 0x9800) << '\n'
        << "OBP0: " << std::hex <<  std::bitset<8>(addresses.read(OBP0)) << '\n'
        << "OBP1: " << std::hex << std::bitset<8>(addresses.read(OBP1))<< '\n'

        << std::hex << "dots: " << ppu.dots << '\n';
        switch(ppu.mode) { 
            case hblank:
            std::ofstream("log.txt", std::ofstream::app) << "state: hblank\n";
            break;
            case vblank:
            std::ofstream("log.txt", std::ofstream::app) << "state: vblank\n";
            break;
            case OAMscan:
            std::ofstream("log.txt", std::ofstream::app) << "state: OAMscan\n";
            break;
            case draw:
            std::ofstream("log.txt", std::ofstream::app) << "state: draw\n";
            break;
        }

        cpu.dump_registers();
        
        std::ofstream("log.txt", std::ofstream::app) << "LY: " << (int)ppu.bus->read(0xFF44) << '\n' << "_________" << std::endl;
        // ppu.dump_info();
        // cpu.dump_info();
    }
}
