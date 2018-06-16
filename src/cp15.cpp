#include "cp15.h"

void cp15_t::init()
{
    //TODO
    control.whole = 0x000500f8;
    control.unaligned_access_enable = 0;
}

u32 cp15_t::read(int opcode1, int opcode2, int crn, int crm)
{
    //TODO
    switch(crn)
    {
        case 0x0:
        {
            switch(opcode1)
            {
                case 0x0:
                {
                    switch(crm)
                    {
                        case 0x0:
                        {
                            switch(opcode2)
                            {
                                case 0x0: return 0x410f8767;
                                case 0x1: return 0x10152152;
                                case 0x2: return 0x00020002;
                                case 0x3: return 0x00000800;
                            }
                            break;
                        }
                        case 0x1:
                        {
                            switch(opcode2)
                            {
                                case 0x0: return 0x00000111;
                                case 0x1: return 0x00000011;
                                case 0x2: return 0x00000033;
                                case 0x3: return 0x00000000;
                                case 0x4: return 0x01130003;
                                case 0x5: return 0x10030302;
                                case 0x6: return 0x01222100;
                                case 0x7: return 0x00000000;
                            }
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
        case 0x1:
        {
            switch(opcode1)
            {
                case 0x0:
                {
                    switch(crm)
                    {
                        case 0x0:
                        {
                            switch(opcode2)
                            {
                                case 0x0: return control.whole;
                            }
                        }
                    }
                    break;
                }
            }
            break;
        }
        case 0xf:
        {
            switch(opcode1)
            {
                case 0x0:
                {
                    switch(crm)
                    {
                        case 0x2:
                        {
                            switch(opcode2)
                            {
                                case 0x4: return control.whole;
                            }
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
    }
    return 0;
}

void cp15_t::write(int opcode1, int opcode2, int crn, int crm, u32 data)
{
    //TODO
    switch(crn)
    {
        case 0x1:
        {
            switch(opcode1)
            {
                case 0x0:
                {
                    switch(crm)
                    {
                        case 0x0:
                        {
                            switch(opcode2)
                            {
                                case 0x0: control.whole = data;
                            }
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
        case 0x7:
        {
            switch(opcode1)
            {
                case 0x0:
                {
                    switch(crm)
                    {
                        case 0x5:
                        {
                            switch(opcode2)
                            {
                                case 0x0:
                                {
                                    printf("Invalidate entire instruction cache\n");
                                    break;
                                }
                                case 0x1:
                                {
                                    printf("Invalidate instruction cache line by MVA\n");
                                    break;
                                }
                                case 0x2:
                                {
                                    printf("Invalidate instruction cache line by index\n");
                                    break;
                                }
                                case 0x4:
                                {
                                    printf("Flush prefetch buffer\n");
                                    break;
                                }
                                case 0x6:
                                {
                                    printf("Flush entire branch target cache\n");
                                    break;
                                }
                                case 0x7:
                                {
                                    printf("Flush branch target cache entry by MVA\n");
                                    break;
                                }
                            }
                            break;
                        }
                        case 0x6:
                        {
                            switch(opcode2)
                            {
                                case 0x0:
                                {
                                    printf("Invalidate entire data cache\n");
                                    break;
                                }
                                case 0x1:
                                {
                                    printf("Invalidate data cache line by MVA\n");
                                    break;
                                }
                                case 0x2:
                                {
                                    printf("Invalidate data cache line by index\n");
                                    break;
                                }
                            }
                            break;
                        }
                        case 0x7:
                        {
                            if(opcode2 == 0) printf("Invalidate both caches\n");
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
        case 0xf:
        {
            switch(opcode1)
            {
                case 0x0:
                {
                    switch(crm)
                    {
                        case 0x2:
                        {
                            if(opcode2 == 0x4) peripheral_port_remap = data;
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
    }
}