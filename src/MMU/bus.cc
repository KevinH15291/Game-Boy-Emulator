#include "bus.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "../bit_ops.h"
#include "CPU/SM83.h"
#include "audio/APU.h"
#include "bus_internal.h"
#include "enums.h"
#include "video/PPU.h"

namespace GBC {
using namespace bus_internal;

byte apu_read_register(APU* apu, half address) {
    return apu->read_register(address);
}

byte apu_read_wave(APU* apu, half address) {
    return apu->read_wave_byte(address);
}

void apu_write_register(APU* apu, half address, byte value) {
    apu->write_register(address, value);
}

void apu_write_wave(APU* apu, half address, byte value) {
    apu->write_wave_byte(address, value);
}

void ppu_io_write(PPU* ppu, half address, byte value) {
    if (ppu != nullptr) {
        ppu->io_write(address, value);
    }
}
void cpu_reset_timer_counter(SM83* cpu) {
    if (cpu != nullptr) {
        cpu->reset_timer_counter();
    }
}
void cpu_mark_tima_written(SM83* cpu) {
    if (cpu != nullptr) {
        cpu->mark_tima_written();
    }
}
address_bus::address_bus(CgbConfig& config)
    : config(config), cartROM(MAX_CART_ROM_SIZE, 0) {
    memset(cartRAM.data(), 0, sizeof(cartRAM));
    memset(workRAM.data(), 0, sizeof(workRAM));
    memset(videoRAM.data(), 0, sizeof(videoRAM));
    memset(OAM.data(), 0, sizeof(OAM));
    memset(IOrange.data(), 0, sizeof(IOrange));
    IOrange[addr(IORegister::JOYP) - addr(MemoryRegion::IO_REGISTERS)] = 0xCF;
    sync_key_registers();
}

#ifndef __EMSCRIPTEN__
namespace {

std::expected<std::vector<byte>, std::string> read_file_bytes(
    const std::filesystem::path& path, size_t max_size = SIZE_MAX) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected("Failed to open " + path.string());
    }
    file.seekg(0, std::ios::end);
    const std::streampos length = file.tellg();
    file.seekg(0, std::ios::beg);
    if (length == 0) {
        return std::unexpected("File is empty: " + path.string());
    }
    if (static_cast<size_t>(length) > max_size) {
        return std::unexpected("File too large: " + path.string());
    }
    std::vector<byte> buffer(static_cast<size_t>(length));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), length)) {
        return std::unexpected("Failed to read " + path.string());
    }
    return buffer;
}

}  // namespace

void address_bus::load_boot_ROM(const char* fname, uint32_t size) {
    booting = true;
    const auto path = std::filesystem::path(fname);
    auto data = read_file_bytes(path, size);
    if (!data) {
        std::cerr << data.error() << std::endl;
        return;
    }
    const auto& bytes = data.value();
    const size_t copy_size = std::min<size_t>(bytes.size(), size);
    std::copy_n(bytes.begin(), copy_size, bootrom.begin());
}

void address_bus::load_ROM(const char* fname) {
    const auto path = std::filesystem::path(fname);
    auto data = read_file_bytes(path, cartROM.size());
    if (!data) {
        std::cerr << data.error() << std::endl;
        return;
    }
    const auto bytes_read = data->size();
    if (bytes_read <= 0x148) {
        std::cerr << "ROM header too small: " << fname << '\n';
        return;
    }
    std::copy(data->begin(), data->end(), cartROM.begin());
    if (bytes_read < cartROM.size()) {
        std::fill(cartROM.begin() + static_cast<std::ptrdiff_t>(bytes_read),
                  cartROM.end(), 0);
    }
    const size_t max_bytes = cartROM.size();
    if (bytes_read == max_bytes && data->size() == max_bytes) {
        std::cerr << "ROM larger than supported buffer (" << max_bytes
                  << " bytes); data truncated.\n";
    }

    mbc = static_cast<MemoryBankController>(cartROM[0x147]);
    rom_size = cartROM[0x148];
    const byte cgb_flag = cartROM[0x143];
    if ((cgb_flag & 0x80) == 0) {
        config.boot_mode = BootMode::DMG;
        config.cgb_mode = false;
    } else if (cgb_flag == 0xC0) {
        config.boot_mode = BootMode::CGB_ONLY;
        config.cgb_mode = true;
    } else {
        config.boot_mode = BootMode::CGB_COMPAT;
        config.cgb_mode = true;
    }
    config.double_speed = false;
    config.speed_switch_armed = false;
    sync_key_registers();
    reset_MBC_state();
}
#endif

#ifdef __EMSCRIPTEN__
void address_bus::load_ROM_buffer(const byte* data, size_t length) {
    size_t copy_size = std::min(length, cartROM.size());
    std::memcpy(cartROM.data(), data, copy_size);
    if (copy_size < cartROM.size()) {
        std::fill(cartROM.begin() + static_cast<std::ptrdiff_t>(copy_size),
                  cartROM.end(), 0);
    }
    if (copy_size > 0x148) {
        mbc = static_cast<MemoryBankController>(cartROM[0x147]);
        rom_size = cartROM[0x148];
        const byte cgb_flag = cartROM[0x143];
        if ((cgb_flag & 0x80) == 0) {
            config.boot_mode = BootMode::DMG;
            config.cgb_mode = false;
        } else if (cgb_flag == 0xC0) {
            config.boot_mode = BootMode::CGB_ONLY;
            config.cgb_mode = true;
        } else {
            config.boot_mode = BootMode::CGB_COMPAT;
            config.cgb_mode = true;
        }
        config.double_speed = false;
        config.speed_switch_armed = false;
        sync_key_registers();
        reset_MBC_state();
    }
}

void address_bus::set_boot_complete(bool completed) { booting = !completed; }
#endif

void address_bus::load_RAM_buffer(const byte* data, size_t length) {
    if (data == nullptr) {
        return;
    }
    size_t copy_size = std::min(length, static_cast<size_t>(CART_RAM_SIZE));
    std::memcpy(cartRAM.data(), data, copy_size);
}

void address_bus::reset_MBC_state() {
    rom_bank = 1;
    eram_bank = ExternalRamBank::Bank0;
    bank_mode = BankMode::ROM;
    RAMenable = 0;
    mbc7_ram_enable_secondary = 0;
    latch_write = 0;
    rtc_selected_register = 0;
    rtc_halted = false;
    rtc_epoch = host_seconds();
    rtc_registers.fill(0);
    rtc_latched.fill(0);
    hdma_active = false;
    hdma_mode_hblank = false;
    hdma_blocks_remaining = 0;
    bgpi = 0;
    obpi = 0;
    bgpi_auto_increment = false;
    obpi_auto_increment = false;
    bg_palette_raw.fill(0);
    obj_palette_raw.fill(0);
    bg_palette_rgb.fill(0);
    obj_palette_rgb.fill(0);
    rp_state = 0;
    cgb_internal_regs.fill(0);
    opri = 0;
    update_hdma_status_register();
}

void address_bus::mark_palette_dirty() const {
    if (ppu != nullptr) {
        ppu->mark_cache_dirty();
    }
}

void address_bus::writeMBC1(half address, byte value) {
    if (address >= addr(MemoryRegion::ROM_BANK_00) && address <= 0x1FFF) {
        RAMenable = (value & 0x0F) == 0x0A;
        return;
    }

    if (address >= 0x2000 && address <= 0x3FFF) {
        byte bank = value & 0x1F;
        if (bank == 0) bank = 1;
        rom_bank = bank;
        if (bank_mode == BankMode::ROM) {
            rom_bank |= (static_cast<byte>(eram_bank) & 0x03) << 5;
        }
        return;
    }

    if (address >= 0x4000 && address <= 0x5FFF) {
        if (bank_mode == BankMode::ROM) {
            eram_bank = static_cast<ExternalRamBank>((value & 0x03));
            rom_bank = (rom_bank & 0x1F) | ((value & 0x03) << 5);
        } else {
            eram_bank = static_cast<ExternalRamBank>(value & 0x03);
        }
        return;
    }

    if (address >= 0x6000 && address <= 0x7FFF) {
        bank_mode = (value & 0x01) ? BankMode::RAM : BankMode::ROM;
        if (bank_mode == BankMode::ROM) {
            rom_bank = (rom_bank & 0x1F) |
                       ((static_cast<byte>(eram_bank) & 0x03) << 5);
        }
        return;
    }
}

void address_bus::writeMBC3(half address, byte value) {
    if (address >= addr(MemoryRegion::ROM_BANK_00) && address <= 0x1FFF) {
        RAMenable = (value & 0x0F) == 0x0A;
        return;
    }

    if (address >= 0x2000 && address <= 0x3FFF) {
        half bank = value & 0x7F;
        if (bank == 0) bank = 1;
        rom_bank = bank;
        return;
    }

    if (address >= 0x4000 && address <= 0x5FFF) {
        if (value <= 0x03) {
            eram_bank = static_cast<ExternalRamBank>(value);
            rtc_selected_register = 0;
        } else if (value >= 0x08 && value <= 0x0C) {
            rtc_selected_register = value;
            eram_bank = ExternalRamBank::Bank0;
        } else {
            rtc_selected_register = 0;
            eram_bank = ExternalRamBank::Bank0;
        }
        return;
    }

    if (address >= 0x6000 && address <= 0x7FFF) {
        handle_rtc_latch_write(value);
        return;
    }
}

void address_bus::writeMBC2(half address, byte value) {
    if (address >= addr(MemoryRegion::ROM_BANK_00) && address <= 0x1FFF) {
        if ((address & 0x0100) == 0) {
            RAMenable = (value & 0x0F) == 0x0A;
        }
        return;
    }

    if (address >= 0x2000 && address <= 0x3FFF) {
        if ((address & 0x0100) != 0) {
            byte bank = value & 0x0F;
            if (bank == 0) bank = 1;
            rom_bank = bank;
        }
        return;
    }
}

void address_bus::writeMBC5(half address, byte value) {
    if (address >= addr(MemoryRegion::ROM_BANK_00) && address <= 0x1FFF) {
        RAMenable = (value & 0x0F) == 0x0A;
        return;
    }

    if (address >= 0x2000 && address <= 0x2FFF) {
        rom_bank = (rom_bank & 0x100) | value;
        return;
    }

    if (address >= 0x3000 && address <= 0x3FFF) {
        rom_bank = (rom_bank & 0xFF) | ((value & 0x01) << 8);
        return;
    }

    if (address >= 0x4000 && address <= 0x5FFF) {
        eram_bank = static_cast<ExternalRamBank>(value & 0x0F);
        return;
    }
}

// WARNING: MBC6 implementation is incomplete and incorrect
// MBC6 is extremely rare (only 2 games: Net de Get and Pocket Bomberman)
// Correct implementation requires:
// - Two separate 8KB ROM banks (A at 4000-5FFF, B at 6000-7FFF)
// - Two separate 4KB RAM banks (A at A000-AFFF, B at B000-BFFF)
// - Fine-grained register mapping (0000-03FF, 0400-07FF, 0800-0BFF, etc.)
// - Flash memory support with erase/program operations
// Current implementation is a placeholder for basic compatibility
void address_bus::writeMBC6(half address, byte value) {
    if (address >= addr(MemoryRegion::ROM_BANK_00) && address <= 0x1FFF) {
        RAMenable = (value & 0x0F) == 0x0A;
        return;
    }

    if (address >= 0x2000 && address <= 0x2FFF) {
        rom_bank = (rom_bank & 0xFE) | (value & 0x01);
        return;
    }

    if (address >= 0x3000 && address <= 0x3FFF) {
        rom_bank = (rom_bank & 0xFD) | ((value & 0x01) << 1);
        return;
    }

    if (address >= 0x4000 && address <= 0x5FFF) {
        eram_bank = static_cast<ExternalRamBank>(value & 0x0F);
        return;
    }
}

void address_bus::writeMBC7(half address, byte value) {
    if (address >= addr(MemoryRegion::ROM_BANK_00) && address <= 0x1FFF) {
        // MBC7 first RAM enable: write $0A to enable
        RAMenable = (value & 0x0F) == 0x0A;
        return;
    }

    if (address >= 0x2000 && address <= 0x3FFF) {
        // ROM bank selection (extended to full 0x2000-0x3FFF range)
        rom_bank = value;
        if (rom_bank == 0) rom_bank = 1;
        return;
    }

    if (address >= 0x4000 && address <= 0x5FFF) {
        // MBC7 second RAM enable: write $40 to enable (both must be enabled)
        mbc7_ram_enable_secondary = (value == 0x40);
        return;
    }
}

void address_bus::writeHuC1(half address, byte value) {
    if (address >= addr(MemoryRegion::ROM_BANK_00) && address <= 0x1FFF) {
        // HuC1 IR/RAM mode selection: write $0E for IR mode, else RAM mode
        // Some games use $0A/$00, so we approximate with RAM enable behavior
        RAMenable = (value & 0x0F) == 0x0A;
        return;
    }

    if (address >= 0x2000 && address <= 0x3FFF) {
        byte bank = value & 0x3F;
        if (bank == 0) bank = 1;
        rom_bank = bank;
        return;
    }

    if (address >= 0x4000 && address <= 0x5FFF) {
        eram_bank = static_cast<ExternalRamBank>(value & 0x03);
        return;
    }

    // Note: HuC1 writes to 0x6000-0x7FFF have no effect (no bank mode register)
}

void address_bus::writeHuC3(half address, byte value) {
    if (address >= addr(MemoryRegion::ROM_BANK_00) && address <= 0x1FFF) {
        RAMenable = (value & 0x0F) == 0x0A;
        return;
    }

    if (address >= 0x2000 && address <= 0x3FFF) {
        // HuC3 allows bank 0 to be selected (like MBC5)
        rom_bank = value & 0x7F;
        return;
    }

    if (address >= 0x4000 && address <= 0x5FFF) {
        eram_bank = static_cast<ExternalRamBank>(value & 0x03);
        return;
    }
}

}  // namespace GBC