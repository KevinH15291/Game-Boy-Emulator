#pragma once

#include <enums.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

#include "../CgbConfig.h"
#include "../DebugMacros.h"

namespace GBC {

class APU;

using byte = uint8_t;
using half = uint16_t;

constexpr size_t KB = 1ull << 10;
constexpr size_t MB = 1ull << 20;
constexpr size_t MAX_CART_ROM_SIZE = 8 * MB;

constexpr size_t BOOTROM_SIZE = 0xFF;
constexpr size_t CART_RAM_SIZE = static_cast<size_t>(32) * KB;
constexpr size_t WORK_RAM_SIZE = static_cast<size_t>(32) * KB;
constexpr size_t VIDEO_RAM_SIZE = static_cast<size_t>(16) * KB;
constexpr size_t OAM_SIZE = 0x9F;
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
    ~address_bus();
    void set_cpu(SM83 *cpu_ptr) { cpu = cpu_ptr; }
    address_bus(address_bus &) = delete;
    address_bus(address_bus &&) = delete;
    auto operator=(address_bus &) = delete;
    auto operator=(address_bus &&) = delete;

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
    const byte *get_cartRAM() const { return cartRAM.data(); }

    void toggle_booting() { booting = !booting; }
    void sync_key_registers();
    byte read_vram(byte bank, half offset, bool privileged = false) const;
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
    byte read_joyp_state() const;
    void handle_rtc_register_write(byte reg_index, byte value);
    void handle_rtc_latch_write(byte value);
    void handle_hdma_register(half address, byte value);
};

}  // namespace GBC