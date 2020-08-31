#pragma once

#include "common.h"

typedef u32(*rw_func)(void*, u32);
typedef void(*ww_func)(void*, u32, u32);

struct dmac
{
    u32 int_tc_status;
    u32 int_tc_clear;
    u32 int_tc_mask;
    u32 int_err_status;
    u32 int_err_clear;
    u32 int_err_mask;
    u16 soft_burst_request;
    u16 soft_single_request;
    u16 soft_last_burst_request;
    u16 soft_last_single_request;
    union
    {
        struct
        {
            u32 enabled : 1;
            u32 ahb1_endianness : 1;
            u32 ahb2_endianness : 1;
            u32 reserved : 29;
        };
        u32 whole;
    } config;

    u16 sync;

    struct
    {
        u32 input_addr;
        u32 output_addr;
        struct
        {
            u32 source;
            u32 destination;
            u32 next;
            u32 control;
        } linked_list_item;

        union
        {
            struct
            {
                u32 ahb_select : 1;
                u32 reserved : 1;
                u32 lli_addr : 30;
            };
            u32 whole;
        } lli_ptr;

        union
        {
            struct
            {
                u32 size : 12;
                u32 source_burst_size : 7;
                u32 dest_burst_size : 7;
                u32 source_width : 7;
                u32 dest_width : 7;
                u32 source_ahb_select : 1;
                u32 dest_ahb_select : 1;
                u32 source_increment : 1;
                u32 dest_increment : 1;
                u32 privileged_enable : 1;
                u32 bufferable : 1;
                u32 cacheable : 1;
                u32 tc_interrupt_enable : 1;
            };
            u32 whole;
        } control0;

        union
        {
            struct 
            {
                u32 channel_enabled : 1;
                u32 source_periph : 5;
                u32 dest_periph : 5;
                u32 flow_control : 3;
                u32 err_interrupt_mask : 1;
                u32 tc_interrupt_mask : 1;
                u32 lock : 1;
                u32 active : 1;
                u32 halt : 1;
                u32 reserved : 14;
            };
            u32 whole;
        } config;
    } channels[8];

    u16 itcr;
    u16 itop1, itop2, itop3;

    u8 id[8];

    void* device;

    rw_func rw;
    ww_func ww;

    void init();

    u32 dmac_rw(u32 addr);
    void dmac_ww(u32 addr, u32 data);
};