#include "spi.h"
#include "iphone2g.h"

void spi_t::init()
{
    cmd = ctrl = setup = status = pin
    = tx_data = rx_data = clk_div = cnt = idd = 0;
}

u32 spi_t::rw(u32 addr)
{
    switch(addr & 0xff)
    {
        case 0x00: return ctrl;
        case 0x04: return setup;
        case 0x08: return status;
        case 0x0c: return pin;
        case 0x10: return tx_data;
        case 0x20:
        {
            //HACK
            switch(cmd)
            {
                case 0x95: return 0x01;
                case 0xda: return 0x71;
                case 0xdb: return 0xc2;
                case 0xdc: return 0x00;
            }
            return 0;
        }
        case 0x30: return clk_div;
        case 0x34: return cnt;
        case 0x38: return idd;
    }
    return 0;
}

void spi_t::ww(u32 addr, u32 data)
{
    iphone2g* dev = (iphone2g*)device;
    switch(addr & 0xff)
    {
        case 0x00:
        {
            if(data & 1)
            {
                status |= 0xff2;
                cmd = tx_data;
                dev->interrupt(interrupt, true);
            }
            break;
        }
        case 0x04: setup = data; break;
        case 0x08:
        {
            status = 0;
            dev->interrupt(interrupt, false);
            break;
        }
        case 0x0c: pin = data; break;
        case 0x10: tx_data = data; break;
        case 0x20: rx_data = data; break;
        case 0x30: clk_div = data; break;
        case 0x34: cnt = data; break;
        case 0x38: idd = data; break;
    }
}