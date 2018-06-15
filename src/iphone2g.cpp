#include "iphone2g.h"

void iphone2g::init()
{
    //TODO
}

u32 iphone2g_rw(void* dev, u32 addr)
{
    iphone2g* device = (iphone2g*) dev;
    if(addr < 0x10000)
    {
        return device->bootrom[(addr+0) & 0xffff] | (device->bootrom[(addr+1) & 0xffff] << 8)
        | (device->bootrom[(addr+2) & 0xffff] << 16) | (device->bootrom[(addr+3) & 0xffff] << 24);
    }
    else if(addr >= 0x20000000 && addr < 0x20010000)
    {
        return device->bootram[(addr+0) & 0xffff] | (device->bootram[(addr+1) & 0xffff] << 8)
        | (device->bootram[(addr+2) & 0xffff] << 16) | (device->bootram[(addr+3) & 0xffff] << 24);
    }
    else printf("Unknown address %08x!\n", addr);
}

void iphone2g_ww(void* dev, u32 addr, u32 data)
{
    iphone2g* device = (iphone2g*) dev;
    if(addr >= 0x20000000 && addr < 0x20010000)
    {
        device->bootram[(addr+0) & 0xffff] = (data >> 0) & 0xff;
        device->bootram[(addr+1) & 0xffff] = (data >> 8) & 0xff;
        device->bootram[(addr+2) & 0xffff] = (data >> 16) & 0xff;
        device->bootram[(addr+3) & 0xffff] = (data >> 24) & 0xff;
    }
    else printf("Unknown address %08x data %08x!\n", addr, data);
}