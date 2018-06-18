#include "timer.h"
#include "iphone2g.h"

void timer::init()
{
    count = 0;
    state = 0;
}

void timer::tick()
{
    iphone2g* dev = (iphone2g*) device;
    if((config & 0x7000) == 0x7000)
    {
        count++;
        if(count == 0) dev->interrupt(0x07, true);
    }
}

u32 timer::rw(u32 addr)
{
    switch(addr & 0x1f)
    {
        case 0x00: return config;
        case 0x04: return state;
        case 0x08: return count_buffer;
        case 0x0c: return count_buffer2;
        case 0x10: return prescaler;
        case 0x14: return unknown_3;
    }
    return 0;
}

void timer::ww(u32 addr, u32 data)
{
    switch(addr & 0x1f)
    {
        case 0x00: config = data; break;
        case 0x04: state = data; break;
        case 0x08: count_buffer = data; break;
        case 0x0c: count_buffer2 = data; break;
        case 0x10: prescaler = data; break;
        case 0x14: unknown_3 = data; break;
    }
}

void timers_t::init()
{
    for(int i = 0; i < 7; i++)
    {
        timers[i].device = device;
        timers[i].init();
    }
}

void timers_t::tick()
{
    //interrupt 0x07
    for(int i = 0; i < 7; i++)
    {
        if(timers[i].state & 1) timers[i].tick();
    }
}

u32 timers_t::rw(u32 addr)
{
    if((addr & 0x1ffff) <= 0x60) return timers[(addr & 0x60) >> 5].rw(addr & 0x1f);
    else if(((addr & 0x1ffff) >= 0xa0) && ((addr & 0x1ffff) <= 0xf8)) return timers[((addr & 0x60) >> 5) - 1].rw(addr & 0x1f);
    else switch(addr & 0x1ffff)
    {
        case 0x80: return ticks_high;
        case 0x84: return ticks_low;
        case 0x88: return unk_reg_0;
        case 0x8c: return unk_reg_1;
        case 0x90: return unk_reg_2;
        case 0x94: return unk_reg_3;
        case 0x98: return unk_reg_4;
        case 0xf8: return 0xffffffff;
        case 0x10000: return irq_stat;
    }
    return 0;
}

void timers_t::ww(u32 addr, u32 data)
{
    iphone2g* dev = (iphone2g*) device;
    if((addr & 0x1ffff) <= 0x60) timers[(addr & 0x60) >> 5].ww(addr & 0x1f, data);
    else if(((addr & 0x1ffff) >= 0xa0) && ((addr & 0x1ffff) <= 0xf8)) timers[((addr & 0x60) >> 5) - 1].ww(addr & 0x1f, data);
    else switch(addr & 0x1ffff)
    {
        case 0x80: ticks_high = data; break;
        case 0x84: ticks_low = data; break;
        case 0x88: unk_reg_0 = data; break;
        case 0x8c: unk_reg_1 = data; break;
        case 0x90: unk_reg_2 = data; break;
        case 0x94: unk_reg_3 = data; break;
        case 0x98: unk_reg_4 = data; break;
        case 0xf8: dev->interrupt(0x07, false); break;
        case 0x10000: irq_stat = data; break;
    }
}