/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "timer.hpp"

#include <cassert>
#include <cstdio>

#include "intc.hpp"

namespace nds::timer {

using IntSource = intc::IntSource;

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

Timer timers7[4], timers9[4];

void checkCascade7(int tmID) {
    auto &tm = timers7[tmID];

    auto &cnt = tm.tmcnt;

    if (cnt.tmen && cnt.cascade) {
        ++tm.ctr;

        if (tm.ctr & (1 << 16)) {
            // Reload counter, trigger interrupt
            tm.ctr = tm.reload;

            if (cnt.irqen) intc::sendInterrupt7((IntSource)(tmID + 3));

            // Check previous timer for cascade
            if ((tmID != 0)) checkCascade7(tmID - 1);
        }
    }
}

void checkCascade9(int tmID) {
    auto &tm = timers9[tmID];

    auto &cnt = tm.tmcnt;

    if (cnt.tmen && cnt.cascade) {
        ++tm.ctr;

        if (tm.ctr & (1 << 16)) {
            // Reload counter, trigger interrupt
            tm.ctr = tm.reload;

            if (cnt.irqen) intc::sendInterrupt9((IntSource)(tmID + 3));

            // Check previous timer for cascade
            if ((tmID != 0)) checkCascade9(tmID - 1);
        }
    }
}

void init() {
    for (auto &i : timers7) {
        i.prescaler = 1;

        i.ctr = i.subctr = 0;
    }

    for (auto &i : timers9) {
        i.prescaler = 1;

        i.ctr = i.subctr = 0;
    }
}

void run7(i64 runCycles) {
    for (int i = 0; i < 4; i++) {
        auto &tm = timers7[i];

        auto &cnt = tm.tmcnt;

        if (!cnt.tmen || cnt.cascade) continue;

        tm.subctr += runCycles;

        while (tm.subctr > tm.prescaler) {
            ++tm.ctr;

            if (tm.ctr & (1 << 16)) {
                // Reload counter, trigger interrupt
                tm.ctr = tm.reload;

                if (cnt.irqen) intc::sendInterrupt7((IntSource)((int)IntSource::Timer0 + i));

                // Check previous timer for cascade
                if (i != 0) checkCascade7(i - 1);
            }

            tm.subctr -= tm.prescaler;
        }
    }
}

void run9(i64 runCycles) {
    for (int i = 0; i < 4; i++) {
        auto &tm = timers9[i];

        auto &cnt = tm.tmcnt;

        if (!cnt.tmen || cnt.cascade) continue;

        tm.subctr += runCycles;

        while (tm.subctr > tm.prescaler) {
            ++tm.ctr;

            if (tm.ctr & (1 << 16)) {
                // Reload counter, trigger interrupt
                tm.ctr = tm.reload;

                if (cnt.irqen) intc::sendInterrupt9((IntSource)((int)IntSource::Timer0 + i));

                // Check previous timer for cascade
                if (i != 0) checkCascade9(i - 1);
            }

            tm.subctr -= tm.prescaler;
        }
    }
}

void run(i64 runCycles) {
    run7(runCycles);
    run9(runCycles);
}

u16 read16ARM7(u32 addr) {
    u16 data;

    const auto tmID = (addr >> 2) & 3;

    auto &tm = timers7[tmID];

    switch (addr & ~(3 << 2)) {
        case static_cast<u32>(TimerReg::TMCNT):
            std::printf("[Timer:ARM7] Read16 @ TM%uCNT_L\n", tmID);
            return tm.ctr;
        case static_cast<u32>(TimerReg::TMCNT_H):
            {
                std::printf("[Timer:ARM7] Read16 @ TM%uCNT_H\n", tmID);

                auto &cnt = tm.tmcnt;

                data  = (u16)cnt.prescaler;
                data |= (u16)cnt.cascade << 2;
                data |= (u16)cnt.irqen   << 6;
                data |= (u16)cnt.tmen    << 7;
            }
            break;
        default:
            std::printf("[Timer:ARM7] Unhandled read16 @ 0x%08X\n", addr);
            
            exit(0);
    }

    return data;
}

u16 read16ARM9(u32 addr) {
    u16 data;

    const auto tmID = (addr >> 2) & 3;

    auto &tm = timers9[tmID];

    switch (addr & ~(3 << 2)) {
        case static_cast<u32>(TimerReg::TMCNT):
            std::printf("[Timer:ARM9] Read16 @ TM%uCNT_L\n", tmID);
            return tm.ctr;
        case static_cast<u32>(TimerReg::TMCNT_H):
            {
                std::printf("[Timer:ARM9] Read16 @ TM%uCNT_H\n", tmID);

                auto &cnt = tm.tmcnt;

                data  = (u16)cnt.prescaler;
                data |= (u16)cnt.cascade << 2;
                data |= (u16)cnt.irqen   << 6;
                data |= (u16)cnt.tmen    << 7;
            }
            break;
        default:
            std::printf("[Timer:ARM9] Unhandled read16 @ 0x%08X\n", addr);
            
            exit(0);
    }

    return data;
}

void write16ARM7(u32 addr, u16 data) {
    const auto tmID = (addr >> 2) & 3;

    auto &tm = timers7[tmID];

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

    auto &tm = timers7[tmID];

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

void write16ARM9(u32 addr, u16 data) {
    const auto tmID = (addr >> 2) & 3;

    auto &tm = timers9[tmID];

    switch (addr & ~(3 << 2)) {
        case static_cast<u32>(TimerReg::TMCNT):
            std::printf("[Timer:ARM9] Write16 @ TM%uCNT_L = 0x%04X\n", tmID, data);

            tm.reload = data;
            break;
        case static_cast<u32>(TimerReg::TMCNT_H):
            {
                std::printf("[Timer:ARM9] Write16 @ TM%uCNT_H = 0x%04X\n", tmID, data);

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

}
