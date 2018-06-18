#pragma once

#include "common.h"

struct gpio_t
{
    u32 int_level[7];
    u32 int_stat[7];
    u32 int_enable[7];
    u32 int_type;
    u32 fsel;
    struct
    {
        u32 state;
    } state[32];

    void* device;

    void init();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);
};