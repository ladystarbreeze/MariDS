/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "cpuint.hpp"

#include <array>

namespace nds::cpu::interpreter {

// Interpreter constants

constexpr auto doDisasm = true;

std::array<void (*)(CPU *, u32), 4096> instrTableARM;

// Instruction handlers

/* Unhandled ARM state instruction */
void aUnhandledInstruction(CPU *cpu, u32 instr) {
    const auto opcode = ((instr >> 4) & 0xF) | ((instr >> 16) & 0xFF0);

    std::printf("[ARM%d      ] Unhandled instruction 0x%03X (0x%08X) @ 0x%08X\n", cpu->cpuID, opcode, instr, cpu->r[CPUReg::PC] - 4);

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
            std::printf("[ARM%d      ] BL 0x%08X; LR = 0x%08X\n", cpu->cpuID, cpu->r[CPUReg::PC], cpu->r[CPUReg::LR]);
        } else {
            std::printf("[ARM%d      ] B 0x%08X\n", cpu->cpuID, cpu->r[CPUReg::PC]);
        }
    }
}

void decodeARM(CPU *cpu) {
    const auto pc = cpu->r[CPUReg::PC];

    assert(!(pc & 3));

    // Fetch instruction, increment program counter
    const auto instr = cpu->read32(pc);

    cpu->r[CPUReg::PC] += 4;

    // Get opcode
    const auto opcode = ((instr >> 4) & 0xF) | ((instr >> 16) & 0xFF0);

    instrTableARM[opcode](cpu, instr);
}

void init() {
    // Populate instruction tables
    for (auto &i : instrTableARM) i = &aUnhandledInstruction;

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
