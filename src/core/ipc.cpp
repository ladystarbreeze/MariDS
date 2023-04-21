/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "ipc.hpp"

#include <cassert>
#include <cstdio>

namespace nds::ipc {

enum class IPCReg {
    IPCSYNC = 0x04000180,
};

struct IPCSYNC {
    u8 in;  // Data from other CPU
    u8 out; // Data to other CPU

    bool irqen; // Enable IRQ
};

IPCSYNC ipcsync[2];

void write16ARM7(u32 addr, u16 data) {
    switch (addr) {
        case static_cast<u32>(IPCReg::IPCSYNC):
            {
                std::printf("[IPC:ARM7  ] Write16 @ IPCSYNC = 0x%04X\n", data);

                auto &sync = ipcsync[0];
                auto &otherSync = ipcsync[1];

                sync.out   = (data >> 4) & 0xF;
                sync.irqen = data & (1 << 14);

                // Send data to other CPU
                otherSync.in = sync.out;

                assert(!otherSync.irqen);
            }
            break;
        default:
            std::printf("[IPC:ARM7  ] Unhandled write16 @ 0x%08X = 0x%04X\n", addr, data);

            exit(0);
    }
}

}
