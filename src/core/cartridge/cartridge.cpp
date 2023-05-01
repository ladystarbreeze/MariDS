/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "cartridge.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

#include "auxspi.hpp"
#include "../dma.hpp"
#include "../intc.hpp"
#include "../scheduler.hpp"

namespace nds::cartridge {

using IntSource = intc::IntSource;

// Cartridge constants

constexpr u32 CHIP_ID = 0x00001FC2;

enum KEYMode {
    None, KEY1, KEY2,
};

enum class CartReg {
    AUXSPICNT  = 0x040001A0,
    AUXSPIDATA = 0x040001A2,
    ROMCTRL    = 0x040001A4,
    ROMCMD     = 0x040001A8,
    ROMSEED0_L = 0x040001B0,
    ROMSEED1_L = 0x040001B4,
    ROMSEED0_H = 0x040001B8,
    ROMSEED1_H = 0x040001BA,
};

struct ROMCTRL {
    bool drq;
    u8   bsize;
    bool clk;
    bool resb;
    bool busy;
};

struct CartStream {
    u8 buf[0x4000];

    int idx;
};

// Cartridge and buffer

std::ifstream cart;

CartStream stream;

int argLen;

// Encryption related stuff

u32 key1Table[0x1048 / 4];

KEYMode keyMode;

// Cartridge IO

ROMCTRL romctrl;

u64 romcmd;

bool isARM9Access;

// Scheduler
u64 idReceive;

u32 bswap32(u32 data)
{
    return (data >> 24) | ((data >> 8) & 0xFF00) | ((data << 8) & 0xFF0000) | (data << 24);
}

void receiveEvent(i64 c) {
    (void)c;

    romctrl.drq = true;

    // Check for cart DMA
    if (isARM9Access) {
        dma::checkCart9();
    } else {
        assert(false);
    }
}

void init(const char *gamePath, u8 *const bios7) {
    // Open cartridge file
    cart.open(gamePath, std::ios::in | std::ios::binary);

    cart.unsetf(std::ios::skipws);

    // Get KEY1 key table
    std::memcpy(key1Table, bios7 + 0x30, 0x1048);

    keyMode = KEYMode::None;

    // Register scheduler event
    idReceive = scheduler::registerEvent([](int, i64 c) { receiveEvent(c); });
}

void setKEY2() {
    keyMode = KEYMode::KEY2;
}

void setARM7Access() {
    isARM9Access = false;
}

void setARM9Access() {
    isARM9Access = true;
}

std::ifstream *getCart() {
    return &cart;
}

// Algorithm taken from GBATEK
void key1Decrypt(u32 *in) {
    u32 x, y, z;

    std::memcpy(&x, &in[1], 4);
    std::memcpy(&y, &in[0], 4);

    for (u32 i = 0x11; i >= 0x2; i--) {
        z = key1Table[i] ^ x;

        x  = key1Table[0x012 + ((z >> 24) & 0xFF)];
        x += key1Table[0x112 + ((z >> 16) & 0xFF)];
        x ^= key1Table[0x212 + ((z >>  8) & 0xFF)];
        x += key1Table[0x312 + ((z >>  0) & 0xFF)];
        x ^= y;

        y = z;
    }

    x ^= key1Table[1];
    y ^= key1Table[0];

    std::memcpy(&in[0], &x, 4);
    std::memcpy(&in[1], &y, 4);
}

void doCmd() {
    stream.idx = 0;

    // Get argument size
    switch (romctrl.bsize) {
        case 0: argLen = 0; break;
        case 7: argLen = 4; break;
        default:
            argLen = 0x100 << romctrl.bsize;
            break;
    }

    switch (keyMode) {
        case KEYMode::None:
            switch (romcmd >> 56) { // Unencrypted commands
                default:
                    std::printf("[Cartridge ] Unhandled command 0x%016llX\n", romcmd);
                    
                    exit(0);
            }
            break;
        case KEYMode::KEY1: // KEY1 decrypted commands
            {
                // key1Decrypt((u32 *)&romcmd);
                
                switch (romcmd >> 60) {
                    default:
                        std::printf("[Cartridge ] Unhandled KEY1 command 0x%016llX\n", romcmd);

                        exit(0);
                }
            }
            break;
        case KEYMode::KEY2:
            switch (romcmd >> 56) {
                case 0xB7:
                    {
                        const u32 addr = romcmd >> 24;

                        std::printf("[Cartridge ] Get Data; Address = 0x%08X, Size = 0x%04X\n", addr, (u32)argLen);

                        assert(!(addr & 0x1FF));

                        // Read cartridge data into buffer
                        cart.seekg(addr, std::ios::beg);
                        cart.read((char *)stream.buf, argLen);
                    }
                    break;
                case 0xB8:
                    std::printf("[Cartridge ] Get Chip ID; Size = 0x%04X\n", (u32)argLen);

                    for (int i = 0; i < argLen; i++) { // Load chip ID into buffer
                        std::memcpy(stream.buf, &CHIP_ID, sizeof(u32));
                    }
                    break;
                default:
                    std::printf("[Cartridge ] Unhandled KEY2 command 0x%016llX\n", romcmd);

                    exit(0);
            }
    }

    if (!argLen) {
        romctrl.busy = false;
    } else {
        scheduler::addEvent(idReceive, 0, (romctrl.clk) ? 32 : 20);
    }
}

u16 read16ARM7(u32 addr) {
    u16 data;

    switch (addr) {
        case static_cast<u32>(CartReg::AUXSPICNT):
            return auxspi::readAUXSPICNT16();
        case static_cast<u32>(CartReg::AUXSPIDATA):
            return auxspi::readAUXSPIDATA16();
        default:
            std::printf("[Cart:ARM7 ] Unhandled read16 @ 0x%08X\n", addr);

            exit(0);
    }

    return data;
}

u32 read32ARM7(u32 addr) {
    u32 data;

    switch (addr) {
        case static_cast<u32>(CartReg::ROMCTRL):
            //std::printf("[Cart:ARM7 ] Read32 @ ROMCTRL\n");

            data  = (u32)romctrl.drq   << 23;
            data |= (u32)romctrl.bsize << 24;
            data |= (u32)romctrl.clk   << 27;
            data |= (u32)romctrl.resb  << 29;
            data |= (u32)romctrl.busy  << 31;
            break;
        default:
            std::printf("[Cart:ARM7 ] Unhandled read32 @ 0x%08X\n", addr);

            exit(0);
    }

    return data;
}

u16 read16ARM9(u32 addr) {
    u16 data;

    switch (addr) {
        case static_cast<u32>(CartReg::AUXSPICNT):
            return auxspi::readAUXSPICNT16();
        case static_cast<u32>(CartReg::AUXSPIDATA):
            return auxspi::readAUXSPIDATA16();
        default:
            std::printf("[Cart:ARM9 ] Unhandled read16 @ 0x%08X\n", addr);

            exit(0);
    }

    return data;
}

u32 read32ARM9(u32 addr) {
    u32 data;

    switch (addr) {
        case static_cast<u32>(CartReg::ROMCTRL):
            //std::printf("[Cart:ARM9 ] Read32 @ ROMCTRL\n");

            data  = (u32)romctrl.drq   << 23;
            data |= (u32)romctrl.bsize << 24;
            data |= (u32)romctrl.clk   << 27;
            data |= (u32)romctrl.resb  << 29;
            data |= (u32)romctrl.busy  << 31;
            break;
        default:
            std::printf("[Cart:ARM9 ] Unhandled read32 @ 0x%08X\n", addr);

            exit(0);
    }

    return data;
}

void write8ARM7(u32 addr, u8 data) {
    switch (addr) {
        case static_cast<u32>(CartReg::AUXSPICNT) + 0:
        case static_cast<u32>(CartReg::AUXSPICNT) + 1:
            return auxspi::writeAUXSPICNT8(addr & 1, data);
        case static_cast<u32>(CartReg::ROMCMD) + 0:
        case static_cast<u32>(CartReg::ROMCMD) + 1:
        case static_cast<u32>(CartReg::ROMCMD) + 2:
        case static_cast<u32>(CartReg::ROMCMD) + 3:
        case static_cast<u32>(CartReg::ROMCMD) + 4:
        case static_cast<u32>(CartReg::ROMCMD) + 5:
        case static_cast<u32>(CartReg::ROMCMD) + 6:
        case static_cast<u32>(CartReg::ROMCMD) + 7:
            {
                const auto i = addr & 7;

                std::printf("[Cart:ARM7 ] Write8 @ ROMCMD[%u] = 0x%02X\n", i, data);

                romcmd &= ~(0xFFull << (56 - (8 * i)));
                romcmd |= (u64)data << (56 - (8 * i));
            }
            break;
        default:
            std::printf("[Cart:ARM7 ] Unhandled write8 @ 0x%08X = 0x%02X\n", addr, data);

            exit(0);
    }
}

void write16ARM7(u32 addr, u16 data) {
    switch (addr) {
        case static_cast<u32>(CartReg::AUXSPICNT):
            return auxspi::writeAUXSPICNT16(data);
        case static_cast<u32>(CartReg::AUXSPIDATA):
            return auxspi::writeAUXSPIDATA16(data);
        case static_cast<u32>(CartReg::ROMSEED0_H):
            std::printf("[Cart:ARM7 ] Write16 @ ROMSEED0_HI = 0x%04X\n", data);
            break;
        case static_cast<u32>(CartReg::ROMSEED1_H):
            std::printf("[Cart:ARM7 ] Write16 @ ROMSEED1_HI = 0x%04X\n", data);
            break;
        default:
            std::printf("[Cart:ARM7 ] Unhandled write16 @ 0x%08X = 0x%04X\n", addr, data);

            exit(0);
    }
}

void write32ARM7(u32 addr, u32 data) {
    switch (addr) {
        case static_cast<u32>(CartReg::ROMCTRL):
            std::printf("[Cart:ARM7 ] Write32 @ ROMCTRL = 0x%08X\n", data);

            romctrl.bsize = (data >> 24) & 7;
            romctrl.clk   = data & (1 << 27);
            romctrl.resb  = romctrl.resb || (data & (1 << 29));
            romctrl.busy  = data & (1 << 31);

            if (romctrl.busy) doCmd();
            break;
        case static_cast<u32>(CartReg::ROMSEED0_L):
            std::printf("[Cart:ARM7 ] Write32 @ ROMSEED0_LO = 0x%08X\n", data);
            break;
        case static_cast<u32>(CartReg::ROMSEED1_L):
            std::printf("[Cart:ARM7 ] Write32 @ ROMSEED1_LO = 0x%08X\n", data);
            break;
        default:
            std::printf("[Cart:ARM7 ] Unhandled write32 @ 0x%08X = 0x%08X\n", addr, data);

            exit(0);
    }
}

void write8ARM9(u32 addr, u8 data) {
    switch (addr) {
        case static_cast<u32>(CartReg::AUXSPICNT) + 0:
        case static_cast<u32>(CartReg::AUXSPICNT) + 1:
            return auxspi::writeAUXSPICNT8(addr & 1, data);
        case static_cast<u32>(CartReg::ROMCMD) + 0:
        case static_cast<u32>(CartReg::ROMCMD) + 1:
        case static_cast<u32>(CartReg::ROMCMD) + 2:
        case static_cast<u32>(CartReg::ROMCMD) + 3:
        case static_cast<u32>(CartReg::ROMCMD) + 4:
        case static_cast<u32>(CartReg::ROMCMD) + 5:
        case static_cast<u32>(CartReg::ROMCMD) + 6:
        case static_cast<u32>(CartReg::ROMCMD) + 7:
            {
                const auto i = addr & 7;

                std::printf("[Cart:ARM9 ] Write8 @ ROMCMD[%u] = 0x%02X\n", i, data);

                romcmd &= ~(0xFFull << (56 - (8 * i)));
                romcmd |= (u64)data << (56 - (8 * i));
            }
            break;
        default:
            std::printf("[Cart:ARM9 ] Unhandled write8 @ 0x%08X = 0x%02X\n", addr, data);

            exit(0);
    }
}

void write16ARM9(u32 addr, u16 data) {
    switch (addr) {
        case static_cast<u32>(CartReg::AUXSPICNT):
            return auxspi::writeAUXSPICNT16(data);
        case static_cast<u32>(CartReg::AUXSPIDATA):
            return auxspi::writeAUXSPIDATA16(data);
        case static_cast<u32>(CartReg::ROMSEED0_H):
            std::printf("[Cart:ARM9 ] Write16 @ ROMSEED0_HI = 0x%04X\n", data);
            break;
        case static_cast<u32>(CartReg::ROMSEED1_H):
            std::printf("[Cart:ARM9 ] Write16 @ ROMSEED1_HI = 0x%04X\n", data);
            break;
        default:
            std::printf("[Cart:ARM9 ] Unhandled write16 @ 0x%08X = 0x%04X\n", addr, data);

            exit(0);
    }
}

void write32ARM9(u32 addr, u32 data) {
    switch (addr) {
        case static_cast<u32>(CartReg::ROMCTRL):
            std::printf("[Cart:ARM9 ] Write32 @ ROMCTRL = 0x%08X\n", data);

            romctrl.bsize = (data >> 24) & 7;
            romctrl.clk   = data & (1 << 27);
            romctrl.resb = romctrl.resb || (data & (1 << 29));
            romctrl.busy = data & (1 << 31);

            if (romctrl.busy) doCmd();
            break;
        case static_cast<u32>(CartReg::ROMCMD) + 0:
        case static_cast<u32>(CartReg::ROMCMD) + 4:
            {
                const auto i = addr & 4;

                std::printf("[Cart:ARM9 ] Write32 @ ROMCMD[%u..%u] = 0x%08X\n", i + 3, i, data);

                if (!i) {
                    romcmd &= 0xFFFFFFFFull;

                    romcmd |= (u64)bswap32(data) << 32;
                } else {
                    romcmd &= ~0xFFFFFFFFull;

                    romcmd |= (u64)bswap32(data);
                }
            }
            break;
        case static_cast<u32>(CartReg::ROMSEED0_L):
            std::printf("[Cart:ARM9 ] Write32 @ ROMSEED0_LO = 0x%08X\n", data);
            break;
        case static_cast<u32>(CartReg::ROMSEED1_L):
            std::printf("[Cart:ARM9 ] Write32 @ ROMSEED1_LO = 0x%08X\n", data);
            break;
        default:
            std::printf("[Cart:ARM9 ] Unhandled write32 @ 0x%08X = 0x%08X\n", addr, data);

            exit(0);
    }
}

u32 readROMDATA() {
    //std::printf("[Cart:ARM7 ] Read32 @ ROMDATA\n");

    assert(argLen);

    u32 data;

    argLen -= 4;

    if (!argLen) {
        romctrl.busy = false;

        if (isARM9Access) {
            intc::sendInterrupt9(IntSource::NDSSlotDone);
        } else {
            intc::sendInterrupt7(IntSource::NDSSlotDone);
        }
    } else {
        scheduler::addEvent(idReceive, 0, (romctrl.clk) ? 32 : 20);
    }

    // Read from cartridge buffer
    std::memcpy(&data, &stream.buf[stream.idx], sizeof(u32));

    stream.idx += 4;

    romctrl.drq = false;

    return data;
}

}
