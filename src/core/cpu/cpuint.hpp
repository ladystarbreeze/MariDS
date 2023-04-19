/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

#include <cstdio>

#include "cpu.hpp"

namespace nds::cpu::interpreter {

void init();

void run(CPU *cpu, i64 runCycles);

}