/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

#include "../../common/types.hpp"

namespace nds::cartridge::auxspi {

u16 readAUXSPICNT16();
u16 readAUXSPIDATA16();

void writeAUXSPICNT8(bool isHi, u8 data);
void writeAUXSPICNT16 (u16 data);
void writeAUXSPIDATA16(u16 data);

}
