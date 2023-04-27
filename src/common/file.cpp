/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "file.hpp"

#include <fstream>
#include <iterator>

std::vector<u8> loadBinary(const char *path) {
    std::ifstream file{path, std::ios::binary};

    file.unsetf(std::ios::skipws);

    return {std::istream_iterator<u8>{file}, {}};
}

void saveBinary(const char *path, u8 *data, size_t size) {
    std::ofstream file;

    file.open(path, std::ios::binary | std::ios::trunc);

    if (!file.is_open()) {
        std::printf("[MariDS    ] Unable to open file \"%s\"\n", path);

        exit(0);
    }

    file.write((char *)data, size);

    file.close();
}
