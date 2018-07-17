#pragma once

#include "common.h"

typedef u32(*rw_func)(void*, u32);

struct sha1_t
{
    u32 config;
    u32 in_addr;
    u32 in_size;
    u32 unk_stat;
    u32 hash_out[5];

    void* device;

    rw_func rw;

    void init();

    void hash();

    u32 sha1_rw(u32 addr);
    void sha1_ww(u32 addr, u32 data);
};