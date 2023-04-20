/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

#include "../common/types.hpp"

namespace nds::bus {

void init(const char *bios7Path, const char *bios9Path);

u8  read8ARM7 (u32 addr);
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
