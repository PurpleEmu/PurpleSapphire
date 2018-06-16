#pragma once

#include "common.h"

struct wdt_t
{
    u32 ctrl;
    u32 cnt;

    void* dev;

    u32 cnt_period;

    void init();

    void tick();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);
};