/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "cpu.hpp"

#include <algorithm>
#include <cstring>

#include "../bus.hpp"

namespace nds::cpu {

// A little hacky but eh
u8 itcm[0x8000];
u8 dtcm[0x4000];

u32 itcmBase , dtcmBase;
u32 itcmLimit, dtcmLimit;

/* Returns true if address is in range [base;limit] */
bool inRange(u64 addr, u64 base, u64 limit) {
    return (addr >= base) && (addr < (base + limit));
}

u8 read8ARM9(u32 addr) {
    if (inRange(addr, itcmBase, itcmLimit)) {
        return itcm[addr & 0x7FFF];
    } else if (inRange(addr, dtcmBase, dtcmLimit)) {
        return dtcm[addr & 0x3FFF];
    } else {
        return bus::read8ARM9(addr);
    }
}

u16 read16ARM9(u32 addr) {
    u16 data;

    if (inRange(addr, itcmBase, itcmLimit)) {
        std::memcpy(&data, &itcm[addr & 0x7FFF], sizeof(u16));
    } else if (inRange(addr, dtcmBase, dtcmLimit)) {
        std::memcpy(&data, &dtcm[addr & 0x3FFF], sizeof(u16));
    } else {
        return bus::read16ARM9(addr);
    }

    return data;
}

u32 read32ARM9(u32 addr) {
    u32 data;

    if (inRange(addr, itcmBase, itcmLimit)) {
        std::memcpy(&data, &itcm[addr & 0x7FFF], sizeof(u32));
    } else if (inRange(addr, dtcmBase, dtcmLimit)) {
        std::memcpy(&data, &dtcm[addr & 0x3FFF], sizeof(u32));
    } else {
        return bus::read32ARM9(addr);
    }

    return data;
}

void write8ARM9(u32 addr, u8 data) {
    if (inRange(addr, itcmBase, itcmLimit)) {
        itcm[addr & 0x7FFF] = data;
    } else if (inRange(addr, dtcmBase, dtcmLimit)) {
        dtcm[addr & 0x3FFF] = data;
    } else {
        return bus::write8ARM9(addr, data);
    }
}

void write16ARM9(u32 addr, u16 data) {
    if (inRange(addr, itcmBase, itcmLimit)) {
        std::memcpy(&itcm[addr & 0x7FFF], &data, sizeof(u16));
    } else if (inRange(addr, dtcmBase, dtcmLimit)) {
        std::memcpy(&dtcm[addr & 0x3FFF], &data, sizeof(u16));
    } else {
        return bus::write16ARM9(addr, data);
    }
}

void write32ARM9(u32 addr, u32 data) {
    if (inRange(addr, itcmBase, itcmLimit)) {
        std::memcpy(&itcm[addr & 0x7FFF], &data, sizeof(u32));
    } else if (inRange(addr, dtcmBase, dtcmLimit)) {
        std::memcpy(&dtcm[addr & 0x3FFF], &data, sizeof(u32));
    } else {
        return bus::write32ARM9(addr, data);
    }
}

/* Exception vector base addresses */
enum class VectorBase : u32 {
    ARM7 = 0,
    ARM9 = 0xFFFF0000,
};

CPU::CPU(int cpuID, CP15 *cp15) {
    assert((cpuID == 7) || (cpuID == 9));

    this->cpuID = cpuID;

    this->cp15 = cp15;

    std::memset(&r, 0, sizeof(r));

    if (cpuID == 7) {
        r[CPUReg::PC] = static_cast<u32>(VectorBase::ARM7);

        read8  = &bus::read8ARM7;
        read16 = &bus::read16ARM7;
        read32 = &bus::read32ARM7;

        write8  = &bus::write8ARM7;
        write16 = &bus::write16ARM7;
        write32 = &bus::write32ARM7;
    } else {
        r[CPUReg::PC] = static_cast<u32>(VectorBase::ARM9);

        read8  = &read8ARM9;
        read16 = &read16ARM9;
        read32 = &read32ARM9;

        write8  = &write8ARM9;
        write16 = &write16ARM9;
        write32 = &write32ARM9;
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

void CPU::setEntry(u32 addr) {
    std::printf("[ARM%d      ] Entry point = 0x%08X\n", cpuID, addr);

    r[CPUReg::PC] = addr;

    changeMode(CPUMode::SYS); // Only happens when fast booting, so we have to do this

    r[CPUReg::R12] = addr;
    r[CPUReg::LR ] = addr;

    if (cpuID == 7) {
        r[CPUReg::SP] = 0x03002F7C;

        spIRQ = 0x03003F80;
        spSVC = 0x03003FC0;
    } else {
        r[CPUReg::SP] = 0x0380FD80;

        spIRQ = 0x0380FF80;
        spSVC = 0x0380FFC0;
    }
}

/* Returns PC + 4 or a register without offset */
u32 CPU::get(u32 idx) {
    assert(idx < 16);

    return (idx == 15) ? r[idx] + ((cpsr.t) ? 2 : 4) : r[idx];
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
                cspsr = NULL;
                break; // Do nothing
            case CPUMode::FIQ:
                std::swap(rFIQ[0], r[8]);
                std::swap(rFIQ[1], r[9]);
                std::swap(rFIQ[2], r[10]);
                std::swap(rFIQ[3], r[11]);
                std::swap(rFIQ[4], r[12]);

                std::swap(spFIQ, r[CPUReg::SP]);
                std::swap(lrFIQ, r[CPUReg::LR]);

                cspsr = &spsrFIQ;
                break;
            case CPUMode::IRQ:
                std::swap(spIRQ, r[CPUReg::SP]);
                std::swap(lrIRQ, r[CPUReg::LR]);

                cspsr = &spsrIRQ;
                break;
            case CPUMode::SVC:
                std::swap(spSVC, r[CPUReg::SP]);
                std::swap(lrSVC, r[CPUReg::LR]);

                cspsr = &spsrSVC;
                break;
            case CPUMode::ABT:
                std::swap(spABT, r[CPUReg::SP]);
                std::swap(lrABT, r[CPUReg::LR]);

                cspsr = &spsrABT;
                break;
            case CPUMode::UND:
                std::swap(spUND, r[CPUReg::SP]);
                std::swap(lrUND, r[CPUReg::LR]);

                cspsr = &spsrUND;
                break;
        }

        cpsr.mode = newMode;
    }
}

void CPU::halt() {
    std::printf("[ARM%d      ] Halted\n", cpuID);
    
    isHalted = true;
}

void CPU::unhalt() {
    std::printf("[ARM%d      ] Unhalted\n", cpuID);

    isHalted = false;
}

void CPU::raiseIRQException() {
    const auto lr = get(CPUReg::PC) + 2 * cpsr.t;

    std::printf("[ARM%d%s    ] IRQ exception @ 0x%08X\n", cpuID, (cpsr.t) ? ":T" : "  ", r[CPUReg::PC]);

    spsrIRQ.set(0xF, cpsr.get());

    cpsr.t = false; // Return to ARM state
    cpsr.f = false; // Keep FIQ enabled
    cpsr.i = true;  // Block IRQs

    changeMode(CPUMode::IRQ);

    const auto vectorBase = (cpuID == 7) ? VectorBase::ARM7 : VectorBase::ARM9;

    r[CPUReg::LR] = lr;
    r[CPUReg::PC] = static_cast<u32>(vectorBase) | 0x18;
}

void CPU::raiseSVCException() {
    const auto lr = r[CPUReg::PC];

    std::printf("[ARM%d%s    ] SVC exception @ 0x%08X\n", cpuID, (cpsr.t) ? ":T" : "  ", r[CPUReg::PC] - ((cpsr.t) ? 2 : 4));

    spsrSVC.set(0xF, cpsr.get());

    cpsr.t = false; // Return to ARM state
    cpsr.f = false; // Keep FIQ enabled
    cpsr.i = true;  // Block IRQs

    changeMode(CPUMode::SVC);

    const auto vectorBase = (cpuID == 7) ? VectorBase::ARM7 : VectorBase::ARM9;

    r[CPUReg::LR] = lr;
    r[CPUReg::PC] = static_cast<u32>(vectorBase) | 0x8;
}

void CPU::setIRQPending(bool irq) {
    irqPending = irq;

    checkInterrupt();
}

void CPU::checkInterrupt() {
    if (irqPending && !cpsr.i) raiseIRQException();
}

void setDTCM(u32 size) {
    dtcmBase  = size & ~0xFFF;
    dtcmLimit = 512 << ((size >> 1) & 0x1F);
}

void setITCM(u32 size) {
    itcmBase  = size & ~0xFFF;
    itcmLimit = 512 << ((size >> 1) & 0x1F);
}

}
