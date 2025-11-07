#include "GBC.h"

#include <bitset>
#include <chrono>
#include <fstream>
#include <thread>

#include "bus.h"
#include "cycles.h"
#include "enums.h"

namespace GBC {
GBC::GBC() : cpu(&addresses), ppu(&addresses), apu(addresses) {
    addresses.ppu = &ppu;
    addresses.apu = &apu;
    start();
#ifndef __EMSCRIPTEN__
    std::ofstream("log.txt") << "";
#endif
}

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

    // Game Boy runs at ~59.7 FPS (70224 cycles per frame = ~16.7ms per frame)
    constexpr double FRAME_TIME_MS = 1000.0 / 59.7275;
    constexpr auto FRAME_TIME_NS =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::duration<double>(1.0 / 59.7275));

    while (true) {
        auto frame_start = std::chrono::high_resolution_clock::now();
        ++frame;

        // frame, TODO move out into function
        for (int i = 0; i < 70224;) {
            int batch_size = 100;
            for (int j = 0; j < batch_size && i < 70224; ++j, ++i) {
                execute_cycle();
            }
            if (ppu.mode == RenderingState::vblank) {
                break;
            }
        }

        while (SDL_PollEvent(&ppu.event)) {
#ifdef __EMSCRIPTEN__
            if (ppu.event.type == SDL_QUIT) breakflag = true;
#else
            if (ppu.event.type == SDL_EVENT_QUIT) breakflag = true;
#endif
        }
        // ppu.render_debug();

        for (int j = 0; j < 4560; ++j) {
            execute_cycle();
        }

        if (breakflag) break;

        // Frame rate limiting with nanosecond precision
        auto frame_end = std::chrono::high_resolution_clock::now();
        auto elapsed = frame_end - frame_start;
        if (elapsed < FRAME_TIME_NS) {
            std::this_thread::sleep_for(FRAME_TIME_NS - elapsed);
        }
    }
}

inline void GBC::handle_input() {
#ifdef __EMSCRIPTEN__
    byte input_s = 0xF0, input_d = 0xF0;

    // Map buttonState array to Game Boy input bits
    // Button indices: 0=A, 1=B, 2=Select, 3=Start, 4=Right, 5=Left, 6=Up,
    // 7=Down
    if (buttonState[0]) input_s |= 1;         // A
    if (buttonState[1]) input_s |= (1 << 1);  // B
    if (buttonState[2]) input_s |= (1 << 2);  // Select
    if (buttonState[3]) input_s |= (1 << 3);  // Start

    if (buttonState[4]) input_d |= 1;         // Right
    if (buttonState[5]) input_d |= (1 << 1);  // Left
    if (buttonState[6]) input_d |= (1 << 2);  // Up
    if (buttonState[7]) input_d |= (1 << 3);  // Down

    if (((input_d & addresses.input_d) ^ input_d) |
        ((input_s & addresses.input_s) ^ input_s)) {
        addresses.write(addr(IORegister::IF),
                        addresses.read(addr(IORegister::IF)) | (1 << 4));
    }

    addresses.input_d = ~input_d;
    addresses.input_s = ~input_s;
#else
    byte input_s = 0xF0, input_d = 0xF0;

    const bool* keyboard_state = SDL_GetKeyboardState(nullptr);
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

    if (((input_d & addresses.input_d) ^ input_d) |
        ((input_s & addresses.input_s) ^ input_s)) {
        addresses.write(addr(IORegister::IF),
                        addresses.read(addr(IORegister::IF)) | (1 << 4));
    }

    addresses.input_d = ~input_d;
    addresses.input_s = ~input_s;
#endif
}

void GBC::execute_frame() {
    ++frame;
    handle_input();
    for (int i = 0; i < 70224;) {
        int batch_size = 100;
        for (int j = 0; j < batch_size && i < 70224; ++j, ++i) {
            ppu.execute_cycle();
            apu.execute_cycle();
            cpu.execute();
            ++cycle_count;
        }
    }
    apu.flush_audio();
}

inline void GBC::execute_cycle() {
    ppu.execute_cycle();
    apu.execute_cycle();
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

        << std::hex << "dots: " << ppu.dots << '\n';
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
        << "_________" << std::endl;
    // ppu.dump_info();
    // cpu.dump_info();
}
}  // namespace GBC
