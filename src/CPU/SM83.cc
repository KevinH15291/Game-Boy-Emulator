#include "SM83.h"
#include "bus.h"
#include "cycles.h"

namespace GBC {

    void SM83::execute() {
    increment_timer();
    cached_interrupt_flags = memory->read(IF) & memory->read(IE);
    if (cached_interrupt_flags != 0) halted = false;
    if (halted) return;

    if (cycles > 0) {
        --cycles;
        return;    
    }
    
    if (pc > 0xFF && memory->booting) {
        memory->booting = false;
        dump_registers();
    }
    
    if (IME && cached_interrupt_flags) {
        if ((cached_interrupt_flags & 1)) {
            IME = 0;
            memory->write(IF, memory->read(IF) & ~(1));
            call_interrupt(0x40);
            return;
        }
        if ((cached_interrupt_flags & (1 << 1))) {
            IME = 0;
            memory->write(IF, memory->read(IF) & ~(1 << 1));
            call_interrupt(0x48);
            return;
        }
        if ((cached_interrupt_flags & (1 << 2))) {
            IME = 0;
            memory->write(IF, memory->read(IF) & ~(1 << 2));
            call_interrupt(0x50);
            return;
        }
        if ((cached_interrupt_flags & (1 << 3))) {
            IME = 0;
            memory->write(IF, memory->read(IF) & ~(1 << 3));
            call_interrupt(0x58);
            return;
        }
        if ((cached_interrupt_flags & (1 << 4))) {
            IME = 0;
            memory->write(IF, memory->read(IF) & ~(1 << 4));
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

    if (opcode <= 0x3F)
    switch (opcode) {
        // ----- Block 0: 0x00 - 0x3F -----
        case 0x00: // NOP
            return;
        case 0x01: // LD BC, d16
            instrLdR16_Imm16(0);
            return;
        case 0x02: // LD (BC), A
            instrLdR16Mem_A(0);
            return;
        case 0x03: // INC BC
            instrIncR16(0);
            return;
        case 0x04: // INC B
            instrIncR8(0);
            return;
        case 0x05: // DEC B
            instrDecR8(0);
            return;
        case 0x06: // LD B, d8
            instrLdR8_Imm8(0);
            return;
        case 0x07: // RLCA
            instrRLCA();
            return;
        case 0x08: // LD [d16], SP
            instrLdImm16SP();
            return;
        case 0x09: // ADD HL, BC
            instrAddHL_R16(0);
            return;
        case 0x0A: // LD A, (BC)
            instrLdA_R16Mem(0);
            return;
        case 0x0B: // DEC BC
            instrDecR16(0);
            return;
        case 0x0C: // INC C
            instrIncR8(1);
            return;
        case 0x0D: // DEC C
            instrDecR8(1);
            return;
        case 0x0E: // LD C, d8
            instrLdR8_Imm8(1);
            return;
        case 0x0F: // RRCA
            instrRRCA();
            return;
        case 0x10: // STOP
            instrSTOP();
            return;
        case 0x11: // LD DE, d16
            instrLdR16_Imm16(1);
            return;
        case 0x12: // LD (DE), A
            instrLdR16Mem_A(1);
            return;
        case 0x13: // INC DE
            instrIncR16(1);
            return;
        case 0x14: // INC D
            instrIncR8(2);
            return;
        case 0x15: // DEC D
            instrDecR8(2);
            return;
        case 0x16: // LD D, d8
            instrLdR8_Imm8(2);
            return;
        case 0x17: // RLA
            instrRLA();
            return;
        case 0x18: // JR d8 
            instrJR_Imm8();
            return;
        case 0x19: // ADD HL, DE
            instrAddHL_R16(1);
            return;
        case 0x1A: // LD A, (DE)
            instrLdA_R16Mem(1);
            return;
        case 0x1B: // DEC DE
            instrDecR16(1);
            return;
        case 0x1C: // INC E
            instrIncR8(3);
            return;
        case 0x1D: // DEC E
            instrDecR8(3);
            return;
        case 0x1E: // LD E, d8
            instrLdR8_Imm8(3);
            return;
        case 0x1F: // RRA
            instrRRA();
            return;
        case 0x20: // JR NZ, d8
            instrJR_Cond_Imm8(!getZeroFlag());
            return;
        case 0x21: // LD HL, d16
            instrLdR16_Imm16(2);
            return;
        case 0x22: // LDI (HL), A
            instrLdiHL_A();
            return;
        case 0x23: // INC HL
            instrIncR16(2);
            return;
        case 0x24: // INC H
            instrIncR8(4);
            return;
        case 0x25: // DEC H
            instrDecR8(4);
            return;
        case 0x26: // LD H, d8
            instrLdR8_Imm8(4);
            return;
        case 0x27: // DAA
            instrDAA();
            return;
        case 0x28: // JR Z, d8
            instrJR_Cond_Imm8(getZeroFlag());
            return;
        case 0x29: // ADD HL, HL
            instrAddHL_R16(2);
            return;
        case 0x2A: // LD A, (HL) 
            instrLdA_HL();
            return;
        case 0x2B: // DEC HL
            instrDecR16(2);
            return;
        case 0x2C: // INC L
            instrIncR8(5);
            return;
        case 0x2D: // DEC L
            instrDecR8(5);
            return;
        case 0x2E: // LD L, d8
            instrLdR8_Imm8(5);
            return;
        case 0x2F: // CPL 
            instrCPL();
            return;
        case 0x30: // JR NC, d8
            instrJR_Cond_Imm8(!getCarryFlag());
            return;
        case 0x31: // LD SP, d16
            instrLdR16_Imm16(3);
            return;
        case 0x32: // LDD (HL), A 
            instrLddHL_A();
            return;
        case 0x33: // INC SP
            instrIncSP();
            return;
        case 0x34: // INC (HL)
            instrIncHL();
            return;
        case 0x35: // DEC (HL)
            instrDecHL();
            return;
        case 0x36: // LD (HL), d8
            instrLdImm8_HL();
            return;
        case 0x37: // SCF 
            instrSCF();
            return;
        case 0x38: // JR C, d8
            instrJR_Cond_Imm8(getCarryFlag());
            return;
        case 0x39: // ADD HL, SP
            instrAddHL_R16(3);
            return;
        case 0x3A: // LD A, (HL-) 
            instrLdA_HLDec();
            return;
        case 0x3B: // DEC SP
            --sp;
            return;
        case 0x3C: // INC A
            instrIncR8(7);
            return;
        case 0x3D: // DEC A
            instrDecR8(7);
            return;
        case 0x3E: // LD A, d8
            instrLdR8_Imm8(7);
            return;
        case 0x3F: // CCF 
            instrCCF();
            return;
        default:
            break;
        } else if (opcode >= 0x40 && opcode <= 0x7F) {
            // ----- Block 1: 0x40 - 0x7F -----
            
            // These opcodes are all "LD r8, r8" instructions.
            // Decoding the destination and source registers:
            uint8_t dest = (opcode >> 3) & 0x07;
            uint8_t src  = opcode & 0x07;

            if (opcode == 0x76) {
                // 0x76 is HALT.
                instrHALT();
            } else if (dest == 6) {
                instrLdHL_FromR8(src);
            } else if (src == 6) {
                instrLdR8_FromHL(dest);
            } else {
                instrLdR8_R8(dest, src);
            }
        } else if (opcode >= 0x80 && opcode <= 0xBF) {
            // ----- Block 2: 0x80 - 0xBF (ALU operations on A with a register) -----
            uint8_t aluGroup = (opcode >> 3) & 0x07;  // 0: ADD, 1: ADC, 2: SUB, 3: SBC, 4: AND, 5: XOR, 6: OR, 7: CP.
            uint8_t reg = opcode & 0x07;
            switch (aluGroup) {
                case 0: instrAddA_R8(reg); return;
                case 1: instrAdcA_R8(reg); return;
                case 2: instrSubA_R8(reg); return;
                case 3: instrSbcA_R8(reg); return;
                case 4: instrAndA_R8(reg); return;
                case 5: instrXorA_R8(reg); return;
                case 6: instrOrA_R8(reg); return;
                case 7: instrCpA_R8(reg); return;
                default:
                    throw std::runtime_error("Invalid ALU opcode");
            }
        } else {
        // ----- Block 3: 0xC0 - 0xFF (Control flow, immediate ALU ops, and miscellaneous) -----
            switch (opcode) {
                case 0xC0: // RET NZ
                    instrRET_Cond(!getZeroFlag());
                    return;
                case 0xC1: // POP BC
                    instrPOP_R16(0);
                    return;
                case 0xC2: // JP NZ, d16
                    instrJP_Cond_Imm16(!getZeroFlag());
                    return;
                case 0xC3: // JP d16
                    instrJP_Imm16();
                    return;
                case 0xC4: // CALL NZ, d16
                    instrCALL_Cond_Imm16(!getZeroFlag());
                    return;
                case 0xC5: // PUSH BC
                    instrPUSH_R16(0);
                    return;
                case 0xC6: // ADD A, d8
                    instrAddA_Imm8();
                    return;
                case 0xC7: // RST 0x00
                    instrRST(0x00);
                    return;
                case 0xC8: // RET Z
                    instrRET_Cond(getZeroFlag());
                    return;
                case 0xC9: // RET
                    instrRET();
                    return;
                case 0xCA: // JP Z, d16
                    instrJP_Cond_Imm16(getZeroFlag());
                    return;
                case 0xCB: // CB-prefixed opcodes
                    executeCB();
                    return;
                case 0xCC: // CALL Z, d16
                    instrCALL_Cond_Imm16(getZeroFlag());
                    return;
                case 0xCD: // CALL d16
                    instrCALL_Imm16();
                    return;
                case 0xCE: // ADC A, d8
                    instrAdcA_Imm8();
                    return;
                case 0xCF: // RST 0x08
                    instrRST(0x08);
                    return;
                case 0xD0: // RET NC
                    instrRET_Cond(!getCarryFlag());
                    return;
                case 0xD1: // POP DE
                    instrPOP_R16(1);
                    return;
                case 0xD2: // JP NC, d16
                    instrJP_Cond_Imm16(!getCarryFlag());
                    return;
                case 0xD3:
                    throw std::runtime_error("Opcode D3 not used");
                    return;
                case 0xD4: // CALL NC, d16
                    instrCALL_Cond_Imm16(!getCarryFlag());
                    return;
                case 0xD5: // PUSH DE
                    instrPUSH_R16(1);
                    return;
                case 0xD6: // SUB A, d8
                    instrSubA_Imm8();
                    setNFlag(true);
                    return;
                case 0xD7: // RST 0x10
                    instrRST(0x10);
                    return;
                case 0xD8: // RET C
                    instrRET_Cond(getCarryFlag());
                    return;
                case 0xD9: // RETI
                    instrRETI();
                    return;
                case 0xDA: // JP C, d16
                    instrJP_Cond_Imm16(getCarryFlag());
                    return;
                case 0xDB:
                    throw std::runtime_error("Opcode DB not used");
                    return;
                case 0xDC: // CALL C, d16
                    instrCALL_Cond_Imm16(getCarryFlag());
                    return;
                case 0xDD:
                    throw std::runtime_error("Opcode DD not used");
                    return;
                case 0xDE: // SBC A, d8
                    instrSbcA_Imm8();
                    return;
                case 0xDF: // RST 0x18
                    instrRST(0x18);
                    return;
                case 0xE0: // LDH (n), A
                    instrLDH_Imm8_A();
                    return;
                case 0xE1: // POP HL
                    instrPOP_R16(2);
                    return;
                case 0xE2: // LDH (C), A
                    instrLDH_C_A();
                    return;
                case 0xE3:
                    throw std::runtime_error("Opcode E3 not used");
                    return;
                case 0xE4:
                    throw std::runtime_error("Opcode E4 not used");
                    return;
                case 0xE5: // PUSH HL
                    instrPUSH_R16(2);
                    return;
                case 0xE6: // AND A, d8
                    instrAndA_Imm8();
                    return;
                case 0xE7: // RST 0x20
                    instrRST(0x20);
                    return;
                case 0xE8: // ADD SP, d8
                    instrAddSP_Imm8();
                    return;
                case 0xE9: // JP (HL)
                    instrJP_HL();
                    return;
                case 0xEA: // LD (a16), A
                    instrLD_Imm16_A();
                    return;
                case 0xEB:
                    throw std::runtime_error("Opcode EB not used");
                    return;
                case 0xEC:
                    throw std::runtime_error("Opcode EC not used");
                    return;
                case 0xED:
                    throw std::runtime_error("Opcode ED not used");
                    return;
                case 0xEE: // XOR A, d8
                    instrXorA_Imm8();
                    return;
                case 0xEF: // RST 0x28
                    instrRST(0x28);
                    return;
                case 0xF0: // LDH A, (n)
                    instrLDH_A_Imm8();
                    return;
                case 0xF1: // POP AF
                    instrPOP_R16(3);
                    return;
                case 0xF2: // LDH A, (C)
                    instrLDH_A_C();
                    return;
                case 0xF3: // DI
                    instrDI();
                    return;
                case 0xF4:
                    throw std::runtime_error("Opcode F4 not used");
                    return;
                case 0xF5: // PUSH AF
                    instrPUSH_R16(3);
                    return;
                case 0xF6: // OR A, d8
                    instrOrA_Imm8();
                    return;
                case 0xF7: // RST 0x30
                    instrRST(0x30);
                    return;
                case 0xF8: // LD HL, SP+d8
                    instrLD_HL_SP_Imm8();
                    return;
                case 0xF9: // LD SP, HL
                    instrLD_SP_HL();
                    return;
                case 0xFA: // LD A, (a16)
                    instrLD_A_Imm16();
                    return;
                case 0xFB: // EI
                    instrEI();
                    return;
                case 0xFC:
                    throw std::runtime_error("Opcode FC not used");
                    return;
                case 0xFD:
                    throw std::runtime_error("Opcode FD not used");
                    return;
                case 0xFE: // CP A, d8
                    instrCpA_Imm8();
                    return;
                case 0xFF: // RST 0x38
                    instrRST(0x38);
                    return;
                default:
                    throw std::runtime_error("Invalid opcode in 0xC0-0xFF range");
            }
        }
    }
    inline void SM83::executeCB() {
        // Fetch the next byte which is the actual CB opcode.
        uint8_t cbOpcode = fetch8();
        cycles += opcode_cycles_cb[cbOpcode];
        opcode = (opcode << 8) +  cbOpcode;
        // Decode based on the top two bits.
        switch (cbOpcode >> 6) {
            case 0: // Rotate and swap instructions.
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
            case 1: // BIT b, r
                instrBIT_R8((cbOpcode >> 3) & 0x07, cbOpcode & 0x07);
                break;
            case 2: // RES b, r
                instrRES_R8((cbOpcode >> 3) & 0x07, cbOpcode & 0x07);
                break;
            case 3: // SET b, r
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
        half temp = (RA & 1);
        temp |= (RA << 8);
        RA >>= 1;
        RA |= ((getCarryFlag() & 1) << 7);
        setHalfCarryFlag(false);
        setNFlag(false);
        setCarryFlag(temp & 1);
        setZeroFlag((RA == 0) && ((temp >> 8) != 0) && (getCarryFlag() == 0));
    }
    
    inline void SM83::instrLdiHL_A() {
        // Store A at address HL, then increment HL.
        memory->write(getHL(), RA);
        setHL(getHL() + 1);
    }
    
    inline void SM83::instrDAA() {
        processDAA();
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
    
    inline void SM83::instrIncSP() {
        sp++;
    }
    
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
        memory->write(DIV, 0);
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
        uint8_t temp = r8[reg];
        r8[reg]++;
        setZeroFlag(r8[reg] == 0);
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
        uint8_t temp = r8[reg];
        r8[reg]--;
        setZeroFlag(r8[reg] == 0);
        setNFlag(true);
        setHalfCarryFlag(halfCarrySub(temp, r8[reg])); 
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
        memory->write(fetch8(), r8[reg]);
    }
    inline void SM83::instrLdR8_Imm8(uint8_t reg) {
        r8[reg] = fetch8();
    }
    
    inline void SM83::instrLdImm8_HL() {
        memory->write(getHL(), fetch8());
    }
    
    // Block 1: Register-to-register loads
    
    inline void SM83::instrHALT() {
        halted = true;
        memory->write(DIV, 0);
        divcounter = 0;
    }
    
    inline void SM83::instrLdR8_FromHL(uint8_t reg) {
        r8[reg] = memory->read(getHL());
    }
    
    inline void SM83::instrLdHL_FromR8(uint8_t reg) {
        memory->write(getHL(), r8[reg]);
    }
    
    inline void SM83::instrLdR8_R8(uint8_t dest, uint8_t src) {
        r8[dest] = r8[src];
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

        setHalfCarryFlag(halfCarryAdd(RA, r8[reg]));
        setCarryFlag(RA + r8[reg] > 0xFF);
        RA += r8[reg];
        setZeroFlag(RA == 0);
        setNFlag(false);
    }
    
    inline void SM83::instrAdcA_R8(uint8_t reg) {
        uint16_t temp = RA;
        if (reg == 6) {
            setHalfCarryFlag(halfCarryAdd_WithCarry(RA, memory->read(getHL())));
            RA += memory->read(getHL()) + getCarryFlag();
            setCarryFlag((temp + memory->read(getHL())+getCarryFlag()) > 0xFF);

            setZeroFlag(RA == 0);
            setNFlag(false);
            return;
        }
        setHalfCarryFlag(halfCarryAdd_WithCarry(RA, r8[reg]));
        RA += r8[reg] + getCarryFlag();
        setCarryFlag((temp+getCarryFlag()) > RA);
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
        setHalfCarryFlag(halfCarrySub(RA, r8[reg]));
        RA -= r8[reg];
        setCarryFlag(RA > temp); 
        setZeroFlag(RA == 0);
    }
    
    inline void SM83::instrSbcA_R8(uint8_t reg) {
        uint16_t temp = RA;
        setNFlag(true);
        if (reg == 6) {
            setHalfCarryFlag(halfCarrySub_WithCarry(RA, memory->read(getHL())));
            RA -= memory->read(getHL())+getCarryFlag();
            setCarryFlag(RA > temp-getCarryFlag()); 
            setZeroFlag(RA == 0);
            return;
        }
        setHalfCarryFlag(halfCarrySub_WithCarry(RA, r8[reg]));
        RA -= r8[reg]+getCarryFlag();
        setCarryFlag(RA > temp-getCarryFlag()); 
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
        RA &= r8[reg];
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
        RA ^= r8[reg];
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
        RA |= r8[reg];
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
        setCarryFlag(RA < r8[reg]);
        setHalfCarryFlag(halfCarrySub(RA, r8[reg]));
        setZeroFlag(RA == r8[reg]);
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
        setCarryFlag((temp+getCarryFlag()) > RA);
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
        RA -= temp+getCarryFlag();
        setCarryFlag(RA > oldRA-getCarryFlag()); 
        
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
        RA ^=  fetch8();
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
    
    inline void SM83::instrJP_Imm16() {
        pc = fetch16();
    }
    
    inline void SM83::instrJP_HL() {
        pc = getHL();
    }
    
    inline void SM83::instrCALL_Imm16() {
        uint16_t temp = fetch16();
        memory->write(--sp, static_cast<uint8_t>(pc >> 8));
        memory->write(--sp, static_cast<uint8_t>(pc));
        pc = temp;
    }
    
    inline void SM83::instrLDH_C_A() {
        memory->write(0xFF00 + RC, RA);
    }
    
    inline void SM83::instrLDH_Imm8_A() {
        memory->write(0xFF00 | fetch8(), RA);
    }
    
    inline void SM83::instrLD_Imm16_A() {
        
        memory->write(fetch16(), RA);
    }
    
    inline void SM83::instrLDH_A_C() {
        RA = memory->read(0xFF00 | RC);  
    }
    
    inline void SM83::instrLDH_A_Imm8() {
        RA = memory->read(0xFF00 | fetch8());
    }
    
    inline void SM83::instrLD_A_Imm16() {
        RA = memory->read(fetch16());
    }
    
    inline void SM83::instrAddSP_Imm8() {
        setZeroFlag(0);
        setNFlag(0);
        half temp = fetch8();
        setHalfCarryFlag(halfCarryAdd(sp, temp));
        setCarryFlag((sp&0xFF) + temp > 0xFF);
        sp += (int8_t)temp;

    }
    
    inline void SM83::instrLD_HL_SP_Imm8() {
        half temp = fetch8();
        setZeroFlag(0);
        setNFlag(0);
        setHalfCarryFlag(halfCarryAdd(sp, temp));
        setCarryFlag(((sp&0xFF) + (temp&0xFF)) > (uint16_t)0xFF);
        setHL((int16_t)sp+int8_t(temp));

    }
    
    inline void SM83::instrLD_SP_HL() {
        sp = getHL();
    }
    
    inline void SM83::instrDI() {
        IME = false;
    }
    
    inline void SM83::instrEI() {
        IMEdelay = true;
    }
    
    // Conditional RET, JP, and CALL (grouped)
    
    inline void SM83::instrRET_Cond(bool condition) {
        if (condition) {
            cycles+=3;
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
            cycles+=3;
        }
    }
    
    // RST Instruction
    
    inline void SM83::instrRST(uint8_t target) {
        // throw std::runtime_error(std::string("WHY AM I HERE!!?!!?!").append(std::to_string(pc)));
        memory->write(--sp, static_cast<uint8_t>(pc >> 8));
        memory->write(--sp, static_cast<uint8_t>(pc));
        pc = target;

        
    }
    
    // POP and PUSH instructions for 16-bit registers
    
    inline void SM83::instrPOP_R16(uint8_t regIndex) {
        half value = (memory->read(sp) ) | ((half)memory->read(half(sp + 1)) << 8);
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
            memory->write(getHL(), (memory->read(getHL()) >> 7) | (memory->read(getHL()) << 1));
            setZeroFlag((memory->read(getHL()) == 0));
            return;
        }
        r8[reg] = (r8[reg] >> 7) | (r8[reg] << 1);
        setZeroFlag((r8[reg] == 0));
        setCarryFlag(r8[reg] & 0x80);
    }
    
    inline void SM83::instrRRC_R8(uint8_t reg) {
        setNFlag(0);
        setHalfCarryFlag(0);
        if (reg == 6) {
            setCarryFlag(memory->read(getHL()) & 1);
            memory->write(getHL(), ((memory->read(getHL()) & 1) << 7) | (memory->read(getHL()) >> 1));
            setZeroFlag((memory->read(getHL()) == 0));
            return;
        }
        setCarryFlag(r8[reg] & 1);
        r8[reg] = ((r8[reg] & 1) << 7) | (r8[reg] >> 1);
        setZeroFlag((r8[reg] == 0));

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
        setCarryFlag(r8[reg] >> 7);
        r8[reg] = (temp) | (r8[reg] << 1);
        setZeroFlag((r8[reg] == 0));
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
        setCarryFlag(r8[reg] & 1);
        r8[reg] = (temp << 7) | (r8[reg] >> 1);
        setZeroFlag((r8[reg] == 0));
    }
    
    inline void SM83::instrSLA_R8(uint8_t reg) {
        setNFlag(0);
        setHalfCarryFlag(0);
        if (reg == 6) {
            setCarryFlag(memory->read(getHL()) >> 7);
            memory->write(getHL(), memory->read(getHL())<<1);
            setZeroFlag((memory->read(getHL()) == 0) && !getCarryFlag());
            return;
        }
        setCarryFlag(r8[reg] >> 7);
        r8[reg] <<= 1;
        setZeroFlag((r8[reg] == 0));
    }
    
    inline void SM83::instrSRA_R8(uint8_t reg) {
        setNFlag(0);
        setHalfCarryFlag(0);
        if (reg == 6) {
            setCarryFlag(memory->read(getHL()) & 1);
            memory->write(getHL(), (memory->read(getHL()) & 0x80) | (memory->read(getHL()) >> 1));
            setZeroFlag((memory->read(getHL()) == 0) && !getCarryFlag());
            return;
        }
        setCarryFlag(r8[reg] & 1);
        r8[reg] = (r8[reg] & 0x80) | (r8[reg] >> 1);
        setZeroFlag((r8[reg] == 0));
    }
    
    inline void SM83::instrSWAP_R8(uint8_t reg) {
        setNFlag(0);
        setHalfCarryFlag(0);
        setCarryFlag(0);
        if (reg == 6) {
            memory->write(getHL(), (memory->read(getHL()) >> 4) | (memory->read(getHL()) << 4));
            setZeroFlag((memory->read(getHL()) == 0));
            return;
        }
        r8[reg] = (r8[reg] >> 4) | (r8[reg] << 4);
        setZeroFlag((r8[reg] == 0));
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
        setCarryFlag(r8[reg] & 1);
        r8[reg] >>= 1;
        setZeroFlag((r8[reg] == 0));
    }
    
    inline void SM83::instrBIT_R8(uint8_t bit, uint8_t reg) {
        setNFlag(0);
        setHalfCarryFlag(1);
        if (reg == 6) {
            setZeroFlag(!((memory->read(getHL()) >> bit) & 1));
            return;
        }
        setZeroFlag(!((r8[reg] >> bit) & 1));
    }
    
    inline void SM83::instrRES_R8(uint8_t bit, uint8_t reg) {
        if (reg == 6) {
            memory->write(getHL(), (memory->read(getHL()) & ~(1 << bit)));
            return;
        }
        r8[reg] &= ~(1 << bit);
    }
    
    inline void SM83::instrSET_R8(uint8_t bit, uint8_t reg) {
        if (reg == 6) {
            memory->write(getHL(), (memory->read(getHL()) | (1 << bit)));
            return;
        }
        r8[reg] |= (1 << bit);
    }
    
    // DAA processing
    
    inline void SM83::processDAA() {
        half temp = RA;
        if (getNFlag()) {
            setZeroFlag((RA == 0) && !getCarryFlag() && !getHalfCarryFlag());
            return;
        }
        
        if (getHalfCarryFlag() && ((RA & 0xF) < 10)) {
            RA = ((RA & 0xF) + 6) % 10;
        } else {
            RA = (RA & 0xF) % 10;
        }
        
        if (getCarryFlag() && ((temp >> 4) < 10)) {
            RA |= (((temp >> 4) % 10) + (temp & 0xF) / 10 + 6) << 4;
        } else {
            RA |= (((temp >> 4) % 10) + (temp & 0xF) / 10) << 4;
        }
        
        setCarryFlag(((temp >> 4) > 10) || getCarryFlag());
        setHalfCarryFlag(false);
        setZeroFlag((RA == 0) && !getCarryFlag());
    }

    inline void SM83::store16t1(uint8_t reg16, uint16_t val) {
        if (reg16 > 3) {
            throw std::runtime_error("store16t1: invalid register index");
        }
        switch(reg16) {
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
        switch(reg16) {
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
        switch(reg16) {
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
        switch(reg16) {
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
                memory->IOrange[DIV-IO_REGISTERS]++;
            }
            
            divcounter %= 256;
        } else {
            divcounter = 0;
            memory->IOrange[DIV-IO_REGISTERS] = 0;
        }
    
        if (tacreg & (1 << 2)) {
            tacreg = memory->read(TAC);
            timareg = memory->read(TIMA);

            timacounter++;
            if ((timacounter == 256*4) && ((tacreg & 0x3) == 0)) {
                memory->IOrange[TIMA-IO_REGISTERS]++;
            } else if ((timacounter == 4*4) && ((tacreg & 0x3) == 1)) {
                memory->IOrange[TIMA-IO_REGISTERS]++;
            } else if ((timacounter == 16*4) && ((tacreg & 0x3) == 2)) {
                memory->IOrange[TIMA-IO_REGISTERS]++;
            } else if ((timacounter == 64*4) && ((tacreg & 0x3) == 3)) { 
                memory->IOrange[TIMA-IO_REGISTERS]++;
            } 
            
            if (timareg < memory->read(TIMA)) {
                memory->IOrange[TIMA-IO_REGISTERS] = memory->read(TMA);
                memory->write(IE, memory->read(IE) | (1<<2));
            }
            
            timacounter %= 1024;
        }
    }

    void SM83::dump_registers() {
        std::ofstream("log.txt", std::ofstream::app) 
        << "rA: " << (int)RA << " "
        << "rB: " << (int)RB << " "
        << "rC: " << (int)RC << " "
        << "rD: " << (int)RD << '\n'
        << "rE: " << (int)RE << " "
        << "rF: " << (int)RF << " "
        << "rH: " << (int)RH << " "
        << "rL: " << (int)RL << '\n';
    }

    void SM83::dump_info() {
        std::ofstream("log.txt", std::ofstream::app) << "timer: " << (int)memory->read(DIV) << '\n';
        std::ofstream("log.txt", std::ofstream::app) << "TAC: " << (int)memory->read(TAC) << '\n';
    }


}