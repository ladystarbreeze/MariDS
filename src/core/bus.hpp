/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

#include "../common/types.hpp"

namespace nds::bus {

void init(const char *bios7Path, const char *bios9Path);

u8  read8ARM9 (u32 addr);
u32 read32ARM9(u32 addr);

void write32ARM9(u32 addr, u32 data);

}
