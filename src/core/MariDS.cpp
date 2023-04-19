/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "MariDS.hpp"

#include <cstdio>

#include "bus.hpp"

namespace nds {

void init(const char *bios7Path, const char *bios9Path) {
    std::printf("[MariDS    ] BIOS7: \"%s\"\n[MariDS    ] BIOS9: \"%s\"\n", bios7Path, bios9Path);

    bus::init(bios7Path, bios9Path);
}

}
