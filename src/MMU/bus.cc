#include "bus.h"
#include <fstream>

namespace GBC {
    address_bus::address_bus() {
        memset(cartROM, 0, sizeof(cartROM));
        memset(cartRAM, 0, sizeof(cartRAM));
        memset(workRAM, 0, sizeof(workRAM));
        memset(videoRAM, 0, sizeof(videoRAM));
        memset(workRAM, 0, sizeof(workRAM));
        memset(OAM, 0, sizeof(OAM));
        memset(IOrange, 0, sizeof(IOrange));    
        IOrange[JOYP-IO_REGISTERS] = 0x3F;
    }

    void address_bus::load_boot_ROM(const char* fname, uint32_t size) {
        booting = 1;
        FILE* fp = fopen(fname, "r");

        for (int i = 0; i < size; ++i) {
            if (feof(fp) == EOF) break;
            bootrom[i] = fgetc(fp);
            std::cout << bootrom[i];
        }
        std::cout << std::endl;
    }
    

    void address_bus::load_ROM(const char* fname, uint32_t size) {
        FILE* fp = fopen(fname, "r");

        for (int i = 0; i < size; ++i) {
            if (feof(fp) == EOF) break;
            cartROM[i] = fgetc(fp);
        }
        mbc = cartROM[0x147];
        std::ofstream("log.txt", std::ofstream::app) << "mbc: " << std::hex << (int)mbc << '\n';
    }
    
    byte address_bus::read(half address) {
        if (address < 0x0100 && booting) {
            return bootrom[address];
        }

        if (address <= END_ROM_BANK_00) {
            return cartROM[address];
        }

        if (address >= ROM_BANK_NN && address <= END_ROM_BANK_NN) {
            return cartROM[address+16*KB*(rom_bank & 0x7F)-ROM_BANK_NN];
        }

        if (address >= VIDEO_RAM && address <= END_VIDEO_RAM) {
            if (lcd_mode == draw) return 0xFF;
            return videoRAM[address+vram_bank*8*KB-VIDEO_RAM]; 
        }

        if (address >= EXTERNAL_RAM && address <=END_EXTERNAL_RAM) {
            return cartRAM[address+8*KB*eram_bank-EXTERNAL_RAM];
        }
          
        if (address >= ECHO_RAM1 && address <= EECHO_RAM2) {
            address-=0x2000;
        }

        if (address >= WORK_RAM_BANK0 && address <= END_WORK_RAM_BANK0) {
            return workRAM[address-WORK_RAM_BANK0];
        }

        if (address >= WORK_RAM_BANKN && address <= END_WORK_RAM_BANKN) {
            return workRAM[address+wram_bank*4*KB-WORK_RAM_BANKN]; 
        }

        if (address >= OAMaddress && address <= END_OAM) {
            if (lcd_mode == draw || lcd_mode == OAMscan) return 0xFF;
            return OAM[address-OAMaddress];
        }

        if (address >= IO_REGISTERS && address <= END_IO_REGISTERS) {
            return readIO(address);
        }

        if (address >= HIGH_RAM && address <= END_HIGH_RAM) {
            return HRAM[address-HIGH_RAM];
        }

        if (address >= NOT_USUABLE && address <= END_NOT_USUABLE) {
            return 0x00;
        }

        if (address == IE) return IEnable; 

        throw std::runtime_error(std::string("read address not mapped ").append(std::to_string(address)));
    }

    uint8_t address_bus::readIO(half address) {
        switch (address) {
            case JOYP:
                if ((read_privledged(address) & (3 << 4)) == 0) {
                    return  (read_privledged(JOYP)&0x30) | (input_s & input_d & 0x0F);
                } else if ((read_privledged(address) & (1 << 4)) == 0) {
                    return  (read_privledged(JOYP)&0x30) | (input_d&0x0F);
                } else if ((read_privledged(address) & (2 << 4)) == 0) {
                    return (read_privledged(JOYP)&0x30) | (input_s&0x0F);
                } else {
                    return 0x3F;
                }
            default:
                return IOrange[address-IO_REGISTERS]; 
        }
    }

    byte address_bus::read_privledged(half address) {
        if (address < 0x0100 && booting) {
            return bootrom[address];
        }

        if (address <= END_ROM_BANK_00) 
            return cartROM[address];

        if (address >= ROM_BANK_NN && address <= END_ROM_BANK_NN) {
            return cartROM[address+16*KB*(rom_bank & 0x7F)-ROM_BANK_NN];
        }

        if (address >= VIDEO_RAM && address <= END_VIDEO_RAM) {
            return videoRAM[address+vram_bank*8*KB-VIDEO_RAM]; 
        }

        // if (address >= EXTERNAL_RAM && address <=END_EXTERNAL_RAM) {
        //     return cartRAM[address&0x1FF];
        // }
          

        if (address >= WORK_RAM_BANK0 && address <= END_WORK_RAM_BANK0) {
            return workRAM[address-WORK_RAM_BANK0];
        }

        if (address >= WORK_RAM_BANKN && address <= END_WORK_RAM_BANKN) {
            return workRAM[address+wram_bank*4*KB-WORK_RAM_BANKN]; 
        }
        
        if (address >= ECHO_RAM1 && address <= EECHO_RAM1) {
            return workRAM[address-ECHO_RAM1];
        }

        if (address >= ECHO_RAM2 && address <= EECHO_RAM2) {
            return workRAM[address+wram_bank*4*KB-ECHO_RAM2];
        }

        if (address >= OAMaddress && address <= END_OAM) {
            return OAM[address-OAMaddress]; 
        }

        if (address >= IO_REGISTERS && address <= END_IO_REGISTERS) {
            return IOrange[address-IO_REGISTERS]; 
        }

        if (address >= HIGH_RAM && address <= END_HIGH_RAM) {
            return HRAM[address-HIGH_RAM];
        }

        if (address == IE) return IEnable; 

        throw std::runtime_error(std::string("read PRIVLEDGED address not mapped ").append(std::to_string(address)));
    }

    void address_bus::write(half address, byte value) {
        if (address >= VIDEO_RAM && address <= END_VIDEO_RAM) {
            if (lcd_mode == draw) return;
            videoRAM[address+vram_bank*8*KB-VIDEO_RAM] = value;
            return;
        }

        if (address >= ECHO_RAM1 && address <= EECHO_RAM2) {
            address-=0x2000;
        }

        if (address >= WORK_RAM_BANK0 && address <= END_WORK_RAM_BANK0) {
            workRAM[address-WORK_RAM_BANK0] = value;
            return;
        }

        if (address >= WORK_RAM_BANKN && address <= END_WORK_RAM_BANKN) {
            workRAM[address+wram_bank*4*KB-WORK_RAM_BANKN] = value; 
            return;
        }

        if (address >= OAMaddress && address <= END_OAM) {
            if (lcd_mode == draw || lcd_mode == OAMscan) return;
            OAM[address-OAMaddress] = value;
            return;
        }
        
        if (address >= EXTERNAL_RAM && address <= END_EXTERNAL_RAM) {
            if (RAMenable) 
               cartRAM[address+8*KB*eram_bank-EXTERNAL_RAM] = value;
            return;
        }

        if (address >= IO_REGISTERS && address <= END_IO_REGISTERS) {
            writeIO(address, value);
            return;
        }

        if (address >= HIGH_RAM && address <= END_HIGH_RAM) {
            HRAM[address-HIGH_RAM] = value;
            return;
        }

        if (address == IE) {
            IEnable = value;
            return;
        }

        switch (mbc) {
            case 0x00:
                return;
            case 0x01:
            case 0x02:
            case 0x03:
            case 0x04:
            case 0x05:
            case 0x06:
            case 0x07:
            case 0x08:
            case 0x09:
            case 0x0A:
            case 0x0B:
            case 0x0C:
            case 0x0D:
            case 0x0E:
            case 0x0F:
            case 0x10:
                throw std::runtime_error(std::string("not supported ").append(std::to_string(mbc)));
                return;
            case 0x11:
            case 0x12:
            throw std::runtime_error(std::string("not supported ").append(std::to_string(mbc)));
            case 0x13: // MBC3 + ram + clock
                writeMBC3(address, value);
                return;
            default:
            throw std::runtime_error(std::string("not supported ").append(std::to_string(mbc)));
        }
        
        // throw std::runtime_error(std::string("mbc not valid ").append(std::to_string(address)));
    }



    void address_bus::writeIO(half address, byte val) {
        switch(address) {
            case JOYP:
                IOrange[address-IO_REGISTERS] = (val & 0xF0) | (IOrange[address-IO_REGISTERS] & 0x0F);
                return;
            case DIV: // 0xFF04
                IOrange[address-IO_REGISTERS] = 0;
                return;
            case TIMA: // 0xFF05
                IOrange[address-IO_REGISTERS] = val;
                return;
            case TMA: // 0xFF06
                IOrange[address-IO_REGISTERS] = val;
                return;
            case TAC: // 0xFF07
                IOrange[address-IO_REGISTERS] = val;
                return;
            case NR10: // 0xFF10
                IOrange[address-IO_REGISTERS] = val;
                return;
            case NR11: // 0xFF11
                IOrange[address-IO_REGISTERS] = val;
                return;
            case NR12: // 0xFF12
                IOrange[address-IO_REGISTERS] = val;
                return;
            case NR13: // 0xFF13
                IOrange[address-IO_REGISTERS] = val;
                return;
            case NR14: // 0xFF14
                IOrange[address-IO_REGISTERS] = val;
                return;
            case NR30: // 0xFF1A
                IOrange[address-IO_REGISTERS] = val;
                return;
            case NR31: // 0xFF1B
                IOrange[address-IO_REGISTERS] = val;
                return;
            case NR32: // 0xFF1C
                IOrange[address-IO_REGISTERS] = val;
                return;
            case NR33: // 0xFF1D
                IOrange[address-IO_REGISTERS] = val;
                return;
            case NR34: // 0xFF1E
                IOrange[address-IO_REGISTERS] = val;
                return;
            case NR41: // 0xFF20
                IOrange[address-IO_REGISTERS] = val;
                return;
            case NR42: // 0xFF21
                IOrange[address-IO_REGISTERS] = val;
                return;
            case NR43: // 0xFF22
                IOrange[address-IO_REGISTERS] = val;
                return;
            case NR44: // 0xFF23
                IOrange[address-IO_REGISTERS] = val;
                return;
            case NR50: // 0xFF24
                IOrange[address-IO_REGISTERS] = val;
                return;
            case NR51: // 0xFF25
                IOrange[address-IO_REGISTERS] = val;
                return;
            case NR52: // 0xFF26
                IOrange[address-IO_REGISTERS] = (val&0xFE)|(IOrange[address-IO_REGISTERS]&1);
                return;
            case 0xFF30: // Wave RAM start
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
            case 0xFF3f: // Wave RAM end
                IOrange[address-IO_REGISTERS] = (val&0xFE)|(IOrange[address-IO_REGISTERS]&1);
                return;
            case LCDC: // 0xFF40
                IOrange[address-IO_REGISTERS] = val;
                return;
            case STAT: // 0xFF41
                IOrange[address-IO_REGISTERS] = (val&0xF8) | (IOrange[address-IO_REGISTERS]&0x07);
                return;
            case SCY: // 0xFF42
                IOrange[address-IO_REGISTERS] = val;
                return;
            case SCX: // 0xFF43
                IOrange[address-IO_REGISTERS] = val;
                return;
            case LY: // 0xFF44
                std::ofstream("log.txt", std::ofstream::app) << "wrote to LY" << std::hex << val;
                return;
            case LYC: // 0xFF45
                IOrange[address-IO_REGISTERS] = val;
                return;
            case OAMDMA: // 0xFF46
                for (int i = 0; i < 0x9F; ++i) {
                    OAM[i] = this->read_privledged(i+((uint16_t)val << 8));
                }
                return;
            case BGP: // 0xFF47
                IOrange[address-IO_REGISTERS] = val;
                return;
            case OBP0: // 0xFF48
                IOrange[address-IO_REGISTERS] = val;
                return;
            case OBP1: // 0xFF49
                IOrange[address-IO_REGISTERS] = val;
                return;
            case WY: // 0xFF4A
                IOrange[address-IO_REGISTERS] = val;
                return;
            case WX: // 0xFF4B
                IOrange[address-IO_REGISTERS] = val;
                return;
            case IF:
                IOrange[address-IO_REGISTERS] = val;
                return;
            default: 
                debug = 1;
                std::ofstream("log.txt", std::ofstream::app) << "unsupported IO write: " << std::hex << address << std::endl;   
        }
    }

    void address_bus::writeMBC1(half address, byte value) {
        if (address >= ROM_BANK_00 && address <= 0x1FFF) {
            if (value == 0xA) {
                RAMenable = 1;
            } else {
                RAMenable = 0;
            }
            return;
        }

        if (address <= 0x3FFF) {
            rom_bank = std::max(1,(value & 0x1F));
            return;
        }

        if (address <= 0x5FFF) {
        if (bank_mode & 1) {
            rom_bank |= value & 0x60;
        }
            return;
        }

        if (address <= 0x7fff) {
            // TODO

            return;
        }


        // throw std::runtime_error(std::string("write address not mapped ").append(std::to_string(address)));
    }

    void address_bus::writeMBC3(half address, byte value) {
        if (address >= ROM_BANK_00 && address <= 0x1FFF) {
            if (value == 0xA) {
                RAMenable = 1;
            } else {
                RAMenable = 0;
            }
            return;
        }

        if (address <= 0x3FFF) {
            rom_bank = std::max(1,(value & 0x7F));
            return;
        }

        if (address <= 0x5FFF) {
            if (value < 3)
            eram_bank = value;
            return;
        }
        if (address <= 0x7fff) {
            return;
        }

        if (address >= EXTERNAL_RAM && address <= END_EXTERNAL_RAM) {
            if (RAMenable) 
               cartRAM[address+8*KB*eram_bank-EXTERNAL_RAM] = value;
            return;
        }

        throw std::runtime_error(std::string("write address not mapped ").append(std::to_string(address)));
    }

}