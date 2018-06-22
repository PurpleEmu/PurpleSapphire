#include "iphone3gs.h"

#define printf(...) do{ if(do_print) printf(__VA_ARGS__); } while(0)

void iphone3gs::init()
{
    //TODO
    cpu->cp15.peripheral_port_remap.whole = 0x80000014;
    reg_access_log = fopen("reglog.txt","w+");
}

void iphone3gs::exit()
{
    //TODO
    if(reg_access_log) fclose(reg_access_log);
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
    else if(addr >= (device->cpu->cp15.peripheral_port_remap.base_addr << 12)
    && addr < ((device->cpu->cp15.peripheral_port_remap.base_addr << 12) + device->cpu->cp15.decode_peripheral_port_size()))
    {
        u32 periph_addr = addr - (device->cpu->cp15.peripheral_port_remap.base_addr << 12);
    }
    else if(addr == 0xbf500000) return 0xfffffeff; //FIXME: find the real chipid. bit 8 is disabled because i don't want to have to deal with wfi instructions lol
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
        fprintf(device->reg_access_log, "Unknown peripheral port address %08x; peripheral port at %08x pc %08x\n", addr, (device->cpu->cp15.peripheral_port_remap.base_addr << 12), device->cpu->r[15]);
    }
    else printf("Unknown address %08x data %08x!\n", addr, data);
}