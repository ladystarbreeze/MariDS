/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "bus.hpp"

#include <cstdio>

#include "../common/file.hpp"

// NDS memory regions

/* ARM9 base addresses */
enum class Memory9Base : i64 {
    BIOS = 0xFFFF0000,
};

// NDS ARM7 memory

std::vector<u8> bios7;

// NDS ARM9 memory

std::vector<u8> bios9;

// NDS shared memory

namespace nds::bus {

void init(const char *bios7Path, const char *bios9Path) {
    bios7 = loadBinary(bios7Path);
    bios9 = loadBinary(bios9Path);

    assert(bios7.size() == 0x4000); // 16KB
    assert(bios9.size() == 0x1000); // 4KB

    std::printf("[Bus       ] OK!\n");
}

}
