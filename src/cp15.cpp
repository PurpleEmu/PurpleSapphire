#include "arm.h"

#define printf(...)

void cp15_t::init()
{
    //TODO
    control_arm11.whole = 0x00050078;
    control_cortex_a8.whole = 0x08c50078;
    aux_control_arm11.whole = 0x00000007;
    aux_control_cortex_a8.whole = 0x00000002;
    coprocessor_access_control.whole = 0x00000000;
    l2_cache_aux_control.whole = 0x00000042;

    do_print = true;
}

u32 cp15_t::decode_peripheral_port_size()
{
    switch(peripheral_port_remap.size)
    {
        case 0x00: return 0x00000000; break;
        case 0x02: return 0x00001000; break;
        case 0x03: return 0x00002000; break;
        case 0x04: return 0x00004000; break;
        case 0x05: return 0x00008000; break;
        case 0x06: return 0x00010000; break;
        case 0x07: return 0x00020000; break;
        case 0x08: return 0x00040000; break;
        case 0x09: return 0x00080000; break;
        case 0x0a: return 0x00100000; break;
        case 0x0b: return 0x00200000; break;
        case 0x0c: return 0x00400000; break;
        case 0x0d: return 0x00800000; break;
        case 0x0e: return 0x01000000; break;
        case 0x0f: return 0x02000000; break;
        case 0x10: return 0x04000000; break;
        case 0x11: return 0x08000000; break;
        case 0x12: return 0x10000000; break;
        case 0x13: return 0x20000000; break;
        case 0x14: return 0x40000000; break;
        case 0x15: return 0x80000000; break;
    }
    return 0;
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
                                case 0x0:
                                {
                                    switch(type)
                                    {
                                        case arm_type::arm11: return control_arm11.whole;
                                        case arm_type::cortex_a8: return control_cortex_a8.whole;
                                    }
                                    break;
                                }
                                case 0x1:
                                {
                                    switch(type)
                                    {
                                        case arm_type::arm11: return aux_control_arm11.whole;
                                        case arm_type::cortex_a8: return aux_control_cortex_a8.whole;
                                    }
                                    break;
                                }
                                case 0x2: return coprocessor_access_control.whole;
                            }
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
        case 0x2:
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
                                case 0x0: return translation_table_base_0.whole;
                                case 0x1: return translation_table_base_1.whole;
                                case 0x2: return translation_table_base_control.whole;
                            }
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
        case 0x3:
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
                                case 0x0: return domain_access_control.whole;
                            }
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
        case 0x9:
        {
            switch(opcode1)
            {
                case 0x1:
                {
                    switch(crm)
                    {
                        case 0x0:
                        {
                            switch(opcode2)
                            {
                                case 0x2: return l2_cache_aux_control.whole;
                            }
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
                            switch(opcode2)
                            {
                                case 0x4: if(type == arm_type::arm11) return peripheral_port_remap.whole;
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
                                case 0x0:
                                {
                                    switch(type)
                                    {
                                        case arm_type::arm11: control_arm11.whole = (data & 0x33e70b0f) | 0x00000078; break;
                                        case arm_type::cortex_a8: control_cortex_a8.whole = (data & 0x72003c07) | 0x00c50078 | (control_cortex_a8.whole & 0x08000000); break;
                                    }
                                    break;
                                }
                                case 0x1:
                                {
                                    switch(type)
                                    {
                                        case arm_type::arm11: aux_control_arm11.whole = data & 0xf000007f; break;
                                        case arm_type::cortex_a8: aux_control_cortex_a8.whole = (data & 0x0001fffff) | (aux_control_cortex_a8.whole & 0xc0000000); break;
                                    }
                                    break;
                                }
                                case 0x2: coprocessor_access_control.whole = data & 0x0fffffff; break;
                            }
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
        case 0x2:
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
                                case 0x0: translation_table_base_0.whole = data; break;
                                case 0x1: translation_table_base_1.whole = data; break;
                                case 0x2: translation_table_base_control.whole = data; break;
                            }
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
        case 0x3:
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
                                case 0x0: domain_access_control.whole = data; break;
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
                                    if(type == arm_type::arm11) printf("Invalidate instruction cache line by index\n");
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
                                    if(type == arm_type::arm11) printf("Invalidate entire data cache\n");
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
                        case 0xa:
                        {
                            switch(opcode2)
                            {
                                case 0x0:
                                {
                                    if(type == arm_type::arm11) printf("Clean entire data cache\n");
                                    break;
                                }
                                case 0x1:
                                {
                                    printf("Clean data cache line by MVA\n");
                                    break;
                                }
                                case 0x2:
                                {
                                    printf("Clean data cache line by index\n");
                                    break;
                                }
                                case 0x4:
                                {
                                    printf("Data Synchronization Barrier\n");
                                    break;
                                }
                            }
                            break;
                        }
                        case 0xe:
                        {
                            switch(opcode2)
                            {
                                case 0x0:
                                {
                                    if(type == arm_type::arm11) printf("Clean and Invalidate entire data cache\n");
                                    break;
                                }
                                case 0x1:
                                {
                                    printf("Clean and Invalidate data cache line by MVA\n");
                                    break;
                                }
                                case 0x2:
                                {
                                    printf("Clean and Invalidate data cache line by index\n");
                                    break;
                                }
                            }
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
        case 0x8:
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
                                case 0x0: printf("Invalidate instruction TLB unlocked entries\n"); break;
                                case 0x1: printf("Invalidate instruction TLB unlocked entry by MVA\n"); break;
                                case 0x2: printf("Invalidate instruction TLB unlocked entry by an ASID match\n"); break;
                            }
                            break;
                        }
                        case 0x6:
                        {
                            switch(opcode2)
                            {
                                case 0x0: printf("Invalidate data TLB unlocked entries\n"); break;
                                case 0x1: printf("Invalidate data TLB unlocked entry by MVA\n"); break;
                                case 0x2: printf("Invalidate data TLB unlocked entry by an ASID match\n"); break;
                            }
                            break;
                        }
                        case 0x7:
                        {
                            switch(opcode2)
                            {
                                case 0x0: printf("Invalidate unified TLB unlocked entries\n"); break;
                                case 0x1: printf("Invalidate unified TLB unlocked entry by MVA\n"); break;
                                case 0x2: printf("Invalidate unified TLB unlocked entry by an ASID match\n"); break;
                            }
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
        case 0x9:
        {
            switch(opcode1)
            {
                case 0x1:
                {
                    switch(crm)
                    {
                        case 0x0:
                        {
                            switch(opcode2)
                            {
                                case 0x2: l2_cache_aux_control.whole = data & 0x3be101cf; break;
                            }
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
                            if(opcode2 == 0x4)
                            {
                                if(type == arm_type::arm11) peripheral_port_remap.whole = data; break;
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
}