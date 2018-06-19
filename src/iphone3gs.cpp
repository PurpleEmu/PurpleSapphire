#include "iphone3gs.h"

#define printf(...) do{ if(do_print) printf(__VA_ARGS__); } while(0)

void iphone3gs::init()
{
    //TODO
}

void iphone3gs::exit()
{
    //TODO
}

void iphone3gs::tick()
{
    //TODO
}

void iphone3gs::interrupt(int num)
{
    //TODO
}

#undef printf
#define printf(...) do{ if(device->do_print) printf(__VA_ARGS__); } while(0)

u32 iphone3gs_rw(void* dev, u32 addr)
{
    iphone3gs* device = (iphone3gs*) dev;
    if(addr < 0x10000)
    {
        return device->bootrom[(addr+0) & 0xffff] | (device->bootrom[(addr+1) & 0xffff] << 8)
        | (device->bootrom[(addr+2) & 0xffff] << 16) | (device->bootrom[(addr+3) & 0xffff] << 24);
    }
    else if(addr >= 0x84000000 && addr < 0x84800000)
    {
        printf("AMC0 read %08x\n", addr);
        return device->amc0[(addr+0) & 0x7fffff] | (device->amc0[(addr+1) & 0x7fffff] << 8)
        | (device->amc0[(addr+2) & 0x7fffff] << 16) | (device->amc0[(addr+3) & 0x7fffff] << 24);
    }
    else if(addr == 0xbf500000) return 0xfffffeff; //FIXME: find the real chipid. bit 8 is disabled because i don't want to have to deal with wfi instructions lol
    else printf("Unknown address %08x!\n", addr);
    return 0;
}

void iphone3gs_ww(void* dev, u32 addr, u32 data)
{
    iphone3gs* device = (iphone3gs*) dev;
    if(addr >= 0x84000000 && addr < 0x84800000)
    {
        printf("AMC0 write %08x data %08x\n", addr, data);
        device->amc0[(addr+0) & 0x7fffff] = (data >> 0) & 0xff;
        device->amc0[(addr+1) & 0x7fffff] = (data >> 8) & 0xff;
        device->amc0[(addr+2) & 0x7fffff] = (data >> 16) & 0xff;
        device->amc0[(addr+3) & 0x7fffff] = (data >> 24) & 0xff;
    }
    else printf("Unknown address %08x data %08x!\n", addr, data);
}