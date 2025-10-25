#pragma once

#include <array>
#include <cstdint>

#include "bus.h"

#define RA registers[7]
#define RB registers[0]
#define RC registers[1]
#define RD registers[2]
#define RE registers[3]
#define RF registers[6]
#define R6 registers[6]
#define RH registers[4]
#define RL registers[5]

namespace GBC {
using byte = uint8_t;
constexpr uint32_t SINGLE_TIME_MHZ = 4194304;
constexpr uint16_t STACK_POINTER_BASE = 0xFFFE;

enum class Instruction : uint8_t {
    NOP = 0x00,
    LD_BC_d16 = 0x01,
    LD_mBC_A = 0x02,
    INC_BC = 0x03,
    INC_B = 0x04,
    DEC_B = 0x05,
    LD_B_d8 = 0x06,
    RLCA = 0x07,
    LD_mImm16_SP = 0x08,
    ADD_HL_BC = 0x09,
    LD_A_mBC = 0x0A,
    DEC_BC = 0x0B,
    INC_C = 0x0C,
    DEC_C = 0x0D,
    LD_C_d8 = 0x0E,
    RRCA = 0x0F,
    STOP = 0x10,
    LD_DE_d16 = 0x11,
    LD_mDE_A = 0x12,
    INC_DE = 0x13,
    INC_D = 0x14,
    DEC_D = 0x15,
    LD_D_d8 = 0x16,
    RLA = 0x17,
    JR_d8 = 0x18,
    ADD_HL_DE = 0x19,
    LD_A_mDE = 0x1A,
    DEC_DE = 0x1B,
    INC_E = 0x1C,
    DEC_E = 0x1D,
    LD_E_d8 = 0x1E,
    RRA = 0x1F,
    JR_NZ_d8 = 0x20,
    LD_HL_d16 = 0x21,
    LDI_mHL_A = 0x22,
    INC_HL = 0x23,
    INC_H = 0x24,
    DEC_H = 0x25,
    LD_H_d8 = 0x26,
    DAA = 0x27,
    JR_Z_d8 = 0x28,
    ADD_HL_HL = 0x29,
    LDI_A_mHL = 0x2A,
    DEC_HL = 0x2B,
    INC_L = 0x2C,
    DEC_L = 0x2D,
    LD_L_d8 = 0x2E,
    CPL = 0x2F,
    JR_NC_d8 = 0x30,
    LD_SP_d16 = 0x31,
    LDD_mHL_A = 0x32,
    INC_SP = 0x33,
    INC_mHL = 0x34,
    DEC_mHL = 0x35,
    LD_mHL_d8 = 0x36,
    SCF = 0x37,
    JR_C_d8 = 0x38,
    ADD_HL_SP = 0x39,
    LDD_A_mHL = 0x3A,
    DEC_SP = 0x3B,
    INC_A = 0x3C,
    DEC_A = 0x3D,
    LD_A_d8 = 0x3E,
    CCF = 0x3F,
    LD_B_B = 0x40,
    LD_B_C = 0x41,
    LD_B_D = 0x42,
    LD_B_E = 0x43,
    LD_B_H = 0x44,
    LD_B_L = 0x45,
    LD_B_mHL = 0x46,
    LD_B_A = 0x47,
    LD_C_B = 0x48,
    LD_C_C = 0x49,
    LD_C_D = 0x4A,
    LD_C_E = 0x4B,
    LD_C_H = 0x4C,
    LD_C_L = 0x4D,
    LD_C_mHL = 0x4E,
    LD_C_A = 0x4F,
    LD_D_B = 0x50,
    LD_D_C = 0x51,
    LD_D_D = 0x52,
    LD_D_E = 0x53,
    LD_D_H = 0x54,
    LD_D_L = 0x55,
    LD_D_mHL = 0x56,
    LD_D_A = 0x57,
    LD_E_B = 0x58,
    LD_E_C = 0x59,
    LD_E_D = 0x5A,
    LD_E_E = 0x5B,
    LD_E_H = 0x5C,
    LD_E_L = 0x5D,
    LD_E_mHL = 0x5E,
    LD_E_A = 0x5F,
    LD_H_B = 0x60,
    LD_H_C = 0x61,
    LD_H_D = 0x62,
    LD_H_E = 0x63,
    LD_H_H = 0x64,
    LD_H_L = 0x65,
    LD_H_mHL = 0x66,
    LD_H_A = 0x67,
    LD_L_B = 0x68,
    LD_L_C = 0x69,
    LD_L_D = 0x6A,
    LD_L_E = 0x6B,
    LD_L_H = 0x6C,
    LD_L_L = 0x6D,
    LD_L_mHL = 0x6E,
    LD_L_A = 0x6F,
    LD_mHL_B = 0x70,
    LD_mHL_C = 0x71,
    LD_mHL_D = 0x72,
    LD_mHL_E = 0x73,
    LD_mHL_H = 0x74,
    LD_mHL_L = 0x75,
    HALT = 0x76,
    LD_mHL_A = 0x77,
    LD_A_B = 0x78,
    LD_A_C = 0x79,
    LD_A_D = 0x7A,
    LD_A_E = 0x7B,
    LD_A_H = 0x7C,
    LD_A_L = 0x7D,
    LD_A_mHL = 0x7E,
    LD_A_A = 0x7F,
    ADD_A_B = 0x80,
    ADD_A_C = 0x81,
    ADD_A_D = 0x82,
    ADD_A_E = 0x83,
    ADD_A_H = 0x84,
    ADD_A_L = 0x85,
    ADD_A_mHL = 0x86,
    ADD_A_A = 0x87,
    ADC_A_B = 0x88,
    ADC_A_C = 0x89,
    ADC_A_D = 0x8A,
    ADC_A_E = 0x8B,
    ADC_A_H = 0x8C,
    ADC_A_L = 0x8D,
    ADC_A_mHL = 0x8E,
    ADC_A_A = 0x8F,
    SUB_B = 0x90,
    SUB_C = 0x91,
    SUB_D = 0x92,
    SUB_E = 0x93,
    SUB_H = 0x94,
    SUB_L = 0x95,
    SUB_mHL = 0x96,
    SUB_A = 0x97,
    SBC_A_B = 0x98,
    SBC_A_C = 0x99,
    SBC_A_D = 0x9A,
    SBC_A_E = 0x9B,
    SBC_A_H = 0x9C,
    SBC_A_L = 0x9D,
    SBC_A_mHL = 0x9E,
    SBC_A_A = 0x9F,
    AND_B = 0xA0,
    AND_C = 0xA1,
    AND_D = 0xA2,
    AND_E = 0xA3,
    AND_H = 0xA4,
    AND_L = 0xA5,
    AND_mHL = 0xA6,
    AND_A = 0xA7,
    XOR_B = 0xA8,
    XOR_C = 0xA9,
    XOR_D = 0xAA,
    XOR_E = 0xAB,
    XOR_H = 0xAC,
    XOR_L = 0xAD,
    XOR_mHL = 0xAE,
    XOR_A = 0xAF,
    OR_B = 0xB0,
    OR_C = 0xB1,
    OR_D = 0xB2,
    OR_E = 0xB3,
    OR_H = 0xB4,
    OR_L = 0xB5,
    OR_mHL = 0xB6,
    OR_A = 0xB7,
    CP_B = 0xB8,
    CP_C = 0xB9,
    CP_D = 0xBA,
    CP_E = 0xBB,
    CP_H = 0xBC,
    CP_L = 0xBD,
    CP_mHL = 0xBE,
    CP_A = 0xBF,
    RET_NZ = 0xC0,
    POP_BC = 0xC1,
    JP_NZ_d16 = 0xC2,
    JP_d16 = 0xC3,
    CALL_NZ_d16 = 0xC4,
    PUSH_BC = 0xC5,
    ADD_A_d8 = 0xC6,
    RST_00H = 0xC7,
    RET_Z = 0xC8,
    RET = 0xC9,
    JP_Z_d16 = 0xCA,
    PREFIX_CB = 0xCB,
    CALL_Z_d16 = 0xCC,
    CALL_d16 = 0xCD,
    ADC_A_d8 = 0xCE,
    RST_08H = 0xCF,
    RET_NC = 0xD0,
    POP_DE = 0xD1,
    JP_NC_d16 = 0xD2,
    // Undocumented/illegal opcodes retain placeholder names
    ILLEGAL_D3 = 0xD3,
    CALL_NC_d16 = 0xD4,
    PUSH_DE = 0xD5,
    SUB_d8 = 0xD6,
    RST_10H = 0xD7,
    RET_C = 0xD8,
    RETI = 0xD9,
    JP_C_d16 = 0xDA,
    ILLEGAL_DB = 0xDB,
    CALL_C_d16 = 0xDC,
    ILLEGAL_DD = 0xDD,
    SBC_A_d8 = 0xDE,
    RST_18H = 0xDF,
    LDH_mImm8_A = 0xE0,
    POP_HL = 0xE1,
    LDH_mC_A = 0xE2,
    ILLEGAL_E3 = 0xE3,
    ILLEGAL_E4 = 0xE4,
    PUSH_HL = 0xE5,
    AND_d8 = 0xE6,
    RST_20H = 0xE7,
    ADD_SP_d8 = 0xE8,
    JP_mHL = 0xE9,
    LD_mImm16_A = 0xEA,
    ILLEGAL_EB = 0xEB,
    ILLEGAL_EC = 0xEC,
    ILLEGAL_ED = 0xED,
    XOR_d8 = 0xEE,
    RST_28H = 0xEF,
    LDH_A_mImm8 = 0xF0,
    POP_AF = 0xF1,
    LDH_A_mC = 0xF2,
    DI = 0xF3,
    ILLEGAL_F4 = 0xF4,
    PUSH_AF = 0xF5,
    OR_d8 = 0xF6,
    RST_30H = 0xF7,
    LD_HL_SP_d8 = 0xF8,
    LD_SP_HL = 0xF9,
    LD_A_mImm16 = 0xFA,
    EI = 0xFB,
    ILLEGAL_FC = 0xFC,
    ILLEGAL_FD = 0xFD,
    CP_d8 = 0xFE,
    RST_38H = 0xFF
};
class SM83 {
   public:
    SM83(GBC::address_bus *memory) : memory(memory) {}

    GBC::address_bus *memory;

    std::array<GBC::byte, 8> registers{};

    uint32_t divcounter = 0;
    uint32_t timacounter = 0;
    uint32_t timareg = 0;
    uint32_t tacreg =
        0;  // TODO probably should maybe make these have underscores
    uint16_t pc = 0;
    uint16_t sp = STACK_POINTER_BASE;
    uint16_t opcode = 0;
    uint8_t cycles = 0;
    bool IME = false;
    bool IMEdelay = false;
    bool halted = false;
    bool stathigh = false;

    void execute_cycle();
    inline void executeCB();

    //===========================================================
    // Flag accessors – F is stored in r8[6]
    //===========================================================
    [[nodiscard]] bool getZeroFlag() const;
    [[nodiscard]] bool getNFlag() const;
    [[nodiscard]] bool getHalfCarryFlag() const;
    [[nodiscard]] bool getCarryFlag() const;

    void setZeroFlag(bool val);
    void setNFlag(bool val);
    void setHalfCarryFlag(bool val);
    void setCarryFlag(bool val);

    //===========================================================
    // Register-pair accessors
    //===========================================================
    [[nodiscard]] uint16_t getAF() const;
    [[nodiscard]] uint16_t getBC() const;
    [[nodiscard]] uint16_t getDE() const;
    [[nodiscard]] uint16_t getHL() const;
    void setAF(uint16_t val);  //
    void setBC(uint16_t val);
    void setDE(uint16_t val);
    void setHL(uint16_t val);

    //===========================================================
    // Fetch functions
    //===========================================================
    uint8_t fetch8();
    uint16_t fetch16();

    //===========================================================
    // Helper functions for flag calculation
    //===========================================================
    bool halfCarryAdd(uint8_t a, uint8_t b);
    bool halfCarryAdd_WithCarry(uint8_t a, uint8_t b);
    bool halfCarrySub(uint8_t a, uint8_t b);
    bool halfCarrySub_WithCarry(uint8_t a, uint8_t b);

    //===========================================================
    // Instruction functions
    // Grouped by opcode block and similar functionality.
    //===========================================================

    // --- Block 0 (opcodes starting with 00)
    inline void instrNOP();
    inline void instrLdImm16SP();
    inline void instrRLCA();
    inline void instrRRCA();
    inline void instrRLA();
    inline void instrRRA();
    inline void instrLdiHL_A();
    inline void instrDAA();
    inline void instrLdA_HL();
    inline void instrCPL();
    inline void instrLddHL_A();
    inline void instrIncSP();
    inline void instrSCF();
    inline void instrLdA_HLDec();
    inline void instrCCF();
    inline void instrJR_Imm8();

    inline void instrJR_Cond_Imm8(bool condition);

    inline void instrLdR16_Imm16(uint8_t regIndex);
    inline void instrAddHL_R16(uint8_t regIndex);
    inline void instrLdR16Mem_A(uint8_t regIndex);
    inline void instrLdA_R16Mem(uint8_t regIndex);
    inline void instrIncR16(uint8_t regIndex);
    inline void instrDecR16(uint8_t regIndex);

    // 8-bit arithmetic and load instructions
    inline void instrIncR8(uint8_t reg);
    inline void instrDecR8(uint8_t reg);
    inline void instrIncHL();
    inline void instrDecHL();
    inline void instrLdImm8_R8(uint8_t reg);
    inline void instrLdR8_Imm8(uint8_t reg);
    inline void instrLdImm8_HL();

    // --- Block 1 (opcodes starting with 01)
    inline void instrHALT();
    inline void instrLdR8_FromHL(uint8_t reg);
    inline void instrLdHL_FromR8(uint8_t reg);
    inline void instrLdR8_R8(uint8_t dest, uint8_t src);

    // --- Block 2 (ALU operations using A and an 8-bit register)
    inline void instrAddA_R8(uint8_t reg);
    inline void instrAdcA_R8(uint8_t reg);
    inline void instrSubA_R8(uint8_t reg);
    inline void instrSbcA_R8(uint8_t reg);
    inline void instrAndA_R8(uint8_t reg);
    inline void instrXorA_R8(uint8_t reg);
    inline void instrOrA_R8(uint8_t reg);
    inline void instrCpA_R8(uint8_t reg);

    // --- Block 3 (ALU operations with immediate data and control flow)
    inline void instrAddA_Imm8();
    inline void instrAdcA_Imm8();
    inline void instrSubA_Imm8();
    inline void instrSbcA_Imm8();
    inline void instrAndA_Imm8();
    inline void instrXorA_Imm8();
    inline void instrOrA_Imm8();
    inline void instrCpA_Imm8();

    inline void instrRET();
    inline void instrRETI();
    inline void instrJP_Imm16();
    inline void instrJP_HL();
    inline void instrCALL_Imm16();
    inline void instrLDH_C_A();
    inline void instrLDH_Imm8_A();
    inline void instrLD_Imm16_A();
    inline void instrLDH_A_C();
    inline void instrLDH_A_Imm8();
    inline void instrLD_A_Imm16();
    inline void instrAddSP_Imm8();
    inline void instrLD_HL_SP_Imm8();
    inline void instrLD_SP_HL();
    inline void instrDI();
    inline void instrEI();

    inline void instrRET_Cond(bool condition);
    inline void instrJP_Cond_Imm16(bool condition);
    inline void instrCALL_Cond_Imm16(bool condition);

    inline void instrRST(uint8_t target);
    inline void instrPOP_R16(uint8_t regIndex);
    inline void instrPUSH_R16(uint8_t regIndex);

    // --- CB-Prefixed instructions (bit/rotate operations)
    inline void instrRLC_R8(uint8_t reg);
    inline void instrRRC_R8(uint8_t reg);
    inline void instrRL_R8(uint8_t reg);
    inline void instrRR_R8(uint8_t reg);
    inline void instrSLA_R8(uint8_t reg);
    inline void instrSRA_R8(uint8_t reg);
    inline void instrSWAP_R8(uint8_t reg);
    inline void instrSRL_R8(uint8_t reg);
    inline void instrBIT_R8(uint8_t bit, uint8_t reg);
    inline void instrRES_R8(uint8_t bit, uint8_t reg);
    inline void instrSET_R8(uint8_t bit, uint8_t reg);

    //===========================================================
    // 16-bit register store/load helper functions
    //===========================================================
    inline void store16t1(uint8_t reg16, uint16_t val);
    inline void store16t2(uint8_t reg16, uint16_t val);
    inline uint16_t load16t1(uint8_t reg16);
    inline uint16_t load16t2(uint8_t reg16);

    inline void instrSTOP();
    inline void processDAA();

    // Interrupt Handler
    inline void call_interrupt(uint16_t handler);

    // Increment timer registers
    inline void increment_timer();

    // Debug, outputs to "log.txt"
    void dump_registers();
    void dump_info();

    friend class GBC;
    friend class address_bus;
};
}  // namespace GBC