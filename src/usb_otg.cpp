#include "usb_otg.h"

void usb_otg_t::init()
{
    grstctl = 0x80000000;
}

u32 usb_otg_t::rw(u32 addr)
{
    u32 usb_addr = addr & 0x3fff;
    if(usb_addr >= 0x104 && usb_addr < 0x144) dieptxf[((usb_addr - 4) >> 2) & 0xf];
    if(usb_addr >= 0x900 && usb_addr < 0xb00) usb_inregs[((usb_addr - 0x900) >> 2)];
    if(usb_addr >= 0xb00 && usb_addr < 0xd00) usb_outregs[((usb_addr - 0xb00) >> 2)];
    if(usb_addr >= 0x1000 && usb_addr < 0x2100) usb_fifo[((usb_addr - 0x1000) >> 2)];
    switch(usb_addr)
    {
        case 0x000: return gotgctl;
        case 0x004: return gotgint;
        case 0x008: return gahbcfg;
        case 0x00c: return gusbcfg;
        case 0x010: return grstctl;
        case 0x014: return gintsts;
        case 0x018: return gintmsk;
        case 0x01c: return grxstsr;
        case 0x020: return grxstsp;
        case 0x024: return grxfsiz;
        case 0x028: return gnptxfsiz;
        case 0x02c: return gnptxfsts;
        case 0x044: return ghwcfg1;
        case 0x048: return ghwcfg2;
        case 0x04c: return ghwcfg3;
        case 0x050: return ghwcfg4;
        case 0x800: return dcfg;
        case 0x804: return dctl;
        case 0x808: return dsts;
        case 0x810: return diepmsk;
        case 0x814: return doepmsk;
        case 0x818: return daintsts;
        case 0x81c: return daintmsk;
        case 0x820: return dtknqr1;
        case 0x824: return dtknqr2;
        case 0x830: return dtknqr3;
        case 0x834: return dtknqr4;
        case 0xe00: return pcgcctl;
    }
    return 0;
}

void usb_otg_t::ww(u32 addr, u32 data)
{
    u32 usb_addr = addr & 0x1fff;
    if(usb_addr >= 0x104 && usb_addr < 0x144) dieptxf[((usb_addr - 4) >> 2) & 0xf] = data;
    if(usb_addr >= 0x900 && usb_addr < 0xb00) usb_inregs[((usb_addr - 0x900) >> 2)] = data;
    if(usb_addr >= 0xb00 && usb_addr < 0xd00) usb_outregs[((usb_addr - 0xb00) >> 2)] = data;
    if(usb_addr >= 0x1000 && usb_addr < 0x2100) usb_fifo[((usb_addr - 0x1000) >> 2)] = data;
    switch(usb_addr)
    {
        case 0x000: gotgctl = data; break;
        case 0x004: gotgint = data; break;
        case 0x008: gahbcfg = data; break;
        case 0x00c: gusbcfg = data; break;
        case 0x010:
        {
            if(data & 1)
            {
                grstctl &= ~1;
                grstctl |= 1 << 31;
                gintmsk |= 1 << 12;
            }
            else grstctl = data;
            break;
        }
        case 0x014: gintsts = data; break;
        case 0x018: gintmsk = data; break;
        case 0x01c: grxstsr = data; break;
        case 0x020: grxstsp = data; break;
        case 0x024: grxfsiz = data; break;
        case 0x028: gnptxfsiz = data; break;
        case 0x02c: gnptxfsts = data; break;
        case 0x044: ghwcfg1 = data; break;
        case 0x048: ghwcfg2 = data; break;
        case 0x04c: ghwcfg3 = data; break;
        case 0x050: ghwcfg4 = data; break;
        case 0x800: dcfg = data; break;
        case 0x804: dctl = data; break;
        case 0x808: dsts = data; break;
        case 0x810: diepmsk = data; break;
        case 0x814: doepmsk = data; break;
        case 0x818: daintsts = data; break;
        case 0x81c: daintmsk = data; break;
        case 0x820: dtknqr1 = data; break;
        case 0x824: dtknqr2 = data; break;
        case 0x830: dtknqr3 = data; break;
        case 0x834: dtknqr4 = data; break;
        case 0xe00: pcgcctl = data; break;
    }
}