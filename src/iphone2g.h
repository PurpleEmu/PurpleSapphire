#include "common.h"

struct iphone2g
{
    u8 bootrom[0x10000];
    u8 bootram[0x10000];

    void init();
};

u32 iphone2g_rw(void* dev, u32 addr);
void iphone2g_ww(void* dev, u32 addr, u32 data);