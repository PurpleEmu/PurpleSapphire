#include "usb_phy.h"

void usb_phy_t::init()
{
    ophypwr = ophyclk = orstcon = ophytune = 0;
}

u32 usb_phy_t::rw(u32 addr)
{
    switch(addr & 0xfff)
    {
        case 0x000: return ophypwr;
        case 0x004: return ophyclk;
        case 0x008: return orstcon;
        case 0x020: return ophytune;
    }
    return 0;
}

void usb_phy_t::ww(u32 addr, u32 data)
{
    switch(addr & 0xfff)
    {
        case 0x000: ophypwr = data; break;
        case 0x004: ophyclk = data; break;
        case 0x008: orstcon = data; break;
        case 0x020: ophytune = data; break;
    }
}