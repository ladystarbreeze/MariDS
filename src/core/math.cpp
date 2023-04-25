/*
 * MariDS is a Nintendo DS emulator.
 * Copyright (C) 2023  Lady Starbreeze (Michelle-Marie Schiller)
 */

#include "math.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

namespace nds::math {

// NDS Math registers
enum class MathReg {
    DIVCNT     = 0x04000280,
    DIVNUMER   = 0x04000290,
    DIVDENOM   = 0x04000298,
    DIVRESULT  = 0x040002A0,
    REMRESULT  = 0x040002A8,
    SQRTCNT    = 0x040002B0,
    SQRTRESULT = 0x040002B4,
    SQRTPARAM  = 0x040002B8,
};

struct DIVCNT {
    u8   divmode;
    bool div0, busy;
};

DIVCNT divcnt;

u32 numer[2], denom[2];

u32 div[2], rem[2];

void doDiv() {
    switch (divcnt.divmode) {
        case 0: case 3: // 32/32 = 32,32
            {
                auto a = (i32)numer[0];
                auto b = (i32)denom[0];

                if (!b) { // Div by 0, REM = NUMER, DIV = +1 (when -NUMER)/-1 (when +NUMER)
                    rem[0] = a;

                    if (a & (1 << 31)) {
                        rem[1] = -1;
                        div[0] =  1;
                        div[1] = -1;
                    } else {
                        rem[1] =  0;
                        div[0] = -1;
                        div[1] =  0;
                    }
                } else if ((a == INT32_MIN) && (b == -1)) {
                    div[0] = INT32_MIN;
                    div[1] = 0;
                } else {
                    const auto q = (i64)(a / b);
                    const auto r = (i64)(a % b);

                    std::memcpy(div, &q, 8);
                    std::memcpy(rem, &r, 8);
                }
            }
            break;
        case 1: // 64/32 = 64,32
            {
                i64 a, b;

                std::memcpy(&a, numer, 8);
                
                b = (i64)(i32)denom[0];

                if (!b) { // Div by 0, REM = NUMER, DIV = +1 (when -NUMER)/-1 (when +NUMER)
                    std::memcpy(rem, numer, 8);

                    if (a & (1ull << 63)) {
                        div[0] = 1;
                        div[1] = 0;
                    } else {
                        div[0] = -1;
                        div[1] = -1;
                    }
                } else if ((a == INT64_MIN) && (b == -1)) {
                    std::memset(rem, 0, 8);
                    
                    div[0] = 0;
                    div[1] = INT32_MIN;
                } else {
                    const auto q = a / b;
                    const auto r = a % b;

                    std::memcpy(div, &q, 8);
                    std::memcpy(rem, &r, 8);
                }
            }
            break;
        case 2: // 64/64 = 64,64
            {
                i64 a, b;

                std::memcpy(&a, numer, 8);
                std::memcpy(&b, denom, 8);

                if (!b) { // Div by 0, REM = NUMER, DIV = +1 (when -NUMER)/-1 (when +NUMER)
                    std::memcpy(rem, numer, 8);

                    if (a & (1ull << 63)) {
                        div[0] = 1;
                        div[1] = 0;
                    } else {
                        div[0] = -1;
                        div[1] = -1;
                    }
                } else if ((a == INT64_MIN) && (b == -1)) {
                    std::memset(rem, 0, 8);

                    div[0] = 0;
                    div[1] = INT32_MIN;
                } else {
                    const auto q = a / b;
                    const auto r = a % b;

                    std::memcpy(div, &q, 8);
                    std::memcpy(rem, &r, 8);
                }
            }
            break;
    }

    divcnt.div0 = !denom[0] && !denom[1];

    std::printf("DIV = 0x%08X%08X, REM = 0x%08X%08X\n", div[1], div[0], rem[1], rem[0]);
}

u16 read16(u32 addr) {
    u16 data;

    switch (addr) {
        case static_cast<u32>(MathReg::DIVCNT):
            std::printf("[Math      ] Read16 @ DIVCNT\n");

            data  = (u16)divcnt.divmode;
            data |= (u16)divcnt.div0 << 14;
            data |= (u16)divcnt.busy << 15;
            break;
        default:
            std::printf("[Math      ] Unhandled read32 @ 0x%08X\n", addr);

            exit(0);
    }

    return data;
}

u32 read32(u32 addr) {
    switch (addr) {
        case static_cast<u32>(MathReg::DIVRESULT):
            std::printf("[Math      ] Read32 @ DIV_RESULT_L\n");
            return div[0];
        case static_cast<u32>(MathReg::DIVRESULT) + 4:
            std::printf("[Math      ] Read32 @ DIV_RESULT_H\n");
            return div[1];
        case static_cast<u32>(MathReg::REMRESULT):
            std::printf("[Math      ] Read32 @ REM_RESULT_L\n");
            return rem[0];
        case static_cast<u32>(MathReg::REMRESULT) + 4:
            std::printf("[Math      ] Read32 @ REM_RESULT_H\n");
            return rem[1];
        default:
            std::printf("[Math      ] Unhandled read32 @ 0x%08X\n", addr);

            exit(0);
    }
}

void write16(u32 addr, u16 data) {
    switch (addr) {
        case static_cast<u32>(MathReg::DIVCNT):
            std::printf("[Math      ] Write16 @ DIVCNT = 0x%04X\n", data);

            divcnt.divmode = data & 3;

            doDiv();
            break;
        default:
            std::printf("[Math      ] Unhandled write16 @ 0x%08X = 0x%04X\n", addr, data);
            
            exit(0);
    }
}

void write32(u32 addr, u32 data) {
    switch (addr) {
        case static_cast<u32>(MathReg::DIVNUMER):
            std::printf("[Math      ] Write32 @ DIV_NUMER_L = 0x%08X\n", data);

            numer[0] = data;

            doDiv();
            break;
        case static_cast<u32>(MathReg::DIVNUMER) + 4:
            std::printf("[Math      ] Write32 @ DIV_NUMER_H = 0x%08X\n", data);

            numer[1] = data;

            doDiv();
            break;
        case static_cast<u32>(MathReg::DIVDENOM):
            std::printf("[Math      ] Write32 @ DIV_DENOM_L = 0x%08X\n", data);

            denom[0] = data;

            doDiv();
            break;
        case static_cast<u32>(MathReg::DIVDENOM) + 4:
            std::printf("[Math      ] Write32 @ DIV_DENOM_H = 0x%08X\n", data);

            denom[1] = data;

            doDiv();
            break;
        default:
            std::printf("[Math      ] Unhandled write32 @ 0x%08X = 0x%08X\n", addr, data);
            
            exit(0);
    }
}

}
