#pragma once

#include <enums.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace GBC {
using byte = uint8_t;
using half = uint16_t;

// Size constants
constexpr uint16_t KB = 1 << 10;
constexpr uint32_t MB = 1 << 20;

// Size values for arrays
constexpr size_t BOOTROM_SIZE = 0xFF;
constexpr size_t CART_RAM_SIZE = static_cast<size_t>(32) * KB;
constexpr size_t WORK_RAM_SIZE = static_cast<size_t>(32) * KB;
constexpr size_t VIDEO_RAM_SIZE = static_cast<size_t>(16) * KB;
constexpr size_t OAM_SIZE = 0x9F;
constexpr size_t IO_RANGE_SIZE = 256;
constexpr size_t HRAM_SIZE = 128;

constexpr uint8_t BYTE_MAX = 0xFF;

// Helper functions to convert enum classes to underlying type
constexpr uint16_t addr(MemoryRegion region) {
    return static_cast<uint16_t>(region);
}
constexpr uint16_t addr(IORegister reg) { return static_cast<uint16_t>(reg); }
constexpr uint16_t addr(AudioRegister reg) {
    return static_cast<uint16_t>(reg);
}
constexpr uint16_t addr(VideoRegister reg) {
    return static_cast<uint16_t>(reg);
}
constexpr uint16_t addr(CGBRegister reg) { return static_cast<uint16_t>(reg); }

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
    MBC3 = 0x13,
    MBC5 = 0x19,
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

// should create more robust constructor
class address_bus {
   public:
    address_bus();
    ~address_bus();
    address_bus(address_bus &) = delete;
    address_bus(address_bus &&) = delete;
    auto operator=(address_bus &) = delete;
    auto operator=(address_bus &&) = delete;

    uint8_t read(half address);
    uint8_t read_privileged(half address);
    uint8_t readIO(half address);

    byte check_set(half address, byte value);
    inline void set_bit(half address, byte bit);
    inline void reset_bit(half address, byte bit);

    void load_boot_ROM(const char *fname, uint32_t size);
    void load_ROM(const char *fname, uint32_t size);
#ifdef __EMSCRIPTEN__
    void load_ROM_buffer(const uint8_t *data, size_t length);
    void set_boot_complete(bool completed);
    void set_booting(bool value) { booting = value; }
#endif

    void load_RAM(const char *fname, uint32_t size);

    void write(half address, byte value);
    void write_privileged(half address, byte value);
    void writeMBC1(half address, byte value);
    void writeMBC2(half address, byte value);
    void writeMBC3(half address, byte value);
    void writeMBC5(half address, byte value);
    void writeMBC6(half address, byte value);
    void writeMBC7(half address, byte value);
    void writeHuC1(half address, byte value);
    void writeHuC3(half address, byte value);

    void writeIO(half address, byte val);

    // Public access to cartRAM for save/load functionality
    byte *get_cartRAM() { return cartRAM.data(); }
    const byte *get_cartRAM() const { return cartRAM.data(); }

    void toggle_booting() { booting = !booting; }

   private:
    std::array<byte, BOOTROM_SIZE> bootrom{};
    std::array<byte, MB> cartROM{};
    std::array<byte, CART_RAM_SIZE> cartRAM{};
    std::array<byte, WORK_RAM_SIZE> workRAM{};
    std::array<byte, VIDEO_RAM_SIZE> videoRAM{};
    std::array<byte, OAM_SIZE> OAM{};
    std::array<byte, IO_RANGE_SIZE> IOrange{};
    std::array<byte, HRAM_SIZE> HRAM{};
    std::array<byte, 4> rt{};

    RenderingState lcd_mode = RenderingState::hblank;

    MemoryBankController mbc = MemoryBankController::None;
    VramBank vram_bank = VramBank::Bank0;
    WramBank wram_bank = WramBank::Bank1;
    uint8_t rom_bank = 1;
    ExternalRamBank eram_bank = ExternalRamBank::Bank0;
    BankMode bank_mode = BankMode::ROM;

    byte input_d = 0x0F;
    byte input_s = 0x0F;
    byte IEnable = 0;
    byte RAMenable = 0;
    byte latch_write = 0;
    byte debug_value = 0;

    bool booting = false;
    bool latched = false;
    bool debug = false;

    friend class SM83;
    friend class PPU;
    friend class GBC;
    friend class APU;
};

}  // namespace GBC