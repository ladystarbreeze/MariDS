/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "dma.hpp"

#include <cassert>
#include <cstdio>

namespace nds::dma {

enum class DMAReg {
    DMASAD   = 0x040000B0,
    DMADAD   = 0x040000B4,
    DMACNT   = 0x040000B8,
    DMACNT_H = 0x040000BA,
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

struct Channel7 {
    DMACNT7 dmacnt;

    u32 dad, sad;
    u32 ctr;
};

Channel7 channels7[4];

int getChnID(u32 addr) {
    if (addr < 0x040000C0) return 0;
    if (addr < 0x040000CC) return 1;
    if (addr < 0x040000D8) return 2;

    return 3;
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

}
