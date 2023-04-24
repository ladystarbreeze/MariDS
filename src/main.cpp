/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include <cstdio>
#include <cstring>

#include "core/MariDS.hpp"

int main(int argc, char **argv) {
    std::printf("[MariDS    ] Nintendo DS emulator\n");

    if (argc < 3) {
        std::printf("Usage: MariDS /path/to/bios7 /path/to/bios9 /path/to/firm [/path/to/game] [-FASTBOOT]\n");

        return -1;
    }

    switch (argc) {
        case 3:
            nds::init(argv[1], argv[2], argv[3], NULL, false);
            break;
        case 4: 
            nds::init(argv[1], argv[2], argv[3], argv[4], false);
            break;
        case 5:
        default:
            nds::init(argv[1], argv[2], argv[3], argv[4], std::strncmp(argv[5], "-FASTBOOT", 9) == 0);
            break;
    }

    nds::run();

    return 0;
}
