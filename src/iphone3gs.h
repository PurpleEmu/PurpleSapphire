#pragma once

#include "common.h"
#include "arm.h"
#include "vic.h"

struct iphone3gs
{
    u8 bootrom[0x10000];
    u8 nor[0x100000];

    arm_cpu* cpu;

    bool hle;
    bool do_print;

    FILE* reg_access_log;

    vic vics[3];

    void init();
    void exit();

    void tick();

    void interrupt(int num, bool level);
};

u32 iphone3gs_rw(void* dev, u32 addr);
void iphone3gs_ww(void* dev, u32 addr, u32 data);