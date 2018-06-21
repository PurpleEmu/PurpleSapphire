#include "power.h"

void power_t::init()
{
    config0 = 0x01123009;
    config1 = 0x00000020;
    config2 = 0x00000000;
    powered_on_devices = 0x10ec;
}

u32 power_t::rw(u32 addr)
{
    switch(addr & 0x7f)
    {
        case 0x00: return config0;
        case 0x08: return setstate;
        case 0x14: return state;
        case 0x20: return config1;
        case 0x44: return (0x3 << 24); //this is mostly to appease openiboot
        case 0x6c: return config2;
    }
    return 0;
}

void power_t::ww(u32 addr, u32 data)
{
    switch(addr & 0x7f)
    {
        case 0x00: config0 = data; break;
        case 0x0c:
        {
            powered_on_devices |= data;
            setstate |= data;
            state |= data;
            break;
        }
        case 0x10:
        {
            powered_on_devices &= ~data;
            setstate &= ~data;
            state &= ~data;
            break;
        }
        case 0x20: config1 = data; break;
        case 0x6c: config2 = data; break;
    }
}