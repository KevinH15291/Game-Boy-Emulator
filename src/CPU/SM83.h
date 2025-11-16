#pragma once

#include "../MMU/bus.h"
#include "../bit_ops.h"

#define RA r8[7]
#define RB r8[0]
#define RC r8[1]
#define RD r8[2]
#define RE r8[3]
#define RF r8[6]
#define R6 r8[6]
#define RH r8[4]
#define RL r8[5]

namespace GBC {
constexpr uint32_t SINGLE_TIME_MHZ = 4194304;
class SM83 {
   public:
    SM83(address_bus *memory, CgbConfig &config) : memory(memory), config(config) {
        memset(r8, 0, 8);
    }

    address_bus *memory;
    CgbConfig &config;

    byte r8[8];

    uint32_t divcounter = 0, timacounter = 0, timareg,
             tacreg;  // TODO probably should maybe make these have underscores
    half pc = 0, sp = 0xFFFE;
    half opcode = 0;
    byte cycles = 0;
    bool IME = false, IMEdelay = false, halted = false, stathigh = false;
    bool tima_written_this_cycle = false;

    void execute();
    inline void executeCB();
    void reset_timer_counter() {
        timacounter = 0;
        tima_written_this_cycle = false;
    }
    void mark_tima_written() { tima_written_this_cycle = true; }

    [[gnu::always_inline]] inline bool getZeroFlag() const {
        return isBitSet(r8[6], 7);
    }
    [[gnu::always_inline]] inline bool getNFlag() const {
        return isBitSet(r8[6], 6);
    }
    [[gnu::always_inline]] inline bool getHalfCarryFlag() const {
        return isBitSet(r8[6], 5);
    }
    [[gnu::always_inline]] inline bool getCarryFlag() const {
        return isBitSet(r8[6], 4);
    }

    [[gnu::always_inline]] inline void setZeroFlag(bool val) {
        r8[6] = val ? setBit(r8[6], 7) : clearBit(r8[6], 7);
    }
    [[gnu::always_inline]] inline void setNFlag(bool val) {
        r8[6] = val ? setBit(r8[6], 6) : clearBit(r8[6], 6);
    }
    [[gnu::always_inline]] inline void setHalfCarryFlag(bool val) {
        r8[6] = val ? setBit(r8[6], 5) : clearBit(r8[6], 5);
    }
    [[gnu::always_inline]] inline void setCarryFlag(bool val) {
        r8[6] = val ? setBit(r8[6], 4) : clearBit(r8[6], 4);
    }

    [[gnu::always_inline]] inline half getAF() const {
        return (r8[7] << 8) | r8[6];
    }
    [[gnu::always_inline]] inline half getBC() const {
        return (r8[0] << 8) | r8[1];
    }
    [[gnu::always_inline]] inline half getDE() const {
        return (r8[2] << 8) | r8[3];
    }
    [[gnu::always_inline]] inline half getHL() const {
        return (r8[4] << 8) | r8[5];
    }
    [[gnu::always_inline]] inline void setAF(half val) {
        r8[7] = val >> 8;
        r8[6] = val & 0xF0;
    }
    [[gnu::always_inline]] inline void setBC(half val) {
        r8[0] = val >> 8;
        r8[1] = val & 0xFF;
    }
    [[gnu::always_inline]] inline void setDE(half val) {
        r8[2] = val >> 8;
        r8[3] = val & 0xFF;
    }
    [[gnu::always_inline]] inline void setHL(half val) {
        r8[4] = val >> 8;
        r8[5] = val & 0xFF;
    }

    [[gnu::always_inline]] inline byte fetch8() {
        return memory->read(pc++);
    }
    [[gnu::always_inline]] inline half fetch16() {
        half result = memory->read(pc) | (memory->read(pc + 1) << 8);
        pc += 2;
        return result;
    }

    inline bool halfCarryAdd(byte a, byte b) {
        return ((a & 0xF) + (b & 0xF)) > 0xF;
    }
    inline bool halfCarryAdd_WithCarry(byte a, byte b) {
        return ((a & 0xF) + (b & 0xF) + getCarryFlag()) > 0xF;
    }
    inline bool halfCarrySub(byte a, byte b) {
        return (a & 0xF) < (b & 0xF);
    }
    inline bool halfCarrySub_WithCarry(byte a, byte b) {
        return (a & 0xF) < ((b & 0xF) + getCarryFlag());
    }

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

    inline void instrLdR16_Imm16(byte regIndex);
    inline void instrAddHL_R16(byte regIndex);
    inline void instrLdR16Mem_A(byte regIndex);
    inline void instrLdA_R16Mem(byte regIndex);
    inline void instrIncR16(byte regIndex);
    inline void instrDecR16(byte regIndex);

    // 8-bit arithmetic and load instructions
    inline void instrIncR8(byte reg);
    inline void instrDecR8(byte reg);
    inline void instrIncHL();
    inline void instrDecHL();
    inline void instrLdImm8_R8(byte reg);
    inline void instrLdR8_Imm8(byte reg);
    inline void instrLdImm8_HL();

    inline void instrHALT();
    inline void instrLdR8_FromHL(byte reg);
    inline void instrLdHL_FromR8(byte reg);
    inline void instrLdR8_R8(byte dest, byte src);

    inline void instrAddA_R8(byte reg);
    inline void instrAdcA_R8(byte reg);
    inline void instrSubA_R8(byte reg);
    inline void instrSbcA_R8(byte reg);
    inline void instrAndA_R8(byte reg);
    inline void instrXorA_R8(byte reg);
    inline void instrOrA_R8(byte reg);
    inline void instrCpA_R8(byte reg);

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

    inline void instrRST(byte target);
    inline void instrPOP_R16(byte regIndex);
    inline void instrPUSH_R16(byte regIndex);

    inline void instrRLC_R8(byte reg);
    inline void instrRRC_R8(byte reg);
    inline void instrRL_R8(byte reg);
    inline void instrRR_R8(byte reg);
    inline void instrSLA_R8(byte reg);
    inline void instrSRA_R8(byte reg);
    inline void instrSWAP_R8(byte reg);
    inline void instrSRL_R8(byte reg);
    inline void instrBIT_R8(byte bit, byte reg);
    inline void instrRES_R8(byte bit, byte reg);
    inline void instrSET_R8(byte bit, byte reg);

    inline void store16t1(byte reg16, half val);
    inline void store16t2(byte reg16, half val);
    inline half load16t1(byte reg16);
    inline half load16t2(byte reg16);

    inline void instrSTOP();

    inline void call_interrupt(half handler);

    inline void increment_timer();

    void dump_registers();
    void dump_info();
};
}  // namespace GBC