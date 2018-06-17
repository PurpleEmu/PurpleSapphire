#include "iphone3gs.h"

void iphone3gs::init()
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

u32 iphone3gs_rw(void* dev, u32 addr)
{
    iphone3gs* device = (iphone3gs*) dev;
    if(addr < 0x10000)
    {
        return device->bootrom[(addr+0) & 0xffff] | (device->bootrom[(addr+1) & 0xffff] << 8)
        | (device->bootrom[(addr+2) & 0xffff] << 16) | (device->bootrom[(addr+3) & 0xffff] << 24);
    }
    else if(addr >= (device->cpu->cp15.peripheral_port_remap.base_addr << 12)
    && addr < ((device->cpu->cp15.peripheral_port_remap.base_addr << 12) + device->cpu->cp15.decode_peripheral_port_size()))
    {
        u32 periph_addr = addr - (device->cpu->cp15.peripheral_port_remap.base_addr << 12);
        printf("Unknown peripheral port address %08x; peripheral port at %08x\n", addr, (device->cpu->cp15.peripheral_port_remap.base_addr << 12));
    }
    else printf("Unknown address %08x!\n", addr);
    return 0;
}

void iphone3gs_ww(void* dev, u32 addr, u32 data)
{
    iphone3gs* device = (iphone3gs*) dev;
    if(addr >= (device->cpu->cp15.peripheral_port_remap.base_addr << 12)
    && addr < ((device->cpu->cp15.peripheral_port_remap.base_addr << 12) + device->cpu->cp15.decode_peripheral_port_size()))
    {
        u32 periph_addr = addr - (device->cpu->cp15.peripheral_port_remap.base_addr << 12);
        printf("Unknown peripheral port address %08x data %08x; peripheral port at %08x\n", addr, data, (device->cpu->cp15.peripheral_port_remap.base_addr << 12));
    }
    else printf("Unknown address %08x data %08x!\n", addr, data);
}