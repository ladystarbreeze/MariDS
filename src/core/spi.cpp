/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "spi.hpp"

#include <cassert>
#include <cstdio>

namespace nds::spi {

struct SPICNT {
    u8   baud;
    bool busy;
    u8   dev;
    bool size;
    bool hold;
    bool irqen;
    bool spien;

    bool chipselect;
};

SPICNT spicnt;

u16 spidata;

u16 readSPICNT() {
    u16 data;

    data  = (u16)spicnt.baud;
    data |= (u16)spicnt.busy  << 7;
    data |= (u16)spicnt.dev   << 8;
    data |= (u16)spicnt.size  << 10;
    data |= (u16)spicnt.hold  << 11;
    data |= (u16)spicnt.irqen << 14;
    data |= (u16)spicnt.spien << 15;

    return data;
}

u16 readSPIDATA() {
    if (!spicnt.spien) return 0;

    return spidata;
}

}
