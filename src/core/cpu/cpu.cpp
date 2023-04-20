/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "cpu.hpp"

#include <algorithm>
#include <cstring>

#include "../bus.hpp"

namespace nds::cpu {

/* Exception vector base addresses */
enum class VectorBase : u32 {
    ARM7 = 0,
    ARM9 = 0xFFFF0000,
};

CPU::CPU(int cpuID) {
    assert((cpuID == 7) || (cpuID == 9));

    this->cpuID = cpuID;

    std::memset(&r, 0, sizeof(r));

    if (cpuID == 7) {
        r[CPUReg::PC] = static_cast<u32>(VectorBase::ARM7);

        assert(false);
    } else {
        r[CPUReg::PC] = static_cast<u32>(VectorBase::ARM9);

        read8  = &bus::read8ARM9;
        read32 = &bus::read32ARM9;

        write32 = &bus::write32ARM9;
    }

    // Set initial CPSR

    cpsr.mode = CPUMode::USR; // Required to make initial mode change work

    cpsr.t = false;
    cpsr.f = true;
    cpsr.i = true;

    changeMode(CPUMode::SVC);

    std::printf("[ARM%d      ] OK!\n", cpuID);
}

CPU::~CPU() {}

/* Returns PC + 4 or a register without offset */
u32 CPU::get(u32 idx) {
    assert(idx < 16);

    return (idx == 15) ? r[idx] + 4 : r[idx];
}

/* Changes CPU mode, swaps register banks */
void CPU::changeMode(CPUMode newMode) {
    if (cpsr.mode != newMode) {
        // Save old register bank
        switch (cpsr.mode) {
            case CPUMode::USR:
            case CPUMode::SYS:
                break; // Do nothing
            case CPUMode::FIQ:
                std::swap(rFIQ[0], r[8]);
                std::swap(rFIQ[1], r[9]);
                std::swap(rFIQ[2], r[10]);
                std::swap(rFIQ[3], r[11]);
                std::swap(rFIQ[4], r[12]);

                std::swap(spFIQ, r[CPUReg::SP]);
                std::swap(lrFIQ, r[CPUReg::LR]);
                break;
            case CPUMode::IRQ:
                std::swap(spIRQ, r[CPUReg::SP]);
                std::swap(lrIRQ, r[CPUReg::LR]);
                break;
            case CPUMode::SVC:
                std::swap(spSVC, r[CPUReg::SP]);
                std::swap(lrSVC, r[CPUReg::LR]);
                break;
            case CPUMode::ABT:
                std::swap(spABT, r[CPUReg::SP]);
                std::swap(lrABT, r[CPUReg::LR]);
                break;
            case CPUMode::UND:
                std::swap(spUND, r[CPUReg::SP]);
                std::swap(lrUND, r[CPUReg::LR]);
                break;
        }

        // Load new register bank
        switch (newMode) {
            case CPUMode::USR:
            case CPUMode::SYS:
                break; // Do nothing
            case CPUMode::FIQ:
                std::swap(rFIQ[0], r[8]);
                std::swap(rFIQ[1], r[9]);
                std::swap(rFIQ[2], r[10]);
                std::swap(rFIQ[3], r[11]);
                std::swap(rFIQ[4], r[12]);

                std::swap(spFIQ, r[CPUReg::SP]);
                std::swap(lrFIQ, r[CPUReg::LR]);
                break;
            case CPUMode::IRQ:
                std::swap(spIRQ, r[CPUReg::SP]);
                std::swap(lrIRQ, r[CPUReg::LR]);
                break;
            case CPUMode::SVC:
                std::swap(spSVC, r[CPUReg::SP]);
                std::swap(lrSVC, r[CPUReg::LR]);
                break;
            case CPUMode::ABT:
                std::swap(spABT, r[CPUReg::SP]);
                std::swap(lrABT, r[CPUReg::LR]);
                break;
            case CPUMode::UND:
                std::swap(spUND, r[CPUReg::SP]);
                std::swap(lrUND, r[CPUReg::LR]);
                break;
        }

        cpsr.mode = newMode;
    }
}

}
