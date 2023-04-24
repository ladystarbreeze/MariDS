/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "dma.hpp"

#include <cassert>
#include <cstdio>

#include "bus.hpp"

namespace nds::dma {

constexpr const char *sync9Names[] = {
    "Immediately",
    "VBLANK", "HBLANK", "VDRAW", "LCDC",
    "NDS Slot", "GBA Slot",
    "GXFIFO",
};

enum class Sync9 {
    Immediately,
    VBLANK, HBLANK, VDRAW, LCDC,
    NDSSlot, GBASlot,
    GXFIFO,
};

enum class DMAReg {
    DMASAD   = 0x040000B0,
    DMADAD   = 0x040000B4,
    DMACNT   = 0x040000B8,
    DMACNT_H = 0x040000BA,
    DMAFILL  = 0x040000E0,
};

struct DMACNT7 {
    u8   dstcnt;
    u8   srccnt;
    bool repeat;
    bool isWord;
    u8   sync;
    bool irqen;
    bool dmaen;
};

struct DMACNT9 {
    u8   dstcnt;
    u8   srccnt;
    bool repeat;
    bool isWord;
    u8   sync;
    bool irqen;
    bool dmaen;
};

struct Channel7 {
    DMACNT7 dmacnt;

    u32 dad, sad;
    u32 ctr;
};

struct Channel9 {
    DMACNT9 dmacnt;

    u32 dad[2], sad[2];
    u32 ctr[2];
    u32 fill;
};

Channel7 channels7[4];
Channel9 channels9[4];

int getChnID(u32 addr) {
    if (addr < 0x040000BC) return 0;
    if (addr < 0x040000C8) return 1;
    if (addr < 0x040000D4) return 2;

    return 3;
}

void doDMA9(int chnID) {
    auto &chn = channels9[chnID];
    auto &cnt = chn.dmacnt;

    std::printf("[DMA:ARM9  ] Channel %d DMA - %s\n", chnID, sync9Names[cnt.sync]);

    // Reload internal registers
    chn.dad[0] = chn.dad[1] & ~1;
    chn.sad[0] = chn.sad[1] & ~1;
    chn.ctr[0] = chn.ctr[1];

    if (!chn.ctr[0]) chn.ctr[0] = 0x200000;

    if ((Sync9)cnt.sync == Sync9::Immediately) {
        cnt.repeat = false; // Doesn't work with sync = 0

        u32 dstOffset, srcOffset;

        switch (cnt.dstcnt) {
            case 0: dstOffset =  2; break; // Increment
            case 1: dstOffset = -2; break; // Decrement
            case 2: dstOffset =  0; break; // Fixed
            case 3: dstOffset =  2; break; // Increment
        }

        switch (cnt.srccnt) {
            case 0: srcOffset =  2; break; // Increment
            case 1: srcOffset = -2; break; // Decrement
            case 2: srcOffset =  0; break; // Fixed
            case 3: srcOffset =  0; break; // Fixed
        }

        if (cnt.isWord) {
            dstOffset *= 2;
            srcOffset *= 2;

            for (auto i = chn.ctr[0]; i > 0; i--) {
                std::printf("[0x%08X] = [0x%08X]\n", chn.dad[0], chn.sad[0]);

                bus::write32ARM9(chn.dad[0], bus::read32ARM9(chn.sad[0]));

                chn.dad[0] += dstOffset;
                chn.sad[0] += srcOffset;
            }
        } else {
            for (auto i = chn.ctr[0]; i > 0; i--) {
                std::printf("[0x%08X] = [0x%08X]\n", chn.dad[0], chn.sad[0]);

                bus::write16ARM9(chn.dad[0], bus::read16ARM9(chn.sad[0]));

                chn.dad[0] += dstOffset;
                chn.sad[0] += srcOffset;
            }
        }

        if (cnt.irqen) {
            std::printf("[DMA:ARM9  ] Unhandled IRQ\n");

            exit(0);
        }

        cnt.dmaen = false;
    }
}

u16 read16ARM7(u32 addr) {
    u16 data;

    const auto chnID = getChnID(addr);

    const auto &chn = channels7[chnID];

    switch (addr - 12 * chnID) {
        case static_cast<u32>(DMAReg::DMACNT_H):
            {
                std::printf("[DMA:ARM7  ] Read16 @ DMA%dCNT_H\n", chnID);

                const auto &cnt = chn.dmacnt;

                data  = (u16)cnt.dstcnt << 5;
                data |= (u16)cnt.srccnt << 7;
                data |= (u16)cnt.repeat << 9;
                data |= (u16)cnt.isWord << 10;
                data |= (u16)cnt.sync   << 12;
                data |= (u16)cnt.irqen  << 14;
                data |= (u16)cnt.dmaen  << 15;
            }
            break;
        default:
            std::printf("[DMA:ARM7  ] Unhandled read16 @ 0x%08X\n", addr);

            exit(0);
    }

    return data;
}

u32 read32ARM9(u32 addr) {
    u32 data;

    const auto chnID = (addr >= static_cast<u32>(DMAReg::DMAFILL)) ? (addr >> 2) & 3 : getChnID(addr);

    auto &chn = channels9[chnID];

    if (addr >= static_cast<u32>(DMAReg::DMAFILL)) {
        //std::printf("[DMA:ARM9  ] Read32 @ DMA%dFILL\n", chnID);

        return chn.fill;
    } else {
        switch (addr - 12 * chnID) {
            case static_cast<u32>(DMAReg::DMACNT):
                {
                    std::printf("[DMA:ARM9  ] Read32 @ DMA%dCNT\n", chnID);

                    auto &cnt = chn.dmacnt;

                    data = chn.ctr[1];

                    data |= (u32)cnt.dstcnt << 21;
                    data |= (u32)cnt.srccnt << 23;
                    data |= (u32)cnt.repeat << 25;
                    data |= (u32)cnt.isWord << 26;
                    data |= (u32)cnt.sync   << 27;
                    data |= (u32)cnt.irqen  << 30;
                    data |= (u32)cnt.dmaen  << 31;
                }
                break;
            default:
                std::printf("[DMA:ARM9  ] Unhandled read32 @ 0x%08X\n", addr);

                exit(0);
        }
    }

    return data;
}

void write16ARM7(u32 addr, u16 data) {
    const auto chnID = getChnID(addr);

    auto &chn = channels7[chnID];

    switch (addr - 12 * chnID) {
        case static_cast<u32>(DMAReg::DMACNT_H):
            {
                std::printf("[DMA:ARM7  ] Write16 @ DMA%dCNT_H = 0x%08X\n", chnID, data);

                auto &cnt = chn.dmacnt;

                const auto dmaen = cnt.dmaen;

                cnt.dstcnt = (data >> 5) & 3;
                cnt.srccnt = (data >> 7) & 3;
                cnt.repeat = data & (1 << 9);
                cnt.isWord = data & (1 << 10);
                cnt.sync   = (data >> 12) & 3;
                cnt.irqen  = data & (1 << 14);
                cnt.dmaen  = data & (1 << 15);

                if (!dmaen && cnt.dmaen) {
                    std::printf("[DMA:ARM7  ] Unhandled channel %d DMA\n", chnID);

                    exit(0);
                }
            }
            break;
        default:
            std::printf("[DMA:ARM7  ] Unhandled write16 @ 0x%08X = 0x%04X\n", addr, data);

            exit(0);
    }
}

void write32ARM9(u32 addr, u32 data) {
    const auto chnID = (addr >= static_cast<u32>(DMAReg::DMAFILL)) ? (addr >> 2) & 3 : getChnID(addr);

    auto &chn = channels9[chnID];

    if (addr >= static_cast<u32>(DMAReg::DMAFILL)) {
        std::printf("[DMA:ARM9  ] Write32 @ DMA%dFILL = 0x%08X\n", chnID, data);

        chn.fill = data;
    } else {
        switch (addr - 12 * chnID) {
            case static_cast<u32>(DMAReg::DMASAD):
                std::printf("[DMA:ARM9  ] Write32 @ DMA%dSAD = 0x%08X\n", chnID, data);

                chn.sad[1] = data;
                break;
            case static_cast<u32>(DMAReg::DMADAD):
                std::printf("[DMA:ARM9  ] Write32 @ DMA%dDAD = 0x%08X\n", chnID, data);

                chn.dad[1] = data;
                break;
            case static_cast<u32>(DMAReg::DMACNT):
                {
                    std::printf("[DMA:ARM9  ] Write32 @ DMA%dCNT = 0x%08X\n", chnID, data);

                    auto &cnt = chn.dmacnt;

                    const auto dmaen = cnt.dmaen;

                    chn.ctr[1] = data & 0x1FFFFF;

                    cnt.dstcnt = (data >> 21) & 3;
                    cnt.srccnt = (data >> 23) & 3;
                    cnt.repeat = data & (1 << 25);
                    cnt.isWord = data & (1 << 26);
                    cnt.sync   = (data >> 27) & 7;
                    cnt.irqen  = data & (1 << 30);
                    cnt.dmaen  = data & (1 << 31);

                    if (!dmaen && cnt.dmaen) doDMA9(chnID);
                }
                break;
            default:
                std::printf("[DMA:ARM9  ] Unhandled write32 @ 0x%08X = 0x%08X\n", addr, data);

                exit(0);
        }
    }
}

}
