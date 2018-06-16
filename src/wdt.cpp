#include "wdt.h"
#include "iphone2g.h"

void wdt_t::init()
{
    ctrl = 0x1fca00;

    //Totally inaccurate, but should suffice.
    cnt_period = ((ctrl >> 16) & 0xf) * (2048 / 4);
    cnt = cnt_period;
}

void wdt_t::tick()
{
    iphone2g* device = (iphone2g*) dev;
    /((ctrl & 0x100000) && ((ctrl & 0xff) != 0xa5))
    {
        cnt--;
    }
    if(ctrl & 0x8000)
    {
        if(cnt == 0)
        {
            cnt = cnt_period;
            device->interrupt(0x33);
        }
    }
}

u32 wdt_t::rw(u32 addr)
{
    switch(addr & 0xfff)
    {
        case 0: return ctrl;
        case 4: return cnt;
    }
    return 0;
}

void wdt_t::ww(u32 addr, u32 data)
{
    switch(addr & 0xfff)
    {
        case 0: ctrl = data; cnt_period = ((ctrl >> 16) & 0xf) * (2048 / 4); break;
    }
}