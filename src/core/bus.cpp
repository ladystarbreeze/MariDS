/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "bus.hpp"

#include <cstdio>

#include "../common/file.hpp"

// NDS memory regions

/* ARM9 base addresses */
enum class Memory9Base : u32 {
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

u32 read32ARM9(u32 addr) {
    assert(!(addr & 3));
    
    u32 data;

    if (addr >= static_cast<u32>(Memory9Base::BIOS)) {
        std::memcpy(&data, &bios9[addr & 0xFFC], sizeof(u32));
    } else {
        std::printf("[Bus:ARM9  ] Unhandled read32 @ 0x%08X\n", addr);

        exit(0);
    }

    return data;
}

}
