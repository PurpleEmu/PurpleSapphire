#include "clock.h"

void clock0_t::init()
{
    //TODO
}

u32 clock0_t::rw(u32 addr)
{
    switch(addr & 0xfff)
    {
        case 0x000: return config;
        case 0x008: return adj1;
        case 0x404: return adj2;
    }
    return 0;
}

void clock0_t::ww(u32 addr, u32 data)
{
    switch(addr & 0xfff)
    {
        case 0x000: config = data; break;
        case 0x008: adj1 = data; break;
        case 0x404: adj2 = data; break;
    }
}

void clock1_t::init()
{
    //TODO
    pll_lock = 1; //HACK: boot rom needs this to not infinite loop
}

u32 clock1_t::rw(u32 addr)
{
    switch(addr & 0xfff)
    {
        case 0x000: return config0;
        case 0x004: return config1;
        case 0x008: return config2;
        case 0x020: return plls[0].con;
        case 0x024: return plls[1].con;
        case 0x028: return plls[2].con;
        case 0x02c: return plls[3].con;
        case 0x030: return plls[0].cnt;
        case 0x034: return plls[1].cnt;
        case 0x038: return plls[2].cnt;
        case 0x03c: return plls[3].cnt;
        case 0x040: return pll_lock;
        case 0x044: return pll_mode;
        case 0x048: return cl2_gates;
        case 0x04c: return cl3_gates;
    }
    return 0;
}

void clock1_t::ww(u32 addr, u32 data)
{
    switch(addr & 0xfff)
    {
        case 0x000: config0 = data; break;
        case 0x004: config1 = data; break;
        case 0x008: config2 = data; break;
        case 0x020: plls[0].con = data; break;
        case 0x024: plls[1].con = data; break;
        case 0x028: plls[2].con = data; break;
        case 0x02c: plls[3].con = data; break;
        case 0x030: plls[0].cnt = data; break;
        case 0x034: plls[1].cnt = data; break;
        case 0x038: plls[2].cnt = data; break;
        case 0x03c: plls[3].cnt = data; break;
        case 0x040: pll_lock = data; break;
        case 0x044: pll_mode = data & 0xff; break;
        case 0x048: cl2_gates = data; break;
        case 0x04c: cl3_gates = data; break;
    }
}