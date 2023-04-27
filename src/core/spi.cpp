/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "spi.hpp"

#include <cassert>
#include <cstdio>

#include "firmware.hpp"

namespace nds::spi {

constexpr const char *devNames[] = {
    "Power Management", "Firmware", "TSC", "Reserved",
};

enum SPIDev {
    PowerManagement, Firmware, TSC, Reserved,
};

struct SPICNT {
    u8     baud;
    bool   busy;
    SPIDev dev;
    bool   size;
    bool   hold;
    bool   irqen;
    bool   spien;

    bool chipselect;
};

SPICNT spicnt;

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

u8 readSPIDATA() {
    if (!spicnt.spien || !spicnt.chipselect) return 0;

    switch (spicnt.dev) {
        case SPIDev::PowerManagement:
            return 0xFF;
        case SPIDev::Firmware:
            return firmware::read();
        case SPIDev::TSC:
            return 0xFF;
        default:
            std::printf("[SPI       ] Unhandled SPI device %s\n", devNames[spicnt.dev]);

            exit(0);
    }
}

void writeSPICNT(u16 data) {
    spicnt.baud  = data & 3;
    spicnt.size  = data & (1 << 10);
    spicnt.hold  = data & (1 << 11);
    spicnt.irqen = data & (1 << 14);
    spicnt.spien = data & (1 << 15);

    if (!spicnt.chipselect) { // Select new device
        spicnt.dev = (SPIDev)((data >> 8) & 3);

        spicnt.chipselect = true;
    }

    assert(!spicnt.size); // Don't think anything uses this
}

void writeSPIDATA(u8 data) {
    if (spicnt.spien && spicnt.chipselect) {
        switch (spicnt.dev) {
            case SPIDev::PowerManagement:
                std::printf("[SPI       ] Unhandled Power Management write = 0x%02X\n", data);
                break;
            case SPIDev::Firmware:
                firmware::write(data);
                break;
            case SPIDev::TSC:
                std::printf("[SPI       ] Unhandled TSC write = 0x%02X\n", data);
                break;
            default:
                std::printf("[SPI       ] Unhandled SPI device %s\n", devNames[spicnt.dev]);

                exit(0);
        }

        if (!spicnt.hold) {
            firmware::release();
            
            spicnt.chipselect = false; // Release chip
        }

        assert(!spicnt.irqen);
    }
}

}
