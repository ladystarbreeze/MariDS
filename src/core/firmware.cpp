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
    RDSR = 0x05,
};

enum FirmState {
    Idle,
    GetAddress,
    Read,
    ReadStatus,
};

std::vector<u8> firm;

FirmState firmState;

bool wip; // Write In Progress
bool wel; // Write Enable Latch

u8 firmCmd;

u32 firmAddr;

int argLen;

void init(const char *firmPath) {
    firm = loadBinary(firmPath);

    assert(firm.size());

    wip = wel = false;

    std::printf("[Firmware  ] OK!\n");

    firmState = FirmState::Idle;
}

void release() { // After chip select is cleared
    firmState = FirmState::Idle;
}

u8 read() {
    u8 data;

    switch (firmState) {
        case FirmState::Read:
            return firm[firmAddr++];
        case FirmState::ReadStatus:
            data  = (u8)wip;
            data |= (u8)wel << 1;
            break;
        default:
            data = 0;
            break;
    }
    
    return data;
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
                case FirmCmd::RDSR:
                    std::printf("[Firmware  ] RDSR\n");

                    firmState = FirmState::ReadStatus;
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
        default:
            break;
    }
}

}
