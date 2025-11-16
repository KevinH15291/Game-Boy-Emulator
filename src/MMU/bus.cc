#include "bus.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>

#include "../bit_ops.h"
#include "CPU/SM83.h"
#include "audio/APU.h"
#include "enums.h"
#include "video/PPU.h"

namespace GBC {
namespace {
constexpr uint64_t kSecondsPerMinute = 60;
constexpr uint64_t kSecondsPerHour = 60 * kSecondsPerMinute;
constexpr uint64_t kSecondsPerDay = 24 * kSecondsPerHour;

uint64_t host_seconds() {
    using clock = std::chrono::system_clock;
    return std::chrono::duration_cast<std::chrono::seconds>(
               clock::now().time_since_epoch())
        .count();
}

uint64_t decode_rtc_registers(const std::array<byte, 5>& regs) {
    const uint64_t seconds = regs[0] & 0x3F;
    const uint64_t minutes = regs[1] & 0x3F;
    const uint64_t hours = regs[2] & 0x1F;
    const uint64_t day_low = regs[3];
    const uint64_t day_high = regs[4] & 0x01;
    const uint64_t days = (day_high << 8) | day_low;
    return (((days * 24) + hours) * 60 + minutes) * 60 + seconds;
}

void encode_rtc_seconds(uint64_t seconds, bool halted,
                        std::array<byte, 5>& regs) {
    uint64_t days = seconds / kSecondsPerDay;
    seconds %= kSecondsPerDay;
    const uint64_t hours = seconds / kSecondsPerHour;
    seconds %= kSecondsPerHour;
    const uint64_t minutes = seconds / kSecondsPerMinute;
    const uint64_t secs = seconds % kSecondsPerMinute;

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
}  // namespace

address_bus::address_bus(CgbConfig& config)
    : config(config), cartROM(MAX_CART_ROM_SIZE, 0) {
    memset(cartRAM.data(), 0, sizeof(cartRAM));
    memset(workRAM.data(), 0, sizeof(workRAM));
    memset(videoRAM.data(), 0, sizeof(videoRAM));
    memset(workRAM.data(), 0, sizeof(workRAM));
    memset(OAM.data(), 0, sizeof(OAM));
    memset(IOrange.data(), 0, sizeof(IOrange));
    IOrange[addr(IORegister::JOYP) - addr(MemoryRegion::IO_REGISTERS)] = 0xCF;
    sync_key_registers();
}

address_bus::~address_bus() = default;

void address_bus::load_boot_ROM(const char* fname, uint32_t size) {
    booting = 1;
    FILE* fp = fopen(fname, "rb");
    if (!fp) {
        std::cerr << "Failed to open boot ROM: " << fname << std::endl;
        return;
    }

    for (int i = 0; i < size; ++i) {
        if (feof(fp)) break;
        bootrom[i] = fgetc(fp);
        std::cout << bootrom[i];
    }
    std::cout << std::endl;
    fclose(fp);
}

void address_bus::load_ROM(const char* fname) {
    FILE* fp = fopen(fname, "rb");
    if (!fp) {
        std::cerr << "Failed to open ROM: " << fname << std::endl;
        return;
    }

    const size_t max_bytes = cartROM.size();
    const size_t bytes_read = fread(cartROM.data(), 1, max_bytes, fp);
    const bool truncated = !feof(fp);
    fclose(fp);
    if (bytes_read == 0) {
        std::cerr << "No data read from ROM: " << fname << std::endl;
        return;
    }
    if (bytes_read < max_bytes) {
        std::fill(cartROM.begin() + static_cast<std::ptrdiff_t>(bytes_read),
                  cartROM.end(), 0);
    } else if (truncated) {
        std::cerr << "ROM larger than supported buffer (" << max_bytes
                  << " bytes); data truncated.\n";
    }
    if (bytes_read <= 0x148) {
        std::cerr << "ROM header too small: " << fname << std::endl;
        return;
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
    std::ofstream("log.txt", std::ofstream::app)
        << "mbc: " << std::hex << (int)mbc << '\n';
}

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

void address_bus::load_RAM_buffer(const byte* data, size_t length) {
    size_t copy_size = std::min(length, static_cast<size_t>(CART_RAM_SIZE));
    std::memcpy(cartRAM.data(), data, copy_size);
}

void address_bus::set_boot_complete(bool completed) { booting = !completed; }
#endif

void address_bus::reset_MBC_state() {
    rom_bank = 1;
    eram_bank = ExternalRamBank::Bank0;
    bank_mode = BankMode::ROM;
    RAMenable = 0;
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

byte address_bus::read(half address) {
    if (address <= addr(MemoryRegion::END_ROM_BANK_00)) {
        if (address < 0x0100 && booting) {
            return bootrom[address];
        }
        return cartROM[address];
    }

    if (address >= addr(MemoryRegion::ROM_BANK_NN) &&
        address <= addr(MemoryRegion::END_ROM_BANK_NN)) {
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

    if (address >= addr(MemoryRegion::WORK_RAM_BANK0) &&
        address <= addr(MemoryRegion::END_WORK_RAM_BANK0)) {
        return workRAM[address - addr(MemoryRegion::WORK_RAM_BANK0)];
    }

    if (address >= addr(MemoryRegion::WORK_RAM_BANKN) &&
        address <= addr(MemoryRegion::END_WORK_RAM_BANKN)) {
        return workRAM[address + static_cast<int>(wram_bank) * 4 * KB -
                       addr(MemoryRegion::WORK_RAM_BANKN)];
    }

    if (address >= addr(MemoryRegion::ECHO_RAM1) &&
        address <= addr(MemoryRegion::EECHO_RAM2)) {
        address -= 0x2000;
        if (address >= addr(MemoryRegion::WORK_RAM_BANK0) &&
            address <= addr(MemoryRegion::END_WORK_RAM_BANK0)) {
            return workRAM[address - addr(MemoryRegion::WORK_RAM_BANK0)];
        }
        return workRAM[address + static_cast<int>(wram_bank) * 4 * KB -
                       addr(MemoryRegion::WORK_RAM_BANKN)];
    }

    if (address >= addr(MemoryRegion::VIDEO_RAM) &&
        address <= addr(MemoryRegion::END_VIDEO_RAM)) {
        const half offset = address - addr(MemoryRegion::VIDEO_RAM);
        return read_vram(static_cast<byte>(vram_bank), offset, false);
    }

    if (address >= addr(MemoryRegion::OAMaddress) &&
        address <= addr(MemoryRegion::END_OAM)) {
        if (lcd_mode == RenderingState::draw ||
            lcd_mode == RenderingState::OAMscan)
            return 0xFF;
        return OAM[address - addr(MemoryRegion::OAMaddress)];
    }

    if (address >= addr(MemoryRegion::IO_REGISTERS) &&
        address <= addr(MemoryRegion::END_IO_REGISTERS)) {
        return readIO(address);
    }

    if (address >= addr(MemoryRegion::HIGH_RAM) &&
        address <= addr(MemoryRegion::END_HIGH_RAM)) {
        return HRAM[address - addr(MemoryRegion::HIGH_RAM)];
    }

    if (address >= addr(MemoryRegion::EXTERNAL_RAM) &&
        address <= addr(MemoryRegion::END_EXTERNAL_RAM)) {
        if (!RAMenable) {
            return 0xFF;
        }
        if (is_mbc3_controller(mbc) && rtc_selected_register != 0) {
            byte reg_index = rtc_selected_register - 0x08;
            if (reg_index < 5) {
                return rtc_latched[reg_index];
            }
            return 0xFF;
        }
        if (mbc == MemoryBankController::MBC2 ||
            mbc == MemoryBankController::MBC2_BATTERY) {
            return (cartRAM[address - addr(MemoryRegion::EXTERNAL_RAM)] &
                    0x0F) |
                   0xF0;
        }
        return cartRAM[address + 8 * KB * static_cast<int>(eram_bank) -
                       addr(MemoryRegion::EXTERNAL_RAM)];
    }

    if (address >= addr(MemoryRegion::NOT_USUABLE) &&
        address <= addr(MemoryRegion::END_NOT_USUABLE)) {
        return 0x00;
    }

    if (address == addr(MemoryRegion::IE)) return IEnable;

    return 0xFF;
}

inline byte address_bus::readIO(half address) {
    switch (address) {
        case addr(IORegister::JOYP):
            if (getBitRange(read_privileged(address), 4, 2) == 0) {
                return (read_privileged(addr(IORegister::JOYP)) & 0x30) |
                       (input_s & input_d & 0x0F);
            } else if (!isBitSet(read_privileged(address), 4)) {
                return (read_privileged(addr(IORegister::JOYP)) & 0x30) |
                       (input_d & 0x0F);
            } else if (!isBitSet(read_privileged(address), 5)) {
                return (read_privileged(addr(IORegister::JOYP)) & 0x30) |
                       (input_s & 0x0F);
            } else {
                return 0x3F;
            }
        case addr(AudioRegister::NR10):
        case addr(AudioRegister::NR11):
        case addr(AudioRegister::NR12):
        case addr(AudioRegister::NR13):
        case addr(AudioRegister::NR14):
        case addr(AudioRegister::NR21):
        case addr(AudioRegister::NR22):
        case addr(AudioRegister::NR23):
        case addr(AudioRegister::NR24):
        case addr(AudioRegister::NR30):
        case addr(AudioRegister::NR31):
        case addr(AudioRegister::NR32):
        case addr(AudioRegister::NR33):
        case addr(AudioRegister::NR34):
        case addr(AudioRegister::NR41):
        case addr(AudioRegister::NR42):
        case addr(AudioRegister::NR43):
        case addr(AudioRegister::NR44):
        case addr(AudioRegister::NR50):
        case addr(AudioRegister::NR51):
        case addr(AudioRegister::NR52):
            return apu->read_register(address);
        case 0xFF30:
        case 0xFF31:
        case 0xFF32:
        case 0xFF33:
        case 0xFF34:
        case 0xFF35:
        case 0xFF36:
        case 0xFF37:
        case 0xFF38:
        case 0xFF39:
        case 0xFF3a:
        case 0xFF3b:
        case 0xFF3c:
        case 0xFF3d:
        case 0xFF3e:
        case 0xFF3f:
            return apu->read_wave_byte(address);
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
            return ppu->read_register(address);
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

byte address_bus::read_privileged(half address) {
    if (address < 0x0100 && booting) {
        return bootrom[address];
    }

    if (address <= addr(MemoryRegion::END_ROM_BANK_00)) return cartROM[address];

    if (address >= addr(MemoryRegion::ROM_BANK_NN) &&
        address <= addr(MemoryRegion::END_ROM_BANK_NN)) {
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

    if (address >= addr(MemoryRegion::VIDEO_RAM) &&
        address <= addr(MemoryRegion::END_VIDEO_RAM)) {
        const half offset = address - addr(MemoryRegion::VIDEO_RAM);
        return read_vram(static_cast<byte>(vram_bank), offset, true);
    }

    if (address >= addr(MemoryRegion::WORK_RAM_BANK0) &&
        address <= addr(MemoryRegion::END_WORK_RAM_BANK0)) {
        return workRAM[address - addr(MemoryRegion::WORK_RAM_BANK0)];
    }

    if (address >= addr(MemoryRegion::WORK_RAM_BANKN) &&
        address <= addr(MemoryRegion::END_WORK_RAM_BANKN)) {
        return workRAM[address + static_cast<int>(wram_bank) * 4 * KB -
                       addr(MemoryRegion::WORK_RAM_BANKN)];
    }

    if (address >= addr(MemoryRegion::ECHO_RAM1) &&
        address <= addr(MemoryRegion::EECHO_RAM1)) {
        return workRAM[address - addr(MemoryRegion::ECHO_RAM1)];
    }

    if (address >= addr(MemoryRegion::ECHO_RAM2) &&
        address <= addr(MemoryRegion::EECHO_RAM2)) {
        return workRAM[address + static_cast<int>(wram_bank) * 4 * KB -
                       addr(MemoryRegion::ECHO_RAM2)];
    }

    if (address >= addr(MemoryRegion::OAMaddress) &&
        address <= addr(MemoryRegion::END_OAM)) {
        return OAM[address - addr(MemoryRegion::OAMaddress)];
    }

    if (address >= addr(MemoryRegion::IO_REGISTERS) &&
        address <= addr(MemoryRegion::END_IO_REGISTERS)) {
        return IOrange[address - addr(MemoryRegion::IO_REGISTERS)];
    }

    if (address >= addr(MemoryRegion::HIGH_RAM) &&
        address <= addr(MemoryRegion::END_HIGH_RAM)) {
        return HRAM[address - addr(MemoryRegion::HIGH_RAM)];
    }

    if (address == addr(MemoryRegion::IE)) return IEnable;

    return 0xFF;
}

void address_bus::write(half address, byte value) {
    if (address >= addr(MemoryRegion::WORK_RAM_BANK0) &&
        address <= addr(MemoryRegion::END_WORK_RAM_BANK0)) {
        workRAM[address - addr(MemoryRegion::WORK_RAM_BANK0)] = value;
        return;
    }

    if (address >= addr(MemoryRegion::WORK_RAM_BANKN) &&
        address <= addr(MemoryRegion::END_WORK_RAM_BANKN)) {
        workRAM[address + static_cast<int>(wram_bank) * 4 * KB -
                addr(MemoryRegion::WORK_RAM_BANKN)] = value;
        return;
    }

    if (address >= addr(MemoryRegion::ECHO_RAM1) &&
        address <= addr(MemoryRegion::EECHO_RAM2)) {
        address -= 0x2000;
        if (address >= addr(MemoryRegion::WORK_RAM_BANK0) &&
            address <= addr(MemoryRegion::END_WORK_RAM_BANK0)) {
            workRAM[address - addr(MemoryRegion::WORK_RAM_BANK0)] = value;
            return;
        }
        workRAM[address + static_cast<int>(wram_bank) * 4 * KB -
                addr(MemoryRegion::WORK_RAM_BANKN)] = value;
        return;
    }

    if (address >= addr(MemoryRegion::VIDEO_RAM) &&
        address <= addr(MemoryRegion::END_VIDEO_RAM)) {
        const half offset = address - addr(MemoryRegion::VIDEO_RAM);
        write_vram(static_cast<byte>(vram_bank), offset, value, false);
        return;
    }

    if (address >= addr(MemoryRegion::OAMaddress) &&
        address <= addr(MemoryRegion::END_OAM)) {
        if (lcd_mode == RenderingState::draw ||
            lcd_mode == RenderingState::OAMscan)
            return;
        OAM[address - addr(MemoryRegion::OAMaddress)] = value;
        return;
    }

    if (address >= addr(MemoryRegion::IO_REGISTERS) &&
        address <= addr(MemoryRegion::END_IO_REGISTERS)) {
        writeIO(address, value);
        return;
    }

    if (address >= addr(MemoryRegion::HIGH_RAM) &&
        address <= addr(MemoryRegion::END_HIGH_RAM)) {
        HRAM[address - addr(MemoryRegion::HIGH_RAM)] = value;
        return;
    }

    if (address >= addr(MemoryRegion::EXTERNAL_RAM) &&
        address <= addr(MemoryRegion::END_EXTERNAL_RAM)) {
        if (!RAMenable) {
            return;
        }
        if (is_mbc3_controller(mbc) && rtc_selected_register != 0) {
            byte reg_index = rtc_selected_register - 0x08;
            if (reg_index < 5) {
                switch (reg_index) {
                    case 0:
                        rtc_registers[0] = value & 0x3F;
                        break;
                    case 1:
                        rtc_registers[1] = value & 0x3F;
                        break;
                    case 2:
                        rtc_registers[2] = value & 0x1F;
                        break;
                    case 3:
                        rtc_registers[3] = value;
                        break;
                    case 4: {
                        const byte previous_control = rtc_registers[4];
                        byte control = static_cast<byte>(previous_control &
                                                         0x80);  // carry
                        control |=
                            static_cast<byte>(value & 0x01);  // Day bit 8
                        bool new_halt = (value & 0x40) != 0;
                        if (new_halt) {
                            control |= 0x40;
                        } else {
                            control &= static_cast<byte>(~0x40);
                        }
                        if ((value & 0x80) == 0) {
                            control &= static_cast<byte>(~0x80);
                        }

                        if (!rtc_halted && new_halt) {
                            const uint64_t seconds =
                                host_seconds() > rtc_epoch
                                    ? host_seconds() - rtc_epoch
                                    : 0;
                            encode_rtc_seconds(seconds, true, rtc_registers);
                        } else if (rtc_halted && !new_halt) {
                            const uint64_t seconds =
                                decode_rtc_registers(rtc_registers);
                            const uint64_t now = host_seconds();
                            rtc_epoch = now > seconds ? (now - seconds) : now;
                        }

                        rtc_halted = new_halt;
                        rtc_registers[4] = control;
                        break;
                    }
                }

                if (!rtc_halted) {
                    const uint64_t seconds =
                        decode_rtc_registers(rtc_registers);
                    const uint64_t now = host_seconds();
                    rtc_epoch = now > seconds ? (now - seconds) : now;
                }
            }
            return;
        }
        if (mbc == MemoryBankController::MBC2 ||
            mbc == MemoryBankController::MBC2_BATTERY) {
            cartRAM[address - addr(MemoryRegion::EXTERNAL_RAM)] =
                (value & 0x0F) | 0xF0;
            return;
        }
        cartRAM[address + 8 * KB * static_cast<int>(eram_bank) -
                addr(MemoryRegion::EXTERNAL_RAM)] = value;
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
            if (debug) {
                std::ofstream("log.txt", std::ofstream::app)
                    << "unsupported MBC: " << std::hex << to_underlying(mbc)
                    << std::endl;
            }
            return;
    }

    // throw std::runtime_error(std::string("mbc not valid
    // ").append(std::to_string(address)));
}

inline void address_bus::writeIO(half address, byte val) {
    switch (address) {
        case addr(IORegister::JOYP):
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] =
                (val & 0xF0) |
                (IOrange[address - addr(MemoryRegion::IO_REGISTERS)] & 0x0F);
            return;
        case addr(IORegister::DIV):  // 0xFF04
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = 0;
            return;
        case addr(IORegister::TIMA):  // 0xFF05
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            if (cpu) {
                cpu->mark_tima_written();
            }
            return;
        case addr(IORegister::TMA):  // 0xFF06
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        case addr(IORegister::TAC):  // 0xFF07
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            if (cpu) {
                cpu->reset_timer_counter();
            }
            return;
        case addr(AudioRegister::NR10):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR11):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR12):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR13):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR14):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR21):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR22):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR23):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR24):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR30):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR31):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR32):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR33):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR34):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR41):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR42):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR43):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR44):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR50):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR51):
            apu->write_register(address, val);
            return;
        case addr(AudioRegister::NR52):
            apu->write_register(address, val);
            return;
        case 0xFF30:
        case 0xFF31:
        case 0xFF32:
        case 0xFF33:
        case 0xFF34:
        case 0xFF35:
        case 0xFF36:
        case 0xFF37:
        case 0xFF38:
        case 0xFF39:
        case 0xFF3a:
        case 0xFF3b:
        case 0xFF3c:
        case 0xFF3d:
        case 0xFF3e:
        case 0xFF3f:
            apu->write_wave_byte(address, val);
            return;
        case addr(VideoRegister::LCDC):  // 0xFF40
            ppu->write_register(address, val);
            return;
        case addr(VideoRegister::STAT):  // 0xFF41
            ppu->write_register(address, val);
            return;
        case addr(VideoRegister::SCY):  // 0xFF42
            ppu->write_register(address, val);
            return;
        case addr(VideoRegister::SCX):  // 0xFF43
            ppu->write_register(address, val);
            return;
        case addr(VideoRegister::LY):  // 0xFF44
            std::ofstream("log.txt", std::ofstream::app)
                << "wrote to LY" << std::hex << val;
            return;
        case addr(VideoRegister::LYC):  // 0xFF45
            ppu->write_register(address, val);
            return;
        case addr(VideoRegister::OAMDMA):  // 0xFF46
            for (int i = 0; i < 0x9F; ++i) {
                OAM[i] = this->read_privileged(i + ((half)val << 8));
            }
            return;
        case addr(VideoRegister::BGP):  // 0xFF47
            ppu->write_register(address, val);
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        case addr(VideoRegister::OBP0):  // 0xFF48
            ppu->write_register(address, val);
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        case addr(VideoRegister::OBP1):  // 0xFF49
            ppu->write_register(address, val);
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        case addr(VideoRegister::WY):  // 0xFF4A
            ppu->write_register(address, val);
            return;
        case addr(VideoRegister::WX):  // 0xFF4B
            ppu->write_register(address, val);
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
            if (ppu != nullptr) {
                ppu->mark_cache_dirty();
            }
            return;
        case addr(CGBRegister::HDMA1):
            hdma_src = static_cast<half>((static_cast<half>(val) << 8) |
                                         (hdma_src & 0x00FF));
            return;
        case addr(CGBRegister::HDMA2):
            hdma_src = static_cast<half>((hdma_src & 0xFF00) | (val & 0xF0));
            return;
        case addr(CGBRegister::HDMA3):
            hdma_dst =
                static_cast<half>(((val & 0x1F) << 8) | (hdma_dst & 0x00FF));
            return;
        case addr(CGBRegister::HDMA4):
            hdma_dst = static_cast<half>((hdma_dst & 0xFF00) | (val & 0xF0));
            return;
        case addr(CGBRegister::HDMA5): {
            if (!config.cgb_mode) {
                IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = 0xFF;
                return;
            }
            if ((val & 0x80) == 0) {
                if (hdma_active && hdma_mode_hblank) {
                    cancel_hdma();
                }
                start_general_hdma(val & 0x7F);
            } else {
                if (hdma_active && !hdma_mode_hblank) {
                    return;
                }
                start_hblank_hdma(val & 0x7F);
            }
            return;
        }
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
            if (ppu != nullptr) {
                ppu->mark_cache_dirty();
            }
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
            debug = 1;
            std::ofstream("log.txt", std::ofstream::app)
                << "unsupported IO write: " << std::hex << address << std::endl;
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
        }
        return;
    }

    if (address >= 0x6000 && address <= 0x7FFF) {
        if (latch_write == 0x00 && value == 0x01) {
            uint64_t seconds = rtc_halted ? decode_rtc_registers(rtc_registers)
                                          : (host_seconds() > rtc_epoch
                                                 ? host_seconds() - rtc_epoch
                                                 : 0);
            encode_rtc_seconds(seconds, rtc_halted, rtc_latched);
        }
        latch_write = value;
        return;
    }
}

byte address_bus::read_vram(byte bank, half offset, bool privileged) const {
    if (!privileged && lcd_mode == RenderingState::draw) {
        return 0xFF;
    }
    bank &= 0x1;
    offset &= 0x1FFF;
    const size_t index =
        static_cast<size_t>(bank) * 8 * KB + static_cast<size_t>(offset);
    return videoRAM[index];
}

void address_bus::write_vram(byte bank, half offset, byte value,
                             bool privileged) {
    if (!privileged && lcd_mode == RenderingState::draw) {
        return;
    }
    bank &= 0x1;
    offset &= 0x1FFF;
    const size_t index =
        static_cast<size_t>(bank) * 8 * KB + static_cast<size_t>(offset);
    videoRAM[index] = value;
    if (ppu != nullptr) {
        ppu->mark_cache_dirty();
    }
}

uint32_t address_bus::get_bg_color(byte palette, byte color) const {
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

uint32_t address_bus::get_obj_color(byte palette, byte color) const {
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

void address_bus::sync_key_registers() {
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

constexpr uint32_t address_bus::rgb15_to_rgb888(half raw) {
    const byte r = static_cast<byte>(getBitRange(raw, 0, 5) << 3);
    const byte g = static_cast<byte>(getBitRange(raw, 5, 5) << 3);
    const byte b = static_cast<byte>(getBitRange(raw, 10, 5) << 3);
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) |
           b;
}

void address_bus::refresh_bg_palette_entry(byte byte_index) {
    const byte aligned_index = byte_index & 0x3E;
    const byte low = bg_palette_raw[aligned_index];
    const byte high = bg_palette_raw[aligned_index + 1];
    const half color =
        static_cast<half>(low) | (static_cast<half>(high & 0x7F) << 8);
    const size_t color_index = aligned_index / 2;
    bg_palette_rgb[color_index] = rgb15_to_rgb888(color);
}

void address_bus::refresh_obj_palette_entry(byte byte_index) {
    const byte aligned_index = byte_index & 0x3E;
    const byte low = obj_palette_raw[aligned_index];
    const byte high = obj_palette_raw[aligned_index + 1];
    const half color =
        static_cast<half>(low) | (static_cast<half>(high & 0x7F) << 8);
    const size_t color_index = aligned_index / 2;
    obj_palette_rgb[color_index] = rgb15_to_rgb888(color);
}

void address_bus::increment_bgpi() { bgpi = (bgpi + 1) & 0x3F; }

void address_bus::increment_obpi() { obpi = (obpi + 1) & 0x3F; }

void address_bus::mark_palette_dirty() const {
    if (ppu != nullptr) {
        ppu->mark_cache_dirty();
    }
}

void address_bus::start_general_hdma(byte block_count) {
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

void address_bus::start_hblank_hdma(byte block_count) {
    if (!config.cgb_mode) {
        update_hdma_status_register();
        return;
    }
    hdma_active = true;
    hdma_mode_hblank = true;
    hdma_blocks_remaining = static_cast<byte>(block_count + 1);
    update_hdma_status_register();
}

void address_bus::complete_hdma_block() {
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

void address_bus::cancel_hdma() {
    hdma_active = false;
    hdma_mode_hblank = false;
    hdma_blocks_remaining = 0;
    update_hdma_status_register();
}

void address_bus::handle_hblank_hdma() {
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

void address_bus::update_hdma_status_register() {
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
        RAMenable = (value & 0x0F) == 0x0A;
        return;
    }

    if (address >= 0x2000 && address <= 0x2FFF) {
        rom_bank = value;
        if (rom_bank == 0) rom_bank = 1;
        return;
    }

    if (address >= 0x4000 && address <= 0x5FFF) {
        eram_bank = static_cast<ExternalRamBank>(value & 0x0F);
        return;
    }
}

void address_bus::writeHuC1(half address, byte value) {
    if (address >= addr(MemoryRegion::ROM_BANK_00) && address <= 0x1FFF) {
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

    if (address >= 0x6000 && address <= 0x7FFF) {
        bank_mode = (value & 0x01) ? BankMode::RAM : BankMode::ROM;
        return;
    }
}

void address_bus::writeHuC3(half address, byte value) {
    if (address >= addr(MemoryRegion::ROM_BANK_00) && address <= 0x1FFF) {
        RAMenable = (value & 0x0F) == 0x0A;
        return;
    }

    if (address >= 0x2000 && address <= 0x3FFF) {
        byte bank = value & 0x7F;
        if (bank == 0) bank = 1;
        rom_bank = bank;
        return;
    }

    if (address >= 0x4000 && address <= 0x5FFF) {
        eram_bank = static_cast<ExternalRamBank>(value & 0x03);
        return;
    }
}

}  // namespace GBC