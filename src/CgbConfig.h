#pragma once

#include <cstdint>

namespace GBC {

enum class BootMode : uint8_t { DMG = 0, CGB_COMPAT = 1, CGB_ONLY = 2 };

struct CgbConfig {
    BootMode boot_mode{BootMode::DMG};
    bool cgb_mode{false};
    bool double_speed{false};
    bool speed_switch_armed{false};
};

}  // namespace GBC


