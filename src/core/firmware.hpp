/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

#include "../common/types.hpp"

namespace nds::firmware {

void init(const char *firmPath);
void release();

u8 read();

void write(u8 data);

}
