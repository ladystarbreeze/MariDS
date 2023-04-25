/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "cp15.hpp"

#include <cassert>
#include <cstdio>

#include "../MariDS.hpp"

namespace nds::cpu::cp15 {

enum CP15Reg {
    Control  = 0x0100,
    CDPR     = 0x0200,
    CIPR     = 0x0201,
    CWB      = 0x0300,
    APDPR    = 0x0500,
    APIPR    = 0x0501,
    EAPDPR   = 0x0502,
    EAPIPR   = 0x0503,
    WFI      = 0x0704,
    IIC      = 0x0750,
    IDC      = 0x0760,
    DWB      = 0x07A4,
    DTCMSize = 0x0910,
    ITCMSize = 0x0911,
};

u32 CP15::get(u32 idx) {
    switch (idx) {
        case CP15Reg::Control:
            std::printf("[ARM9:CP15 ] Read @ Control\n");

            return control;
        case CP15Reg::CDPR:
            std::printf("[ARM9:CP15 ] Read @ Cacheability (data protection region)\n");
            return 0;
        case CP15Reg::CIPR:
            std::printf("[ARM9:CP15 ] Read @ Cacheability (instruction protection region)\n");
            return 0;
        case CP15Reg::CWB:
            std::printf("[ARM9:CP15 ] Read @ Cache write bufferability\n");
            return 0;
        case CP15Reg::APDPR:
            std::printf("[ARM9:CP15 ] Read @ Access permission (data protection region)\n");
            return 0;
        case CP15Reg::APIPR:
            std::printf("[ARM9:CP15 ] Read @ Access permission (instruction protection region)\n");
            return 0;
        case CP15Reg::EAPDPR:
            std::printf("[ARM9:CP15 ] Read @ Extended access permission (data protection region)\n");
            return 0;
        case CP15Reg::EAPIPR:
            std::printf("[ARM9:CP15 ] Read @ Extended access permission (instruction protection region)\n");
            return 0;
        case 0x0600:
        case 0x0610:
        case 0x0620:
        case 0x0630:
        case 0x0640:
        case 0x0650:
        case 0x0660:
        case 0x0670:
            std::printf("[ARM9:CP15 ] Read @ PU data region %u\n", (idx >> 4) & 0xF);
            return 0;
        case CP15Reg::DTCMSize:
            std::printf("[ARM9:CP15 ] Read @ DTCM size\n");

            return dtcmSize;
        case CP15Reg::ITCMSize:
            std::printf("[ARM9:CP15 ] Read @ ITCM size\n");

            return itcmSize;
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
        case CP15Reg::CDPR:
            std::printf("[ARM9:CP15 ] Write @ Cacheability (data protection region) = 0x%08X\n", data);
            break;
        case CP15Reg::CIPR:
            std::printf("[ARM9:CP15 ] Write @ Cacheability (instruction protection region) = 0x%08X\n", data);
            break;
        case CP15Reg::CWB:
            std::printf("[ARM9:CP15 ] Write @ Cache write bufferability = 0x%08X\n", data);
            break;
        case CP15Reg::EAPDPR:
            std::printf("[ARM9:CP15 ] Write @ Extended access permission (data protection region) = 0x%08X\n", data);
            break;
        case CP15Reg::EAPIPR:
            std::printf("[ARM9:CP15 ] Write @ Extended access permission (instruction protection region) = 0x%08X\n", data);
            break;
        case 0x0600:
        case 0x0610:
        case 0x0620:
        case 0x0630:
        case 0x0640:
        case 0x0650:
        case 0x0660:
        case 0x0670:
            std::printf("[ARM9:CP15 ] Write @ PU data region %u = 0x%08X\n", (idx >> 4) & 0xF, data);
            break;
        case CP15Reg::WFI:
            std::printf("[ARM9:CP15 ] Wait for interrupt\n");

            haltCPU(9);
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
        case CP15Reg::ITCMSize:
            std::printf("[ARM9:CP15 ] Write @ ITCM size = 0x%08X\n", data);

            itcmSize = data & 0xFFFFF003E;
            break;
        default:
            std::printf("[ARM9:CP15 ] Unhandled write @ 0x%04X = 0x%08X\n", idx, data);

            exit(0);
    }
}

}
