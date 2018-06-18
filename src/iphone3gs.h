#pragma once

#include "common.h"
#include "arm.h"

struct iphone3gs
{
    u8 bootrom[0x10000];
    u8 amc0[0x800000];
    u8 nor[0x100000];

    arm_cpu* cpu;

    void init();

    void tick();

    void interrupt(int num);
};

u32 iphone3gs_rw(void* dev, u32 addr);
void iphone3gs_ww(void* dev, u32 addr, u32 data);