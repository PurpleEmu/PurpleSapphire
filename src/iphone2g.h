#include "common.h"

struct iphone2g
{
    u8 bootrom[0x10000];
    u8 bootram[0x10000];

    void init();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);
};