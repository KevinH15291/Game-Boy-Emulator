#pragma once

#include <enums.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

#include "../CgbConfig.h"
#include "../DebugMacros.h"
#include "../bit_ops.h"

namespace GBC {

class APU;
class PPU;
class SM83;

byte apu_read_register(APU *, half);
byte apu_read_wave(APU *, half);
void apu_write_register(APU *, half, byte);
void apu_write_wave(APU *, half, byte);
void ppu_io_write(PPU *, half, byte);
void cpu_reset_timer_counter(SM83 *);
void cpu_mark_tima_written(SM83 *);

using byte = uint8_t;
using half = uint16_t;

constexpr size_t KB = 1ull << 10;
constexpr size_t MB = 1ull << 20;
constexpr size_t MAX_CART_ROM_SIZE = 8 * MB;

constexpr size_t BOOTROM_SIZE = 0x100;
constexpr size_t CART_RAM_SIZE = static_cast<size_t>(32) * KB;
constexpr size_t WORK_RAM_SIZE = static_cast<size_t>(32) * KB;
constexpr size_t VIDEO_RAM_SIZE = static_cast<size_t>(16) * KB;
constexpr size_t OAM_SIZE = 0xA0;
constexpr size_t IO_RANGE_SIZE = 256;
constexpr size_t HRAM_SIZE = 128;

constexpr byte BYTE_MAX = 0xFF;

constexpr half addr(MemoryRegion region) { return static_cast<half>(region); }
constexpr half addr(IORegister reg) { return static_cast<half>(reg); }
constexpr half addr(AudioRegister reg) { return static_cast<half>(reg); }
constexpr half addr(VideoRegister reg) { return static_cast<half>(reg); }
constexpr half addr(CGBRegister reg) { return static_cast<half>(reg); }

template <typename Enum>
constexpr auto to_underlying(Enum e) noexcept {
    return static_cast<std::underlying_type_t<Enum>>(e);
}

enum class RenderingState : uint8_t {
    hblank = 0,
    vblank = 1,
    OAMscan = 2,
    draw = 3
};

enum class MemoryBankController : uint8_t {
    None = 0x00,
    MBC1 = 0x01,
    MBC1_RAM = 0x02,
    MBC1_RAM_BATTERY = 0x03,
    MBC2 = 0x05,
    MBC2_BATTERY = 0x06,
    MMM01 = 0x0B,
    MMM01_RAM = 0x0C,
    MMM01_RAM_BATTERY = 0x0D,
    MBC3_RTC_BATTERY = 0x0F,
    MBC3_RTC_RAM_BATTERY = 0x10,
    MBC3 = 0x11,
    MBC3_RAM = 0x12,
    MBC3_RAM_BATTERY = 0x13,
    MBC5 = 0x19,
    MBC5_RAM = 0x1A,
    MBC5_RAM_BATTERY = 0x1B,
    MBC5_RUMBLE = 0x1C,
    MBC6 = 0x20,
    MBC7 = 0x22,
    HuC1 = 0xFE,
    HuC3 = 0xFF
};

#include "bus_internal.h"

enum class BankMode : uint8_t { ROM = 0, RAM = 1 };

enum class VramBank : uint8_t { Bank0 = 0, Bank1 = 1 };

enum class WramBank : uint8_t {
    Bank0 = 0,
    Bank1 = 1,
    Bank2 = 2,
    Bank3 = 3,
    Bank4 = 4,
    Bank5 = 5,
    Bank6 = 6,
    Bank7 = 7
};

enum class ExternalRamBank : uint8_t {
    Bank0 = 0,
    Bank1 = 1,
    Bank2 = 2,
    Bank3 = 3,
    Bank4 = 4,
    Bank5 = 5,
    Bank6 = 6,
    Bank7 = 7,
    Bank8 = 8,
    Bank9 = 9,
    Bank10 = 10,
    Bank11 = 11,
    Bank12 = 12,
    Bank13 = 13,
    Bank14 = 14,
    Bank15 = 15
};

class SM83;
class address_bus {
   public:
    explicit address_bus(CgbConfig &config);
    address_bus(address_bus &) = delete;
    address_bus(address_bus &&) = delete;
    auto operator=(address_bus &) = delete;
    auto operator=(address_bus &&) = delete;
    ~address_bus() = default;
    void set_cpu(SM83 *cpu_ptr) { cpu = cpu_ptr; }

    byte read(half address);
    byte read_privileged(half address);
    void write_privileged(half address, byte value);
    inline byte readIO(half address);

    byte check_set(half address, byte value);
    void set_bit(half address, byte bit);
    void reset_bit(half address, byte bit);

    void load_boot_ROM(const char *fname, uint32_t size);
    void load_ROM(const char *fname);
#ifdef __EMSCRIPTEN__
    void load_ROM_buffer(const byte *data, size_t length);
    void load_RAM_buffer(const byte *data, size_t length);
    void set_boot_complete(bool completed);
    void set_booting(bool value) { booting = value; }
#endif
    void reset_MBC_state();

    void load_RAM(const char *fname, uint32_t size);

    void write(half address, byte value);
    void writeMBC1(half address, byte value);
    void writeMBC2(half address, byte value);
    void writeMBC3(half address, byte value);
    void writeMBC5(half address, byte value);
    void writeMBC6(half address, byte value);
    void writeMBC7(half address, byte value);
    void writeHuC1(half address, byte value);
    void writeHuC3(half address, byte value);

    inline void writeIO(half address, byte val);

    byte *get_cartRAM() { return cartRAM.data(); }
    [[nodiscard]] const byte *get_cartRAM() const { return cartRAM.data(); }

    void toggle_booting() { booting = !booting; }
    void sync_key_registers();
    [[nodiscard]] byte read_vram(byte bank, half offset,
                                 bool privileged = false) const;
    void write_vram(byte bank, half offset, byte value,
                    bool privileged = false);
    [[nodiscard]] uint32_t get_bg_color(byte palette, byte color) const;
    [[nodiscard]] uint32_t get_obj_color(byte palette, byte color) const;
    [[nodiscard]] bool bg_priority_over_obj() const {
        return (opri & 0x01) != 0;
    }
    void handle_hblank_hdma();

   private:
    CgbConfig &config;
    std::array<byte, BOOTROM_SIZE> bootrom;
    std::vector<byte> cartROM;
    std::array<byte, CART_RAM_SIZE> cartRAM;
    std::array<byte, WORK_RAM_SIZE> workRAM;
    std::array<byte, VIDEO_RAM_SIZE> videoRAM;
    std::array<byte, OAM_SIZE> OAM;
    std::array<byte, IO_RANGE_SIZE> IOrange;
    std::array<byte, HRAM_SIZE> HRAM;
    std::array<byte, 5> rtc_registers = {0, 0, 0, 0, 0};
    std::array<byte, 5> rtc_latched = {0, 0, 0, 0, 0};
    bool rtc_halted = false;
    uint64_t rtc_epoch = 0;
    byte rtc_selected_register = 0;

    RenderingState lcd_mode = RenderingState::hblank;

    MemoryBankController mbc = MemoryBankController::None;
    VramBank vram_bank = VramBank::Bank0;
    WramBank wram_bank = WramBank::Bank1;
    half rom_bank = 1;
    ExternalRamBank eram_bank = ExternalRamBank::Bank0;
    BankMode bank_mode = BankMode::ROM;
    byte rom_size = 0;

    byte input_d = 0x0F;
    byte input_s = 0x0F;
    byte IEnable = 0;
    byte RAMenable = 0;
    byte latch_write = 0;
    byte debug_value = 0;
    byte bgpi = 0;
    bool bgpi_auto_increment = false;
    byte obpi = 0;
    bool obpi_auto_increment = false;
    std::array<byte, 8 * 4 * 2> bg_palette_raw{};
    std::array<byte, 8 * 4 * 2> obj_palette_raw{};
    std::array<uint32_t, 8 * 4> bg_palette_rgb{};
    std::array<uint32_t, 8 * 4> obj_palette_rgb{};
    half hdma_src = 0;
    half hdma_dst = 0;
    byte hdma_blocks_remaining = 0;
    bool hdma_active = false;
    bool hdma_mode_hblank = false;
    byte rp_state = 0;
    std::array<byte, 4> cgb_internal_regs{};
    byte opri = 0;

    bool booting = false;
    bool latched = false;

    class APU *apu = nullptr;
    class PPU *ppu = nullptr;
    SM83 *cpu = nullptr;

    friend class SM83;
    friend class PPU;
    friend class GBC;
    friend class APU;

    static constexpr uint32_t rgb15_to_rgb888(half raw);
    void refresh_bg_palette_entry(byte byte_index);
    void refresh_obj_palette_entry(byte byte_index);
    void increment_bgpi();
    void increment_obpi();
    void mark_palette_dirty() const;
    void start_general_hdma(byte block_count);
    void start_hblank_hdma(byte block_count);
    void complete_hdma_block();
    void cancel_hdma();
    void update_hdma_status_register();
    byte read_internal(half address, bool privileged);
    void write_internal(half address, byte value, bool privileged);
    [[nodiscard]] byte read_joyp_state() const;
    void handle_rtc_register_write(byte reg_index, byte value);
    void handle_rtc_latch_write(byte value);
    void handle_hdma_register(half address, byte value);
};

#ifndef __EMSCRIPTEN__
inline uint64_t host_seconds() {
    using clock = std::chrono::system_clock;
    return std::chrono::duration_cast<std::chrono::seconds>(
               clock::now().time_since_epoch())
        .count();
}
#else
inline uint64_t host_seconds() { return 0; }
#endif

inline uint64_t decode_rtc_registers(const std::array<byte, 5> &regs) {
    const uint64_t seconds = regs[0] & 0x3F;
    const uint64_t minutes = regs[1] & 0x3F;
    const uint64_t hours = regs[2] & 0x1F;
    const uint64_t day_low = regs[3];
    const uint64_t day_high = regs[4] & 0x01;
    const uint64_t days = (day_high << 8) | day_low;
    return (((days * 24) + hours) * 60 + minutes) * 60 + seconds;
}

inline void encode_rtc_seconds(uint64_t seconds, bool halted,
                               std::array<byte, 5> &regs) {
    uint64_t days = seconds / (60ull * 60 * 24);
    seconds %= (60ull * 60 * 24);
    const uint64_t hours = seconds / (60ull * 60);
    seconds %= (60ull * 60);
    const uint64_t minutes = seconds / 60ull;
    const uint64_t secs = seconds % 60ull;

    regs[0] = static_cast<byte>(secs & 0x3F);
    regs[1] = static_cast<byte>(minutes & 0x3F);
    regs[2] = static_cast<byte>(hours & 0x1F);
    regs[3] = static_cast<byte>(days & 0xFF);

    byte control = 0;
    control |= static_cast<byte>((days >> 8) & 0x01);
    if (halted) {
        control |= 0x40;
    }
    if (days > 0x1FF) {
        control |= 0x80;
    }
    regs[4] = control;
}

constexpr uint32_t address_bus::rgb15_to_rgb888(half raw) {
    const byte r = static_cast<byte>(getBitRange(raw, 0, 5) << 3);
    const byte g = static_cast<byte>(getBitRange(raw, 5, 5) << 3);
    const byte b = static_cast<byte>(getBitRange(raw, 10, 5) << 3);
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) |
           b;
}

inline void address_bus::refresh_bg_palette_entry(byte byte_index) {
    const byte aligned_index = byte_index & 0x3E;
    const byte low = bg_palette_raw[aligned_index];
    const byte high = bg_palette_raw[aligned_index + 1];
    const half color =
        static_cast<half>(low) | (static_cast<half>(high & 0x7F) << 8);
    const size_t color_index = aligned_index / 2;
    bg_palette_rgb[color_index] = rgb15_to_rgb888(color);
}

inline void address_bus::refresh_obj_palette_entry(byte byte_index) {
    const byte aligned_index = byte_index & 0x3E;
    const byte low = obj_palette_raw[aligned_index];
    const byte high = obj_palette_raw[aligned_index + 1];
    const half color =
        static_cast<half>(low) | (static_cast<half>(high & 0x7F) << 8);
    const size_t color_index = aligned_index / 2;
    obj_palette_rgb[color_index] = rgb15_to_rgb888(color);
}

inline void address_bus::increment_bgpi() { bgpi = (bgpi + 1) & 0x3F; }

inline void address_bus::increment_obpi() { obpi = (obpi + 1) & 0x3F; }

inline void address_bus::sync_key_registers() {
    constexpr auto io_base = addr(MemoryRegion::IO_REGISTERS);
    const size_t key0_index =
        static_cast<size_t>(addr(CGBRegister::KEY0) - io_base);
    const size_t key1_index =
        static_cast<size_t>(addr(CGBRegister::KEY1) - io_base);
    const byte key0_value = config.cgb_mode ? 0x7F : 0xFF;
    byte key1_value = 0x7E;
    if (config.double_speed) {
        key1_value |= 0x80;
    }
    if (config.speed_switch_armed) {
        key1_value |= 0x01;
    }
    IOrange[key0_index] = key0_value;
    IOrange[key1_index] = key1_value;
}

inline void address_bus::start_general_hdma(byte block_count) {
    if (!config.cgb_mode) {
        return;
    }
    hdma_active = false;
    hdma_mode_hblank = false;
    hdma_blocks_remaining = static_cast<byte>(block_count + 1);
    while (hdma_blocks_remaining > 0) {
        complete_hdma_block();
        --hdma_blocks_remaining;
    }
    update_hdma_status_register();
}

inline void address_bus::start_hblank_hdma(byte block_count) {
    if (!config.cgb_mode) {
        update_hdma_status_register();
        return;
    }
    hdma_active = true;
    hdma_mode_hblank = true;
    hdma_blocks_remaining = static_cast<byte>(block_count + 1);
    update_hdma_status_register();
}

inline void address_bus::complete_hdma_block() {
    half current_src = (hdma_src & 0xFFF0);
    half current_dst = 0x8000 | (hdma_dst & 0x1FF0);
    for (half i = 0; i < 0x10; ++i) {
        const byte value = read_privileged(current_src + i);
        const half vram_offset = static_cast<half>(
            (current_dst + i) - addr(MemoryRegion::VIDEO_RAM));
        write_vram(static_cast<byte>(vram_bank), vram_offset, value, true);
    }
    hdma_src = static_cast<half>((hdma_src & 0xFFF0) + 0x10);
    hdma_dst = static_cast<half>((hdma_dst + 0x10) & 0x1FF0);
    mark_palette_dirty();
}

inline void address_bus::cancel_hdma() {
    hdma_active = false;
    hdma_mode_hblank = false;
    hdma_blocks_remaining = 0;
    update_hdma_status_register();
}

inline void address_bus::handle_hblank_hdma() {
    if (!hdma_active || !hdma_mode_hblank) {
        return;
    }
    complete_hdma_block();
    if (hdma_blocks_remaining > 0) {
        --hdma_blocks_remaining;
    }
    if (hdma_blocks_remaining == 0) {
        hdma_active = false;
        hdma_mode_hblank = false;
    }
    update_hdma_status_register();
}

inline void address_bus::update_hdma_status_register() {
    constexpr size_t ff55_index = static_cast<size_t>(
        addr(CGBRegister::HDMA5) - addr(MemoryRegion::IO_REGISTERS));
    if (!config.cgb_mode || !hdma_active) {
        IOrange[ff55_index] = 0xFF;
        return;
    }
    const byte remaining =
        hdma_blocks_remaining > 0
            ? static_cast<byte>((hdma_blocks_remaining - 1) & 0x7F)
            : 0;
    IOrange[ff55_index] = static_cast<byte>(0x80 | remaining);
}

inline void address_bus::handle_rtc_register_write(byte reg_index, byte value) {
    switch (reg_index) {
        case 0:
            rtc_registers[0] = static_cast<byte>(value & 0x3F);
            break;
        case 1:
            rtc_registers[1] = static_cast<byte>(value & 0x3F);
            break;
        case 2:
            rtc_registers[2] = static_cast<byte>(value & 0x1F);
            break;
        case 3:
            rtc_registers[3] = value;
            break;
        case 4: {
            const byte previous_control = rtc_registers[4];
            byte control = static_cast<byte>(previous_control & 0x80);
            control |= static_cast<byte>(value & 0x01);
            const bool new_halt = (value & 0x40) != 0;
            if (new_halt) {
                control = static_cast<byte>(control | 0x40);
            } else {
                control = static_cast<byte>(control & static_cast<byte>(~0x40));
            }
            if ((value & 0x80) == 0) {
                control = static_cast<byte>(control & static_cast<byte>(~0x80));
            }

            if (!rtc_halted && new_halt) {
                const uint64_t seconds =
                    host_seconds() > rtc_epoch ? host_seconds() - rtc_epoch : 0;
                encode_rtc_seconds(seconds, true, rtc_registers);
            } else if (rtc_halted && !new_halt) {
                const uint64_t seconds = decode_rtc_registers(rtc_registers);
                const uint64_t now = host_seconds();
                rtc_epoch = now > seconds ? (now - seconds) : now;
            }

            rtc_halted = new_halt;
            rtc_registers[4] = control;
            break;
        }
    }

    if (!rtc_halted) {
        const uint64_t seconds = decode_rtc_registers(rtc_registers);
        const uint64_t now = host_seconds();
        rtc_epoch = now > seconds ? (now - seconds) : now;
    }
}

inline void address_bus::handle_rtc_latch_write(byte value) {
    if (latch_write == 0x00 && value == 0x01) {
        uint64_t seconds =
            rtc_halted
                ? decode_rtc_registers(rtc_registers)
                : (host_seconds() > rtc_epoch ? host_seconds() - rtc_epoch : 0);
        encode_rtc_seconds(seconds, rtc_halted, rtc_latched);
    }
    latch_write = value;
}

inline void address_bus::handle_hdma_register(half address, byte value) {
    switch (address) {
        case addr(CGBRegister::HDMA1):
            hdma_src = static_cast<half>((static_cast<half>(value) << 8) |
                                         (hdma_src & 0x00FF));
            return;
        case addr(CGBRegister::HDMA2):
            hdma_src = static_cast<half>((hdma_src & 0xFF00) | (value & 0xF0));
            return;
        case addr(CGBRegister::HDMA3):
            hdma_dst =
                static_cast<half>(((value & 0x1F) << 8) | (hdma_dst & 0x00FF));
            return;
        case addr(CGBRegister::HDMA4):
            hdma_dst = static_cast<half>((hdma_dst & 0xFF00) | (value & 0xF0));
            return;
        case addr(CGBRegister::HDMA5):
            if (!config.cgb_mode) {
                IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = 0xFF;
                return;
            }
            if ((value & 0x80) == 0) {
                if (hdma_active && hdma_mode_hblank) {
                    cancel_hdma();
                }
                start_general_hdma(value & 0x7F);
            } else {
                if (hdma_active && !hdma_mode_hblank) {
                    return;
                }
                start_hblank_hdma(value & 0x7F);
            }
            return;
    }
}

inline byte address_bus::read(half address) {
    return read_internal(address, false);
}

inline byte address_bus::read_privileged(half address) {
    return read_internal(address, true);
}

inline byte address_bus::read_internal(half address, bool privileged) {
    if (bus_internal::in_region(address, bus_internal::kRomBank00)) {
        if (address < 0x0100 && booting) {
            return bootrom[address];
        }
        return cartROM[address];
    }

    if (bus_internal::in_region(address, bus_internal::kRomBankNN)) {
        half bank = rom_bank;
        if (mbc == MemoryBankController::MBC5 ||
            mbc == MemoryBankController::MBC5_RAM ||
            mbc == MemoryBankController::MBC5_RAM_BATTERY ||
            mbc == MemoryBankController::MBC5_RUMBLE) {
            bank &= 0x1FF;
        } else if (mbc == MemoryBankController::MBC1 ||
                   mbc == MemoryBankController::MBC1_RAM ||
                   mbc == MemoryBankController::MBC1_RAM_BATTERY) {
            if (rom_size <= 0x04) {
                bank &= 0x1F;
            } else {
                bank &= 0x7F;
            }
        } else {
            bank &= 0x7F;
        }
        return cartROM[address + 16 * KB * bank -
                       addr(MemoryRegion::ROM_BANK_NN)];
    }

    if (bus_internal::in_region(address, bus_internal::kWorkRam0)) {
        return workRAM[address - bus_internal::kWorkRam0.begin];
    }

    if (bus_internal::in_region(address, bus_internal::kWorkRamN)) {
        return workRAM[address + static_cast<int>(wram_bank) * 4 * KB -
                       bus_internal::kWorkRamN.begin];
    }

    if (address >= addr(MemoryRegion::ECHO_RAM1) &&
        address <= addr(MemoryRegion::EECHO_RAM2)) {
        half echo_address = static_cast<half>(address - 0x2000);
        if (bus_internal::in_region(echo_address, bus_internal::kWorkRam0)) {
            return workRAM[echo_address - bus_internal::kWorkRam0.begin];
        }
        return workRAM[echo_address + static_cast<int>(wram_bank) * 4 * KB -
                       bus_internal::kWorkRamN.begin];
    }

    if (bus_internal::in_region(address, bus_internal::kVideoRam)) {
        const half offset = address - bus_internal::kVideoRam.begin;
        return read_vram(static_cast<byte>(vram_bank), offset, privileged);
    }

    if (bus_internal::in_region(address, bus_internal::kOamRange)) {
        if (!privileged && (lcd_mode == RenderingState::draw ||
                            lcd_mode == RenderingState::OAMscan)) {
            return 0xFF;
        }
        return OAM[address - bus_internal::kOamRange.begin];
    }

    if (bus_internal::in_region(address, bus_internal::kIoRange)) {
        return privileged ? IOrange[address - bus_internal::kIoRange.begin]
                          : readIO(address);
    }

    if (bus_internal::in_region(address, bus_internal::kHighRam)) {
        return HRAM[address - bus_internal::kHighRam.begin];
    }

    if (bus_internal::in_region(address, bus_internal::kExternalRam)) {
        if (!RAMenable) {
            return 0xFF;
        }
        if (bus_internal::is_mbc3_controller(mbc) &&
            rtc_selected_register != 0) {
            byte reg_index = rtc_selected_register - 0x08;
            if (reg_index < 5) {
                return rtc_latched[reg_index];
            }
            return 0xFF;
        }
        if (mbc == MemoryBankController::MBC2 ||
            mbc == MemoryBankController::MBC2_BATTERY) {
            return (cartRAM[address - bus_internal::kExternalRam.begin] &
                    0x0F) |
                   0xF0;
        }
        return cartRAM[address + 8 * KB * static_cast<int>(eram_bank) -
                       bus_internal::kExternalRam.begin];
    }

    if (bus_internal::in_region(address, bus_internal::kNotUsable)) {
        return 0x00;
    }

    if (address == addr(MemoryRegion::IE)) return IEnable;

    return 0xFF;
}

inline byte address_bus::readIO(half address) {
    if (address == addr(IORegister::JOYP)) {
        return read_joyp_state();
    }

    if (bus_internal::is_audio_register(address)) {
        if (apu != nullptr) {
            return apu_read_register(apu, address);
        }
        byte value = IOrange[address - addr(MemoryRegion::IO_REGISTERS)];
        switch (address) {
            case addr(AudioRegister::NR10):
                return value | 0x80;
            case addr(AudioRegister::NR11):
                return value | 0x3F;
            case addr(AudioRegister::NR12):
                return value;
            case addr(AudioRegister::NR13):
                return 0xFF;
            case addr(AudioRegister::NR14):
                return value | 0xBF;
            case addr(AudioRegister::NR21):
                return value | 0x3F;
            case addr(AudioRegister::NR22):
                return value;
            case addr(AudioRegister::NR23):
                return 0xFF;
            case addr(AudioRegister::NR24):
                return value | 0xBF;
            case addr(AudioRegister::NR30):
                return value | 0x7F;
            case addr(AudioRegister::NR31):
                return 0xFF;
            case addr(AudioRegister::NR32):
                return value | 0x9F;
            case addr(AudioRegister::NR33):
                return 0xFF;
            case addr(AudioRegister::NR34):
                return value | 0xBF;
            case addr(AudioRegister::NR41):
                return 0xFF;
            case addr(AudioRegister::NR42):
                return value;
            case addr(AudioRegister::NR43):
                return value;
            case addr(AudioRegister::NR44):
                return value | 0xBF;
            case addr(AudioRegister::NR50):
                return value;
            case addr(AudioRegister::NR51):
                return value;
            case addr(AudioRegister::NR52):
                return value;
            default:
                return value;
        }
    }

    if (bus_internal::is_wave_table_register(address)) {
        if (apu != nullptr) {
            return apu_read_wave(apu, address);
        }
        return IOrange[address - addr(MemoryRegion::IO_REGISTERS)];
    }

    switch (address) {
        case addr(VideoRegister::LCDC):
        case addr(VideoRegister::STAT):
        case addr(VideoRegister::SCY):
        case addr(VideoRegister::SCX):
        case addr(VideoRegister::LY):
        case addr(VideoRegister::LYC):
        case addr(VideoRegister::BGP):
        case addr(VideoRegister::OBP0):
        case addr(VideoRegister::OBP1):
        case addr(VideoRegister::WY):
        case addr(VideoRegister::WX):
            return IOrange[address - addr(MemoryRegion::IO_REGISTERS)];
        case addr(CGBRegister::KEY0):
            return config.cgb_mode ? 0x7F : 0xFF;
        case addr(CGBRegister::KEY1):
            return config.cgb_mode
                       ? IOrange[address - addr(MemoryRegion::IO_REGISTERS)]
                       : 0xFF;
        case addr(CGBRegister::VBK):
            return config.cgb_mode
                       ? static_cast<byte>(
                             0xFE | (static_cast<byte>(vram_bank) & 0x01))
                       : 0xFF;
        case addr(CGBRegister::HDMA1):
            return static_cast<byte>(hdma_src >> 8);
        case addr(CGBRegister::HDMA2):
            return static_cast<byte>(hdma_src & 0xF0);
        case addr(CGBRegister::HDMA3):
            return static_cast<byte>(getBitRange(hdma_dst, 8, 5));
        case addr(CGBRegister::HDMA4):
            return static_cast<byte>(hdma_dst & 0xF0);
        case addr(CGBRegister::HDMA5):
            return IOrange[address - addr(MemoryRegion::IO_REGISTERS)];
        case addr(CGBRegister::RP):
            return static_cast<byte>((rp_state & 0xC1) | 0x02);
        case addr(CGBRegister::BGPI):
            return config.cgb_mode
                       ? static_cast<byte>((bgpi & 0x3F) |
                                           (bgpi_auto_increment ? 0x80 : 0))
                       : 0xFF;
        case addr(CGBRegister::BGPD):
            return config.cgb_mode ? bg_palette_raw[bgpi] : 0xFF;
        case addr(CGBRegister::OBPI):
            return config.cgb_mode
                       ? static_cast<byte>((obpi & 0x3F) |
                                           (obpi_auto_increment ? 0x80 : 0))
                       : 0xFF;
        case addr(CGBRegister::OBPD):
            return config.cgb_mode ? obj_palette_raw[obpi] : 0xFF;
        case addr(CGBRegister::OPRI):
            return static_cast<byte>(0xFE | (opri & 0x01));
        case addr(CGBRegister::SVBK):
            return static_cast<byte>(0xF8 | static_cast<byte>(wram_bank));
        case 0xFF72:
            return cgb_internal_regs[0];
        case 0xFF73:
            return cgb_internal_regs[1];
        case 0xFF74:
            return cgb_internal_regs[2];
        case 0xFF75:
            return cgb_internal_regs[3];
        default:
            return IOrange[address - addr(MemoryRegion::IO_REGISTERS)];
    }
}

inline void address_bus::write(half address, byte value) {
    write_internal(address, value, false);
}

inline void address_bus::write_privileged(half address, byte value) {
    write_internal(address, value, true);
}

inline void address_bus::write_internal(half address, byte value,
                                        bool privileged) {
    if (bus_internal::in_region(address, bus_internal::kWorkRam0)) {
        workRAM[address - bus_internal::kWorkRam0.begin] = value;
        return;
    }

    if (bus_internal::in_region(address, bus_internal::kWorkRamN)) {
        workRAM[address + static_cast<int>(wram_bank) * 4 * KB -
                addr(MemoryRegion::WORK_RAM_BANKN)] = value;
        return;
    }

    if (address >= addr(MemoryRegion::ECHO_RAM1) &&
        address <= addr(MemoryRegion::EECHO_RAM2)) {
        half echo_address = static_cast<half>(address - 0x2000);
        if (bus_internal::in_region(echo_address, bus_internal::kWorkRam0)) {
            workRAM[echo_address - bus_internal::kWorkRam0.begin] = value;
            return;
        }
        workRAM[echo_address + static_cast<int>(wram_bank) * 4 * KB -
                bus_internal::kWorkRamN.begin] = value;
        return;
    }

    if (bus_internal::in_region(address, bus_internal::kVideoRam)) {
        const half offset = address - bus_internal::kVideoRam.begin;
        write_vram(static_cast<byte>(vram_bank), offset, value, privileged);
        return;
    }

    if (bus_internal::in_region(address, bus_internal::kOamRange)) {
        if (!privileged && (lcd_mode == RenderingState::draw ||
                            lcd_mode == RenderingState::OAMscan)) {
            return;
        }
        OAM[address - bus_internal::kOamRange.begin] = value;
        return;
    }

    if (bus_internal::in_region(address, bus_internal::kIoRange)) {
        writeIO(address, value);
        return;
    }

    if (bus_internal::in_region(address, bus_internal::kHighRam)) {
        HRAM[address - bus_internal::kHighRam.begin] = value;
        return;
    }

    if (bus_internal::in_region(address, bus_internal::kExternalRam)) {
        if (!RAMenable) {
            return;
        }
        if (bus_internal::is_mbc3_controller(mbc) &&
            rtc_selected_register != 0) {
            byte reg_index = rtc_selected_register - 0x08;
            if (reg_index < 5) {
                handle_rtc_register_write(reg_index, value);
            }
            return;
        }
        if (mbc == MemoryBankController::MBC2 ||
            mbc == MemoryBankController::MBC2_BATTERY) {
            cartRAM[address - bus_internal::kExternalRam.begin] =
                static_cast<byte>((value & 0x0F) | 0xF0);
            return;
        }
        cartRAM[address + 8 * KB * static_cast<int>(eram_bank) -
                bus_internal::kExternalRam.begin] = value;
        return;
    }

    if (address == addr(MemoryRegion::IE)) {
        IEnable = value;
        return;
    }

    switch (mbc) {
        case MemoryBankController::None:
            return;
        case MemoryBankController::MBC1:
        case MemoryBankController::MBC1_RAM:
        case MemoryBankController::MBC1_RAM_BATTERY:
            writeMBC1(address, value);
            return;
        case MemoryBankController::MBC2:
        case MemoryBankController::MBC2_BATTERY:
            writeMBC2(address, value);
            return;
        case MemoryBankController::MMM01:
        case MemoryBankController::MMM01_RAM:
        case MemoryBankController::MMM01_RAM_BATTERY:
            writeMBC1(address, value);
            return;
        case MemoryBankController::MBC3_RTC_BATTERY:
        case MemoryBankController::MBC3_RTC_RAM_BATTERY:
        case MemoryBankController::MBC3:
        case MemoryBankController::MBC3_RAM:
        case MemoryBankController::MBC3_RAM_BATTERY:
            writeMBC3(address, value);
            return;
        case MemoryBankController::MBC5:
        case MemoryBankController::MBC5_RAM:
        case MemoryBankController::MBC5_RAM_BATTERY:
        case MemoryBankController::MBC5_RUMBLE:
            writeMBC5(address, value);
            return;
        case MemoryBankController::MBC6:
            writeMBC6(address, value);
            return;
        case MemoryBankController::MBC7:
            writeMBC7(address, value);
            return;
        case MemoryBankController::HuC1:
            writeHuC1(address, value);
            return;
        case MemoryBankController::HuC3:
            writeHuC3(address, value);
            return;
        default:
            return;
    }
}

inline void address_bus::writeIO(half address, byte val) {
    switch (address) {
        case addr(IORegister::JOYP):
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] =
                (val & 0xF0) |
                (IOrange[address - addr(MemoryRegion::IO_REGISTERS)] & 0x0F);
            return;
        case addr(IORegister::DIV):
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = 0;
            if (cpu != nullptr) {
                cpu_reset_timer_counter(cpu);
            }
            return;
        case addr(IORegister::TIMA):
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            if (cpu != nullptr) {
                cpu_mark_tima_written(cpu);
            }
            return;
        case addr(IORegister::TMA):
        case addr(IORegister::TAC):
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        default:
            break;
    }

    if (bus_internal::is_audio_register(address)) {
        if (apu != nullptr) {
            apu_write_register(apu, address, val);
        } else {
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
        }
        return;
    }

    if (bus_internal::is_wave_table_register(address)) {
        if (apu != nullptr) {
            apu_write_wave(apu, address, val);
        } else {
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
        }
        return;
    }

    switch (address) {
        case addr(VideoRegister::LCDC):
        case addr(VideoRegister::STAT):
        case addr(VideoRegister::SCY):
        case addr(VideoRegister::SCX):
        case addr(VideoRegister::LY):
        case addr(VideoRegister::LYC):
        case addr(VideoRegister::BGP):
        case addr(VideoRegister::OBP0):
        case addr(VideoRegister::OBP1):
        case addr(VideoRegister::WY):
        case addr(VideoRegister::WX):
            if (ppu != nullptr) {
                ppu_io_write(ppu, address, val);
            } else if (address == addr(VideoRegister::STAT)) {
                IOrange[address - addr(MemoryRegion::IO_REGISTERS)] =
                    (val & 0xF8) |
                    (IOrange[address - addr(MemoryRegion::IO_REGISTERS)] &
                     0x07);
            } else if (address != addr(VideoRegister::LY)) {
                IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            }
            return;
        case addr(VideoRegister::OAMDMA):
            for (size_t i = 0; i < OAM_SIZE; ++i) {
                OAM[i] = this->read_privileged(static_cast<half>(i) +
                                               (static_cast<half>(val) << 8));
            }
            return;
        case addr(CGBRegister::KEY0):
            return;
        case addr(CGBRegister::KEY1):
            if (!config.cgb_mode) return;
            config.speed_switch_armed = (val & 0x01) != 0;
            sync_key_registers();
            return;
        case addr(CGBRegister::VBK):
            if (!config.cgb_mode) return;
            vram_bank = static_cast<VramBank>(val & 0x01);
            return;
        case addr(CGBRegister::HDMA1):
        case addr(CGBRegister::HDMA2):
        case addr(CGBRegister::HDMA3):
        case addr(CGBRegister::HDMA4):
        case addr(CGBRegister::HDMA5):
            handle_hdma_register(address, val);
            return;
        case addr(CGBRegister::RP):
            rp_state = val & 0xC1;
            return;
        case addr(CGBRegister::BGPI):
            if (!config.cgb_mode) return;
            bgpi = val & 0x3F;
            bgpi_auto_increment = (val & 0x80) != 0;
            return;
        case addr(CGBRegister::BGPD):
            if (!config.cgb_mode) return;
            bg_palette_raw[bgpi] = val;
            refresh_bg_palette_entry(bgpi);
            mark_palette_dirty();
            if (bgpi_auto_increment) {
                increment_bgpi();
            }
            return;
        case addr(CGBRegister::OBPI):
            if (!config.cgb_mode) return;
            obpi = val & 0x3F;
            obpi_auto_increment = (val & 0x80) != 0;
            return;
        case addr(CGBRegister::OBPD):
            if (!config.cgb_mode) return;
            obj_palette_raw[obpi] = val;
            refresh_obj_palette_entry(obpi);
            mark_palette_dirty();
            if (obpi_auto_increment) {
                increment_obpi();
            }
            return;
        case addr(CGBRegister::OPRI):
            opri = val & 0x01;
            return;
        case addr(CGBRegister::SVBK): {
            if (!config.cgb_mode) return;
            byte bank = val & 0x07;
            if (bank == 0) bank = 1;
            wram_bank = static_cast<WramBank>(bank);
            return;
        }
        case 0xFF72:
            cgb_internal_regs[0] = val;
            return;
        case 0xFF73:
            cgb_internal_regs[1] = val;
            return;
        case 0xFF74:
            cgb_internal_regs[2] = val;
            return;
        case 0xFF75:
            cgb_internal_regs[3] = val & 0x0F;
            return;
        case addr(IORegister::IF):
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        default:
            break;
    }

    IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
}

inline byte address_bus::read_vram(byte bank, half offset,
                                   bool privileged) const {
    if (!privileged && lcd_mode == RenderingState::draw) {
        return 0xFF;
    }
    bank &= 0x1;
    offset &= 0x1FFF;
    const size_t index =
        static_cast<size_t>(bank) * 8 * KB + static_cast<size_t>(offset);
    return videoRAM[index];
}

inline void address_bus::write_vram(byte bank, half offset, byte value,
                                    bool privileged) {
    if (!privileged && lcd_mode == RenderingState::draw) {
        return;
    }
    bank &= 0x1;
    offset &= 0x1FFF;
    const size_t index =
        static_cast<size_t>(bank) * 8 * KB + static_cast<size_t>(offset);
    videoRAM[index] = value;
}

inline byte address_bus::read_joyp_state() const {
    const byte joyp = IOrange[bus_internal::kJoypIndex];
    const byte select = static_cast<byte>((joyp >> 4) & 0x03);
    const byte upper = static_cast<byte>(joyp & 0x30);
    if (select == 0) {
        return static_cast<byte>(upper | (input_s & input_d & 0x0F));
    }
    if (!isBitSet(joyp, 4)) {
        return static_cast<byte>(upper | (input_d & 0x0F));
    }
    if (!isBitSet(joyp, 5)) {
        return static_cast<byte>(upper | (input_s & 0x0F));
    }
    return 0x3F;
}

inline uint32_t address_bus::get_bg_color(byte palette, byte color) const {
    if (config.cgb_mode) {
        const size_t idx = static_cast<size_t>(palette % 8) * 4 + (color & 0x3);
        return bg_palette_rgb[idx];
    }
    const byte bgp =
        IOrange[addr(VideoRegister::BGP) - addr(MemoryRegion::IO_REGISTERS)];
    const byte palette_entry = (bgp >> (2 * (color & 0x3))) & 0x3;
    static constexpr std::array<uint32_t, 4> dmg_palette{0xE0F8D0u, 0x88C070u,
                                                         0x346856u, 0x081820u};
    return dmg_palette[palette_entry];
}

inline uint32_t address_bus::get_obj_color(byte palette, byte color) const {
    if (config.cgb_mode) {
        const size_t idx = static_cast<size_t>(palette % 8) * 4 + (color & 0x3);
        return obj_palette_rgb[idx];
    }
    const byte palette_index = (palette & 0x1) ? 1 : 0;
    const half reg_address = palette_index == 0 ? addr(VideoRegister::OBP0)
                                                : addr(VideoRegister::OBP1);
    const byte reg_value =
        IOrange[reg_address - addr(MemoryRegion::IO_REGISTERS)];
    const byte palette_entry = (reg_value >> (2 * (color & 0x3))) & 0x3;
    static constexpr std::array<uint32_t, 4> dmg_obj_palette{
        0xE0F8D0u, 0xF8D878u, 0xC05800u, 0x181010u};
    return dmg_obj_palette[palette_entry == 0 ? 0 : palette_entry];
}

}  // namespace GBC