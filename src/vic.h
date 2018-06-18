#pragma once

#include "common.h"
#include "arm.h"

struct vic
{
    arm_cpu* cpu;

    u32 irq_status;
    u32 fiq_status;
    u32 raw_intr;
    u32 int_select;
    u32 int_enable;
    u32 soft_int;
    u32 protection;
    u32 sw_priority_mask;
    u32 vect_addr[32];
    u32 vect_priority[32];
    u32 address;

    u32 current_intr;
    u32 current_highest_intr;

    s32 stack_i;
    u32 priority_stack[17];
    u8 irq_stack[17];
    u32 priority;

    u32 daisy_vect_addr;
    u32 daisy_priority;
    u8 daisy_input;

    vic* daisy;

    u32 id[8];

    void init();

    void raise(bool fiq);
    void lower(bool fiq);

    int priority_sorter();
    void update();

    void mask_priority();
    void unmask_priority();

    u32 irq_ack();
    void irq_fin();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);
};