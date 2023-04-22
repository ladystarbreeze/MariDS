/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "firmware.hpp"

#include <cassert>
#include <cstdio>

#include "../common/file.hpp"

namespace nds::firmware {

enum FirmCmd {
    READ = 0x03,
    BLAH = 0xFF,
};

enum FirmState {
    Idle,
    GetAddress,
    Read,
};

std::vector<u8> firm;

FirmState firmState;

u8 firmCmd;

u32 firmAddr;

int argLen;

void init(const char *firmPath) {
    firm = loadBinary(firmPath);

    assert(firm.size());

    std::printf("[Firmware  ] OK!\n");

    firmState = FirmState::Idle;
}

void release() { // After chip select is cleared
    firmState = FirmState::Idle;
}

u8 read() {
    if (firmState != FirmState::Read) return 0;

    return firm[firmAddr++];
}

void write(u8 data) {
    std::printf("[Firmware  ] Write = 0x%02X\n", data);

    switch (firmState) {
        case FirmState::Idle:
            firmCmd = data;

            switch (firmCmd) {
                case FirmCmd::READ:
                    std::printf("[Firmware  ] READ\n");

                    firmState = FirmState::GetAddress;

                    firmAddr = 0;

                    argLen = 3;
                    break;
                default:
                    std::printf("[Firmware  ] Unhandled command 0x%02X\n", firmCmd);

                    exit(0);
            }
            break;
        case FirmState::GetAddress:
            firmAddr <<= 8;
            firmAddr  |= data;

            if (!--argLen) {
                std::printf("[Firmware  ] Address = 0x%06X\n", firmAddr);

                switch (firmCmd) {
                    case FirmCmd::READ:
                        firmState = FirmState::Read;
                        break;
                    default:
                        exit(0);
                }
            }
            break;
    }
}

}
