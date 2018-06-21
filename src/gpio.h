#pragma once

#include "common.h"

struct gpioic_t
{
    u32 int_level[7];
    u32 int_stat[7];
    u32 int_enable[7];
    u32 int_type[7];

    void* device;

    void init();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);
};

struct gpio_t
{
    struct
    {
        u32 con;
        u32 dat;
        u32 pud1;
        u32 pud2;
        u32 conslp1;
        u32 conslp2;
        u32 pudslp1;
        u32 pudslp2;
    } regs[32];
    union
    {
        struct
        {
            u32 umask : 4;
            u32 reserved1 : 4;
            u32 minor_port : 3;
            u32 reserved2 : 5;
            u32 major_port : 5;
            u32 reserved3 : 11;
        };
        u32 whole;
    } fsel;

    void* device;
    gpioic_t gpioic;

    void init();

    u32 decode_fsel_port();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);
};