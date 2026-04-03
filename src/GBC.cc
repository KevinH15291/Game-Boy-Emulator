#include "GBC.h"

#include <bitset>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <thread>

#include "CPU/cycles.h"
#include "bit_ops.h"
#include "bus.h"

namespace GBC {
GBC::GBC(bool enable_window)
    : window_enabled_(enable_window),
      addresses(config),
      cpu(&addresses, config),
      ppu(&addresses, config),
      apu(addresses, config) {
    addresses.ppu = &ppu;
    addresses.apu = &apu;
    addresses.set_cpu(&cpu);
    start();
}

void GBC::start() {
    if (window_enabled_) {
        ppu.init_window();
    }

    reset_after_rom_load();
}

void GBC::reset_after_rom_load() {
    addresses.booting = false;
    constexpr byte kDmgPalette = 0b11100100;
    constexpr byte kDefaultLcdc = 0x91;
    addresses
        .IOrange[addr(IORegister::JOYP) - addr(MemoryRegion::IO_REGISTERS)] =
        0xCF;
    addresses.write(addr(VideoRegister::BGP), kDmgPalette);
    addresses.write(addr(VideoRegister::OBP0), kDmgPalette);
    addresses.write(addr(VideoRegister::OBP1), kDmgPalette);
    addresses.write(addr(VideoRegister::LCDC), kDefaultLcdc);
    addresses.write(addr(VideoRegister::SCX), 0x00);
    addresses.write(addr(VideoRegister::SCY), 0x00);
    addresses.write(addr(VideoRegister::LYC), 0x00);
    addresses.write(addr(CGBRegister::VBK), 0x00);
    addresses.write(addr(CGBRegister::SVBK), 0x01);

    const auto set_dmg_registers = [this]() {
        cpu.RA = 0x01;
        cpu.RB = 0xFF;
        cpu.RC = 0x13;
        cpu.RD = 0x00;
        cpu.RE = 0xC1;
        cpu.RH = 0x84;
        cpu.RL = 0x03;
        cpu.RF = 0xB0;
        cpu.pc = 0x100;
        cpu.sp = 0xFFFE;
    };

    const auto set_cgb_registers = [this]() {
        cpu.RA = 0x11;
        cpu.RB = 0x00;
        cpu.RC = 0x00;
        cpu.RD = 0xFF;
        cpu.RE = 0x56;
        cpu.RH = 0x00;
        cpu.RL = 0x0D;
        cpu.RF = 0x80;
        cpu.pc = 0x100;
        cpu.sp = 0xFFFE;
    };

    if (config.cgb_mode) {
        set_cgb_registers();
    } else {
        set_dmg_registers();
    }

    ppu.mode = RenderingState::hblank;
    ppu.lines = 0;
    ppu.dots = 0;
    ppu.renderX = 0;
    ppu.wly = 0;
    ppu.window_vert_active = false;
    apu.reset();
    cycle_count = 0;
    config.double_speed = false;
    config.speed_switch_armed = false;
    addresses.sync_key_registers();
}

#ifndef __EMSCRIPTEN__
void GBC::run() {
    const auto initialize_registers = [this]() {
        if (config.cgb_mode) {
            cpu.RA = 0x11;
            cpu.RB = 0x00;
            cpu.RC = 0x00;
            cpu.RD = 0xFF;
            cpu.RE = 0x56;
            cpu.RH = 0x00;
            cpu.RL = 0x0D;
            cpu.RF = 0x80;
        } else {
            cpu.RA = 0x01;
            cpu.RB = 0xFF;
            cpu.RC = 0x13;
            cpu.RD = 0x00;
            cpu.RE = 0xC1;
            cpu.RH = 0x84;
            cpu.RL = 0x03;
        }
        cpu.pc = 0x100;
        cpu.sp = 0xFFFE;
    };

    if (!addresses.booting) {
        initialize_registers();
    } else {
        cpu.pc = 0;
    }
    bool breakflag = false;

    // constexpr double FRAME_TIME_MS = 1000.0 / 60;
    constexpr auto FRAME_TIME_NS =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::duration<double>(1.0 / 60));

    while (true) {
        auto frame_start = std::chrono::high_resolution_clock::now();
        ++frame;

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
            if (ppu.event.type == SDL_EVENT_QUIT) breakflag = true;
        }

        for (int j = 0; j < 4560; ++j) {
            execute_cycle();
        }

        if (breakflag) break;

        auto frame_end = std::chrono::high_resolution_clock::now();
        auto elapsed = frame_end - frame_start;
        if (elapsed < FRAME_TIME_NS) {
            std::this_thread::sleep_for(FRAME_TIME_NS - elapsed);
        }
        ppu.present();
    }
}
#endif

inline void GBC::handle_input() {
#ifndef __EMSCRIPTEN__
    if (!window_enabled_) {
        return;
    }
#endif
#ifdef __EMSCRIPTEN__
    byte input_s = 0xF0, input_d = 0xF0;

    const auto set_if_pressed = [](bool pressed, byte bit, byte& target) {
        if (pressed) {
            target = setBit(target, bit);
        }
    };

    set_if_pressed(buttonState[0], 0, input_s);
    set_if_pressed(buttonState[1], 1, input_s);
    set_if_pressed(buttonState[2], 2, input_s);
    set_if_pressed(buttonState[3], 3, input_s);

    set_if_pressed(buttonState[4], 0, input_d);
    set_if_pressed(buttonState[5], 1, input_d);
    set_if_pressed(buttonState[6], 2, input_d);
    set_if_pressed(buttonState[7], 3, input_d);

    if (((input_d & addresses.input_d) ^ input_d) |
        ((input_s & addresses.input_s) ^ input_s)) {
        auto& if_reg = addresses.IOrange[addr(IORegister::IF) -
                                         addr(MemoryRegion::IO_REGISTERS)];
        if_reg = setBit(if_reg, 4);
    }

    addresses.input_d = ~input_d;
    addresses.input_s = ~input_s;
#else
    byte input_s = 0xF0, input_d = 0xF0;

    const bool* keyboard_state = SDL_GetKeyboardState(nullptr);
    const auto set_if_key_down = [&](SDL_Scancode scancode, byte bit,
                                     byte& target) {
        if (keyboard_state[scancode]) {
            target = setBit(target, bit);
        }
    };

    set_if_key_down(SDL_SCANCODE_A, 0, input_s);
    set_if_key_down(SDL_SCANCODE_S, 1, input_s);
    set_if_key_down(SDL_SCANCODE_X, 2, input_s);
    set_if_key_down(SDL_SCANCODE_Z, 3, input_s);

    set_if_key_down(SDL_SCANCODE_RIGHT, 0, input_d);
    set_if_key_down(SDL_SCANCODE_LEFT, 1, input_d);
    set_if_key_down(SDL_SCANCODE_UP, 2, input_d);
    set_if_key_down(SDL_SCANCODE_DOWN, 3, input_d);

    if (((input_d & addresses.input_d) ^ input_d) |
        ((input_s & addresses.input_s) ^ input_s)) {
        auto& if_reg = addresses.IOrange[addr(IORegister::IF) -
                                         addr(MemoryRegion::IO_REGISTERS)];
        if_reg = setBit(if_reg, 4);
    }

    addresses.input_d = ~input_d;
    addresses.input_s = ~input_s;
#endif
}

void GBC::execute_frame() {
    ++frame;
    handle_input();
    constexpr int CYCLES_PER_FRAME = 70224;
    for (int i = 0; i < CYCLES_PER_FRAME; ++i) {
        ppu.execute_cycle();
        apu.execute_cycle();
        cpu.execute();
        if (config.double_speed) {
            cpu.execute();
            cycle_count += 2;
        } else {
            ++cycle_count;
        }
    }
    apu.flush_audio();
}

inline void GBC::execute_cycle() {
    ppu.execute_cycle();
    apu.execute_cycle();
    cpu.execute();
    if (config.double_speed) {
        cpu.execute();
        cycle_count += 2;
    } else {
        ++cycle_count;
    }

#ifndef __EMSCRIPTEN__
    if (cycle_count % 100 == 0) handle_input();
#endif
}

inline void GBC::debug_execute_cycle(uint32_t freq) {
    (void)freq;
    execute_cycle();

#if GBC_CPU_DEBUG && !defined(__EMSCRIPTEN__)
    static bool debug_active = false;
    if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_D]) {
        debug_active = true;
    }
    if (debug_active && cpu.cycles == 0) {
        dump_stuff();
    }
#endif
}

#if GBC_CPU_DEBUG && !defined(__EMSCRIPTEN__)
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
        << ((half)addresses.read(cpu.sp) |
            ((half)addresses.read(cpu.sp + 1) << 8))
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
        << "IE: " << std::hex << (int)addresses.IEnable << '\n'
        << "IF: " << std::hex
        << (int)addresses
               .IOrange[addr(IORegister::IF) - addr(MemoryRegion::IO_REGISTERS)]
        << '\n'
        << "IME: " << std::hex << (int)cpu.IME << '\n'
        << "bg tile map: " << std::hex
        << (isBitSet(addresses.read(addr(VideoRegister::LCDC)), 3) ? 0x9C00
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
}
#endif
#if GBC_CPU_DEBUG && !defined(__EMSCRIPTEN__)

void GBC::log_frame_state(uint32_t frame_index) {
    static constexpr uint32_t kMaxLoggedFrames = 32;
    if (frame_index >= kMaxLoggedFrames) {
        return;
    }

    std::ofstream log("log.txt", std::ios::app);
    if (!log.is_open()) {
        return;
    }

    const byte lcdc = addresses.read(addr(VideoRegister::LCDC));
    const byte stat = addresses.read(addr(VideoRegister::STAT));
    const byte ly = addresses.read(addr(VideoRegister::LY));
    const byte key1 = addresses.read(addr(CGBRegister::KEY1));
    const bool hdma_active = addresses.hdma_active;
    const bool hdma_hblank = addresses.hdma_mode_hblank;
    const byte hdma_blocks = addresses.hdma_blocks_remaining;
    const half hdma_src = addresses.hdma_src;
    const half hdma_dst = addresses.hdma_dst;

    log << std::dec << "frame=" << frame_index << " cycles=" << cycle_count
        << " double_speed=" << (config.double_speed ? "1" : "0") << " lcdc=0x"
        << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(lcdc)
        << " stat_mode=" << static_cast<int>(stat & 0x03)
        << " ly=" << static_cast<int>(ly)
        << " hdma_active=" << (hdma_active ? "1" : "0")
        << " hdma_hblank=" << (hdma_hblank ? "1" : "0")
        << " hdma_blocks=" << static_cast<int>(hdma_blocks) << " hdma_src=0x"
        << std::hex << std::setw(4) << std::setfill('0')
        << static_cast<int>(hdma_src) << " hdma_dst=0x" << std::setw(4)
        << static_cast<int>(hdma_dst) << " key1=0x" << std::setw(2)
        << static_cast<int>(key1) << std::dec << '\n';

    log << " rtc_halted=" << (addresses.rtc_halted ? "1" : "0")
        << " rtc_sel=" << static_cast<int>(addresses.rtc_selected_register)
        << " rtc_regs=[";
    for (size_t i = 0; i < addresses.rtc_registers.size(); ++i) {
        if (i != 0) {
            log << ',';
        }
        log << static_cast<int>(addresses.rtc_registers[i]);
    }
    log << "]\n";
}
#endif

}  // namespace GBC
