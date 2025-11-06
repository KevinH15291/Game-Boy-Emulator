#pragma once 

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <thread>

#include "CPU/SM83.h"
#include "MMU/bus.h"
#include "video/PPU.h"
#include "bus.h"

namespace GBC {
    using namespace std::chrono_literals;

    class GBC {
        public: 
            GBC();
            void start();
            void run();
            void start_window();
            void execute_frame();
            [[gnu::always_inline]] inline void execute_cycle();
            inline void debug_execute_cycle(uint32_t freq);
            inline void dump_stuff();
            inline void handle_input();

            address_bus addresses;

            SM83 cpu;
            PPU ppu;

            int prevpc = 0, cachedline = 0, frame = 0;
            unsigned long long cycle_count = 0;

            bool debug_flag = 0;

    };
}