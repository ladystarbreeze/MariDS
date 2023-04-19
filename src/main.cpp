/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include <cstdio>

#include "core/MariDS.hpp"

int main(int argc, char **argv) {
    std::printf("[MariDS    ] Nintendo DS emulator\n");

    if (argc < 2) {
        std::printf("Usage: MariDS /path/to/bios7 /path/to/bios9\n");

        return -1;
    }

    nds::init(argv[1], argv[2]);
    //nds::run();

    return 0;
}
