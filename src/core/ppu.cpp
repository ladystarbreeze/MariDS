/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "ppu.hpp"

#include <cstdio>
#include <vector>

#include "intc.hpp"
#include "MariDS.hpp"
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

std::vector<u8> vram;

DISPSTAT dispstat[2];

u16 vcount;

// Scheduler
u64 idHBLANK, idScanline;

void hblankEvent(i64 c) {
    (void)c;
    
    dispstat[0].hblank = dispstat[1].hblank = true; // Technically incorrect, but should be fine for now

    assert(!dispstat[0].hirqen && !dispstat[1].hirqen);

    scheduler::addEvent(idHBLANK, 0, CYCLES_PER_SCANLINE);
}

void scanlineEvent(i64 c) {
    (void)c;

    dispstat[0].hblank = dispstat[1].hblank = false;

    ++vcount;

    if (vcount == LINES_PER_VDRAW) {
        dispstat[0].vblank = dispstat[1].vblank = true;

        if (dispstat[0].virqen) {
            intc::sendInterrupt7(IntSource::VBLANK);
        }

        if (dispstat[1].virqen) {
            intc::sendInterrupt9(IntSource::VBLANK);
        }

        update(vram.data());
    } else if (vcount == (LINES_PER_FRAME - 1)) {
        dispstat[0].vblank = dispstat[1].vblank = false; // Is turned off on the last scanline
    } else if (vcount == LINES_PER_FRAME) {
        vcount = 0;
    }

    if (vcount == dispstat[0].lyc) {
        dispstat[0].vcounter = true;

        assert(!dispstat[0].lycirqen);
    } else {
        dispstat[0].vcounter = false;
    }

    if (vcount == dispstat[1].lyc) {
        dispstat[1].vcounter = true;

        assert(!dispstat[1].lycirqen);
    } else {
        dispstat[1].vcounter = false;
    }

    scheduler::addEvent(idScanline, 0, CYCLES_PER_SCANLINE);
}

void init() {
    vcount = 0;

    idHBLANK   = scheduler::registerEvent([](int, i64 c) { hblankEvent  (c); });
    idScanline = scheduler::registerEvent([](int, i64 c) { scanlineEvent(c); });

    scheduler::addEvent(idHBLANK  , 0, CYCLES_PER_HDRAW);
    scheduler::addEvent(idScanline, 0, CYCLES_PER_SCANLINE);

    vram.resize(0xA4000);
}

void writeVRAM16(u32 addr, u16 data) {
    std::memcpy(&vram[addr], &data, sizeof(u16));
}

void writeVRAM32(u32 addr, u32 data) {
    std::memcpy(&vram[addr], &data, sizeof(u32));
}

u16 readDISPSTAT7() {
    u16 data;

    data  = (u16)dispstat[0].vblank   << 0;
    data |= (u16)dispstat[0].hblank   << 1;
    data |= (u16)dispstat[0].vcounter << 2;
    data |= (u16)dispstat[0].virqen   << 3;
    data |= (u16)dispstat[0].hirqen   << 4;
    data |= (u16)dispstat[0].lycirqen << 5;
    
    return data | (dispstat[0].lyc << 7);
}

u16 readDISPSTAT9() {
    u16 data;

    data  = (u16)dispstat[1].vblank   << 0;
    data |= (u16)dispstat[1].hblank   << 1;
    data |= (u16)dispstat[1].vcounter << 2;
    data |= (u16)dispstat[1].virqen   << 3;
    data |= (u16)dispstat[1].hirqen   << 4;
    data |= (u16)dispstat[1].lycirqen << 5;
    
    return data | (dispstat[1].lyc << 7);
}

void writeDISPSTAT7(u16 data) {
    dispstat[0].virqen   = data & (1 << 3);
    dispstat[0].hirqen   = data & (1 << 4);
    dispstat[0].lycirqen = data & (1 << 5);

    dispstat[0].lyc = data >> 7;
}

void writeDISPSTAT9(u16 data) {
    dispstat[1].virqen   = data & (1 << 3);
    dispstat[1].hirqen   = data & (1 << 4);
    dispstat[1].lycirqen = data & (1 << 5);

    dispstat[1].lyc = data >> 7;
}

}
