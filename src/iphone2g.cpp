#include "iphone2g.h"

void iphone2g::init()
{
    //TODO
}

u32 iphone2g::rw(u32 addr)
{
    if(addr < 0x10000)
    {
        return bootrom[(addr+0) & 0xffff] | (bootrom[(addr+1) & 0xffff] << 8) | (bootrom[(addr+2) & 0xffff] << 16) | (bootrom[(addr+3) & 0xffff] << 24);
    }
    if(addr >= 0x20000000 && addr < 0x20010000)
    {
        return bootram[(addr+0) & 0xffff] | (bootram[(addr+1) & 0xffff] << 8) | (bootram[(addr+2) & 0xffff] << 16) | (bootram[(addr+3) & 0xffff] << 24);
    }
}

void iphone2g::ww(u32 addr, u32 data)
{
    if(addr >= 0x20000000 && addr < 0x20010000)
    {
        bootram[(addr+0) & 0xffff] = (data >> 0) & 0xff;
        bootram[(addr+1) & 0xffff] = (data >> 8) & 0xff;
        bootram[(addr+2) & 0xffff] = (data >> 16) & 0xff;
        bootram[(addr+3) & 0xffff] = (data >> 24) & 0xff;
    }
}