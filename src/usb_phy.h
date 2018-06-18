#pragma once

#include "common.h"

struct usb_phy_t
{
    u32 ophypwr;
    u32 ophyclk;
    u32 orstcon;
    u32 ophytune;

    void init();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);
};