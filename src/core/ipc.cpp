/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "ipc.hpp"

#include <cassert>
#include <cstdio>
#include <queue>

#include "intc.hpp"

namespace nds::ipc {

using IntSource = intc::IntSource;

// IPC constants

constexpr auto FIFO_SIZE = 16;

enum class IPCReg {
    IPCSYNC     = 0x04000180,
    IPCFIFOCNT  = 0x04000184,
    IPCFIFOSEND = 0x04000188,
};

struct IPCSYNC {
    u8 out; // Data to other CPU

    bool irqen; // Enable IRQ
};

struct IPCFIFOCNT {
    bool sempty;
    bool sfull;
    bool sirqen; // SEND IRQ enable
    bool rempty;
    bool rfull;
    bool rirqen; // RECV IRQ enable
    bool error;
    bool fifoen;
};

IPCSYNC ipcsync[2];

IPCFIFOCNT ipcfifocnt[2];

std::queue<u32> send[2];

u32 lastWord[2];

/* Clears SEND, resets SEND/RECV status flags */
void clearSend(int idx) {
    auto &cnt = ipcfifocnt[idx ^ 0];
    auto &otherCnt = ipcfifocnt[idx ^ 1];

    auto &s = send[idx];

    while (!s.empty()) s.pop();

    lastWord[idx] = 0;

    cnt.sempty = true;
    cnt.sfull  = false;

    otherCnt.rempty = true;
    otherCnt.rfull  = false;
}

void init() {
    clearSend(0);
    clearSend(1);
}

u16 read16ARM7(u32 addr) {
    u16 data;

    switch (addr) {
        case static_cast<u32>(IPCReg::IPCSYNC):
            {
                //std::printf("[IPC:ARM7  ] Read16 @ IPCSYNC\n");

                auto &sync      = ipcsync[0];
                auto &otherSync = ipcsync[1];

                data = otherSync.out;

                data |= (u16)sync.out   << 8;
                data |= (u16)sync.irqen << 14;
            }
            break;
        case static_cast<u32>(IPCReg::IPCFIFOCNT):
            {
                std::printf("[IPC:ARM7  ] Read16 @ IPCFIFOCNT\n");

                auto &cnt = ipcfifocnt[0];

                data  = (u16)cnt.sempty <<  0;
                data |= (u16)cnt.sfull  <<  1;
                data |= (u16)cnt.sirqen <<  2;
                data |= (u16)cnt.rempty <<  8;
                data |= (u16)cnt.rfull  <<  9;
                data |= (u16)cnt.rirqen << 10;
                data |= (u16)cnt.error  << 14;
                data |= (u16)cnt.fifoen << 15;
            }
            break;
        default:
            std::printf("[IPC:ARM7  ] Unhandled read16 @ 0x%08X\n", addr);

            exit(0);
    }

    return data;
}

u32 readRECV7() {
    auto &cnt = ipcfifocnt[0];

    auto &r = send[1];

    if (cnt.fifoen) {
        if (!r.empty()) {
            lastWord[0] = r.front(); r.pop();

            cnt.rempty = r.empty();
            cnt.rfull  = false;

            // Check for SEND empty IRQ
            auto &otherCnt = ipcfifocnt[1];

            otherCnt.sempty = r.empty();
            otherCnt.sfull  = false;

            if (r.empty() && otherCnt.sirqen) intc::sendInterrupt9(IntSource::IPCSEND);
        } else {
            cnt.error = true; // RECV empty
        }
    } else {
        if (!r.empty()) {
            lastWord[0] = r.front();
        } else {
            lastWord[0] = 0;
        }
    }

    return lastWord[0];
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
        case static_cast<u32>(IPCReg::IPCFIFOCNT):
            {
                std::printf("[IPC:ARM9  ] Read16 @ IPCFIFOCNT\n");

                auto &cnt = ipcfifocnt[1];

                data  = (u16)cnt.sempty <<  0;
                data |= (u16)cnt.sfull  <<  1;
                data |= (u16)cnt.sirqen <<  2;
                data |= (u16)cnt.rempty <<  8;
                data |= (u16)cnt.rfull  <<  9;
                data |= (u16)cnt.rirqen << 10;
                data |= (u16)cnt.error  << 14;
                data |= (u16)cnt.fifoen << 15;
            }
            break;
        default:
            std::printf("[IPC:ARM9  ] Unhandled read16 @ 0x%08X\n", addr);

            exit(0);
    }

    return data;
}

u32 readRECV9() {
    auto &cnt = ipcfifocnt[1];

    auto &r = send[0];

    if (cnt.fifoen) {
        if (!r.empty()) {
            lastWord[1] = r.front(); r.pop();

            cnt.rempty = r.empty();
            cnt.rfull  = false;

            // Check for SEND empty IRQ
            auto &otherCnt = ipcfifocnt[0];

            otherCnt.sempty = r.empty();
            otherCnt.sfull  = false;

            if (r.empty() && otherCnt.sirqen) intc::sendInterrupt7(IntSource::IPCSEND);
        } else {
            cnt.error = true; // RECV empty
        }
    } else {
        if (!r.empty()) {
            lastWord[1] = r.front();
        } else {
            lastWord[1] = 0;
        }
    }

    return lastWord[1];
}

void write16ARM7(u32 addr, u16 data) {
    switch (addr) {
        case static_cast<u32>(IPCReg::IPCSYNC):
            {
                //std::printf("[IPC:ARM7  ] Write16 @ IPCSYNC = 0x%04X\n", data);

                auto &sync      = ipcsync[0];
                auto &otherSync = ipcsync[1];

                sync.out   = (data >> 8) & 0xF;
                sync.irqen = data & (1 << 14);

                if (otherSync.irqen) intc::sendInterrupt9(IntSource::IPCSYNC);
            }
            break;
        case static_cast<u32>(IPCReg::IPCFIFOCNT):
            {
                std::printf("[IPC:ARM7  ] Write16 @ IPCFIFOCNT = 0x%04X\n", data);

                auto &cnt = ipcfifocnt[0];

                if (data & (1 << 3)) { // Clear SEND
                    clearSend(0);
                }

                if (data & (1 << 14)) { // Clear ERROR
                    cnt.error = false;
                }

                if ((data & (1 <<  2)) && !cnt.sirqen &&  send[0].empty()) intc::sendInterrupt7(IntSource::IPCSEND);
                if ((data & (1 << 10)) && !cnt.rirqen && !send[1].empty()) intc::sendInterrupt7(IntSource::IPCRECV);

                cnt.sirqen = data & (1 <<  2);
                cnt.rirqen = data & (1 << 10);
                cnt.fifoen = data & (1 << 15);
            }
            break;
        default:
            std::printf("[IPC:ARM7  ] Unhandled write16 @ 0x%08X = 0x%04X\n", addr, data);

            exit(0);
    }
}

void write32ARM7(u32 addr, u32 data) {
    switch (addr) {
        case static_cast<u32>(IPCReg::IPCFIFOSEND):
            {
                std::printf("[IPC:ARM7  ] Write32 @ IPCFIFOSEND = 0x%08X\n", data);

                auto &cnt = ipcfifocnt[0];

                auto &s = send[0];

                if (cnt.fifoen) {
                    if (s.size() < FIFO_SIZE) {
                        s.push(data);

                        cnt.sempty = false;
                        cnt.sfull  = s.size() == FIFO_SIZE;

                        // Check for RECV empty IRQ
                        auto &otherCnt = ipcfifocnt[1];

                        const auto oldEmpty = otherCnt.rempty;

                        otherCnt.rempty = false;
                        otherCnt.rfull  = s.size() == FIFO_SIZE;

                        if (otherCnt.rirqen && oldEmpty) intc::sendInterrupt9(IntSource::IPCRECV);
                    } else {
                        cnt.error = true; // SEND full
                    }
                }
            }
            break;
        default:
            std::printf("[IPC:ARM7  ] Unhandled write32 @ 0x%08X = 0x%08X\n", addr, data);

            exit(0);
    }
}

void write16ARM9(u32 addr, u16 data) {
    switch (addr) {
        case static_cast<u32>(IPCReg::IPCSYNC):
            {
                //std::printf("[IPC:ARM9  ] Write16 @ IPCSYNC = 0x%04X\n", data);

                auto &sync      = ipcsync[1];
                auto &otherSync = ipcsync[0];

                sync.out   = (data >> 8) & 0xF;
                sync.irqen = data & (1 << 14);

                if (otherSync.irqen) intc::sendInterrupt7(IntSource::IPCSYNC);
            }
            break;
        case static_cast<u32>(IPCReg::IPCFIFOCNT):
            {
                std::printf("[IPC:ARM9  ] Write16 @ IPCFIFOCNT = 0x%04X\n", data);

                auto &cnt = ipcfifocnt[1];

                if (data & (1 << 3)) { // Clear SEND
                    clearSend(1);
                }

                if (data & (1 << 14)) { // Clear ERROR
                    cnt.error = false;
                }

                if ((data & (1 <<  2)) && !cnt.sirqen &&  send[1].empty()) intc::sendInterrupt9(IntSource::IPCSEND);
                if ((data & (1 << 10)) && !cnt.rirqen && !send[0].empty()) intc::sendInterrupt9(IntSource::IPCRECV);

                cnt.sirqen = data & (1 <<  2);
                cnt.rirqen = data & (1 << 10);
                cnt.fifoen = data & (1 << 15);
            }
            break;
        default:
            std::printf("[IPC:ARM9  ] Unhandled write16 @ 0x%08X = 0x%04X\n", addr, data);

            exit(0);
    }
}

void write32ARM9(u32 addr, u32 data) {
    switch (addr) {
        case static_cast<u32>(IPCReg::IPCFIFOSEND):
            {
                std::printf("[IPC:ARM9  ] Write32 @ IPCFIFOSEND = 0x%08X\n", data);

                auto &cnt = ipcfifocnt[1];

                auto &s = send[1];

                if (cnt.fifoen) {
                    if (s.size() < FIFO_SIZE) {
                        s.push(data);

                        cnt.sempty = false;
                        cnt.sfull  = s.size() == FIFO_SIZE;

                        // Check for RECV empty IRQ
                        auto &otherCnt = ipcfifocnt[0];

                        const auto oldEmpty = otherCnt.rempty;

                        otherCnt.rempty = false;
                        otherCnt.rfull  = s.size() == FIFO_SIZE;

                        if (otherCnt.rirqen && oldEmpty) intc::sendInterrupt7(IntSource::IPCRECV);
                    } else {
                        cnt.error = true; // SEND full
                    }
                }
            }
            break;
        default:
            std::printf("[IPC:ARM9  ] Unhandled write32 @ 0x%08X = 0x%08X\n", addr, data);

            exit(0);
    }
}

}
