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

namespace nds::timer {

void write32ARM7(u32 addr, u32 data) {
    const auto tmID = (addr >> 2) & 3;

    switch (addr & ~(3 << 2)) {
        case static_cast<u32>(TimerReg::TMCNT):
            std::printf("[Timer:ARM7] Write32 @ TM%uCNT = 0x%08X\n", tmID, data);
            break;
        default:
            break;
    }
}

}
