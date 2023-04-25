/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

#include "../common/types.hpp"

namespace nds::ppu {

void init();

u16 readDISPSTAT();

void writeDISPSTAT(u16 data);

}
