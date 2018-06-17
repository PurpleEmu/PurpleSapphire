#pragma once

#include "common.h"

struct timer
{
    u32 config;
    u32 state;
    u32 count_buffer;
    u32 count_buffer2;
    u32 prescaler;
    u32 unknown_3;

    u32 count;

    void* device;

    void init();

    void tick();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);
};

struct timers_t
{
    timer timers[7];

    u32 ticks_high;
    u32 ticks_low;
    u32 unk_reg_0;
    u32 unk_reg_1;
    u32 unk_reg_2;
    u32 unk_reg_3;
    u32 unk_reg_4;
    u32 irq_stat;

    void* device;

    void init();

    void tick();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);
};