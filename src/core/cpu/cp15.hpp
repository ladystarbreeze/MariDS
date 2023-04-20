/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#pragma once

#include "../../common/types.hpp"

namespace nds::cpu::cp15 {

struct CP15 {
    u32 get(u32 idx);

    void set(u32 idx, u32 data);

private:
    u32 control;

    u32 dtcmSize, itcmSize;
};

}
