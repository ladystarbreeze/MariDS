/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "MariDS.hpp"

#include <cstdio>

#include "bus.hpp"
#include "firmware.hpp"
#include "timer.hpp"
#include "cpu/cpu.hpp"
#include "cpu/cpuint.hpp"

namespace nds {

cpu::CP15 cp15;

cpu::CPU arm7(7, NULL), arm9(9, &cp15);

void init(const char *bios7Path, const char *bios9Path, const char *firmPath) {
    std::printf("[MariDS    ] BIOS7: \"%s\"\n[MariDS    ] BIOS9: \"%s\"\n[MariDS    ] Firmware: \"%s\"\n", bios7Path, bios9Path, firmPath);

    bus::init(bios7Path, bios9Path);
    firmware::init(firmPath);
    timer::init();

    cpu::interpreter::init();
}

void run() {
    while (true) {
        cpu::interpreter::run(&arm9, 16);
        cpu::interpreter::run(&arm7, 8);

        timer::run(8);
    }
}

}
