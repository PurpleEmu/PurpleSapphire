#pragma once

#include "common.h"
#include "arm.h"
#include "clock.h"
#include "vic.h"
#include "wdt.h"

struct iphone2g
{
    u8 bootrom[0x10000];
    u8 bootram[0x10000];
    u8 ram[0x40000];

    arm_cpu* cpu;

    vic vics[2];
    clock0_t clock0;
    clock1_t clock1;
    wdt_t wdt;

    void init();

    void tick();

    void interrupt(int num);
};

u32 iphone2g_rw(void* dev, u32 addr);
void iphone2g_ww(void* dev, u32 addr, u32 data);