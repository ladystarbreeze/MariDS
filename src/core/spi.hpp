/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

#include "../common/types.hpp"

namespace nds::spi {

u16 readSPICNT();
u8  readSPIDATA();

void writeSPICNT (u16 data);
void writeSPIDATA(u8  data);

}
