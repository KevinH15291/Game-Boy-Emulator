#pragma once

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "CPU/SM83.h"
#include "MMU/bus.h"
#include "audio/APU.h"
#include "video/PPU.h"

namespace GBC {
class GBC {
   public:
    GBC();
    ~GBC();
    GBC(GBC &) = delete;
    GBC(GBC &&) = delete;
    auto operator=(GBC &) = delete;
    auto operator=(GBC &&) = delete;

    void start();
    void run();
    void start_window();
    inline void execute_cycle();
    inline void debug_execute_cycle(uint32_t freq = 10000);
    inline void dump_stuff();
    inline void handle_input();
    void reset_cpu_state();
    void execute_frame();

    address_bus addresses;

    SM83 cpu;
    PPU ppu;
    APU apu;

    int prevpc = 0, cachedline = 0, frame = 0;
    unsigned long long cycle_count = 0;

    bool debug_flag = false;
};
}  // namespace GBC