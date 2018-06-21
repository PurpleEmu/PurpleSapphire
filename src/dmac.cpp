#include "dmac.h"

void dmac::init()
{
    config.whole = 0;
    for(int i = 0; i < 8; i++)
    {
        channels[i].lli_ptr.whole = 0;
    }
    id[0] = 0x80;
    id[1] = 0x10;
    id[2] = 0x04;
    id[3] = 0x0a;
    id[4] = 0x0d;
    id[5] = 0xf0;
    id[6] = 0x05;
    id[7] = 0xb1;
}

u32 dmac::dmac_rw(u32 addr)
{
    u32 dmac_addr = addr & 0xfff;

    if(dmac_addr >= 0x100 && dmac_addr <= 0x200)
    {
        u32 channel_addr = dmac_addr & 0x1f;
        u32 channel = (dmac_addr & 0xe0) >> 5;

        switch(channel_addr)
        {
            case 0x00: return channels[channel].input_addr;
            case 0x04: return channels[channel].output_addr;
            case 0x08: return channels[channel].lli_ptr.whole;
            case 0x0c: return channels[channel].control0.whole;
            case 0x10: return channels[channel].config.whole;
        }
    }
    if(dmac_addr >= 0xfe0) return id[(addr >> 2) & 7];

    switch(addr & 0xfff)
    {
        case 0x000: return (int_tc_status & int_tc_mask) | (int_err_status & int_err_mask);
        case 0x004: return int_tc_status & int_tc_mask;
        case 0x00c: return int_err_status & int_err_mask;
        case 0x014: return int_tc_status;
        case 0x018: return int_err_status;
        case 0x01c:
        {
            u8 mask = 0;
            for(int i = 0; i < 8; i++)
            {
                if(channels[i].config.channel_enabled) mask |= 1 << i;
            }
            return mask;
        }
        case 0x030: return config.whole;
        case 0x034: return sync;
        case 0x500: return itcr;
        case 0x504: return itop1;
        case 0x508: return itop2;
        case 0x50c: return itop3;
    }
    return 0;
}

void dmac::dmac_ww(u32 addr, u32 data)
{
    u32 dmac_addr = addr & 0xfff;

    if(dmac_addr >= 0x100 && dmac_addr <= 0x200)
    {
        u32 channel_addr = dmac_addr & 0x1f;
        u32 channel = (dmac_addr & 0xe0) >> 5;

        switch(channel_addr)
        {
            case 0x00: channels[channel].input_addr = data; break;
            case 0x04: channels[channel].output_addr = data; break;
            case 0x08: channels[channel].lli_ptr.whole = data & ~2; break;
            case 0x0c: channels[channel].control0.whole = data; break;
            case 0x10:
            {
                channels[channel].config.whole = data;
                if(config.enabled)
                {
                    if(channels[channel].config.channel_enabled)
                    {
                        printf("dma triggered on channel %u!\n", channel);
                    }
                }
                break;
            }
        }
    }

    switch(addr & 0xfff)
    {
        case 0x008: int_tc_status &= ~data; break;
        case 0x010: int_err_status &= ~data; break;
        case 0x030: config.whole = data; break;
        case 0x034: sync = data; break;
        case 0x500: itcr = data & 1; break;
        case 0x504: itop1 = data; break;
        case 0x508: itop2 = data; break;
        case 0x50c: itop3 = data; break;
    }
}