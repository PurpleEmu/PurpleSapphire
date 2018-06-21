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

    union
    {
        float f32[2];
        u32 w[2];
        double f64;
        u64 d;
    } vfp_r[16];
    
    u32 r8_fiq, r9_fiq, r10_fiq, r11_fiq, r12_fiq, r13_fiq, r14_fiq;
    u32 r13_svc, r14_svc;
    u32 r13_abt, r14_abt;
    u32 r13_irq, r14_irq;
    u32 r13_und, r14_und;

    bool fiq, irq, fiq_enable, irq_enable;
    bool data_abort, abort_enable;
    bool undefined, undefined_enable;
    bool reset;

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

    union
    {
        struct
        {
            u32 invalid_op_exception : 1;
            u32 division_by_zero_exception : 1;
            u32 overflow_exception : 1;
            u32 underflow_exception : 1;
            u32 inexact_exception : 1;
            u32 reserved1 : 2;
            u32 input_denormal_exception : 1;
            u32 invalid_op_trap : 1;
            u32 division_by_zero_trap : 1;
            u32 overflow_trap : 1;
            u32 underflow_trap : 1;
            u32 inexact_trap : 1;
            u32 reserved2 : 2;
            u32 input_denormal_trap : 1;
            u32 vector_length : 7;
            u32 reserved3 : 1;
            u32 vector_stride : 2;
            u32 rounding_mode : 2;
            u32 flush_to_zero_enable : 1;
            u32 default_nan_enable : 1;
            u32 reserved4 : 2;
            u32 unordered_flag : 1;
            u32 equal_greater_than_or_unordered_flag : 1;
            u32 equal_flag : 1;
            u32 less_than_flag : 1;
        };
        u32 whole;
    } fpscr;

    union
    {
        struct
        {
            u32 potential_invalid_op : 1;
            u32 reserved1 : 1;
            u32 potential_overflow : 1;
            u32 potential_underflow : 1;
            u32 reserved2 : 3;
            u32 input_exception : 1;
            u32 vector_iterations_remaining : 3;
            u32 reserved3 : 17;
            u32 fpinst2_instruction_valid : 1;
            u32 reserved4 : 1;
            u32 vfp_enable : 1;
            u32 exception_flag : 1;
        };
        u32 whole;
    } fpexc;

    u32 fpinst, fpinst2;

    union
    {
        struct
        {
            u32 media_register_support : 4;
            u32 single_precision_support : 4;
            u32 double_precision_support : 4;
            u32 user_traps_support : 4;
            u32 hw_divide_support : 4;
            u32 hw_sqrt_support : 4;
            u32 short_vectors_support : 4;
            u32 vfp_support_when_no_user_traps : 4;
        };
        u32 whole;
    } mvfr0;

    union
    {
        struct
        {
            u32 media_load_store_support : 4;
            u32 media_integer_support : 4;
            u32 media_single_precision_support : 4;
            u32 reserved1 : 20;
        };
        u32 whole;
    } mvfr1_arm11;

    union
    {
        struct
        {
            u32 full_denormal_math : 4;
            u32 nan_propagation_support : 4;
            u32 media_load_store_support : 4;
            u32 media_integer_support : 4;
            u32 media_single_precision_support : 4;
            u32 reserved1 : 12;
        };
        u32 whole;
    } mvfr1_cortex_a8;

    arm_type type;

    cp15_t cp15;

    void* device;

    std::function<u32(void*,u32)> rw_real;
    std::function<void(void*,u32,u32)> ww_real;

    void init();
    void init_hle(bool print);

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);

    u32 get_load_store_addr();
    u32 get_load_store_multi_addr();
    u32 get_shift_operand(bool s);

    void tick_media(u32 opcode);
    void tick();
    void run(int insns);
};