#pragma once

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "CPU/SM83.h"
#include "CgbConfig.h"
#include "DebugMacros.h"
#include "MMU/bus.h"
#include "audio/APU.h"
#include "bus.h"
#include "video/PPU.h"

namespace GBC {
using namespace std::chrono_literals;

class GBC {
   public:
    explicit GBC(bool enable_window = true);
    void start();
    void run();
    void start_window();
    void execute_frame();
    [[gnu::always_inline]] inline void execute_cycle();
    inline void debug_execute_cycle(uint32_t freq);

    inline void handle_input();
    void reset_after_rom_load();
#if GBC_CPU_DEBUG && !defined(__EMSCRIPTEN__)
    inline void dump_stuff();
    void log_frame_state(uint32_t frame_index);
#endif

    const bool window_enabled_;
    CgbConfig config{};
    address_bus addresses;

    SM83 cpu;
    PPU ppu;
    APU apu;

    int prevpc = 0, cachedline = 0, frame = 0;
    unsigned long long cycle_count = 0;

#ifdef __EMSCRIPTEN__
    std::array<byte, 8> buttonState = {
        0};  // 8 buttons: A, B, Select, Start, Right, Left, Up, Down
#endif
};
}  // namespace GBC