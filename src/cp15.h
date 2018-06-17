#pragma once

#include "common.h"

struct cp15_t
{
    union
    {
        struct
        {
            u32 mmu_enable : 1;
            u32 strict_alignment : 1;
            u32 l1_data_cache_enable : 1;
            u32 write_buffer_enable : 1;
            u32 reserved1 : 3;
            u32 endian : 1;
            u32 system_protection : 1;
            u32 rom_protection : 1;
            u32 impl_defined1 : 1;
            u32 branch_prediction_enable : 1;
            u32 l1_insn_cache_enable : 1;
            u32 high_vectors : 1;
            u32 predictable_cache_enable : 1;
            u32 thumb_disable : 1;
            u32 reserved2 : 5;
            u32 low_intr_latency_enable : 1;
            u32 unaligned_access_enable : 1;
            u32 subpage_ap_enable : 1;
            u32 intr_vector_scramble_enable : 1;
            u32 endian_on_exception : 1;
            u32 l2_cache_enable : 1;
            u32 reserved3 : 5;
        };
        u32 whole;
    } control;

    union
    {
        struct
        {
            u32 size : 5;
            u32 reserved : 7;
            u32 base_addr : 20;
        };
        u32 whole;
    } peripheral_port_remap;

    void init();

    u32 decode_peripheral_port_size();

    u32 read(int opcode1, int opcode2, int crn, int crm);
    void write(int opcode1, int opcode2, int crn, int crm, u32 data);
};