/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

namespace nds {

void init(const char *bios7Path, const char *bios9Path, const char *firmPath);
void run();

}
