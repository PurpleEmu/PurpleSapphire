#pragma once

#include "common.h"

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
    u32 r[16];

    bool hle;
    bool do_print;

    bool just_branched;

    u32 opcode;
    u32 opcode_2;
    u32 next_opcode;

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
    } cpsr, spsr_fiq, spsr_irq, spsr_svc, spsr_abt, spsr_und, spsr_mon;
    
    u32 r8_fiq, r9_fiq, r10_fiq, r11_fiq, r12_fiq, r13_fiq, r14_fiq;
    u32 r13_svc, r14_svc;
    u32 r13_abt, r14_abt;
    u32 r13_irq, r14_irq;
    u32 r13_und, r14_und;

    bool fiq, irq, fiq_enable, irq_enable;
    bool data_abort, abort_enable;
    bool undefined, undefined_enable;
    bool reset;

    void* device;

    std::function<u32(void*,u32)> rw_real;
    std::function<void(void*,u32,u32)> ww_real;

    void init();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);

    void tick();
    void run(int insns);
};