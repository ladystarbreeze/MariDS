/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "cp15.hpp"

#include <cassert>
#include <cstdio>

namespace nds::cpu::cp15 {

enum CP15Reg {
    Control  = 0x0100,
    IIC      = 0x0750,
    IDC      = 0x0760,
    DWB      = 0x07A4,
    DTCMSize = 0x0910,
};

u32 CP15::get(u32 idx) {
    switch (idx) {
        case CP15Reg::Control:
            std::printf("[ARM9:CP15 ] Read @ Control\n");

            return control;
        case CP15Reg::DTCMSize:
            std::printf("[ARM9:CP15 ] Read @ DTCM size\n");

            return dtcmSize;
        default:
            std::printf("[ARM9:CP15 ] Unhandled read @ 0x%04X\n", idx);

            exit(0);
    }
}

void CP15::set(u32 idx, u32 data) {
    switch (idx) {
        case CP15Reg::Control:
            std::printf("[ARM9:CP15 ] Write @ Control = 0x%08X\n", data);

            control = data;
            break;
        case CP15Reg::IIC:
            std::printf("[ARM9:CP15 ] Invalidate instruction cache\n");
            break;
        case CP15Reg::IDC:
            std::printf("[ARM9:CP15 ] Invalidate data cache\n");
            break;
        case CP15Reg::DWB:
            std::printf("[ARM9:CP15 ] Drain write buffer\n");
            break;
        case CP15Reg::DTCMSize:
            std::printf("[ARM9:CP15 ] Write @ DTCM size = 0x%08X\n", data);

            dtcmSize = data & 0xFFFFF003E;
            break;
        default:
            std::printf("[ARM9:CP15 ] Unhandled write @ 0x%04X = 0x%08X\n", idx, data);

            exit(0);
    }
}

}
