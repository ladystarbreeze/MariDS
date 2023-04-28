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

constexpr int OBJA_VRAM[] = { 0, 1, 4, 5, 6 };
constexpr int  BGB_VRAM[] = { 2, 7, 8 };


// Display Engine registers

enum class PPUReg {
    DISPCNT      = 0x04000000,
    DISPSTAT     = 0x04000004,
    VCOUNT       = 0x04000006,
    BGCNT        = 0x04000008,
    BGHOFS       = 0x04000010,
    BGVOFS       = 0x04000012,
    BG2PA        = 0x04000020,
    BG2PB        = 0x04000022,
    BG2PC        = 0x04000024,
    BG2PD        = 0x04000026,
    BG2X_L       = 0x04000028,
    BG2X_H       = 0x0400002A,
    BG2Y_L       = 0x0400002C,
    BG2Y_H       = 0x0400002E,
    BG3PA        = 0x04000030,
    BG3PB        = 0x04000032,
    BG3PC        = 0x04000034,
    BG3PD        = 0x04000036,
    BG3X_L       = 0x04000038,
    BG3X_H       = 0x0400003A,
    BG3Y_L       = 0x0400003C,
    BG3Y_H       = 0x0400003E,
    WIN0H        = 0x04000040,
    WIN1H        = 0x04000042,
    WIN0V        = 0x04000044,
    WIN1V        = 0x04000046,
    WININ        = 0x04000048,
    WINOUT       = 0x0400004A,
    MOSAIC       = 0x0400004C,
    BLDCNT       = 0x04000050,
    BLDALPHA     = 0x04000052,
    BLDY         = 0x04000054,
    DISP3DCNT    = 0x04000060,
    DISPCAPCNT   = 0x04000064,
    DISPMMEMFIFO = 0x04000068,
    MASTERBRIGHT = 0x0400006C,
};

struct BGCNT {
    u8   prio;
    u8   charbase;
    bool mosaicen;
    bool pal256;
    u8   scrbase;
    bool extpalslot;
    u8   scrsize;
};

struct DISPCNT {
    u8   bgmode;
    bool bg3Den;
    bool obj1D;
    bool bitobjdim;
    bool bitobj1D;
    bool forcedblank;
    bool bgen[5];
    bool windowen[3];
    u8   dispmode;
    u8   bselect;
    u8   objbound;
    bool bitobjbound;
    bool objblanking;
    u8   charbase;
    u8   scrbase;
    bool bgextpal;
    bool objextpal;
};

struct DISPSTAT {
    bool vblank, hblank, vcounter;

    bool virqen, hirqen, lycirqen;

    u16 lyc;
};

struct WINH {
    u8 x2, x1;
};

struct WINV {
    u8 y2, y1;
};

struct DisplayEngine {
    DISPCNT dispcnt;

    BGCNT bgcnt[4];

    u16 bghofs[4], bgvofs[4];

    i16 bgp[2][4];
    i32 bgx[2], bgy[2];

    WINH winh[2];
    WINV winv[2];

    u8 masterbright;
};

// VRAM stuff

struct VRAMCNT {
    u8 mst;
    u8 ofs;

    bool vramen;
};

struct VRAMBank {
    VRAMCNT vramcnt;

    std::vector<u8> data;
};

// Tile stuff

struct TileLine {
    u16 tileBuf[8];

    u8 prios[8];
};

VRAMBank banks[9];

u16 palette[2][2 * 256];

std::vector<u8> fb;

DisplayEngine disp[2];

DISPSTAT dispstat[2];

u16 vcount;

// Scheduler
u64 idHBLANK, idScanline;

u16 readLCDC16(u32);

void drawScreen();

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

        drawScreen();

        update(fb.data());
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

    // Initialize VRAM banks

    // Banks A-D are 128KB
    banks[0].data.resize(0x20000);
    banks[1].data.resize(0x20000);
    banks[2].data.resize(0x20000);
    banks[3].data.resize(0x20000);

    // Bank E is 64KB
    banks[4].data.resize(0x10000);

    // Bank H is 32KB
    banks[7].data.resize(0x8000);

    // Banks F-G, I are 16 KB
    banks[5].data.resize(0x4000);
    banks[6].data.resize(0x4000);
    banks[8].data.resize(0x4000);

    fb.resize(0x30000);
}

u8 readVRAMCNT(int bank) {
    const auto &cnt = banks[bank].vramcnt;

    u8 data;

    data  =     cnt.mst;
    data |=     cnt.ofs    << 3;
    data |= (u8)cnt.vramen << 7;

    return data;
}

u8 readVRAMSTAT() {
    u8 data = 0;

    const auto &cnt2 = banks[2].vramcnt;
    const auto &cnt3 = banks[3].vramcnt;

    if (cnt2.vramen && (cnt2.mst == 2)) data |= 1;
    if (cnt3.vramen && (cnt3.mst == 2)) data |= 2;

    return data;
}

void writeVRAMCNT(int bank, u8 data) {
    auto &cnt = banks[bank].vramcnt;

    cnt.mst = (data >> 0) & 7;
    cnt.ofs = (data >> 3) & 3;

    cnt.vramen = data & (1 << 7);
}

u8 readVRAM8(u32 addr) {
    u8 data = 0;

    switch (addr & ~0x1FFFFF) {
        case 0x06000000: // Display Engine A, BG-VRAM
            {
                for (int i = 0; i < 7; i++) { // Banks A-G
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen || (cnt.mst != 1)) continue;

                    // Get bank address
                    u32 bankAddr = 0x06000000;

                    switch (i) {
                        case 0: case 1: case 2: case 3:
                            bankAddr += 0x20000 * cnt.ofs;
                            break;
                        case 5: case 6:
                            bankAddr += 0x10000 * (cnt.ofs >> 1) + 0x4000 * (cnt.ofs & 1);
                            break;
                        case 4:
                        default:
                            break;
                    }

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) data |= b.data[addr & limit];
                }
            }
            break;
        case 0x06200000: // Display Engine B, BG-VRAM
            {
                for (auto i : BGB_VRAM) {
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen) continue;

                    if (((i == 2) && (cnt.mst != 4)) || ((i > 7) && (cnt.mst != 1))) continue;

                    // Get bank address
                    u32 bankAddr = 0x06200000;

                    if (i == 8) bankAddr += 0x8000;

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) data |= b.data[addr & limit];
                }
            }
            break;
        case 0x06400000: // Display Engine A, OBJ-VRAM
            {
                for (auto i : OBJA_VRAM) { // Banks A-G
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen || (cnt.mst != 2)) continue;

                    // Get bank address
                    u32 bankAddr = 0x06400000;

                    switch (i) {
                        case 0: case 1:
                            bankAddr += 0x20000 * (cnt.ofs & 1);
                            break;
                        case 5: case 6:
                            bankAddr += 0x10000 * (cnt.ofs >> 1) + 0x4000 * (cnt.ofs & 1);
                            break;
                        case 4:
                        default:
                            break;
                    }

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) data |= b.data[addr & limit];
                }
            }
            break;
        case 0x06600000: // Display Engine B OBJ-VRAM
            {
                for (int i = 3; i < 9; i += 5) { // Banks D, I
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen) continue;

                    if (((i == 3) && (cnt.mst != 4)) || ((i == 8) && (cnt.mst != 2))) continue;

                    // Get bank address
                    const u32 bankAddr = 0x06600000;

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) data |= b.data[addr & limit];
                }
            }
            break;
        default:
            std::printf("[PPU       ] Unhandled VRAM read8 @ 0x%08X\n", addr);

            exit(0);
    }

    return data;
}

u16 readVRAM16(u32 addr) {
    u16 data = 0;

    switch (addr & ~0x1FFFFF) {
        case 0x06000000: // Display Engine A, BG-VRAM
            {
                for (int i = 0; i < 7; i++) { // Banks A-G
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen || (cnt.mst != 1)) continue;

                    // Get bank address
                    u32 bankAddr = 0x06000000;

                    switch (i) {
                        case 0: case 1: case 2: case 3:
                            bankAddr += 0x20000 * cnt.ofs;
                            break;
                        case 5: case 6:
                            bankAddr += 0x10000 * (cnt.ofs >> 1) + 0x4000 * (cnt.ofs & 1);
                            break;
                        case 4:
                        default:
                            break;
                    }

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) {
                        u16 tmp;

                        std::memcpy(&tmp, &b.data[addr & limit], sizeof(u16));

                        data |= tmp;
                    }
                }
            }
            break;
        case 0x06200000: // Display Engine B, BG-VRAM
            {
                for (auto i : BGB_VRAM) {
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen) continue;

                    if (((i == 2) && (cnt.mst != 4)) || ((i > 7) && (cnt.mst != 1))) continue;

                    // Get bank address
                    u32 bankAddr = 0x06200000;

                    if (i == 8) bankAddr += 0x8000;

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) {
                        u16 tmp;

                        std::memcpy(&tmp, &b.data[addr & limit], sizeof(u16));

                        data |= tmp;
                    }
                }
            }
            break;
        case 0x06400000: // Display Engine A, OBJ-VRAM
            {
                for (auto i : OBJA_VRAM) { // Banks A-G
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen || (cnt.mst != 2)) continue;

                    // Get bank address
                    u32 bankAddr = 0x06400000;

                    switch (i) {
                        case 0: case 1:
                            bankAddr += 0x20000 * (cnt.ofs & 1);
                            break;
                        case 5: case 6:
                            bankAddr += 0x10000 * (cnt.ofs >> 1) + 0x4000 * (cnt.ofs & 1);
                            break;
                        case 4:
                        default:
                            break;
                    }

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) {
                        u16 tmp;

                        std::memcpy(&tmp, &b.data[addr & limit], sizeof(u16));

                        data |= tmp;
                    }
                }
            }
            break;
        case 0x06600000: // Display Engine B OBJ-VRAM
            {
                for (int i = 3; i < 9; i += 5) { // Banks D, I
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen) continue;

                    if (((i == 3) && (cnt.mst != 4)) || ((i == 8) && (cnt.mst != 2))) continue;

                    // Get bank address
                    const u32 bankAddr = 0x06600000;

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) {
                        u16 tmp;

                        std::memcpy(&tmp, &b.data[addr & limit], sizeof(u16));

                        data |= tmp;
                    }
                }
            }
            break;
        default:
            std::printf("[PPU       ] Unhandled VRAM read32 @ 0x%08X\n", addr);

            exit(0);
    }

    return data;
}

u32 readVRAM32(u32 addr) {
    u32 data = 0;

    switch (addr & ~0x1FFFFF) {
        case 0x06000000: // Display Engine A, BG-VRAM
            {
                for (int i = 0; i < 7; i++) { // Banks A-G
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen || (cnt.mst != 1)) continue;

                    // Get bank address
                    u32 bankAddr = 0x06000000;

                    switch (i) {
                        case 0: case 1: case 2: case 3:
                            bankAddr += 0x20000 * cnt.ofs;
                            break;
                        case 5: case 6:
                            bankAddr += 0x10000 * (cnt.ofs >> 1) + 0x4000 * (cnt.ofs & 1);
                            break;
                        case 4:
                        default:
                            break;
                    }

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) {
                        u32 tmp;

                        std::memcpy(&tmp, &b.data[addr & limit], sizeof(u32));

                        data |= tmp;
                    }
                }
            }
            break;
        case 0x06200000: // Display Engine B, BG-VRAM
            {
                for (auto i : BGB_VRAM) {
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen) continue;

                    if (((i == 2) && (cnt.mst != 4)) || ((i > 7) && (cnt.mst != 1))) continue;

                    // Get bank address
                    u32 bankAddr = 0x06200000;

                    if (i == 8) bankAddr += 0x8000;

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) {
                        u32 tmp;

                        std::memcpy(&tmp, &b.data[addr & limit], sizeof(u32));

                        data |= tmp;
                    }
                }
            }
            break;
        case 0x06400000: // Display Engine A, OBJ-VRAM
            {
                for (auto i : OBJA_VRAM) { // Banks A-G
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen || (cnt.mst != 2)) continue;

                    // Get bank address
                    u32 bankAddr = 0x06400000;

                    switch (i) {
                        case 0: case 1:
                            bankAddr += 0x20000 * (cnt.ofs & 1);
                            break;
                        case 5: case 6:
                            bankAddr += 0x10000 * (cnt.ofs >> 1) + 0x4000 * (cnt.ofs & 1);
                            break;
                        case 4:
                        default:
                            break;
                    }

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) {
                        u32 tmp;

                        std::memcpy(&tmp, &b.data[addr & limit], sizeof(u32));

                        data |= tmp;
                    }
                }
            }
            break;
        case 0x06600000: // Display Engine B OBJ-VRAM
            {
                for (int i = 3; i < 9; i += 5) { // Banks D, I
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen) continue;

                    if (((i == 3) && (cnt.mst != 4)) || ((i == 8) && (cnt.mst != 2))) continue;

                    // Get bank address
                    const u32 bankAddr = 0x06600000;

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) {
                        u32 tmp;

                        std::memcpy(&tmp, &b.data[addr & limit], sizeof(u32));

                        data |= tmp;
                    }
                }
            }
            break;
        default:
            std::printf("[PPU       ] Unhandled VRAM read32 @ 0x%08X\n", addr);

            exit(0);
    }

    return data;
}


u32 readWRAM32(u32 addr) { // For ARM7
    u32 data = 0;

    const auto bankAddr = addr & 0x20000;

    addr &= 0x1FFFF;

    for (int i = 2; i < 4; i++) {
        const auto &b = banks[i];

        assert(b.vramcnt.ofs < 2);

        if (b.vramcnt.vramen && ((0x20000 * b.vramcnt.ofs) == bankAddr)) {
            u32 tmp;

            std::memcpy(&tmp, &b.data[addr], sizeof(u32));

            data |= tmp;
        }
    }

    return data;
}

u8 readLCDC8(u32 addr) {
    u8 data = 0;
    
    VRAMBank *b;

    // Select bank;
    switch (addr & ~0x3FFF) {
        case 0x06800000: case 0x06804000: case 0x06808000: case 0x0680C000:
        case 0x06810000: case 0x06814000: case 0x06818000: case 0x0681C000:
            b = &banks[0];
            break;
        case 0x06820000: case 0x06824000: case 0x06828000: case 0x0682C000:
        case 0x06830000: case 0x06834000: case 0x06838000: case 0x0683C000:
            b = &banks[1];
            break;
        case 0x06840000: case 0x06844000: case 0x06848000: case 0x0684C000:
        case 0x06850000: case 0x06854000: case 0x06858000: case 0x0685C000:
            b = &banks[2];
            break;
        case 0x06860000: case 0x06864000: case 0x06868000: case 0x0686C000:
        case 0x06870000: case 0x06874000: case 0x06878000: case 0x0687C000:
            b = &banks[3];
            break;
        case 0x06880000: case 0x06884000: case 0x06888000: case 0x0688C000:
            b = &banks[4];
            break;
        case 0x06890000:
            b = &banks[5];
            break;
        case 0x06894000:
            b = &banks[6];
            break;
        case 0x06898000: case 0x0689C000:
            b = &banks[7];
            break;
        case 0x068A0000:
            b = &banks[8];
            break;
        default:
            std::printf("[PPU       ] Unhandled LCDC read8 @ 0x%08X\n", addr);

            exit(0);
    }

    if (b->vramcnt.vramen) data = b->data[addr & (b->data.size() - 1)];

    return data;
}

u16 readLCDC16(u32 addr) {
    u16 data = 0;
    
    VRAMBank *b;

    // Select bank;
    switch (addr & ~0x3FFF) {
        case 0x06800000: case 0x06804000: case 0x06808000: case 0x0680C000:
        case 0x06810000: case 0x06814000: case 0x06818000: case 0x0681C000:
            b = &banks[0];
            break;
        case 0x06820000: case 0x06824000: case 0x06828000: case 0x0682C000:
        case 0x06830000: case 0x06834000: case 0x06838000: case 0x0683C000:
            b = &banks[1];
            break;
        case 0x06840000: case 0x06844000: case 0x06848000: case 0x0684C000:
        case 0x06850000: case 0x06854000: case 0x06858000: case 0x0685C000:
            b = &banks[2];
            break;
        case 0x06860000: case 0x06864000: case 0x06868000: case 0x0686C000:
        case 0x06870000: case 0x06874000: case 0x06878000: case 0x0687C000:
            b = &banks[3];
            break;
        case 0x06880000: case 0x06884000: case 0x06888000: case 0x0688C000:
            b = &banks[4];
            break;
        case 0x06890000:
            b = &banks[5];
            break;
        case 0x06894000:
            b = &banks[6];
            break;
        case 0x06898000: case 0x0689C000:
            b = &banks[7];
            break;
        case 0x068A0000:
            b = &banks[8];
            break;
        default:
            std::printf("[PPU       ] Unhandled LCDC read16 @ 0x%08X\n", addr);

            exit(0);
    }

    if (b->vramcnt.vramen) std::memcpy(&data, &b->data[addr & (b->data.size() - 1)], sizeof(u16));

    return data;
}

u32 readLCDC32(u32 addr) {
    u32 data = 0;
    
    VRAMBank *b;

    // Select bank;
    switch (addr & ~0x3FFF) {
        case 0x06800000: case 0x06804000: case 0x06808000: case 0x0680C000:
        case 0x06810000: case 0x06814000: case 0x06818000: case 0x0681C000:
            b = &banks[0];
            break;
        case 0x06820000: case 0x06824000: case 0x06828000: case 0x0682C000:
        case 0x06830000: case 0x06834000: case 0x06838000: case 0x0683C000:
            b = &banks[1];
            break;
        case 0x06840000: case 0x06844000: case 0x06848000: case 0x0684C000:
        case 0x06850000: case 0x06854000: case 0x06858000: case 0x0685C000:
            b = &banks[2];
            break;
        case 0x06860000: case 0x06864000: case 0x06868000: case 0x0686C000:
        case 0x06870000: case 0x06874000: case 0x06878000: case 0x0687C000:
            b = &banks[3];
            break;
        case 0x06880000: case 0x06884000: case 0x06888000: case 0x0688C000:
            b = &banks[4];
            break;
        case 0x06890000:
            b = &banks[5];
            break;
        case 0x06894000:
            b = &banks[6];
            break;
        case 0x06898000: case 0x0689C000:
            b = &banks[7];
            break;
        case 0x068A0000:
            b = &banks[8];
            break;
        default:
            std::printf("[PPU       ] Unhandled LCDC read32 @ 0x%08X\n", addr);

            exit(0);
    }

    if (b->vramcnt.vramen) std::memcpy(&data, &b->data[addr & (b->data.size() - 1)], sizeof(u32));

    return data;
}

void writeVRAM16(u32 addr, u16 data) {
    switch (addr & ~0x1FFFFF) {
        case 0x06000000: // Display Engine A, BG-VRAM
            {
                for (int i = 0; i < 7; i++) { // Banks A-G
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen || (cnt.mst != 1)) continue;

                    // Get bank address
                    u32 bankAddr = 0x06000000;

                    switch (i) {
                        case 0: case 1: case 2: case 3:
                            bankAddr += 0x20000 * cnt.ofs;
                            break;
                        case 5: case 6:
                            bankAddr += 0x10000 * (cnt.ofs >> 1) + 0x4000 * (cnt.ofs & 1);
                            break;
                        case 4:
                        default:
                            break;
                    }

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) std::memcpy(&b.data[addr & limit], &data, sizeof(u16));
                }
            }
            break;
        case 0x06200000: // Display Engine B, BG-VRAM
            {
                for (auto i : BGB_VRAM) {
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen) continue;

                    if (((i == 2) && (cnt.mst != 4)) || ((i > 7) && (cnt.mst != 1))) continue;

                    // Get bank address
                    u32 bankAddr = 0x06200000;

                    if (i == 8) bankAddr += 0x8000;

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) std::memcpy(&b.data[addr & limit], &data, sizeof(u16));
                }
            }
            break;
        case 0x06400000: // Display Engine A, OBJ-VRAM
            {
                for (auto i : OBJA_VRAM) { // Banks A-G
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen || (cnt.mst != 2)) continue;

                    // Get bank address
                    u32 bankAddr = 0x06400000;

                    switch (i) {
                        case 0: case 1:
                            bankAddr += 0x20000 * (cnt.ofs & 1);
                            break;
                        case 5: case 6:
                            bankAddr += 0x10000 * (cnt.ofs >> 1) + 0x4000 * (cnt.ofs & 1);
                            break;
                        case 4:
                        default:
                            break;
                    }

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) std::memcpy(&b.data[addr & limit], &data, sizeof(u16));
                }
            }
            break;
        case 0x06600000: // Display Engine B OBJ-VRAM
            {
                for (int i = 3; i < 9; i += 5) { // Banks D, I
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen) continue;

                    if (((i == 3) && (cnt.mst != 4)) || ((i == 8) && (cnt.mst != 2))) continue;

                    // Get bank address
                    const u32 bankAddr = 0x06600000;

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) std::memcpy(&b.data[addr & limit], &data, sizeof(u16));
                }
            }
            break;
        default:
            std::printf("[PPU       ] Unhandled VRAM write16 @ 0x%08X = 0x%04X\n", addr, data);

            exit(0);
    }
}

void writeVRAM32(u32 addr, u32 data) {
    switch (addr & ~0x1FFFFF) {
        case 0x06000000: // Display Engine A, BG-VRAM
            {
                for (int i = 0; i < 7; i++) { // Banks A-G
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen || (cnt.mst != 1)) continue;

                    // Get bank address
                    u32 bankAddr = 0x06000000;

                    switch (i) {
                        case 0: case 1: case 2: case 3:
                            bankAddr += 0x20000 * cnt.ofs;
                            break;
                        case 5: case 6:
                            bankAddr += 0x10000 * (cnt.ofs >> 1) + 0x4000 * (cnt.ofs & 1);
                            break;
                        case 4:
                        default:
                            break;
                    }

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) std::memcpy(&b.data[addr & limit], &data, sizeof(u32));
                }
            }
            break;
        case 0x06200000: // Display Engine B, BG-VRAM
            {
                for (auto i : BGB_VRAM) {
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen) continue;

                    if (((i == 2) && (cnt.mst != 4)) || ((i > 7) && (cnt.mst != 1))) continue;

                    // Get bank address
                    u32 bankAddr = 0x06200000;

                    if (i == 8) bankAddr += 0x8000;

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) std::memcpy(&b.data[addr & limit], &data, sizeof(u32));
                }
            }
            break;
        case 0x06400000: // Display Engine A, OBJ-VRAM
            {
                for (auto i : OBJA_VRAM) { // Banks A-G
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen || (cnt.mst != 2)) continue;

                    // Get bank address
                    u32 bankAddr = 0x06400000;

                    switch (i) {
                        case 0: case 1:
                            bankAddr += 0x20000 * (cnt.ofs & 1);
                            break;
                        case 5: case 6:
                            bankAddr += 0x10000 * (cnt.ofs >> 1) + 0x4000 * (cnt.ofs & 1);
                            break;
                        case 4:
                        default:
                            break;
                    }

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) std::memcpy(&b.data[addr & limit], &data, sizeof(u32));
                }
            }
            break;
        case 0x06600000: // Display Engine B OBJ-VRAM
            {
                for (int i = 3; i < 9; i += 5) { // Banks D, I
                    auto &b = banks[i];

                    auto &cnt = b.vramcnt;

                    if (!cnt.vramen) continue;

                    if (((i == 3) && (cnt.mst != 4)) || ((i == 8) && (cnt.mst != 2))) continue;

                    // Get bank address
                    const u32 bankAddr = 0x06600000;

                    const auto limit = b.data.size() - 1;

                    if (bankAddr == (addr & ~limit)) std::memcpy(&b.data[addr & limit], &data, sizeof(u32));
                }
            }
            break;
        default:
            std::printf("[PPU       ] Unhandled VRAM write32 @ 0x%08X = 0x%08X\n", addr, data);

            exit(0);
    }
}

void writeWRAM32(u32 addr, u32 data) { // For ARM7
    const auto bankAddr = addr & 0x20000;

    addr &= 0x1FFFF;

    for (int i = 2; i < 4; i++) {
        auto &b = banks[i];

        assert(b.vramcnt.ofs < 2);

        if (b.vramcnt.vramen && ((0x20000 * b.vramcnt.ofs) == bankAddr)) std::memcpy(&b.data[addr], &data, sizeof(u32));
    }
}

void writeLCDC8(u32 addr, u8 data) {
    std::printf("[PPU       ] Unhandled LCDC write8 @ 0x%08X = 0x%02X\n", addr, data);

    exit(0);
}

void writeLCDC16(u32 addr, u16 data) {
    VRAMBank *b;

    // Select bank;
    switch (addr & ~0x3FFF) {
        case 0x06800000: case 0x06804000: case 0x06808000: case 0x0680C000:
        case 0x06810000: case 0x06814000: case 0x06818000: case 0x0681C000:
            b = &banks[0];
            break;
        case 0x06820000: case 0x06824000: case 0x06828000: case 0x0682C000:
        case 0x06830000: case 0x06834000: case 0x06838000: case 0x0683C000:
            b = &banks[1];
            break;
        case 0x06840000: case 0x06844000: case 0x06848000: case 0x0684C000:
        case 0x06850000: case 0x06854000: case 0x06858000: case 0x0685C000:
            b = &banks[2];
            break;
        case 0x06860000: case 0x06864000: case 0x06868000: case 0x0686C000:
        case 0x06870000: case 0x06874000: case 0x06878000: case 0x0687C000:
            b = &banks[3];
            break;
        case 0x06880000: case 0x06884000: case 0x06888000: case 0x0688C000:
            b = &banks[4];
            break;
        case 0x06890000:
            b = &banks[5];
            break;
        case 0x06894000:
            b = &banks[6];
            break;
        case 0x06898000: case 0x0689C000:
            b = &banks[7];
            break;
        case 0x068A0000:
            b = &banks[8];
            break;
        default:
            std::printf("[PPU       ] Unhandled LCDC write16 @ 0x%08X = 0x%04X\n", addr, data);

            exit(0);
    }

    if (b->vramcnt.vramen) std::memcpy(&b->data[addr & (b->data.size() - 1)], &data, sizeof(u16));
}

u16 getColor4BPP(int disp, int pal, int num) {
    u16 data;

    std::memcpy(&data, &palette[disp][16 * pal + num], sizeof(u16));

    return data;
}

u16 getColor8BPP(int disp, int num) {
    u16 data;

    std::memcpy(&data, &palette[disp][num], sizeof(u16));

    return data;
}

void writePal16(u32 addr, u16 data) {
    const bool pal = addr & (1 << 10);

    std::memcpy(&palette[pal][(addr >> 1) & 0x1FF], &data, sizeof(u16));
}

void writePal32(u32 addr, u32 data) {
    const bool pal = addr & (1 << 10);

    std::memcpy(&palette[pal][(addr >> 1) & 0x1FF], &data, sizeof(u32));
}

void decode4BPP(int d, TileLine &tileLine, u32 baseAddr, u32 charBase, int pal, int num, int tileY, bool flipX) {
    const u32 base = baseAddr + charBase + 32 * num + 4 * tileY;

    if (flipX) {
        for (auto x = 0; x < 4; x++) {
            const auto tile = readVRAM8(base + x);

            const auto p0 = tile & 0xF;
            const auto p1 = tile >> 4;

            const auto flip1 = ((2 * x + 0) ^ 7) & 0xF;
            const auto flip2 = ((2 * x + 1) ^ 7) & 0xF;

            const auto col0 = getColor4BPP(d, pal, p0);
            const auto col1 = getColor4BPP(d, pal, p1);

            if (!p0) tileLine.prios[flip1] = 5;
            if (!p1) tileLine.prios[flip2] = 5;

            std::memcpy(&tileLine.tileBuf[flip1], &col0, sizeof(u16));
            std::memcpy(&tileLine.tileBuf[flip2], &col1, sizeof(u16));
        }
    } else {
        for (auto x = 0; x < 4; x++) {
            const auto tile = readVRAM8(base + x);

            const auto p0 = tile & 15;
            const auto p1 = tile >> 4;

            const auto flip1 = 2 * x + 0;
            const auto flip2 = 2 * x + 1;

            const auto col0 = getColor4BPP(d, pal, p0);
            const auto col1 = getColor4BPP(d, pal, p1);

            if (!p0) tileLine.prios[flip1] = 5;
            if (!p1) tileLine.prios[flip2] = 5;

            std::memcpy(&tileLine.tileBuf[flip1], &col0, sizeof(u16));
            std::memcpy(&tileLine.tileBuf[flip2], &col1, sizeof(u16));
        }
    }
}

void decode8BPP(int d, TileLine &tileLine, u32 baseAddr, u32 charBase, int num, int tileY, bool flipX) {
    const u32 base = baseAddr + charBase + 64 * num + 8 * tileY;

    if (flipX) {
        for (auto x = 7; x > 0; x--) {
            const auto tile = readVRAM8(base + x);

            const auto col = getColor8BPP(d, tile);

            if (!tile) tileLine.prios[x] = 5;

            std::memcpy(&tileLine.tileBuf[x], &col, sizeof(u16));
        }
    } else {
        for (auto x = 0; x < 7; x++) {
            const auto tile = readVRAM8(base + x);

            const auto col = getColor8BPP(d, tile);

            if (!tile) tileLine.prios[x] = 5;

            std::memcpy(&tileLine.tileBuf[x], &col, sizeof(u16));
        }
    }
}

void drawDE(int idx) {
    const u32 baseAddr = (idx) ? 0x06200000 : 0x06000000;

    const u32 fbBaseAddr = (idx) ? 0x18000 : 0;

    const auto &d = disp[idx];

    const auto &cnt = d.dispcnt;

    if (cnt.forcedblank) { // Draw white pixels
        std::memset(&fb[fbBaseAddr], 0xFF, 0x18000);

        return;
    }

    u16 tmpFB[192][256];

    u8 prios[192][256];

    std::memset(tmpFB, 0, sizeof(tmpFB));
    std::memset(prios, 5, sizeof(prios));

    for (int i = 3; i >= 0; i--) {
        if (!cnt.bgen[i]) continue;

        const auto &bgcnt = d.bgcnt[i];

        u32 charBase = 0x4000 * bgcnt.charbase;
        u32 scrBase  = 0x800  * bgcnt.scrbase;

        if (!idx) {
            charBase += 0x10000 * cnt.charbase;
            scrBase  += 0x10000 * cnt.scrbase;
        }

        const auto hofs = d.bghofs[i];
        const auto vofs = d.bgvofs[i];

        for (int line = 0; line < 192; line++) {
            auto drawX = -(int)(hofs % 8);
            auto gridX =  (int)(hofs / 8);

            const auto tileY = (line + vofs) % 8;
            const auto gridY = (line + vofs) / 8;

            const auto scrX = (gridX / 32) % 2;
            const auto scrY = (gridY / 32) % 2;

            u32 base = scrBase + 64 * (gridY % 32);

            gridX %= 32;

            u32 baseAdjust;

            switch (bgcnt.scrsize) {
                case 0:
                    baseAdjust = 0;
                    break;
                case 1:
                    baseAdjust = 2048;

                    base += 2048 * scrX;
                    break;
                case 2:
                    baseAdjust = 0;

                    base += 2048 * scrY;
                    break;
                case 3:
                    baseAdjust = 2048;

                    base += 2048 * scrX + 4096 * scrY;
                    break;
            }

            if (scrX == 1) baseAdjust *= -1;

            do {
                do {
                    const auto offset = base + 2 * gridX;

                    ++gridX;

                    const auto tile = readVRAM16(baseAddr + offset);

                    const auto num = tile & 0x3FF;
                    const auto pal = tile >> 12;

                    const bool flipX = tile & (1 << 10);
                    const bool flipY = tile & (1 << 11);

                    const auto _tileY = (flipY) ? (tileY ^ 7) & 7 : tileY;

                    TileLine tileLine;

                    std::memset(tileLine.prios, bgcnt.prio, sizeof(tileLine.prios));

                    if (bgcnt.pal256) {
                        decode8BPP(idx, tileLine, baseAddr, charBase, num, _tileY, flipX);
                    } else {
                        decode4BPP(idx, tileLine, baseAddr, charBase, pal, num, _tileY, flipX);
                    }

                    if ((drawX >= 0) && (drawX <= 248)) {
                        for (int x = 0; x < 8; x++) {
                            if (tileLine.prios[x] <= prios[line][drawX + x]) {
                                prios[line][drawX + x] = tileLine.prios[x];

                                tmpFB[line][drawX + x] = tileLine.tileBuf[x];
                            }
                        }

                        drawX += 8;
                    } else {
                        int x   = 0;
                        int max = 8;

                        if (drawX < 0) {
                            x = -drawX;

                            drawX = 0;

                            for (; x < max; x++) {
                                if (tileLine.prios[x] <= prios[line][drawX]) {
                                    prios[line][drawX] = tileLine.prios[x];

                                    tmpFB[line][drawX++] = tileLine.tileBuf[x];
                                }
                            }
                        } else if (drawX > 248) {
                            max -= drawX - 248;

                            for (; x < max; x++) {
                                if (tileLine.prios[x] <= prios[line][drawX]) {
                                    prios[line][drawX] = tileLine.prios[x];

                                    tmpFB[line][drawX++] = tileLine.tileBuf[x];
                                }
                            }
                        }
                    }
                } while (gridX < 32);

                base += baseAdjust;

                baseAdjust *= -1;

                gridX = 0;
            } while (drawX < 256);
        }
    }

    std::memcpy(&fb[fbBaseAddr], (u8 *)tmpFB, 0x18000);
}

void drawLCDC(int b) {
    for (int i = 0; i < 0x18000; i += 4) {
        const auto data = readLCDC32(0x06800000 + 0x20000 * b + i);

        std::memcpy(&fb[i], &data, sizeof(4));
    }
}

void drawScreen() {
    const auto &cnta = disp[0].dispcnt;
    const auto &cntb = disp[1].dispcnt;

    // Draw Display Engine A
    switch (cnta.dispmode) {
        case 0: // Display off
            std::memset(&fb[0], 0xFF, 0x18000);
            break;
        case 1: // Normal display
            drawDE(0);
            break;
        case 2: // VRAM display
            drawLCDC(cnta.bselect);
            break;
        default:
            std::printf("[DISPA     ] Unhandled display mode %u\n", cnta.dispmode);

            exit(0);
    }

    // Draw Display Engine B
    switch (cntb.dispmode) {
        case 0: // Display off
            std::memset(&fb[0x18000], 0xFF, 0x18000);
            break;
        case 1: // Normal display
            drawDE(1);
            break;
        default:
            std::printf("[DISPB     ] Unhandled display mode %u\n", cnta.dispmode);

            exit(0);
    }
}

void writeLCDC32(u32 addr, u32 data) {
    VRAMBank *b;

    // Select bank;
    switch (addr & ~0x3FFF) {
        case 0x06800000: case 0x06804000: case 0x06808000: case 0x0680C000:
        case 0x06810000: case 0x06814000: case 0x06818000: case 0x0681C000:
            b = &banks[0];
            break;
        case 0x06820000: case 0x06824000: case 0x06828000: case 0x0682C000:
        case 0x06830000: case 0x06834000: case 0x06838000: case 0x0683C000:
            b = &banks[1];
            break;
        case 0x06840000: case 0x06844000: case 0x06848000: case 0x0684C000:
        case 0x06850000: case 0x06854000: case 0x06858000: case 0x0685C000:
            b = &banks[2];
            break;
        case 0x06860000: case 0x06864000: case 0x06868000: case 0x0686C000:
        case 0x06870000: case 0x06874000: case 0x06878000: case 0x0687C000:
            b = &banks[3];
            break;
        case 0x06880000: case 0x06884000: case 0x06888000: case 0x0688C000:
            b = &banks[4];
            break;
        case 0x06890000:
            b = &banks[5];
            break;
        case 0x06894000:
            b = &banks[6];
            break;
        case 0x06898000: case 0x0689C000:
            b = &banks[7];
            break;
        case 0x068A0000:
            b = &banks[8];
            break;
        default:
            std::printf("[PPU       ] Unhandled LCDC write32 @ 0x%08X = 0x%08X\n", addr, data);

            exit(0);
    }

    if (b->vramcnt.vramen) std::memcpy(&b->data[addr & (b->data.size() - 1)], &data, sizeof(u32));
}

u16 read16(int idx, u32 addr) {
    u16 data;

    auto &d = disp[idx];

    switch (addr & ~0x1000) {
        case static_cast<u32>(PPUReg::DISPSTAT):
            //std::printf("[DISPA+B   ] Read16 @ DISPSTAT\n");

            data  = (u16)dispstat[1].vblank   << 0;
            data |= (u16)dispstat[1].hblank   << 1;
            data |= (u16)dispstat[1].vcounter << 2;
            data |= (u16)dispstat[1].virqen   << 3;
            data |= (u16)dispstat[1].hirqen   << 4;
            data |= (u16)dispstat[1].lycirqen << 5;
            
            data |= (dispstat[1].lyc << 7);
            break;
        case static_cast<u32>(PPUReg::VCOUNT):
            //std::printf("[DISPA+B   ] Read16 @ VCOUNT\n");

            return vcount;
        case static_cast<u32>(PPUReg::BGCNT) + 0:
        case static_cast<u32>(PPUReg::BGCNT) + 2:
        case static_cast<u32>(PPUReg::BGCNT) + 4:
        case static_cast<u32>(PPUReg::BGCNT) + 6:
            {
                const auto _idx = (addr >> 1) & 3;

                std::printf("[DISP%c     ] Read16 @ BG%uCNT\n", (idx) ? 'B' : 'A', _idx);

                const auto &bgcnt = d.bgcnt[_idx];

                data = (u16)bgcnt.prio;

                data |= (u16)bgcnt.charbase << 2;
                data |= (u16)bgcnt.mosaicen << 6;
                data |= (u16)bgcnt.pal256   << 7;
                data |= (u16)bgcnt.scrbase  << 8;

                data |= (u16)bgcnt.extpalslot << 13;

                data |= (u16)bgcnt.scrsize << 14;
            }
            break;
        case static_cast<u32>(PPUReg::WININ):
            std::printf("[DISP%c     ] Read16 @ WININ\n", (idx) ? 'B' : 'A');
            return 0;
        case static_cast<u32>(PPUReg::WINOUT):
            std::printf("[DISP%c     ] Read16 @ WINOUT\n", (idx) ? 'B' : 'A');
            return 0;
        case static_cast<u32>(PPUReg::DISP3DCNT):
            if (idx) {
                std::printf("[DISP%c     ] Unhandled read16 @ 0x%08X\n", (idx) ? 'B' : 'A', addr);

                return 0;
            }

            std::printf("[DISPA     ] Read16 @ DISP3DCNT\n");
            return 0;
        default:
            std::printf("[DISP%c     ] Unhandled read16 @ 0x%08X\n", (idx) ? 'B' : 'A', addr);

            exit(0);
    }

    return data;
}

u32 read32(int idx, u32 addr) {
    u32 data;

    auto &d = disp[idx];

    switch (addr & ~0x1000) {
        case static_cast<u32>(PPUReg::DISPCNT):
            {
                std::printf("[DISP%c     ] Read32 @ DISPCNT\n", (idx) ? 'B' : 'A');

                const auto &cnt = d.dispcnt;

                data  = (u32)cnt.bgmode;
                data |= (u32)cnt.bg3Den << 3;
                data |= (u32)cnt.obj1D  << 4;

                data |= (u32)cnt.bitobjdim << 5;
                data |= (u32)cnt.bitobj1D  << 6;

                data |= (u32)cnt.forcedblank << 7;

                for (int i = 0; i < 5; i++) data |= (u32)cnt.bgen[i] << (8 + i);

                for (int i = 0; i < 3; i++) data |= (u32)cnt.windowen[i] << (13 + i);

                data |= (u32)cnt.dispmode << 16;
                data |= (u32)cnt.bselect  << 18;
                data |= (u32)cnt.objbound << 20;

                data |= (u32)cnt.bitobjbound << 22;
                data |= (u32)cnt.objblanking << 23;

                data |= (u32)cnt.charbase  << 24;
                data |= (u32)cnt.scrbase   << 27;
                data |= (u32)cnt.bgextpal  << 30;
                data |= (u32)cnt.objextpal << 31;
            }
            break;
        default:
            std::printf("[DISP%c     ] Unhandled read32 @ 0x%08X\n", (idx) ? 'B' : 'A', addr);
            
            exit(0);
    }

    return data;
}

void write16(int idx, u32 addr, u16 data) {
    auto &d = disp[idx];
    
    switch (addr & ~0x1000) {
        case static_cast<u32>(PPUReg::DISPSTAT):
            std::printf("[DISPA+B   ] Write16 @ DISPSTAT = 0x%04X\n", data);

            dispstat[1].virqen   = data & (1 << 3);
            dispstat[1].hirqen   = data & (1 << 4);
            dispstat[1].lycirqen = data & (1 << 5);

            dispstat[1].lyc = data >> 7;
            break;
        case static_cast<u32>(PPUReg::BGCNT) + 0:
        case static_cast<u32>(PPUReg::BGCNT) + 2:
        case static_cast<u32>(PPUReg::BGCNT) + 4:
        case static_cast<u32>(PPUReg::BGCNT) + 6:
            {
                const auto _idx = (addr >> 1) & 3;

                std::printf("[DISP%c     ] Write16 @ BG%uCNT = 0x%04X\n", (idx) ? 'B' : 'A', _idx, data);

                auto &bgcnt = d.bgcnt[_idx];

                bgcnt.prio = (data >> 0) & 3;

                bgcnt.charbase = (data >> (2)) & 0xF;
                bgcnt.mosaicen = data & (1 << 6);
                bgcnt.pal256   = data & (1 << 7);
                bgcnt.scrbase  = (data >> 8) & 0x1F;

                bgcnt.extpalslot = data & (1 << 13);

                bgcnt.scrsize = (data >> 14) & 3;
            }
            break;
        case static_cast<u32>(PPUReg::BG2PA):
        case static_cast<u32>(PPUReg::BG2PB):
        case static_cast<u32>(PPUReg::BG2PC):
        case static_cast<u32>(PPUReg::BG2PD):
        case static_cast<u32>(PPUReg::BG3PA):
        case static_cast<u32>(PPUReg::BG3PB):
        case static_cast<u32>(PPUReg::BG3PC):
        case static_cast<u32>(PPUReg::BG3PD):
            {
                const auto bg = (addr >> 4) & 1;
                const auto p  = (addr >> 1) & 3;

                std::printf("[DISP%c     ] Write16 @ BG%uP%c = 0x%04X\n", (idx) ? 'B' : 'A', 2 + bg, 'A' + p, data);

                d.bgp[bg][p + 0] = data;
                d.bgp[bg][p + 1] = data >> 16;
            }
            break;
        case static_cast<u32>(PPUReg::WIN0H):
        case static_cast<u32>(PPUReg::WIN1H):
            {
                const auto _idx = (addr >> 1) & 1;

                std::printf("[DISP%c     ] Write32 @ WIN%uH = 0x%04X\n", (idx) ? 'B' : 'A', _idx, data);

                d.winh[_idx].x2 = data;
                d.winh[_idx].x1 = data >>  8;
            }
            break;
        case static_cast<u32>(PPUReg::WIN0V):
        case static_cast<u32>(PPUReg::WIN1V):
            {
                const auto _idx = (addr >> 1) & 1;

                std::printf("[DISP%c     ] Write32 @ WIN%uV = 0x%04X\n", (idx) ? 'B' : 'A', _idx, data);

                d.winv[_idx].y2 = data;
                d.winv[_idx].y1 = data >>  8;
            }
            break;
        case static_cast<u32>(PPUReg::WININ):
            std::printf("[DISP%c     ] Write16 @ WININ = 0x%04X\n", (idx) ? 'B' : 'A', data);
            break;
        case static_cast<u32>(PPUReg::WINOUT):
            std::printf("[DISP%c     ] Write16 @ WINOUT = 0x%04X\n", (idx) ? 'B' : 'A', data);
            break;
        case static_cast<u32>(PPUReg::BLDCNT):
            std::printf("[DISP%c     ] Write16 @ BLDCNT = 0x%04X\n", (idx) ? 'B' : 'A', data);
            break;
        case static_cast<u32>(PPUReg::BLDALPHA):
            std::printf("[DISP%c     ] Write16 @ BLDALPHA = 0x%04X\n", (idx) ? 'B' : 'A', data);
            break;
        case static_cast<u32>(PPUReg::BLDY):
            std::printf("[DISP%c     ] Write16 @ BLDY = 0x%04X\n", (idx) ? 'B' : 'A', data);
            break;
        case static_cast<u32>(PPUReg::DISP3DCNT):
            if (idx) {
                std::printf("[DISP%c     ] Unhandled write32 @ 0x%08X = 0x%08X\n", (idx) ? 'B' : 'A', addr, data);

                return;
            }

            std::printf("[DISPA     ] Write16 @ DISP3DCNT = 0x%04X\n", data);
            break;
        case static_cast<u32>(PPUReg::MASTERBRIGHT):
            std::printf("[DISP%c     ] Write16 @ MASTERBRIGHT = 0x%04X\n", (idx) ? 'B' : 'A', data);

            d.masterbright = data & 0x1F;
            break;
        default:
            std::printf("[DISP%c     ] Unhandled write16 @ 0x%08X = 0x%04X\n", (idx) ? 'B' : 'A', addr, data);

            exit(0);
    }
}

void write32(int idx, u32 addr, u32 data) {
    auto &d = disp[idx];
    
    switch (addr & ~0x1000) {
        case static_cast<u32>(PPUReg::DISPCNT):
            {
                std::printf("[DISP%c     ] Write32 @ DISPCNT = 0x%08X\n", (idx) ? 'B' : 'A', data);

                auto &cnt = d.dispcnt;

                cnt.bgmode = data & 3;
                cnt.bg3Den = data & (1 << 3);
                cnt.obj1D  = data & (1 << 4);

                cnt.bitobjdim = data & (1 << 5);
                cnt.bitobj1D  = data & (1 << 6);

                cnt.forcedblank = data & (1 << 7);

                for (int i = 0; i < 5; i++) cnt.bgen[i] = data & (1 << (8 + i));

                for (int i = 0; i < 3; i++) cnt.windowen[i] = data & (1 << (13 + i));

                cnt.dispmode = (data >> 16) & 3;
                cnt.bselect  = (data >> 18) & 3;
                cnt.objbound = (data >> 20) & 3;

                cnt.bitobjbound = data & (1 << 22);
                cnt.objblanking = data & (1 << 23);

                cnt.charbase  = (data >> 24) & 7;
                cnt.scrbase   = (data >> 27) & 7;
                cnt.bgextpal  = data & (1 << 30);
                cnt.objextpal = data & (1 << 31);
            }
            break;
        case static_cast<u32>(PPUReg::DISPSTAT):
            if (idx) {
                std::printf("[DISP%c     ] Unhandled write32 @ 0x%08X = 0x%08X\n", (idx) ? 'B' : 'A', addr, data);

                return;
            }

            std::printf("[DISPA+B   ] Write32 @ DISPSTAT = 0x%08X\n", data);

            dispstat[1].virqen   = data & (1 << 3);
            dispstat[1].hirqen   = data & (1 << 4);
            dispstat[1].lycirqen = data & (1 << 5);

            dispstat[1].lyc = data >> 7;
            break;
        case static_cast<u32>(PPUReg::BGCNT) + 0:
        case static_cast<u32>(PPUReg::BGCNT) + 4:
            {
                const auto _idx = (addr >> 2) & 1;

                std::printf("[DISP%c     ] Write32 @ BG%u/%uCNT = 0x%08X\n", (idx) ? 'B' : 'A', _idx, _idx + 1, data);

                for (int i = 0; i < 2; i++) {
                    auto &bgcnt = d.bgcnt[2 * _idx + i];

                    bgcnt.prio = (data >> (16 * i + 0)) & 3;

                    bgcnt.charbase = (data >> (16 * i + 2)) & 0xF;
                    bgcnt.mosaicen = data & (1 << (16 * i + 6));
                    bgcnt.pal256   = data & (1 << (16 * i + 7));
                    bgcnt.scrbase  = (data >> (16 * i + 8)) & 0x1F;

                    bgcnt.extpalslot = data & (1 << (16 * i + 13));

                    bgcnt.scrsize = (data >> (16 * i + 14)) & 3;
                }
            }
            break;
        case static_cast<u32>(PPUReg::BGHOFS) + 0x0:
        case static_cast<u32>(PPUReg::BGHOFS) + 0x4:
        case static_cast<u32>(PPUReg::BGHOFS) + 0x8:
        case static_cast<u32>(PPUReg::BGHOFS) + 0xC:
            {
                const auto _idx = (addr >> 2) & 3;

                std::printf("[DISP%c     ] Write32 @ BG%uHOFS/BG%uVOFS = 0x%08X\n", (idx) ? 'B' : 'A', _idx, _idx, data);

                d.bghofs[_idx] = (data >>  0) & 0x1FF;
                d.bgvofs[_idx] = (data >> 16) & 0x1FF;
            }
            break;
        case static_cast<u32>(PPUReg::BG2PA):
        case static_cast<u32>(PPUReg::BG2PC):
        case static_cast<u32>(PPUReg::BG3PA):
        case static_cast<u32>(PPUReg::BG3PC):
            {
                const auto bg = (addr >> 4) & 1;
                const auto p  = (addr >> 1) & 2;

                std::printf("[DISP%c     ] Write32 @ BG%uP%c/%c = 0x%08X\n", (idx) ? 'B' : 'A', 2 + bg, 'A' + p, 'B' + p, data);

                d.bgp[bg][p + 0] = data;
                d.bgp[bg][p + 1] = data >> 16;
            }
            break;
        case static_cast<u32>(PPUReg::BG2X_L):
        case static_cast<u32>(PPUReg::BG2Y_L):
        case static_cast<u32>(PPUReg::BG3X_L):
        case static_cast<u32>(PPUReg::BG3Y_L):
            {
                const auto bg = (addr >> 4) & 1;
                const auto xy = (addr >> 2) & 1;

                std::printf("[DISP%c     ] Write32 @ BG%u%c = 0x%08X\n", (idx) ? 'B' : 'A', 2 + bg, 'X' + xy, data);

                d.bgp[bg][xy] = (i32)(data << 4) >> 4;
            }
            break;
        case static_cast<u32>(PPUReg::WIN0H):
            {
                std::printf("[DISP%c     ] Write32 @ WIN0/1H = 0x%08X\n", (idx) ? 'B' : 'A', data);

                d.winh[0].x2 = data;
                d.winh[0].x1 = data >>  8;
                d.winh[1].x2 = data >> 16;
                d.winh[1].x1 = data >> 24;
            }
            break;
        case static_cast<u32>(PPUReg::WIN0V):
            {
                std::printf("[DISP%c     ] Write32 @ WIN0/1V = 0x%08X\n", (idx) ? 'B' : 'A', data);

                d.winv[0].y2 = data;
                d.winv[0].y1 = data >>  8;
                d.winv[1].y2 = data >> 16;
                d.winv[1].y1 = data >> 24;
            }
            break;
        case static_cast<u32>(PPUReg::WININ):
            std::printf("[DISP%c     ] Write32 @ WININ/OUT = 0x%08X\n", (idx) ? 'B' : 'A', data);
            break;
        case static_cast<u32>(PPUReg::MOSAIC):
            std::printf("[DISP%c     ] Write32 @ MOSAIC = 0x%08X\n", (idx) ? 'B' : 'A', data);
            break;
        case static_cast<u32>(PPUReg::BLDCNT):
            std::printf("[DISP%c     ] Write32 @ BLDCNT/BLDALPHA = 0x%08X\n", (idx) ? 'B' : 'A', data);
            break;
        case static_cast<u32>(PPUReg::BLDY):
            std::printf("[DISP%c     ] Write32 @ BLDY = 0x%08X\n", (idx) ? 'B' : 'A', data);
            break;
        case static_cast<u32>(PPUReg::DISP3DCNT):
            if (idx) {
                std::printf("[DISP%c     ] Unhandled write32 @ 0x%08X = 0x%08X\n", (idx) ? 'B' : 'A', addr, data);

                return;
            }

            std::printf("[DISP%c     ] Write32 @ DISP3DCNT = 0x%08X\n", (idx) ? 'B' : 'A', data);
            break;
        case static_cast<u32>(PPUReg::DISPCAPCNT):
            if (idx) {
                std::printf("[DISP%c     ] Unhandled write32 @ 0x%08X = 0x%08X\n", (idx) ? 'B' : 'A', addr, data);

                return;
            }

            std::printf("[DISP%c     ] Write32 @ DISPCAPCNT = 0x%08X\n", (idx) ? 'B' : 'A', data);
            break;
        case static_cast<u32>(PPUReg::DISPMMEMFIFO):
            if (idx) {
                std::printf("[DISP%c     ] Unhandled write32 @ 0x%08X = 0x%08X\n", (idx) ? 'B' : 'A', addr, data);

                return;
            }

            std::printf("[DISP%c     ] Write32 @ DISPMMEMFIFO = 0x%08X\n", (idx) ? 'B' : 'A', data);
            break;
        case static_cast<u32>(PPUReg::MASTERBRIGHT):
            std::printf("[DISP%c     ] Write32 @ MASTERBRIGHT = 0x%08X\n", (idx) ? 'B' : 'A', data);

            d.masterbright = data & 0x1F;
            break;
        default:
            std::printf("[DISP%c     ] Unhandled write32 @ 0x%08X = 0x%08X\n", (idx) ? 'B' : 'A', addr, data);
            break;
    }
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

void writeDISPSTAT7(u16 data) {
    dispstat[0].virqen   = data & (1 << 3);
    dispstat[0].hirqen   = data & (1 << 4);
    dispstat[0].lycirqen = data & (1 << 5);

    dispstat[0].lyc = data >> 7;
}

}
