#include "iphone2g.h"

#define printf(...) do{ if(do_print) printf(__VA_ARGS__); } while(0)

void iphone2g::init()
{
    //TODO
    bootrom = (u8*)malloc(0x08000000);
    memset(bootrom, 0, 0x08000000);
    nor = (u8*)malloc(0x00100000);
    memset(nor, 0, 0x00100000);
    iboot = (u8*)malloc(0x00140000);
    memset(iboot, 0, 0x00048000);
    ram = (u8*)malloc(0x08000000);
    memset(ram, 0, 0x08000000);
    sram = (u8*)malloc(0x00500000);
    memset(sram, 0, 0x00500000);

    vics[0].init();
    vics[1].init();

    vics[0].daisy = &vics[1];
    vics[0].cpu = cpu;
    vics[1].cpu = cpu;

    clock0.init();
    clock1.init();

    wdt.device = this;
    wdt.init();

    timers.device = this;
    timers.init();

    gpio.device = this;
    gpio.init();

    spi[0].device = this;
    spi[1].device = this;
    spi[2].device = this;
    spi[0].interrupt = 0x09;
    spi[1].interrupt = 0x0a;
    spi[2].interrupt = 0x0b;
    spi[0].init();
    spi[1].init();
    spi[2].init();

    usb_phy.init();

    sha1.device = this;
    sha1.rw = iphone2g_rw;
    sha1.init();

    aes.device = this;
    aes.rw = iphone2g_rw;
    aes.ww = iphone2g_ww;
    aes.init();

    dmacs[0].device = this;
    dmacs[0].rw = iphone2g_rw;
    dmacs[0].ww = iphone2g_ww;
    dmacs[0].init();

    dmacs[1].device = this;
    dmacs[1].rw = iphone2g_rw;
    dmacs[1].ww = iphone2g_ww;
    dmacs[1].init();

    usb_otg.device = this;
    usb_otg.init();

    cpu->cp15.peripheral_port_remap.whole = 0x38000012;
    hle = false;
    do_print = true;
    reg_access_log = fopen("reglog.txt","w+");
}

void iphone2g::init_hle()
{
    hle = true;
    wdt.init_hle();

    u32 magic = iphone2g_rw(this, 0x18000000);
    if(magic == 0x496D6732) //IMG2
    {
        printf("iBoot is in IMG2 format\n");
        u32 image_type = iphone2g_rw(this, 0x18000004);
        u16 security_epoch = iphone2g_rw(this, 0x18000008) >> 16;
        u32 flags1 = iphone2g_rw(this, 0x1800000c);
        u32 data_length_padded = iphone2g_rw(this, 0x18000010);
        u32 data_length = iphone2g_rw(this, 0x18000014);
        u32 index = iphone2g_rw(this, 0x18000018);
        u32 flags2 = iphone2g_rw(this, 0x1800001c);
        
        u32 addr = 0x18000400;

        if(data_length_padded < (0x00140000 - 0x400)) memcpy(bootrom, iboot + 0x400, data_length_padded);

        cpu->r[15] = 0;
        do_print = true;
        cpu->init_hle(true);
    }
    else if(magic == 0x496d6733) //IMG3
    {
        printf("iBoot is in IMG3 format\n");
        u32 full_size = iphone2g_rw(this, 0x18000004);
        u32 size_no_pack = iphone2g_rw(this, 0x18000008);
        u32 sig_check_area = iphone2g_rw(this, 0x1800000c);
        u32 ident = iphone2g_rw(this, 0x18000010);

        u32 addr = 0x18000014;

        bool found_data_tag = false;

        while(!found_data_tag)
        {
            u32 tag_magic = iphone2g_rw(this, addr);
            addr += 4;
            u32 total_length = iphone2g_rw(this, addr);
            addr += 4;
            u32 data_length = iphone2g_rw(this, addr);
            addr += 4;
            //tag.data = (u8*)malloc(tag.data_length);
            //memcpy(tag.data, iboot + addr, tag.data_length);
            if(tag_magic == 0x44415441) //DATA
            {
                cpu->r[15] = addr;
                found_data_tag = true;
            }
            else addr += total_length - 12;
        }
        do_print = false;
        cpu->init_hle(false);
    }

    serial_buffer_log = fopen("ibootlog.txt","w+");
}

void iphone2g::exit()
{
    free(bootrom);
    free(nor);
    free(iboot);
    free(ram);
    free(sram);
    if(hle && serial_buffer_log) fclose(serial_buffer_log);
    if(reg_access_log) fclose(reg_access_log);
}

void iphone2g::tick()
{
    wdt.tick();
    timers.tick();
}

void iphone2g::interrupt(int num, bool level)
{
    num &= 0x3f;
    printf("Interrupt %08x %s\n", num, level ? "triggered!" : "lowered");
    if(num < 0x20)
    {
        if(level) vics[0].raw_intr |= 1 << num;
        else vics[0].raw_intr &= ~(1 << num);
        vics[0].update();
        switch(cpu->type)
        {
            case arm_type::arm11:
            {
                if(cpu->cp15.control_arm11.intr_vectored_mode_enable)
                    cpu->r[15] = vics[0].vect_addr[num];
                break;
            }
        }
    }
    else
    {
        if(level) vics[1].raw_intr |= 1 << (num & 0x1f);
        else vics[1].raw_intr &= ~(1 << (num & 0x1f));
        vics[1].update();
        switch(cpu->type)
        {
            case arm_type::arm11:
            {
                if(cpu->cp15.control_arm11.intr_vectored_mode_enable)
                    cpu->r[15] = vics[0].daisy_vect_addr;
                break;
            }
        }
    }
}
#undef printf
#define printf(...) do{ if(device->do_print) printf(__VA_ARGS__); } while(0)

u32 iphone2g_rw(void* dev, u32 addr)
{
    iphone2g* device = (iphone2g*) dev;
    addr &= 0xfffffffc;
    if(((addr < 0x10000) && !device->hle) || (addr >= 0x20000000 && addr < 0x20010000))
    {
        return device->bootrom[(addr+0) & 0xffff] | (device->bootrom[(addr+1) & 0xffff] << 8)
        | (device->bootrom[(addr+2) & 0xffff] << 16) | (device->bootrom[(addr+3) & 0xffff] << 24);
    }
    else if(device->hle && (addr < 0x08000000))
    {
         return device->bootrom[(addr+0) & 0x7ffffff] | (device->bootrom[(addr+1) & 0x7ffffff] << 8)
        | (device->bootrom[(addr+2) & 0x7ffffff] << 16) | (device->bootrom[(addr+3) & 0x7ffffff] << 24);
    }
    else if(addr >= 0x08000000 && addr < 0x10000000)
    {
        return device->ram[(addr+0) - 0x08000000] | (device->ram[(addr+1) - 0x08000000] << 8)
        | (device->ram[(addr+2) - 0x08000000] << 16) | (device->ram[(addr+3) - 0x08000000] << 24);
    }
    else if(addr >= 0x18000000 && addr < 0x18140000)
    {
        return device->iboot[(addr+0) - 0x18000000] | (device->iboot[(addr+1) - 0x18000000] << 8)
        | (device->iboot[(addr+2) - 0x18000000] << 16) | (device->iboot[(addr+3) - 0x18000000] << 24);
    }
    else if(addr >= 0x22000000 && addr < 0x22500000)
    {
        printf("SRAM read %08x\n", addr);
        if(addr != 0x22026430 && addr != 0x22026428 && addr != 0x22026574
        && addr != 0x22026378 && addr != 0x22026394)
        {
            return device->sram[(addr+0) - 0x22000000] | (device->sram[(addr+1) - 0x22000000] << 8)
            | (device->sram[(addr+2) - 0x22000000] << 16) | (device->sram[(addr+3) - 0x22000000] << 24);
        }
        else if(addr == 0x22026430) return 0x00000001;
        else if(addr == 0x22026428) return 0x00000001;
        else if(addr == 0x22026574) return 0x00100000;
        else if(addr == 0x22026378) return 0x01010101;
        else if(addr == 0x22026394) return 0x00000000;
    }
    else if(addr >= 0x24000000 && addr < 0x24100000)
    {
        return device->nor[(addr+0) & 0xfffff] | (device->nor[(addr+1) & 0xfffff] << 8)
        | (device->nor[(addr+2) & 0xfffff] << 16) | (device->nor[(addr+3) & 0xfffff] << 24);
    }
    else if(addr >= (device->cpu->cp15.peripheral_port_remap.base_addr << 12)
    && addr < ((device->cpu->cp15.peripheral_port_remap.base_addr << 12) + device->cpu->cp15.decode_peripheral_port_size()))
    {
        u32 periph_addr = addr - (device->cpu->cp15.peripheral_port_remap.base_addr << 12);
        if(periph_addr < 0x1000)
        {
            u32 data = device->sha1.sha1_rw(addr & 0xfff);
            fprintf(device->reg_access_log, "Sha1 read %08x pc %08x\n", addr, device->cpu->r[15]);
            return data;
        }
        else if(periph_addr >= 0x00100000 && periph_addr < 0x00101000)
        {
            fprintf(device->reg_access_log, "Clock0 read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->clock0.rw(addr & 0xfff);
        }
        else if(periph_addr >= 0x00200000 && periph_addr < 0x00201000)
        {
            fprintf(device->reg_access_log, "Dmac0 read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->dmacs[0].dmac_rw(addr & 0xfff);
        }
        else if(periph_addr >= 0x00400000 && periph_addr < 0x00403000)
        {
            fprintf(device->reg_access_log, "Usb Otg read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->usb_otg.rw(addr & 0x3fff);
        }
        else if(periph_addr >= 0x00c00000 && periph_addr < 0x00c01000)
        {
            if(device->cpu->r[15] == 0x20001fac)
            {
                printf("STOP FUCKING WITH ME\n");
            }
            u32 data = device->aes.aes_rw(addr & 0xfff);
            fprintf(device->reg_access_log, "Aes read %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            return data;
        }
        else if(periph_addr >= 0x00e00000 && periph_addr < 0x00e01000)
        {
            fprintf(device->reg_access_log, "Vic0 read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->vics[0].rw(addr & 0xfff);
        }
        else if(periph_addr >= 0x00e01000 && periph_addr < 0x00e02000)
        {
            fprintf(device->reg_access_log, "Vic1 read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->vics[1].rw(addr & 0xfff);
        }
        else if(periph_addr >= 0x00e02000 && periph_addr < 0x00e03000)
        {
            fprintf(device->reg_access_log, "EdgeIC read %08x pc %08x\n", addr, device->cpu->r[15]);
        }
        else if(periph_addr >= 0x01900000 && periph_addr < 0x01901000)
        {
            fprintf(device->reg_access_log, "Dmac1 read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->dmacs[1].dmac_rw(addr & 0xfff);
        }
        else if(periph_addr >= 0x01a00000 && periph_addr < 0x01a00080)
        {
            fprintf(device->reg_access_log, "POWER read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->power.rw(addr & 0xff);
        }
        else if(periph_addr >= 0x01a00080 && periph_addr < 0x01a00100)
        {
            fprintf(device->reg_access_log, "GPIOIC read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->gpio.gpioic.rw(addr & 0xff);
        }
        else if(periph_addr >= 0x04300000 && periph_addr < 0x04300100)
        {
            fprintf(device->reg_access_log, "Spi0 read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->spi[0].rw(addr & 0xff);
        }
        else if(periph_addr >= 0x04400000 && periph_addr < 0x04401000)
        {
            fprintf(device->reg_access_log, "Usb Phy read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->usb_phy.rw(addr & 0xfff);
        }
        else if(periph_addr >= 0x04500000 && periph_addr < 0x04501000)
        {
            fprintf(device->reg_access_log, "Clock1 read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->clock1.rw(addr & 0xfff);
        }
        else if(periph_addr >= 0x04e00000 && periph_addr < 0x04e00100)
        {
            fprintf(device->reg_access_log, "Spi1 read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->spi[1].rw(addr & 0xff);
        }
        else if(periph_addr >= 0x05200000 && periph_addr < 0x05200100)
        {
            fprintf(device->reg_access_log, "Spi2 read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->spi[2].rw(addr & 0xff);
        }
        else if(periph_addr >= 0x06200000 && periph_addr < 0x06220000)
        {
            fprintf(device->reg_access_log, "Timer read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->timers.rw(addr & 0x1ffff);
        }
        else if(periph_addr >= 0x06300000 && periph_addr < 0x06301000)
        {
            fprintf(device->reg_access_log, "WDT read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->wdt.rw(addr & 0xfff);
        }
        else if(periph_addr >= 0x06400000 && periph_addr < 0x06401000)
        {
            fprintf(device->reg_access_log, "GPIO read %08x pc %08x\n", addr, device->cpu->r[15]);
            return device->gpio.rw(addr & 0xfff);
        }
        else fprintf(device->reg_access_log, "Unknown peripheral port address %08x; peripheral port at %08x pc %08x\n", addr, (device->cpu->cp15.peripheral_port_remap.base_addr << 12), device->cpu->r[15]);
    }
    else if(addr >= 0x88000000 && addr < 0x90000000)
    {
        return device->ram[(addr+0) - 0x88000000] | (device->ram[(addr+1) - 0x88000000] << 8)
        | (device->ram[(addr+2) - 0x88000000] << 16) | (device->ram[(addr+3) - 0x88000000] << 24);
    }
    else printf("Unknown address %08x!\n", addr);
    return 0;
}

void iphone2g_ww(void* dev, u32 addr, u32 data)
{
    iphone2g* device = (iphone2g*) dev;
    addr &= 0xfffffffc;
    if((addr < 0x08000000) && device->hle)
    {
        device->bootrom[(addr+0) & 0x7ffffff] = (data >> 0) & 0xff;
        device->bootrom[(addr+1) & 0x7ffffff] = (data >> 8) & 0xff;
        device->bootrom[(addr+2) & 0x7ffffff] = (data >> 16) & 0xff;
        device->bootrom[(addr+3) & 0x7ffffff] = (data >> 24) & 0xff;
    }
    else if(addr >= 0x08000000 && addr < 0x10000000)
    {
        device->ram[(addr+0) & 0x7ffffff] = (data >> 0) & 0xff;
        device->ram[(addr+1) & 0x7ffffff] = (data >> 8) & 0xff;
        device->ram[(addr+2) & 0x7ffffff] = (data >> 16) & 0xff;
        device->ram[(addr+3) & 0x7ffffff] = (data >> 24) & 0xff;
        if(addr >= 0x000447a0 && addr <= 0x00044b88)
        {
            printf("bufferPrint write %08x data %08x\n", addr, data);
            if(device->cpu->r[15] == 0x0001423e)
            {
                char c1 = device->bootrom[addr];
                char c2 = device->bootrom[addr + 1];
                char c3 = device->bootrom[addr + 2];
                char c4 = device->bootrom[addr + 3];
                fputc(c1, device->serial_buffer_log);
                fputc(c2, device->serial_buffer_log);
                fputc(c3, device->serial_buffer_log);
                fputc(c4, device->serial_buffer_log);
                fflush(device->serial_buffer_log);
            }
        }
    }
    else if(addr >= 0x18000000 && addr < 0x18048000)
    {
        device->iboot[(addr+0) - 0x18000000] = (data >> 0) & 0xff;
        device->iboot[(addr+1) - 0x18000000] = (data >> 8) & 0xff;
        device->iboot[(addr+2) - 0x18000000] = (data >> 16) & 0xff;
        device->iboot[(addr+3) - 0x18000000] = (data >> 24) & 0xff;
    }
    else if(addr >= 0x22000000 && addr < 0x22500000)
    {
        printf("SRAM write %08x data %08x\n", addr, data);
        device->sram[(addr+0) - 0x22000000] = (data >> 0) & 0xff;
        device->sram[(addr+1) - 0x22000000] = (data >> 8) & 0xff;
        device->sram[(addr+2) - 0x22000000] = (data >> 16) & 0xff;
        device->sram[(addr+3) - 0x22000000] = (data >> 24) & 0xff;
    }
    else if(addr >= (device->cpu->cp15.peripheral_port_remap.base_addr << 12)
    && addr < ((device->cpu->cp15.peripheral_port_remap.base_addr << 12) + device->cpu->cp15.decode_peripheral_port_size()))
    {
        u32 periph_addr = addr - (device->cpu->cp15.peripheral_port_remap.base_addr << 12);
        if(periph_addr < 0x1000)
        {
            fprintf(device->reg_access_log, "Sha1 write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->sha1.sha1_ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x00100000 && periph_addr < 0x00101000)
        {
            fprintf(device->reg_access_log, "Clock0 write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->clock0.ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x00200000 && periph_addr < 0x00201000)
        {
            fprintf(device->reg_access_log, "Dmac0 write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->dmacs[0].dmac_ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x00400000 && periph_addr < 0x00410000)
        {
            fprintf(device->reg_access_log, "Usb Otg write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->usb_otg.ww(addr & 0x3fff, data);
        }
        else if(periph_addr >= 0x00c00000 && periph_addr < 0x00c01000)
        {
            fprintf(device->reg_access_log, "Aes write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            fflush(device->reg_access_log);
            device->aes.aes_ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x00e00000 && periph_addr < 0x00e01000)
        {
            fprintf(device->reg_access_log, "Vic0 write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->vics[0].ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x00e01000 && periph_addr < 0x00e02000)
        {
            fprintf(device->reg_access_log, "Vic1 write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->vics[1].ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x00e02000 && periph_addr < 0x00e03000)
        {
            fprintf(device->reg_access_log, "EdgeIC write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
        }
        else if(periph_addr >= 0x01900000 && periph_addr < 0x01901000)
        {
            fprintf(device->reg_access_log, "Dmac1 write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->dmacs[1].dmac_ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x01a00000 && periph_addr < 0x01a00080)
        {
            fprintf(device->reg_access_log, "POWER write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->power.ww(addr & 0x7f, data);
        }
        else if(periph_addr >= 0x01a00080 && periph_addr < 0x01a00100)
        {
            fprintf(device->reg_access_log, "GPIOIC write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->gpio.gpioic.ww(addr & 0x7f, data);
        }
        else if(periph_addr >= 0x04300000 && periph_addr < 0x04301000)
        {
            fprintf(device->reg_access_log, "Spi0 write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->spi[0].ww(addr & 0xff, data);
        }
        else if(periph_addr >= 0x04400000 && periph_addr < 0x04401000)
        {
            fprintf(device->reg_access_log, "Usb Phy write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->usb_phy.ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x04500000 && periph_addr < 0x04501000)
        {
            fprintf(device->reg_access_log, "Clock1 write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->clock1.ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x04e00000 && periph_addr < 0x04e00100)
        {
            fprintf(device->reg_access_log, "Spi1 write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->spi[1].ww(addr & 0xff, data);
        }
        else if(periph_addr >= 0x05200000 && periph_addr < 0x05200100)
        {
            fprintf(device->reg_access_log, "Spi2 write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->spi[2].ww(addr & 0xff, data);
        }
        else if(periph_addr >= 0x06200000 && periph_addr < 0x06220000)
        {
            fprintf(device->reg_access_log, "Timer write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->timers.ww(addr & 0x1ffff, data);
        }
        else if(periph_addr >= 0x06300000 && periph_addr < 0x06301000)
        {
            fprintf(device->reg_access_log, "WDT write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->wdt.ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x06400000 && periph_addr < 0x06401000)
        {
            fprintf(device->reg_access_log, "GPIO write %08x data %08x pc %08x\n", addr, data, device->cpu->r[15]);
            device->gpio.ww(addr & 0xfff, data);
        }
        else fprintf(device->reg_access_log, "Unknown peripheral port address %08x data %08x; peripheral port at %08x pc %08x\n", addr, data, (device->cpu->cp15.peripheral_port_remap.base_addr << 12), device->cpu->r[15]);
    }
    else if(addr >= 0x80000000 && addr < 0x88000000)
    {
        device->ram[(addr+0) & 0x7ffffff] = (data >> 0) & 0xff;
        device->ram[(addr+1) & 0x7ffffff] = (data >> 8) & 0xff;
        device->ram[(addr+2) & 0x7ffffff] = (data >> 16) & 0xff;
        device->ram[(addr+3) & 0x7ffffff] = (data >> 24) & 0xff;
    }
    else printf("Unknown address %08x data %08x!\n", addr, data);
}