/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

#include <vector>

#include "types.hpp"

/* Reads a binary file into a std::vector */
std::vector<u8> loadBinary(const char *path);

/* Writes binary data into a file */
void saveBinary(const char *path, u8 *data, size_t size);
