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
    "N/A", "N/A",
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
    } else {
        setIRQPending(7, false);
    }
}

void checkInterrupt9() {
    if (ie9 & if9) {
        unhaltCPU(9);

        setIRQPending(9, ime9);
    } else {
        setIRQPending(9, false);
    }
}

void sendInterrupt7(IntSource intSource) {
    std::printf("[INTC:ARM7 ] %s interrupt request\n", intNames[intSource]);

    if7 |= 1 << intSource;

    checkInterrupt7();
}

void sendInterrupt9(IntSource intSource) {
    std::printf("[INTC:ARM9 ] %s interrupt request\n", intNames[intSource]);

    if9 |= 1 << intSource;

    checkInterrupt9();
}

u16 read16ARM7(u32 addr) {
    switch (addr) {
        case INTCReg::IME:
            std::printf("[INTC:ARM7 ] Read16 @ IME\n");
            return ime7;
        default:
            std::printf("[INTC:ARM7 ] Unhandled read16 @ 0x%08X\n", addr);

            exit(0);
    }
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

u8 read8ARM9(u32 addr) {
    switch (addr) {
        case INTCReg::IME:
            std::printf("[INTC:ARM9 ] Read8 @ IME\n");
            return ime9;
        default:
            std::printf("[INTC:ARM9 ] Unhandled read8 @ 0x%08X\n", addr);

            exit(0);
    }
}

u16 read16ARM9(u32 addr) {
    switch (addr) {
        case INTCReg::IME:
            std::printf("[INTC:ARM9 ] Read16 @ IME\n");
            return ime9;
        default:
            std::printf("[INTC:ARM9 ] Unhandled read16 @ 0x%08X\n", addr);

            exit(0);
    }
}

u32 read32ARM9(u32 addr) {
    switch (addr) {
        case INTCReg::IME:
            std::printf("[INTC:ARM9 ] Read32 @ IME\n");
            return ime9;
        case INTCReg::IE:
            std::printf("[INTC:ARM9 ] Read32 @ IE\n");
            return ie9;
        case INTCReg::IF:
            std::printf("[INTC:ARM9 ] Read32 @ IF\n");
            return if9;
        default:
            std::printf("[INTC:ARM9 ] Unhandled read32 @ 0x%08X\n", addr);

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

void write16ARM7(u32 addr, u16 data) {
    switch (addr) {
        case INTCReg::IME:
            std::printf("[INTC:ARM7 ] Write16 @ IME = 0x%04X\n", data);
            
            ime7 = data & 1;

            checkInterrupt7();
            break;
        default:
            std::printf("[INTC:ARM7 ] Unhandled write16 @ 0x%08X = 0x%04X\n", addr, data);

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

void write8ARM9(u32 addr, u8 data) {
    switch (addr) {
        case INTCReg::IME:
            std::printf("[INTC:ARM9 ] Write8 @ IME = 0x%02X\n", data);
            
            ime9 = data & 1;

            checkInterrupt9();
            break;
        default:
            std::printf("[INTC:ARM9 ] Unhandled write8 @ 0x%08X = 0x%02X\n", addr, data);

            exit(0);
    }
}

void write16ARM9(u32 addr, u16 data) {
    switch (addr) {
        case INTCReg::IME:
            std::printf("[INTC:ARM9 ] Write16 @ IME = 0x%04X\n", data);
            
            ime9 = data & 1;

            checkInterrupt9();
            break;
        default:
            std::printf("[INTC:ARM9 ] Unhandled write16 @ 0x%08X = 0x%04X\n", addr, data);

            exit(0);
    }
}

void write32ARM9(u32 addr, u32 data) {
    switch (addr) {
        case INTCReg::IME:
            std::printf("[INTC:ARM9 ] Write32 @ IME = 0x%08X\n", data);
            
            ime9 = data & 1;
            break;
        case INTCReg::IE:
            std::printf("[INTC:ARM9 ] Write32 @ IE = 0x%08X\n", data);
            
            ie9 = data;
            break;
        case INTCReg::IF:
            std::printf("[INTC:ARM9 ] Write32 @ IF = 0x%08X\n", data);
            
            if9 &= ~data;
            break;
        default:
            std::printf("[INTC:ARM9 ] Unhandled write32 @ 0x%08X = 0x%08X\n", addr, data);

            exit(0);
    }

    checkInterrupt9();
}

}
