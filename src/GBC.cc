#include "GBC.h"

#include <bitset>
#include <cstdint>
#include <fstream>
#include <ios>

#include "bus.h"
#include "cycles.h"

namespace GBC {
constexpr uint32_t CYCLES_PER_SECOND = 70224;
constexpr uint32_t CYCLES_PER_FRAME = 4560;

GBC::GBC() : cpu(&addresses), ppu(&addresses), apu(addresses) {
    start();
    std::ofstream("log.txt") << "";
}

GBC::~GBC() = default;

void GBC::start() {
    // ppu.init_debug_window();
    ppu.init_window();

    addresses.booting = false;
    addresses
        .IOrange[addr(IORegister::JOYP) - addr(MemoryRegion::IO_REGISTERS)] =
        0xFF;
    addresses.write(addr(VideoRegister::OBP0), 0b11100100);
    addresses.write(addr(VideoRegister::OBP1), 0b11100100);
}

// This function is a mess, I'm using it to debug stuff right now
void GBC::run() {
    if (!addresses.booting) {
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
    bool breakflag = false;
    uint64_t last_time = SDL_GetTicks();
    SDL_Event event;
    while (true) {
        ++frame;
        // frame, TODO move out into function
        for (uint32_t i = 0; i < CYCLES_PER_SECOND; ++i) {
            execute_cycle();

            if (ppu.mode == RenderingState::vblank) {
                break;
            }
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) breakflag = true;
        }

        for (uint32_t j = 0; j < CYCLES_PER_FRAME; ++j) {
            execute_cycle();
        }
        if (breakflag) break;

        // apu.end_frame();

        uint64_t this_time = SDL_GetTicks();
        // std::cout << 16 - last_time + this_time << std::endl;
        if (this_time - last_time < 16) {
            SDL_Delay(16 -
                      (this_time - last_time));  // TODO make sleep based on
                                                 // elapsed frame execution time
        }
        last_time = SDL_GetTicks();
    }
}

inline void GBC::handle_input() {
    byte input_s = 0xF0;
    byte input_d = 0xF0;

    if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_A]) {
        input_s |= 1;
    }
    if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_S]) {
        input_s |= (1 << 1);
    }
    if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_Z]) {
        input_s |= (1 << 2);  // select
    }
    if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_X]) {
        input_s |= (1 << 3);  // start
    }
    if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_RIGHT]) {
        input_d |= 1;
    }
    if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_LEFT]) {
        input_d |= (1 << 1);
    }
    if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_UP]) {
        input_d |= (1 << 2);
    }
    if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_DOWN]) {
        input_d |= (1 << 3);
    }

    if (((input_d & addresses.input_d) ^ input_d) |
        ((input_s & addresses.input_s) ^ input_s)) {
        addresses.write(addr(IORegister::IF),
                        addresses.read(addr(IORegister::IF)) | (1 << 4));
    }

    addresses.input_d = ~input_d;
    addresses.input_s = ~input_s;
}

inline void GBC::execute_cycle() {
    ppu.execute_cycle();
    cpu.execute_cycle();
    apu.execute_cycle();

    ++cycle_count;

    if (cycle_count % 1000 == 0) handle_input();
}

inline void GBC::debug_execute_cycle(uint32_t freq) {
    execute_cycle();

    if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_D]) {
        debug_flag = true;
    }
    if (debug_flag && cycle_count % freq == 0) {
        // std::cout << std::dec << cycle_count << std::hex << std::endl;
        dump_stuff();
    }
}

inline void GBC::dump_stuff() {
    std::ofstream("log.txt", std::ofstream::app)
        << "_________\n"
        << "cycles: " << cycle_count << '\n'
        << "frame: " << frame << '\n'
        << "pc: " << std::hex << cpu.pc << '\n'
        << "cycles: " << std::hex << (int)cpu.cycles << '\n'
        << "opcode: " << std::hex << (int)cpu.opcode << '\n'
        << "instr: " << std::hex
        << (cpu.opcode > 0xFF ? CBinstructions[cpu.opcode & 0xFF]
                              : instructions[cpu.opcode & 0xFF])
        << '\n'
        << "rom bank: " << std::hex << (int)addresses.rom_bank << '\n'
        << "sp: " << std::hex << cpu.sp << '\n'
        << "top of stack: " << std::hex
        << ((uint16_t)addresses.read(cpu.sp) |
            ((uint16_t)addresses.read(cpu.sp + 1) << 8))
        << '\n'
        << "flags: " << std::bitset<8>(cpu.RF) << '\n'
        << "HL: " << std::hex << cpu.getHL() << '\n'
        << "renderX: " << ppu.renderX << '\n'
        << "lines: " << ppu.lines << '\n'
        << "stat: " << std::bitset<8>(addresses.read(addr(VideoRegister::STAT)))
        << '\n'
        << "LCDC: " << std::bitset<8>(addresses.read(addr(VideoRegister::LCDC)))
        << '\n'
        << "lyc: " << std::hex << (int)addresses.read(addr(VideoRegister::LYC))
        << '\n'
        << "IE: " << std::hex << (int)addresses.read(addr(MemoryRegion::IE))
        << '\n'
        << "IF: " << std::hex << (int)addresses.read(addr(IORegister::IF))
        << '\n'
        << "IME: " << std::hex << (int)cpu.IME << '\n'
        << "bg tile map: " << std::hex
        << (addresses.read(addr(VideoRegister::LCDC)) & (1 << 3) ? 0x9C00
                                                                 : 0x9800)
        << '\n'
        << "OBP0: " << std::hex
        << std::bitset<8>(addresses.read(addr(VideoRegister::OBP0))) << '\n'
        << "OBP1: " << std::hex
        << std::bitset<8>(addresses.read(addr(VideoRegister::OBP1))) << '\n'
        << std::hex << "dots: " << ppu.dots << '\n'
        << std::bitset<8>(addresses.read_privileged(addr(AudioRegister::NR52)))
        << '\n';
    switch (ppu.mode) {
        case RenderingState::hblank:
            std::ofstream("log.txt", std::ofstream::app) << "state: hblank\n";
            break;
        case RenderingState::vblank:
            std::ofstream("log.txt", std::ofstream::app) << "state: vblank\n";
            break;
        case RenderingState::OAMscan:
            std::ofstream("log.txt", std::ofstream::app) << "state: OAMscan\n";
            break;
        case RenderingState::draw:
            std::ofstream("log.txt", std::ofstream::app) << "state: draw\n";
            break;
    }

    cpu.dump_registers();

    std::ofstream("log.txt", std::ofstream::app)
        << "LY: " << (int)ppu.bus->read(0xFF44) << '\n'
        << "_________" << '\n';
    // ppu.dump_info();
    // cpu.dump_info();
}
}  // namespace GBC
