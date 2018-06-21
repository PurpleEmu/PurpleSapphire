#include "common.h"

typedef u32(*rw_func)(void*, u32);
typedef void(*ww_func)(void*, u32, u32);

struct dmac
{
    u32 int_status;
    u32 int_tc_status;
    u32 int_tc_clear;
    u32 config;
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

        u32 lli_ptr;

        union
        {
            struct
            {
                u32 unknown1 : 12;
                u32 source_buffer_size : 7;
                u32 dest_buffer_size : 7;
                u32 source_width : 7;
                u32 dest_width : 7;
                u32 source_ahb_master_select : 1;
                u32 unknown2 : 1;
                u32 source_increment : 1;
                u32 dest_increment : 1;
                u32 unknown3 : 3;
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
                u32 unknown1 : 1;
                u32 tc_interrupt_mask : 1;
                u32 unknown2 : 16;
            };
            u32 whole;
        } config;
    } channels[8];

    void* device;

    rw_func rw;
    ww_func ww;

    void init();

    u32 dmac_rw(u32 addr);
    void dmac_ww(u32 addr, u32 data);
};