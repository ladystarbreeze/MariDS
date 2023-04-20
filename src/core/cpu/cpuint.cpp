/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "cpuint.hpp"

#include <array>
#include <bit>

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

constexpr const char *regNames[] = {
    "R0", "R1", "R2" , "R3" , "R4" , "R5", "R6", "R7",
    "R8", "R9", "R10", "R11", "R12", "SP", "LR", "PC",
};

/* Condition codes */
enum Condition {
    EQ, NE, HS, LO, MI, PL, VS, VC,
    HI, LS, GE, LT, GT, LE, AL, NV,
};

/* Data Processing opcodes */
enum DPOpcode {
    AND, EOR, SUB, RSB, ADD, ADC, SBC, RSC,
    TST, TEQ, CMP, CMN, ORR, MOV, BIC, MVN,
};

std::array<void (*)(CPU *, u32), 4096> instrTableARM;

// Flag handlers

/* Sets bit op flags */
void setBitFlags(CPU *cpu, u32 c) {
    cpu->cpsr.n = c & (1 << 31);
    cpu->cpsr.z = !c;
    cpu->cpsr.c = cpu->cout; // Carry out of barrel shifter
    // V is left untouched
}

/* Sets SUB/RSB/CMP flags */
void setSubFlags(CPU *cpu, u32 a, u32 b, u32 c) {
    cpu->cpsr.n = c & (1 << 31);
    cpu->cpsr.z = !c;
    cpu->cpsr.c = a >= b;
    cpu->cpsr.v = ((a ^ b) & (1 << 31)) && ((a ^ c) & (1 << 31)); // Signed overflow if a & b have different signs, and a & c have different signs
}

// Barrel shifter

/* Rotates an 8-bit immediate by 2 * amt, sets carry out */
u32 rotateImm(CPU *cpu, u32 imm, u32 amt, bool isS) {
    if (!amt) return imm; // Don't set any flags

    amt <<= 1;

    if (isS) cpu->cout = imm & (1 << (amt - 1));

    return std::__rotr(imm, amt);
}

// Instruction handlers

/* Unhandled ARM state instruction */
void aUnhandledInstruction(CPU *cpu, u32 instr) {
    const auto opcode = ((instr >> 4) & 0xF) | ((instr >> 16) & 0xFF0);

    std::printf("[ARM%d      ] Unhandled instruction 0x%03X (0x%08X) @ 0x%08X\n", cpu->cpuID, opcode, instr, cpu->cpc);

    exit(0);
}

/* ARM state Branch (and Link) */
template<bool isLink>
void aBranch(CPU *cpu, u32 instr) {
    // Get offset
    const auto offset = (i32)(instr << 8) >> 6;

    const auto pc = cpu->get(CPUReg::PC);

    if constexpr (isLink) cpu->r[CPUReg::LR] = pc;

    cpu->r[CPUReg::PC] = pc + offset;

    if (doDisasm) {
        if constexpr (isLink) {
            std::printf("[ARM%d      ] [0x%08X] BL%s 0x%08X; LR = 0x%08X\n", cpu->cpuID, cpu->cpc, condNames[instr >> 28], cpu->r[CPUReg::PC], cpu->r[CPUReg::LR]);
        } else {
            std::printf("[ARM%d      ] [0x%08X] B%s 0x%08X\n", cpu->cpuID, cpu->cpc, condNames[instr >> 28], cpu->r[CPUReg::PC]);
        }
    }
}

/* ARM state data processing */
template<bool isImm, bool isImmShift, bool isRegShift>
void aDataProcessing(CPU *cpu, u32 instr) {
    if constexpr (isImmShift || isRegShift) {
        std::printf("[ARM%d      ] Unhandled data processing instruction 0x%08X\n", cpu->cpuID, instr);
        std::printf("IsImm = %d, IsImmShift = %d, IsRegShift = %d\n", isImm, isImmShift, isRegShift);

        exit(0);
    }

    // Get operands and opcode
    const auto rd = (instr >> 12) & 0xF;
    const auto rn = (instr >> 16) & 0xF;

    const auto opcode = (DPOpcode)((instr >> 21) & 0xF);

    bool isS = instr & (1 << 20);

    const auto op1 = cpu->get(rn);

    // Decode op2
    u32 op2;

    if constexpr (isImm) {
        // op2 is a rotated immediate
        const auto amt = (instr >> 8) & 0xF;
        const auto imm = instr & 0xFF;

        op2 = rotateImm(cpu, imm, amt, isS);
    } else {
        assert(false);
    }

    switch (opcode) {
        case DPOpcode::CMP:
            assert(isS); // Safeguard

            setSubFlags(cpu, op1, op2, op1 - op2);
            break;
        case DPOpcode::MOV:
            if (isS) setBitFlags(cpu, op2);

            cpu->r[rd] = op2;
            break;
        default:
            std::printf("[ARM%d      ] Unhandled Data Processing opcode %s\n", cpu->cpuID, dpNames[opcode]);

            exit(0);
    }

    if (doDisasm) {
        const auto cond = condNames[instr >> 28];

        if constexpr (isImm) {
            switch (opcode) {
                case DPOpcode::TST: case DPOpcode::TEQ: case DPOpcode::CMP: case DPOpcode::CMN:
                    std::printf("[ARM%d      ] [0x%08X] %s%s %s, 0x%08X; %s = 0x%08X\n", cpu->cpuID, cpu->cpc, dpNames[opcode], cond, regNames[rn], op2, regNames[rn], cpu->get(rn));
                    break;
                case DPOpcode::MOV: case DPOpcode::MVN:
                    std::printf("[ARM%d      ] [0x%08X] %s%s %s, 0x%08X; %s = 0x%08X\n", cpu->cpuID, cpu->cpc, dpNames[opcode], cond, regNames[rd], op2, regNames[rd], cpu->get(rd));
                    break;
                default:
                    assert(false);
            }

        } else {
            assert(false);
        }
    }
}

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

void decodeARM(CPU *cpu) {
    cpu->cpc = cpu->r[CPUReg::PC];

    assert(!(cpu->cpc & 3));

    // Fetch instruction, increment program counter
    const auto instr = cpu->read32(cpu->cpc);

    cpu->r[CPUReg::PC] += 4;

    // Check condition code
    const auto cond = Condition(instr >> 28);

    assert(cond != Condition::NV);

    if (!testCond(cpu, cond)) return; // Instruction failed condition, don't execute

    // Get opcode
    const auto opcode = ((instr >> 4) & 0xF) | ((instr >> 16) & 0xFF0);

    instrTableARM[opcode](cpu, instr);
}

void init() {
    // Populate instruction tables
    for (auto &i : instrTableARM) i = &aUnhandledInstruction;

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

    for (int i = 0xA00; i < 0xB00; i++) {
        instrTableARM[i | 0x000] = &aBranch<false>;
        instrTableARM[i | 0x100] = &aBranch<true>;
    }
}

void run(CPU *cpu, i64 runCycles) {
    for (auto c = runCycles; c > 0; c--) {
        assert(!cpu->cpsr.t); // TODO: implement THUMB

        decodeARM(cpu);
    }
}

}
