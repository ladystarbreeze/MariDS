/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "cpuint.hpp"

#include <array>
#include <bit>
#include <string>

namespace nds::cpu::interpreter {

// Interpreter constants

constexpr auto doDisasm = true;

constexpr const char *condNames[] = {
    "EQ", "NE", "HS", "LO", "MI", "PL", "VS", "VC",
    "HI", "LS", "GE", "LT", "GT", "LE", ""  , "NV",
};

constexpr const char *dpNames[] = {
    "AND", "EOR", "SUB", "RSB", "ADD", "ADC", "SBC", "RSC",
    "TST", "TEQ", "CMP", "CMN", "ORR", "MOV", "BIC", "MVN",
};

constexpr const char *thumbDPNames[] = {
    "AND", "EOR", "LSL", "LSR", "ASR", "ADC", "SBC", "ROR",
    "TST", "NEG", "CMP", "CMN", "ORR", "MUL", "BIC", "MVN",
};

constexpr const char *extraLoadNames[] = {
    "N/A", "STRH", "LDRD", "STRD", "N/A", "LDRH", "LDRSB", "LDRSH",
};

constexpr const char *regNames[] = {
    "R0", "R1", "R2" , "R3" , "R4" , "R5", "R6", "R7",
    "R8", "R9", "R10", "R11", "R12", "SP", "LR", "PC",
};

constexpr const char *shiftNames[] = {
    "LSL", "LSR", "ASR", "ROR",
};

constexpr const char *thumbLoadNames[] = {
    "STR",
};

/* Condition codes */
enum Condition {
    EQ, NE, HS, LO, MI, PL, VS, VC,
    HI, LS, GE, LT, GT, LE, AL, NV,
};

/* Data Processing opcodes */
enum class DPOpcode {
    AND, EOR, SUB, RSB, ADD, ADC, SBC, RSC,
    TST, TEQ, CMP, CMN, ORR, MOV, BIC, MVN,
};

/* Extra loadstores */
enum ExtraLoadOpcode {
    STRH  = 1,
    LDRD  = 2,
    STRD  = 3,
    LDRH  = 5,
    LDRSB = 6,
    LDRSH = 7,
};

/* Data Processing opcodes */
enum class THUMBDPOpcode {
    AND, EOR, LSL, LSR, ASR, ADC, SBC, ROR,
    TST, NEG, CMP, CMN, ORR, MUL, BIC, MVN,
};

/* THUMB loadstores */
enum THUMBLoadOpcode {
    STR,
};

enum class ShiftType {
    LSL, LSR, ASR, ROR,
};

std::array<void (*)(CPU *, u32), 4096> instrTableARM;
std::array<void (*)(CPU *, u16), 1024> instrTableTHUMB;

std::string getReglist(u32 reglist) {
    assert(reglist);

    std::string list;

    while (reglist) {
        const auto i = std::__countr_zero(reglist);

        list += regNames[i];

        if (std::__popcount(reglist) != 1) list += ", ";

        reglist ^= 1 << i;
    }

    return list;
}

// Flag handlers

/* Returns true if the instruction passes the condition code test */
bool testCond(CPU *cpu, Condition cond) {
    auto &cpsr = cpu->cpsr;

    switch (cond) {
        case Condition::EQ: return cpsr.z;
        case Condition::NE: return !cpsr.z;
        case Condition::HS: return cpsr.c;
        case Condition::LO: return !cpsr.c;
        case Condition::MI: return cpsr.n;
        case Condition::PL: return !cpsr.n;
        case Condition::VS: return cpsr.v;
        case Condition::VC: return !cpsr.v;
        case Condition::HI: return cpsr.c && !cpsr.z;
        case Condition::LS: return cpsr.z && !cpsr.c;
        case Condition::GE: return cpsr.n == cpsr.v;
        case Condition::LT: return cpsr.n != cpsr.v;
        case Condition::GT: return (cpsr.n == cpsr.v) && !cpsr.z;
        case Condition::LE: return (cpsr.n != cpsr.v) && cpsr.z;
        case Condition::AL: return true;
        case Condition::NV: return true; // Requires special handling on ARM9
    }
}

/* Sets bit op flags */
void setBitFlags(CPU *cpu, u32 c) {
    cpu->cpsr.n = c & (1 << 31);
    cpu->cpsr.z = !c;
    cpu->cpsr.c = cpu->cout; // Carry out of barrel shifter
    // V is left untouched
}

/* Sets ADD/CMN flags */
void setAddFlags(CPU *cpu, u32 a, u32 b, u32 c) {
    cpu->cpsr.n = c & (1 << 31);
    cpu->cpsr.z = !c;
    cpu->cpsr.c = ((u32)-1 - a) < b;
    cpu->cpsr.v = !((a ^ b) & (1 << 31)) && ((a ^ c) & (1 << 31)); // Signed overflow if a & b have the same sign sign, and a & c have different signs
}

/* Sets SUB/RSB/CMP flags */
void setSubFlags(CPU *cpu, u32 a, u32 b, u32 c) {
    cpu->cpsr.n = c & (1 << 31);
    cpu->cpsr.z = !c;
    cpu->cpsr.c = a >= b;
    cpu->cpsr.v = ((a ^ b) & (1 << 31)) && ((a ^ c) & (1 << 31)); // Signed overflow if a & b have different signs, and a & c have different signs
}

// Barrel shifter

/* Performs an arithmetic right shift */
template<bool isImm>
u32 doASR(CPU *cpu, u32 data, u32 amt) {
    if (!amt) {
        if constexpr (isImm) { // Don't set any flags
            cpu->cout = cpu->cpsr.c;
        
            return data;
        }

        amt = 32;
    }

    if (amt >= 32) {
        const auto sign = data >> 31;

        cpu->cout = sign;

        return -1 * sign;
    }

    cpu->cout = (data >> (amt - 1)) & 1;

    return (i32)data >> amt;
}

/* Performs a left shift */
u32 doLSL(CPU *cpu, u32 data, u32 amt) {
    if (!amt) { // Don't set any flags
        cpu->cout = cpu->cpsr.c;
        
        return data;
    }

    if (amt >= 32) {
        cpu->cout = (amt > 32) ? false : data & 1;

        return 0;
    }

    cpu->cout = ((data << (amt - 1)) >> 31) & 1;

    return data << amt;
}

/* Performs a right shift */
template<bool isImm>
u32 doLSR(CPU *cpu, u32 data, u32 amt) {
    if (!amt) {
        if constexpr (isImm) { // Don't set any flags
            cpu->cout = cpu->cpsr.c;
        
            return data;
        }

        amt = 32;
    }

    if (amt >= 32) {
        cpu->cout = (amt > 32) ? false : data >> 31;

        return 0;
    }

    cpu->cout = (data >> (amt - 1)) & 1;

    return data >> amt;
}

/* Performs a right rotation */
template<bool isImm>
u32 doROR(CPU *cpu, u32 data, u32 amt) {
    if (!isImm || amt) {
        if (!amt) {
            cpu->cout = cpu->cpsr.c;

            return data;
        }

        amt &= 0x1F;

        data = (data >> (amt - 1)) | (data << (31 - amt));

        cpu->cout = data & 1;

        return (data >> 1) || (data << 31);
    } else {
        cpu->cout = data & 1;

        return (data >> 1) | (cpu->cpsr.c << 31);
    }
}

/* Shifts a register by an immediate/register-specified amount */
template<ShiftType stype, bool isImm>
u32 shift(CPU *cpu, u32 data, u32 amt) {
    switch (stype) {
        case ShiftType::LSL: return doLSL(cpu, data, amt & 0xFF);
        case ShiftType::LSR: return doLSR<isImm>(cpu, data, amt & 0xFF);
        case ShiftType::ASR: return doASR<isImm>(cpu, data, amt & 0xFF);
        case ShiftType::ROR: return doROR<isImm>(cpu, data, amt & 0xFF);
    }
}

/* Rotates an 8-bit immediate by 2 * amt, sets carry out */
u32 rotateImm(CPU *cpu, u32 imm, u32 amt) {
    if (!amt) { // Don't set any flags
        cpu->cout = cpu->cpsr.c;

        return imm;
    }

    amt <<= 1;

    cpu->cout = imm & (1 << (amt - 1));

    return std::__rotr(imm, amt);
}

// Instruction handlers (ARM)

/* Unhandled ARM state instruction */
void aUnhandledInstruction(CPU *cpu, u32 instr) {
    const auto opcode = ((instr >> 4) & 0xF) | ((instr >> 16) & 0xFF0);

    std::printf("[ARM%d      ] Unhandled instruction 0x%03X (0x%08X) @ 0x%08X\n", cpu->cpuID, opcode, instr, cpu->cpc);

    exit(0);
}

/* ARM state BLX */
template<bool isImm>
void aBLX(CPU *cpu, u32 instr) {
    // Get source register (for register BLX)
    const auto rm = instr & 0xF;

    const auto pc = cpu->get(CPUReg::PC);

    cpu->r[CPUReg::LR] = pc - 4;

    u32 offset;

    if constexpr (isImm) {
        // Get offset
        offset = (i32)(instr << 8) >> 6;

        cpu->r[CPUReg::PC] = pc + (offset | ((instr >> 23) & 2)); // PC += offset | (H << 1)

        cpu->cpsr.t = true;
    } else {
        assert(rm != CPUReg::PC);

        const auto target = cpu->r[rm];

        cpu->cpsr.t = target & 1;

        cpu->r[CPUReg::PC] = target & ~1;
    }

    if (doDisasm) {
        if constexpr (isImm) {
            std::printf("[ARM%d      ] [0x%08X] BLX 0x%08X; LR = 0x%08X\n", cpu->cpuID, cpu->cpc, cpu->r[CPUReg::PC], cpu->r[CPUReg::LR]);
        } else {
            std::printf("[ARM%d      ] [0x%08X] BLX %s; PC = 0x%08X, LR = 0x%08X\n", cpu->cpuID, cpu->cpc, regNames[rm], cpu->r[CPUReg::PC], cpu->r[CPUReg::LR]);
        }
    }
}

/* ARM state Branch (and Link) */
template<bool isLink>
void aBranch(CPU *cpu, u32 instr) {
    // Get offset
    const auto offset = (i32)(instr << 8) >> 6;

    const auto pc = cpu->get(CPUReg::PC);

    if constexpr (isLink) cpu->r[CPUReg::LR] = pc - 4;

    cpu->r[CPUReg::PC] = pc + offset;

    if (doDisasm) {
        const auto cond = condNames[instr >> 28];

        if constexpr (isLink) {
            std::printf("[ARM%d      ] [0x%08X] BL%s 0x%08X; LR = 0x%08X\n", cpu->cpuID, cpu->cpc, cond, cpu->r[CPUReg::PC], cpu->r[CPUReg::LR]);
        } else {
            std::printf("[ARM%d      ] [0x%08X] B%s 0x%08X\n", cpu->cpuID, cpu->cpc, cond, cpu->r[CPUReg::PC]);
        }
    }
}

/* ARM state Branch and Exchange */
void aBX(CPU *cpu, u32 instr) {
    assert(((instr >> 12) & 0xF) == CPUReg::PC);

    // Get source register
    const auto rm = instr & 0xF;

    assert(rm != CPUReg::PC);

    const auto target = cpu->r[rm];

    cpu->cpsr.t = target & 1;

    cpu->r[CPUReg::PC] = target & ~1;

    if (doDisasm) {
        const auto cond = condNames[instr >> 28];

        std::printf("[ARM%d      ] [0x%08X] BX%s %s; PC = 0x%08X\n", cpu->cpuID, cpu->cpc, cond, regNames[rm], cpu->r[CPUReg::PC]);
    }
}

/* MCR/MRC */
template<bool isL>
void aCoprocessorRegisterTransfer(CPU *cpu, u32 instr) {
    assert(cpu->cpuID == 9); // NDS7 doesn't have any usable coprocessors

    // Get operands
    const auto rn = (instr >> 16) & 0xF;
    const auto rd = (instr >> 12) & 0xF;
    const auto rm = (instr >>  0) & 0xF;

    assert(rd != CPUReg::PC);

    const auto opcode1 = (instr >> 21) & 7;
    const auto opcode2 = (instr >>  5) & 7;

    const auto cpNum = (instr >> 8) & 0xF;

    assert(cpNum == 15);

    // TODO: emulate CP15 accesses

    assert(cpu->cp15);

    const auto idx = (opcode1 << 12) | (rn << 8) | (rm << 4) | opcode2;

    if constexpr (isL) {
        cpu->r[rd] = cpu->cp15->get(idx);
    } else {
        cpu->cp15->set(idx, cpu->r[rd]);
    }

    if (doDisasm) {
        const auto cond = condNames[instr >> 28];

        if constexpr (isL) {
            std::printf("[ARM%d      ] [0x%08X] MRC%s P%u, %u, %s, C%s, C%s, %u\n", cpu->cpuID, cpu->cpc, cond, cpNum, opcode1, regNames[rd], regNames[rn], regNames[rm], opcode2);
        } else {
            std::printf("[ARM%d      ] [0x%08X] MCR%s P%u, %u, %s, C%s, C%s, %u\n", cpu->cpuID, cpu->cpc, cond, cpNum, opcode1, regNames[rd], regNames[rn], regNames[rm], opcode2);
        }
    }
}

/* ARM state data processing */
template<bool isImm, bool isImmShift, bool isRegShift>
void aDataProcessing(CPU *cpu, u32 instr) {
    if constexpr (isRegShift) {
        std::printf("[ARM%d      ] Unhandled data processing instruction 0x%08X\n", cpu->cpuID, instr);

        exit(0);
    }

    // Get operands and opcode
    const auto rd = (instr >> 12) & 0xF;
    const auto rn = (instr >> 16) & 0xF;
    const auto rs = (instr >>  8) & 0xF;
    const auto rm = (instr >>  0) & 0xF;

    assert(rd != CPUReg::PC);

    const auto opcode = (DPOpcode)((instr >> 21) & 0xF);

    bool isS = instr & (1 << 20);

    const auto op1 = cpu->get(rn);

    // Decode op2
    u32 op2;
    u32 amt; // Shift amount

    const auto stype = (ShiftType)((instr >> 5) & 3);

    if constexpr (isImm) {
        // op2 is a rotated immediate
        const auto amt = (instr >> 8) & 0xF;
        const auto imm = instr & 0xFF;

        op2 = rotateImm(cpu, imm, amt);
    } else {
        op2 = cpu->get(rm);

        if constexpr (isImmShift) {
            amt = (instr >> 7) & 0x1F;
        } else {
            assert(false);
        }

        switch (stype) {
            case ShiftType::LSL: op2 = shift<ShiftType::LSL, isImm>(cpu, op2, amt); break;
            case ShiftType::LSR: op2 = shift<ShiftType::LSR, isImm>(cpu, op2, amt); break;
            case ShiftType::ASR: op2 = shift<ShiftType::ASR, isImm>(cpu, op2, amt); break;
            case ShiftType::ROR: op2 = shift<ShiftType::ROR, isImm>(cpu, op2, amt); break;
        }
    }

    switch (opcode) {
        case DPOpcode::ADD:
            {
                const auto res = op1 + op2;

                if (isS) setAddFlags(cpu, op1, op2, res);

                cpu->r[rd] = res;
            }
            break;
        case DPOpcode::TEQ:
            assert(isS); // Safeguard

            setBitFlags(cpu, op1 ^ op2);
            break;
        case DPOpcode::CMP:
            assert(isS); // Safeguard

            setSubFlags(cpu, op1, op2, op1 - op2);
            break;
        case DPOpcode::MOV:
            if (isS) setBitFlags(cpu, op2);

            cpu->r[rd] = op2;
            break;
        default:
            std::printf("[ARM%d      ] Unhandled Data Processing opcode %s\n", cpu->cpuID, dpNames[static_cast<int>(opcode)]);

            exit(0);
    }

    if (doDisasm) {
        const auto cond = condNames[instr >> 28];

        if constexpr (isImm) {
            switch (opcode) {
                case DPOpcode::TST: case DPOpcode::TEQ: case DPOpcode::CMP: case DPOpcode::CMN:
                    std::printf("[ARM%d      ] [0x%08X] %s%s %s, 0x%08X; %s = 0x%08X\n", cpu->cpuID, cpu->cpc, dpNames[static_cast<int>(opcode)], cond, regNames[rn], op2, regNames[rn], cpu->get(rn));
                    break;
                case DPOpcode::MOV: case DPOpcode::MVN:
                    std::printf("[ARM%d      ] [0x%08X] %s%s %s, 0x%08X; %s = 0x%08X\n", cpu->cpuID, cpu->cpc, dpNames[static_cast<int>(opcode)], cond, regNames[rd], op2, regNames[rd], cpu->r[rd]);
                    break;
                default:
                    std::printf("[ARM%d      ] [0x%08X] %s%s %s, %s, 0x%08X; %s = 0x%08X\n", cpu->cpuID, cpu->cpc, dpNames[static_cast<int>(opcode)], cond, regNames[rd], regNames[rn], op2, regNames[rd], cpu->r[rd]);
                    break;
            }
        } else {
            if constexpr (isImmShift) {
                switch (opcode) {
                    case DPOpcode::TST: case DPOpcode::TEQ: case DPOpcode::CMP: case DPOpcode::CMN:
                        std::printf("[ARM%d      ] [0x%08X] %s%s %s, %s %s %u; %s = 0x%08X\n", cpu->cpuID, cpu->cpc, dpNames[static_cast<int>(opcode)], cond, regNames[rn], regNames[rm], shiftNames[static_cast<int>(stype)], amt, regNames[rn], cpu->get(rn));
                        break;
                    case DPOpcode::MOV: case DPOpcode::MVN:
                        std::printf("[ARM%d      ] [0x%08X] %s%s %s, %s %s %u; %s = 0x%08X\n", cpu->cpuID, cpu->cpc, dpNames[static_cast<int>(opcode)], cond, regNames[rd], regNames[rm], shiftNames[static_cast<int>(stype)], amt, regNames[rd], cpu->r[rd]);
                        break;
                    default:
                        assert(false);
                }
            } else {
                assert(false);
            }
        }
    }
}

/* ARM state extra load-stores */
template<ExtraLoadOpcode opcode, bool isP, bool isU, bool isI, bool isW>
void aExtraLoad(CPU *cpu, u32 instr) {
    // Get operands
    const auto rd = (instr >> 12) & 0xF;
    const auto rn = (instr >> 16) & 0xF;
    const auto rm = (instr >>  0) & 0xF;

    assert((rd != CPUReg::PC) && (rn != CPUReg::PC));

    auto addr = cpu->get(rn);
    auto data = cpu->get(rd);

    assert(!(!isP && isW)); // ???

    u32 offset;

    if constexpr (isI) {
        offset = ((instr >> 4) & 0xF0) | (instr & 0xF);
    } else {
        assert(rm != CPUReg::PC);

        offset = cpu->get(rm);
    }

    // Handle pre-index
    if constexpr (isP) {
        if constexpr (isU) {
            addr += offset;
        } else {
            addr -= offset;
        }
    }

    switch (opcode) {
        case ExtraLoadOpcode::STRH:
            assert(rd != CPUReg::PC);

            cpu->write16(addr, data);
            break;
        case ExtraLoadOpcode::LDRH:
            assert(rd != CPUReg::PC);

            cpu->r[rd] = cpu->read16(addr);
            break;
        default:
            std::printf("[ARM%d      ] Unhandled Extra Load opcode %s\n", cpu->cpuID, extraLoadNames[opcode]);

            exit(0);
    }

    // Handle post-index & writeback
    if (((opcode == ExtraLoadOpcode::STRH) || (opcode == ExtraLoadOpcode::STRD)) || (rn != rd)) {
        if constexpr (!isP) {
            assert(rn != CPUReg::PC); // Shouldn't happen

            if constexpr (isU) {
                addr += offset;
            } else {
                addr -= offset;
            }

            cpu->r[rn] = addr;
        } else if constexpr (isW) {
            cpu->r[rn] = addr;
        }
    }

    if (doDisasm) {
        constexpr const char *elNames[] = {
            "N/A", "H", "D", "D", "N/A", "H", "SB", "SH",
        };

        assert(isI);

        const auto cond = condNames[instr >> 28];

        if constexpr ((opcode == ExtraLoadOpcode::LDRH) || (opcode == ExtraLoadOpcode::LDRSB) || (opcode == ExtraLoadOpcode::LDRSB)) {
            std::printf("[ARM%d      ] [0x%08X] LDR%s%s %s, %s[%s%s, %s0x%02X%s; %s = [0x%08X] = 0x%08X\n", cpu->cpuID, cpu->cpc, cond, elNames[opcode], regNames[rd], (isW) ? "!" : "", regNames[rn], (!isP) ? "]" : "", (!isU) ? "-" : "", offset, (isP) ? "]" : "", regNames[rd], addr, cpu->get(rd));
        } else if constexpr (opcode == ExtraLoadOpcode::STRH) {
            std::printf("[ARM%d      ] [0x%08X] STR%s%s %s, %s[%s%s, %s0x%02X%s; [0x%08X] = %s = 0x%08X\n", cpu->cpuID, cpu->cpc, cond, elNames[opcode], regNames[rd], (isW) ? "!" : "", regNames[rn], (!isP) ? "]" : "", (!isU) ? "-" : "", offset, (isP) ? "]" : "", addr, regNames[rd], data);
        } else {
            assert(false); // TODO: LDRD/STRD
        }
    }
}

/* Move to Status from Register */
template<bool isR, bool isImm>
void aMSR(CPU *cpu, u32 instr) {
    if constexpr (isImm) {
        std::printf("[ARM%d      ] Unhandled immediate MSR instruction 0x%08X\n", cpu->cpuID, instr);

        exit(0);
    }

    // Get operands
    const auto rm = instr & 0xF;

    assert(rm != CPUReg::PC); // Would be a bit weird

    auto mask = (instr >> 16) & 0xF;

    if (cpu->cpsr.mode == CPUMode::USR) mask &= ~1; // User mode doesn't have privileges to change the control bits

    u32 op;

    if constexpr (isImm) {
        assert(false);
    } else {
        assert(rm != CPUReg::PC);

        op = cpu->get(rm);
    }

    if constexpr (isR) {
        assert(cpu->cspsr);
        
        cpu->cspsr->set(mask, op);
    } else {
        if (mask & 1) {
            const auto newMode = (CPUMode)(op & 0xF);

            cpu->changeMode(newMode);
        }

        cpu->cpsr.set(mask, op);
    }

    if (doDisasm) {
        constexpr const char *maskNames[] = {
            ""  , "C"  , "X"  , "CX"  ,
            "S" , "CS" , "XS" , "CXS" ,
            "F" , "CF" , "XF" , "CXF" ,
            "SF", "CSF", "XSF", "CXSF",
        };

        const auto cond = condNames[instr >> 28];

        if constexpr (isImm) {
            assert(false);
        } else {
            std::printf("[ARM%d      ] [0x%08X] MSR%s %sPSR_%s, %s; %sPSR = 0x%08X\n", cpu->cpuID, cpu->cpc, cond, (isR) ? "S" : "C", maskNames[mask], regNames[rm], (isR) ? "S" : "C", op);
        }
    }
}

/* ARM state Single Data Transfer */
template<bool isP, bool isU, bool isB, bool isW, bool isL, bool isImm>
void aSingleDataTransfer(CPU *cpu, u32 instr) {
    if constexpr (!isImm) {
        std::printf("[ARM%d      ] Unhandled register offset Single Data Transfer instruction 0x%08X\n", cpu->cpuID, instr);

        exit(0);
    }
    
    // Get operands
    const auto rd = (instr >> 12) & 0xF;
    const auto rn = (instr >> 16) & 0xF;

    auto addr = cpu->get(rn);
    auto data = cpu->get(rd);

    assert(!(!isP && isW)); // Unprivileged transfer?

    // Get offset
    u32 offset;

    if constexpr (isImm) {
        offset = instr & 0xFFF;
    } else {
        assert(false);
    }

    // Handle pre-index
    if constexpr (isP) {
        if constexpr (isU) {
            addr += offset;
        } else {
            addr -= offset;
        }
    }

    if constexpr (isL) {
        if constexpr (isB) {
            assert(rd != CPUReg::PC); // Shouldn't happen

            cpu->r[rd] = cpu->read8(addr);
        } else {
            assert(rd != CPUReg::PC); // Could happen, we'll implement this when we need it
            assert(!(addr & 3));      // See above

            cpu->r[rd] = cpu->read32(addr);
        }
    } else { // STR/STRB
        if (rd == 15) data += 4; // STR takes an extra cycle, which causes PC to be 12 bytes ahead

        if constexpr (isB) {
            cpu->write8(addr, data);
        } else {
            cpu->write32(addr & ~3, data); // Force-align address
        }
    }

    // Handle post-index & writeback
    if (!isL || (rn != rd)) {
        if constexpr (!isP) {
            assert(rn != CPUReg::PC); // Shouldn't happen

            if constexpr (isU) {
                addr += offset;
            } else {
                addr -= offset;
            }

            cpu->r[rn] = addr;
        } else if constexpr (isW) {
            cpu->r[rn] = addr;
        }
    }

    if (doDisasm) {
        const auto cond = condNames[instr >> 28];

        if constexpr (isL) {
            assert(isImm);

            std::printf("[ARM%d      ] [0x%08X] LDR%s%s %s, %s[%s%s, %s0x%03X%s; %s = [0x%08X] = 0x%08X\n", cpu->cpuID, cpu->cpc, cond, (isB) ? "B" : "", regNames[rd], (isW) ? "!" : "", regNames[rn], (!isP) ? "]" : "", (!isU) ? "-" : "", offset, (isP) ? "]" : "", regNames[rd], addr, cpu->get(rd));
        } else {
            assert(isImm);

            std::printf("[ARM%d      ] [0x%08X] STR%s%s %s, %s[%s%s, %s0x%03X%s; [0x%08X] = %s = 0x%08X\n", cpu->cpuID, cpu->cpc, cond, (isB) ? "B" : "", regNames[rd], (isW) ? "!" : "", regNames[rn], (!isP) ? "]" : "", (!isU) ? "-" : "", offset, (isP) ? "]" : "", addr, regNames[rd], data);
        }
    }
}

// Instruction handlers (THUMB)

/* Unhandled THUMB state instruction */
void tUnhandledInstruction(CPU *cpu, u16 instr) {
    const auto opcode = (instr >> 6) & 0x3FF;

    std::printf("[ARM%d:T    ] Unhandled instruction 0x%03X (0x%04X) @ 0x%08X\n", cpu->cpuID, opcode, instr, cpu->cpc);

    exit(0);
}

/* THUMB Branch and Link */
template<int H>
void tBranchLink(CPU *cpu, u16 instr) {
    static_assert((H >= 1) && (H < 4));

    // Get offset
    auto offset = (u32)(instr & 0x7FF);

    if constexpr (H == 2) { // Sign-extend offset, left shift by 12
        offset = (i32)(offset << 21) >> 9;

        cpu->r[CPUReg::LR] = cpu->get(CPUReg::PC) + offset;
    } else { // * 2
        offset <<= 1;

        const auto pc = cpu->r[CPUReg::PC];

        cpu->r[CPUReg::PC] = cpu->r[CPUReg::LR] + offset;
        cpu->r[CPUReg::LR] = pc | 1;

        if constexpr (H == 1) { // BLX
            cpu->r[CPUReg::PC] &= ~3;

            cpu->cpsr.t = false;
        }
    }

    if (doDisasm) {
        if constexpr (H == 2) {
            std::printf("[ARM%d:T    ] [0x%08X] BL; LR = 0x%08X\n", cpu->cpuID, cpu->cpc, cpu->r[CPUReg::LR]);
        } else {
            std::printf("[ARM%d:T    ] [0x%08X] BL%s 0x%08X; LR = 0x%08X\n", cpu->cpuID, cpu->cpc, (H == 1) ? "X" : "", cpu->r[CPUReg::PC], cpu->r[CPUReg::LR]);
        }
    }
}

/* THUMB Branch and Exchange */
template<bool isLink>
void tBranchExchange(CPU *cpu, u16 instr) {
    // Get source register
    const auto rm = (instr >> 3) & 0xF; // Includes H bit

    assert(rm != CPUReg::PC); // Stoobit

    if constexpr (isLink) {
        assert(rm != CPUReg::LR);

        cpu->r[CPUReg::LR] = cpu->r[CPUReg::PC];
    }

    cpu->r[CPUReg::PC] = cpu->r[rm] & ~1;

    cpu->cpsr.t = cpu->r[rm] & 1;

    if (doDisasm) {
        if constexpr (isLink) {
            std::printf("[ARM%d:T    ] [0x%08X] BLX %s; PC = 0x%08X, LR = 0x%08X\n", cpu->cpuID, cpu->cpc, regNames[rm], cpu->r[CPUReg::PC], cpu->r[CPUReg::LR]);
        } else {
            std::printf("[ARM%d:T    ] [0x%08X] BX %s; PC = 0x%08X\n", cpu->cpuID, cpu->cpc, regNames[rm], cpu->r[CPUReg::PC]);
        }
    }
}

/* THUMB conditional branch */
void tConditionalBranch(CPU *cpu, u16 instr) {
    // Get condition and offset
    const auto cond = (Condition)((instr >> 8) & 0xF);

    const auto offset = (i32)(i8)instr << 1;
    const auto target = cpu->get(CPUReg::PC) + offset;

    if (testCond(cpu, cond)) cpu->r[CPUReg::PC] = target;

    if (doDisasm) std::printf("[ARM%d:T    ] [0x%08X] B%s 0x%08X\n", cpu->cpuID, cpu->cpc, condNames[cond], target);
}

/* THUMB Data Processing (register) */
template<THUMBDPOpcode opcode>
void tDataProcessing(CPU *cpu, u16 instr) {
    // Get operands
    const auto rd = (instr >> 0) & 7;
    const auto rm = (instr >> 3) & 7;

    cpu->cout = cpu->cpsr.t; // Required for bit flags

    switch (opcode) {
        case THUMBDPOpcode::CMP:
            setSubFlags(cpu, cpu->r[rd], cpu->r[rm], cpu->r[rd] - cpu->r[rm]);
            break;
        case THUMBDPOpcode::MVN:
            cpu->r[rd] = ~cpu->r[rm];

            setBitFlags(cpu, cpu->r[rd]);
            break;
        default:
            std::printf("[ARM%d:T    ] Unhandled Data Processing opcode %s\n", cpu->cpuID, thumbDPNames[static_cast<int>(opcode)]);

            exit(0);
    }

    if (doDisasm) {
        switch (opcode) {
            case THUMBDPOpcode::TST: case THUMBDPOpcode::CMP: case THUMBDPOpcode::CMN:
                std::printf("[ARM%d:T    ] [0x%08X] %s %s, %s; %s = 0x%08X, %s = 0x%08X\n", cpu->cpuID, cpu->cpc, thumbDPNames[static_cast<int>(opcode)], regNames[rd], regNames[rm], regNames[rd], cpu->r[rd], regNames[rm], cpu->r[rm]);
                break;
            default:
                std::printf("[ARM%d:T    ] [0x%08X] %sS %s, %s; %s = 0x%08X\n", cpu->cpuID, cpu->cpc, thumbDPNames[static_cast<int>(opcode)], regNames[rd], regNames[rm], regNames[rd], cpu->r[rd]);
                break;
        }
    }
}

/* THUMB Data Processing (ADD/SUB/MOV/CMP) */
template<DPOpcode opcode>
void tDataProcessingLarge(CPU *cpu, u16 instr) {
    static_assert((opcode == DPOpcode::ADD) || (opcode == DPOpcode::SUB) || (opcode == DPOpcode::MOV) || (opcode == DPOpcode::CMP));

    // Get operands
    const auto rd = (instr >> 8) & 7;

    const auto imm = instr & 0xFF;

    cpu->cout = cpu->cpsr.t; // Required for bit flags

    switch (opcode) {
        case DPOpcode::ADD:
            {
                const auto res = cpu->r[rd] + imm;

                setAddFlags(cpu, cpu->r[rd], imm, res);

                cpu->r[rd] = res;
            }
            break;
        case DPOpcode::SUB:
            {
                const auto res = cpu->r[rd] - imm;

                setSubFlags(cpu, cpu->r[rd], imm, res);

                cpu->r[rd] = res;
            }
            break;
        case DPOpcode::MOV:
            setBitFlags(cpu, imm);

            cpu->r[rd] = imm;
            break;
        case DPOpcode::CMP:
            setSubFlags(cpu, cpu->r[rd], imm, cpu->r[rd] - imm);
            break;
    }

    if (doDisasm) std::printf("[ARM%d:T    ] [0x%08X] %s%s %s, %u; %s = 0x%08X\n", cpu->cpuID, cpu->cpc, dpNames[static_cast<int>(opcode)], (opcode != DPOpcode::CMP) ? "S" : "", regNames[rd], imm, regNames[rd], cpu->r[rd]);
}

/* Load from literal pool */
void tLoadFromPool(CPU *cpu, u16 instr) {
    // Get operands
    const auto rd = (instr >> 8) & 7;

    const auto offset = (instr & 0xFF) << 2;

    const auto addr = (cpu->get(CPUReg::PC) & ~3) + offset;

    cpu->r[rd] = cpu->read32(addr);

    if (doDisasm) std::printf("[ARM%d:T    ] [0x%08X] LDR %s, [0x%08X]; %s = 0x%08X\n", cpu->cpuID, cpu->cpc, regNames[rd], addr, regNames[rd], cpu->r[rd]);
}

/* Load/store halfword with immediate offset */
template<bool isL>
void tLoadHalfwordImmediateOffset(CPU *cpu, u16 instr) {
    // Get operands
    const auto rd = (instr >> 0) & 7;
    const auto rn = (instr >> 3) & 7;

    const auto offset = ((instr >> 6) & 0x1F) << 1;

    const auto addr = cpu->r[rn] + offset;

    if constexpr (isL) {
        cpu->r[rd] = cpu->read16(addr);
    } else {
        cpu->write16(addr, cpu->r[rd]);
    }

    if (doDisasm) {
        if constexpr (isL) {
            std::printf("[ARM%d:T    ] [0x%08X] LDRH %s, [%s, 0x%02X]; %s = [0x%08X] = 0x%08X\n", cpu->cpuID, cpu->cpc, regNames[rd], regNames[rn], offset, regNames[rd], addr, cpu->r[rd]);
        } else {
            std::printf("[ARM%d:T    ] [0x%08X] STRH %s, [%s, 0x%02X]; [0x%08X] = %s = 0x%04X\n", cpu->cpuID, cpu->cpc, regNames[rd], regNames[rn], offset, addr, regNames[rd], cpu->r[rd]);
        }
    }
}

/* Load/store with immediate offset */
template<bool isB, bool isL>
void tLoadImmediateOffset(CPU *cpu, u16 instr) {
    // Get operands
    const auto rd = (instr >> 0) & 7;
    const auto rn = (instr >> 3) & 7;

    auto offset = (instr >> 6) & 0x1F;

    if constexpr (!isB) offset <<= 2;

    const auto addr = cpu->r[rn] + offset;
    const auto data = cpu->r[rd];

    if constexpr (isL) {
        if constexpr (isB) {
            cpu->r[rd] = cpu->read8(addr);
        } else {
            assert(!(addr & 3));

            cpu->r[rd] = cpu->read32(addr);
        }
    } else {
        if constexpr (isB) {
            cpu->write8(addr, data);
        } else {
            cpu->write32(addr & ~3, data);
        }
    }

    if (doDisasm) {
        if constexpr (isL) {
            std::printf("[ARM%d:T    ] [0x%08X] LDR%s %s, [%s, %u]; %s = [0x%08X] = 0x%08X\n", cpu->cpuID, cpu->cpc, (isB) ? "B" : "", regNames[rd], regNames[rn], offset, regNames[rd], addr, cpu->r[rd]);
        } else {
            std::printf("[ARM%d:T    ] [0x%08X] STR%s %s, [%s, %u]; [0x%08X] = %s = 0x%08X\n", cpu->cpuID, cpu->cpc, (isB) ? "B" : "", regNames[rd], regNames[rn], offset, addr, regNames[rd], data);
        }
    }
}

/* Load/store with register offset */
template<THUMBLoadOpcode opcode>
void tLoadRegisterOffset(CPU *cpu, u16 instr) {
    // Get operands
    const auto rd = (instr >> 0) & 7;
    const auto rn = (instr >> 3) & 7;
    const auto rm = (instr >> 6) & 7;

    const auto addr = cpu->r[rn] + cpu->r[rm];
    const auto data = cpu->r[rd];

    switch (opcode) {
        case THUMBLoadOpcode::STR:
            cpu->write32(addr & ~3, data);
            break;
        default:
            std::printf("[ARM%d:T    ] Unhandled register offset load instruction 0x%04X @ 0x%08X\n", cpu->cpuID, instr, cpu->cpc);

            exit(0);
    }

    if (doDisasm) {
        if constexpr (opcode == THUMBLoadOpcode::STR) {
            std::printf("[ARM%d:T    ] [0x%08X] STR %s, [%s, %s]; [0x%08X] = %s = 0x%08X\n", cpu->cpuID, cpu->cpc, regNames[rd], regNames[rn], regNames[rm], addr, regNames[rd], data);
        } else {
            assert(false);
        }
    }
}

/* PUSH/POP */
template<bool isL, bool isR>
void tPop(CPU *cpu, u16 instr) {
    // Get reg list
    u32 reglist = (u8)instr;

    if constexpr (isR) reglist |= 1 << ((isL) ? CPUReg::PC : CPUReg::LR); // Add PC/LR to reglist

    assert(reglist); // Can happen, shouldn't happen though

    // Get base address from stack pointer, handle PUSH writeback
    if constexpr (!isL) { // All LDM/STM variants start at the *lowest* memory address, calculate this for PUSH (aka STMDB)
        cpu->r[CPUReg::SP] -= 4 * std::__popcount(reglist);
    }

    auto addr = cpu->r[CPUReg::SP];

    for (auto rlist = reglist; rlist != 0; ) {
        // Get next register
        const auto i = std::__countr_zero(rlist);

        if constexpr (isL) {
            cpu->r[i] = cpu->read32(addr);

            if (doDisasm) std::printf("R%u = [0x%08X] = 0x%08X\n", i, addr, cpu->r[i]);

            if ((i == CPUReg::PC) && (cpu->cpuID == 9)) {
                // Change processor state
                cpu->cpsr.t = cpu->r[CPUReg::PC & 1];

                cpu->r[CPUReg::PC] &= ~1;
            }
        } else {
            if (doDisasm) std::printf("[0x%08X] = R%u = 0x%08X\n", addr, i, cpu->r[i]);

            cpu->write32(addr, cpu->r[i]);
        }

        addr += 4;

        rlist ^= 1 << i;
    }

    // Handle POP writeback
    if constexpr (isL) cpu->r[CPUReg::SP] = addr;

    if (doDisasm) {
        const auto list = getReglist(reglist);
        if constexpr (isL && isR) {
            std::printf("[ARM%d:T    ] [0x%08X] %s {%s}; PC = 0x%08X\n", cpu->cpuID, cpu->cpc, (isL) ? "POP" : "PUSH", list.c_str(), cpu->r[CPUReg::PC]);
        } else {
            std::printf("[ARM%d:T    ] [0x%08X] %s {%s}\n", cpu->cpuID, cpu->cpc, (isL) ? "POP" : "PUSH", list.c_str());
        }
    }
}

/* Shift */
template<ShiftType stype>
void tShift(CPU *cpu, u16 instr) {
    // Get operands
    const auto rd = (instr >> 0) & 7;
    const auto rm = (instr >> 3) & 7;

    const auto amt = (instr >> 6) & 0x1F;

    cpu->r[rd] = shift<stype, true>(cpu, cpu->r[rm], amt);

    setBitFlags(cpu, cpu->r[rd]);

    if (doDisasm) std::printf("[ARM%d:T    ] [0x%08X] %sS %s, %s, %u; %s = 0x%08X\n", cpu->cpuID, cpu->cpc, shiftNames[static_cast<int>(stype)], regNames[rd], regNames[rm], amt, regNames[rd], cpu->r[rd]);
}

void decodeUnconditional(CPU *cpu, u32 instr) {
    assert(cpu->cpuID == 9);

    switch ((instr >> 24) & 0xF) {
        case 0xA:
        case 0xB:
            aBLX<true>(cpu, instr);
            break;
        default:
            std::printf("[ARM%d      ] Unhandled unconditional instruction 0x%08X @ 0x%08X\n", cpu->cpuID, instr, cpu->cpc);

            exit(0);
    }
}

void decodeARM(CPU *cpu) {
    cpu->cpc = cpu->r[CPUReg::PC];

    assert(!(cpu->cpc & 3));

    // Fetch instruction, increment program counter
    const auto instr = cpu->read32(cpu->cpc);

    cpu->r[CPUReg::PC] += 4;

    // Check condition code
    const auto cond = Condition(instr >> 28);

    if (cond == Condition::NV) return decodeUnconditional(cpu, instr);

    if (!testCond(cpu, cond)) return; // Instruction failed condition, don't execute

    // Get opcode
    const auto opcode = ((instr >> 4) & 0xF) | ((instr >> 16) & 0xFF0);

    instrTableARM[opcode](cpu, instr);
}

void decodeTHUMB(CPU *cpu) {
    cpu->cpc = cpu->r[CPUReg::PC];

    assert(!(cpu->cpc & 1));

    // Fetch instruction, increment program counter
    const auto instr = cpu->read16(cpu->cpc);

    cpu->r[CPUReg::PC] += 2;

    // Get opcode
    const auto opcode = (instr >> 6) & 0x3FF;

    instrTableTHUMB[opcode](cpu, instr);
}

void init() {
    // Populate instruction tables

    // ARM
    for (auto &i : instrTableARM  ) i = &aUnhandledInstruction;
    for (auto &i : instrTableTHUMB) i = &tUnhandledInstruction;

    for (int i = 0x000; i < 0x200; i++) {
        if (!(i & 1) && ((i & 0x191) != 0x100)) { // Don't include misc instructions
            // Immediate shift
            instrTableARM[i] = &aDataProcessing<false, true, false>;
        }

        if (((i & 9) == 1) && ((i & 0x199) != 0x101)) { // Don't include multiplies
            // Register shift
            instrTableARM[i] = &aDataProcessing<false, false, true>;
        }

        if (((i & 0x1B0) != 0x100) && ((i & 0x1B0) != 0x120)) { // Don't include UDF and MSR
            // Immediate DP
            instrTableARM[i | 0x200] = &aDataProcessing<true, false, false>;
        }
    }

    instrTableARM[0x00B] = &aExtraLoad<ExtraLoadOpcode::STRH , 0, 0, 0, 0>;
    instrTableARM[0x02B] = &aExtraLoad<ExtraLoadOpcode::STRH , 0, 0, 0, 1>;
    instrTableARM[0x04B] = &aExtraLoad<ExtraLoadOpcode::STRH , 0, 0, 1, 0>;
    instrTableARM[0x06B] = &aExtraLoad<ExtraLoadOpcode::STRH , 0, 0, 1, 1>;
    instrTableARM[0x08B] = &aExtraLoad<ExtraLoadOpcode::STRH , 0, 1, 0, 0>;
    instrTableARM[0x0AB] = &aExtraLoad<ExtraLoadOpcode::STRH , 0, 1, 0, 1>;
    instrTableARM[0x0CB] = &aExtraLoad<ExtraLoadOpcode::STRH , 0, 1, 1, 0>;
    instrTableARM[0x0EB] = &aExtraLoad<ExtraLoadOpcode::STRH , 0, 1, 1, 1>;
    instrTableARM[0x10B] = &aExtraLoad<ExtraLoadOpcode::STRH , 1, 0, 0, 0>;
    instrTableARM[0x12B] = &aExtraLoad<ExtraLoadOpcode::STRH , 1, 0, 0, 1>;
    instrTableARM[0x14B] = &aExtraLoad<ExtraLoadOpcode::STRH , 1, 0, 1, 0>;
    instrTableARM[0x16B] = &aExtraLoad<ExtraLoadOpcode::STRH , 1, 0, 1, 1>;
    instrTableARM[0x18B] = &aExtraLoad<ExtraLoadOpcode::STRH , 1, 1, 0, 0>;
    instrTableARM[0x1AB] = &aExtraLoad<ExtraLoadOpcode::STRH , 1, 1, 0, 1>;
    instrTableARM[0x1CB] = &aExtraLoad<ExtraLoadOpcode::STRH , 1, 1, 1, 0>;
    instrTableARM[0x1EB] = &aExtraLoad<ExtraLoadOpcode::STRH , 1, 1, 1, 1>;
    instrTableARM[0x01B] = &aExtraLoad<ExtraLoadOpcode::LDRH , 0, 0, 0, 0>;
    instrTableARM[0x03B] = &aExtraLoad<ExtraLoadOpcode::LDRH , 0, 0, 0, 1>;
    instrTableARM[0x05B] = &aExtraLoad<ExtraLoadOpcode::LDRH , 0, 0, 1, 0>;
    instrTableARM[0x07B] = &aExtraLoad<ExtraLoadOpcode::LDRH , 0, 0, 1, 1>;
    instrTableARM[0x09B] = &aExtraLoad<ExtraLoadOpcode::LDRH , 0, 1, 0, 0>;
    instrTableARM[0x0BB] = &aExtraLoad<ExtraLoadOpcode::LDRH , 0, 1, 0, 1>;
    instrTableARM[0x0DB] = &aExtraLoad<ExtraLoadOpcode::LDRH , 0, 1, 1, 0>;
    instrTableARM[0x0FB] = &aExtraLoad<ExtraLoadOpcode::LDRH , 0, 1, 1, 1>;
    instrTableARM[0x11B] = &aExtraLoad<ExtraLoadOpcode::LDRH , 1, 0, 0, 0>;
    instrTableARM[0x13B] = &aExtraLoad<ExtraLoadOpcode::LDRH , 1, 0, 0, 1>;
    instrTableARM[0x15B] = &aExtraLoad<ExtraLoadOpcode::LDRH , 1, 0, 1, 0>;
    instrTableARM[0x17B] = &aExtraLoad<ExtraLoadOpcode::LDRH , 1, 0, 1, 1>;
    instrTableARM[0x19B] = &aExtraLoad<ExtraLoadOpcode::LDRH , 1, 1, 0, 0>;
    instrTableARM[0x1BB] = &aExtraLoad<ExtraLoadOpcode::LDRH , 1, 1, 0, 1>;
    instrTableARM[0x1DB] = &aExtraLoad<ExtraLoadOpcode::LDRH , 1, 1, 1, 0>;
    instrTableARM[0x1FB] = &aExtraLoad<ExtraLoadOpcode::LDRH , 1, 1, 1, 1>;

    //instrTableARM[0x00D] = &aExtraLoad<ExtraLoadOpcode::LDRD , 0, 0, 0, 0>;
    //instrTableARM[0x00F] = &aExtraLoad<ExtraLoadOpcode::STRD , 0, 0, 0, 0>;
    //instrTableARM[0x01D] = &aExtraLoad<ExtraLoadOpcode::LDRSB, 0, 0, 0, 0>;
    //instrTableARM[0x01F] = &aExtraLoad<ExtraLoadOpcode::LDRSH, 0, 0, 0, 0>;

    instrTableARM[0x120] = &aMSR<false, false>;
    instrTableARM[0x160] = &aMSR<true , false>;
    
    instrTableARM[0x121] = &aBX;
    instrTableARM[0x123] = &aBLX<false>;

    for (int i = 0x400; i < 0x600; i++) {
        // Immediate SDT
        switch ((i >> 4) & 0x1F) {
            case 0x00: instrTableARM[i] = &aSingleDataTransfer<0, 0, 0, 0, 0, 1>; break;
            case 0x01: instrTableARM[i] = &aSingleDataTransfer<0, 0, 0, 0, 1, 1>; break;
            case 0x02: instrTableARM[i] = &aSingleDataTransfer<0, 0, 0, 1, 0, 1>; break;
            case 0x03: instrTableARM[i] = &aSingleDataTransfer<0, 0, 0, 1, 1, 1>; break;
            case 0x04: instrTableARM[i] = &aSingleDataTransfer<0, 0, 1, 0, 0, 1>; break;
            case 0x05: instrTableARM[i] = &aSingleDataTransfer<0, 0, 1, 0, 1, 1>; break;
            case 0x06: instrTableARM[i] = &aSingleDataTransfer<0, 0, 1, 1, 0, 1>; break;
            case 0x07: instrTableARM[i] = &aSingleDataTransfer<0, 0, 1, 1, 1, 1>; break;
            case 0x08: instrTableARM[i] = &aSingleDataTransfer<0, 1, 0, 0, 0, 1>; break;
            case 0x09: instrTableARM[i] = &aSingleDataTransfer<0, 1, 0, 0, 1, 1>; break;
            case 0x0A: instrTableARM[i] = &aSingleDataTransfer<0, 1, 0, 1, 0, 1>; break;
            case 0x0B: instrTableARM[i] = &aSingleDataTransfer<0, 1, 0, 1, 1, 1>; break;
            case 0x0C: instrTableARM[i] = &aSingleDataTransfer<0, 1, 1, 0, 0, 1>; break;
            case 0x0D: instrTableARM[i] = &aSingleDataTransfer<0, 1, 1, 0, 1, 1>; break;
            case 0x0E: instrTableARM[i] = &aSingleDataTransfer<0, 1, 1, 1, 0, 1>; break;
            case 0x0F: instrTableARM[i] = &aSingleDataTransfer<0, 1, 1, 1, 1, 1>; break;
            case 0x10: instrTableARM[i] = &aSingleDataTransfer<1, 0, 0, 0, 0, 1>; break;
            case 0x11: instrTableARM[i] = &aSingleDataTransfer<1, 0, 0, 0, 1, 1>; break;
            case 0x12: instrTableARM[i] = &aSingleDataTransfer<1, 0, 0, 1, 0, 1>; break;
            case 0x13: instrTableARM[i] = &aSingleDataTransfer<1, 0, 0, 1, 1, 1>; break;
            case 0x14: instrTableARM[i] = &aSingleDataTransfer<1, 0, 1, 0, 0, 1>; break;
            case 0x15: instrTableARM[i] = &aSingleDataTransfer<1, 0, 1, 0, 1, 1>; break;
            case 0x16: instrTableARM[i] = &aSingleDataTransfer<1, 0, 1, 1, 0, 1>; break;
            case 0x17: instrTableARM[i] = &aSingleDataTransfer<1, 0, 1, 1, 1, 1>; break;
            case 0x18: instrTableARM[i] = &aSingleDataTransfer<1, 1, 0, 0, 0, 1>; break;
            case 0x19: instrTableARM[i] = &aSingleDataTransfer<1, 1, 0, 0, 1, 1>; break;
            case 0x1A: instrTableARM[i] = &aSingleDataTransfer<1, 1, 0, 1, 0, 1>; break;
            case 0x1B: instrTableARM[i] = &aSingleDataTransfer<1, 1, 0, 1, 1, 1>; break;
            case 0x1C: instrTableARM[i] = &aSingleDataTransfer<1, 1, 1, 0, 0, 1>; break;
            case 0x1D: instrTableARM[i] = &aSingleDataTransfer<1, 1, 1, 0, 1, 1>; break;
            case 0x1E: instrTableARM[i] = &aSingleDataTransfer<1, 1, 1, 1, 0, 1>; break;
            case 0x1F: instrTableARM[i] = &aSingleDataTransfer<1, 1, 1, 1, 1, 1>; break;
        }

        // Register SDT
        if (!(i & 1)) {
            switch ((i >> 4) & 0x1F) {
                case 0x00: instrTableARM[i | 0x200] = &aSingleDataTransfer<0, 0, 0, 0, 0, 0>; break;
                case 0x01: instrTableARM[i | 0x200] = &aSingleDataTransfer<0, 0, 0, 0, 1, 0>; break;
                case 0x02: instrTableARM[i | 0x200] = &aSingleDataTransfer<0, 0, 0, 1, 0, 0>; break;
                case 0x03: instrTableARM[i | 0x200] = &aSingleDataTransfer<0, 0, 0, 1, 1, 0>; break;
                case 0x04: instrTableARM[i | 0x200] = &aSingleDataTransfer<0, 0, 1, 0, 0, 0>; break;
                case 0x05: instrTableARM[i | 0x200] = &aSingleDataTransfer<0, 0, 1, 0, 1, 0>; break;
                case 0x06: instrTableARM[i | 0x200] = &aSingleDataTransfer<0, 0, 1, 1, 0, 0>; break;
                case 0x07: instrTableARM[i | 0x200] = &aSingleDataTransfer<0, 0, 1, 1, 1, 0>; break;
                case 0x08: instrTableARM[i | 0x200] = &aSingleDataTransfer<0, 1, 0, 0, 0, 0>; break;
                case 0x09: instrTableARM[i | 0x200] = &aSingleDataTransfer<0, 1, 0, 0, 1, 0>; break;
                case 0x0A: instrTableARM[i | 0x200] = &aSingleDataTransfer<0, 1, 0, 1, 0, 0>; break;
                case 0x0B: instrTableARM[i | 0x200] = &aSingleDataTransfer<0, 1, 0, 1, 1, 0>; break;
                case 0x0C: instrTableARM[i | 0x200] = &aSingleDataTransfer<0, 1, 1, 0, 0, 0>; break;
                case 0x0D: instrTableARM[i | 0x200] = &aSingleDataTransfer<0, 1, 1, 0, 1, 0>; break;
                case 0x0E: instrTableARM[i | 0x200] = &aSingleDataTransfer<0, 1, 1, 1, 0, 0>; break;
                case 0x0F: instrTableARM[i | 0x200] = &aSingleDataTransfer<0, 1, 1, 1, 1, 0>; break;
                case 0x10: instrTableARM[i | 0x200] = &aSingleDataTransfer<1, 0, 0, 0, 0, 0>; break;
                case 0x11: instrTableARM[i | 0x200] = &aSingleDataTransfer<1, 0, 0, 0, 1, 0>; break;
                case 0x12: instrTableARM[i | 0x200] = &aSingleDataTransfer<1, 0, 0, 1, 0, 0>; break;
                case 0x13: instrTableARM[i | 0x200] = &aSingleDataTransfer<1, 0, 0, 1, 1, 0>; break;
                case 0x14: instrTableARM[i | 0x200] = &aSingleDataTransfer<1, 0, 1, 0, 0, 0>; break;
                case 0x15: instrTableARM[i | 0x200] = &aSingleDataTransfer<1, 0, 1, 0, 1, 0>; break;
                case 0x16: instrTableARM[i | 0x200] = &aSingleDataTransfer<1, 0, 1, 1, 0, 0>; break;
                case 0x17: instrTableARM[i | 0x200] = &aSingleDataTransfer<1, 0, 1, 1, 1, 0>; break;
                case 0x18: instrTableARM[i | 0x200] = &aSingleDataTransfer<1, 1, 0, 0, 0, 0>; break;
                case 0x19: instrTableARM[i | 0x200] = &aSingleDataTransfer<1, 1, 0, 0, 1, 0>; break;
                case 0x1A: instrTableARM[i | 0x200] = &aSingleDataTransfer<1, 1, 0, 1, 0, 0>; break;
                case 0x1B: instrTableARM[i | 0x200] = &aSingleDataTransfer<1, 1, 0, 1, 1, 0>; break;
                case 0x1C: instrTableARM[i | 0x200] = &aSingleDataTransfer<1, 1, 1, 0, 0, 0>; break;
                case 0x1D: instrTableARM[i | 0x200] = &aSingleDataTransfer<1, 1, 1, 0, 1, 0>; break;
                case 0x1E: instrTableARM[i | 0x200] = &aSingleDataTransfer<1, 1, 1, 1, 0, 0>; break;
                case 0x1F: instrTableARM[i | 0x200] = &aSingleDataTransfer<1, 1, 1, 1, 1, 0>; break;
            }
        }
    }

    for (int i = 0xA00; i < 0xB00; i++) {
        instrTableARM[i | 0x000] = &aBranch<false>;
        instrTableARM[i | 0x100] = &aBranch<true>;
    }

    for (int i = 0xE00; i < 0xF00; i++) {
        if (i & 1) {
            if (i & (1 << 4)) {
                instrTableARM[i] = &aCoprocessorRegisterTransfer<true>;
            } else {
                instrTableARM[i] = &aCoprocessorRegisterTransfer<false>;
            }
        }
    }

    // THUMB
    for (int i = 0x000; i < 0x080; i++) {
        switch ((i >> 5) & 3) {
            case 0: instrTableTHUMB[i] = &tShift<ShiftType::LSL>; break;
            case 1: instrTableTHUMB[i] = &tShift<ShiftType::LSR>; break;
            case 2: instrTableTHUMB[i] = &tShift<ShiftType::ASR>; break;
        }
    }

    for (int i = 0x080; i < 0x0A0; i++) {
        instrTableTHUMB[i | (0 << 5)] = &tDataProcessingLarge<DPOpcode::MOV>;
        instrTableTHUMB[i | (1 << 5)] = &tDataProcessingLarge<DPOpcode::CMP>;
        instrTableTHUMB[i | (2 << 5)] = &tDataProcessingLarge<DPOpcode::ADD>;
        instrTableTHUMB[i | (3 << 5)] = &tDataProcessingLarge<DPOpcode::SUB>;
    }

    instrTableTHUMB[0x100] = &tDataProcessing<THUMBDPOpcode::AND>;
    instrTableTHUMB[0x101] = &tDataProcessing<THUMBDPOpcode::EOR>;
    instrTableTHUMB[0x102] = &tDataProcessing<THUMBDPOpcode::LSL>;
    instrTableTHUMB[0x103] = &tDataProcessing<THUMBDPOpcode::LSR>;
    instrTableTHUMB[0x104] = &tDataProcessing<THUMBDPOpcode::ASR>;
    instrTableTHUMB[0x105] = &tDataProcessing<THUMBDPOpcode::ADC>;
    instrTableTHUMB[0x106] = &tDataProcessing<THUMBDPOpcode::SBC>;
    instrTableTHUMB[0x107] = &tDataProcessing<THUMBDPOpcode::ROR>;
    instrTableTHUMB[0x108] = &tDataProcessing<THUMBDPOpcode::TST>;
    instrTableTHUMB[0x109] = &tDataProcessing<THUMBDPOpcode::NEG>;
    instrTableTHUMB[0x10A] = &tDataProcessing<THUMBDPOpcode::CMP>;
    instrTableTHUMB[0x10B] = &tDataProcessing<THUMBDPOpcode::CMN>;
    instrTableTHUMB[0x10C] = &tDataProcessing<THUMBDPOpcode::ORR>;
    instrTableTHUMB[0x10D] = &tDataProcessing<THUMBDPOpcode::MUL>;
    instrTableTHUMB[0x10E] = &tDataProcessing<THUMBDPOpcode::BIC>;
    instrTableTHUMB[0x10F] = &tDataProcessing<THUMBDPOpcode::MVN>;

    instrTableTHUMB[0x11C] = &tBranchExchange<false>;
    instrTableTHUMB[0x11D] = &tBranchExchange<false>;
    instrTableTHUMB[0x11E] = &tBranchExchange<true>;
    instrTableTHUMB[0x11F] = &tBranchExchange<true>;

    for (int i = 0x120; i < 0x140; i++) {
        instrTableTHUMB[i] = &tLoadFromPool;
    }

    for (int i = 0x140; i < 0x180; i++) {
        switch ((i >> 9) & 7) {
            case 0: instrTableTHUMB[i] = &tLoadRegisterOffset<THUMBLoadOpcode::STR>; break;
        }
    }

    for (int i = 0x180; i < 0x1A0; i++) {
        instrTableTHUMB[i | (0 << 5)] = &tLoadImmediateOffset<0, 0>;
        instrTableTHUMB[i | (1 << 5)] = &tLoadImmediateOffset<0, 1>;
        instrTableTHUMB[i | (2 << 5)] = &tLoadImmediateOffset<1, 0>;
        instrTableTHUMB[i | (3 << 5)] = &tLoadImmediateOffset<1, 1>;
    }

    for (int i = 0x200; i < 0x240; i++) {
        instrTableTHUMB[i] = (i & (1 << 5)) ? &tLoadHalfwordImmediateOffset<true> : &tLoadHalfwordImmediateOffset<false>;
    }

    instrTableTHUMB[0x2D0] = &tPop<0, 0>;
    instrTableTHUMB[0x2D1] = &tPop<0, 0>;
    instrTableTHUMB[0x2D2] = &tPop<0, 0>;
    instrTableTHUMB[0x2D3] = &tPop<0, 0>;
    instrTableTHUMB[0x2D4] = &tPop<0, 1>;
    instrTableTHUMB[0x2D5] = &tPop<0, 1>;
    instrTableTHUMB[0x2D6] = &tPop<0, 1>;
    instrTableTHUMB[0x2D7] = &tPop<0, 1>;
    instrTableTHUMB[0x2F0] = &tPop<1, 0>;
    instrTableTHUMB[0x2F1] = &tPop<1, 0>;
    instrTableTHUMB[0x2F2] = &tPop<1, 0>;
    instrTableTHUMB[0x2F3] = &tPop<1, 0>;
    instrTableTHUMB[0x2F4] = &tPop<1, 1>;
    instrTableTHUMB[0x2F5] = &tPop<1, 1>;
    instrTableTHUMB[0x2F6] = &tPop<1, 1>;
    instrTableTHUMB[0x2F7] = &tPop<1, 1>;

    for (int i = 0x340; i < 0x380; i++) {
        if (((i >> 2) != 0xDE) && ((i >> 2) != 0xDF)) {
            instrTableTHUMB[i] = &tConditionalBranch;
        }
    }

    for (int i = 0x380; i < 0x400; i++) {
        switch ((i >> 5) & 3) {
            case 1: instrTableTHUMB[i] = &tBranchLink<1>; break;
            case 2: instrTableTHUMB[i] = &tBranchLink<2>; break;
            case 3: instrTableTHUMB[i] = &tBranchLink<3>; break;
        }
    }
}

void run(CPU *cpu, i64 runCycles) {
    for (auto c = runCycles; c > 0; c--) {
        (cpu->cpsr.t) ? decodeTHUMB(cpu) : decodeARM(cpu);

        assert(cpu->r[CPUReg::PC]);
    }
}

}
