/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "bus.hpp"

#include <cstdio>

#include "dma.hpp"
#include "intc.hpp"
#include "ipc.hpp"
#include "MariDS.hpp"
#include "spi.hpp"
#include "timer.hpp"
#include "../common/file.hpp"

namespace nds::bus {

// NDS memory regions

/* ARM7 base addresses */
enum class Memory7Base : u32 {
    BIOS  = 0x00000000,
    Main  = 0x02000000,
    DMA   = 0x040000B0,
    Timer = 0x04000100,
    IPC   = 0x04000180,
    INTC  = 0x04000208,
    SWRAM = 0x03000000,
    WRAM  = 0x03800000,
    MMIO  = 0x04000000,
};

/* ARM7 memory limits */
enum class Memory7Limit : u32 {
    BIOS  = 0x00004000,
    Main  = 0x00400000,
    SWRAM = 0x00008000,
    WRAM  = 0x00010000,
};

/* ARM9 base addresses */
enum class Memory9Base : u32 {
    DTCM = 0x00800000,
    Main = 0x02000000,
    MMIO = 0x04000000,
    BIOS = 0xFFFF0000,
};

/* ARM9 memory limits */
enum class Memory9Limit : u32 {
    DTCM = 0x00004000,
    Main = 0x00400000,
};

// NDS ARM7 memory

std::vector<u8> bios7;
std::vector<u8> wram;

// NDS ARM9 memory

std::vector<u8> bios9;

u8 dtcm[static_cast<u32>(Memory9Limit::DTCM)];

// NDS shared memory

std::vector<u8> mainMem;
std::vector<u8> swram;

// Registers

u8 wramcnt;
u8 postflg7, postflg9;

// WRAM control
u8  *swram7, *swram9;
u32 swramLimit7, swramLimit9;

/* Returns true if address is in range [base;limit] */
bool inRange(u64 addr, u64 base, u64 limit) {
    return (addr >= base) && (addr < (base + limit));
}

void init(const char *bios7Path, const char *bios9Path) {
    bios7 = loadBinary(bios7Path);
    bios9 = loadBinary(bios9Path);

    assert(bios7.size() == 0x4000); // 16KB
    assert(bios9.size() == 0x1000); // 4KB

    mainMem.resize(static_cast<u32>(Memory9Limit::Main));
    swram.resize(static_cast<u32>(Memory7Limit::SWRAM));
    wram.resize(static_cast<u32>(Memory7Limit::WRAM));

    wramcnt  = 0;
    postflg7 = postflg9 = 0;

    std::printf("[Bus       ] OK!\n");
}

u8 read8ARM7(u32 addr) {
    if (inRange(addr, static_cast<u32>(Memory7Base::SWRAM), 16 * 16 * static_cast<u32>(Memory7Limit::SWRAM))) {
        return swram7[addr & swramLimit7];
    } else if (inRange(addr, static_cast<u32>(Memory7Base::WRAM), 16 * 8 * static_cast<u32>(Memory7Limit::WRAM))) {
        return wram[addr & (static_cast<u32>(Memory7Limit::WRAM) - 1)];
    } else {
        switch (addr) {
            case static_cast<u32>(Memory9Base::MMIO) + 0x138:
                std::printf("[Bus:ARM7  ] Read8 @ RTC\n");
                return 0;
            case static_cast<u32>(Memory9Base::MMIO) + 0x1C2:
                std::printf("[SPI       ] Read8 @ SPIDATA\n");
                return spi::readSPIDATA();
            case static_cast<u32>(Memory9Base::MMIO) + 0x300:
                std::printf("[Bus:ARM7  ] Read8 @ POSTFLG\n");
                return postflg7;
            default:
                std::printf("[Bus:ARM7  ] Unhandled read8 @ 0x%08X\n", addr);

                exit(0);
        }
    }
}

u16 read16ARM7(u32 addr) {
    assert(!(addr & 1));

    u16 data;

    if (inRange(addr, static_cast<u32>(Memory7Base::BIOS), static_cast<u32>(Memory7Limit::BIOS))) {
        std::memcpy(&data, &bios7[addr & (static_cast<u32>(Memory7Limit::BIOS) - 1)], sizeof(u16));
    } else if (inRange(addr, static_cast<u32>(Memory7Base::Main), 2 * static_cast<u32>(Memory7Limit::Main))) {
        std::memcpy(&data, &mainMem[addr & (static_cast<u32>(Memory7Limit::Main) - 1)], sizeof(u16));
    } else if (inRange(addr, static_cast<u32>(Memory7Base::SWRAM), 16 * 16 * static_cast<u32>(Memory7Limit::SWRAM))) {
        std::memcpy(&data, &swram[addr & swramLimit7], sizeof(u16));
    } else if (inRange(addr, static_cast<u32>(Memory7Base::WRAM), 16 * 8 * static_cast<u32>(Memory7Limit::WRAM))) {
        std::memcpy(&data, &wram[addr & (static_cast<u32>(Memory7Limit::WRAM) - 1)], sizeof(u16));
    } else if (inRange(addr, static_cast<u32>(Memory7Base::DMA), 0x30)) {
        return dma::read16ARM7(addr);
    } else if (inRange(addr, static_cast<u32>(Memory7Base::IPC), 0x10)) {
        return ipc::read16ARM7(addr);
    } else {
        switch (addr) {
            default:
                std::printf("[Bus:ARM7  ] Unhandled read16 @ 0x%08X\n", addr);

                exit(0);
        }
    }

    return data;
}

u32 read32ARM7(u32 addr) {
    assert(!(addr & 3));

    u32 data;

    if (inRange(addr, static_cast<u32>(Memory7Base::BIOS), static_cast<u32>(Memory7Limit::BIOS))) {
        std::memcpy(&data, &bios7[addr & (static_cast<u32>(Memory7Limit::BIOS) - 1)], sizeof(u32));
    } else if (inRange(addr, static_cast<u32>(Memory7Base::Main), 2 * static_cast<u32>(Memory7Limit::Main))) {
        std::memcpy(&data, &mainMem[addr & (static_cast<u32>(Memory7Limit::Main) - 1)], sizeof(u32));
    } else if (inRange(addr, static_cast<u32>(Memory7Base::SWRAM), 16 * 16 * static_cast<u32>(Memory7Limit::SWRAM))) {
        std::memcpy(&data, &swram[addr & swramLimit7], sizeof(u32));
    } else if (inRange(addr, static_cast<u32>(Memory7Base::WRAM), 16 * 8 * static_cast<u32>(Memory7Limit::WRAM))) {
        std::memcpy(&data, &wram[addr & (static_cast<u32>(Memory7Limit::WRAM) - 1)], sizeof(u32));
    } else if (inRange(addr, static_cast<u32>(Memory7Base::INTC), 0x10)) {
        return intc::read32ARM7(addr);
    } else {
        switch (addr) {
            case static_cast<u32>(Memory9Base::MMIO) + 0x1A4:
                std::printf("[Bus:ARM9  ] Read32 @ ROMCTRL\n");
                return 1 << 23;
            case static_cast<u32>(Memory9Base::MMIO) + 0x1C0:
                std::printf("[SPI       ] Read32 @ SPICNT\n"); // And SPIDATA??
                //return (u32)spi::readSPICNT() | ((u32)spi::readSPIDATA() << 16);
                return spi::readSPICNT();
            case 0x04100010:
                std::printf("[Bus:ARM9  ] Read32 @ ROMDATA\n");
                return -1;
            default:
                std::printf("[Bus:ARM7  ] Unhandled read32 @ 0x%08X\n", addr);

                exit(0);
        }
    }

    return data;
}

u8 read8ARM9(u32 addr) {
    switch (addr) {
        case static_cast<u32>(Memory9Base::MMIO) + 0x300:
            std::printf("[Bus:ARM9  ] Read8 @ POSTFLG\n");
            return postflg9;
        default:
            std::printf("[Bus:ARM9  ] Unhandled read8 @ 0x%08X\n", addr);

            exit(0);
    }
}

u16 read16ARM9(u32 addr) {
    assert(!(addr & 1));
    
    u16 data;

    if (inRange(addr, static_cast<u32>(Memory9Base::Main), 2 * static_cast<u32>(Memory9Limit::Main))) {
        std::memcpy(&data, &mainMem[addr & (static_cast<u32>(Memory9Limit::Main) - 1)], sizeof(u16));
    } else if (inRange(addr, static_cast<u32>(Memory7Base::IPC), 0x10)) { // Same memory base as ARM7
        return ipc::read16ARM9(addr);
    } else if (addr >= static_cast<u32>(Memory9Base::BIOS)) {
        std::memcpy(&data, &bios9[addr & 0xFFE], sizeof(u16));
    } else {
        std::printf("[Bus:ARM9  ] Unhandled read16 @ 0x%08X\n", addr);

        exit(0);
    }

    return data;
}

u32 read32ARM9(u32 addr) {
    assert(!(addr & 3));
    
    u32 data;

    if (inRange(addr, static_cast<u32>(Memory9Base::DTCM), static_cast<u32>(Memory9Limit::DTCM))) {
        std::memcpy(&data, &dtcm[addr & (static_cast<u32>(Memory9Limit::DTCM) - 1)], sizeof(u32));
    } else if (addr >= static_cast<u32>(Memory9Base::BIOS)) {
        std::memcpy(&data, &bios9[addr & 0xFFC], sizeof(u32));
    } else {
        std::printf("[Bus:ARM9  ] Unhandled read32 @ 0x%08X\n", addr);

        exit(0);
    }

    return data;
}

void write8ARM7(u32 addr, u8 data) {
    if (inRange(addr, static_cast<u32>(Memory7Base::BIOS), static_cast<u32>(Memory7Limit::BIOS))) {
        std::printf("[Bus:ARM7  ] Bad write8 @ BIOS (0x%08X) = 0x%02X\n", addr, data);
    } else if (inRange(addr, static_cast<u32>(Memory7Base::WRAM), 16 * 8 * static_cast<u32>(Memory7Limit::WRAM))) {
        wram[addr & (static_cast<u32>(Memory7Limit::WRAM) - 1)] = data;
    } else if (inRange(addr, static_cast<u32>(Memory7Base::INTC), 0x10)) {
        return intc::write8ARM7(addr, data);
    } else {
        switch (addr) {
            case static_cast<u32>(Memory9Base::MMIO) + 0x138:
                std::printf("[Bus:ARM7  ] Write8 @ RTC = 0x%02X\n", data);
                break;
            case static_cast<u32>(Memory9Base::MMIO) + 0x1A1:
                std::printf("[Bus:ARM7  ] Write8 @ AUXSPICNT_HI = 0x%02X\n", data);
                break;
            case static_cast<u32>(Memory9Base::MMIO) + 0x1A8:
            case static_cast<u32>(Memory9Base::MMIO) + 0x1A9:
            case static_cast<u32>(Memory9Base::MMIO) + 0x1AA:
            case static_cast<u32>(Memory9Base::MMIO) + 0x1AB:
            case static_cast<u32>(Memory9Base::MMIO) + 0x1AC:
            case static_cast<u32>(Memory9Base::MMIO) + 0x1AD:
            case static_cast<u32>(Memory9Base::MMIO) + 0x1AE:
            case static_cast<u32>(Memory9Base::MMIO) + 0x1AF:
                std::printf("[Bus:ARM7  ] Write8 @ ROMCMD[%u] = 0x%02X\n", addr & 7, data);
                break;
            case static_cast<u32>(Memory9Base::MMIO) + 0x1C2:
                std::printf("[SPI       ] Write8 @ SPIDATA = 0x%02X\n", data);
                return spi::writeSPIDATA(data);
            case static_cast<u32>(Memory7Base::MMIO) + 0x301:
                std::printf("[Bus:ARM7  ] Write8 @ HALTCNT = 0x%02X\n", data);

                if (data & (1 << 7)) haltCPU(7);
                break;
            default:
                std::printf("[Bus:ARM7  ] Unhandled write8 @ 0x%08X = 0x%02X\n", addr, data);

                exit(0);
        }
    }
}

void write16ARM7(u32 addr, u16 data) {
    assert(!(addr & 1));

    if (inRange(addr, static_cast<u32>(Memory7Base::Main), 2 * static_cast<u32>(Memory7Limit::Main))) {
        std::memcpy(&mainMem[addr & (static_cast<u32>(Memory7Limit::Main) - 1)], &data, sizeof(u16));
    } else if (inRange(addr, static_cast<u32>(Memory7Base::SWRAM), 16 * 16 * static_cast<u32>(Memory7Limit::SWRAM))) {
        std::memcpy(&swram7[addr & swramLimit7], &data, sizeof(u32));
    } else if (inRange(addr, static_cast<u32>(Memory7Base::WRAM), 16 * 8 * static_cast<u32>(Memory7Limit::WRAM))) {
        std::memcpy(&wram[addr & (static_cast<u32>(Memory7Limit::WRAM) - 1)], &data, sizeof(u16));
    } else if (inRange(addr, static_cast<u32>(Memory7Base::DMA), 0x30)) {
        return dma::write16ARM7(addr, data);
    } else if (inRange(addr, static_cast<u32>(Memory7Base::Timer), 0x10)) {
        return timer::write16ARM7(addr, data);
    } else if (inRange(addr, static_cast<u32>(Memory7Base::IPC), 0x10)) {
        return ipc::write16ARM7(addr, data);
    } else {
        switch (addr) {
            case static_cast<u32>(Memory9Base::MMIO) + 0x1C0:
                std::printf("[SPI       ] Write16 @ SPICNT = 0x%04X\n", data);
                return spi::writeSPICNT(data);
            case static_cast<u32>(Memory9Base::MMIO) + 0x1C2:
                std::printf("[SPI       ] Write16 @ SPIDATA = 0x%04X\n", data);
                return spi::writeSPIDATA(data);
            default:
                std::printf("[Bus:ARM7  ] Unhandled write16 @ 0x%08X = 0x%04X\n", addr, data);

                exit(0);
        }
    }
}

void write32ARM7(u32 addr, u32 data) {
    assert(!(addr & 3));
    
    if (inRange(addr, static_cast<u32>(Memory7Base::BIOS), static_cast<u32>(Memory7Limit::BIOS))) {
        std::printf("[Bus:ARM7  ] Bad write32 @ BIOS (0x%08X) = 0x%08X\n", addr, data);
    } else if (inRange(addr, static_cast<u32>(Memory7Base::Main), 2 * static_cast<u32>(Memory7Limit::Main))) {
        std::memcpy(&mainMem[addr & (static_cast<u32>(Memory7Limit::Main) - 1)], &data, sizeof(u32));
    } else if (inRange(addr, static_cast<u32>(Memory7Base::SWRAM), 16 * 16 * static_cast<u32>(Memory7Limit::SWRAM))) {
        std::memcpy(&swram7[addr & swramLimit7], &data, sizeof(u32));
    } else if (inRange(addr, static_cast<u32>(Memory7Base::WRAM), 16 * 8 * static_cast<u32>(Memory7Limit::WRAM))) {
        std::memcpy(&wram[addr & (static_cast<u32>(Memory7Limit::WRAM) - 1)], &data, sizeof(u32));
    } else if (inRange(addr, static_cast<u32>(Memory7Base::Timer), 0x10)) {
        return timer::write32ARM7(addr, data);
    } else if (inRange(addr, static_cast<u32>(Memory7Base::INTC), 0x10)) {
        return intc::write32ARM7(addr, data);
    } else {
        switch (addr) {
            case static_cast<u32>(Memory9Base::MMIO) + 0x1A4:
                std::printf("[Bus:ARM7  ] Write32 @ ROMCNT = 0x%08X\n", data);
                break;
            default:
                std::printf("[Bus:ARM7  ] Unhandled write32 @ 0x%08X = 0x%08X\n", addr, data);

                exit(0);
        }
    }
}

void write8ARM9(u32 addr, u8 data) {
    if (inRange(addr, static_cast<u32>(Memory9Base::Main), 2 * static_cast<u32>(Memory9Limit::Main))) {
        mainMem[addr & (static_cast<u32>(Memory9Limit::Main) - 1)] = data;
    } else {
        switch (addr) {
            case static_cast<u32>(Memory9Base::MMIO) + 0x208:
                std::printf("[Bus:ARM9  ] Write8 @ IME = 0x%02X\n", data);
                break;
            case static_cast<u32>(Memory9Base::MMIO) + 0x247:
                std::printf("[Bus:ARM9  ] Write8 @ WRAMCNT = 0x%02X\n", data);

                wramcnt = data & 3;

                switch (wramcnt) {
                    case 0: // Full allocation to ARM9 (ARM7 SWRAM is mapped to ARM7 WRAM)
                        swram7 = wram.data();
                        swram9 = swram.data();

                        swramLimit7 = 0xFFFF;
                        swramLimit9 = 0x7FFF;
                        break;
                    case 1: // Second half to ARM9, first half to ARM7
                        swram7 = swram.data();
                        swram9 = swram.data() + 0x4000;

                        swramLimit7 = swramLimit9 = 0x3FFF;
                        break;
                    case 2: // First half to ARM9, second half to ARM7
                        swram7 = swram.data() + 0x4000;
                        swram9 = swram.data();

                        swramLimit7 = swramLimit9 = 0x3FFF;
                        break;
                    case 3: // Full allocation to ARM7 (ARM9 SWRAM is unmapped)
                        swram7 = swram.data();
                        swram9 = NULL;

                        swramLimit7 = 0x7FFF;
                        swramLimit9 = 0;
                        break;
                }
                break;
            default:
                std::printf("[Bus:ARM9  ] Unhandled write8 @ 0x%08X = 0x%02X\n", addr, data);

                exit(0);
        }
    }
}

void write16ARM9(u32 addr, u16 data) {
    assert(!(addr & 1));

    if (inRange(addr, static_cast<u32>(Memory9Base::Main), 2 * static_cast<u32>(Memory9Limit::Main))) {
        std::memcpy(&mainMem[addr & (static_cast<u32>(Memory9Limit::Main) - 1)], &data, sizeof(u16));
    } else if (inRange(addr, static_cast<u32>(Memory7Base::IPC), 0x10)) { // Same memory base as ARM7
        return ipc::write16ARM9(addr, data);
    } else {
        switch (addr) {
            case static_cast<u32>(Memory9Base::MMIO) + 0x204:
                std::printf("[Bus:ARM9  ] Write16 @ EXMEMCNT = 0x%04X\n", data);
                break;
            case static_cast<u32>(Memory9Base::MMIO) + 0x304:
                std::printf("[Bus:ARM9  ] Write16 @ POWCNT1 = 0x%04X\n", data);
                break;
            default:
                std::printf("[Bus:ARM9  ] Unhandled write16 @ 0x%08X = 0x%04X\n", addr, data);

                exit(0);
        }
    }
}

void write32ARM9(u32 addr, u32 data) {
    assert(!(addr & 3));

    if (inRange(addr, static_cast<u32>(Memory9Base::DTCM), static_cast<u32>(Memory9Limit::DTCM))) {
        std::memcpy(&dtcm[addr & (static_cast<u32>(Memory9Limit::DTCM) - 1)], &data, sizeof(u32));
    } else if (inRange(addr, static_cast<u32>(Memory9Base::Main), 2 * static_cast<u32>(Memory9Limit::Main))) { // Same as ARM9 Main Mem
        std::memcpy(&mainMem[addr & (static_cast<u32>(Memory9Limit::Main) - 1)], &data, sizeof(u32));
    } else {
        switch (addr) {
            case static_cast<u32>(Memory9Base::MMIO) + 0x1A0:
                std::printf("[Bus:ARM9  ] Write32 @ AUXSPICNT/AUXSPIDATA = 0x%08X\n", data);
                break;
            case static_cast<u32>(Memory9Base::MMIO) + 0x1A4:
                std::printf("[Bus:ARM9  ] Write32 @ ROMCNT = 0x%08X\n", data);
                break;
            case static_cast<u32>(Memory9Base::MMIO) + 0x208:
                std::printf("[Bus:ARM9  ] Write32 @ IME = 0x%08X\n", data);
                break;
            case static_cast<u32>(Memory9Base::MMIO) + 0x210:
                std::printf("[Bus:ARM9  ] Write32 @ IE = 0x%08X\n", data);
                break;
            case static_cast<u32>(Memory9Base::MMIO) + 0x214:
                std::printf("[Bus:ARM9  ] Write32 @ IF = 0x%08X\n", data);
                break;
            default:
                std::printf("[Bus:ARM9  ] Unhandled write32 @ 0x%08X = 0x%08X\n", addr, data);

                exit(0);
        }
    }
}

}
