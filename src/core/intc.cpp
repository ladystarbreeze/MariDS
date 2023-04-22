/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "intc.hpp"

#include <cassert>
#include <cstdio>

#include "MariDS.hpp"

namespace nds::intc {

constexpr const char *intNames[] = {
    "VBLANK", "HBLANK", "VCOUNT",
    "Timer 0", "Timer 1", "Timer 2", "Timer 3",
    "RTC",
    "DMA 0", "DMA 1", "DMA 2", "DMA 3",
    "Key Pad",
    "GBA Slot",
    "IPCSYNC", "IPCSEND Empty", "IPCRECV Not Empty",
    "NDS Slot Done", "NDS Slot IREQ",
    "GXFIFO",
    "Hinge",
    "SPI",
    "Wi-Fi",
};

enum INTCReg {
    IME = 0x04000208,
    IE  = 0x04000210,
    IF  = 0x04000214,
};

bool ime7, ime9;

u32 ie7, ie9;
u32 if7, if9;

void checkInterrupt7() {
    if (ie7 & if7) {
        unhaltCPU(7);

        setIRQPending(7, ime7);
    }
}

void sendInterrupt7(IntSource intSource) {
    std::printf("[INTC:ARM7 ] %s interrupt request\n", intNames[intSource]);

    if7 |= 1 << intSource;

    checkInterrupt7();
}

u32 read32ARM7(u32 addr) {
    switch (addr) {
        case INTCReg::IME:
            std::printf("[INTC:ARM7 ] Read32 @ IME\n");
            return ime7;
        case INTCReg::IE:
            std::printf("[INTC:ARM7 ] Read32 @ IE\n");
            return ie7;
        case INTCReg::IF:
            std::printf("[INTC:ARM7 ] Read32 @ IF\n");
            return if7;
        default:
            std::printf("[INTC:ARM7 ] Unhandled read32 @ 0x%08X\n", addr);

            exit(0);
    }
}

void write8ARM7(u32 addr, u8 data) {
    switch (addr) {
        case INTCReg::IME:
            std::printf("[INTC:ARM7 ] Write8 @ IME = 0x%02X\n", data);
            
            ime7 = data & 1;

            checkInterrupt7();
            break;
        default:
            std::printf("[INTC:ARM7 ] Unhandled write8 @ 0x%08X = 0x%02X\n", addr, data);

            exit(0);
    }
}

void write32ARM7(u32 addr, u32 data) {
    switch (addr) {
        case INTCReg::IME:
            std::printf("[INTC:ARM7 ] Write32 @ IME = 0x%08X\n", data);
            
            ime7 = data & 1;
            break;
        case INTCReg::IE:
            std::printf("[INTC:ARM7 ] Write32 @ IE = 0x%08X\n", data);
            
            ie7 = data;
            break;
        case INTCReg::IF:
            std::printf("[INTC:ARM7 ] Write32 @ IF = 0x%08X\n", data);
            
            if7 &= ~data;
            break;
        default:
            std::printf("[INTC:ARM7 ] Unhandled write32 @ 0x%08X = 0x%08X\n", addr, data);

            exit(0);
    }

    checkInterrupt7();
}

}