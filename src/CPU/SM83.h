#pragma once 

#include "../MMU/bus.h"
#include "timer.h"


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
        SM83(address_bus *memory) : memory(memory) { memset(r8, 0, 8); }

        address_bus *memory;

        byte r8[8];

        uint32_t divcounter = 0, timacounter = 0, timareg, tacreg; // TODO probably should maybe make these have underscores
        uint16_t pc = 0, sp = 0xFFFE;
        uint16_t opcode = 0;
        uint8_t cycles = 0;
        bool IME = false, IMEdelay = false, halted = false, stathigh = false;
        uint8_t cached_interrupt_flags = 0;

        void execute();
        inline void executeCB();

        //===========================================================
        // Flag accessors – F is stored in r8[6]
        //===========================================================
        [[gnu::always_inline]] inline bool getZeroFlag() const       { return (r8[6] >> 7) & 1; }
        [[gnu::always_inline]] inline bool getNFlag() const          { return (r8[6] >> 6) & 1; }
        [[gnu::always_inline]] inline bool getHalfCarryFlag() const  { return (r8[6] >> 5) & 1; }
        [[gnu::always_inline]] inline bool getCarryFlag() const      { return (r8[6] >> 4) & 1; }

        [[gnu::always_inline]] inline void setZeroFlag(bool val)     { r8[6] = (r8[6] & ~(1 << 7)) | ((val ? 1 : 0) << 7); }
        [[gnu::always_inline]] inline void setNFlag(bool val)        { r8[6] = (r8[6] & ~(1 << 6)) | ((val ? 1 : 0) << 6); }
        [[gnu::always_inline]] inline void setHalfCarryFlag(bool val){ r8[6] = (r8[6] & ~(1 << 5)) | ((val ? 1 : 0) << 5); }
        [[gnu::always_inline]] inline void setCarryFlag(bool val)    { r8[6] = (r8[6] & ~(1 << 4)) | ((val ? 1 : 0) << 4); }

        //===========================================================
        // Register-pair accessors
        //===========================================================
        [[gnu::always_inline]] inline uint16_t getAF() const { return (r8[7] << 8) | r8[6]; }
        [[gnu::always_inline]] inline uint16_t getBC() const { return (r8[0] << 8) | r8[1]; }
        [[gnu::always_inline]] inline uint16_t getDE() const { return (r8[2] << 8) | r8[3]; }
        [[gnu::always_inline]] inline uint16_t getHL() const { return (r8[4] << 8) | r8[5]; }
        [[gnu::always_inline]] inline void setAF(uint16_t val) { r8[7] = val >> 8; r8[6] = val & 0xF0; }
        [[gnu::always_inline]] inline void setBC(uint16_t val) { r8[0] = val >> 8; r8[1] = val & 0xFF; }
        [[gnu::always_inline]] inline void setDE(uint16_t val) { r8[2] = val >> 8; r8[3] = val & 0xFF; }
        [[gnu::always_inline]] inline void setHL(uint16_t val) { r8[4] = val >> 8; r8[5] = val & 0xFF; }

        //===========================================================
        // Fetch functions
        //===========================================================
        [[gnu::always_inline]] inline uint8_t fetch8() {
            return memory->read(pc++);
        }
        [[gnu::always_inline]] inline uint16_t fetch16() {
            uint16_t result = memory->read(pc) | (memory->read(pc + 1) << 8);
            pc += 2;
            return result;
        }

        //===========================================================
        // Helper functions for flag calculation
        //===========================================================
        inline bool halfCarryAdd(uint8_t a, uint8_t b) {
            return ((a & 0xF) + (b & 0xF)) > 0xF;
        }
        inline bool halfCarryAdd_WithCarry(uint8_t a, uint8_t b) {
            return ((a & 0xF) + (b & 0xF) + getCarryFlag()) > 0xF;
        }
        inline bool halfCarrySub(uint8_t a, uint8_t b) {
            return (a & 0xF) < (b & 0xF);
        }
        inline bool halfCarrySub_WithCarry(uint8_t a, uint8_t b) {
            return (a & 0xF) < ((b & 0xF)+getCarryFlag());
        }

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
    };
}