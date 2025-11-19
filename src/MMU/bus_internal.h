#pragma once

#include <cstddef>
#include <cstdint>

#include "enums.h"
using half = uint16_t;
enum class MemoryBankController : uint8_t;

namespace bus_internal {

struct Region {
    half begin;
    half end;
};

constexpr Region make_region(MemoryRegion begin, MemoryRegion end) {
    return Region{addr(begin), addr(end)};
}

constexpr Region kRomBank00 =
    make_region(MemoryRegion::ROM_BANK_00, MemoryRegion::END_ROM_BANK_00);
constexpr Region kRomBankNN =
    make_region(MemoryRegion::ROM_BANK_NN, MemoryRegion::END_ROM_BANK_NN);
constexpr Region kWorkRam0 =
    make_region(MemoryRegion::WORK_RAM_BANK0, MemoryRegion::END_WORK_RAM_BANK0);
constexpr Region kWorkRamN =
    make_region(MemoryRegion::WORK_RAM_BANKN, MemoryRegion::END_WORK_RAM_BANKN);
constexpr Region kVideoRam =
    make_region(MemoryRegion::VIDEO_RAM, MemoryRegion::END_VIDEO_RAM);
constexpr Region kExternalRam =
    make_region(MemoryRegion::EXTERNAL_RAM, MemoryRegion::END_EXTERNAL_RAM);
constexpr Region kOamRange =
    make_region(MemoryRegion::OAMaddress, MemoryRegion::END_OAM);
constexpr Region kIoRange =
    make_region(MemoryRegion::IO_REGISTERS, MemoryRegion::END_IO_REGISTERS);
constexpr Region kHighRam =
    make_region(MemoryRegion::HIGH_RAM, MemoryRegion::END_HIGH_RAM);
constexpr Region kNotUsable =
    make_region(MemoryRegion::NOT_USUABLE, MemoryRegion::END_NOT_USUABLE);

constexpr half kIoBase = addr(MemoryRegion::IO_REGISTERS);
constexpr size_t kJoypIndex =
    addr(IORegister::JOYP) - addr(MemoryRegion::IO_REGISTERS);

template <typename T>
constexpr bool in_region(T address, const Region& region) {
    return address >= region.begin && address <= region.end;
}

constexpr bool is_audio_register(half address) {
    return address >= addr(AudioRegister::NR10) &&
           address <= addr(AudioRegister::NR52);
}

constexpr bool is_wave_table_register(half address) {
    return address >= 0xFF30 && address <= 0xFF3F;
}

constexpr bool is_mbc3_controller(MemoryBankController controller) {
    switch (controller) {
        case MemoryBankController::MBC3_RTC_BATTERY:
        case MemoryBankController::MBC3_RTC_RAM_BATTERY:
        case MemoryBankController::MBC3:
        case MemoryBankController::MBC3_RAM:
        case MemoryBankController::MBC3_RAM_BATTERY:
            return true;
        default:
            return false;
    }
}

}  // namespace bus_internal

