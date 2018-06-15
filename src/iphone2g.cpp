#include "iphone2g.h"

void iphone2g::init()
{
    //TODO
    vics[0].daisy = &vics[1];
    vics[0].cpu = cpu;
    vics[1].cpu = cpu;

    vics[0].init();
    vics[1].init();

    clock0.init();
    clock1.init();
}

u32 iphone2g_rw(void* dev, u32 addr)
{
    iphone2g* device = (iphone2g*) dev;
    if((addr < 0x10000) || (addr >= 0x20000000 && addr < 0x20010000))
    {
        return device->bootrom[(addr+0) & 0xffff] | (device->bootrom[(addr+1) & 0xffff] << 8)
        | (device->bootrom[(addr+2) & 0xffff] << 16) | (device->bootrom[(addr+3) & 0xffff] << 24);
    }
    /*else if(addr >= 0x20000000 && addr < 0x20010000)
    {
        return device->bootram[(addr+0) & 0xffff] | (device->bootram[(addr+1) & 0xffff] << 8)
        | (device->bootram[(addr+2) & 0xffff] << 16) | (device->bootram[(addr+3) & 0xffff] << 24);
    }*/
    else if(addr >= 0x38100000 && addr < 0x38101000) return device->clock0.rw(addr & 0xfff);
    else if(addr >= 0x38e00000 && addr < 0x38e01000) return device->vics[0].rw(addr & 0xfff);
    else if(addr >= 0x38e01000 && addr < 0x38e02000) return device->vics[1].rw(addr & 0xfff);
    else if(addr >= 0x3c500000 && addr < 0x3c501000) return device->clock1.rw(addr & 0xfff);
    else printf("Unknown address %08x!\n", addr);
    return 0;
}

void iphone2g_ww(void* dev, u32 addr, u32 data)
{
    iphone2g* device = (iphone2g*) dev;
    /*if(addr >= 0x20000000 && addr < 0x20010000)
    {
        device->bootram[(addr+0) & 0xffff] = (data >> 0) & 0xff;
        device->bootram[(addr+1) & 0xffff] = (data >> 8) & 0xff;
        device->bootram[(addr+2) & 0xffff] = (data >> 16) & 0xff;
        device->bootram[(addr+3) & 0xffff] = (data >> 24) & 0xff;
    }
    else*/ if(addr >= 0x38100000 && addr < 0x38101000) device->clock0.ww(addr & 0xfff, data);
    else if(addr >= 0x38e00000 && addr < 0x38e01000) device->vics[0].ww(addr & 0xfff, data);
    else if(addr >= 0x38e01000 && addr < 0x38e02000) device->vics[1].ww(addr & 0xfff, data);
    else if(addr >= 0x3c500000 && addr < 0x3c501000) device->clock1.ww(addr & 0xfff, data);
    else printf("Unknown address %08x data %08x!\n", addr, data);
}