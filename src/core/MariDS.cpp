/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "MariDS.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

#include "bus.hpp"
#include "cartridge.hpp"
#include "firmware.hpp"
#include "timer.hpp"
#include "cpu/cpu.hpp"
#include "cpu/cpuint.hpp"

namespace nds {

cpu::CP15 cp15;

cpu::CPU arm7(7, NULL), arm9(9, &cp15);

/* Returns true if address is in range [base;limit] */
bool inRange(u64 addr, u64 base, u64 limit) {
    return (addr >= base) && (addr < (base + limit));
}

void init(const char *bios7Path, const char *bios9Path, const char *firmPath, const char *gamePath, bool doFastBoot) {
    std::printf("[MariDS    ] BIOS7: \"%s\"\n[MariDS    ] BIOS9: \"%s\"\n[MariDS    ] Firmware: \"%s\"[MariDS    ] Game: \"%s\"\n", bios7Path, bios9Path, firmPath, gamePath);

    if (doFastBoot) assert(gamePath); // No fast boot without a game!

    bus::init(bios7Path, bios9Path, gamePath);
    firmware::init(firmPath);
    timer::init();

    cpu::interpreter::init();

    if (doFastBoot) {
        std::printf("[MariDS    ] Fast booting \"%s\"\n", gamePath);

        const auto cart = cartridge::getCart();

        if (!cart->is_open()) assert(false);

        // Allocate SWRAM to ARM7
        bus::setWRAMCNT(3);

        // Copy cartridge header to RAM
        u8 header[0x200];

        cart->seekg(0, std::ios::beg);
        cart->read((char *)header, 0x200);

        for (u32 i = 0; i < 0x170; i++) bus::write8ARM7(0x027FFE00 + i, header[i]);

        assert(!(header[0x12] & 1)); // Make sure this is not a DSi game

        // Set up more values in RAM

        // NDS7 BIOS checksum
        bus::write16ARM7(0x027FF850, 0x5835);
        bus::write16ARM7(0x027FFC10, 0x5835);

        // Cartridge IDs
        bus::write16ARM7(0x027FF800, 0x1FC2);
        bus::write16ARM7(0x027FF804, 0x1FC2);
        bus::write16ARM7(0x027FFC00, 0x1FC2);
        bus::write16ARM7(0x027FFC04, 0x1FC2);

        // ARM9->ARM7 message
        bus::write16ARM7(0x027FF844, 0x0006);

        // ??
        bus::write16ARM7(0x027FFC30, 0xFFFF);

        // Normal boot
        bus::write16ARM7(0x027FFC40, 0x0001);

        // Load ARM7 and ARM9 binaries

        u32 arm7Offset, arm9Offset;
        u32 arm7Entry , arm9Entry;
        u32 arm7Addr  , arm9Addr;
        u32 arm7Size  , arm9Size;

        std::memcpy(&arm9Offset, &header[0x20], 4);
        std::memcpy(&arm9Entry , &header[0x24], 4);
        std::memcpy(&arm9Addr  , &header[0x28], 4);
        std::memcpy(&arm9Size  , &header[0x2C], 4);
        std::memcpy(&arm7Offset, &header[0x30], 4);
        std::memcpy(&arm7Entry , &header[0x34], 4);
        std::memcpy(&arm7Addr  , &header[0x38], 4);
        std::memcpy(&arm7Size  , &header[0x3C], 4);

        std::printf("ARM9 offset = 0x%08X, entry point = 0x%08X, address = 0x%08X, size = 0x%08X\n", arm9Offset, arm9Entry, arm9Addr, arm9Size);

        // Check for secure area
        if (arm9Offset < 0x8000) {
            std::printf("[MariDS    ] Unhandled Secure Area\n");

            exit(0);
        }

        if (!(arm9Offset & 0xFFF)) arm9Offset = (arm9Offset | 0xFFF) + 1; // Round up ARM9 binary offset

        assert(inRange(arm9Entry, 0x02000000, 0x3BFE00));
        assert(inRange(arm9Addr , 0x02000000, 0x3BFE00));

        arm9Size = std::max(arm9Size, (u32)0x3BFE00);

        std::vector<u8> arm9Binary;

        arm9Binary.resize(arm9Size);

        cart->seekg(arm9Offset, std::ios::beg);
        cart->read((char *)arm9Binary.data(), arm9Size);

        // Copy ARM9 binary
        for (u32 i = 0; i < arm9Size; i++) {
            bus::write8ARM9(arm9Addr + i, arm9Binary[i]);
        }

        std::printf("ARM7 offset = 0x%08X, entry point = 0x%08X, address = 0x%08X, size = 0x%08X\n", arm7Offset, arm7Entry, arm7Addr, arm7Size);

        assert(inRange(arm7Entry, 0x02000000, 0x3BFE00) || inRange(arm7Entry, 0x037F8000, 0xFE00));
        assert(inRange(arm7Addr , 0x02000000, 0x3BFE00) || inRange(arm7Addr , 0x037F8000, 0xFE00));

        arm7Size = (arm7Addr >= 0x037F8000) ? std::max(arm7Size, (u32)0xFE00) : std::max(arm7Size, (u32)0x3BFE00);

        std::vector<u8> arm7Binary;

        arm7Binary.resize(arm7Size);

        cart->seekg(arm7Offset, std::ios::beg);
        cart->read((char *)arm7Binary.data(), arm7Size);

        // Copy ARM7 binary
        for (u32 i = 0; i < arm7Size; i++) {
            bus::write8ARM9(arm7Addr + i, arm7Binary[i]);
        }

        // Set CPU entry points
        arm7.setEntry(arm7Entry);
        arm9.setEntry(arm9Entry);

        bus::setPOSTFLG(1);
    }
}

void run() {
    while (true) {
        cpu::interpreter::run(&arm9, 16);
        cpu::interpreter::run(&arm7, 8);

        timer::run(8);
    }
}

void haltCPU(int cpuID) {
    assert((cpuID == 7) || (cpuID == 9));

    (cpuID == 7) ? arm7.halt() : arm9.halt();
}

void unhaltCPU(int cpuID) {
    assert((cpuID == 7) || (cpuID == 9));

    (cpuID == 7) ? arm7.unhalt() : arm9.unhalt();
}

void setIRQPending(int cpuID, bool irq) {
    assert((cpuID == 7) || (cpuID == 9));

    (cpuID == 7) ? arm7.setIRQPending(irq) : arm9.setIRQPending(irq);
}

}
