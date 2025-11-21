#include "SM83.h"

#include <cassert>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

#include "../bit_ops.h"
#include "bus.h"
#include "cycles.h"
#include "enums.h"

#ifdef __EMSCRIPTEN__
#define THROW_ERROR(msg) std::abort()
#else
#define THROW_ERROR(msg) throw std::runtime_error(msg)
#endif

namespace GBC {

void SM83::execute() {
    increment_timer();

    byte current_IE = memory->IEnable;
    byte current_IF =
        memory
            ->IOrange[addr(IORegister::IF) - addr(MemoryRegion::IO_REGISTERS)];
    byte pending_interrupts = current_IF & current_IE;

    if (pending_interrupts != 0) halted = false;
    if (halted) return;

    if (cycles > 0) {
        --cycles;
        return;
    }

    if (IME && pending_interrupts) {
        static constexpr std::array<half, 5> interrupt_handlers = {
            0x40, 0x48, 0x50, 0x58, 0x60};

        for (int i = 0; i < 5; ++i) {
            if (isBitSet(pending_interrupts, i)) {
                IME = false;
                auto& if_reg =
                    memory->IOrange[addr(IORegister::IF) -
                                    addr(MemoryRegion::IO_REGISTERS)];
                if_reg = clearBit(if_reg, i);
                call_interrupt(interrupt_handlers[i]);
                return;
            }
        }
    }

    if (IMEdelay) {
        IME = true;
        IMEdelay = false;
    }

    opcode = fetch8();
    cycles += opcode_cycles[opcode];

    if (halt_bug) {
        halt_bug = false;
        --pc;
    }

    if (opcode <= 0x3F) switch (opcode) {
            case 0x00:
                return;
            case 0x01:  // LD BC, d16
                instrLdR16_Imm16(0);
                return;
            case 0x02:  // LD (BC), A
                instrLdR16Mem_A(0);
                return;
            case 0x03:  // INC BC
                instrIncR16(0);
                return;
            case 0x04:  // INC B
                instrIncR8(0);
                return;
            case 0x05:  // DEC B
                instrDecR8(0);
                return;
            case 0x06:  // LD B, d8
                instrLdR8_Imm8(0);
                return;
            case 0x07:  // RLCA
                instrRLCA();
                return;
            case 0x08:  // LD [d16], SP
                instrLdImm16SP();
                return;
            case 0x09:  // ADD HL, BC
                instrAddHL_R16(0);
                return;
            case 0x0A:  // LD A, (BC)
                instrLdA_R16Mem(0);
                return;
            case 0x0B:  // DEC BC
                instrDecR16(0);
                return;
            case 0x0C:  // INC C
                instrIncR8(1);
                return;
            case 0x0D:  // DEC C
                instrDecR8(1);
                return;
            case 0x0E:  // LD C, d8
                instrLdR8_Imm8(1);
                return;
            case 0x0F:  // RRCA
                instrRRCA();
                return;
            case 0x10:  // STOP
                instrSTOP();
                return;
            case 0x11:  // LD DE, d16
                instrLdR16_Imm16(1);
                return;
            case 0x12:  // LD (DE), A
                instrLdR16Mem_A(1);
                return;
            case 0x13:  // INC DE
                instrIncR16(1);
                return;
            case 0x14:  // INC D
                instrIncR8(2);
                return;
            case 0x15:  // DEC D
                instrDecR8(2);
                return;
            case 0x16:  // LD D, d8
                instrLdR8_Imm8(2);
                return;
            case 0x17:  // RLA
                instrRLA();
                return;
            case 0x18:  // JR d8
                instrJR_Imm8();
                return;
            case 0x19:  // ADD HL, DE
                instrAddHL_R16(1);
                return;
            case 0x1A:  // LD A, (DE)
                instrLdA_R16Mem(1);
                return;
            case 0x1B:  // DEC DE
                instrDecR16(1);
                return;
            case 0x1C:  // INC E
                instrIncR8(3);
                return;
            case 0x1D:  // DEC E
                instrDecR8(3);
                return;
            case 0x1E:  // LD E, d8
                instrLdR8_Imm8(3);
                return;
            case 0x1F:  // RRA
                instrRRA();
                return;
            case 0x20:  // JR NZ, d8
                instrJR_Cond_Imm8(!getZeroFlag());
                return;
            case 0x21:  // LD HL, d16
                instrLdR16_Imm16(2);
                return;
            case 0x22:  // LDI (HL), A
                instrLdiHL_A();
                return;
            case 0x23:  // INC HL
                instrIncR16(2);
                return;
            case 0x24:  // INC H
                instrIncR8(4);
                return;
            case 0x25:  // DEC H
                instrDecR8(4);
                return;
            case 0x26:  // LD H, d8
                instrLdR8_Imm8(4);
                return;
            case 0x27:  // DAA
                instrDAA();
                return;
            case 0x28:  // JR Z, d8
                instrJR_Cond_Imm8(getZeroFlag());
                return;
            case 0x29:  // ADD HL, HL
                instrAddHL_R16(2);
                return;
            case 0x2A:  // LD A, (HL)
                instrLdA_HL();
                return;
            case 0x2B:  // DEC HL
                instrDecR16(2);
                return;
            case 0x2C:  // INC L
                instrIncR8(5);
                return;
            case 0x2D:  // DEC L
                instrDecR8(5);
                return;
            case 0x2E:  // LD L, d8
                instrLdR8_Imm8(5);
                return;
            case 0x2F:  // CPL
                instrCPL();
                return;
            case 0x30:  // JR NC, d8
                instrJR_Cond_Imm8(!getCarryFlag());
                return;
            case 0x31:  // LD SP, d16
                instrLdR16_Imm16(3);
                return;
            case 0x32:  // LDD (HL), A
                instrLddHL_A();
                return;
            case 0x33:  // INC SP
                instrIncSP();
                return;
            case 0x34:  // INC (HL)
                instrIncHL();
                return;
            case 0x35:  // DEC (HL)
                instrDecHL();
                return;
            case 0x36:  // LD (HL), d8
                instrLdImm8_HL();
                return;
            case 0x37:  // SCF
                instrSCF();
                return;
            case 0x38:  // JR C, d8
                instrJR_Cond_Imm8(getCarryFlag());
                return;
            case 0x39:  // ADD HL, SP
                instrAddHL_R16(3);
                return;
            case 0x3A:  // LD A, (HL-)
                instrLdA_HLDec();
                return;
            case 0x3B:  // DEC SP
                --sp;
                return;
            case 0x3C:  // INC A
                instrIncR8(7);
                return;
            case 0x3D:  // DEC A
                instrDecR8(7);
                return;
            case 0x3E:  // LD A, d8
                instrLdR8_Imm8(7);
                return;
            case 0x3F:  // CCF
                instrCCF();
                return;
            default:
                break;
        }
    else if (opcode >= 0x40 && opcode <= 0x7F) {
        byte dest = getBitRange(opcode, 3, 3);
        byte src = getBitRange(opcode, 0, 3);

        if (opcode == 0x76) {
            if (!IME && pending_interrupts != 0) {
                halt_bug = true;
            }
            instrHALT();
        } else if (dest == 6) {
            instrLdHL_FromR8(src);
        } else if (src == 6) {
            instrLdR8_FromHL(dest);
        } else {
            instrLdR8_R8(dest, src);
        }
    } else if (opcode >= 0x80 && opcode <= 0xBF) {
        byte aluGroup = getBitRange(opcode, 3, 3);
        byte reg = getBitRange(opcode, 0, 3);
        switch (aluGroup) {
            case 0:
                instrAddA_R8(reg);
                return;
            case 1:
                instrAdcA_R8(reg);
                return;
            case 2:
                instrSubA_R8(reg);
                return;
            case 3:
                instrSbcA_R8(reg);
                return;
            case 4:
                instrAndA_R8(reg);
                return;
            case 5:
                instrXorA_R8(reg);
                return;
            case 6:
                instrOrA_R8(reg);
                return;
            case 7:
                instrCpA_R8(reg);
                return;
            default:
                THROW_ERROR("Invalid ALU opcode");
                return;
        }
    } else {
        switch (opcode) {
            case 0xC0:  // RET NZ
                instrRET_Cond(!getZeroFlag());
                return;
            case 0xC1:  // POP BC
                instrPOP_R16(0);
                return;
            case 0xC2:  // JP NZ, d16
                instrJP_Cond_Imm16(!getZeroFlag());
                return;
            case 0xC3:  // JP d16
                instrJP_Imm16();
                return;
            case 0xC4:  // CALL NZ, d16
                instrCALL_Cond_Imm16(!getZeroFlag());
                return;
            case 0xC5:  // PUSH BC
                instrPUSH_R16(0);
                return;
            case 0xC6:  // ADD A, d8
                instrAddA_Imm8();
                return;
            case 0xC7:  // RST 0x00
                instrRST(0x00);
                return;
            case 0xC8:  // RET Z
                instrRET_Cond(getZeroFlag());
                return;
            case 0xC9:  // RET
                instrRET();
                return;
            case 0xCA:  // JP Z, d16
                instrJP_Cond_Imm16(getZeroFlag());
                return;
            case 0xCB:  // CB-prefixed opcodes
                executeCB();
                return;
            case 0xCC:  // CALL Z, d16
                instrCALL_Cond_Imm16(getZeroFlag());
                return;
            case 0xCD:  // CALL d16
                instrCALL_Imm16();
                return;
            case 0xCE:  // ADC A, d8
                instrAdcA_Imm8();
                return;
            case 0xCF:  // RST 0x08
                instrRST(0x08);
                return;
            case 0xD0:  // RET NC
                instrRET_Cond(!getCarryFlag());
                return;
            case 0xD1:  // POP DE
                instrPOP_R16(1);
                return;
            case 0xD2:  // JP NC, d16
                instrJP_Cond_Imm16(!getCarryFlag());
                return;
            case 0xD3:
                THROW_ERROR("Opcode D3 not used");
                return;
            case 0xD4:  // CALL NC, d16
                instrCALL_Cond_Imm16(!getCarryFlag());
                return;
            case 0xD5:  // PUSH DE
                instrPUSH_R16(1);
                return;
            case 0xD6:  // SUB A, d8
                instrSubA_Imm8();
                return;
            case 0xD7:  // RST 0x10
                instrRST(0x10);
                return;
            case 0xD8:  // RET C
                instrRET_Cond(getCarryFlag());
                return;
            case 0xD9:  // RETI
                instrRETI();
                return;
            case 0xDA:  // JP C, d16
                instrJP_Cond_Imm16(getCarryFlag());
                return;
            case 0xDB:
                THROW_ERROR("Opcode DB not used");
                return;
            case 0xDC:  // CALL C, d16
                instrCALL_Cond_Imm16(getCarryFlag());
                return;
            case 0xDD:
                THROW_ERROR("Opcode DD not used");
                return;
            case 0xDE:  // SBC A, d8
                instrSbcA_Imm8();
                return;
            case 0xDF:  // RST 0x18
                instrRST(0x18);
                return;
            case 0xE0:  // LDH (n), A
                instrLDH_Imm8_A();
                return;
            case 0xE1:  // POP HL
                instrPOP_R16(2);
                return;
            case 0xE2:  // LDH (C), A
                instrLDH_C_A();
                return;
            case 0xE3:
                THROW_ERROR("Opcode E3 not used");
                return;
            case 0xE4:
                THROW_ERROR("Opcode E4 not used");
                return;
            case 0xE5:  // PUSH HL
                instrPUSH_R16(2);
                return;
            case 0xE6:  // AND A, d8
                instrAndA_Imm8();
                return;
            case 0xE7:  // RST 0x20
                instrRST(0x20);
                return;
            case 0xE8:  // ADD SP, d8
                instrAddSP_Imm8();
                return;
            case 0xE9:  // JP (HL)
                instrJP_HL();
                return;
            case 0xEA:  // LD (a16), A
                instrLD_Imm16_A();
                return;
            case 0xEB:
                THROW_ERROR("Opcode EB not used");
                return;
            case 0xEC:
                THROW_ERROR("Opcode EC not used");
                return;
            case 0xED:
                THROW_ERROR("Opcode ED not used");
                return;
            case 0xEE:  // XOR A, d8
                instrXorA_Imm8();
                return;
            case 0xEF:  // RST 0x28
                instrRST(0x28);
                return;
            case 0xF0:  // LDH A, (n)
                instrLDH_A_Imm8();
                return;
            case 0xF1:  // POP AF
                instrPOP_R16(3);
                return;
            case 0xF2:  // LDH A, (C)
                instrLDH_A_C();
                return;
            case 0xF3:  // DI
                instrDI();
                return;
            case 0xF4:
                THROW_ERROR("Opcode F4 not used");
                return;
            case 0xF5:  // PUSH AF
                instrPUSH_R16(3);
                return;
            case 0xF6:  // OR A, d8
                instrOrA_Imm8();
                return;
            case 0xF7:  // RST 0x30
                instrRST(0x30);
                return;
            case 0xF8:  // LD HL, SP+d8
                instrLD_HL_SP_Imm8();
                return;
            case 0xF9:  // LD SP, HL
                instrLD_SP_HL();
                return;
            case 0xFA:  // LD A, (a16)
                instrLD_A_Imm16();
                return;
            case 0xFB:  // EI
                instrEI();
                return;
            case 0xFC:
                THROW_ERROR("Opcode FC not used");
                return;
            case 0xFD:
                THROW_ERROR("Opcode FD not used");
                return;
            case 0xFE:  // CP A, d8
                instrCpA_Imm8();
                return;
            case 0xFF:  // RST 0x38
                instrRST(0x38);
                return;
            default:
                THROW_ERROR("Invalid opcode in 0xC0-0xFF range");
        }
    }
}
inline void SM83::executeCB() {
    byte cbOpcode = fetch8();
    cycles += opcode_cycles_cb[cbOpcode];
    opcode = (opcode << 8) + cbOpcode;
    switch (cbOpcode >> 6) {
        case 0:
            switch (getBitRange(cbOpcode, 3, 3)) {
                case 0:
                    instrRLC_R8(getBitRange(cbOpcode, 0, 3));
                    break;
                case 1:
                    instrRRC_R8(getBitRange(cbOpcode, 0, 3));
                    break;
                case 2:
                    instrRL_R8(getBitRange(cbOpcode, 0, 3));
                    break;
                case 3:
                    instrRR_R8(getBitRange(cbOpcode, 0, 3));
                    break;
                case 4:
                    instrSLA_R8(getBitRange(cbOpcode, 0, 3));
                    break;
                case 5:
                    instrSRA_R8(getBitRange(cbOpcode, 0, 3));
                    break;
                case 6:
                    instrSWAP_R8(getBitRange(cbOpcode, 0, 3));
                    break;
                case 7:
                    instrSRL_R8(getBitRange(cbOpcode, 0, 3));
                    break;
                default:
                    THROW_ERROR("Invalid CB rotation opcode");
            }
            break;
        case 1:  // BIT b, r
            instrBIT_R8(getBitRange(cbOpcode, 3, 3),
                        getBitRange(cbOpcode, 0, 3));
            break;
        case 2:  // RES b, r
            instrRES_R8(getBitRange(cbOpcode, 3, 3),
                        getBitRange(cbOpcode, 0, 3));
            break;
        case 3:  // SET b, r
            instrSET_R8(getBitRange(cbOpcode, 3, 3),
                        getBitRange(cbOpcode, 0, 3));
            break;
        default:
            THROW_ERROR("Invalid CB opcode group");
    }
}

inline void SM83::instrNOP() {}

inline void SM83::instrLdImm16SP() {
    half addr = fetch16();
    memory->write(addr, static_cast<byte>(sp));
    memory->write(addr + 1, static_cast<byte>(sp >> 8));
}

inline void SM83::instrRLCA() {
    byte original = RA;
    setCarryFlag(original & 0x80);
    RA = (original << 1) | (original >> 7);
    setHalfCarryFlag(false);
    setNFlag(false);
    setZeroFlag(false);
}

inline void SM83::instrRRCA() {
    byte original = RA;
    setCarryFlag(original & 1);
    RA = ((original & 1) << 7) | (original >> 1);
    setHalfCarryFlag(false);
    setNFlag(false);
    setZeroFlag(false);
}

inline void SM83::instrRLA() {
    byte old_carry = getCarryFlag() ? 1 : 0;
    setCarryFlag(RA & 0x80);
    RA = (RA << 1) | old_carry;
    setHalfCarryFlag(false);
    setNFlag(false);
    setZeroFlag(false);
}

inline void SM83::instrRRA() {
    half temp = (RA & 1);
    temp |= (RA << 8);
    RA >>= 1;
    RA |= ((getCarryFlag() & 1) << 7);
    setHalfCarryFlag(false);
    setNFlag(false);
    setCarryFlag(temp & 1);
    setZeroFlag(false);
}

inline void SM83::instrLdiHL_A() {
    memory->write(getHL(), RA);
    setHL(getHL() + 1);
}

inline void SM83::instrDAA() {
    byte a_value = RA;
    bool half_carry = getHalfCarryFlag();
    bool carry = getCarryFlag();
    bool subtract = getNFlag();

    if (!subtract) {
        byte correction = 0;
        bool carry_out = false;
        if (carry || a_value > 0x99) {
            correction |= 0x60;
            carry_out = true;
        }
        if (half_carry || (a_value & 0x0F) > 0x09) {
            correction |= 0x06;
        }
        RA = static_cast<byte>(a_value + correction);
        setCarryFlag(carry_out);
    } else {
        byte correction = 0;
        if (half_carry) {
            correction |= 0x06;
        }
        if (carry) {
            correction |= 0x60;
        }
        RA = static_cast<byte>(a_value - correction);
        setCarryFlag(carry);
    }

    setHalfCarryFlag(false);
    setZeroFlag(RA == 0);
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
    pc += static_cast<int8_t>(memory->read(pc)) + 1;
}

inline void SM83::instrSTOP() {
    if (config.cgb_mode && config.speed_switch_armed) {
        config.speed_switch_armed = false;
        config.double_speed = !config.double_speed;
        if (memory != nullptr) {
            memory->sync_key_registers();
        }
        return;
    }
    halted = true;
}

inline void SM83::instrJR_Cond_Imm8(bool condition) {
    int8_t offset = static_cast<int8_t>(fetch8());
    if (condition) {
        pc += offset;
        ++cycles;
    }
}

inline void SM83::instrLdR16_Imm16(byte regIndex) {
    half value = fetch16();
    store16t1(regIndex, value);
}

inline void SM83::instrAddHL_R16(byte regIndex) {
    half value = load16t1(regIndex);
    setCarryFlag(static_cast<half>(getHL() + value) < getHL());
    setHalfCarryFlag((getHL() & 0xFFF) + (value & 0xFFF) > 0xFFF);
    setHL(getHL() + value);
    setNFlag(false);
}

inline void SM83::instrLdR16Mem_A(byte regIndex) {
    memory->write(load16t1(regIndex), RA);
}

inline void SM83::instrLdA_R16Mem(byte regIndex) {
    RA = memory->read(load16t1(regIndex));
}

inline void SM83::instrIncR16(byte regIndex) {
    half value = load16t1(regIndex);
    store16t1(regIndex, value + 1);
}

inline void SM83::instrDecR16(byte regIndex) {
    half value = load16t1(regIndex);
    store16t1(regIndex, value - 1);
}

inline void SM83::instrIncR8(byte reg) {
    byte temp = r8[reg];
    r8[reg]++;
    setZeroFlag(r8[reg] == 0);
    setNFlag(false);
    setHalfCarryFlag(halfCarryAdd(temp, 1));
}

inline void SM83::instrIncHL() {
    byte temp = memory->read(getHL());
    byte result = temp + 1;
    memory->write(getHL(), result);
    setZeroFlag(result == 0);
    setNFlag(false);
    setHalfCarryFlag(halfCarryAdd(temp, 1));
}

inline void SM83::instrDecR8(byte reg) {
    byte temp = r8[reg];
    r8[reg]--;
    setZeroFlag(r8[reg] == 0);
    setNFlag(true);
    setHalfCarryFlag(halfCarrySub(temp, r8[reg]));
}

inline void SM83::instrDecHL() {
    byte temp = memory->read(getHL());
    byte result = temp - 1;
    memory->write(getHL(), result);
    setZeroFlag(result == 0);
    setNFlag(true);
    setHalfCarryFlag(halfCarrySub(temp, 1));
}

inline void SM83::instrLdImm8_R8(byte reg) { memory->write(fetch8(), r8[reg]); }
inline void SM83::instrLdR8_Imm8(byte reg) { r8[reg] = fetch8(); }

inline void SM83::instrLdImm8_HL() { memory->write(getHL(), fetch8()); }

inline void SM83::instrHALT() { halted = true; }

inline void SM83::instrLdR8_FromHL(byte reg) {
    r8[reg] = memory->read(getHL());
}

inline void SM83::instrLdHL_FromR8(byte reg) {
    memory->write(getHL(), r8[reg]);
}

inline void SM83::instrLdR8_R8(byte dest, byte src) { r8[dest] = r8[src]; }

// Block 2: ALU operations (A with r8)

inline void SM83::instrAddA_R8(byte reg) {
    if (reg == 6) {
        byte value = memory->read(getHL());
        setHalfCarryFlag(halfCarryAdd(RA, value));
        setCarryFlag((static_cast<half>(RA) + static_cast<half>(value)) > 0xFF);
        RA += value;
        setZeroFlag(RA == 0);
        setNFlag(false);
        return;
    }

    setHalfCarryFlag(halfCarryAdd(RA, r8[reg]));
    setCarryFlag((static_cast<half>(RA) + static_cast<half>(r8[reg])) > 0xFF);
    RA += r8[reg];
    setZeroFlag(RA == 0);
    setNFlag(false);
}

inline void SM83::instrAdcA_R8(byte reg) {
    byte carry = getCarryFlag();
    if (reg == 6) {
        byte value = memory->read(getHL());
        setHalfCarryFlag(halfCarryAdd_WithCarry(RA, value));
        half result = static_cast<half>(RA) + static_cast<half>(value) + carry;
        RA = static_cast<byte>(result);
        setCarryFlag(result > 0xFF);
        setZeroFlag(RA == 0);
        setNFlag(false);
        return;
    }
    setHalfCarryFlag(halfCarryAdd_WithCarry(RA, r8[reg]));
    half result = static_cast<half>(RA) + static_cast<half>(r8[reg]) + carry;
    RA = static_cast<byte>(result);
    setCarryFlag(result > 0xFF);
    setZeroFlag(RA == 0);
    setNFlag(false);
}

inline void SM83::instrSubA_R8(byte reg) {
    setNFlag(true);
    if (reg == 6) {
        byte value = memory->read(getHL());
        setHalfCarryFlag(halfCarrySub(RA, value));
        setCarryFlag(RA < value);
        RA -= value;
        setZeroFlag(RA == 0);
        return;
    }
    setHalfCarryFlag(halfCarrySub(RA, r8[reg]));
    setCarryFlag(RA < r8[reg]);
    RA -= r8[reg];
    setZeroFlag(RA == 0);
}

inline void SM83::instrSbcA_R8(byte reg) {
    setNFlag(true);
    byte carry = getCarryFlag();
    if (reg == 6) {
        byte value = memory->read(getHL());
        setHalfCarryFlag(halfCarrySub_WithCarry(RA, value));
        half subtrahend = static_cast<half>(value) + carry;
        setCarryFlag(static_cast<half>(RA) < subtrahend);
        RA -= static_cast<byte>(subtrahend);
        setZeroFlag(RA == 0);
        return;
    }
    setHalfCarryFlag(halfCarrySub_WithCarry(RA, r8[reg]));
    half subtrahend = static_cast<half>(r8[reg]) + carry;
    setCarryFlag(static_cast<half>(RA) < subtrahend);
    RA -= static_cast<byte>(subtrahend);
    setZeroFlag(RA == 0);
}

inline void SM83::instrAndA_R8(byte reg) {
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

inline void SM83::instrXorA_R8(byte reg) {
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

inline void SM83::instrOrA_R8(byte reg) {
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

inline void SM83::instrCpA_R8(byte reg) {
    setNFlag(true);
    if (reg == 6) {
        byte value = memory->read(getHL());
        setCarryFlag(RA < value);
        setHalfCarryFlag(halfCarrySub(RA, value));
        setZeroFlag(RA == value);
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
    byte carry = getCarryFlag();
    setHalfCarryFlag(halfCarryAdd_WithCarry(RA, temp));
    half result = static_cast<half>(RA) + static_cast<half>(temp) + carry;
    RA = static_cast<byte>(result);
    setCarryFlag(result > 0xFF);
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
    byte carry = getCarryFlag();
    setNFlag(true);
    setHalfCarryFlag(halfCarrySub_WithCarry(RA, temp));
    half subtrahend = static_cast<half>(temp) + carry;
    setCarryFlag(static_cast<half>(RA) < subtrahend);
    RA -= static_cast<byte>(subtrahend);
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
    half temp = fetch16();
    memory->write(--sp, static_cast<byte>(pc >> 8));
    memory->write(--sp, static_cast<byte>(pc));
    pc = temp;
}

inline void SM83::instrLDH_C_A() {
    memory->write(addr(MemoryRegion::IO_REGISTERS) + RC, RA);
}

inline void SM83::instrLDH_Imm8_A() {
    memory->write(addr(MemoryRegion::IO_REGISTERS) | fetch8(), RA);
}

inline void SM83::instrLD_Imm16_A() { memory->write(fetch16(), RA); }

inline void SM83::instrLDH_A_C() {
    RA = memory->read(addr(MemoryRegion::IO_REGISTERS) | RC);
}

inline void SM83::instrLDH_A_Imm8() {
    RA = memory->read(addr(MemoryRegion::IO_REGISTERS) | fetch8());
}

inline void SM83::instrLD_A_Imm16() { RA = memory->read(fetch16()); }

inline void SM83::instrAddSP_Imm8() {
    setZeroFlag(0);
    setNFlag(0);
    int8_t temp = static_cast<int8_t>(fetch8());
    byte sp_low = static_cast<byte>(sp & 0xFF);
    byte temp_unsigned = static_cast<byte>(temp);
    setHalfCarryFlag(((sp_low & 0xF) + (temp_unsigned & 0xF)) > 0xF);
    setCarryFlag(
        (static_cast<half>(sp_low) + static_cast<half>(temp_unsigned)) > 0xFF);
    sp += temp;
}

inline void SM83::instrLD_HL_SP_Imm8() {
    int8_t temp = static_cast<int8_t>(fetch8());
    setZeroFlag(0);
    setNFlag(0);
    byte sp_low = static_cast<byte>(sp & 0xFF);
    byte temp_unsigned = static_cast<byte>(temp);
    setHalfCarryFlag(((sp_low & 0xF) + (temp_unsigned & 0xF)) > 0xF);
    setCarryFlag(
        (static_cast<half>(sp_low) + static_cast<half>(temp_unsigned)) > 0xFF);
    setHL(static_cast<int16_t>(sp) + temp);
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
    half temp = fetch16();
    if (condition) {
        pc = temp;
        ++cycles;
    }
}

inline void SM83::instrCALL_Cond_Imm16(bool condition) {
    half temp = fetch16();
    if (condition) {
        memory->write(--sp, static_cast<byte>(pc >> 8));
        memory->write(--sp, static_cast<byte>(pc));
        pc = temp;
        cycles += 3;
    }
}

inline void SM83::instrRST(byte target) {
    memory->write(--sp, static_cast<byte>(pc >> 8));
    memory->write(--sp, static_cast<byte>(pc));
    pc = target;
}

inline void SM83::instrPOP_R16(byte regIndex) {
    half value = (memory->read(sp)) | ((half)memory->read(half(sp + 1)) << 8);
    store16t2(regIndex, value);
    sp += 2;
}

inline void SM83::instrPUSH_R16(byte regIndex) {
    memory->write(--sp, static_cast<byte>(load16t2(regIndex) >> 8));
    memory->write(--sp, static_cast<byte>(load16t2(regIndex)));
}

inline void SM83::instrRLC_R8(byte reg) {
    setNFlag(0);
    setHalfCarryFlag(0);
    if (reg == 6) {
        byte value = memory->read(getHL());
        setCarryFlag(value & 0x80);
        value = (value << 1) | (value >> 7);
        memory->write(getHL(), value);
        setZeroFlag(value == 0);
        return;
    }
    byte original = r8[reg];
    setCarryFlag(original & 0x80);
    r8[reg] = (original << 1) | (original >> 7);
    setZeroFlag(r8[reg] == 0);
}

inline void SM83::instrRRC_R8(byte reg) {
    setNFlag(0);
    setHalfCarryFlag(0);
    if (reg == 6) {
        byte value = memory->read(getHL());
        setCarryFlag(value & 1);
        value = ((value & 1) << 7) | (value >> 1);
        memory->write(getHL(), value);
        setZeroFlag(value == 0);
        return;
    }
    byte original = r8[reg];
    setCarryFlag(original & 1);
    r8[reg] = ((original & 1) << 7) | (original >> 1);
    setZeroFlag(r8[reg] == 0);
}

inline void SM83::instrRL_R8(byte reg) {
    byte temp = getCarryFlag();
    setNFlag(0);
    setHalfCarryFlag(0);
    if (reg == 6) {
        byte value = memory->read(getHL());
        setCarryFlag(value >> 7);
        value = (temp & 1) | (value << 1);
        memory->write(getHL(), value);
        setZeroFlag(value == 0);
        return;
    }
    byte original = r8[reg];
    setCarryFlag(original >> 7);
    r8[reg] = (temp & 1) | (original << 1);
    setZeroFlag(r8[reg] == 0);
}

inline void SM83::instrRR_R8(byte reg) {
    byte temp = getCarryFlag();
    setNFlag(0);
    setHalfCarryFlag(0);
    if (reg == 6) {
        byte value = memory->read(getHL());
        setCarryFlag(value & 1);
        value = ((temp & 1) << 7) | (value >> 1);
        memory->write(getHL(), value);
        setZeroFlag(value == 0);
        return;
    }
    byte original = r8[reg];
    setCarryFlag(original & 1);
    r8[reg] = ((temp & 1) << 7) | (original >> 1);
    setZeroFlag(r8[reg] == 0);
}

inline void SM83::instrSLA_R8(byte reg) {
    setNFlag(0);
    setHalfCarryFlag(0);
    if (reg == 6) {
        byte value = memory->read(getHL());
        setCarryFlag(value >> 7);
        value = value << 1;
        memory->write(getHL(), value);
        setZeroFlag(value == 0);
        return;
    }
    byte original = r8[reg];
    setCarryFlag(original >> 7);
    r8[reg] = original << 1;
    setZeroFlag(r8[reg] == 0);
}

inline void SM83::instrSRA_R8(byte reg) {
    setNFlag(0);
    setHalfCarryFlag(0);
    if (reg == 6) {
        byte value = memory->read(getHL());
        setCarryFlag(value & 1);
        value = (value & 0x80) | (value >> 1);
        memory->write(getHL(), value);
        setZeroFlag(value == 0);
        return;
    }
    byte original = r8[reg];
    setCarryFlag(original & 1);
    r8[reg] = (original & 0x80) | (original >> 1);
    setZeroFlag(r8[reg] == 0);
}

inline void SM83::instrSWAP_R8(byte reg) {
    setNFlag(0);
    setHalfCarryFlag(0);
    setCarryFlag(0);
    if (reg == 6) {
        byte value = memory->read(getHL());
        value = (value >> 4) | (value << 4);
        memory->write(getHL(), value);
        setZeroFlag(value == 0);
        return;
    }
    r8[reg] = (r8[reg] >> 4) | (r8[reg] << 4);
    setZeroFlag(r8[reg] == 0);
}

inline void SM83::instrSRL_R8(byte reg) {
    setNFlag(0);
    setHalfCarryFlag(0);
    if (reg == 6) {
        byte value = memory->read(getHL());
        setCarryFlag(value & 1);
        value = value >> 1;
        memory->write(getHL(), value);
        setZeroFlag(value == 0);
        return;
    }
    byte original = r8[reg];
    setCarryFlag(original & 1);
    r8[reg] = original >> 1;
    setZeroFlag(r8[reg] == 0);
}

inline void SM83::instrBIT_R8(byte bit, byte reg) {
    setNFlag(0);
    setHalfCarryFlag(1);
    if (reg == 6) {
        setZeroFlag(!isBitSet(memory->read(getHL()), bit));
        return;
    }
    setZeroFlag(!isBitSet(r8[reg], bit));
}

inline void SM83::instrRES_R8(byte bit, byte reg) {
    if (reg == 6) {
        byte value = memory->read(getHL());
        memory->write(getHL(), clearBit(value, bit));
        return;
    }
    r8[reg] = clearBit(r8[reg], bit);
}

inline void SM83::instrSET_R8(byte bit, byte reg) {
    if (reg == 6) {
        byte value = memory->read(getHL());
        memory->write(getHL(), setBit(value, bit));
        return;
    }
    r8[reg] = setBit(r8[reg], bit);
}

inline void SM83::store16t1(byte reg16, half val) {
    if (reg16 > 3) {
        THROW_ERROR("store16t1: invalid register index");
    }
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
            THROW_ERROR("store16t1: invalid register index");
    }
}

inline void SM83::store16t2(byte reg16, half val) {
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
            THROW_ERROR("store16t2: invalid register index");
    }
}

inline half SM83::load16t1(byte reg16) {
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
            THROW_ERROR("load16t1: invalid register index");
    }
}

inline half SM83::load16t2(byte reg16) {
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
            THROW_ERROR("load16t2: invalid register index");
    }
}

inline void SM83::call_interrupt(half handler) {
    IME = false;
    memory->write(--sp, static_cast<byte>(pc >> 8));
    memory->write(--sp, static_cast<byte>(pc));
    pc = handler;
    cycles += 20;
}

inline void SM83::increment_timer() {
    // Increment DIV (updates every 256 cycles)
    divcounter++;
    if (divcounter >= 256) {
        divcounter = 0;
        memory->IOrange[addr(IORegister::DIV) -
                        addr(MemoryRegion::IO_REGISTERS)]++;
    }

    if (tima_overflow_cycles > 0) {
        tima_overflow_cycles--;
        if (tima_overflow_cycles == 0) {
            byte& tima_reg = memory->IOrange[addr(IORegister::TIMA) -
                                             addr(MemoryRegion::IO_REGISTERS)];
            byte& tma_reg = memory->IOrange[addr(IORegister::TMA) -
                                            addr(MemoryRegion::IO_REGISTERS)];
            if (!tima_written_this_cycle) {
                tima_reg = tma_reg;
            }
            auto& if_reg = memory->IOrange[addr(IORegister::IF) -
                                           addr(MemoryRegion::IO_REGISTERS)];
            if_reg = setBit(if_reg, 2);
        }
    }

    tima_written_this_cycle = false;

    tacreg =
        memory
            ->IOrange[addr(IORegister::TAC) - addr(MemoryRegion::IO_REGISTERS)];
    bool timer_enabled = isBitSet(tacreg, 2);

    // Get the appropriate bit from the divider based on TAC frequency select
    // TAC & 3 == 0: bit 9 (divcounter bit 1, since divcounter counts to 256 =
    // DIV) TAC & 3 == 1: bit 3 (divcounter bit -5, use divcounter directly) TAC
    // & 3 == 2: bit 5 (divcounter bit -3, use divcounter directly) TAC & 3 ==
    // 3: bit 7 (divcounter bit -1, use divcounter directly)

    byte timer_bit = 0;
    switch (tacreg & 0x3) {
        case 0:  // 4096 Hz - use divcounter bit 1 (every 512 cycles, falling
                 // edge = 1024)
            timer_bit = (divcounter & (1u << 9)) ? 1 : 0;
            break;
        case 1:  // 262144 Hz - check every 16 cycles
            timer_bit = (divcounter & (1u << 3)) ? 1 : 0;
            break;
        case 2:  // 65536 Hz - check every 64 cycles
            timer_bit = (divcounter & (1u << 5)) ? 1 : 0;
            break;
        case 3:  // 16384 Hz - check every 256 cycles
            timer_bit = (divcounter & (1u << 7)) ? 1 : 0;
            break;
    }

    byte current_timer_bit = timer_enabled ? timer_bit : 0;

    // Detect falling edge (1 -> 0 transition)
    if (prev_timer_bit && !current_timer_bit) {
        byte& tima_reg = memory->IOrange[addr(IORegister::TIMA) -
                                         addr(MemoryRegion::IO_REGISTERS)];
        byte new_tima = tima_reg + 1;

        if (new_tima == 0) {
            tima_overflow_cycles = 4;
        }

        tima_reg = new_tima;
    }

    prev_timer_bit = current_timer_bit;
}

#if GBC_CPU_DEBUG && !defined(__EMSCRIPTEN__)
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
        << (int)memory->IOrange[addr(IORegister::DIV) -
                                addr(MemoryRegion::IO_REGISTERS)]
        << '\n';
    std::ofstream("log.txt", std::ofstream::app)
        << "TAC: "
        << (int)memory->IOrange[addr(IORegister::TAC) -
                                addr(MemoryRegion::IO_REGISTERS)]
        << '\n';
}
#endif

}  // namespace GBC