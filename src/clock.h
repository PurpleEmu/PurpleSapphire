#pragma once

#include "common.h"

struct clock0_t
{
    union
    {
        struct
        {
            u32 base_divisor : 3;
            u32 unknown1 : 29;
        };
        u32 whole;
    } config;
    u32 adj1, adj2;

    void init();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);
};

struct clock1_t
{
    u32 config0;
    u32 config1, config2;

    struct pll
    {
        u32 con, cnt;
    } plls[4];

    u32 pll_lock, pll_mode;

    u32 cl2_gates;
    u32 cl3_gates;

    void init();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);
};