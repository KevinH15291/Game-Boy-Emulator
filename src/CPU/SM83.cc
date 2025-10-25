#include "SM83.h"

#include <fstream>

#include "bus.h"
#include "cycles.h"

namespace GBC {
void SM83::execute_cycle() {
    increment_timer();
    if ((memory->read(addr(IORegister::IF)) &
         memory->read(addr(MemoryRegion::IE))) != 0) {
        halted = false;
    }
    if (halted) {
        return;
    }

    if (cycles > 0) {
        --cycles;
        return;
    }
    if (pc > 0xFF && memory->booting) {
        memory->booting = false;
    }
    if (IME && (memory->read(addr(IORegister::IF)) &
                memory->read(addr(MemoryRegion::IE)))) {
        if ((memory->read(addr(IORegister::IF)) & 1) &&
            (memory->read(addr(MemoryRegion::IE)) & 1)) {
            IME = false;

            memory->write(addr(IORegister::IF),
                          (memory->read(addr(IORegister::IF)) & 1) & ~(1));
            call_interrupt(0x40);
            return;
        }
        if ((memory->read(addr(IORegister::IF)) & (1 << 1)) &&
            (memory->read(addr(MemoryRegion::IE)) & (1 << 1))) {
            IME = false;

            memory->write(addr(IORegister::IF),
                          (memory->read(addr(IORegister::IF)) & 1) & ~(1 << 1));
            call_interrupt(0x48);
            return;
        }
        if ((memory->read(addr(IORegister::IF)) & (1 << 2)) &&
            (memory->read(addr(MemoryRegion::IE)) & (1 << 2))) {
            IME = false;

            memory->write(addr(IORegister::IF),
                          (memory->read(addr(IORegister::IF)) & 1) & ~(1 << 2));
            call_interrupt(0x50);
            return;
        }
        if ((memory->read(addr(IORegister::IF)) & (1 << 3)) &&
            (memory->read(addr(MemoryRegion::IE)) & (1 << 3))) {
            IME = false;

            memory->write(addr(IORegister::IF),
                          (memory->read(addr(IORegister::IF)) & 1) & ~(1 << 3));
            call_interrupt(0x58);
            return;
        }
        if ((memory->read(addr(IORegister::IF)) & (1 << 4)) &&
            (memory->read(addr(MemoryRegion::IE)) & (1 << 4))) {
            IME = false;

            memory->write(addr(IORegister::IF),
                          (memory->read(addr(IORegister::IF)) & 1) & ~(1 << 4));
            call_interrupt(0x60);
            return;
        }
    }

    if (IMEdelay) {
        IME = true;
        IMEdelay = false;
    }

    opcode = fetch8();
    cycles += opcode_cycles[opcode];

    const auto instruction = static_cast<Instruction>(opcode);

    switch (instruction) {
        // ----- Block 0: 0x00 - 0x3F -----
        case Instruction::NOP:  // NOP
            return;
        case Instruction::LD_BC_d16:  // LD BC, d16
            instrLdR16_Imm16(0);
            return;
        case Instruction::LD_mBC_A:  // LD (BC), A
            instrLdR16Mem_A(0);
            return;
        case Instruction::INC_BC:  // INC BC
            instrIncR16(0);
            return;
        case Instruction::INC_B:  // INC B
            instrIncR8(0);
            return;
        case Instruction::DEC_B:  // DEC B
            instrDecR8(0);
            return;
        case Instruction::LD_B_d8:  // LD B, d8
            instrLdR8_Imm8(0);
            return;
        case Instruction::RLCA:  // RLCA
            instrRLCA();
            return;
        case Instruction::LD_mImm16_SP:  // LD [d16], SP
            instrLdImm16SP();
            return;
        case Instruction::ADD_HL_BC:  // ADD HL, BC
            instrAddHL_R16(0);
            return;
        case Instruction::LD_A_mBC:  // LD A, (BC)
            instrLdA_R16Mem(0);
            return;
        case Instruction::DEC_BC:  // DEC BC
            instrDecR16(0);
            return;
        case Instruction::INC_C:  // INC C
            instrIncR8(1);
            return;
        case Instruction::DEC_C:  // DEC C
            instrDecR8(1);
            return;
        case Instruction::LD_C_d8:  // LD C, d8
            instrLdR8_Imm8(1);
            return;
        case Instruction::RRCA:  // RRCA
            instrRRCA();
            return;
        case Instruction::STOP:  // STOP
            instrSTOP();
            return;
        case Instruction::LD_DE_d16:  // LD DE, d16
            instrLdR16_Imm16(1);
            return;
        case Instruction::LD_mDE_A:  // LD (DE), A
            instrLdR16Mem_A(1);
            return;
        case Instruction::INC_DE:  // INC DE
            instrIncR16(1);
            return;
        case Instruction::INC_D:  // INC D
            instrIncR8(2);
            return;
        case Instruction::DEC_D:  // DEC D
            instrDecR8(2);
            return;
        case Instruction::LD_D_d8:  // LD D, d8
            instrLdR8_Imm8(2);
            return;
        case Instruction::RLA:  // RLA
            instrRLA();
            return;
        case Instruction::JR_d8:  // JR d8
            instrJR_Imm8();
            return;
        case Instruction::ADD_HL_DE:  // ADD HL, DE
            instrAddHL_R16(1);
            return;
        case Instruction::LD_A_mDE:  // LD A, (DE)
            instrLdA_R16Mem(1);
            return;
        case Instruction::DEC_DE:  // DEC DE
            instrDecR16(1);
            return;
        case Instruction::INC_E:  // INC E
            instrIncR8(3);
            return;
        case Instruction::DEC_E:  // DEC E
            instrDecR8(3);
            return;
        case Instruction::LD_E_d8:  // LD E, d8
            instrLdR8_Imm8(3);
            return;
        case Instruction::RRA:  // RRA
            instrRRA();
            return;
        case Instruction::JR_NZ_d8:  // JR NZ, d8
            instrJR_Cond_Imm8(!getZeroFlag());
            return;
        case Instruction::LD_HL_d16:  // LD HL, d16
            instrLdR16_Imm16(2);
            return;
        case Instruction::LDI_mHL_A:  // LDI (HL), A
            instrLdiHL_A();
            return;
        case Instruction::INC_HL:  // INC HL
            instrIncR16(2);
            return;
        case Instruction::INC_H:  // INC H
            instrIncR8(4);
            return;
        case Instruction::DEC_H:  // DEC H
            instrDecR8(4);
            return;
        case Instruction::LD_H_d8:  // LD H, d8
            instrLdR8_Imm8(4);
            return;
        case Instruction::DAA:  // DAA
            instrDAA();
            return;
        case Instruction::JR_Z_d8:  // JR Z, d8
            instrJR_Cond_Imm8(getZeroFlag());
            return;
        case Instruction::ADD_HL_HL:  // ADD HL, HL
            instrAddHL_R16(2);
            return;
        case Instruction::LDI_A_mHL:  // LD A, (HL)
            instrLdA_HL();
            return;
        case Instruction::DEC_HL:  // DEC HL
            instrDecR16(2);
            return;
        case Instruction::INC_L:  // INC L
            instrIncR8(5);
            return;
        case Instruction::DEC_L:  // DEC L
            instrDecR8(5);
            return;
        case Instruction::LD_L_d8:  // LD L, d8
            instrLdR8_Imm8(5);
            return;
        case Instruction::CPL:  // CPL
            instrCPL();
            return;
        case Instruction::JR_NC_d8:  // JR NC, d8
            instrJR_Cond_Imm8(!getCarryFlag());
            return;
        case Instruction::LD_SP_d16:  // LD SP, d16
            instrLdR16_Imm16(3);
            return;
        case Instruction::LDD_mHL_A:  // LDD (HL), A
            instrLddHL_A();
            return;
        case Instruction::INC_SP:  // INC SP
            instrIncSP();
            return;
        case Instruction::INC_mHL:  // INC (HL)
            instrIncHL();
            return;
        case Instruction::DEC_mHL:  // DEC (HL)
            instrDecHL();
            return;
        case Instruction::LD_mHL_d8:  // LD (HL), d8
            instrLdImm8_HL();
            return;
        case Instruction::SCF:  // SCF
            instrSCF();
            return;
        case Instruction::JR_C_d8:  // JR C, d8
            instrJR_Cond_Imm8(getCarryFlag());
            return;
        case Instruction::ADD_HL_SP:  // ADD HL, SP
            instrAddHL_R16(3);
            return;
        case Instruction::LDD_A_mHL:  // LD A, (HL-)
            instrLdA_HLDec();
            return;
        case Instruction::DEC_SP:  // DEC SP
            --sp;
            return;
        case Instruction::INC_A:  // INC A
            instrIncR8(7);
            return;
        case Instruction::DEC_A:  // DEC A
            instrDecR8(7);
            return;
        case Instruction::LD_A_d8:  // LD A, d8
            instrLdR8_Imm8(7);
            return;
        case Instruction::CCF:  // CCF
            instrCCF();
            return;

        // ----- Block 1: 0x40 - 0x7F -----
        // LD r8, r8 instructions
        case Instruction::LD_B_B:  // LD B, B
            instrLdR8_R8(0, 0);
            return;
        case Instruction::LD_B_C:  // LD B, C
            instrLdR8_R8(0, 1);
            return;
        case Instruction::LD_B_D:  // LD B, D
            instrLdR8_R8(0, 2);
            return;
        case Instruction::LD_B_E:  // LD B, E
            instrLdR8_R8(0, 3);
            return;
        case Instruction::LD_B_H:  // LD B, H
            instrLdR8_R8(0, 4);
            return;
        case Instruction::LD_B_L:  // LD B, L
            instrLdR8_R8(0, 5);
            return;
        case Instruction::LD_B_mHL:  // LD B, (HL)
            instrLdR8_FromHL(0);
            return;
        case Instruction::LD_B_A:  // LD B, A
            instrLdR8_R8(0, 7);
            return;
        case Instruction::LD_C_B:  // LD C, B
            instrLdR8_R8(1, 0);
            return;
        case Instruction::LD_C_C:  // LD C, C
            instrLdR8_R8(1, 1);
            return;
        case Instruction::LD_C_D:  // LD C, D
            instrLdR8_R8(1, 2);
            return;
        case Instruction::LD_C_E:  // LD C, E
            instrLdR8_R8(1, 3);
            return;
        case Instruction::LD_C_H:  // LD C, H
            instrLdR8_R8(1, 4);
            return;
        case Instruction::LD_C_L:  // LD C, L
            instrLdR8_R8(1, 5);
            return;
        case Instruction::LD_C_mHL:  // LD C, (HL)
            instrLdR8_FromHL(1);
            return;
        case Instruction::LD_C_A:  // LD C, A
            instrLdR8_R8(1, 7);
            return;
        case Instruction::LD_D_B:  // LD D, B
            instrLdR8_R8(2, 0);
            return;
        case Instruction::LD_D_C:  // LD D, C
            instrLdR8_R8(2, 1);
            return;
        case Instruction::LD_D_D:  // LD D, D
            instrLdR8_R8(2, 2);
            return;
        case Instruction::LD_D_E:  // LD D, E
            instrLdR8_R8(2, 3);
            return;
        case Instruction::LD_D_H:  // LD D, H
            instrLdR8_R8(2, 4);
            return;
        case Instruction::LD_D_L:  // LD D, L
            instrLdR8_R8(2, 5);
            return;
        case Instruction::LD_D_mHL:  // LD D, (HL)
            instrLdR8_FromHL(2);
            return;
        case Instruction::LD_D_A:  // LD D, A
            instrLdR8_R8(2, 7);
            return;
        case Instruction::LD_E_B:  // LD E, B
            instrLdR8_R8(3, 0);
            return;
        case Instruction::LD_E_C:  // LD E, C
            instrLdR8_R8(3, 1);
            return;
        case Instruction::LD_E_D:  // LD E, D
            instrLdR8_R8(3, 2);
            return;
        case Instruction::LD_E_E:  // LD E, E
            instrLdR8_R8(3, 3);
            return;
        case Instruction::LD_E_H:  // LD E, H
            instrLdR8_R8(3, 4);
            return;
        case Instruction::LD_E_L:  // LD E, L
            instrLdR8_R8(3, 5);
            return;
        case Instruction::LD_E_mHL:  // LD E, (HL)
            instrLdR8_FromHL(3);
            return;
        case Instruction::LD_E_A:  // LD E, A
            instrLdR8_R8(3, 7);
            return;
        case Instruction::LD_H_B:  // LD H, B
            instrLdR8_R8(4, 0);
            return;
        case Instruction::LD_H_C:  // LD H, C
            instrLdR8_R8(4, 1);
            return;
        case Instruction::LD_H_D:  // LD H, D
            instrLdR8_R8(4, 2);
            return;
        case Instruction::LD_H_E:  // LD H, E
            instrLdR8_R8(4, 3);
            return;
        case Instruction::LD_H_H:  // LD H, H
            instrLdR8_R8(4, 4);
            return;
        case Instruction::LD_H_L:  // LD H, L
            instrLdR8_R8(4, 5);
            return;
        case Instruction::LD_H_mHL:  // LD H, (HL)
            instrLdR8_FromHL(4);
            return;
        case Instruction::LD_H_A:  // LD H, A
            instrLdR8_R8(4, 7);
            return;
        case Instruction::LD_L_B:  // LD L, B
            instrLdR8_R8(5, 0);
            return;
        case Instruction::LD_L_C:  // LD L, C
            instrLdR8_R8(5, 1);
            return;
        case Instruction::LD_L_D:  // LD L, D
            instrLdR8_R8(5, 2);
            return;
        case Instruction::LD_L_E:  // LD L, E
            instrLdR8_R8(5, 3);
            return;
        case Instruction::LD_L_H:  // LD L, H
            instrLdR8_R8(5, 4);
            return;
        case Instruction::LD_L_L:  // LD L, L
            instrLdR8_R8(5, 5);
            return;
        case Instruction::LD_L_mHL:  // LD L, (HL)
            instrLdR8_FromHL(5);
            return;
        case Instruction::LD_L_A:  // LD L, A
            instrLdR8_R8(5, 7);
            return;
        case Instruction::LD_mHL_B:  // LD (HL), B
            instrLdHL_FromR8(0);
            return;
        case Instruction::LD_mHL_C:  // LD (HL), C
            instrLdHL_FromR8(1);
            return;
        case Instruction::LD_mHL_D:  // LD (HL), D
            instrLdHL_FromR8(2);
            return;
        case Instruction::LD_mHL_E:  // LD (HL), E
            instrLdHL_FromR8(3);
            return;
        case Instruction::LD_mHL_H:  // LD (HL), H
            instrLdHL_FromR8(4);
            return;
        case Instruction::LD_mHL_L:  // LD (HL), L
            instrLdHL_FromR8(5);
            return;
        case Instruction::HALT:  // HALT
            instrHALT();
            return;
        case Instruction::LD_mHL_A:  // LD (HL), A
            instrLdHL_FromR8(7);
            return;
        case Instruction::LD_A_B:  // LD A, B
            instrLdR8_R8(7, 0);
            return;
        case Instruction::LD_A_C:  // LD A, C
            instrLdR8_R8(7, 1);
            return;
        case Instruction::LD_A_D:  // LD A, D
            instrLdR8_R8(7, 2);
            return;
        case Instruction::LD_A_E:  // LD A, E
            instrLdR8_R8(7, 3);
            return;
        case Instruction::LD_A_H:  // LD A, H
            instrLdR8_R8(7, 4);
            return;
        case Instruction::LD_A_L:  // LD A, L
            instrLdR8_R8(7, 5);
            return;
        case Instruction::LD_A_mHL:  // LD A, (HL)
            instrLdR8_FromHL(7);
            return;
        case Instruction::LD_A_A:  // LD A, A
            instrLdR8_R8(7, 7);
            return;

        // ----- Block 2: 0x80 - 0xBF (ALU operations on A with a register)
        // -----
        case Instruction::ADD_A_B:  // ADD A, B
            instrAddA_R8(0);
            return;
        case Instruction::ADD_A_C:  // ADD A, C
            instrAddA_R8(1);
            return;
        case Instruction::ADD_A_D:  // ADD A, D
            instrAddA_R8(2);
            return;
        case Instruction::ADD_A_E:  // ADD A, E
            instrAddA_R8(3);
            return;
        case Instruction::ADD_A_H:  // ADD A, H
            instrAddA_R8(4);
            return;
        case Instruction::ADD_A_L:  // ADD A, L
            instrAddA_R8(5);
            return;
        case Instruction::ADD_A_mHL:  // ADD A, (HL)
            instrAddA_R8(6);
            return;
        case Instruction::ADD_A_A:  // ADD A, A
            instrAddA_R8(7);
            return;
        case Instruction::ADC_A_B:  // ADC A, B
            instrAdcA_R8(0);
            return;
        case Instruction::ADC_A_C:  // ADC A, C
            instrAdcA_R8(1);
            return;
        case Instruction::ADC_A_D:  // ADC A, D
            instrAdcA_R8(2);
            return;
        case Instruction::ADC_A_E:  // ADC A, E
            instrAdcA_R8(3);
            return;
        case Instruction::ADC_A_H:  // ADC A, H
            instrAdcA_R8(4);
            return;
        case Instruction::ADC_A_L:  // ADC A, L
            instrAdcA_R8(5);
            return;
        case Instruction::ADC_A_mHL:  // ADC A, (HL)
            instrAdcA_R8(6);
            return;
        case Instruction::ADC_A_A:  // ADC A, A
            instrAdcA_R8(7);
            return;
        case Instruction::SUB_B:  // SUB B
            instrSubA_R8(0);
            return;
        case Instruction::SUB_C:  // SUB C
            instrSubA_R8(1);
            return;
        case Instruction::SUB_D:  // SUB D
            instrSubA_R8(2);
            return;
        case Instruction::SUB_E:  // SUB E
            instrSubA_R8(3);
            return;
        case Instruction::SUB_H:  // SUB H
            instrSubA_R8(4);
            return;
        case Instruction::SUB_L:  // SUB L
            instrSubA_R8(5);
            return;
        case Instruction::SUB_mHL:  // SUB (HL)
            instrSubA_R8(6);
            return;
        case Instruction::SUB_A:  // SUB A
            instrSubA_R8(7);
            return;
        case Instruction::SBC_A_B:  // SBC A, B
            instrSbcA_R8(0);
            return;
        case Instruction::SBC_A_C:  // SBC A, C
            instrSbcA_R8(1);
            return;
        case Instruction::SBC_A_D:  // SBC A, D
            instrSbcA_R8(2);
            return;
        case Instruction::SBC_A_E:  // SBC A, E
            instrSbcA_R8(3);
            return;
        case Instruction::SBC_A_H:  // SBC A, H
            instrSbcA_R8(4);
            return;
        case Instruction::SBC_A_L:  // SBC A, L
            instrSbcA_R8(5);
            return;
        case Instruction::SBC_A_mHL:  // SBC A, (HL)
            instrSbcA_R8(6);
            return;
        case Instruction::SBC_A_A:  // SBC A, A
            instrSbcA_R8(7);
            return;
        case Instruction::AND_B:  // AND B
            instrAndA_R8(0);
            return;
        case Instruction::AND_C:  // AND C
            instrAndA_R8(1);
            return;
        case Instruction::AND_D:  // AND D
            instrAndA_R8(2);
            return;
        case Instruction::AND_E:  // AND E
            instrAndA_R8(3);
            return;
        case Instruction::AND_H:  // AND H
            instrAndA_R8(4);
            return;
        case Instruction::AND_L:  // AND L
            instrAndA_R8(5);
            return;
        case Instruction::AND_mHL:  // AND (HL)
            instrAndA_R8(6);
            return;
        case Instruction::AND_A:  // AND A
            instrAndA_R8(7);
            return;
        case Instruction::XOR_B:  // XOR B
            instrXorA_R8(0);
            return;
        case Instruction::XOR_C:  // XOR C
            instrXorA_R8(1);
            return;
        case Instruction::XOR_D:  // XOR D
            instrXorA_R8(2);
            return;
        case Instruction::XOR_E:  // XOR E
            instrXorA_R8(3);
            return;
        case Instruction::XOR_H:  // XOR H
            instrXorA_R8(4);
            return;
        case Instruction::XOR_L:  // XOR L
            instrXorA_R8(5);
            return;
        case Instruction::XOR_mHL:  // XOR (HL)
            instrXorA_R8(6);
            return;
        case Instruction::XOR_A:  // XOR A
            instrXorA_R8(7);
            return;
        case Instruction::OR_B:  // OR B
            instrOrA_R8(0);
            return;
        case Instruction::OR_C:  // OR C
            instrOrA_R8(1);
            return;
        case Instruction::OR_D:  // OR D
            instrOrA_R8(2);
            return;
        case Instruction::OR_E:  // OR E
            instrOrA_R8(3);
            return;
        case Instruction::OR_H:  // OR H
            instrOrA_R8(4);
            return;
        case Instruction::OR_L:  // OR L
            instrOrA_R8(5);
            return;
        case Instruction::OR_mHL:  // OR (HL)
            instrOrA_R8(6);
            return;
        case Instruction::OR_A:  // OR A
            instrOrA_R8(7);
            return;
        case Instruction::CP_B:  // CP B
            instrCpA_R8(0);
            return;
        case Instruction::CP_C:  // CP C
            instrCpA_R8(1);
            return;
        case Instruction::CP_D:  // CP D
            instrCpA_R8(2);
            return;
        case Instruction::CP_E:  // CP E
            instrCpA_R8(3);
            return;
        case Instruction::CP_H:  // CP H
            instrCpA_R8(4);
            return;
        case Instruction::CP_L:  // CP L
            instrCpA_R8(5);
            return;
        case Instruction::CP_mHL:  // CP (HL)
            instrCpA_R8(6);
            return;
        case Instruction::CP_A:  // CP A
            instrCpA_R8(7);
            return;

            // ----- Block 3: 0xC0 - 0xFF (Control flow, immediate ALU ops, and
            // miscellaneous) -----

        case Instruction::RET_NZ:  // RET NZ
            instrRET_Cond(!getZeroFlag());
            return;
        case Instruction::POP_BC:  // POP BC
            instrPOP_R16(0);
            return;
        case Instruction::JP_NZ_d16:  // JP NZ, d16
            instrJP_Cond_Imm16(!getZeroFlag());
            return;
        case Instruction::JP_d16:  // JP d16
            instrJP_Imm16();
            return;
        case Instruction::CALL_NZ_d16:  // CALL NZ, d16
            instrCALL_Cond_Imm16(!getZeroFlag());
            return;
        case Instruction::PUSH_BC:  // PUSH BC
            instrPUSH_R16(0);
            return;
        case Instruction::ADD_A_d8:  // ADD A, d8
            instrAddA_Imm8();
            return;
        case Instruction::RST_00H:  // RST 0x00
            instrRST(0x00);
            return;
        case Instruction::RET_Z:  // RET Z
            instrRET_Cond(getZeroFlag());
            return;
        case Instruction::RET:  // RET
            instrRET();
            return;
        case Instruction::JP_Z_d16:  // JP Z, d16
            instrJP_Cond_Imm16(getZeroFlag());
            return;
        case Instruction::PREFIX_CB:  // CB-prefixed opcodes
            executeCB();
            return;
        case Instruction::CALL_Z_d16:  // CALL Z, d16
            instrCALL_Cond_Imm16(getZeroFlag());
            return;
        case Instruction::CALL_d16:  // CALL d16
            instrCALL_Imm16();
            return;
        case Instruction::ADC_A_d8:  // ADC A, d8
            instrAdcA_Imm8();
            return;
        case Instruction::RST_08H:  // RST 0x08
            instrRST(0x08);
            return;
        case Instruction::RET_NC:  // RET NC
            instrRET_Cond(!getCarryFlag());
            return;
        case Instruction::POP_DE:  // POP DE
            instrPOP_R16(1);
            return;
        case Instruction::JP_NC_d16:  // JP NC, d16
            instrJP_Cond_Imm16(!getCarryFlag());
            return;
        case Instruction::ILLEGAL_D3:
            throw std::runtime_error("Opcode D3 not used");
            return;
        case Instruction::CALL_NC_d16:  // CALL NC, d16
            instrCALL_Cond_Imm16(!getCarryFlag());
            return;
        case Instruction::PUSH_DE:  // PUSH DE
            instrPUSH_R16(1);
            return;
        case Instruction::SUB_d8:  // SUB A, d8
            instrSubA_Imm8();
            setNFlag(true);
            return;
        case Instruction::RST_10H:  // RST 0x10
            instrRST(0x10);
            return;
        case Instruction::RET_C:  // RET C
            instrRET_Cond(getCarryFlag());
            return;
        case Instruction::RETI:  // RETI
            instrRETI();
            return;
        case Instruction::JP_C_d16:  // JP C, d16
            instrJP_Cond_Imm16(getCarryFlag());
            return;
        case Instruction::ILLEGAL_DB:
            throw std::runtime_error("Opcode DB not used");
            return;
        case Instruction::CALL_C_d16:  // CALL C, d16
            instrCALL_Cond_Imm16(getCarryFlag());
            return;
        case Instruction::ILLEGAL_DD:
            throw std::runtime_error("Opcode DD not used");
            return;
        case Instruction::SBC_A_d8:  // SBC A, d8
            instrSbcA_Imm8();
            return;
        case Instruction::RST_18H:  // RST 0x18
            instrRST(0x18);
            return;
        case Instruction::LDH_mImm8_A:  // LDH (n), A
            instrLDH_Imm8_A();
            return;
        case Instruction::POP_HL:  // POP HL
            instrPOP_R16(2);
            return;
        case Instruction::LDH_mC_A:  // LDH (C), A
            instrLDH_C_A();
            return;
        case Instruction::ILLEGAL_E3:
            throw std::runtime_error("Opcode E3 not used");
            return;
        case Instruction::ILLEGAL_E4:
            throw std::runtime_error("Opcode E4 not used");
            return;
        case Instruction::PUSH_HL:  // PUSH HL
            instrPUSH_R16(2);
            return;
        case Instruction::AND_d8:  // AND A, d8
            instrAndA_Imm8();
            return;
        case Instruction::RST_20H:  // RST 0x20
            instrRST(0x20);
            return;
        case Instruction::ADD_SP_d8:  // ADD SP, d8
            instrAddSP_Imm8();
            return;
        case Instruction::JP_mHL:  // JP (HL)
            instrJP_HL();
            return;
        case Instruction::LD_mImm16_A:  // LD (a16), A
            instrLD_Imm16_A();
            return;
        case Instruction::ILLEGAL_EB:
            throw std::runtime_error("Opcode EB not used");
            return;
        case Instruction::ILLEGAL_EC:
            throw std::runtime_error("Opcode EC not used");
            return;
        case Instruction::ILLEGAL_ED:
            throw std::runtime_error("Opcode ED not used");
            return;
        case Instruction::XOR_d8:  // XOR A, d8
            instrXorA_Imm8();
            return;
        case Instruction::RST_28H:  // RST 0x28
            instrRST(0x28);
            return;
        case Instruction::LDH_A_mImm8:  // LDH A, (n)
            instrLDH_A_Imm8();
            return;
        case Instruction::POP_AF:  // POP AF
            instrPOP_R16(3);
            return;
        case Instruction::LDH_A_mC:  // LDH A, (C)
            instrLDH_A_C();
            return;
        case Instruction::DI:  // DI
            instrDI();
            return;
        case Instruction::ILLEGAL_F4:
            throw std::runtime_error("Opcode F4 not used");
            return;
        case Instruction::PUSH_AF:  // PUSH AF
            instrPUSH_R16(3);
            return;
        case Instruction::OR_d8:  // OR A, d8
            instrOrA_Imm8();
            return;
        case Instruction::RST_30H:  // RST 0x30
            instrRST(0x30);
            return;
        case Instruction::LD_HL_SP_d8:  // LD HL, SP+d8
            instrLD_HL_SP_Imm8();
            return;
        case Instruction::LD_SP_HL:  // LD SP, HL
            instrLD_SP_HL();
            return;
        case Instruction::LD_A_mImm16:  // LD A, (a16)
            instrLD_A_Imm16();
            return;
        case Instruction::EI:  // EI
            instrEI();
            return;
        case Instruction::ILLEGAL_FC:
            throw std::runtime_error("Opcode FC not used");
            return;
        case Instruction::ILLEGAL_FD:
            throw std::runtime_error("Opcode FD not used");
            return;
        case Instruction::CP_d8:  // CP A, d8
            instrCpA_Imm8();
            return;
        case Instruction::RST_38H:  // RST 0x38
            instrRST(0x38);
            return;
        default:
            throw std::runtime_error("Invalid opcode: " +
                                     std::to_string(static_cast<int>(opcode)));
    }
}
inline void SM83::executeCB() {
    // Fetch the next byte which is the actual CB opcode.
    uint8_t cbOpcode = fetch8();
    cycles += opcode_cycles_cb[cbOpcode];
    opcode = (opcode << 8) + cbOpcode;
    // Decode based on the top two bits.
    switch (cbOpcode >> 6) {
        case 0:  // Rotate and swap instructions.
            switch ((cbOpcode >> 3) & 0x07) {
                case 0:
                    instrRLC_R8(cbOpcode & 0x07);
                    break;
                case 1:
                    instrRRC_R8(cbOpcode & 0x07);
                    break;
                case 2:
                    instrRL_R8(cbOpcode & 0x07);
                    break;
                case 3:
                    instrRR_R8(cbOpcode & 0x07);
                    break;
                case 4:
                    instrSLA_R8(cbOpcode & 0x07);
                    break;
                case 5:
                    instrSRA_R8(cbOpcode & 0x07);
                    break;
                case 6:
                    instrSWAP_R8(cbOpcode & 0x07);
                    break;
                case 7:
                    instrSRL_R8(cbOpcode & 0x07);
                    break;
                default:
                    throw std::runtime_error("Invalid CB rotation opcode");
            }
            break;
        case 1:  // BIT b, r
            instrBIT_R8((cbOpcode >> 3) & 0x07, cbOpcode & 0x07);
            break;
        case 2:  // RES b, r
            instrRES_R8((cbOpcode >> 3) & 0x07, cbOpcode & 0x07);
            break;
        case 3:  // SET b, r
            instrSET_R8((cbOpcode >> 3) & 0x07, cbOpcode & 0x07);
            break;
        default:
            throw std::runtime_error("Invalid CB opcode group");
    }
}

inline void SM83::instrNOP() {
    // Do nothing.
}

inline void SM83::instrLdImm16SP() {
    half addr = fetch16();
    memory->write(addr, static_cast<uint8_t>(sp));
    memory->write(addr + 1, static_cast<uint8_t>(sp >> 8));
}

inline void SM83::instrRLCA() {
    half temp = RA;
    RA = ((RA & 0b10000000) >> 7) | (RA << 1);
    setHalfCarryFlag(false);
    setCarryFlag(RA & 1);
    setNFlag(false);
    setZeroFlag((RA == 0) && (temp != 0));
}

inline void SM83::instrRRCA() {
    half temp = RA;
    setCarryFlag(RA & 1);
    setHalfCarryFlag(false);
    setNFlag(false);
    setZeroFlag((RA == 0) && (temp != 0));
    RA = ((RA & 0b1) << 7) | (RA >> 1);
}

inline void SM83::instrRLA() {
    half temp = getCarryFlag();
    temp |= (RA << 8);
    setCarryFlag(RA >> 7);
    RA <<= 1;
    RA |= (temp & 1);
    setHalfCarryFlag(false);
    setNFlag(false);
    setZeroFlag((RA == 0) && ((temp >> 8) != 0));
}

inline void SM83::instrRRA() {
    uint16_t a = (RA & 1);
    a |= (RA << 8);
    RA >>= 1;
    RA |= ((getCarryFlag() & 1) << 7);
    setHalfCarryFlag(false);
    setNFlag(false);
    setCarryFlag((a & 1) != 0);
    setZeroFlag((RA == 0) && ((a >> 8) != 0) && (getCarryFlag() == 0));
}

inline void SM83::instrLdiHL_A() {
    memory->write(getHL(), RA);
    setHL(getHL() + 1);
}

inline void SM83::instrDAA() {
    uint8_t a = RA;

    if (!getNFlag()) {
        if (getCarryFlag() || a > 0x99) {
            a += 0x60;
            setCarryFlag(true);
        }
        if (getHalfCarryFlag() || (a & 0x0F) > 0x09) a += 0x06;
    } else {
        if (getCarryFlag()) a -= 0x60;
        if (getHalfCarryFlag()) a -= 0x06;
    }

    setHalfCarryFlag(false);
    setZeroFlag((RA == 0));
}

inline void SM83::instrLdA_HL() {
    RA = memory->read(getHL());
    setHL(getHL() + 1);
}

inline void SM83::instrCPL() {
    RA = ~RA;
    setHalfCarryFlag(true);
    setNFlag(true);
}

inline void SM83::instrLddHL_A() {
    // Store A at address HL, then decrement HL.
    memory->write(getHL(), RA);
    setHL(getHL() - 1);
}

inline void SM83::instrIncSP() { sp++; }

inline void SM83::instrSCF() {
    setCarryFlag(true);
    setNFlag(false);
    setHalfCarryFlag(false);
}

inline void SM83::instrLdA_HLDec() {
    RA = memory->read(getHL());
    setHL(getHL() - 1);
}

inline void SM83::instrCCF() {
    setCarryFlag(!getCarryFlag());
    setNFlag(false);
    setHalfCarryFlag(false);
}

inline void SM83::instrJR_Imm8() {
    // Unconditional relative jump.
    // (Assumes that memory is used for instruction memory->)
    pc += static_cast<int8_t>(memory->read(pc)) + 1;
}

inline void SM83::instrSTOP() {
    halted = true;
    divcounter = 0;
    memory->write(addr(IORegister::DIV), 0);
}

// Instead of separate functions for each conditional JR, we use a grouped one:
inline void SM83::instrJR_Cond_Imm8(bool condition) {
    int8_t offset = static_cast<int8_t>(fetch8());
    if (condition) {
        pc += offset;
        ++cycles;
    }
}

// 16-bit register instructions

inline void SM83::instrLdR16_Imm16(uint8_t regIndex) {
    half value = fetch16();
    store16t1(regIndex, value);
}

inline void SM83::instrAddHL_R16(uint8_t regIndex) {
    half value = load16t1(regIndex);
    setCarryFlag(static_cast<uint16_t>(getHL() + value) < getHL());
    setHalfCarryFlag((getHL() & 0xFFF) + (value & 0xFFF) > 0xFFF);
    setHL(getHL() + value);
    setNFlag(false);
}

// 16-bit memory instructions

inline void SM83::instrLdR16Mem_A(uint8_t regIndex) {
    memory->write(load16t1(regIndex), RA);
}

inline void SM83::instrLdA_R16Mem(uint8_t regIndex) {
    RA = memory->read(load16t1(regIndex));
}

inline void SM83::instrIncR16(uint8_t regIndex) {
    half value = load16t1(regIndex);
    store16t1(regIndex, value + 1);
}

inline void SM83::instrDecR16(uint8_t regIndex) {
    half value = load16t1(regIndex);
    store16t1(regIndex, value - 1);
}

// 8-bit increment/decrement instructions

inline void SM83::instrIncR8(uint8_t reg) {
    uint8_t temp = registers[reg];
    registers[reg]++;
    setZeroFlag(registers[reg] == 0);
    setNFlag(false);
    setHalfCarryFlag(halfCarryAdd(temp, 1));
}

inline void SM83::instrIncHL() {
    uint8_t temp = memory->read(getHL());
    memory->write(getHL(), temp + 1);
    setZeroFlag(((temp + 1) & 0xF) == 0);
    setNFlag(false);
    setHalfCarryFlag(halfCarryAdd(temp, 1));
}

inline void SM83::instrDecR8(uint8_t reg) {
    uint8_t temp = registers[reg];
    registers[reg]--;
    setZeroFlag(registers[reg] == 0);
    setNFlag(true);
    setHalfCarryFlag(halfCarrySub(temp, registers[reg]));
}

inline void SM83::instrDecHL() {
    uint8_t temp = memory->read(getHL());
    memory->write(getHL(), temp - 1);
    setZeroFlag(((temp - 1) & 0xF) == 0);
    setNFlag(true);
    setHalfCarryFlag(halfCarrySub(temp, 1));
}

// 8-bit load immediate instructions

inline void SM83::instrLdImm8_R8(uint8_t reg) {
    memory->write(fetch8(), registers[reg]);
}
inline void SM83::instrLdR8_Imm8(uint8_t reg) { registers[reg] = fetch8(); }

inline void SM83::instrLdImm8_HL() { memory->write(getHL(), fetch8()); }

// Block 1: Register-to-register loads

inline void SM83::instrHALT() {
    halted = true;
    memory->write(addr(IORegister::DIV), 0);
    divcounter = 0;
}

inline void SM83::instrLdR8_FromHL(uint8_t reg) {
    registers[reg] = memory->read(getHL());
}

inline void SM83::instrLdHL_FromR8(uint8_t reg) {
    memory->write(getHL(), registers[reg]);
}

inline void SM83::instrLdR8_R8(uint8_t dest, uint8_t src) {
    registers[dest] = registers[src];
}

// Block 2: ALU operations (A with r8)

inline void SM83::instrAddA_R8(uint8_t reg) {
    if (reg == 6) {
        setHalfCarryFlag(halfCarryAdd(RA, memory->read(getHL())));
        setCarryFlag((RA + memory->read(getHL())) > 0xFF);
        RA += memory->read(getHL());
        setZeroFlag(RA == 0);
        setNFlag(false);
        return;
    }

    setHalfCarryFlag(halfCarryAdd(RA, registers[reg]));
    setCarryFlag(RA + registers[reg] > 0xFF);
    RA += registers[reg];
    setZeroFlag(RA == 0);
    setNFlag(false);
}

inline void SM83::instrAdcA_R8(uint8_t reg) {
    uint16_t temp = RA;
    if (reg == 6) {
        setHalfCarryFlag(halfCarryAdd_WithCarry(RA, memory->read(getHL())));
        RA += memory->read(getHL()) + getCarryFlag();
        setCarryFlag((temp + memory->read(getHL()) + getCarryFlag()) > 0xFF);

        setZeroFlag(RA == 0);
        setNFlag(false);
        return;
    }
    setHalfCarryFlag(halfCarryAdd_WithCarry(RA, registers[reg]));
    RA += registers[reg] + getCarryFlag();
    setCarryFlag((temp + getCarryFlag()) > RA);
    setZeroFlag(RA == 0);
    setNFlag(false);
}

inline void SM83::instrSubA_R8(uint8_t reg) {
    uint16_t temp = RA;
    setNFlag(true);
    if (reg == 6) {
        setHalfCarryFlag(halfCarrySub(RA, memory->read(getHL())));
        RA -= memory->read(getHL());
        setCarryFlag(RA > temp);
        setZeroFlag(RA == 0);
        return;
    }
    setHalfCarryFlag(halfCarrySub(RA, registers[reg]));
    RA -= registers[reg];
    setCarryFlag(RA > temp);
    setZeroFlag(RA == 0);
}

inline void SM83::instrSbcA_R8(uint8_t reg) {
    uint16_t temp = RA;
    setNFlag(true);
    if (reg == 6) {
        setHalfCarryFlag(halfCarrySub_WithCarry(RA, memory->read(getHL())));
        RA -= memory->read(getHL()) + getCarryFlag();
        setCarryFlag(RA > temp - getCarryFlag());
        setZeroFlag(RA == 0);
        return;
    }
    setHalfCarryFlag(halfCarrySub_WithCarry(RA, registers[reg]));
    RA -= registers[reg] + getCarryFlag();
    setCarryFlag(RA > temp - getCarryFlag());
    setZeroFlag(RA == 0);
}

inline void SM83::instrAndA_R8(uint8_t reg) {
    setHalfCarryFlag(1);
    setNFlag(0);
    setCarryFlag(0);
    if (reg == 6) {
        RA &= memory->read(getHL());
        setZeroFlag(RA == 0);
        return;
    }
    RA &= registers[reg];
    setZeroFlag(RA == 0);
}

inline void SM83::instrXorA_R8(uint8_t reg) {
    setHalfCarryFlag(0);
    setNFlag(0);
    setCarryFlag(0);
    if (reg == 6) {
        RA ^= memory->read(getHL());
        setZeroFlag(RA == 0);
        return;
    }
    RA ^= registers[reg];
    setZeroFlag(RA == 0);
}

inline void SM83::instrOrA_R8(uint8_t reg) {
    setHalfCarryFlag(0);
    setNFlag(0);
    setCarryFlag(0);
    if (reg == 6) {
        RA |= memory->read(getHL());
        setZeroFlag(RA == 0);
        return;
    }
    RA |= registers[reg];
    setZeroFlag(RA == 0);
}

inline void SM83::instrCpA_R8(uint8_t reg) {
    setNFlag(true);
    if (reg == 6) {
        setCarryFlag(RA < memory->read(getHL()));
        setHalfCarryFlag(halfCarrySub(RA, memory->read(getHL())));
        setZeroFlag(RA == memory->read(getHL()));
        return;
    }
    setCarryFlag(RA < registers[reg]);
    setHalfCarryFlag(halfCarrySub(RA, registers[reg]));
    setZeroFlag(RA == registers[reg]);
}

// Block 3: Immediate ALU and control flow

inline void SM83::instrAddA_Imm8() {
    half temp = fetch8();
    setHalfCarryFlag(halfCarryAdd(RA, temp));
    setCarryFlag(RA + temp > 0xFF);
    RA += temp;
    setZeroFlag(RA == 0);
    setNFlag(false);
}

inline void SM83::instrAdcA_Imm8() {
    half temp = fetch8();
    setHalfCarryFlag(halfCarryAdd_WithCarry(RA, temp));
    RA += temp + getCarryFlag();
    setCarryFlag((temp + getCarryFlag()) > RA);
    setZeroFlag(RA == 0);
    setNFlag(false);
}

inline void SM83::instrSubA_Imm8() {
    half temp = fetch8();
    setNFlag(true);
    setHalfCarryFlag(halfCarrySub(RA, temp));
    setCarryFlag(RA < temp);
    RA -= temp;

    setZeroFlag(RA == 0);
}

inline void SM83::instrSbcA_Imm8() {
    half temp = fetch8();
    half oldRA = RA;
    setNFlag(true);
    setHalfCarryFlag(halfCarrySub_WithCarry(RA, temp));
    RA -= temp + getCarryFlag();
    setCarryFlag(RA > oldRA - getCarryFlag());

    setZeroFlag(RA == 0);
}

inline void SM83::instrAndA_Imm8() {
    setHalfCarryFlag(1);
    setNFlag(0);
    setCarryFlag(0);
    RA &= fetch8();
    setZeroFlag(RA == 0);
}

inline void SM83::instrXorA_Imm8() {
    setHalfCarryFlag(0);
    setNFlag(0);
    setCarryFlag(0);
    RA ^= fetch8();
    setZeroFlag(RA == 0);
}

inline void SM83::instrOrA_Imm8() {
    setHalfCarryFlag(0);
    setNFlag(0);
    setCarryFlag(0);

    RA |= fetch8();
    setZeroFlag(RA == 0);
}

inline void SM83::instrCpA_Imm8() {
    half imm8 = fetch8();
    setNFlag(1);
    setCarryFlag(RA < imm8);
    setHalfCarryFlag(halfCarrySub(RA, imm8));
    setZeroFlag(RA == imm8);
}

inline void SM83::instrRET() {
    pc = memory->read(sp++);
    pc |= (memory->read(sp++) << 8);
}

inline void SM83::instrRETI() {
    pc = memory->read(sp++);
    pc |= (memory->read(sp++) << 8);
    IME = true;
}

inline void SM83::instrJP_Imm16() { pc = fetch16(); }

inline void SM83::instrJP_HL() { pc = getHL(); }

inline void SM83::instrCALL_Imm16() {
    uint16_t temp = fetch16();
    memory->write(--sp, static_cast<uint8_t>(pc >> 8));
    memory->write(--sp, static_cast<uint8_t>(pc));
    pc = temp;
}

inline void SM83::instrLDH_C_A() { memory->write(0xFF00 + RC, RA); }

inline void SM83::instrLDH_Imm8_A() { memory->write(0xFF00 | fetch8(), RA); }

inline void SM83::instrLD_Imm16_A() { memory->write(fetch16(), RA); }

inline void SM83::instrLDH_A_C() { RA = memory->read(0xFF00 | RC); }

inline void SM83::instrLDH_A_Imm8() { RA = memory->read(0xFF00 | fetch8()); }

inline void SM83::instrLD_A_Imm16() { RA = memory->read(fetch16()); }

inline void SM83::instrAddSP_Imm8() {
    setZeroFlag(0);
    setNFlag(0);
    half temp = fetch8();
    setHalfCarryFlag(halfCarryAdd(sp, temp));
    setCarryFlag((sp & 0xFF) + temp > 0xFF);
    sp += (int8_t)temp;
}

inline void SM83::instrLD_HL_SP_Imm8() {
    half temp = fetch8();
    setZeroFlag(0);
    setNFlag(0);
    setHalfCarryFlag(halfCarryAdd(sp, temp));
    setCarryFlag(((sp & 0xFF) + (temp & 0xFF)) > (uint16_t)0xFF);
    setHL((int16_t)sp + int8_t(temp));
}

inline void SM83::instrLD_SP_HL() { sp = getHL(); }

inline void SM83::instrDI() { IME = false; }

inline void SM83::instrEI() { IMEdelay = true; }

// Conditional RET, JP, and CALL (grouped)

inline void SM83::instrRET_Cond(bool condition) {
    if (condition) {
        cycles += 3;
        instrRET();
    }
}

inline void SM83::instrJP_Cond_Imm16(bool condition) {
    uint16_t temp = fetch16();
    if (condition) {
        pc = temp;
        ++cycles;
    }
}

inline void SM83::instrCALL_Cond_Imm16(bool condition) {
    uint16_t temp = fetch16();
    if (condition) {
        memory->write(--sp, static_cast<uint8_t>(pc >> 8));
        memory->write(--sp, static_cast<uint8_t>(pc));
        pc = temp;
        cycles += 3;
    }
}

// RST Instruction
inline void SM83::instrRST(uint8_t target) {
    // throw std::runtime_error(std::string("WHY AM I
    // HERE!!?!!?!").append(std::to_string(pc)));
    memory->write(--sp, static_cast<uint8_t>(pc >> 8));
    memory->write(--sp, static_cast<uint8_t>(pc));
    pc = target;
}

// POP and PUSH instructions for 16-bit registers
inline void SM83::instrPOP_R16(uint8_t regIndex) {
    half value = (memory->read(sp)) | ((half)memory->read(half(sp + 1)) << 8);
    store16t2(regIndex, value);
    sp += 2;
}

inline void SM83::instrPUSH_R16(uint8_t regIndex) {
    memory->write(--sp, static_cast<uint8_t>(load16t2(regIndex) >> 8));
    memory->write(--sp, static_cast<uint8_t>(load16t2(regIndex)));
}

// CB-Prefixed Instructions
inline void SM83::instrRLC_R8(uint8_t reg) {
    setNFlag(0);
    setHalfCarryFlag(0);
    if (reg == 6) {
        setCarryFlag(memory->read(getHL()) & 0x80);
        memory->write(getHL(), (memory->read(getHL()) >> 7) |
                                   (memory->read(getHL()) << 1));
        setZeroFlag((memory->read(getHL()) == 0));
        return;
    }
    registers[reg] = (registers[reg] >> 7) | (registers[reg] << 1);
    setZeroFlag((registers[reg] == 0));
    setCarryFlag(registers[reg] & 0x80);
}

inline void SM83::instrRRC_R8(uint8_t reg) {
    setNFlag(0);
    setHalfCarryFlag(0);
    if (reg == 6) {
        setCarryFlag(memory->read(getHL()) & 1);
        memory->write(getHL(), ((memory->read(getHL()) & 1) << 7) |
                                   (memory->read(getHL()) >> 1));
        setZeroFlag((memory->read(getHL()) == 0));
        return;
    }
    setCarryFlag(registers[reg] & 1);
    registers[reg] = ((registers[reg] & 1) << 7) | (registers[reg] >> 1);
    setZeroFlag((registers[reg] == 0));
}

inline void SM83::instrRL_R8(uint8_t reg) {
    half temp = getCarryFlag();
    setNFlag(0);
    setHalfCarryFlag(0);
    if (reg == 6) {
        setCarryFlag(memory->read(getHL()) >> 7);
        memory->write(getHL(), (temp) | (memory->read(getHL()) << 1));
        setZeroFlag((memory->read(getHL()) == 0) && !getCarryFlag());
        return;
    }
    setCarryFlag(registers[reg] >> 7);
    registers[reg] = (temp) | (registers[reg] << 1);
    setZeroFlag((registers[reg] == 0));
}

inline void SM83::instrRR_R8(uint8_t reg) {
    half temp = getCarryFlag();
    setNFlag(0);
    setHalfCarryFlag(0);
    if (reg == 6) {
        setCarryFlag(memory->read(getHL()) & 1);
        memory->write(getHL(), (temp << 7) | (memory->read(getHL()) >> 1));
        setZeroFlag((memory->read(getHL()) == 0) && !getCarryFlag());
        return;
    }
    setCarryFlag(registers[reg] & 1);
    registers[reg] = (temp << 7) | (registers[reg] >> 1);
    setZeroFlag((registers[reg] == 0));
}

inline void SM83::instrSLA_R8(uint8_t reg) {
    setNFlag(0);
    setHalfCarryFlag(0);
    if (reg == 6) {
        setCarryFlag(memory->read(getHL()) >> 7);
        memory->write(getHL(), memory->read(getHL()) << 1);
        setZeroFlag((memory->read(getHL()) == 0) && !getCarryFlag());
        return;
    }
    setCarryFlag(registers[reg] >> 7);
    registers[reg] <<= 1;
    setZeroFlag((registers[reg] == 0));
}

inline void SM83::instrSRA_R8(uint8_t reg) {
    setNFlag(0);
    setHalfCarryFlag(0);
    if (reg == 6) {
        setCarryFlag(memory->read(getHL()) & 1);
        memory->write(getHL(), (memory->read(getHL()) & 0x80) |
                                   (memory->read(getHL()) >> 1));
        setZeroFlag((memory->read(getHL()) == 0) && !getCarryFlag());
        return;
    }
    setCarryFlag(registers[reg] & 1);
    registers[reg] = (registers[reg] & 0x80) | (registers[reg] >> 1);
    setZeroFlag((registers[reg] == 0));
}

inline void SM83::instrSWAP_R8(uint8_t reg) {
    setNFlag(0);
    setHalfCarryFlag(0);
    setCarryFlag(0);
    if (reg == 6) {
        memory->write(getHL(), (memory->read(getHL()) >> 4) |
                                   (memory->read(getHL()) << 4));
        setZeroFlag((memory->read(getHL()) == 0));
        return;
    }
    registers[reg] = (registers[reg] >> 4) | (registers[reg] << 4);
    setZeroFlag((registers[reg] == 0));
}

inline void SM83::instrSRL_R8(uint8_t reg) {
    setNFlag(0);
    setHalfCarryFlag(0);
    if (reg == 6) {
        setCarryFlag(memory->read(getHL()) & 1);
        memory->write(getHL(), (memory->read(getHL()) >> 1));
        setZeroFlag((memory->read(getHL()) == 0) && !getCarryFlag());
        return;
    }
    setCarryFlag(registers[reg] & 1);
    registers[reg] >>= 1;
    setZeroFlag((registers[reg] == 0));
}

inline void SM83::instrBIT_R8(uint8_t bit, uint8_t reg) {
    setNFlag(0);
    setHalfCarryFlag(1);
    if (reg == 6) {
        setZeroFlag(!((memory->read(getHL()) >> bit) & 1));
        return;
    }
    setZeroFlag(!((registers[reg] >> bit) & 1));
}

inline void SM83::instrRES_R8(uint8_t bit, uint8_t reg) {
    if (reg == 6) {
        memory->write(getHL(), (memory->read(getHL()) & ~(1 << bit)));
        return;
    }
    registers[reg] &= ~(1 << bit);
}

inline void SM83::instrSET_R8(uint8_t bit, uint8_t reg) {
    if (reg == 6) {
        memory->write(getHL(), (memory->read(getHL()) | (1 << bit)));
        return;
    }
    registers[reg] |= (1 << bit);
}

inline void SM83::store16t1(uint8_t reg16, uint16_t val) {
    switch (reg16) {
        case 0:
            setBC(val);
            return;
        case 1:
            setDE(val);
            return;
        case 2:
            setHL(val);
            return;
        case 3:
            sp = val;
            return;
        default:
            throw std::runtime_error("store16t1: invalid register index");
    }
}

// Stores a 16-bit value into a register pair or AF as follows:
//   reg16 == 0 → BC
//   reg16 == 1 → DE
//   reg16 == 2 → HL
//   reg16 == 3 → AF
inline void SM83::store16t2(uint8_t reg16, uint16_t val) {
    switch (reg16) {
        case 0:
            setBC(val);
            return;
        case 1:
            setDE(val);
            return;
        case 2:
            setHL(val);
            return;
        case 3:
            setAF(val);
            return;
        default:
            throw std::runtime_error("store16t2: invalid register index");
    }
}

// Loads a 16-bit value from a register or SP:
//   reg16 == 0 → BC
//   reg16 == 1 → DE
//   reg16 == 2 → HL
//   reg16 == 3 → SP
inline uint16_t SM83::load16t1(uint8_t reg16) {
    switch (reg16) {
        case 0:
            return getBC();
        case 1:
            return getDE();
        case 2:
            return getHL();
        case 3:
            return sp;
        default:
            throw std::runtime_error("load16t1: invalid register index");
    }
}

// Loads a 16-bit value from a register pair:
//   reg16 == 0 → BC
//   reg16 == 1 → DE
//   reg16 == 2 → HL
//   reg16 == 3 → AF
inline uint16_t SM83::load16t2(uint8_t reg16) {
    switch (reg16) {
        case 0:
            return getBC();
        case 1:
            return getDE();
        case 2:
            return getHL();
        case 3:
            return getAF();
        default:
            throw std::runtime_error("load16t2: invalid register index");
    }
}

inline void SM83::call_interrupt(uint16_t handler) {
    IME = false;
    memory->write(--sp, static_cast<uint8_t>(pc >> 8));
    memory->write(--sp, static_cast<uint8_t>(pc));
    pc = handler;
    cycles += 20;
}

inline void SM83::increment_timer() {
    if (!halted) {
        divcounter++;
        if (divcounter == 256) {
            divcounter = 0;
            byte current_div = memory->read(addr(IORegister::DIV));
            memory->write(addr(IORegister::DIV), current_div + 1);
        }

        divcounter %= 256;
    } else {
        divcounter = 0;
        memory->write(addr(IORegister::DIV), 0);
    }

    if (tacreg & (1 << 2)) {
        tacreg = memory->read(addr(IORegister::TAC));
        timareg = memory->read(addr(IORegister::TIMA));

        timacounter++;
        if ((timacounter == 256 * 4) && ((tacreg & 0x3) == 0)) {
            byte current_tima = memory->read(addr(IORegister::TIMA));
            memory->write(addr(IORegister::TIMA), current_tima + 1);
        } else if ((timacounter == 4 * 4) && ((tacreg & 0x3) == 1)) {
            byte current_tima = memory->read(addr(IORegister::TIMA));
            memory->write(addr(IORegister::TIMA), current_tima + 1);
        } else if ((timacounter == 16 * 4) && ((tacreg & 0x3) == 2)) {
            byte current_tima = memory->read(addr(IORegister::TIMA));
            memory->write(addr(IORegister::TIMA), current_tima + 1);
        } else if ((timacounter == 64 * 4) && ((tacreg & 0x3) == 3)) {
            byte current_tima = memory->read(addr(IORegister::TIMA));
            memory->write(addr(IORegister::TIMA), current_tima + 1);
        }

        if (timareg < memory->read(addr(IORegister::TIMA))) {
            memory->write(addr(IORegister::TIMA),
                          memory->read(addr(IORegister::TMA)));
            memory->write(addr(MemoryRegion::IE),
                          memory->read(addr(MemoryRegion::IE)) | (1 << 2));
        }

        timacounter %= 1024;
    }
}

void SM83::dump_registers() {
    std::ofstream("log.txt", std::ofstream::app) << "rA: " << (int)RA << " "
                                                 << "rB: " << (int)RB << " "
                                                 << "rC: " << (int)RC << " "
                                                 << "rD: " << (int)RD << '\n'
                                                 << "rE: " << (int)RE << " "
                                                 << "rF: " << (int)RF << " "
                                                 << "rH: " << (int)RH << " "
                                                 << "rL: " << (int)RL << '\n';
}

void SM83::dump_info() {
    std::ofstream("log.txt", std::ofstream::app)
        << "timer: "
        << (int)memory->read(static_cast<uint16_t>(IORegister::DIV)) << '\n';
    std::ofstream("log.txt", std::ofstream::app)
        << "TAC: " << (int)memory->read(static_cast<uint16_t>(IORegister::TAC))
        << '\n';
}
bool SM83::getZeroFlag() const {
    return static_cast<bool>((registers[6] >> 7) & 1);
}
bool SM83::getNFlag() const {
    return static_cast<bool>((registers[6] >> 6) & 1);
}
bool SM83::getHalfCarryFlag() const {
    return static_cast<bool>((registers[6] >> 5) & 1);
}
bool SM83::getCarryFlag() const {
    return static_cast<bool>((registers[6] >> 4) & 1);
}
void SM83::setZeroFlag(bool val) {
    RF = (RF & ~(1 << 7)) | ((val ? 1 : 0) << 7);
}
void SM83::setNFlag(bool val) { RF = (RF & ~(1 << 6)) | ((val ? 1 : 0) << 6); }
void SM83::setHalfCarryFlag(bool val) {
    RF = (RF & ~(1 << 5)) | ((val ? 1 : 0) << 5);
}
void SM83::setCarryFlag(bool val) {
    RF = (RF & ~(1 << 4)) | ((val ? 1 : 0) << 4);
}

uint16_t SM83::getAF() const { return (RA << 8) | RF; }
uint16_t SM83::getBC() const { return (RB << 8) | RC; }
uint16_t SM83::getDE() const { return (RD << 8) | RE; }
uint16_t SM83::getHL() const { return (RH << 8) | RL; }

void SM83::setAF(uint16_t val) {
    registers[7] = val >> 8;
    registers[6] = val & 0xF0;
}
void SM83::setBC(uint16_t val) {
    registers[0] = val >> 8;
    registers[1] = val & 0xFF;
}
void SM83::setDE(uint16_t val) {
    registers[2] = val >> 8;
    registers[3] = val & 0xFF;
}
void SM83::setHL(uint16_t val) {
    registers[4] = val >> 8;
    registers[5] = val & 0xFF;
}
uint8_t SM83::fetch8() { return memory->read(pc++); }
uint16_t SM83::fetch16() {
    uint16_t result = memory->read(pc) | (memory->read(pc + 1) << 8);
    pc += 2;
    return result;
}
bool SM83::halfCarryAdd(uint8_t a, uint8_t b) {
    return ((a & 0xF) + (b & 0xF)) > 0xF;
}
bool SM83::halfCarryAdd_WithCarry(uint8_t a, uint8_t b) {
    return ((a & 0xF) + (b & 0xF) + getCarryFlag()) > 0xF;
}
bool SM83::halfCarrySub(uint8_t a, uint8_t b) { return (a & 0xF) < (b & 0xF); }
bool SM83::halfCarrySub_WithCarry(uint8_t a, uint8_t b) {
    return (a & 0xF) < ((b & 0xF) + getCarryFlag());
}
}  // namespace GBC