/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

#include <cassert>
#include <cstdio>

#include "cp15.hpp"
#include "../../common/types.hpp"

namespace nds::cpu {

using CP15 = cp15::CP15;

// PSR

/* CPU mode */
enum CPUMode {
    USR = 0x0,
    FIQ = 0x1,
    IRQ = 0x2,
    SVC = 0x3,
    ABT = 0x7,
    UND = 0xB,
    SYS = 0xF,
};

/* Processor status register */
struct PSR {
    CPUMode mode;

    bool t, f, i; // THUMB state, FIQ disable, IRQ disable

    bool q, v, c, z, n; // Sticky overflow, overflow, carry, zero, negative

    u32 get() {
        u32 data = 0x10; // Bit 4 is always high!

        data |= (u32)mode;

        data |= (u32)t << 5;
        data |= (u32)f << 6;
        data |= (u32)i << 7;

        data |= (u32)q << 27;
        data |= (u32)v << 28;
        data |= (u32)c << 29;
        data |= (u32)z << 30;
        data |= (u32)n << 31;

        return data;
    }

    void set(u8 mask, u32 data) {
        if (mask & (1 << 0)) {
            // Bit 4 *is* always high, but software usually doesn't write 0 to it

            switch (data & 0xF) {
                case CPUMode::USR: mode = CPUMode::USR; break;
                case CPUMode::FIQ: mode = CPUMode::FIQ; break;
                case CPUMode::IRQ: mode = CPUMode::IRQ; break;
                case CPUMode::SVC: mode = CPUMode::SVC; break;
                case CPUMode::ABT: mode = CPUMode::ABT; break;
                case CPUMode::UND: mode = CPUMode::UND; break;
                case CPUMode::SYS: mode = CPUMode::SYS; break;
                default:
                    std::printf("Invalid CPU mode %u\n", data & 0xF);

                    exit(0);
            }

            assert(t == !!(data & (1 << 5))); // :)

            f = data & (1 << 6);
            i = data & (1 << 7);
        }

        if (mask & (1 << 3)) {
            q = data & (1 << 27);
            v = data & (1 << 28);
            c = data & (1 << 29);
            z = data & (1 << 30);
            n = data & (1 << 31);
        }
    }
};

// CPU

/* CPU registers */
enum CPUReg {
    R0, R1, R2 , R3 , R4 , R5, R6, R7,
    R8, R9, R10, R11, R12, SP, LR, PC,
};

struct CPU {
    CPU(int cpuID, CP15 *cp15);
    ~CPU();

    int cpuID;

    CP15 *cp15;

    u32 r[16];
    u32 cpc;

    PSR cpsr;
    PSR *cspsr;

    bool cout; // Carry out

    u8  (*read8 )(u32);
    u16 (*read16)(u32);
    u32 (*read32)(u32);

    void (*write8 )(u32, u8);
    void (*write16)(u32, u16);
    void (*write32)(u32, u32);

    u32 get(u32 idx);

    void changeMode(CPUMode newMode);

private:
    u32 rFIQ[5];

    u32 spFIQ, spSVC, spABT, spIRQ, spUND;
    u32 lrFIQ, lrSVC, lrABT, lrIRQ, lrUND;

    PSR spsrFIQ, spsrSVC, spsrABT, spsrIRQ, spsrUND;
};

}