#include "common.h"

struct power_t
{
    u32 config0, config1, config2;
    u32 setstate, state;
    u32 powered_on_devices;

    //void* device;

    void init();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);
};