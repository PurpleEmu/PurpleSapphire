#include "aes.h"
#include "iphone2g.h"

void aes_t::init()
{
    key_length_op = 0;
    key_length.whole = 0;
    key_type.whole = 0;
    status = 0;
    memset(key_uid, 0, 16); //fake having a uid key lol
}

void aes_t::decrypt_encrypt()
{
    u32* mem = (u32*)malloc(in_size);
    for(int i = 0; i < (in_size >> 2); i++)
    {
        mem[i] = rw(device, in_addr + (i << 2));
    }

    switch(key_type.which_key)
    {
        case aes_key_type::custom:
        {
            switch(key_length.key_length)
            {
                case aes_key_length::aes128:
                {
                    AES_set_decrypt_key((u8*)key, 128, &actual_key);
                    break;
                }
                case aes_key_length::aes192:
                {
                    AES_set_decrypt_key((u8*)(key + 0x02), 192, &actual_key);
                    break;
                }
                case aes_key_length::aes256:
                {
                    AES_set_decrypt_key((u8*)(key + 0x04), 256, &actual_key);
                    break;
                }
            }
            break;
        }
        case aes_key_type::gid:
        {
            printf("Uh-oh! Nobody has a GID key! ABORT\n");
            FILE* encryptfp = fopen("gidkeyreplace.bin", "wb");
            if(!encryptfp)
            {
                free(mem);
                return;
            }
            fwrite(mem, 1, in_size, encryptfp);
            fclose(encryptfp);
            free(mem);
            return;
        }
        case aes_key_type::uid:
        {
            AES_set_decrypt_key(key_uid, 128, &actual_key);
            break;
        }
    }

    u32* mem_writeback = (u32*)malloc(in_size);

    AES_cbc_encrypt((u8*)mem, (u8*)mem_writeback, in_size, &actual_key, (u8*)iv, key_length.operation);

    FILE* fp = nullptr;

    if(!key_length.operation) fp = fopen("aesoutput.bin", "wb");
    else fp = fopen("aesencoutput.bin", "wb");

    if(fp)
    {
        fwrite(mem_writeback, 1, out_size, fp);
        fclose(fp);
    }

    for(int i = 0; i < (out_size >> 2); i++)
    {
        ww(device, (out_addr + (i << 2)) & 0x7fffffff, mem_writeback[i]);
    }

    memset(key, 0, 0x20);
    memset(iv, 0, 0x10);

    key_length_op = 0;

    out_size = in_size;
    status = 0xf;

    free(mem);
    free(mem_writeback);
}

u32 aes_t::aes_rw(u32 addr)
{
    switch(addr & 0xfff)
    {
        case 0x000: return control;
        case 0x00c: return status;
        case 0x014: return key_length.whole;
        case 0x06c: return key_type.whole;
    }
    return 0;
}

void aes_t::aes_ww(u32 addr, u32 data)
{
    iphone2g* dev = (iphone2g*)device;
    u32 aes_addr = addr & 0xfff;
    if(aes_addr >= 0x4c && aes_addr < 0x6c) key[(addr - 0x4c) >> 2] = data;
    else if(aes_addr >= 0x74 && aes_addr < 0x84) iv[(addr - 0x74) >> 2] = data;
    else switch(aes_addr)
    {
        case 0x000: control = data; break;
        case 0x004:
        {
            decrypt_encrypt();
            dev->interrupt(0x27, true);
            dev->interrupt(0x27, false);
            break;
        }
        case 0x008: unk_reg_0 = data; break;
        case 0x010: unk_reg_1 = data; break;
        case 0x014:
        {
            if(key_length_op == 0) key_length.unknown1 = data >> 1;
            else if(key_length_op == 1) key_length.operation = data;
            else if(key_length_op == 2) key_length.key_length = data >> 4;
            else if(key_length_op == 3) key_length.unknown2 = data >> 3;

            key_length_op++;
            if(key_length_op == 4) key_length_op = 0;
            break;
        }
        case 0x018: in_size = data; break;
        case 0x020: in_addr = data; break;
        case 0x024: out_size = data; break;
        case 0x028: out_addr = data; break;
        case 0x02c: aux_size = data; break;
        case 0x030: aux_addr = data; break;
        case 0x034: size3 = data; break;
        case 0x06c: key_type.whole = data; break;
    }
}