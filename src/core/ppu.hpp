/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

#include "../common/types.hpp"

namespace nds::ppu {

void init();

// VRAM stuff

u8 readVRAMCNT(int bank);
u8 readVRAMSTAT();

void writeVRAMCNT(int bank, u8 data);

u8  readVRAM8 (u32 addr);
u16 readVRAM16(u32 addr);
u32 readVRAM32(u32 addr);

u32 readWRAM32(u32 addr);

u8  readLCDC8 (u32 addr);
u16 readLCDC16(u32 addr);
u32 readLCDC32(u32 addr);

void writeVRAM16(u32 addr, u16 data);
void writeVRAM32(u32 addr, u32 data);

void writeWRAM32(u32 addr, u32 data);

void writeLCDC8 (u32 addr, u8  data);
void writeLCDC16(u32 addr, u16 data);
void writeLCDC32(u32 addr, u32 data);

void writePal16(u32 addr, u16 data);
void writePal32(u32 addr, u32 data);

// Display Engine registers

u16 read16(int idx, u32 addr);
u32 read32(int idx, u32 addr);

void write8 (int idx, u32 addr, u8  data);
void write16(int idx, u32 addr, u16 data);
void write32(int idx, u32 addr, u32 data);

u16 readDISPSTAT7();
u16 readVCOUNT();

void writeDISPSTAT7(u16 data);

}
