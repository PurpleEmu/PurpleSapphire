#pragma once

#include "common.h"
#include "arm.h"

enum class arm_type
{
    arm7tdmi, arm9, arm11, cortex_a8
};

enum cp15_domain_access_permissions
{
    cp15_domain_no_access = 0,
    cp15_domain_client = 1,
    cp15_domain_reserved = 2,
    cp15_domain_manager = 3
};

enum cp15_coprocessor_access_permissions
{
    cp15_coprocessor_no_access = 0,
    cp15_coprocessor_privileged_only = 1,
    cp15_coprocessor_reserved = 2,
    cp15_coprocessor_privileged_and_user = 3,
};

struct cp15_t
{
    bool do_print;

    void* cpu;

    union
    {
        struct
        {
            u32 mmu_enable : 1;
            u32 strict_alignment : 1;
            u32 l1_data_cache_enable : 1;
            u32 reserved1 : 4;
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
            u32 intr_vectored_mode_enable : 1;
            u32 endian_on_exception : 1;
            u32 l2_cache_enable : 1;
            u32 reserved3 : 5;
        };
        u32 whole;
    } control_arm11;

    union
    {
        struct
        {
            u32 mmu_enable : 1;
            u32 strict_alignment : 1;
            u32 l1_data_cache_enable : 1;
            u32 reserved1 : 8;
            u32 branch_prediction_enable : 1;
            u32 l1_insn_cache_enable : 1;
            u32 high_vectors : 1;
            u32 reserved2 : 11;
            u32 endian_on_exception : 1;
            u32 reserved3 : 1;
            u32 fiq_masking_disable : 1;
            u32 tex_remap_enable : 1;
            u32 access_flag_behavior_enable : 1;
            u32 thumb_exception_enable : 1;
            u32 reserved4 : 1;
        };
        u32 whole;
    } control_cortex_a8;

    union
    {
        struct
        {
            u32 return_stack_enable : 1;
            u32 dynamic_branch_prediction_enable : 1;
            u32 static_branch_prediction_enable : 1;
            u32 micro_tlb_random_stategy : 1;
            u32 clean_entire_data_cache_enable : 1;
            u32 block_transfer_cache_enable : 1;
            u32 cache_size_restriction_enable : 1;
            u32 reserved1 : 21;
            u32 prefetch_halting_disable : 1;
            u32 branch_folding_enable : 1;
            u32 force_speculative_ops_disable : 1;
            u32 low_interrupt_latency_enable : 1;
        };
        u32 whole;
    } aux_control_arm11;

    union
    {
        struct
        {
            u32 l1_data_cache_hw_alias_disable : 1;
            u32 l2_cache_enable : 1;
            u32 reserved1 : 1;
            u32 l1_cache_parity_enable : 1;
            u32 speculative_axi_access_enable : 1;
            u32 neon_caching_enable : 1;
            u32 cache_invalidate_enable : 1;
            u32 branch_size_mispredict_disable : 1;
            u32 wfi_as_nop : 1;
            u32 pld_as_nop : 1;
            u32 force_single_issue_all : 1;
            u32 force_single_issue_load_store : 1;
            u32 force_single_issue_asimd : 1;
            u32 force_main_clock_on : 1;
            u32 force_neon_clock_on : 1;
            u32 force_etm_clock_on : 1;
            u32 force_cp14_15_pipeline_flush : 1;
            u32 force_cp14_15_serialization : 1;
            u32 clock_stop_request_disable : 1;
            u32 data_cache_op_pipelining_enable : 1;
            u32 reserved2 : 9;
            u32 l1_cache_hw_reset_disable : 1;
            u32 l2_cache_hw_reset_disable : 1;

        };
        u32 whole;
    } aux_control_cortex_a8;

    union
    {
        struct
        {
            cp15_coprocessor_access_permissions cp0 : 2;
            cp15_coprocessor_access_permissions cp1 : 2;
            cp15_coprocessor_access_permissions cp2 : 2;
            cp15_coprocessor_access_permissions cp3 : 2;
            cp15_coprocessor_access_permissions cp4 : 2;
            cp15_coprocessor_access_permissions cp5 : 2;
            cp15_coprocessor_access_permissions cp6 : 2;
            cp15_coprocessor_access_permissions cp7 : 2;
            cp15_coprocessor_access_permissions cp8 : 2;
            cp15_coprocessor_access_permissions cp9 : 2;
            cp15_coprocessor_access_permissions cp10 : 2;
            cp15_coprocessor_access_permissions cp11 : 2;
            cp15_coprocessor_access_permissions cp12 : 2;
            cp15_coprocessor_access_permissions cp13 : 2;
            u32 reserved1 : 4;
        };
        u32 whole;
    } coprocessor_access_control;

    union
    {
        struct
        {
            u32 data_ram_latency : 4;
            u32 reserved1 : 2;
            u32 tag_ram_latency : 3;
            u32 reserved2 : 7;
            u32 l2_cacheability : 1;
            u32 reserved3 : 4;
            u32 ecc_or_parity_enable : 1;
            u32 write_alloc_disable : 1;
            u32 write_alloc_combine_disable : 1;
            u32 write_alloc_delay_disable : 1;
            u32 write_combine_disable : 1;
            u32 reserved4 : 1;
            u32 load_data_forwarding_disable : 1;
            u32 ecc_or_parity : 1;
            u32 two_cycle_l2_read_select : 1;
            u32 reserved5 : 2;
        };
        u32 whole;
    } l2_cache_aux_control;

    union
    {
        struct
        {
            u32 inner_cacheability : 1;
            u32 page_table_walk_shared_mem : 1;
            u32 ecc_enabled : 1;
            u32 outer_cacheability : 2;
            u32 reserved1 : 2;
            u32 translation_table_base : 25;
        };
        u32 whole;
    } translation_table_base_0, translation_table_base_1;

    union
    {
        struct
        {
            u32 size_base_0 : 3;
            u32 reserved1 : 1;
            u32 page_table_walk_on_tlb_miss_0_disabled : 1;
            u32 page_table_walk_on_tlb_miss_1_disabled : 1;
            u32 reserved2 : 27;
        };
        u32 whole;
    } translation_table_base_control;

    union
    {
        struct
        {
            cp15_domain_access_permissions d0 : 2;
            cp15_domain_access_permissions d1 : 2;
            cp15_domain_access_permissions d2 : 2;
            cp15_domain_access_permissions d3 : 2;
            cp15_domain_access_permissions d4 : 2;
            cp15_domain_access_permissions d5 : 2;
            cp15_domain_access_permissions d6 : 2;
            cp15_domain_access_permissions d7 : 2;
            cp15_domain_access_permissions d8 : 2;
            cp15_domain_access_permissions d9 : 2;
            cp15_domain_access_permissions d10 : 2;
            cp15_domain_access_permissions d11 : 2;
            cp15_domain_access_permissions d12 : 2;
            cp15_domain_access_permissions d13 : 2;
            cp15_domain_access_permissions d14 : 2;
            cp15_domain_access_permissions d15 : 2;
        };
        u32 whole;
    } domain_access_control;

    union
    {
        struct
        {
            u32 size : 5;
            u32 reserved1 : 7;
            u32 base_addr : 20;
        };
        u32 whole;
    } peripheral_port_remap;

    arm_type type;

    void init();

    u32 decode_peripheral_port_size();

    u32 read(int opcode1, int opcode2, int crn, int crm);
    void write(int opcode1, int opcode2, int crn, int crm, u32 data);
};