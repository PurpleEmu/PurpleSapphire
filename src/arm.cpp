#include "arm.h"

void arm_cpu::init()
{
    for(int i = 0; i < 16; i++)
    {
        r[i] = 0;
    }

    for(int i = 0; i < 7; i++)
    {
        r_fiq[i] = 0;
    }

    for(int i = 0; i < 2; i++)
    {
        r_abt[i] = 0;
        r_irq[i] = 0;
        r_svc[i] = 0;
        r_und[i] = 0;
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

u32 arm_cpu::get_register(int reg)
{
    if((reg < 8) || (reg == 15)) return r[reg];
    switch(cpsr.mode)
    {
        case arm_mode::mode_user:
        case arm_mode::mode_system: return r[reg];
        case arm_mode::mode_fiq: return r_fiq[reg - 8];
        default:
        {
            if(reg < 13) return r[reg];
            else
            {
                switch(cpsr.mode)
                {
                    case arm_mode::mode_irq: return r_irq[reg - 13];
                    case arm_mode::mode_supervisor: return r_svc[reg - 13];
                    case arm_mode::mode_abort: return r_abt[reg - 13];
                    case arm_mode::mode_undefined: return r_und[reg - 13];
                }
            }
        }
    }
    return 0;
}

void arm_cpu::set_register(int reg, u32 data)
{
    if((reg < 8) || (reg == 15)) r[reg] = data;
    else switch(cpsr.mode)
    {
        case arm_mode::mode_user:
        case arm_mode::mode_system: r[reg] = data; break;
        case arm_mode::mode_fiq: r_fiq[reg - 8] = data; break;
        default:
        {
            if(reg < 13) r[reg] = data;
            else
            {
                switch(cpsr.mode)
                {
                    case arm_mode::mode_irq: r_irq[reg - 13] = data; break;
                    case arm_mode::mode_supervisor: r_svc[reg - 13] = data; break;
                    case arm_mode::mode_abort: r_abt[reg - 13] = data; break;
                    case arm_mode::mode_undefined: r_und[reg - 13] = data; break;
                }
            }
        }
    }
}

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
        
        set_register(14, r[15] + 8);
        r[15] = 0x10;
        cpsr.thumb = false;
        cpsr.abort_disable = true;
        abort_enable = false;
    }
    else if(fiq && !cpsr.fiq_disable && fiq_enable)
    {
        arm_mode old_mode = cpsr.mode;

        cpsr.mode = mode_fiq;

        set_register(14, r[15] + 4);
        r[15] = 0x1c;
        cpsr.fiq_disable = true;
        fiq_enable = false;
        printf("ARM11 got FIQ!\n");
    }
    else if(irq && !cpsr.irq_disable && irq_enable)
    {
        arm_mode old_mode = cpsr.mode;

        cpsr.mode = mode_irq;

        set_register(14, r[15] + 4);
        r[15] = 0x18;
        cpsr.irq_disable = true;
        irq_enable = false;
        printf("ARM11 got IRQ!\n");
    }

    if(undefined && undefined_enable)
    {
        arm_mode old_mode = cpsr.mode;

        cpsr.mode = mode_undefined;

        set_register(14, r[15] + 4);
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

        bool condition = true;
        switch((opcode >> 28) & 0xf)
        {
            case 0x0: condition = cpsr.zero; break;
            case 0x1: condition = !cpsr.zero; break;
            case 0x2: condition = cpsr.carry; break;
            case 0x3: condition = !cpsr.carry; break;
            case 0x4: condition = cpsr.sign; break;
            case 0x5: condition = !cpsr.sign; break;
            case 0x6: condition = cpsr.overflow; break;
            case 0x7: condition = !cpsr.overflow; break;
            case 0x8: condition = cpsr.carry && !cpsr.zero; break;
            case 0x9: condition = !cpsr.carry || cpsr.zero; break;
            case 0xa: condition = cpsr.sign == cpsr.overflow; break;
            case 0xb: condition = cpsr.sign != cpsr.overflow; break;
            case 0xc: condition = !cpsr.zero && (cpsr.sign == cpsr.overflow); break;
            case 0xd: condition = cpsr.zero || (cpsr.sign != cpsr.overflow); break;
            //case 0xe: condition = true; break;
            case 0xf: condition = false; break;
        }

        switch((opcode >> 25) & 7)
        {
            case 0x5:
            {
                printf("[ARM] B\n");
                if(condition)
                {
                    bool link = (opcode >> 24) & 1;
                    u32 offset = (opcode & 0xffffff) << 2;
                    
                    if(offset & 0x2000000) offset |= 0xfc000000;
                    if(link) set_register(14, r[15] - 4);
                    
                    r[15] += offset;
                    just_branched = true;
                }
                break;
            }
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