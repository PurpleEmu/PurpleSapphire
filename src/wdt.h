#pragma once

#include "common.h"

struct wdt_t
{
    u32 ctrl;
    u32 cnt;

    void* device;

    u32 cnt_period;

    void init();
    void init_hle();

    void tick();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);
};