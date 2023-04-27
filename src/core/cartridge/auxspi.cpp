/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "auxspi.hpp"

#include <cassert>
#include <cstdio>

namespace nds::cartridge::auxspi {

struct AUXSPICNT {
    u8   baud;
    bool hold;
    bool busy;
    bool mode;
    bool irqen;
    bool sloten;
};

AUXSPICNT auxspicnt;

u16 readAUXSPICNT16() {
    u16 data;

    std::printf("[AUXSPI    ] Read16 @ AUXSPICNT\n");

    data  = (u16)auxspicnt.baud;
    data |= (u16)auxspicnt.hold   <<  6;
    data |= (u16)auxspicnt.busy   <<  7;
    data |= (u16)auxspicnt.mode   << 13;
    data |= (u16)auxspicnt.irqen  << 14;
    data |= (u16)auxspicnt.sloten << 15;

    return data;
}

u16 readAUXSPIDATA16() {
    std::printf("[AUXSPI    ] Read16 @ AUXSPIDATA\n");

    return 0;
}

void writeAUXSPICNT8(bool isHi, u8 data) {
    std::printf("[AUXSPI    ] Write8 @ AUXSPICNT_%s = 0x%02X\n", (isHi) ? "H" : "L", data);

    if (isHi) {
        auxspicnt.mode   = data & (1 << 5);
        auxspicnt.irqen  = data & (1 << 6);
        auxspicnt.sloten = data & (1 << 7);
    } else {
        auxspicnt.baud = data & 3;
        auxspicnt.hold = data & (1 << 6);
    }
}

void writeAUXSPICNT16(u16 data) {
    std::printf("[AUXSPI    ] Write16 @ AUXSPICNT = 0x%04X\n", data);

    auxspicnt.baud   = data & 3;
    auxspicnt.hold   = data & (1 <<  6);
    auxspicnt.mode   = data & (1 << 13);
    auxspicnt.irqen  = data & (1 << 14);
    auxspicnt.sloten = data & (1 << 15);
}

void writeAUXSPIDATA16(u16 data) {
    std::printf("[AUXSPI    ] Write16 @ AUXSPIDATA = 0x%04X\n", data);
}

}
