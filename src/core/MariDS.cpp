/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "MariDS.hpp"

#include <cstdio>

#include "bus.hpp"
#include "cpu/cpu.hpp"
#include "cpu/cpuint.hpp"

namespace nds {

cpu::CP15 cp15;

cpu::CPU arm7(7, NULL), arm9(9, &cp15);

void init(const char *bios7Path, const char *bios9Path) {
    std::printf("[MariDS    ] BIOS7: \"%s\"\n[MariDS    ] BIOS9: \"%s\"\n", bios7Path, bios9Path);

    bus::init(bios7Path, bios9Path);
    cpu::interpreter::init();
}

void run() {
    while (true) {
        cpu::interpreter::run(&arm9, 16);
        cpu::interpreter::run(&arm7, 8);
    }
}

}
