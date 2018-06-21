#pragma once

#include "common.h"

struct usb_otg_t
{
    u32 gotgctl;
    u32 gotgint;
    u32 gahbcfg;
    u32 gusbcfg;
    u32 gintsts;
    u32 grstctl;
    u32 gintmsk;
    u32 grxstsr;
    u32 grxstsp;
    u32 grxfsiz;
    u32 gnptxfsiz;
    u32 gnptxfsts;
    u32 ghwcfg1;
    u32 ghwcfg2;
    u32 ghwcfg3;
    u32 ghwcfg4;
    u32 dieptxf[16];
    u32 dcfg;
    u32 dctl;
    u32 dsts;
    u32 diepmsk;
    u32 doepmsk;
    u32 daintsts;
    u32 daintmsk;
    u32 dtknqr1;
    u32 dtknqr2;
    u32 dtknqr3;
    u32 dtknqr4;
    u32 usb_inregs[0x80];
    u32 usb_outregs[0x80];
    u32 pcgcctl;
    u32 usb_fifo[0x440];

    void* device;

    void init();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);
};