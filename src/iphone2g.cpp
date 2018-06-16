#include "iphone2g.h"

void iphone2g::init()
{
    //TODO
    vics[0].init();
    vics[1].init();

    vics[0].daisy = &vics[1];
    vics[0].cpu = cpu;
    vics[1].cpu = cpu;

    clock0.init();
    clock1.init();

    wdt.dev = this;
    wdt.init();
}

void iphone2g::tick()
{
    wdt.tick();
}

void iphone2g::interrupt(int num)
{
    num &= 0x3f;
    printf("Interrupt %08x triggered!\n", num);
    if(num < 0x20)
    {
        vics[0].raw_intr |= 1 << num;
        vics[0].update();
    }
    else
    {
        vics[1].raw_intr |= 1 << (num & 0x1f);
        vics[1].update();
    }
}

u32 iphone2g_rw(void* dev, u32 addr)
{
    iphone2g* device = (iphone2g*) dev;
    if((addr < 0x10000) || (addr >= 0x20000000 && addr < 0x20010000))
    {
        return device->bootrom[(addr+0) & 0xffff] | (device->bootrom[(addr+1) & 0xffff] << 8)
        | (device->bootrom[(addr+2) & 0xffff] << 16) | (device->bootrom[(addr+3) & 0xffff] << 24);
    }
    if(addr >= 0x22000000 && addr < 0x22040000)
    {
        return device->amc0[(addr+0) & 0x3ffff] | (device->amc0[(addr+1) & 0x3ffff] << 8)
        | (device->amc0[(addr+2) & 0x3ffff] << 16) | (device->amc0[(addr+3) & 0x3ffff] << 24);
    }
    else if(addr >= 0x38100000 && addr < 0x38101000)
    {
        printf("Clock0 read %08x\n", addr);
        return device->clock0.rw(addr & 0xfff);
    }
    else if(addr >= 0x38e00000 && addr < 0x38e01000)
    {
        printf("Vic0 read %08x\n", addr);
        return device->vics[0].rw(addr & 0xfff);
    }
    else if(addr >= 0x38e01000 && addr < 0x38e02000)
    {
        printf("Vic1 read %08x\n", addr);
        return device->vics[1].rw(addr & 0xfff);
    }
    else if(addr >= 0x3c500000 && addr < 0x3c501000)
    {
        printf("Clock1 read %08x\n", addr);
        return device->clock1.rw(addr & 0xfff);
    }
    else if(addr >= 0x3e300000 && addr < 0x3e301000)
    {
        printf("WDT read %08x\n", addr);
        return device->wdt.rw(addr & 0xfff);
    }
    else printf("Unknown address %08x!\n", addr);
    return 0;
}

void iphone2g_ww(void* dev, u32 addr, u32 data)
{
    iphone2g* device = (iphone2g*) dev;
    if(addr >= 0x22000000 && addr < 0x22040000)
    {
        device->amc0[(addr+0) & 0x3ffff] = (data >> 0) & 0xff;
        device->amc0[(addr+1) & 0x3ffff] = (data >> 8) & 0xff;
        device->amc0[(addr+2) & 0x3ffff] = (data >> 16) & 0xff;
        device->amc0[(addr+3) & 0x3ffff] = (data >> 24) & 0xff;
    }
    else if(addr >= 0x38100000 && addr < 0x38101000)
    {
        printf("Clock0 write %08x data %08x\n", addr, data);
        device->clock0.ww(addr & 0xfff, data);
    }
    else if(addr >= 0x38e00000 && addr < 0x38e01000)
    {
        printf("Vic0 write %08x data %08x\n", addr, data);
        device->vics[0].ww(addr & 0xfff, data);
    }
    else if(addr >= 0x38e01000 && addr < 0x38e02000)
    {
        printf("Vic1 write %08x data %08x\n", addr, data);
        device->vics[1].ww(addr & 0xfff, data);
    }
    else if(addr >= 0x3c500000 && addr < 0x3c501000)
    {
        printf("Clock1 write %08x data %08x\n", addr, data);
        device->clock1.ww(addr & 0xfff, data);
    }
    else if(addr >= 0x3e300000 && addr < 0x3e301000)
    {
        printf("WDT write %08x data %08x\n", addr, data);
        device->clock1.ww(addr & 0xfff, data);
    }
    else printf("Unknown address %08x data %08x!\n", addr, data);
}