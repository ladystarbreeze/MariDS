/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "cartridge.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

namespace nds::cartridge {

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
    bool resb;
    bool busy;
};

std::ifstream cart;

u32 key1Table[0x1048 / 4];

KEYMode keyMode;

ROMCTRL romctrl;

u64 romcmd;

int argLen;

void init(const char *gamePath, u8 *const bios7) {
    // Open cartridge file
    cart.open(gamePath, std::ios::in | std::ios::binary);

    cart.unsetf(std::ios::skipws);

    // Get KEY1 key table
    std::memcpy(key1Table, bios7 + 0x30, 0x1048);

    keyMode = KEYMode::None;
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
    romctrl.drq = true;

    switch (keyMode) {
        case KEYMode::None:
            switch (romcmd >> 56) { // Unencrypted commands
                case 0x00:
                    std::printf("[Cartridge ] Get Header (0x%016llX)\n", romcmd);

                    argLen = 0x200;
                    break;
                case 0x3C:
                    std::printf("[Cartridge ] Activate KEY1 (0x%016llX)\n", romcmd);

                    keyMode = KEYMode::KEY1;

                    argLen = 0;

                    romctrl.busy = romctrl.drq = false;
                    break;
                case 0x90:
                    std::printf("[Cartridge ] Get 1st Chip ID (0x%016llX)\n", romcmd);

                    argLen = 4;
                    break;
                case 0x9F:
                    std::printf("[Cartridge ] Dummy (0x%016llX)\n", romcmd);

                    argLen = 0x2000;
                    break;
                default:
                    std::printf("[Cartridge ] Unhandled command 0x%016llX\n", romcmd);
                    
                    exit(0);
            }
            break;
        case KEYMode::KEY1: // KEY1 decrypted commands
            {
                key1Decrypt((u32 *)&romcmd);
                
                switch (romcmd >> 60) {
                    default:
                        std::printf("[Cartridge ] Unhandled KEY1 command 0x%016llX\n", romcmd);

                        exit(0);
                }
            }
            break;
        case KEYMode::KEY2:
            std::printf("[Cartridge ] Unhandled KEY2 command 0x%016llX\n", romcmd);

            exit(0);
    }
}

u32 read32ARM7(u32 addr) {
    u32 data;

    switch (addr) {
        case static_cast<u32>(CartReg::ROMCTRL):
            std::printf("[Cart:ARM7 ] Read32 @ ROMCTRL\n");

            data  = (u32)romctrl.drq  << 23;
            data |= (u32)romctrl.resb << 29;
            data |= (u32)romctrl.busy << 31;
            break;
        default:
            std::printf("[Cart:ARM7 ] Unhandled read32 @ 0x%08X\n", addr);

            exit(0);
    }

    return data;
}

void write8ARM7(u32 addr, u8 data) {
    switch (addr) {
        case static_cast<u32>(CartReg::AUXSPICNT) + 1:
            std::printf("[Cart:ARM7 ] Write8 @ AUXSPICNT_HI = 0x%02X\n", data);
            break;
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

            romctrl.resb = romctrl.resb || (data & (1 << 29));
            romctrl.busy = data & (1 << 31);

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

u32 readROMDATA() {
    std::printf("[Cart:ARM7 ] Read32 @ ROMDATA\n");

    if (argLen > 0) {
        argLen -= 4;

        if (!argLen) {
            romctrl.busy = romctrl.drq = false;
        }
    }

    return -1;
}

}
