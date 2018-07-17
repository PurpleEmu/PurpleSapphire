#include <openssl/sha.h>

#include "sha1.h"
#include "iphone2g.h"

void sha1_t::init()
{
    //TODO
    config = 0;
    in_addr = 0;
    in_size = 0;
    unk_stat = 0;
    for(int i = 0; i < 5; i++)
    {
        hash_out[i] = 0;
    }
}

void sha1_t::hash()
{
    u32 actual_in_size = in_size + 0x20;
    u32* mem = (u32*)malloc(actual_in_size);
    for(int i = 0; i < (actual_in_size >> 2); i += 4)
    {
        mem[i] = rw(device, in_addr + i);
    }

    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, (u8*)mem, actual_in_size);
    SHA1_Final((u8*)hash_out, &ctx);

    free(mem);
}

u32 sha1_t::sha1_rw(u32 addr)
{
    if((addr & 0xfff) >= 0x020 && (addr & 0xfff) < 0x34) return hash_out[(addr - 0x20) >> 2];
    switch(addr & 0xfff)
    {
        case 0x000: return config;
        case 0x084: return in_addr;
        case 0x08c: return in_size;
    }
    return 0;
}

void sha1_t::sha1_ww(u32 addr, u32 data)
{
    iphone2g* dev = (iphone2g*)device;
    switch(addr & 0xfff)
    {
        case 0x000:
        {
            if((data & 2) && (config & 8))
            {
                //start sha1
                if(!in_addr || !in_size) return;
                hash();
                dev->interrupt(0x28, true);
                dev->interrupt(0x28, false);
            }
            config = data;
            break;
        }
        case 0x004: init(); break; //Reset the SHA1 engine
        case 0x07c: unk_stat = data; break;
        case 0x084: in_addr = data; break;
        case 0x08c: in_size = data; break;
    }
}