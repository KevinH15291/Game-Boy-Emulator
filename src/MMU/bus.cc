#include "bus.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>

#include "audio/APU.h"
#include "enums.h"
#include "video/PPU.h"

namespace GBC {
address_bus::address_bus() {
    memset(cartROM.data(), 0, sizeof(cartROM));
    memset(cartRAM.data(), 0, sizeof(cartRAM));
    memset(workRAM.data(), 0, sizeof(workRAM));
    memset(videoRAM.data(), 0, sizeof(videoRAM));
    memset(workRAM.data(), 0, sizeof(workRAM));
    memset(OAM.data(), 0, sizeof(OAM));
    memset(IOrange.data(), 0, sizeof(IOrange));
    IOrange[addr(IORegister::JOYP) - addr(MemoryRegion::IO_REGISTERS)] = 0x3F;
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

void address_bus::load_ROM(const char* fname, uint32_t size) {
    FILE* fp = fopen(fname, "rb");
    if (!fp) {
        std::cerr << "Failed to open ROM: " << fname << std::endl;
        return;
    }

    for (int i = 0; i < size; ++i) {
        if (feof(fp)) break;
        cartROM[i] = fgetc(fp);
    }
    fclose(fp);
    mbc = static_cast<MemoryBankController>(cartROM[0x147]);
    std::ofstream("log.txt", std::ofstream::app)
        << "mbc: " << std::hex << (int)mbc << '\n';
}

#ifdef __EMSCRIPTEN__
void address_bus::load_ROM_buffer(const uint8_t* data, size_t length) {
    size_t copy_size = std::min(length, static_cast<size_t>(KB * KB));
    std::memcpy(cartROM.data(), data, copy_size);
    if (copy_size > 0x147) {
        mbc = static_cast<MemoryBankController>(cartROM[0x147]);
    }
}

void address_bus::set_boot_complete(bool completed) { booting = !completed; }
#endif

// TODO: Benchmark how slow this is. Might stop using it in static contexts,
// I'll see how well the compiler optimizes it. Also probably want to implement
// mask read/writes
byte address_bus::read(half address) {
    if (address <= addr(MemoryRegion::END_ROM_BANK_00)) {
        if (address < 0x0100 && booting) {
            return bootrom[address];
        }
        return cartROM[address];
    }

    if (address >= addr(MemoryRegion::ROM_BANK_NN) &&
        address <= addr(MemoryRegion::END_ROM_BANK_NN)) {
        uint16_t bank = rom_bank;
        if (static_cast<int>(mbc) == 0x19 || static_cast<int>(mbc) == 0x1A ||
            static_cast<int>(mbc) == 0x1B || static_cast<int>(mbc) == 0x1C) {
            bank &= 0x1FF;
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
        if (lcd_mode == RenderingState::draw) return 0xFF;
        return videoRAM[address + static_cast<int>(vram_bank) * 8 * KB -
                        addr(MemoryRegion::VIDEO_RAM)];
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
        if (static_cast<int>(mbc) == 0x13 && rtc_selected_register != 0) {
            uint8_t reg_index = rtc_selected_register - 0x08;
            if (reg_index < 5) {
                return rtc_latched[reg_index];
            }
            return 0xFF;
        }
        if (static_cast<int>(mbc) == 0x05 || static_cast<int>(mbc) == 0x06) {
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

uint8_t address_bus::readIO(half address) {
    switch (address) {
        case addr(IORegister::JOYP):
            if ((read_privileged(address) & (3 << 4)) == 0) {
                return (read_privileged(addr(IORegister::JOYP)) & 0x30) |
                       (input_s & input_d & 0x0F);
            } else if ((read_privileged(address) & (1 << 4)) == 0) {
                return (read_privileged(addr(IORegister::JOYP)) & 0x30) |
                       (input_d & 0x0F);
            } else if ((read_privileged(address) & (2 << 4)) == 0) {
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
        uint16_t bank = rom_bank;
        if (static_cast<int>(mbc) == 0x19 || static_cast<int>(mbc) == 0x1A ||
            static_cast<int>(mbc) == 0x1B || static_cast<int>(mbc) == 0x1C) {
            bank &= 0x1FF;
        } else {
            bank &= 0x7F;
        }
        return cartROM[address + 16 * KB * bank -
                       addr(MemoryRegion::ROM_BANK_NN)];
    }

    if (address >= addr(MemoryRegion::VIDEO_RAM) &&
        address <= addr(MemoryRegion::END_VIDEO_RAM)) {
        return videoRAM[address + static_cast<int>(vram_bank) * 8 * KB -
                        addr(MemoryRegion::VIDEO_RAM)];
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
        if (lcd_mode == RenderingState::draw) return;
        videoRAM[address + static_cast<int>(vram_bank) * 8 * KB -
                 addr(MemoryRegion::VIDEO_RAM)] = value;
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
        if (static_cast<int>(mbc) == 0x13 && rtc_selected_register != 0) {
            uint8_t reg_index = rtc_selected_register - 0x08;
            if (reg_index < 5) {
                rtc_registers[reg_index] = value;
                if (reg_index == 4) {
                    rtc_halted = (value & 0x40) != 0;
                    if ((value & 0x80) != 0) {
                        rtc_registers[4] &= 0x7F;
                        rtc_registers[3] = 0;
                    }
                }
            }
            return;
        }
        if (static_cast<int>(mbc) == 0x05 || static_cast<int>(mbc) == 0x06) {
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

    switch (static_cast<int>(mbc)) {
        case 0x00:
            return;
        case 0x01:
        case 0x02:
        case 0x03:
            writeMBC1(address, value);
            return;
        case 0x05:
        case 0x06:
            writeMBC2(address, value);
            return;
        case 0x0B:
        case 0x0C:
        case 0x0D:
            writeMBC1(address, value);
            return;
        case 0x13:
            writeMBC3(address, value);
            return;
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
            writeMBC5(address, value);
            return;
        case 0x20:
            writeMBC6(address, value);
            return;
        case 0x22:
            writeMBC7(address, value);
            return;
        case 0xFE:
            writeHuC1(address, value);
            return;
        case 0xFF:
            writeHuC3(address, value);
            return;
        default:
            if (debug) {
                std::ofstream("log.txt", std::ofstream::app)
                    << "unsupported MBC: " << std::hex << (int)mbc << std::endl;
            }
            return;
    }

    // throw std::runtime_error(std::string("mbc not valid
    // ").append(std::to_string(address)));
}

void address_bus::writeIO(half address, byte val) {
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
            return;
        case addr(IORegister::TMA):  // 0xFF06
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        case addr(IORegister::TAC):  // 0xFF07
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
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
                OAM[i] = this->read_privileged(i + ((uint16_t)val << 8));
            }
            return;
        case addr(VideoRegister::BGP):  // 0xFF47
            ppu->write_register(address, val);
            return;
        case addr(VideoRegister::OBP0):  // 0xFF48
            ppu->write_register(address, val);
            return;
        case addr(VideoRegister::OBP1):  // 0xFF49
            ppu->write_register(address, val);
            return;
        case addr(VideoRegister::WY):  // 0xFF4A
            ppu->write_register(address, val);
            return;
        case addr(VideoRegister::WX):  // 0xFF4B
            ppu->write_register(address, val);
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
        uint8_t bank = value & 0x1F;
        if (bank == 0) bank = 1;
        rom_bank = bank;
        if (bank_mode == BankMode::ROM) {
            rom_bank |= (static_cast<uint8_t>(eram_bank) & 0x03) << 5;
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
            eram_bank = ExternalRamBank::Bank0;
            rom_bank = (rom_bank & 0x1F) |
                       ((static_cast<uint8_t>(eram_bank) & 0x03) << 5);
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
        uint16_t bank = value & 0x7F;
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
            if (!rtc_halted) {
                auto now = std::chrono::system_clock::now();
                auto duration = now.time_since_epoch();
                auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
                if (rtc_epoch == 0) {
                    rtc_epoch = seconds;
                }
                seconds -= rtc_epoch;
                
                uint64_t days = seconds / 86400;
                seconds %= 86400;
                uint8_t hours = (seconds / 3600) % 24;
                seconds %= 3600;
                uint8_t minutes = (seconds / 60) % 60;
                seconds %= 60;
                
                rtc_latched[0] = seconds & 0x3F;
                rtc_latched[1] = minutes & 0x3F;
                rtc_latched[2] = hours & 0x1F;
                rtc_latched[3] = days & 0xFF;
                rtc_latched[4] = ((days >> 8) & 0x01) | (rtc_halted ? 0x40 : 0) | ((days >= 512) ? 0x80 : 0);
            } else {
                rtc_latched = rtc_registers;
            }
        }
        latch_write = value;
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
            uint8_t bank = value & 0x0F;
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
        uint8_t bank = value & 0x3F;
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
        uint8_t bank = value & 0x7F;
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