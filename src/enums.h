#pragma once

#include <cstdint>

enum class MemoryRegion : uint16_t {
    BOOT_ROM = 0x0100,

    ROM_BANK_00 = 0x0000,
    END_ROM_BANK_00 = 0x3FFF,

    ROM_BANK_NN = 0x4000,
    END_ROM_BANK_NN = 0x7FFF,

    VIDEO_RAM = 0x8000,
    END_VIDEO_RAM = 0x9FFF,

    EXTERNAL_RAM = 0xA000,
    END_EXTERNAL_RAM = 0xBFFF,

    WORK_RAM_BANK0 = 0xC000,
    END_WORK_RAM_BANK0 = 0xCFFF,

    WORK_RAM_BANKN = 0xD000,
    END_WORK_RAM_BANKN = 0xDFFF,

    ECHO_RAM1 = 0xE000,
    EECHO_RAM1 = 0xEFFF,

    ECHO_RAM2 = 0xF000,
    EECHO_RAM2 = 0xFDFF,

    SPRITEATTRIBUTETABLE = 0xFE00,
    OAMaddress = 0xFE00,
    END_OAM = 0XFE9F,

    IO_REGISTERS = 0xFF00,
    END_IO_REGISTERS = 0xFF7F,

    NOT_USUABLE = 0xFFA0,
    END_NOT_USUABLE = 0xFF9F,

    HIGH_RAM = 0xFF80,
    END_HIGH_RAM = 0XFFFE,

    IE = 0xFFFF
};

enum class IORegister : uint16_t {
    JOYP = 0xFF00,

    SB = 0xFF01,
    SC = 0xFF02,

    DIV = 0xFF04,
    TIMA = 0xFF05,
    TMA = 0xFF06,
    TAC = 0xFF07,

    IF = 0xFF0F
};

enum class AudioRegister : uint16_t {
    NR10 = 0xFF10,
    NR11 = 0xFF11,
    NR12 = 0xFF12,
    NR13 = 0xFF13,
    NR14 = 0xFF14,

    NR21 = 0xFF16,
    NR22 = 0xFF17,
    NR23 = 0xFF18,
    NR24 = 0xFF19,

    NR30 = 0xFF1A,
    NR31 = 0xFF1B,
    NR32 = 0xFF1C,
    NR33 = 0xFF1D,
    NR34 = 0xFF1E,

    NR41 = 0xFF20,
    NR42 = 0xFF21,
    NR43 = 0xFF22,
    NR44 = 0xFF23,

    NR50 = 0xFF24,
    NR51 = 0xFF25,
    NR52 = 0xFF26,

    WavePatternRAM = 0xFF30
};

enum class VideoRegister : uint16_t {
    LCDC = 0xFF40,
    STAT = 0xFF41,
    SCY = 0xFF42,
    SCX = 0xFF43,
    LY = 0xFF44,
    LYC = 0xFF45,
    OAMDMA = 0xFF46,
    BGP = 0xFF47,
    OBP0 = 0xFF48,
    OBP1 = 0xFF49,
    WY = 0xFF4A,
    WX = 0xFF4B
};

enum class CGBRegister : uint16_t {
    KEY0 = 0xFF4C,
    KEY1 = 0xFF4D,
    VBK = 0xFF4F,
    HDMA1 = 0xFF51,
    HDMA2 = 0xFF52,
    HDMA3 = 0xFF53,
    HDMA4 = 0xFF54,
    HDMA5 = 0xFF55,
    RP = 0xFF56,
    BGPI = 0xFF68,
    BGPD = 0xFF69,
    OBPI = 0xFF6A,
    OBPD = 0xFF6B,
    OPRI = 0xFF6C,
    SVBK = 0xFF70
};