/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

#include "../common/types.hpp"

namespace nds::intc {

enum IntSource {
    VBLANK, HBLANK, VCOUNT,
    Timer0, Timer1, Timer2, Timer3,
    RTC,
    DMA0, DMA1, DMA2, DMA3,
    Keypad,
    GBASlot,
    IPCSYNC = 16, IPCSEND, IPCRECV,
    NDSSlotDone, NDSSlotIREQ,
    GXFIFO,
    Hinge,
    SPI,
    WiFi,
};

void sendInterrupt7(IntSource intSource);
void sendInterrupt9(IntSource intSource);

u16 read16ARM7(u32 addr);
u32 read32ARM7(u32 addr);

u8  read8ARM9 (u32 addr);
u16 read16ARM9(u32 addr);
u32 read32ARM9(u32 addr);

void write8ARM7 (u32 addr, u8  data);
void write16ARM7(u32 addr, u16 data);
void write32ARM7(u32 addr, u32 data);

void write8ARM9 (u32 addr, u8  data);
void write16ARM9(u32 addr, u16 data);
void write32ARM9(u32 addr, u32 data);

}
