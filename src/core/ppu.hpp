/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

#include "../common/types.hpp"

namespace nds::ppu {

void init();

void writeVRAM16(u32 addr, u16 data);
void writeVRAM32(u32 addr, u32 data);

u16 readDISPSTAT();

void writeDISPSTAT(u16 data);

}
