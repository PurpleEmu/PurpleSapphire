#pragma once

#include "common.h"
#include "cp15.h"

enum arm_mode
{
    mode_user = 0x10,
    mode_fiq = 0x11,
    mode_irq = 0x12,
    mode_supervisor = 0x13,
    mode_monitor = 0x16,
    mode_abort = 0x17,
    mode_undefined = 0x1b,
    mode_system = 0x1f
};

struct arm_cpu
{
    union
    {
        struct
        {
            arm_mode mode : 5;
            u32 thumb : 1;
            u32 fiq_disable : 1;
            u32 irq_disable : 1;
            u32 abort_disable : 1;
            u32 endianness : 1;
            u32 reserved0 : 6;
            u32 ge : 4;
            u32 reserved1 : 4;
            u32 jazelle : 1;
            u32 reserved2 : 2;
            u32 saturation : 1;
            u32 overflow : 1;
            u64 carry : 1;
            u32 zero : 1;
            u32 sign : 1;
        };
        u32 whole;
    } cpsr, spsr_fiq, spsr_irq, spsr_svc, spsr_abt, spsr_und;

    u32 r[16];

    union
    {
        float f32[2];
        u32 w[2];
        double f64;
        u64 d;
    } vfp_d[16];
    
    u32 r8_fiq, r9_fiq, r10_fiq, r11_fiq, r12_fiq, r13_fiq, r14_fiq;
    u32 r13_svc, r14_svc;
    u32 r13_abt, r14_abt;
    u32 r13_irq, r14_irq;
    u32 r13_und, r14_und;

    bool fiq, irq, fiq_enable, irq_enable;
    bool data_abort, abort_enable;
    bool undefined, undefined_enable;

    union
    {
        struct
        {
            u32 revision : 4;
            u32 variant : 4;
            u32 part_number : 8;
            u32 architecture : 4;
            u32 single_precision_only : 1;
            u32 reserved1 : 2;
            u32 software_only : 1;
            u32 implementor : 8;
        };
        u32 whole;
    } fpsid;

    arm_type type;

    cp15_t cp15;

    void* device;

    u32 next_opcode;
    u32 opcode;

    std::function<u32(void*,u32)> rw_real;
    std::function<void(void*,u32,u32)> ww_real;

    void init();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);

    u32 get_load_store_addr(u32 opcode);
    u32 get_load_store_multi_addr(u32 opcode);
    u32 get_shift_operand(u32 opcode, bool s);

    void tick();
    void run(int insns);
};