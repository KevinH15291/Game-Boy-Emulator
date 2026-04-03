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
    SM83(address_bus *memory, CgbConfig &config)
        : memory(memory), config(config) {}

    address_bus *memory;
    CgbConfig &config;

    std::array<byte, 8> r8{};

    uint32_t divcounter = 0;
    uint32_t timareg = 0;
    uint32_t tacreg = 0;
    byte prev_timer_bit = 0;
    byte tima_overflow_cycles = 0;

    half pc = 0;
    half sp = 0xFFFE;
    half opcode = 0;
    byte cycles = 0;

    bool IME = false;
    bool IMEdelay = false;
    bool halted = false;
    bool stathigh = false;
    bool tima_written_this_cycle = false;
    bool halt_bug = false;

    void execute();
    inline void executeCB();
    void reset_timer_counter() {
        divcounter = 0;
        tima_written_this_cycle = false;
        prev_timer_bit = 0;
    }
    void mark_tima_written() { tima_written_this_cycle = true; }

    [[gnu::always_inline]] bool getZeroFlag() const;
    [[gnu::always_inline]] bool getNFlag() const;
    [[gnu::always_inline]] bool getHalfCarryFlag() const;
    [[gnu::always_inline]] bool getCarryFlag() const;

    [[gnu::always_inline]] void setZeroFlag(bool val);
    [[gnu::always_inline]] void setNFlag(bool val);
    [[gnu::always_inline]] void setHalfCarryFlag(bool val);
    [[gnu::always_inline]] void setCarryFlag(bool val);

    [[gnu::always_inline]] half getAF() const;
    [[gnu::always_inline]] half getBC() const;
    [[gnu::always_inline]] half getDE() const;
    [[gnu::always_inline]] half getHL() const;
    [[gnu::always_inline]] void setAF(half val);
    [[gnu::always_inline]] void setBC(half val);
    [[gnu::always_inline]] void setDE(half val);
    [[gnu::always_inline]] void setHL(half val);

    [[gnu::always_inline]] byte fetch8();
    [[gnu::always_inline]] half fetch16();

    [[gnu::always_inline]] static bool halfCarryAdd(byte a, byte b);
    [[gnu::always_inline]] static bool halfCarrySub(byte a, byte b);
    [[gnu::always_inline]] bool halfCarryAdd_WithCarry(byte a, byte b) const;
    [[gnu::always_inline]] bool halfCarrySub_WithCarry(byte a, byte b) const;

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
    [[nodiscard]] inline half load16t1(byte reg16) const;
    [[nodiscard]] inline half load16t2(byte reg16) const;

    inline void instrSTOP();

    inline void call_interrupt(half handler);

    inline void increment_timer();

#if GBC_CPU_DEBUG && !defined(__EMSCRIPTEN__)
    void dump_registers();
    void dump_info();
#endif
};
}  // namespace GBC