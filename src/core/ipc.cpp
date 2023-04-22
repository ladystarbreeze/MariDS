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
    u8 out; // Data to other CPU

    bool irqen; // Enable IRQ
};

IPCSYNC ipcsync[2];

u16 read16ARM7(u32 addr) {
    u16 data;

    switch (addr) {
        case static_cast<u32>(IPCReg::IPCSYNC):
            {
                std::printf("[IPC:ARM7  ] Read16 @ IPCSYNC\n");

                auto &sync      = ipcsync[0];
                auto &otherSync = ipcsync[1];

                data = otherSync.out;

                data |= (u16)sync.out   << 8;
                data |= (u16)sync.irqen << 14;
            }
            break;
        default:
            std::printf("[IPC:ARM7  ] Unhandled read16 @ 0x%08X\n", addr);

            exit(0);
    }

    return data;
}

u16 read16ARM9(u32 addr) {
    u16 data;

    switch (addr) {
        case static_cast<u32>(IPCReg::IPCSYNC):
            {
                //std::printf("[IPC:ARM9  ] Read16 @ IPCSYNC\n");

                auto &sync      = ipcsync[1];
                auto &otherSync = ipcsync[0];

                data = otherSync.out;

                data |= (u16)sync.out   << 8;
                data |= (u16)sync.irqen << 14;
            }
            break;
        default:
            std::printf("[IPC:ARM9  ] Unhandled read16 @ 0x%08X\n", addr);

            exit(0);
    }

    return data;
}

void write16ARM7(u32 addr, u16 data) {
    switch (addr) {
        case static_cast<u32>(IPCReg::IPCSYNC):
            {
                std::printf("[IPC:ARM7  ] Write16 @ IPCSYNC = 0x%04X\n", data);

                auto &sync      = ipcsync[0];
                auto &otherSync = ipcsync[1];

                sync.out   = (data >> 8) & 0xF;
                sync.irqen = data & (1 << 14);

                assert(!otherSync.irqen);
            }
            break;
        default:
            std::printf("[IPC:ARM7  ] Unhandled write16 @ 0x%08X = 0x%04X\n", addr, data);

            exit(0);
    }
}

void write16ARM9(u32 addr, u16 data) {
    switch (addr) {
        case static_cast<u32>(IPCReg::IPCSYNC):
            {
                std::printf("[IPC:ARM9  ] Write16 @ IPCSYNC = 0x%04X\n", data);

                auto &sync      = ipcsync[1];
                auto &otherSync = ipcsync[0];

                sync.out   = (data >> 8) & 0xF;
                sync.irqen = data & (1 << 14);

                assert(!otherSync.irqen);
            }
            break;
        default:
            std::printf("[IPC:ARM9  ] Unhandled write16 @ 0x%08X = 0x%04X\n", addr, data);

            exit(0);
    }
}

}
