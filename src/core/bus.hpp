/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

#include "../common/types.hpp"

namespace nds::bus {

void init(const char *bios7Path, const char *bios9Path);

u32 read32(u32 addr);

}
