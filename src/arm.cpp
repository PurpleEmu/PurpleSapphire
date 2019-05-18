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

arm_cpu::arm_psr arm_cpu::get_spsr()
{
    switch(cpsr.mode)
    {
        case arm_mode::mode_fiq: return spsr_fiq;
        case arm_mode::mode_irq: return spsr_irq;
        case arm_mode::mode_supervisor: return spsr_svc;
        case arm_mode::mode_abort: return spsr_abt;
    }
    return cpsr;
}

void arm_cpu::set_spsr(u32 data)
{
    switch(cpsr.mode)
    {
        case arm_mode::mode_fiq: spsr_fiq.whole = data; break;
        case arm_mode::mode_irq: spsr_irq.whole = data; break;
        case arm_mode::mode_supervisor: spsr_svc.whole = data; break;
        case arm_mode::mode_abort: spsr_abt.whole = data; break;
    }
}

u32 arm_cpu::get_load_store_addr()
{
    int rn = (opcode >> 16) & 0xf;
    int w = (opcode >> 21) & 1;
    int u = (opcode >> 23) & 1;
    int p = (opcode >> 24) & 1;
    int i = (opcode >> 25) & 1;

    arm_mode old_mode = cpsr.mode;
    if(!p && w) cpsr.mode = mode_user;
    u32 offset = 0;
    if(i)
    {
        int rm = opcode & 0xf;
        int shift_mode = (opcode >> 5) & 3;
        int shift_imm = (opcode >> 7) & 0x1f;

        switch(shift_mode)
        {
            case 0:
            {
                //LSL
                offset = get_register(rm) << shift_imm;
                break;
            }
            case 1:
            {
                //LSR
                offset = shift_imm != 0 ? get_register(rm) >> shift_imm : 0;
                break;
            }
            case 2:
            {
                //ASR
                if(shift_imm == 0) offset = (get_register(rm) & 0x80000000) != 0 ? 0xffffffff : 0;
                else offset = get_register(rm) >> shift_imm;
                break;
            }
            case 3:
            {
                //ROR
                if(shift_imm == 0) offset = (get_register(rm) >> 1) | (cpsr.carry << 31);
                else offset = (get_register(rm) >> shift_imm) | (get_register(rm) << (32 - shift_imm));
                break;
            }
        }
    }
    else offset = opcode & 0xfff;

    u32 addr = 0;
    if(p)
    {
        if(w)
        {
            if(u) set_register(rn, get_register(rn) + offset);
            else set_register(rn, get_register(rn) - offset);
            addr = get_register(rn);
        }
        else addr = u ? get_register(rn) + offset : get_register(rn) - offset;
    }
    else
    {
        if(!w)
        {
            addr = get_register(rn);
            if(u) set_register(rn, get_register(rn) + offset);
            else set_register(rn, get_register(rn) - offset);
        }
    }

    cpsr.mode = old_mode;
    return addr;
}

u32 arm_cpu::get_shifter_operand(int s)
{
    int i = (opcode >> 25) & 1;
    u32 shifter_operand = opcode & 0xfff;

    if(i)
    {
        int rotate = (shifter_operand >> 8) << 1;
        u32 operand = shifter_operand & 0xff;
        operand = (operand >> rotate) | (operand << (32 - rotate));
        if(s && rotate) cpsr.carry = (operand & 0x80000000) != 0;
        return operand;
    }
    else
    {
        u32 operand = 0;
        bool carry = false;
        int rm = opcode & 0xf;
        int shift_mode = (opcode >> 5) & 3;
        int shift_imm = 0;
        bool reg = (opcode >> 4) & 1;

        if(reg)
        {
            int rs = (opcode >> 8) & 0xf;
            shift_imm = get_register(rs) & 0xff;
        }
        else
        {
            shift_imm = (opcode >> 7) & 0x1f;
            if(shift_imm == 0 && (shift_mode == 1 || shift_mode == 2)) shift_imm = 32;
        }

        if((shift_imm != 0) && (shift_imm <= 32))
        {
            switch(shift_mode)
            {
                case 0:
                {
                    //LSL
                    operand = shift_imm < 32 ? get_register(rm) << shift_imm : 0;
                    carry = get_register(rm) & (1 << (32 - shift_imm)) != 0;
                    break;
                }
                case 1:
                {
                    //LSR
                    operand = shift_imm < 32 ? get_register(rm) >> shift_imm : 0;
                    carry = (get_register(rm) & (1 << (shift_imm - 1))) != 0;
                    break;
                }
                case 2:
                {
                    //ASR
                    s32 rm_val = get_register(rm);
                    operand = rm_val >> shift_imm;
                    carry = (rm_val & (1 << (shift_imm - 1))) != 0;
                    break;
                }
                case 3:
                {
                    //ROR
                    if(reg) shift_imm &= 0x1f;
                    operand = (get_register(rm) >> shift_imm) | (get_register(rm) << (32 - shift_imm));
                    if(shift_imm != 0) carry = get_register(rm) & (1 << (shift_imm - 1)) != 0;
                    else carry = (get_register(rm) & 0x80000000) != 0;
                    break;
                }
            }
        }
        else
        {
            if(shift_imm == 0)
            {
                operand = get_register(rm);
                carry = cpsr.carry;

                if(!reg && (shift_mode == 3))
                {
                    //RRX
                    operand = (operand >> 1) | (carry ? 0x80000000 : 0);
                    carry = get_register(rm) & 1;
                }
            }
            else if(shift_mode > 1)
            {
                carry = get_register(rm) & 0x80000000;
                operand = carry ? 0xffffffff : 0;
            }
        }

        if(s) cpsr.carry = carry;
        return operand;
    }
}

void arm_cpu::tick_media()
{
}

void arm_cpu::tick_dsp()
{
    if((opcode >> 25) & 1)
    {
        printf("[ARM] MSR\n");
        u32 mask = (opcode >> 16) & 0xf;
        int r = (opcode >> 22) & 1;
        
        u32 byte_mask = 0;
        if(mask & 1) byte_mask |= 0xff;
        if(mask & 2) byte_mask |= 0xff00;
        if(mask & 4) byte_mask |= 0xff0000;
        if(mask & 8) byte_mask |= 0xff000000;

        u32 imm = opcode & 0xff;
        int rotate = (opcode >> 7) & 0x1e;
        u32 value = (imm >> rotate) | (imm << (32 - rotate));

        if(r)
        {
            mask = byte_mask & 0xF90F03FF; //User, Privileged, State bits
            set_spsr((get_spsr().whole & ~mask) | (value & mask));
        }
        else
        {
            mask = byte_mask & (cpsr.mode == arm_mode::mode_user ? 0xf80f0200 : 0xf80f03df);
            cpsr.whole = (cpsr.whole & ~mask) | (value & mask);
        }
    }
    else
    {
        if((opcode >> 7) & 1)
        {
            printf("[ARM] signed multiplies\n");
        }
        else
        {
            switch((opcode >> 4) & 7)
            {
                case 0:
                {
                    if((opcode >> 21) & 1)
                    {
                        printf("[ARM] MSR\n");
                        int rm = opcode & 0xf;
                        u32 mask = (opcode >> 16) & 0xf;
                        int r = (opcode >> 22) & 1;
        
                        u32 byte_mask = 0;
                        if(mask & 1) byte_mask |= 0xff;
                        if(mask & 2) byte_mask |= 0xff00;
                        if(mask & 4) byte_mask |= 0xff0000;
                        if(mask & 8) byte_mask |= 0xff000000;

                        u32 value = get_register(rm);

                        if(r)
                        {
                            mask = byte_mask & 0xF90F03FF; //User, Privileged, State bits
                            set_spsr((get_spsr().whole & ~mask) | (value & mask));
                        }
                        else
                        {
                            mask = byte_mask & (cpsr.mode == arm_mode::mode_user ? 0xf80f0200 : 0xf80f03df);
                            cpsr.whole = (cpsr.whole & ~mask) | (value & mask);
                        }
                    }
                    else
                    {
                        printf("[ARM] MRS\n");
                    }
                    break;
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
            case 0x0:
            case 0x1:
            {
                if(((opcode >> 4) & 1) && ((opcode >> 7) & 1)
                && !((opcode >> 25) & 1))
                {
                    if(((opcode >> 5) & 3) || ((opcode >> 24) & 1))
                    {
                        printf("[ARM] loadstore ext\n");
                    }
                    else
                    {
                        printf("[ARM] multiply ext\n");
                    }
                }
                else
                {
                    if((((opcode >> 23) & 3) == 2) && !((opcode >> 20) & 1))
                    {
                        tick_dsp();
                    }
                    else switch((opcode >> 21) & 0xf)
                    {
                        case 0x0:
                        {
                            printf("[ARM] AND\n");
                            int rd = (opcode >> 12) & 0xf;
                            int rn = (opcode >> 16) & 0xf;
                            int s = (opcode >> 20) & 1;
                            u32 shifter_operand = get_shifter_operand(s && (rd != 15));

                            set_register(rd, get_register(rn) & shifter_operand);

                            if(s)
                            {
                                if(rd != 15)
                                {
                                    cpsr.zero = get_register(rd) == 0;
                                    cpsr.sign = get_register(rd) & 0x80000000;
                                }
                                else cpsr.whole = get_spsr().whole;
                            }
                            break;
                        }
                        case 0x1:
                        {
                            printf("[ARM] EOR\n");
                            int rd = (opcode >> 12) & 0xf;
                            int rn = (opcode >> 16) & 0xf;
                            int s = (opcode >> 20) & 1;
                            u32 shifter_operand = get_shifter_operand(s && (rd != 15));

                            set_register(rd, get_register(rn) ^ shifter_operand);

                            if(s)
                            {
                                if(rd != 15)
                                {
                                    cpsr.zero = get_register(rd) == 0;
                                    cpsr.sign = get_register(rd) & 0x80000000;
                                }
                                else cpsr.whole = get_spsr().whole;
                            }
                            break;
                        }
                        case 0x8:
                        {
                            printf("[ARM] TST\n");
                            int rn = (opcode >> 16) & 0xf;
                            u32 shifter_operand = get_shifter_operand(true);

                            u32 result = get_register(rn) & shifter_operand;

                            cpsr.zero = result == 0;
                            cpsr.sign = result & 0x80000000;
                            break;
                        }
                        case 0x9:
                        {
                            printf("[ARM] TEQ\n");
                            int rn = (opcode >> 16) & 0xf;
                            u32 shifter_operand = get_shifter_operand(true);

                            u32 result = get_register(rn) ^ shifter_operand;

                            cpsr.zero = result == 0;
                            cpsr.sign = result & 0x80000000;
                            break;
                        }
                        case 0xa:
                        {
                            printf("[ARM] CMP\n");
                            int rn = (opcode >> 16) & 0xf;
                            u32 shifter_operand = get_shifter_operand(true);

                            u32 rn_val = get_register(rn);
                            u64 result64 = rn_val - shifter_operand;
                            u32 result = (u32)result64;

                            cpsr.carry = result64 < 0x100000000;
                            cpsr.overflow = (rn_val ^ shifter_operand) & (rn_val ^ result) & 0x80000000;
                            cpsr.zero = result == 0;
                            cpsr.sign = result & 0x80000000;
                            break;
                        }
                        case 0xb:
                        {
                            printf("[ARM] CMN\n");
                            int rn = (opcode >> 16) & 0xf;
                            u32 shifter_operand = get_shifter_operand(true);

                            u32 rn_val = get_register(rn);
                            u64 result64 = rn_val + shifter_operand;
                            u32 result = (u32)result64;

                            cpsr.carry = result64 > 0xffffffff;
                            cpsr.overflow = ~(rn_val ^ shifter_operand) & (rn_val ^ result) & 0x80000000;
                            cpsr.zero = result == 0;
                            cpsr.sign = result & 0x80000000;
                            break;
                        }
                        case 0xc:
                        {
                            printf("[ARM] ORR\n");
                            int rd = (opcode >> 12) & 0xf;
                            int rn = (opcode >> 16) & 0xf;
                            int s = (opcode >> 20) & 1;
                            u32 shifter_operand = get_shifter_operand(s && (rd != 15));

                            set_register(rd, get_register(rn) | shifter_operand);

                            if(s)
                            {
                                if(rd != 15)
                                {
                                    cpsr.zero = get_register(rd) == 0;
                                    cpsr.sign = get_register(rd) & 0x80000000;
                                }
                                else cpsr.whole = get_spsr().whole;
                            }
                            break;
                        }
                        case 0xd:
                        {
                            printf("[ARM] MOV\n");
                            int rd = (opcode >> 12) & 0xf;
                            int s = (opcode >> 20) & 1;
                            u32 shifter_operand = get_shifter_operand(s && (rd != 15));

                            set_register(rd, shifter_operand);

                            if(s)
                            {
                                if(rd != 15)
                                {
                                    cpsr.zero = get_register(rd) == 0;
                                    cpsr.sign = get_register(rd) & 0x80000000;
                                }
                                else cpsr.whole = get_spsr().whole;
                            }
                            break;
                        }
                        case 0xe:
                        {
                            printf("[ARM] BIC\n");
                            int rd = (opcode >> 12) & 0xf;
                            int rn = (opcode >> 16) & 0xf;
                            int s = (opcode >> 20) & 1;
                            u32 shifter_operand = get_shifter_operand(s && (rd != 15));

                            set_register(rd, get_register(rn) & ~shifter_operand);

                            if(s)
                            {
                                if(rd != 15)
                                {
                                    cpsr.zero = get_register(rd) == 0;
                                    cpsr.sign = get_register(rd) & 0x80000000;
                                }
                                else cpsr.whole = get_spsr().whole;
                            }
                            break;
                        }
                        case 0xf:
                        {
                            printf("[ARM] MVN\n");
                            int rd = (opcode >> 12) & 0xf;
                            int s = (opcode >> 20) & 1;
                            u32 shifter_operand = get_shifter_operand(s && (rd != 15));

                            set_register(rd, ~shifter_operand);

                            if(s)
                            {
                                if(rd != 15)
                                {
                                    cpsr.zero = get_register(rd) == 0;
                                    cpsr.sign = get_register(rd) & 0x80000000;
                                }
                                else cpsr.whole = get_spsr().whole;
                            }
                            break;
                        }
                    }
                }

                break;
            }
            case 0x2:
            case 0x3:
            {
                if(((opcode >> 4) & 1) && ((opcode >> 25) & 1)) tick_media();
                else
                {
                    if((opcode >> 20) & 1)
                    {
                        if((opcode >> 22) & 1)
                        {
                            printf("[ARM] LDRB\n");
                        }
                        else
                        {
                            printf("[ARM] LDR\n");
                            int rd = (opcode >> 12) & 0xf;
                            u32 addr = get_load_store_addr();
                            u32 value = rw(addr & 0xfffffffc);
                            value = (value >> ((addr & 3) << 3)) | (value << (32 - ((addr & 3) << 3)));
                            if(rd == 15)
                            {
                                r[15] = value & 0xfffffffe;
                                cpsr.thumb = value & 1;
                                just_branched = true;
                            }
                            else set_register(rd, value);
                        }
                    }
                    else
                    {
                        if((opcode >> 22) & 1)
                        {
                            printf("[ARM] STRB\n");
                        }
                        else
                        {
                            printf("[ARM] STR\n");
                            int rd = (opcode >> 12) & 0xf;
                            u32 addr = get_load_store_addr();
                            ww(addr, get_register(rd));
                        }
                    }
                }
                break;
            }
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