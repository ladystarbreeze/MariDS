/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "timer.hpp"

#include <cassert>
#include <cstdio>

enum class TimerReg {
    TMCNT   = 0x04000100,
    TMCNT_H = 0x04000102,
};

struct TMCNT {
    u8   prescaler;
    bool cascade;
    bool irqen;
    bool tmen;
};

struct Timer {
    TMCNT tmcnt;

    u16 reload; // TMCNT_L

    u32 ctr, subctr, prescaler;
};

Timer timers[4];

namespace nds::timer {

void checkCascade(int tmID) {
    auto &tm = timers[tmID];

    auto &cnt = tm.tmcnt;

    if (cnt.tmen && cnt.cascade) {
        ++tm.ctr;

        if (tm.ctr & (1 << 16)) {
            // Reload counter, trigger interrupt
            tm.ctr = tm.reload;

            if (cnt.irqen) {
                std::printf("[Timer:ARM7] Unhandled timer %d IRQ\n", tmID);

                exit(0);
            }

            // Check previous timer for cascade
            if (tmID > 0) checkCascade(tmID - 1);
        }
    }
}

void init() {
    for (auto &i : timers) {
        i.prescaler = 1;

        i.ctr = i.subctr = 0;
    }
}

void run(i64 runCycles) {
    for (int i = 0; i < 4; i++) {
        auto &tm = timers[i];

        auto &cnt = tm.tmcnt;

        if (!cnt.tmen || cnt.cascade) continue;

        tm.subctr += runCycles;

        while (tm.subctr > tm.prescaler) {
            ++tm.ctr;

            if (tm.ctr & (1 << 16)) {
                // Reload counter, trigger interrupt
                tm.ctr = tm.reload;

                if (cnt.irqen) {
                    std::printf("[Timer:ARM7] Unhandled timer %d IRQ\n", i);

                    exit(0);
                }

                // Check previous timer for cascade
                if (i > 0) checkCascade(i - 1);
            }

            tm.subctr -= tm.prescaler;
        }
    }
}

void write16ARM7(u32 addr, u16 data) {
    const auto tmID = (addr >> 2) & 3;

    auto &tm = timers[tmID];

    switch (addr & ~(3 << 2)) {
        case static_cast<u32>(TimerReg::TMCNT):
            std::printf("[Timer:ARM7] Write16 @ TM%uCNT_L = 0x%04X\n", tmID, data);

            tm.reload = data;
            break;
        case static_cast<u32>(TimerReg::TMCNT_H):
            {
                std::printf("[Timer:ARM7] Write16 @ TM%uCNT_H = 0x%04X\n", tmID, data);

                auto &cnt = tm.tmcnt;

                const auto tmen = cnt.tmen;

                cnt.prescaler = data & 3;
                cnt.cascade   = data & (1 << 2);

                cnt.irqen = data & (1 << 6);
                cnt.tmen  = data & (1 << 7);

                if (!tmen && cnt.tmen) { // Set up timer
                    tm.ctr = tm.reload;

                    tm.subctr = 0;

                    switch (cnt.prescaler) {
                        case 0: tm.prescaler =    1; break;
                        case 1: tm.prescaler =   64; break;
                        case 2: tm.prescaler =  256; break;
                        case 3: tm.prescaler = 1024; break;
                    }
                }
            }
            break;
        default:
            break;
    }
}

void write32ARM7(u32 addr, u32 data) {
    const auto tmID = (addr >> 2) & 3;

    auto &tm = timers[tmID];

    switch (addr & ~(3 << 2)) {
        case static_cast<u32>(TimerReg::TMCNT):
            {
                std::printf("[Timer:ARM7] Write32 @ TM%uCNT = 0x%08X\n", tmID, data);

                auto &cnt = tm.tmcnt;

                const auto tmen = cnt.tmen;

                tm.reload = data;

                cnt.prescaler = (data >> 16) & 3;
                cnt.cascade   = data & (1 << 18);

                cnt.irqen = data & (1 << 22);
                cnt.tmen  = data & (1 << 23);

                if (!tmen && cnt.tmen) { // Set up timer
                    tm.ctr = tm.reload;

                    tm.subctr = 0;

                    switch (cnt.prescaler) {
                        case 0: tm.prescaler =    1; break;
                        case 1: tm.prescaler =   64; break;
                        case 2: tm.prescaler =  256; break;
                        case 3: tm.prescaler = 1024; break;
                    }
                }
            }
            break;
        default:
            break;
    }
}

}
