/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "firmware.hpp"

#include <cassert>
#include <cstdio>

#include "../common/file.hpp"

namespace nds::firmware {

std::vector<u8> firm;

void init(const char *firmPath) {
    firm = loadBinary(firmPath);

    assert(firm.size());

    std::printf("[Firmware  ] OK!\n");
}

void write(u8 data) {
    std::printf("[Firmware  ] Write = 0x%02X\n", data);

    exit(0);
}

}
