#include "bus.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

namespace GBC {
address_bus::address_bus() {
    IOrange[addr(IORegister::JOYP) - addr(MemoryRegion::IO_REGISTERS)] = 0x3F;
}

address_bus::~address_bus() {
    std::ofstream file("cartRAMdump(save).bin", std::ios::binary);
    file.write((char *)cartRAM.data(), sizeof(cartRAM));
}

void address_bus::load_boot_ROM(const char *fname, uint32_t size) {
    booting = true;
    std::ifstream file(fname, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not open boot ROM file: " << fname << '\n';
        return;
    }

    const std::size_t bytes_to_read =
        std::min<std::size_t>(static_cast<std::size_t>(size), bootrom.size());

    file.read(reinterpret_cast<char *>(bootrom.data()), bytes_to_read);
    const std::size_t bytes_read = static_cast<std::size_t>(file.gcount());

    if (bytes_read < bootrom.size()) {
        std::fill(bootrom.begin() + bytes_read, bootrom.end(), 0);
    }
}

void address_bus::load_ROM(const char *fname, uint32_t size) {
    std::ifstream file(fname, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Could not open ROM file: " << fname << '\n';
        return;
    }

    const std::size_t bytes_to_read =
        std::min<std::size_t>(static_cast<std::size_t>(size), cartROM.size());

    file.read(reinterpret_cast<char *>(cartROM.data()), bytes_to_read);

    mbc = static_cast<MemoryBankController>(cartROM[0x147]);
    std::ofstream("log.txt", std::ofstream::app)
        << "mbc: " << std::hex << (int)mbc << '\n';
}

#ifdef __EMSCRIPTEN__
void address_bus::load_ROM_buffer(const uint8_t *data, size_t length) {
    size_t copy_len = std::min(cartROM.size(), length);
    std::copy(data, data + copy_len, cartROM.begin());
    rom_bank = 1;
    mbc = static_cast<MemoryBankController>(cartROM[0x147]);
    booting = false;
}

void address_bus::set_boot_complete(bool completed) { booting = !completed; }
#endif

// TODO: Benchmark how slow this is. Might stop using it in static contexts,
// I'll see how well the compiler optimizes it. Also probably want to implement
// mask read/writes
byte address_bus::read(half address) {
    if (address < 0x0100 && booting) {
        return bootrom[address];
    }

    if (address <= addr(MemoryRegion::END_ROM_BANK_00)) {
        return cartROM[address];
    }

    if (address >= addr(MemoryRegion::ROM_BANK_NN) &&
        address <= addr(MemoryRegion::END_ROM_BANK_NN)) {
        return cartROM[address + 16 * KB * (rom_bank & 0x7F) -
                       addr(MemoryRegion::ROM_BANK_NN)];
    }

    if (address >= addr(MemoryRegion::VIDEO_RAM) &&
        address <= addr(MemoryRegion::END_VIDEO_RAM)) {
        if (lcd_mode == RenderingState::draw) return 0xFF;
        return videoRAM[address + static_cast<uint8_t>(vram_bank) * 8 * KB -
                        addr(MemoryRegion::VIDEO_RAM)];
    }

    if (address >= addr(MemoryRegion::EXTERNAL_RAM) &&
        address <= addr(MemoryRegion::END_EXTERNAL_RAM)) {
        return cartRAM[address + 8 * KB * static_cast<uint8_t>(eram_bank) -
                       addr(MemoryRegion::EXTERNAL_RAM)];
    }

    if (address >= addr(MemoryRegion::ECHO_RAM1) &&
        address <= addr(MemoryRegion::EECHO_RAM2)) {
        address -= 0x2000;
    }

    if (address >= addr(MemoryRegion::WORK_RAM_BANK0) &&
        address <= addr(MemoryRegion::END_WORK_RAM_BANK0)) {
        return workRAM[address - addr(MemoryRegion::WORK_RAM_BANK0)];
    }

    if (address >= addr(MemoryRegion::WORK_RAM_BANKN) &&
        address <= addr(MemoryRegion::END_WORK_RAM_BANKN)) {
        return workRAM[address + static_cast<uint8_t>(wram_bank) * 4 * KB -
                       addr(MemoryRegion::WORK_RAM_BANKN)];
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

    if (address >= 0xFEA0 && address <= 0xFEFF) {
        return 0xFF;  // Not usable area
    }

    if (address >= addr(MemoryRegion::NOT_USUABLE) &&
        address <= addr(MemoryRegion::END_NOT_USUABLE)) {
        return 0x00;
    }

    if (address == addr(MemoryRegion::IE)) return IEnable;

    throw std::runtime_error(std::string("read address not mapped ")
                                 .append(std::to_string(address)));
}

uint8_t address_bus::readIO(half address) {
    switch (address) {
        case static_cast<uint16_t>(IORegister::JOYP):
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
        return cartROM[address + 16 * KB * (rom_bank & 0x7F) -
                       addr(MemoryRegion::ROM_BANK_NN)];
    }

    if (address >= addr(MemoryRegion::VIDEO_RAM) &&
        address <= addr(MemoryRegion::END_VIDEO_RAM)) {
        return videoRAM[address + static_cast<uint8_t>(vram_bank) * 8 * KB -
                        addr(MemoryRegion::VIDEO_RAM)];
    }

    if (address >= addr(MemoryRegion::EXTERNAL_RAM) &&
        address <= addr(MemoryRegion::END_EXTERNAL_RAM)) {
        return cartRAM[address - addr(MemoryRegion::EXTERNAL_RAM)];
    }

    if (address >= addr(MemoryRegion::WORK_RAM_BANK0) &&
        address <= addr(MemoryRegion::END_WORK_RAM_BANK0)) {
        return workRAM[address - addr(MemoryRegion::WORK_RAM_BANK0)];
    }

    if (address >= addr(MemoryRegion::WORK_RAM_BANKN) &&
        address <= addr(MemoryRegion::END_WORK_RAM_BANKN)) {
        return workRAM[address + static_cast<uint8_t>(wram_bank) * 4 * KB -
                       addr(MemoryRegion::WORK_RAM_BANKN)];
    }

    if (address >= addr(MemoryRegion::ECHO_RAM1) &&
        address <= addr(MemoryRegion::EECHO_RAM1)) {
        return workRAM[address - addr(MemoryRegion::ECHO_RAM1)];
    }

    if (address >= addr(MemoryRegion::ECHO_RAM2) &&
        address <= addr(MemoryRegion::EECHO_RAM2)) {
        return workRAM[address + static_cast<uint8_t>(wram_bank) * 4 * KB -
                       addr(MemoryRegion::ECHO_RAM2)];
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
        return IOrange[address - addr(MemoryRegion::IO_REGISTERS)];
    }

    if (address >= addr(MemoryRegion::HIGH_RAM) &&
        address <= addr(MemoryRegion::END_HIGH_RAM)) {
        return HRAM[address - addr(MemoryRegion::HIGH_RAM)];
    }

    if (address == addr(MemoryRegion::IE)) return IEnable;

    throw std::runtime_error(std::string("read privileged address not mapped ")
                                 .append(std::to_string(address)));
}

void address_bus::write(half address, byte value) {
    if (address >= addr(MemoryRegion::VIDEO_RAM) &&
        address <= addr(MemoryRegion::END_VIDEO_RAM)) {
        if (lcd_mode == RenderingState::draw) return;
        videoRAM[address + static_cast<uint8_t>(vram_bank) * 8 * KB -
                 addr(MemoryRegion::VIDEO_RAM)] = value;
        return;
    }

    if (address >= addr(MemoryRegion::ECHO_RAM1) &&
        address <= addr(MemoryRegion::EECHO_RAM2)) {
        address -= 0x2000;
    }

    if (address >= addr(MemoryRegion::WORK_RAM_BANK0) &&
        address <= addr(MemoryRegion::END_WORK_RAM_BANK0)) {
        workRAM[address - addr(MemoryRegion::WORK_RAM_BANK0)] = value;
        return;
    }

    if (address >= addr(MemoryRegion::WORK_RAM_BANKN) &&
        address <= addr(MemoryRegion::END_WORK_RAM_BANKN)) {
        workRAM[address + static_cast<uint8_t>(wram_bank) * 4 * KB -
                addr(MemoryRegion::WORK_RAM_BANKN)] = value;
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

    if (address >= addr(MemoryRegion::EXTERNAL_RAM) &&
        address <= addr(MemoryRegion::END_EXTERNAL_RAM)) {
        if (RAMenable)
            cartRAM[address + (8 * KB * static_cast<uint8_t>(eram_bank)) -
                    addr(MemoryRegion::EXTERNAL_RAM)] = value;
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

    // Handle gap between OAM (0xFE9F) and IO registers (0xFF00)
    if (address >= 0xFEA0 && address <= 0xFEFF) {
        return;  // Not usable area
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
        case MemoryBankController::MBC3:  // MBC3 + ram + clock
            writeMBC3(address, value);
            return;
        case MemoryBankController::MBC5:
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
            throw std::runtime_error(
                std::string("not supported ")
                    .append(std::to_string(to_underlying(mbc))));
    }

    // throw std::runtime_error(std::string("mbc not valid
    // ").append(std::to_string(address)));
}

void address_bus::writeIO(half address, byte val) {
    if (address <= addr(AudioRegister::NR52) &&
        address >= addr(AudioRegister::NR10))
        std::ofstream("log.txt", std::ofstream::app)
            << "iowrite" << std::hex << address << '\n';
    switch (address) {
        case static_cast<uint16_t>(IORegister::JOYP):
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] =
                (val & 0xF0) |
                (IOrange[address - addr(MemoryRegion::IO_REGISTERS)] & 0x0F);
            return;
        case static_cast<uint16_t>(IORegister::DIV):  // 0xFF04
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = 0;
            return;
        case static_cast<uint16_t>(IORegister::TIMA):  // 0xFF05
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        case static_cast<uint16_t>(IORegister::TMA):  // 0xFF06
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        case static_cast<uint16_t>(IORegister::TAC):  // 0xFF07
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
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
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        case addr(AudioRegister::NR52):
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] =
                (val & 0xF0) |
                IOrange[address - addr(MemoryRegion::IO_REGISTERS)];
            return;
        case 0xFF30:  // Wave RAM start
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
        case 0xFF3f:  // Wave RAM end
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] =
                (val & 0xFE) |
                (IOrange[address - addr(MemoryRegion::IO_REGISTERS)] & 1);
            return;
        case addr(VideoRegister::LCDC):  // 0xFF40
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        case addr(VideoRegister::STAT):  // 0xFF41
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] =
                (val & 0xF8) |
                (IOrange[address - addr(MemoryRegion::IO_REGISTERS)] & 0x07);
            return;
        case addr(VideoRegister::SCY):  // 0xFF42
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        case addr(VideoRegister::SCX):  // 0xFF43
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        case addr(VideoRegister::LY):  // 0xFF44
            std::ofstream("log.txt", std::ofstream::app)
                << "wrote to LY" << std::hex << val;
            return;
        case addr(VideoRegister::LYC):  // 0xFF45
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        case addr(VideoRegister::OAMDMA):  // 0xFF46
            for (half i = 0; i < 0x9F; ++i) {
                OAM[i] = this->read_privileged(i + ((uint16_t)val << 8));
            }
            return;
        case addr(VideoRegister::BGP):  // 0xFF47
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        case addr(VideoRegister::OBP0):  // 0xFF48
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        case addr(VideoRegister::OBP1):  // 0xFF49
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        case addr(VideoRegister::WY):  // 0xFF4A
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
            return;
        case addr(VideoRegister::WX):  // 0xFF4B
            IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = val;
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
        if (value == 0xA) {
            RAMenable = 1;
        } else {
            RAMenable = 0;
        }
        return;
    }

    if (address <= 0x3FFF) {
        rom_bank = std::max(1, (value & 0x1F));
        return;
    }

    if (address <= 0x5FFF) {
        if (bank_mode == BankMode::RAM) {
            rom_bank |= value & 0x60;
        }
        return;
    }

    if (address <= 0x7fff) {
        // TODO

        return;
    }
    // throw std::runtime_error(std::string("write address not mapped
    // ").append(std::to_string(address)));
}

void address_bus::writeMBC3(half address, byte value) {
    if (address >= addr(MemoryRegion::ROM_BANK_00) && address <= 0x1FFF) {
        if (value == 0xA) {
            RAMenable = 1;
        } else {
            RAMenable = 0;
        }
        return;
    }

    if (address <= 0x3FFF) {
        rom_bank = std::max(1, (value & 0x7F));
        return;
    }

    if (address <= 0x5FFF) {
        if (value < 3) eram_bank = static_cast<ExternalRamBank>(value);
        return;
    }
    if (address <= 0x7fff) {
        return;
    }

    if (address >= addr(MemoryRegion::EXTERNAL_RAM) &&
        address <= addr(MemoryRegion::END_EXTERNAL_RAM)) {
        if (RAMenable)
            cartRAM[address + 8 * KB * static_cast<uint8_t>(eram_bank) -
                    addr(MemoryRegion::EXTERNAL_RAM)] = value;
        return;
    }

    throw std::runtime_error(std::string("write address not mapped ")
                                 .append(std::to_string(address)));
}

void address_bus::writeMBC2(half address, byte value) {
    if (address >= addr(MemoryRegion::ROM_BANK_00) && address <= 0x1FFF) {
        // MBC2 RAM enable/disable
        if ((address & 0x100) == 0) {
            if (value == 0x0A) {
                RAMenable = 1;
            } else {
                RAMenable = 0;
            }
        }
        return;
    }

    if (address <= 0x3FFF) {
        // ROM bank select (only lower 4 bits used for MBC2)
        if (address & 0x100) {
            rom_bank = std::max(1, (value & 0x0F));
        }
        return;
    }

    if (address >= addr(MemoryRegion::EXTERNAL_RAM) &&
        address <= addr(MemoryRegion::END_EXTERNAL_RAM)) {
        if (RAMenable) {
            // MBC2 has 512x4 bits RAM, only lower 4 bits are stored
            cartRAM[address - addr(MemoryRegion::EXTERNAL_RAM)] = value & 0x0F;
        }
        return;
    }

    throw std::runtime_error(std::string("MBC2 write address not mapped ")
                                 .append(std::to_string(address)));
}

void address_bus::writeMBC5(half address, byte value) {
    if (address >= addr(MemoryRegion::ROM_BANK_00) && address <= 0x1FFF) {
        // RAM enable/disable
        if (value == 0x0A) {
            RAMenable = 1;
        } else {
            RAMenable = 0;
        }
        return;
    }

    if (address <= 0x2FFF) {
        // ROM bank number (lower 8 bits)
        rom_bank = (rom_bank & 0x100) | value;
        return;
    }

    if (address <= 0x3FFF) {
        // ROM bank number (upper bit)
        rom_bank = (rom_bank & 0xFF) | ((value & 0x01) << 8);
        return;
    }

    if (address <= 0x5FFF) {
        // RAM bank number
        eram_bank = static_cast<ExternalRamBank>(value & 0x0F);
        return;
    }

    if (address >= addr(MemoryRegion::EXTERNAL_RAM) &&
        address <= addr(MemoryRegion::END_EXTERNAL_RAM)) {
        if (RAMenable)
            cartRAM[address + 8 * KB * static_cast<uint8_t>(eram_bank) -
                    addr(MemoryRegion::EXTERNAL_RAM)] = value;
        return;
    }

    throw std::runtime_error(std::string("MBC5 write address not mapped ")
                                 .append(std::to_string(address)));
}

void address_bus::writeMBC6(half address, byte value) {
    if (address >= addr(MemoryRegion::ROM_BANK_00) && address <= 0x1FFF) {
        // RAM enable/disable
        if (value == 0x0A) {
            RAMenable = 1;
        } else {
            RAMenable = 0;
        }
        return;
    }

    if (address <= 0x2FFF) {
        // ROM bank number (lower 8 bits)
        rom_bank = (rom_bank & 0x100) | value;
        return;
    }

    if (address <= 0x3FFF) {
        // ROM bank number (upper bit)
        rom_bank = (rom_bank & 0xFF) | ((value & 0x01) << 8);
        return;
    }

    if (address <= 0x5FFF) {
        // RAM bank number
        eram_bank = static_cast<ExternalRamBank>(value & 0x0F);
        return;
    }

    if (address >= addr(MemoryRegion::EXTERNAL_RAM) &&
        address <= addr(MemoryRegion::END_EXTERNAL_RAM)) {
        if (RAMenable)
            cartRAM[address + 8 * KB * static_cast<uint8_t>(eram_bank) -
                    addr(MemoryRegion::EXTERNAL_RAM)] = value;
        return;
    }

    throw std::runtime_error(std::string("MBC6 write address not mapped ")
                                 .append(std::to_string(address)));
}

void address_bus::writeMBC7(half address, byte value) {
    if (address >= addr(MemoryRegion::ROM_BANK_00) && address <= 0x1FFF) {
        // RAM enable/disable
        if (value == 0x0A) {
            RAMenable = 1;
        } else {
            RAMenable = 0;
        }
        return;
    }

    if (address <= 0x2FFF) {
        // ROM bank number (lower 8 bits)
        rom_bank = (rom_bank & 0x100) | value;
        return;
    }

    if (address <= 0x3FFF) {
        // ROM bank number (upper bit)
        rom_bank = (rom_bank & 0xFF) | ((value & 0x01) << 8);
        return;
    }

    if (address <= 0x5FFF) {
        // RAM bank number
        eram_bank = static_cast<ExternalRamBank>(value & 0x0F);
        return;
    }

    if (address >= addr(MemoryRegion::EXTERNAL_RAM) &&
        address <= addr(MemoryRegion::END_EXTERNAL_RAM)) {
        if (RAMenable)
            cartRAM[address + 8 * KB * static_cast<uint8_t>(eram_bank) -
                    addr(MemoryRegion::EXTERNAL_RAM)] = value;
        return;
    }

    throw std::runtime_error(std::string("MBC7 write address not mapped ")
                                 .append(std::to_string(address)));
}

void address_bus::writeHuC1(half address, byte value) {
    if (address >= addr(MemoryRegion::ROM_BANK_00) && address <= 0x1FFF) {
        // RAM enable/disable
        if (value == 0x0A) {
            RAMenable = 1;
        } else {
            RAMenable = 0;
        }
        return;
    }

    if (address <= 0x3FFF) {
        // ROM bank select (lower 5 bits)
        rom_bank = std::max(1, (value & 0x1F));
        return;
    }

    if (address <= 0x5FFF) {
        // RAM bank select or ROM bank upper bits
        if (bank_mode == BankMode::RAM) {
            // RAM banking mode
            eram_bank = static_cast<ExternalRamBank>(value & 0x03);
        } else {
            // ROM banking mode
            rom_bank |= (value & 0x60);
        }
        return;
    }

    if (address <= 0x7FFF) {
        // Banking mode select
        bank_mode = static_cast<BankMode>(value & 0x01);
        return;
    }

    if (address >= addr(MemoryRegion::EXTERNAL_RAM) &&
        address <= addr(MemoryRegion::END_EXTERNAL_RAM)) {
        if (RAMenable)
            cartRAM[address + 8 * KB * static_cast<uint8_t>(eram_bank) -
                    addr(MemoryRegion::EXTERNAL_RAM)] = value;
        return;
    }

    throw std::runtime_error(std::string("HuC1 write address not mapped ")
                                 .append(std::to_string(address)));
}

void address_bus::writeHuC3(half address, byte value) {
    if (address >= addr(MemoryRegion::ROM_BANK_00) && address <= 0x1FFF) {
        // RAM enable/disable
        if (value == 0x0A) {
            RAMenable = 1;
        } else {
            RAMenable = 0;
        }
        return;
    }

    if (address <= 0x3FFF) {
        // ROM bank select (lower 5 bits)
        rom_bank = std::max(1, (value & 0x1F));
        return;
    }

    if (address <= 0x5FFF) {
        // RAM bank select or ROM bank upper bits
        if (bank_mode == BankMode::RAM) {
            // RAM banking mode
            eram_bank = static_cast<ExternalRamBank>(value & 0x03);
        } else {
            // ROM banking mode
            rom_bank |= (value & 0x60);
        }
        return;
    }

    if (address <= 0x7FFF) {
        // Banking mode select
        bank_mode = static_cast<BankMode>(value & 0x01);
        return;
    }

    if (address >= addr(MemoryRegion::EXTERNAL_RAM) &&
        address <= addr(MemoryRegion::END_EXTERNAL_RAM)) {
        if (RAMenable)
            cartRAM[address + 8 * KB * static_cast<uint8_t>(eram_bank) -
                    addr(MemoryRegion::EXTERNAL_RAM)] = value;
        return;
    }

    throw std::runtime_error(std::string("HuC3 write address not mapped ")
                                 .append(std::to_string(address)));
}

void address_bus::write_privileged(half address, byte value) {
    if (address >= addr(MemoryRegion::VIDEO_RAM) &&
        address <= addr(MemoryRegion::END_VIDEO_RAM)) {
        videoRAM[address + static_cast<uint8_t>(vram_bank) * 8 * KB -
                 addr(MemoryRegion::VIDEO_RAM)] = value;
        return;
    }

    if (address >= addr(MemoryRegion::ECHO_RAM1) &&
        address <= addr(MemoryRegion::EECHO_RAM2)) {
        address -= 0x2000;
    }

    if (address >= addr(MemoryRegion::WORK_RAM_BANK0) &&
        address <= addr(MemoryRegion::END_WORK_RAM_BANK0)) {
        workRAM[address - addr(MemoryRegion::WORK_RAM_BANK0)] = value;
        return;
    }

    if (address >= addr(MemoryRegion::WORK_RAM_BANKN) &&
        address <= addr(MemoryRegion::END_WORK_RAM_BANKN)) {
        workRAM[address + static_cast<uint8_t>(wram_bank) * 4 * KB -
                addr(MemoryRegion::WORK_RAM_BANKN)] = value;
        return;
    }

    if (address >= addr(MemoryRegion::OAMaddress) &&
        address <= addr(MemoryRegion::END_OAM)) {
        OAM[address - addr(MemoryRegion::OAMaddress)] = value;
        return;
    }

    if (address >= addr(MemoryRegion::EXTERNAL_RAM) &&
        address <= addr(MemoryRegion::END_EXTERNAL_RAM)) {
        cartRAM[address + 8 * KB * static_cast<uint8_t>(eram_bank) -
                addr(MemoryRegion::EXTERNAL_RAM)] = value;
        return;
    }

    if (address >= addr(MemoryRegion::IO_REGISTERS) &&
        address <= addr(MemoryRegion::END_IO_REGISTERS)) {
        IOrange[address - addr(MemoryRegion::IO_REGISTERS)] = value;
        return;
    }

    if (address >= addr(MemoryRegion::HIGH_RAM) &&
        address <= addr(MemoryRegion::END_HIGH_RAM)) {
        HRAM[address - addr(MemoryRegion::HIGH_RAM)] = value;
        return;
    }

    if (address == addr(MemoryRegion::IE)) {
        IEnable = value;
        return;
    }

    throw std::runtime_error(
        std::string("mbc not valid").append(std::to_string(address)));
}

byte address_bus::check_set(half address, byte value) {
    byte check = read_privileged(address);
    return ((value ^ check) & check);
}
inline void address_bus::set_bit(half address, byte bit) {
    write_privileged(address, read_privileged(address) | (1 << bit));
}
inline void address_bus::reset_bit(half address, byte bit) {
    write_privileged(address, read_privileged(address) | ~(1 << bit));
}
}  // namespace GBC