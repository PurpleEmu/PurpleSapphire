#pragma once

#include "common.h"

struct spi_t
{
    u32 cmd;
    u32 base;
    u32 ctrl;
    u32 setup;
    u32 status;
    u32 pin;
    u32 tx_data;
    u32 rx_data;
    u32 clk_div;
    u32 cnt;
    u32 idd;

    u8 interrupt;

    void* device;

    void init();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);
};