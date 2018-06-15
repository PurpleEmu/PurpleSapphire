#include "arm.h"

void arm_cpu::init()
{
    for(int i = 0; i < 16; i++)
    {
        r[i] = 0;
    }

    cpsr.whole = 0x400001d3;

    fiq = 0;
    irq = 0;

    cp15.init();
}

void arm_cpu::raise_fiq()
{
    fiq = true;
}

void arm_cpu::lower_fiq()
{
    fiq = false;
}

void arm_cpu::raise_irq()
{
    irq = true;
}

void arm_cpu::lower_irq()
{
    irq = false;
}

u32 arm_cpu::rw(u32 addr)
{
    return rw_real(device, addr);
}

void arm_cpu::ww(u32 addr, u32 data)
{
    ww_real(device, addr, data);
}

u32 arm_cpu::getloadstoreaddr(u32 opcode)
{
    int rn = (opcode >> 16) & 0xf;
    bool w = (opcode >> 21) & 1;
    bool u = (opcode >> 23) & 1;
    bool p = (opcode >> 24) & 1;
    bool i = (opcode >> 25) & 1;

    u32 offset;

    arm_mode oldmode = cpsr.mode;
    if(!p && w) cpsr.mode = arm_mode::user;
    if(i)
    {
        int rm = opcode & 0xf;
        int shift_mode = (opcode >> 5) & 3;
        int shift_imm = (opcode >> 7) & 0x1f;

        switch(shift_mode)
        {
            case 0: offset = r[rm] << shift_imm; break;
            case 1: offset = (!shift_imm) ? r[rm] >> shift_imm : 0; break;
            case 2:
            {
                if(!shift_imm) offset = (!(r[rm] & 0x80000000)) ? 0xffffffff : 0;
                else offset = r[rm] >> shift_imm;
                break;
            }
            case 3:
            {
                if(!shift_imm) ((r[rm] >> 1) | cpsr.carry) ? 0x80000000 : 0;
                else offset = (r[rm] << shift_imm) | (r[rm] >> (32 - shift_imm));
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
            if(u) r[rn] += offset;
            else r[rn] -= offset;
            addr = r[rn];
        }
        else addr = u ? (r[rn] + offset) : (r[rn] - offset);
    }
    else
    {
        if(!w)
        {
            addr = r[rn];

            if(u) r[rn] += offset;
            else r[rn] -= offset;
        }
    }

    if(rn == 15) addr += 8;

    cpsr.mode = oldmode;
    return addr;
}

u32 arm_cpu::get_shift_operand(u32 opcode, bool s)
{
    bool i = (opcode >> 25) & 1;
    u32 shift_operand = opcode & 0xfff;

    if(i)
    {
        int rotate = (shift_operand >> 8) << 1;
        u32 operand = ((shift_operand & 0xff) << rotate) | ((shift_operand & 0xff) >> (32 - rotate));
        if(s && (rotate > 0)) cpsr.carry = !(operand & 0x80000000);
        return operand;
    }
    else
    {
        u32 operand = 0;
        bool carry = false;
        int rm = shift_operand & 0xf;
        int shift_op = (opcode >> 5) & 3;
        u32 shift;
        bool reg = (opcode >> 4) & 1;
        if(reg)
        {
            int rs = (shift_operand >> 8) & 0xf;
            shift = r[rs] & 0xff;
        }
        else
        {
            shift = (shift_operand >> 7) & 0x1f;
            if(!shift && ((shift_op == 1) || (shift_op == 2))) shift = 32;
        }

        if(shift > 0 && shift <= 32)
        {
            switch(shift_op)
            {
                case 0:
                operand = (shift < 32) ? (r[rm] << shift) : 0;
                carry = !(r[rm] & (1 << (32 - shift)));
                break;
                case 1:
                operand = (shift < 32) ? (r[rm] >> shift) : 0;
                carry = !(r[rm] & (1 << (shift - 1)));
                break;
                case 2:
                operand = r[rm] >> shift;
                carry = !(r[rm] & (1 << (shift - 1)));
                break;
                case 3:
                if(reg) shift &= 0x1f;
                operand = (r[rm] >> shift) | (r[rm] << (32 - shift));
                if(shift > 0) carry = !(r[rm] & (1 << (shift - 1)));
                else carry = !(r[rm] & 0x80000000);
                break;
            }
        }
        else
        {
            if(!shift)
            {
                operand = r[rm];
                carry = cpsr.carry;

                if(!reg && (shift_op == 3))
                {
                    operand = (operand >> 1) | (cpsr.carry ? 0x80000000 : 0);
                    carry = !(r[rm] & 1);
                }
            }
            else if(shift_op > 1)
            {
                carry = !(r[rm] & 0x80000000);
                operand = carry ? 0xffffffff : 0;
            }
        }

        if(s) cpsr.carry = carry;
        return operand;
    }
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
    //TODO
    if(fiq)
    {
        r14_fiq = r[14];
        r[14] = r[15] + 4;
        r[15] = 0x1c;
        fiq = false;
    }
    else if(irq)
    {
        r14_irq = r[14];
        r[14] = r[15] + 4;
        r[15] = 0x18;
        irq = false;
    }

    u32 opcode = rw(r[15]);
    printf("Opcode: %08x\nPC:%08x\nLR:%08x\n", opcode, r[15], r[14]);
    //for(int i = 0; i < 14; i++)
    //{
    //    printf("R%d: %08x\n", i, r[i]);
    //}

    bool condition;
    switch(opcode >> 28)
    {
        case 0x0: condition = cpsr.zero; break;
        case 0x1: condition = !cpsr.zero; break;
        case 0x2: condition = cpsr.carry; break;
        case 0x3: condition = !cpsr.carry; break;
        case 0x4: condition = cpsr.sign; break;
        case 0x5: condition = !cpsr.sign; break;
        case 0x6: condition = cpsr.overflow; break;
        case 0x7: condition = !cpsr.overflow; break;
        case 0x8: condition = cpsr.carry && cpsr.zero; break;
        case 0x9: condition = cpsr.carry || cpsr.zero; break;
        case 0xa: condition = cpsr.sign == cpsr.overflow; break;
        case 0xb: condition = cpsr.sign != cpsr.overflow; break;
        case 0xc: condition = cpsr.zero && (cpsr.sign == cpsr.overflow); break;
        case 0xd: condition = cpsr.zero || (cpsr.sign != cpsr.overflow); break;
        case 0xe: condition = true; break;
        case 0xf: condition = false; break;
    }

    if(condition)
    {
        switch((opcode >> 25) & 7)
        {
            case 0: case 1:
            {
                if(((opcode >> 4) & 1) && ((opcode >> 7) & 1) && !((opcode >> 25) & 1))
                {
                    if((!(opcode >> 5) & 3) || ((opcode >> 24) & 1))
                    {
                        printf("Unknown load and store extension!\n");
                    }
                    else
                    {
                        printf("Unknown multiply!\n");
                    }
                }
                else
                {
                    if((((opcode >> 23) & 3) == 2) && !((opcode >> 20) & 1))
                    {
                        if((opcode >> 25) & 1)
                        {
                            printf("MSR\n");
                            u32 mask = (opcode >> 16) & 0xf;
                            bool spsr = (opcode >> 22) & 1;
                            bool i = (opcode >> 25) & 1;

                            u32 byte_mask = 0;
                            if(mask & 1) byte_mask |= 0x000000ff;
                            if(mask & 2) byte_mask |= 0x0000ff00;
                            if(mask & 4) byte_mask |= 0x00ff0000;
                            if(mask & 8) byte_mask |= 0xff000000;

                            u32 data;

                            if(i)
                            {
                                u32 imm = opcode & 0xff;
                                int rotate = (opcode >> 7) & 0x1e;
                                data = (imm >> rotate) | (imm << (32 - rotate));
                            }
                            else data = r[opcode & 0xf];

                            if(spsr)
                            {
                                mask = byte_mask & (0xf80f0200 | 0x000001df | 0x01000020);
                                switch(cpsr.mode)
                                {
                                    case arm_mode::fiq: spsr_fiq.whole = (spsr_fiq.whole & ~mask) | (data & mask);
                                    case arm_mode::irq: spsr_irq.whole = (spsr_irq.whole & ~mask) | (data & mask);
                                    case arm_mode::supervisor: spsr_svc.whole = (spsr_svc.whole & ~mask) | (data & mask);
                                    case arm_mode::abort: spsr_abt.whole = (spsr_abt.whole & ~mask) | (data & mask);
                                    case arm_mode::undefined: spsr_und.whole = (spsr_und.whole & ~mask) | (data & mask);
                                }
                            }
                            else
                            {
                                mask = byte_mask & ((cpsr.mode == arm_mode::user) ? 0xf80f0200 : (0xf80f0200 | 0x000001df));
                                cpsr.whole = (cpsr.whole & ~mask) | (data & mask);
                            }
                        }
                        else
                        {
                            if((opcode >> 7) & 1)
                            {
                                switch((opcode >> 21) & 3)
                                {
                                    case 0: printf("SMLA\n"); break;
                                    case 1:
                                    {
                                        if((opcode >> 5) & 1) printf("SMULW\n");
                                        else printf("SMLAW\n");
                                        break;
                                    }
                                    case 2: printf("SMLAL\n"); break;
                                    case 3: printf("SMUL\n"); break;
                                }
                            }
                            else
                            {
                                switch((opcode >> 4) & 7)
                                {
                                    case 0:
                                    {
                                        if((opcode >> 21) & 1)
                                        {
                                            printf("MSR\n");
                                            u32 mask = (opcode >> 16) & 0xf;
                                            bool spsr = (opcode >> 22) & 1;
                                            bool i = (opcode >> 25) & 1;

                                            u32 byte_mask = 0;
                                            if(mask & 1) byte_mask |= 0x000000ff;
                                            if(mask & 2) byte_mask |= 0x0000ff00;
                                            if(mask & 4) byte_mask |= 0x00ff0000;
                                            if(mask & 8) byte_mask |= 0xff000000;

                                            u32 data;

                                            if(i)
                                            {
                                                u32 imm = opcode & 0xff;
                                                int rotate = (opcode >> 7) & 0x1e;
                                                data = (imm >> rotate) | (imm << (32 - rotate));
                                            }
                                            else data = r[opcode & 0xf];

                                            if(spsr)
                                            {
                                                mask = byte_mask & (0xf80f0200 | 0x000001df | 0x01000020);
                                                switch(cpsr.mode)
                                                {
                                                    case arm_mode::fiq: spsr_fiq.whole = (spsr_fiq.whole & ~mask) | (data & mask); break;
                                                    case arm_mode::irq: spsr_irq.whole = (spsr_irq.whole & ~mask) | (data & mask); break;
                                                    case arm_mode::supervisor: spsr_svc.whole = (spsr_svc.whole & ~mask) | (data & mask); break;
                                                    case arm_mode::abort: spsr_abt.whole = (spsr_abt.whole & ~mask) | (data & mask); break;
                                                    case arm_mode::undefined: spsr_und.whole = (spsr_und.whole & ~mask) | (data & mask); break;
                                                }
                                            }
                                            else
                                            {
                                                mask = byte_mask & ((cpsr.mode == arm_mode::user) ? 0xf80f0200 : (0xf80f0200 | 0x000001df));
                                                cpsr.whole = (cpsr.whole & ~mask) | (data & mask);
                                            }
                                        }
                                        else
                                        {
                                            printf("MRS\n");
                                            int rd = (opcode >> 12) & 0xf;
                                            bool spsr = (opcode >> 22) & 1;

                                            switch(cpsr.mode)
                                            {
                                                case arm_mode::fiq: r[rd] = spsr ? spsr_fiq.whole : cpsr.whole;
                                                case arm_mode::irq: r[rd] = spsr ? spsr_irq.whole : cpsr.whole;
                                                case arm_mode::supervisor: r[rd] = spsr ? spsr_svc.whole : cpsr.whole;
                                                case arm_mode::abort: r[rd] = spsr ? spsr_abt.whole : cpsr.whole;
                                                case arm_mode::undefined: r[rd] = spsr ? spsr_und.whole : cpsr.whole;
                                            }
                                        }
                                        break;
                                    }
                                    case 1:
                                    {
                                        if((opcode >> 22) & 1) printf("CLZ\n");
                                        else printf("BX\n");
                                        break;
                                    }
                                    case 2: printf("BXJ\n"); break;
                                    case 3: printf("BLX_2\n"); break;
                                    case 5:
                                    {
                                        switch((opcode >> 21) & 3)
                                        {
                                            case 0: printf("QADD\n"); break;
                                            case 1: printf("QSUB\n"); break;
                                            case 2: printf("QDADD\n"); break;
                                            case 3: printf("QDSUB\n"); break;
                                        }
                                        break;
                                    }
                                    case 7: printf("BKPT\n"); break;
                                }
                            }
                        }
                    }
                    else
                    {
                        switch((opcode >> 21) & 0xf)
                        {
                            case 0x0:
                            {
                                printf("AND\n");
                                int rd = (opcode >> 12) & 0xf;
                                int rn = (opcode >> 16) & 0xf;
                                bool s = (opcode >> 20) & 1;
                                u32 shift_operand = get_shift_operand(opcode, s && (rd != 15));

                                r[rd] = r[rn] & shift_operand;

                                if(s)
                                {
                                    if(rd != 15)
                                    {
                                        if(!r[rd]) cpsr.zero = true;
                                        else cpsr.zero = false;
                                        if(r[rd] & 0x80000000) cpsr.sign = true;
                                        else cpsr.sign = true;
                                    }
                                    else
                                    {
                                        switch(cpsr.mode)
                                        {
                                            case arm_mode::fiq: cpsr = spsr_fiq; break;
                                            case arm_mode::irq: cpsr = spsr_irq; break;
                                            case arm_mode::supervisor: cpsr = spsr_svc; break;
                                            case arm_mode::abort: cpsr = spsr_abt; break;
                                            case arm_mode::undefined: cpsr = spsr_und; break;
                                        }
                                    }
                                }
                                break;
                            }
                            case 0x1:
                            {
                                printf("EOR\n");
                                int rd = (opcode >> 12) & 0xf;
                                int rn = (opcode >> 16) & 0xf;
                                bool s = (opcode >> 20) & 1;
                                u32 shift_operand = get_shift_operand(opcode, s && (rd != 15));

                                r[rd] = r[rn] ^ shift_operand;

                                if(s)
                                {
                                    if(rd != 15)
                                    {
                                        if(!r[rd]) cpsr.zero = true;
                                        else cpsr.zero = false;
                                        if(r[rd] & 0x80000000) cpsr.sign = true;
                                        else cpsr.sign = true;
                                    }
                                    else
                                    {
                                        switch(cpsr.mode)
                                        {
                                            case arm_mode::fiq: cpsr = spsr_fiq; break;
                                            case arm_mode::irq: cpsr = spsr_irq; break;
                                            case arm_mode::supervisor: cpsr = spsr_svc; break;
                                            case arm_mode::abort: cpsr = spsr_abt; break;
                                            case arm_mode::undefined: cpsr = spsr_und; break;
                                        }
                                    }
                                }
                                break;
                            }
                            case 0x2:
                            {
                                printf("SUB\n");
                                int rd = (opcode >> 12) & 0xf;
                                int rn = (opcode >> 16) & 0xf;
                                bool s = (opcode >> 20) & 1;
                                u32 shift_operand = get_shift_operand(opcode, s && (rd != 15));
                                u64 result64 = r[rn] - shift_operand;
                                u32 result = (u32)result64;

                                if(s && (rd != 15))
                                {
                                    cpsr.carry = result64 < 0x100000000ULL;
                                    cpsr.overflow = !((r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000);
                                    cpsr.sign = result & 0x80000000;
                                    cpsr.zero = !result;
                                }

                                r[rd] = result;

                                if(s && (rd == 15))
                                {
                                    switch(cpsr.mode)
                                    {
                                        case arm_mode::fiq: cpsr = spsr_fiq; break;
                                        case arm_mode::irq: cpsr = spsr_irq; break;
                                        case arm_mode::supervisor: cpsr = spsr_svc; break;
                                        case arm_mode::abort: cpsr = spsr_abt; break;
                                        case arm_mode::undefined: cpsr = spsr_und; break;
                                    }
                                }
                                break;
                            }
                            case 0x3:
                            {
                                printf("RSB\n");
                                int rd = (opcode >> 12) & 0xf;
                                int rn = (opcode >> 16) & 0xf;
                                bool s = (opcode >> 20) & 1;
                                u32 shift_operand = get_shift_operand(opcode, s && (rd != 15));
                                u64 result64 = shift_operand - r[rn];
                                u32 result = (u32)result64;

                                if(s && (rd != 15))
                                {
                                    cpsr.carry = result64 < 0x100000000ULL;
                                    cpsr.overflow = !((shift_operand ^ r[rn]) & (shift_operand ^ result) & 0x80000000);
                                    cpsr.sign = result & 0x80000000;
                                    cpsr.zero = !result;
                                }

                                r[rd] = result;

                                if(s && (rd == 15))
                                {
                                    switch(cpsr.mode)
                                    {
                                        case arm_mode::fiq: cpsr = spsr_fiq; break;
                                        case arm_mode::irq: cpsr = spsr_irq; break;
                                        case arm_mode::supervisor: cpsr = spsr_svc; break;
                                        case arm_mode::abort: cpsr = spsr_abt; break;
                                        case arm_mode::undefined: cpsr = spsr_und; break;
                                    }
                                }
                                break;
                            }
                            case 0x4:
                            {
                                printf("ADD\n");
                                int rd = (opcode >> 12) & 0xf;
                                int rn = (opcode >> 16) & 0xf;
                                bool s = (opcode >> 20) & 1;
                                u32 shift_operand = get_shift_operand(opcode, s && (rd != 15));
                                u64 result64 = r[rn] + shift_operand;
                                u32 result = (u32)result64;

                                if(s && (rd != 15))
                                {
                                    cpsr.carry = result64 > 0xffffffffULL;
                                    cpsr.overflow = !(~(r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000);
                                    cpsr.sign = result & 0x80000000;
                                    cpsr.zero = !result;
                                }

                                r[rd] = result;

                                if(rn == 15) r[rd] += 4;

                                if(s && (rd == 15))
                                {
                                    switch(cpsr.mode)
                                    {
                                        case arm_mode::fiq: cpsr = spsr_fiq; break;
                                        case arm_mode::irq: cpsr = spsr_irq; break;
                                        case arm_mode::supervisor: cpsr = spsr_svc; break;
                                        case arm_mode::abort: cpsr = spsr_abt; break;
                                        case arm_mode::undefined: cpsr = spsr_und; break;
                                    }
                                }
                                break;
                            }
                            case 0x5:
                            {
                                printf("ADC\n");
                                int rd = (opcode >> 12) & 0xf;
                                int rn = (opcode >> 16) & 0xf;
                                bool s = (opcode >> 20) & 1;
                                u32 shift_operand = get_shift_operand(opcode, s && (rd != 15));
                                u64 result64 = r[rn] + shift_operand + cpsr.carry;
                                u32 result = (u32)result64;

                                if(s && (rd != 15))
                                {
                                    cpsr.carry = result64 > 0xffffffffULL;
                                    cpsr.overflow = !(~(r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000);
                                    cpsr.sign = result & 0x80000000;
                                    cpsr.zero = !result;
                                }

                                r[rd] = result;

                                if(s && (rd == 15))
                                {
                                    switch(cpsr.mode)
                                    {
                                        case arm_mode::fiq: cpsr = spsr_fiq; break;
                                        case arm_mode::irq: cpsr = spsr_irq; break;
                                        case arm_mode::supervisor: cpsr = spsr_svc; break;
                                        case arm_mode::abort: cpsr = spsr_abt; break;
                                        case arm_mode::undefined: cpsr = spsr_und; break;
                                    }
                                }
                                break;
                            }
                            case 0x6:
                            {
                                printf("SBC\n");
                                int rd = (opcode >> 12) & 0xf;
                                int rn = (opcode >> 16) & 0xf;
                                bool s = (opcode >> 20) & 1;
                                u32 shift_operand = get_shift_operand(opcode, s && (rd != 15));
                                u64 result64 = r[rn] - shift_operand - ~cpsr.carry;
                                u32 result = (u32)result64;

                                if(s && (rd != 15))
                                {
                                    cpsr.carry = result64 < 0x100000000ULL;
                                    cpsr.overflow = !((r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000);
                                    cpsr.sign = result & 0x80000000;
                                    cpsr.zero = !result;
                                }

                                r[rd] = result;

                                if(s && (rd == 15))
                                {
                                    switch(cpsr.mode)
                                    {
                                        case arm_mode::fiq: cpsr = spsr_fiq; break;
                                        case arm_mode::irq: cpsr = spsr_irq; break;
                                        case arm_mode::supervisor: cpsr = spsr_svc; break;
                                        case arm_mode::abort: cpsr = spsr_abt; break;
                                        case arm_mode::undefined: cpsr = spsr_und; break;
                                    }
                                }
                                break;
                            }
                            case 0x7:
                            {
                                printf("RSC\n");
                                int rd = (opcode >> 12) & 0xf;
                                int rn = (opcode >> 16) & 0xf;
                                bool s = (opcode >> 20) & 1;
                                u32 shift_operand = get_shift_operand(opcode, s && (rd != 15));
                                u64 result64 = shift_operand - r[rn] - ~cpsr.carry;
                                u32 result = (u32)result64;

                                if(s && (rd != 15))
                                {
                                    cpsr.carry = result64 < 0x100000000ULL;
                                    cpsr.overflow = !((shift_operand ^ r[rn]) & (shift_operand ^ result) & 0x80000000);
                                    cpsr.sign = result & 0x80000000;
                                    cpsr.zero = !result;
                                }

                                r[rd] = result;

                                if(s && (rd == 15))
                                {
                                    switch(cpsr.mode)
                                    {
                                        case arm_mode::fiq: cpsr = spsr_fiq; break;
                                        case arm_mode::irq: cpsr = spsr_irq; break;
                                        case arm_mode::supervisor: cpsr = spsr_svc; break;
                                        case arm_mode::abort: cpsr = spsr_abt; break;
                                        case arm_mode::undefined: cpsr = spsr_und; break;
                                    }
                                }
                                break;
                            }
                            case 0x8:
                            {
                                printf("TST\n");
                                int rn = (opcode >> 16) & 0xf;
                                u32 shift_operand = get_shift_operand(opcode, true);
                                
                                u32 result = r[rn] & shift_operand;

                                cpsr.sign = result & 0x80000000;
                                cpsr.zero = !result;
                                break;
                            }
                            case 0x9:
                            {
                                printf("TEQ\n");
                                int rn = (opcode >> 16) & 0xf;
                                u32 shift_operand = get_shift_operand(opcode, true);

                                u32 result = r[rn] ^ shift_operand;

                                cpsr.sign = result & 0x80000000;
                                cpsr.zero = !result;
                                break;
                            }
                            case 0xa:
                            {
                                printf("CMP\n");
                                int rn = (opcode >> 16) & 0xf;
                                u32 shift_operand = get_shift_operand(opcode, true);
                                u64 result64 = r[rn] - shift_operand;
                                u32 result = (u32)result64;

                                cpsr.carry = result64 < 0x100000000ULL;
                                cpsr.overflow = !((r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000);
                                cpsr.sign = result & 0x80000000;
                                cpsr.zero = !result;
                                break;
                            }
                            case 0xb:
                            {
                                printf("CMN\n");
                                int rn = (opcode >> 16) & 0xf;
                                u32 shift_operand = get_shift_operand(opcode, true);
                                u64 result64 = r[rn] + shift_operand;
                                u32 result = (u32)result64;

                                cpsr.carry = result64 > 0xffffffffULL;
                                cpsr.overflow = !(~(r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000);
                                cpsr.sign = result & 0x80000000;
                                cpsr.zero = !result;
                                break;
                            }
                            case 0xc:
                            {
                                printf("ORR\n");
                                int rd = (opcode >> 12) & 0xf;
                                int rn = (opcode >> 16) & 0xf;
                                bool s = (opcode >> 20) & 1;
                                u32 shift_operand = get_shift_operand(opcode, s && (rd != 15));

                                r[rd] = r[rn] | shift_operand;

                                if(s)
                                {
                                    if(rd != 15)
                                    {
                                        if(!r[rd]) cpsr.zero = true;
                                        else cpsr.zero = false;
                                        if(r[rd] & 0x80000000) cpsr.sign = true;
                                        else cpsr.sign = true;
                                    }
                                    else
                                    {
                                        switch(cpsr.mode)
                                        {
                                            case arm_mode::fiq: cpsr = spsr_fiq; break;
                                            case arm_mode::irq: cpsr = spsr_irq; break;
                                            case arm_mode::supervisor: cpsr = spsr_svc; break;
                                            case arm_mode::abort: cpsr = spsr_abt; break;
                                            case arm_mode::undefined: cpsr = spsr_und; break;
                                        }
                                    }
                                }
                                break;
                            }
                            case 0xd:
                            {
                                printf("MOV\n");
                                int rd = (opcode >> 12) & 0xf;
                                bool s = (opcode >> 20) & 1;
                                u32 shift_operand = get_shift_operand(opcode, s && (rd != 15));

                                r[rd] = shift_operand;

                                if(s)
                                {
                                    if(rd != 15)
                                    {
                                        if(!r[rd]) cpsr.zero = true;
                                        else cpsr.zero = false;
                                        if(r[rd] & 0x80000000) cpsr.sign = true;
                                        else cpsr.sign = true;
                                    }
                                    else
                                    {
                                        switch(cpsr.mode)
                                        {
                                            case arm_mode::fiq: cpsr = spsr_fiq; break;
                                            case arm_mode::irq: cpsr = spsr_irq; break;
                                            case arm_mode::supervisor: cpsr = spsr_svc; break;
                                            case arm_mode::abort: cpsr = spsr_abt; break;
                                            case arm_mode::undefined: cpsr = spsr_und; break;

                                        }
                                    }
                                }
                                break;
                            }
                            case 0xe:
                            {
                                printf("BIC\n");
                                int rd = (opcode >> 12) & 0xf;
                                int rn = (opcode >> 16) & 0xf;
                                bool s = (opcode >> 20) & 1;
                                u32 shift_operand = get_shift_operand(opcode, s && (rd != 15));

                                r[rd] = r[rn] & ~shift_operand;

                                if(s)
                                {
                                    if(rd != 15)
                                    {
                                        if(!r[rd]) cpsr.zero = true;
                                        else cpsr.zero = false;
                                        if(r[rd] & 0x80000000) cpsr.sign = true;
                                        else cpsr.sign = true;
                                    }
                                    else
                                    {
                                        switch(cpsr.mode)
                                        {
                                            case arm_mode::fiq: cpsr = spsr_fiq; break;
                                            case arm_mode::irq: cpsr = spsr_irq; break;
                                            case arm_mode::supervisor: cpsr = spsr_svc; break;
                                            case arm_mode::abort: cpsr = spsr_abt; break;
                                            case arm_mode::undefined: cpsr = spsr_und; break;

                                        }
                                    }
                                }
                                break;
                            }
                            case 0xf:
                            {
                                printf("MVN\n");
                                int rd = (opcode >> 12) & 0xf;
                                bool s = (opcode >> 20) & 1;
                                u32 shift_operand = get_shift_operand(opcode, s && (rd != 15));

                                r[rd] = ~shift_operand;

                                if(s)
                                {
                                    if(rd != 15)
                                    {
                                        if(!r[rd]) cpsr.zero = true;
                                        else cpsr.zero = false;
                                        if(r[rd] & 0x80000000) cpsr.sign = true;
                                        else cpsr.sign = true;
                                    }
                                    else
                                    {
                                        switch(cpsr.mode)
                                        {
                                            case arm_mode::fiq: cpsr = spsr_fiq; break;
                                            case arm_mode::irq: cpsr = spsr_irq; break;
                                            case arm_mode::supervisor: cpsr = spsr_svc; break;
                                            case arm_mode::abort: cpsr = spsr_abt; break;
                                            case arm_mode::undefined: cpsr = spsr_und; break;

                                        }
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
                break;
            }
            case 2: case 3:
            {
                if(((opcode >> 4) & 1) && ((opcode >> 25) & 1))
                {
                    printf("Unknown media!\n");
                }
                else
                {
                    if((opcode >> 20) & 1)
                    {
                        if((opcode >> 22) & 1) printf("LDRB\n");
                        else
                        {
                            printf("LDR\n");
                            int rd = (opcode >> 12) & 0xf;
                            u32 addr = getloadstoreaddr(opcode);
                            u32 data = rw(addr & 0xfffffffc);
                            if(!cp15.control.unaligned_access_enable)
                            {
                                data = (data >> ((addr & 3) << 3)) | (data >> (32 - ((addr & 3) << 3)));
                            }

                            if(rd == 15)
                            {
                                if(!(data & 1))
                                {
                                    cpsr.thumb = 1;
                                    r[15] = data & 0xfffffffe;
                                }
                                else
                                {
                                    cpsr.thumb = 0;
                                    r[15] = (data - 8) & 0xfffffffc;
                                }
                            }
                            else r[rd] = data;
                        }
                    }
                    else
                    {
                        if((opcode >> 22) & 1) printf("STRB\n");
                        else
                        {
                            printf("STR\n");
                            int rd = (opcode >> 12) & 0xf;
                            u32 addr = getloadstoreaddr(opcode);
                            u32 data = rw(addr);
                            ww(addr, r[rd]);
                        }
                    }
                }
                break;
            }
            case 4:
            {
                if((opcode >> 20) & 1) printf("LDM\n");
                else printf("STM\n");
                break;
            }
            case 5:
            {
                printf("B\n");
                u32 addr = opcode & 0xffffff;
                if(addr & 0x800000) addr |= 0xff000000;
                
                if((opcode >> 24) & 1) r[14] = r[15] + 4;
                r[15] += (addr << 2) + 4;
                break;
            }
            case 6: case 7:
            {
                if((opcode >> 25) & 1)
                {
                    if((opcode >> 24) & 1) printf("SWI\n");
                    else
                    {
                        if((opcode >> 4) & 1)
                        {
                            if((opcode >> 20) & 1)
                            {
                                printf("MRC\n");
                                int CRm = opcode & 0xf;
                                int opcode2 = (opcode >> 5) & 7;
                                int cp_index = (opcode >> 8) & 0xf;
                                int rd = (opcode >> 12) & 0xf;
                                int CRn = (opcode >> 16) & 0xf;
                                int opcode1 = (opcode >> 21) & 7;

                                if(cp_index == 15)
                                {
                                    u32 data = cp15.read(opcode1, opcode2, CRn, CRm);

                                    if(rd == 15) cpsr.whole = (cpsr.whole & 0x0fffffff) | (data & 0xf0000000);
                                    else r[rd] = data;
                                }
                            }
                            else
                            {
                                printf("MCR\n");
                                int crm = opcode & 0xf;
                                int opcode2 = (opcode >> 5) & 7;
                                int cp_index = (opcode >> 8) & 0xf;
                                int rd = (opcode >> 12) & 0xf;
                                int crn = (opcode >> 16) & 0xf;
                                int opcode1 = (opcode >> 21) & 7;

                                if(cp_index == 15) cp15.write(opcode1, opcode2, crn, crm, r[rd]);
                            }
                        }
                        else printf("CDP\n");
                    }
                }
                else
                {
                    if((opcode >> 20) & 1) printf("MRRC\n");
                    else printf("MCRR\n");
                }
                break;
            }
        }
    }
    if((opcode >> 28) == 0xf)
    {
        switch((opcode >> 26) & 3)
        {
            case 0:
            {
                if((opcode >> 16) & 1) printf("SETEND\n");
                else printf("CPS\n");
                break;
            }
            case 1:
            {
                printf("PLD\n");
                break;
            }
            case 2:
            {
                if((opcode >> 25) & 1) printf("BLX\n");
                else
                {
                    if((opcode >> 22) & 1) printf("SRS\n");
                    else printf("RFE\n");
                }
                break;
            }
            case 3:
            {
                if((opcode >> 25) & 1)
                {
                    if((opcode >> 20) & 1) printf("MRC\n");
                    else printf("MCR\n");
                }
                else
                {
                    if((opcode >> 20) & 1) printf("MRRC\n");
                    else printf("MCRR\n");
                }
                break;
            }
        }
    }

    r[15] += 4;
}

void arm_cpu::run(int insns)
{
    for(int i = 0;i<insns;i++)
    {
        tick();
    }
}