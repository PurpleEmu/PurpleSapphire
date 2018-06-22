#include "iphone3gs.h"

#define printf(...) do{ if(do_print) printf(__VA_ARGS__); } while(0)

void iphone3gs::init()
{
    //TODO
    vics[0].init();
    vics[1].init();
    vics[2].init();

    vics[0].daisy = &vics[1];
    vics[1].daisy = &vics[2];
    vics[0].cpu = cpu;
    vics[1].cpu = cpu;
    vics[2].cpu = cpu;

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

void iphone3gs::interrupt(int num, bool level)
{
    //TODO
    num %= 0x60;
    printf("Interrupt %08x %s\n", num, level ? "triggered!" : "lowered");
    if(num < 0x20)
    {
        if(level) vics[0].raw_intr |= 1 << num;
        else vics[0].raw_intr &= ~(1 << num);
        vics[0].update();
        //the cortex-a8 doesn't appear to have a vectored interrupt mode, so this is commented out.
        //if(cpu->cp15.control_cortex_a8.intr_vectored_mode_enable)
        //    cpu->r[15] = vics[0].vect_addr[num];
    }
    else if(num < 0x40)
    {
        if(level) vics[1].raw_intr |= 1 << (num & 0x1f);
        else vics[1].raw_intr &= ~(1 << (num & 0x1f));
        vics[1].update();
        //see above
        //if(cpu->cp15.control_cortex_a8.intr_vectored_mode_enable)
        //    cpu->r[15] = vics[0].daisy_vect_addr;
    }
    else
    {
        if(level) vics[2].raw_intr |= 1 << (num & 0x1f);
        else vics[2].raw_intr &= ~(1 << (num & 0x1f));
        vics[2].update();
        //see above
        //if(cpu->cp15.control_cortex_a8.intr_vectored_mode_enable)
        //    cpu->r[15] = vics[0].daisy_vect_addr;
    }
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
        if(periph_addr >= 0x4f200000 && periph_addr < 0x4f210000)
        {
            fprintf(device->reg_access_log, "Vic0 read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->vics[0].rw(addr & 0xfff);
        }
        else if(periph_addr >= 0x4f210000 && periph_addr < 0x4f220000)
        {
            fprintf(device->reg_access_log, "Vic1 read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->vics[1].rw(addr & 0xfff);
        }
        else if(periph_addr >= 0x4f220000 && periph_addr < 0x4f230000)
        {
            fprintf(device->reg_access_log, "Vic2 read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->vics[2].rw(addr & 0xfff);
        }
        else fprintf(device->reg_access_log, "Unknown peripheral port address %08x; peripheral port at %08x pc %08x\n", addr, (device->cpu->cp15.peripheral_port_remap.base_addr << 12), device->cpu->r[15]);
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
        if(periph_addr >= 0x4f200000 && periph_addr < 0x4f210000)
        {
            fprintf(device->reg_access_log, "Vic0 write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->vics[0].ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x4f210000 && periph_addr < 0x4f220000)
        {
            fprintf(device->reg_access_log, "Vic1 write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->vics[1].ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x4f220000 && periph_addr < 0x4f230000)
        {
            fprintf(device->reg_access_log, "Vic2 write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->vics[2].ww(addr & 0xfff, data);
        }
        else fprintf(device->reg_access_log, "Unknown peripheral port address %08x data %08x; peripheral port at %08x pc %08x\n", addr, data, (device->cpu->cp15.peripheral_port_remap.base_addr << 12), device->cpu->r[15]);
    }
    else printf("Unknown address %08x data %08x!\n", addr, data);
}