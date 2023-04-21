/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

#include "../common/types.hpp"

namespace nds::ipc {

u16 read16ARM7(u32 addr);

u16 read16ARM9(u32 addr);

void write16ARM7(u32 addr, u16 data);

void write16ARM9(u32 addr, u16 data);

}
