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

struct DISPSTAT {
    bool vblank, hblank, vcounter;

    bool virqen, hirqen, lycirqen;

    u16 lyc;
};

struct VRAMCNT {
    u8 mst;
    u8 ofs;

    bool vramen;
};

struct VRAMBank {
    VRAMCNT vramcnt;

    std::vector<u8> data;
};

VRAMBank banks[9];

std::vector<u8> fb;

DISPSTAT dispstat[2];

u16 vcount;

// Scheduler
u64 idHBLANK, idScanline;

u16 readLCDC16(u32);

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

        // Copy LCDC data to frame buffer
        for (int i = 0; i < 0x18000; i++) {
            const auto data = readLCDC16(0x06800000 + i);

            std::memcpy(&fb[i], &data, sizeof(u16));
        }

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
    std::printf("[PPU       ] Unhandled VRAM read16 @ 0x%08X\n", addr);

    exit(0);
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
