#pragma once

#include "common.h"
#include "aes.h"
#include "arm.h"
#include "clock.h"
#include "dmac.h"
#include "gpio.h"
#include "power.h"
#include "sha1.h"
#include "spi.h"
#include "timer.h"
#include "usb_otg.h"
#include "usb_phy.h"
#include "vic.h"
#include "wdt.h"

struct iphone2g
{
    u8* bootrom; //[0x8000000]
    u8* nor; //[0x100000]
    u8* iboot; //[0x140000]
    u8* ram; //[0x8000000]
    u8* sram; //[0x500000]

    arm_cpu* cpu;

    bool hle;
    bool do_print;

    FILE* reg_access_log;
    FILE* serial_buffer_log;

    vic vics[2];
    clock0_t clock0;
    clock1_t clock1;
    wdt_t wdt;
    timers_t timers;
    gpio_t gpio;
    spi_t spi[3];
    usb_phy_t usb_phy;
    power_t power;
    sha1_t sha1;
    aes_t aes;
    dmac dmacs[2];
    usb_otg_t usb_otg;

    void init();
    void init_hle();
    void exit();

    void tick();

    void interrupt(int num, bool level);
};

u32 iphone2g_rw(void* dev, u32 addr);
void iphone2g_ww(void* dev, u32 addr, u32 data);