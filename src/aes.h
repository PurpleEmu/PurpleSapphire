#pragma once

#include <openssl/aes.h>
#include "common.h"

typedef u32(*rw_func)(void*, u32);
typedef void(*ww_func)(void*, u32, u32);

enum aes_key_type
{
    custom = 0, uid, gid
};

enum aes_key_length
{
    aes128 = 0, aes192 = 1, aes256 = 2
};

struct aes_t
{
    AES_KEY actual_key;

    u32 control;
    u32 unk_reg_0;
    u32 status;
    u32 unk_reg_1;
    union
    {
        struct
        {
            u32 operation : 1;
            u32 unknown1 : 2; //usually set to 3
            u32 unknown2 : 1; //usually set to 1
            u32 key_length : 2;
            u32 reserved : 26;
        };
        u32 whole;
    } key_length;
    u32 in_size;
    u32 in_addr;
    u32 out_size;
    u32 out_addr;
    u32 aux_size;
    u32 aux_addr;
    u32 size3;
    u32 key[8];

    union
    {
        struct
        {
            u32 which_key : 2;
            u32 unknown1 : 30;
        };
        u32 whole;
    } key_type;

    u32 iv[4];
    
    int key_length_op;

    u8 key_uid[16];

    void* device;

    rw_func rw;
    ww_func ww;

    void init();

    void decrypt_encrypt();

    u32 aes_rw(u32 addr);
    void aes_ww(u32 addr, u32 data);
};