/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

#include "../common/types.hpp"

namespace nds::math {

u16 read16(u32 addr);
u32 read32(u32 addr);

void write16(u32 addr, u16 data);
void write32(u32 addr, u32 data);

}
