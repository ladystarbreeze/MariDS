/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

#include "../common/types.hpp"

namespace nds {

void init(const char *bios7Path, const char *bios9Path, const char *firmPath, const char *gamePath, bool doFastBoot);
void run();
void update(const u8 *fb);

u32 getKEYINPUT();

void haltCPU(int cpuID);
void unhaltCPU(int cpuID);

void setIRQPending(int cpuID, bool irq);

}
