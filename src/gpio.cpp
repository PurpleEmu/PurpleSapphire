#include "gpio.h"
#include "iphone2g.h"

void gpioic_t::init()
{
    for(int i = 0; i < 7; i++)
    {
        int_level[i] = 0;
        int_stat[i] = 0xffffffff;
        int_enable[i] = 0;
        int_type[i] = 0xffffffff;
    }
}

u32 gpioic_t::rw(u32 addr)
{
    if((addr & 0xfe0) == 0x080) return int_level[(addr & 0x1c) >> 2];
    if((addr & 0xfe0) == 0x0a0) return int_stat[(addr & 0x1c) >> 2];
    if((addr & 0xfe0) == 0x0c0) return int_enable[(addr & 0x1c) >> 2];
    if((addr & 0xfff) == 0x0e0) return int_type[(addr & 0x1c) >> 2];
    return 0;
}

void gpioic_t::ww(u32 addr, u32 data)
{
    if((addr & 0xfe0) == 0x080) int_level[(addr & 0x1c) >> 2] = data;
    if((addr & 0xfe0) == 0x0a0) int_stat[(addr & 0x1c) >> 2] = data;
    if((addr & 0xfe0) == 0x0c0) int_enable[(addr & 0x1c) >> 2] = data;
    if((addr & 0xffc) == 0x0e0) int_type[(addr & 0x1c) >> 2] = data;
}

void gpio_t::init()
{
    //TODO
    gpioic.init();
    gpioic.device = device;
    fsel.whole = 0;
    regs[0x16].dat = 0x000000a0; //bootrom expects you to be holding the power button... for some reason
}

u32 gpio_t::decode_fsel_port()
{
    u32 port_low = fsel.minor_port;
    u32 port_high = fsel.major_port << 8;
    u32 port = port_high | port_low;
    return port;
}

u32 gpio_t::rw(u32 addr)
{
    if((addr & 0xfff) == 0x320) return fsel.whole;
    if((addr & 0xfff) < 0x300)
    {
        int port = (addr >> 5) & 0x1f;
        switch((addr >> 2) & 7)
        {
            case 0: return regs[port].con;
            case 1: return regs[port].dat;
            case 2: return regs[port].pud1;
            case 3: return regs[port].pud2;
            case 4: return regs[port].conslp1;
            case 5: return regs[port].conslp2;
            case 6: return regs[port].pudslp1;
            case 7: return regs[port].pudslp2;
        }
    }
    return 0;
}

void gpio_t::ww(u32 addr, u32 data)
{
    if((addr & 0xfff) == 0x320)
    {
        fsel.whole = data & 0x001f070f;
        if((fsel.umask & 0xe) == 0xe)
        {
            u32 port = decode_fsel_port();
            regs[fsel.major_port].dat |= (fsel.umask & 1) << fsel.minor_port;
        }
    }
    //a guess based on the fact that if these registers went on, they would overwrite fsel
    if((addr & 0xfff) < 0x300)
    {
        int port = (addr >> 5) & 0x1f;
        switch((addr >> 2) & 7)
        {
            case 0: regs[port].con = data; break;
            case 1: regs[port].dat = data; break;
            case 2: regs[port].pud1 = data; break;
            case 3: regs[port].pud2 = data; break;
            case 4: regs[port].conslp1 = data; break;
            case 5: regs[port].conslp2 = data; break;
            case 6: regs[port].pudslp1 = data; break;
            case 7: regs[port].pudslp2 = data; break;
        }
    }
}