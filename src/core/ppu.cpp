/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "ppu.hpp"

#include <cstdio>

#include "intc.hpp"
#include "scheduler.hpp"

namespace nds::ppu {

using IntSource = intc::IntSource;

// PPU constants

constexpr i64 PIXELS_PER_HDRAW  = 256;
constexpr i64 PIXELS_PER_HBLANK = 99;
constexpr i64 LINES_PER_VDRAW   = 192;
constexpr i64 LINES_PER_FRAME   = 263;

constexpr i64 CYCLES_PER_HDRAW    = 6 * PIXELS_PER_HDRAW;
constexpr i64 CYCLES_PER_SCANLINE = 6 * (PIXELS_PER_HDRAW + PIXELS_PER_HBLANK);

struct DISPSTAT {
    bool vblank, hblank, vcounter;

    bool virqen, hirqen, lycirqen;

    u16 lyc;
};

DISPSTAT dispstat;

u16 vcount;

// Scheduler
u64 idHBLANK, idScanline;

void hblankEvent(i64 c) {
    (void)c;
    
    dispstat.hblank = true; // Technically incorrect, but should be fine for now

    assert(!dispstat.hirqen);

    scheduler::addEvent(idHBLANK, 0, CYCLES_PER_SCANLINE);
}

void scanlineEvent(i64 c) {
    (void)c;

    dispstat.hblank = false;

    ++vcount;

    if (vcount == LINES_PER_VDRAW) {
        dispstat.vblank = true;

        if (dispstat.virqen) {
            intc::sendInterrupt7(IntSource::VBLANK);
            intc::sendInterrupt9(IntSource::VBLANK);
        }
    } else if (vcount == (LINES_PER_FRAME - 1)) {
        dispstat.vblank = false; // Is turned off on the last scanline
    } else if (vcount == LINES_PER_FRAME) {
        vcount = 0;
    }

    if (vcount == dispstat.lyc) {
        dispstat.vcounter = true;

        assert(!dispstat.lycirqen);
    } else {
        dispstat.vcounter = false;
    }

    scheduler::addEvent(idScanline, 0, CYCLES_PER_SCANLINE);
}

void init() {
    vcount = 0;

    idHBLANK   = scheduler::registerEvent([](int, i64 c) { hblankEvent  (c); });
    idScanline = scheduler::registerEvent([](int, i64 c) { scanlineEvent(c); });

    scheduler::addEvent(idHBLANK  , 0, CYCLES_PER_HDRAW);
    scheduler::addEvent(idScanline, 0, CYCLES_PER_SCANLINE);
}

u16 readDISPSTAT() {
    u16 data;

    data  = (u16)dispstat.vblank   << 0;
    data |= (u16)dispstat.hblank   << 1;
    data |= (u16)dispstat.vcounter << 2;
    data |= (u16)dispstat.virqen   << 3;
    data |= (u16)dispstat.hirqen   << 4;
    data |= (u16)dispstat.lycirqen << 5;
    
    return data | (dispstat.lyc << 7);
}

void writeDISPSTAT(u16 data) {
    dispstat.virqen   = data & (1 << 3);
    dispstat.hirqen   = data & (1 << 4);
    dispstat.lycirqen = data & (1 << 5);

    dispstat.lyc = data >> 7;
}

}
