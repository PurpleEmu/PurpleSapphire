#include "arm.h"

void arm_cpu::init()
{
    for(int i = 0; i < 16; i++)
    {
        r[i] = 0;
    }

    cpsr.whole = 0x400001d3;

    fiq = false;
    irq = false;
    data_abort = false;
    undefined = false;
    reset = true;
    fiq_enable = true;
    irq_enable = true;
    abort_enable = true;
    undefined_enable = true;

    do_print = true;
    just_branched = false;

    opcode = 0;
}

u32 arm_cpu::rw(u32 addr)
{
    return rw_real(device, addr);
}

void arm_cpu::ww(u32 addr, u32 data)
{
    ww_real(device, addr, data);
}

enum class arm_cond : u8
{
    equal = 0x0, notequal = 0x1, carryset = 0x2, carryclear = 0x3,
    negative = 0x4, positive = 0x5, overflow = 0x6, nooverflow = 0x7,
    above = 0x8, beloworequal = 0x9, greaterorequal = 0xa, lesser = 0xb,
    greater = 0xc, lesserorequal = 0xd, always = 0xe, never = 0xf
};

void arm_cpu::tick()
{
    u32 true_r15 = 0;
    if(!cpsr.thumb)
    {
        true_r15 = r[15] - 4;
        if(just_branched) true_r15 -= 4;
    }
    else
    {
        true_r15 = r[15] - 2;
        if(just_branched) true_r15 -= 2;
    }

    if(reset)
    {
        arm_mode old_mode = cpsr.mode;

        cpsr.mode = mode_undefined;

        u32 saved_reg[15];

        switch(old_mode)
        {
        case mode_user:
        case mode_system:
        {
            for(int i = 0; i < 15; i++)
            {
                saved_reg[i] = r[i];
            }
            break;
        }
        case mode_fiq:
        {
            for(int i = 8; i < 15; i++)
            {
                saved_reg[i] = r[i];
            }
            break;
        }
        case mode_irq:
        case mode_supervisor:
        case mode_abort:
        case mode_undefined:
        {
            for(int i = 13; i < 15; i++)
            {
                saved_reg[i] = r[i];
            }
            break;
        }
        }

        switch(cpsr.mode)
        {
        case mode_user:
        case mode_system:
        {
            for(int i = 0; i < 15; i++)
            {
                r[i] = saved_reg[i];
            }
            break;
        }
        case mode_fiq:
        {
            r8_fiq = saved_reg[8];
            r9_fiq = saved_reg[9];
            r10_fiq = saved_reg[10];
            r11_fiq = saved_reg[11];
            r12_fiq = saved_reg[12];
            r13_fiq = saved_reg[13];
            r14_fiq = saved_reg[14];
            break;
        }
        case mode_irq:
        {
            r13_irq = saved_reg[13];
            r14_irq = saved_reg[14];
            break;
        }
        case mode_supervisor:
        {
            r13_svc = saved_reg[13];
            r14_svc = saved_reg[14];
            break;
        }
        case mode_abort:
        {
            r13_abt = saved_reg[13];
            r14_abt = saved_reg[14];
            break;
        }
        case mode_undefined:
        {
            r13_und = saved_reg[13];
            r14_und = saved_reg[14];
            break;
        }
        }
        
        r[15] = 0x00;

        cpsr.thumb = false;
        cpsr.jazelle = false;
        cpsr.irq_disable = true;
        cpsr.fiq_disable = true;
        cpsr.abort_disable = true;
        reset = false;

        next_opcode = rw(r[15]);
        r[15] += 4;
    }

    if(data_abort && !cpsr.abort_disable && abort_enable)
    {
        arm_mode old_mode = cpsr.mode;

        cpsr.mode = mode_abort;

        u32 saved_reg[15];

        switch(old_mode)
        {
        case mode_user:
        case mode_system:
        {
            for(int i = 0; i < 15; i++)
            {
                saved_reg[i] = r[i];
            }
            break;
        }
        case mode_fiq:
        {
            for(int i = 8; i < 15; i++)
            {
                saved_reg[i] = r[i];
            }
            break;
        }
        case mode_irq:
        case mode_supervisor:
        case mode_abort:
        case mode_undefined:
        {
            for(int i = 13; i < 15; i++)
            {
                saved_reg[i] = r[i];
            }
            break;
        }
        }

        switch(cpsr.mode)
        {
        case mode_user:
        case mode_system:
        {
            for(int i = 0; i < 15; i++)
            {
                r[i] = saved_reg[i];
            }
            break;
        }
        case mode_fiq:
        {
            r8_fiq = saved_reg[8];
            r9_fiq = saved_reg[9];
            r10_fiq = saved_reg[10];
            r11_fiq = saved_reg[11];
            r12_fiq = saved_reg[12];
            r13_fiq = saved_reg[13];
            r14_fiq = saved_reg[14];
            break;
        }
        case mode_irq:
        {
            r13_irq = saved_reg[13];
            r14_irq = saved_reg[14];
            break;
        }
        case mode_supervisor:
        {
            r13_svc = saved_reg[13];
            r14_svc = saved_reg[14];
            break;
        }
        case mode_abort:
        {
            r13_abt = saved_reg[13];
            r14_abt = saved_reg[14];
            break;
        }
        case mode_undefined:
        {
            r13_und = saved_reg[13];
            r14_und = saved_reg[14];
            break;
        }
        }
        r[14] = r[15] + 8;
        r[15] = 0x10;
        cpsr.thumb = false;
        cpsr.abort_disable = true;
        abort_enable = false;
    }
    else if(fiq && !cpsr.fiq_disable && fiq_enable)
    {
        arm_mode old_mode = cpsr.mode;

        cpsr.mode = mode_fiq;

        u32 saved_reg[15];

        switch(old_mode)
        {
        case mode_user:
        case mode_system:
        {
            for(int i = 0; i < 15; i++)
            {
                saved_reg[i] = r[i];
            }
            break;
        }
        case mode_fiq:
        {
            for(int i = 8; i < 15; i++)
            {
                saved_reg[i] = r[i];
            }
            break;
        }
        case mode_irq:
        case mode_supervisor:
        case mode_abort:
        case mode_undefined:
        {
            for(int i = 13; i < 15; i++)
            {
                saved_reg[i] = r[i];
            }
            break;
        }
        }

        switch(cpsr.mode)
        {
        case mode_user:
        case mode_system:
        {
            for(int i = 0; i < 15; i++)
            {
                r[i] = saved_reg[i];
            }
            break;
        }
        case mode_fiq:
        {
            r8_fiq = saved_reg[8];
            r9_fiq = saved_reg[9];
            r10_fiq = saved_reg[10];
            r11_fiq = saved_reg[11];
            r12_fiq = saved_reg[12];
            r13_fiq = saved_reg[13];
            r14_fiq = saved_reg[14];
            break;
        }
        case mode_irq:
        {
            r13_irq = saved_reg[13];
            r14_irq = saved_reg[14];
            break;
        }
        case mode_supervisor:
        {
            r13_svc = saved_reg[13];
            r14_svc = saved_reg[14];
            break;
        }
        case mode_abort:
        {
            r13_abt = saved_reg[13];
            r14_abt = saved_reg[14];
            break;
        }
        case mode_undefined:
        {
            r13_und = saved_reg[13];
            r14_und = saved_reg[14];
            break;
        }
        }
        r[14] = r[15] + 4;
        r[15] = 0x1c;
        cpsr.fiq_disable = true;
        fiq_enable = false;
        printf("ARM11 got FIQ!\n");
    }
    else if(irq && !cpsr.irq_disable && irq_enable)
    {
        arm_mode old_mode = cpsr.mode;

        cpsr.mode = mode_irq;

        u32 saved_reg[15];

        switch(old_mode)
        {
        case mode_user:
        case mode_system:
        {
            for(int i = 0; i < 15; i++)
            {
                saved_reg[i] = r[i];
            }
            break;
        }
        case mode_fiq:
        {
            for(int i = 8; i < 15; i++)
            {
                saved_reg[i] = r[i];
            }
            break;
        }
        case mode_irq:
        case mode_supervisor:
        case mode_abort:
        case mode_undefined:
        {
            for(int i = 13; i < 15; i++)
            {
                saved_reg[i] = r[i];
            }
            break;
        }
        }

        switch(cpsr.mode)
        {
        case mode_user:
        case mode_system:
        {
            for(int i = 0; i < 15; i++)
            {
                r[i] = saved_reg[i];
            }
            break;
        }
        case mode_fiq:
        {
            r8_fiq = saved_reg[8];
            r9_fiq = saved_reg[9];
            r10_fiq = saved_reg[10];
            r11_fiq = saved_reg[11];
            r12_fiq = saved_reg[12];
            r13_fiq = saved_reg[13];
            r14_fiq = saved_reg[14];
            break;
        }
        case mode_irq:
        {
            r13_irq = saved_reg[13];
            r14_irq = saved_reg[14];
            break;
        }
        case mode_supervisor:
        {
            r13_svc = saved_reg[13];
            r14_svc = saved_reg[14];
            break;
        }
        case mode_abort:
        {
            r13_abt = saved_reg[13];
            r14_abt = saved_reg[14];
            break;
        }
        case mode_undefined:
        {
            r13_und = saved_reg[13];
            r14_und = saved_reg[14];
            break;
        }
        }
        r[14] = r[15] + 4;
        r[15] = 0x18;
        cpsr.irq_disable = true;
        irq_enable = false;
        printf("ARM11 got IRQ!\n");
    }

    if(undefined && undefined_enable)
    {
        arm_mode old_mode = cpsr.mode;

        cpsr.mode = mode_undefined;

        u32 saved_reg[15];

        switch(old_mode)
        {
        case mode_user:
        case mode_system:
        {
            for(int i = 0; i < 15; i++)
            {
                saved_reg[i] = r[i];
            }
            break;
        }
        case mode_fiq:
        {
            for(int i = 8; i < 15; i++)
            {
                saved_reg[i] = r[i];
            }
            break;
        }
        case mode_irq:
        case mode_supervisor:
        case mode_abort:
        case mode_undefined:
        {
            for(int i = 13; i < 15; i++)
            {
                saved_reg[i] = r[i];
            }
            break;
        }
        }

        switch(cpsr.mode)
        {
        case mode_user:
        case mode_system:
        {
            for(int i = 0; i < 15; i++)
            {
                r[i] = saved_reg[i];
            }
            break;
        }
        case mode_fiq:
        {
            r8_fiq = saved_reg[8];
            r9_fiq = saved_reg[9];
            r10_fiq = saved_reg[10];
            r11_fiq = saved_reg[11];
            r12_fiq = saved_reg[12];
            r13_fiq = saved_reg[13];
            r14_fiq = saved_reg[14];
            break;
        }
        case mode_irq:
        {
            r13_irq = saved_reg[13];
            r14_irq = saved_reg[14];
            break;
        }
        case mode_supervisor:
        {
            r13_svc = saved_reg[13];
            r14_svc = saved_reg[14];
            break;
        }
        case mode_abort:
        {
            r13_abt = saved_reg[13];
            r14_abt = saved_reg[14];
            break;
        }
        case mode_undefined:
        {
            r13_und = saved_reg[13];
            r14_und = saved_reg[14];
            break;
        }
        }
        r[14] = r[15] + 4;
        r[15] = 0x04;
        cpsr.thumb = false;
        cpsr.jazelle = false;
        cpsr.irq_disable = true;
        undefined_enable = false;
    }

    if(!irq) irq_enable = true;
    if(!fiq) fiq_enable = true;
    if(!data_abort) abort_enable = true;
    if(!undefined) undefined_enable = true;

    if(!cpsr.thumb)
    {
        bool old_just_branched = just_branched;
        opcode = next_opcode;
        next_opcode = rw(r[15]);
        r[15] += 4;
        just_branched = false;
        printf("Opcode:%08x\nPC:%08x\n", opcode, true_r15);
        for(int i = 0; i < 15; i++)
        {
            printf("R%d:%08x\n", i, r[i]);
        }
    }
    else
    {
        bool old_just_branched = just_branched;
        opcode = next_opcode;
        next_opcode = rw(r[15]);
        if(r[15] & 2) next_opcode >>= 16;
        else next_opcode &= 0xffff;
        r[15] += 2;
        just_branched = false;
        printf("Opcode:%04x\nPC:%08x\n", opcode, true_r15);
        for(int i = 0; i < 15; i++)
        {
            printf("R%d:%08x\n", i, r[i]);
        }
    }
    if(just_branched)
    {
        if(cpsr.thumb)
        {
            next_opcode = rw(r[15]);
            if(r[15] & 2) next_opcode >>= 16;
            else next_opcode &= 0xffff;
            r[15] += 2;
        }
        else
        {
            next_opcode = rw(r[15]);
            r[15] += 4;
        }
        just_branched = false;
    }
}

void arm_cpu::run(int insns)
{
    for(int i = 0; i<insns; i++)
    {
        tick();
    }
}