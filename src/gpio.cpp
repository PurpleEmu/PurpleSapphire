#include "gpio.h"
#include "iphone2g.h"

void gpio_t::init()
{
    //TODO
    for(int i = 0; i < 7; i++)
    {
        int_level[i] = 0;
        int_stat[i] = 0;
        int_enable[i] = 0;
    }
    int_type = 0;
    state[0].state = 0;
    fsel = 0;
}

u32 gpio_t::rw(u32 addr)
{
    if((addr & 0xfe0) == 0x080) return int_level[(addr & 0x1c) >> 2];
    if((addr & 0xfe0) == 0x0a0) return int_stat[(addr & 0x1c) >> 2];
    if((addr & 0xfe0) == 0x0c0) return int_enable[(addr & 0x1c) >> 2];
    if((addr & 0xfff) == 0x0e0) return int_type;
    if((addr & 0xfff) == 0x2c4) return state[0].state;
    if((addr & 0xfff) == 0x320) return fsel;
    return 0;
}

void gpio_t::ww(u32 addr, u32 data)
{
    if((addr & 0xfe0) == 0x080) int_level[(addr & 0x1c) >> 2] = data;
    if((addr & 0xfe0) == 0x0a0) int_stat[(addr & 0x1c) >> 2] = data;
    if((addr & 0xfe0) == 0x0c0) int_enable[(addr & 0x1c) >> 2] = data;
    if((addr & 0xfff) == 0x0e0) int_type = data;
    if((addr & 0xfff) == 0x2c4) state[0].state = data;
    if((addr & 0xfff) == 0x320) fsel = data;
}