#pragma once

#include "common.h"

enum class aes_key_type
{
    custom, uid, gid
};

enum class aes_key_length
{
    aes128, aes192, aes256
};

struct aes_t
{
    u32 control;
    u32 unk_reg_0;
    u32 status;
    u32 unk_reg_1;
    u32 key_length;
    u32 in_size;
    u32 in_addr;
    u32 out_size;
    u32 out_addr;
    u32 aux_size;
    u32 aux_addr;
    u32 size_3;
    u8 key[0x20];
    u32 type;
    u8 iv[0x10];

    void init();

    void decrypt();
    void encrypt();

    u32 rw(u32 addr);
    void ww(u32 addr, u32 data);
};