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

    cpu->cp15.peripheral_port_remap.whole = 0x38000012;
}

void iphone2g::tick()
{
    wdt.tick();
    timers.tick();
}

void iphone2g::interrupt(int num, bool level)
{
    num &= 0x3f;
    printf("Interrupt %08x triggered!\n", num);
    if(num < 0x20)
    {
        if(level) vics[0].raw_intr |= 1 << num;
        else vics[0].raw_intr &= ~(1 << num);
        vics[0].update();
    }
    else
    {
        if(level) vics[1].raw_intr |= 1 << (num & 0x1f);
        else vics[1].raw_intr &= ~(1 << (num & 0x1f));
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
    else if(addr >= 0x22000000 && addr < 0x22400000)
    {
        printf("AMC0 read %08x\n", addr);
        //if(addr != 0x22026430 && addr != 0x2202631c && addr != 0x22026428 && addr != 0x2202637b
        //&& addr != 0x22026574 && addr != 0x22026394 && addr != 0x22026394 && addr != 0x220263bc) //HACK
        {
            return device->amc0[(addr+0) & 0x3fffff] | (device->amc0[(addr+1) & 0x3fffff] << 8)
            | (device->amc0[(addr+2) & 0x3fffff] << 16) | (device->amc0[(addr+3) & 0x3fffff] << 24);
        }
        /*else if(addr == 0x22026430) return 0x00000001;
        else if(addr == 0x2202631c) return 0x00000001;
        else if(addr == 0x22026428) return 0x00000001;
        else if(addr == 0x2202637b) return 0x01010101;
        else if(addr == 0x22026574) return 0x00100000;
        else if(addr == 0x22026394) return 0x00000001;
        else if(addr == 0x220263bc) return 0x00000001;*/
    }
    else if(addr >= 0x24000000 && addr < 0x24100000)
    {
        return device->nor[(addr+0) & 0x1fffff] | (device->nor[(addr+1) & 0x1fffff] << 8)
        | (device->nor[(addr+2) & 0x1fffff] << 16) | (device->nor[(addr+3) & 0x1fffff] << 24);
    }
    else if(addr >= (device->cpu->cp15.peripheral_port_remap.base_addr << 12)
    && addr < ((device->cpu->cp15.peripheral_port_remap.base_addr << 12) + device->cpu->cp15.decode_peripheral_port_size()))
    {
        u32 periph_addr = addr - (device->cpu->cp15.peripheral_port_remap.base_addr << 12);
        if(periph_addr == 0) return 0x00000008;
        else if(periph_addr >= 0x00100000 && periph_addr < 0x00101000)
        {
            printf("Clock0 read %08x\n", addr);
            return device->clock0.rw(addr & 0xfff);
        }
        else if(periph_addr >= 0x00e00000 && periph_addr < 0x00e01000)
        {
            printf("Vic0 read %08x\n", addr);
            return device->vics[0].rw(addr & 0xfff);
        }
        else if(periph_addr >= 0x00e01000 && periph_addr < 0x00e02000)
        {
            printf("Vic1 read %08x\n", addr);
            return device->vics[1].rw(addr & 0xfff);
        }
        else if(periph_addr >= 0x04300000 && periph_addr < 0x04300100)
        {
            printf("Spi0 read %08x\n", addr);
            return device->spi[0].rw(addr & 0xff);
        }
        else if(periph_addr >= 0x04400000 && periph_addr < 0x04401000)
        {
            printf("Usb Phy read %08x\n", addr);
            return device->usb_phy.rw(addr & 0xfff);
        }
        else if(periph_addr >= 0x04500000 && periph_addr < 0x04501000)
        {
            printf("Clock1 read %08x\n", addr);
            return device->clock1.rw(addr & 0xfff);
        }
        else if(periph_addr >= 0x04e00000 && periph_addr < 0x04e00100)
        {
            printf("Spi1 read %08x\n", addr);
            return device->spi[1].rw(addr & 0xff);
        }
        else if(periph_addr >= 0x05200000 && periph_addr < 0x05200100)
        {
            printf("Spi2 read %08x\n", addr);
            return device->spi[2].rw(addr & 0xff);
        }
        else if(periph_addr >= 0x06200000 && periph_addr < 0x06220000)
        {
            printf("Timer read %08x\n", addr);
            return device->timers.rw(addr & 0x1ffff);
        }
        else if(periph_addr >= 0x06300000 && periph_addr < 0x06301000)
        {
            printf("WDT read %08x\n", addr);
            return device->wdt.rw(addr & 0xfff);
        }
        else if(periph_addr >= 0x06400000 && periph_addr < 0x06401000)
        {
            printf("GPIO read %08x\n", addr);
            return device->gpio.rw(addr & 0xfff);
        }
        else printf("Unknown peripheral port address %08x; peripheral port at %08x\n", addr, (device->cpu->cp15.peripheral_port_remap.base_addr << 12));
    }
    else printf("Unknown address %08x!\n", addr);
    return 0;
}

void iphone2g_ww(void* dev, u32 addr, u32 data)
{
    iphone2g* device = (iphone2g*) dev;
    if(addr >= 0x22000000 && addr < 0x22400000)
    {
        printf("AMC0 write %08x data %08x\n", addr, data);
        device->amc0[(addr+0) & 0x3fffff] = (data >> 0) & 0xff;
        device->amc0[(addr+1) & 0x3fffff] = (data >> 8) & 0xff;
        device->amc0[(addr+2) & 0x3fffff] = (data >> 16) & 0xff;
        device->amc0[(addr+3) & 0x3fffff] = (data >> 24) & 0xff;
    }
    else if(addr >= (device->cpu->cp15.peripheral_port_remap.base_addr << 12)
    && addr < ((device->cpu->cp15.peripheral_port_remap.base_addr << 12) + device->cpu->cp15.decode_peripheral_port_size()))
    {
        u32 periph_addr = addr - (device->cpu->cp15.peripheral_port_remap.base_addr << 12);
        if(periph_addr >= 0x00100000 && periph_addr < 0x00101000)
        {
            printf("Clock0 write %08x data %08x\n", addr, data);
            device->clock0.ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x00e00000 && periph_addr < 0x00e01000)
        {
            printf("Vic0 write %08x data %08x\n", addr, data);
            device->vics[0].ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x00e01000 && periph_addr < 0x00e02000)
        {
            printf("Vic1 write %08x data %08x\n", addr, data);
            device->vics[1].ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x04300000 && periph_addr < 0x04300100)
        {
            printf("Spi0 write %08x data %08x\n", addr, data);
            device->spi[0].ww(addr & 0xff, data);
        }
        else if(periph_addr >= 0x04400000 && periph_addr < 0x04401000)
        {
            printf("Usb Phy write %08x data %08x\n", addr, data);
            device->usb_phy.ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x04500000 && periph_addr < 0x04501000)
        {
            printf("Clock1 write %08x data %08x\n", addr, data);
            device->clock1.ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x04e00000 && periph_addr < 0x04e00100)
        {
            printf("Spi1 write %08x data %08x\n", addr, data);
            device->spi[1].ww(addr & 0xff, data);
        }
        else if(periph_addr >= 0x05200000 && periph_addr < 0x05200100)
        {
            printf("Spi0 write %08x data %08x\n", addr, data);
            device->spi[0].ww(addr & 0xff, data);
        }
        else if(periph_addr >= 0x06200000 && periph_addr < 0x06220000)
        {
            printf("Timer write %08x data %08x\n", addr, data);
            device->timers.ww(addr & 0x1ffff, data);
        }
        else if(periph_addr >= 0x06300000 && periph_addr < 0x06301000)
        {
            printf("WDT write %08x data %08x\n", addr, data);
            device->wdt.ww(addr & 0xfff, data);
        }
        else if(periph_addr >= 0x06400000 && periph_addr < 0x06401000)
        {
            printf("GPIO write %08x data %08x\n", addr, data);
            device->gpio.ww(addr & 0xfff, data);
        }
        else printf("Unknown peripheral port address %08x data %08x; peripheral port at %08x\n", addr, data, (device->cpu->cp15.peripheral_port_remap.base_addr << 12));
    }
    else printf("Unknown address %08x data %08x!\n", addr, data);
}