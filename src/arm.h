#pragma once

#include "common.h"
#include "cp15.h"

enum class arm_mode : u8
{
    user = 0x10,
    fiq = 0x11,
    irq = 0x12,
    supervisor = 0x13,
    abort = 0x17,
    undefined = 0x1b,
    system = 0x1f
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
            u32 reserved1 : 7;
            u32 saturation : 1;
            u32 overflow : 1;
            u32 carry : 1;
            u32 zero : 1;
            u32 sign : 1;
        };
        u32 whole;
    } cpsr, spsr_fiq, spsr_irq, spsr_svc, spsr_abt, spsr_und;

    u32 r[16];
    
    u32 r8_fiq, r9_fiq, r10_fiq, r11_fiq, r12_fiq, r13_fiq, r14_fiq;
    u32 r13_svc, r14_svc;
    u32 r13_abt, r14_abt;
    u32 r13_irq, r14_irq;
    u32 r13_und, r14_und;

    bool fiq, irq;

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