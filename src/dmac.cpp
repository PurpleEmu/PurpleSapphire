#include "dmac.h"

void dmac::init()
{
    config = 0;
}

u32 dmac::dmac_rw(u32 addr)
{
    u32 dmac_addr = addr & 0xfff;

    if(dmac_addr >= 0x100)
    {
        u32 channel_addr = dmac_addr & 0x1f;
        u32 channel = (dmac_addr & 0xe0) >> 5;

        switch(channel_addr)
        {
            case 0x00: return channels[channel].input_addr;
            case 0x04: return channels[channel].output_addr;
            case 0x08: return channels[channel].lli_ptr;
            case 0x0c: return channels[channel].control0.whole;
            case 0x10: return channels[channel].config.whole;
        }
    }

    switch(addr & 0xfff)
    {
        case 0x000: return int_status;
        case 0x004: return int_tc_status;
        case 0x030: return config;
    }
    return 0;
}

void dmac::dmac_ww(u32 addr, u32 data)
{
    u32 dmac_addr = addr & 0xfff;

    if(dmac_addr >= 0x100)
    {
        u32 channel_addr = dmac_addr & 0x1f;
        u32 channel = (dmac_addr & 0xe0) >> 5;

        switch(channel_addr)
        {
            case 0x00: channels[channel].input_addr = data; break;
            case 0x04: channels[channel].output_addr = data; break;
            case 0x08: channels[channel].lli_ptr = data; break;
            case 0x0c: channels[channel].control0.whole = data; break;
            case 0x10: channels[channel].config.whole = data; break;
        }
    }

    switch(addr & 0xfff)
    {
        case 0x000: int_status = data; break;
        case 0x004: int_tc_status |= data; break;
        case 0x008: int_tc_status &= ~data; break;
        case 0x030: config = data & 1; break;
    }
}