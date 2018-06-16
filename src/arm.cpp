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

u32 arm_cpu::rw(u32 addr)
{
    return rw_real(device, addr);
}

void arm_cpu::ww(u32 addr, u32 data)
{
    ww_real(device, addr, data);
}

u32 arm_cpu::get_load_store_addr(u32 opcode)
{
    int rn = (opcode >> 16) & 0xf;
    bool w = (opcode >> 21) & 1;
    bool u = (opcode >> 23) & 1;
    bool p = (opcode >> 24) & 1;
    bool i = (opcode >> 25) & 1;

    arm_mode old_mode = cpsr.mode;
    if(!p && w) cpsr.mode = arm_mode::user;
    u32 offset = 0;
    if(i)
    {
        int rm = opcode & 0xf;
        int shift_op = (opcode >> 5) & 3;
        int shift_imm = (opcode >> 7) & 0x1f;

        switch(shift_op)
        {
            case 0: offset = r[rm] << shift_imm; break;
            case 1: offset = (shift_imm != 0) ? r[rm] >> shift_imm : 0; break;
            case 2:
            {
                if(shift_imm == 0) offset = (r[rm] >> 31) ? 0xffffffff : 0;
                else offset = r[rm] >> shift_imm;
                break;
            }
            case 3:
            {
                if(shift_imm == 0) offset = (cpsr.carry << 31) | (r[rm] >> 1);
                else offset = (r[rm] >> shift_imm) | (r[rm] << (32 - shift_imm));
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

            addr = (rn == 15) ? (r[rn] + 8) : r[rn];
        }
        else if(rn != 15) addr = u ? (r[rn] + offset) : (r[rn] - offset);
        else addr = u ? (r[rn] + offset + 8) : (r[rn] - offset + 8);
    }
    else
    {
        if(!w)
        {
            addr = (rn == 15) ? (r[rn] + 8) : r[rn];

            if(u) r[rn] += offset;
            else r[rn] -= offset;
        }
    }

    cpsr.mode = old_mode;
    return addr;
}

u32 arm_cpu::get_load_store_multi_addr(u32 opcode)
{
    u16 reg_list = opcode & 0xffff;
    int rn = (opcode >> 16) & 0xf;
    bool w = (opcode >> 21) & 1;
    bool u = (opcode >> 23) & 1;
    bool p = (opcode >> 24) & 1;

    u32 count = 0;
    for(int i = 0; i < 16; i++)
    {
        if(reg_list & (1 << i)) count += 4;
    }

    u32 addr;
    if(u) addr = r[rn] + (p ? 4 : 0);
    else addr = r[rn] - count + (p ? 0 : 4);

    if(w)
    {
        if(u) r[rn] += count;
        else r[rn] -= count;
    }

    return addr;
}

u32 arm_cpu::get_shift_operand(u32 opcode, bool s)
{
    bool i = (opcode >> 25) & 1;
    u32 shift_operand = opcode & 0xfff;

    if(i)
    {
        //Immediate
        u8 imm = shift_operand & 0xff;
        u8 rotate_imm = ((shift_operand >> 8) & 0xf) << 1;
        u32 operand = (imm >> rotate_imm) | (imm << (32 - rotate_imm));
        if(rotate_imm != 0) cpsr.carry = (operand >> 31) & 1;
        return operand;
    }
    else if(!((shift_operand >> 4) & 0xff))
    {
        //Register
        int rm = shift_operand & 0xf;
        u32 operand = r[rm];
        return operand;
    }
    else
    {
        switch((shift_operand >> 4) & 7)
        {
            case 0:
            {
                //LSLImm
                int shift_imm = (shift_operand >> 7) & 0x1f;
                int rm = shift_operand & 0xf;
                u32 operand;
                if(shift_imm == 0) operand = r[rm];
                else
                {
                    operand = r[rm] << shift_imm;
                    cpsr.carry = r[rm] & (1 << (32 - shift_imm));
                }
                return operand;
            }
            case 1:
            {
                //LSLReg
                int rm = shift_operand & 0xf;
                int rs = (shift_operand >> 8) & 0xf;
                u32 operand = 0;
                if((r[rs] & 0xff) == 0) operand = r[rm];
                else if((r[rs] & 0xff) < 32)
                {
                    operand = r[rm] << (r[rs] & 0xff);
                    cpsr.carry = r[rm] & (1 << (32 - (r[rs] & 0xff)));
                }
                else if((r[rs] & 0xff) == 32)
                {
                    operand = 0;
                    cpsr.carry = r[rm] & 1;
                }
                else
                {
                    operand = 0;
                    cpsr.carry = false;
                }
                return operand;
            }
            case 2:
            {
                //LSRImm
                int shift_imm = (shift_operand >> 7) & 0x1f;
                int rm = shift_operand & 0xf;
                u32 operand;
                if(shift_imm == 0)
                {
                    operand = 0;
                    cpsr.carry = ((r[rm] >> 31) & 1);
                }
                else
                {
                    operand = r[rm] >> shift_imm;
                    cpsr.carry = r[rm] & (1 << (shift_imm - 1));
                }
                return operand;
            }
            case 3:
            {
                //LSRReg
                int rm = shift_operand & 0xf;
                int rs = (shift_operand >> 8) & 0xf;
                u32 operand = 0;
                if((r[rs] & 0xff) == 0) operand = r[rm];
                else if((r[rs] & 0xff) < 32)
                {
                    operand = r[rm] >> (r[rs] & 0xff);
                    cpsr.carry = r[rm] & (1 << ((r[rs] & 0xff) - 1));
                }
                else if((r[rs] & 0xff) == 32)
                {
                    operand = 0;
                    cpsr.carry = (r[rm] >> 31) & 1;
                }
                else
                {
                    operand = 0;
                    cpsr.carry = false;
                }
                return operand;
            }
            case 4:
            {
                //ASRImm
                int shift_imm = (shift_operand >> 7) & 0x1f;
                int rm = shift_operand & 0xf;
                u32 operand;
                if(shift_imm == 0)
                {
                    if(((r[rm] >> 31) & 1) == 0) operand = 0;
                    else operand = 0xffffffff;
                    cpsr.carry = ((r[rm] >> 31) & 1);
                }
                else
                {
                    operand = (s32)r[rm] >> shift_imm;
                    cpsr.carry = r[rm] & (1 << (shift_imm - 1));
                }
                return operand;
            }
            case 5:
            {
                //ASRReg
                int rm = shift_operand & 0xf;
                int rs = (shift_operand >> 8) & 0xf;
                u32 operand = 0;
                if((r[rs] & 0xff) == 0) operand = r[rm];
                else if((r[rs] & 0xff) < 32)
                {
                    operand = (s32)r[rm] >> (r[rs] & 0xff);
                    cpsr.carry = r[rm] & (1 << ((r[rs] & 0xff) - 1));
                }
                else
                {
                    if(((r[rm] >> 31) & 1) == 0) operand = 0;
                    else operand = 0xffffffff;
                    cpsr.carry = (r[rm] >> 31) & 1;
                }
                return operand;
            }
            case 6:
            {
                //RORImm
                int shift_imm = (shift_operand >> 7) & 0x1f;
                int rm = shift_operand & 0xf;
                u32 operand;
                if(shift_imm == 0)
                {
                    operand = (cpsr.carry << 31) | (r[rm] >> 1);
                    cpsr.carry = r[rm] & 1;
                }
                else
                {
                    operand = (r[rm] >> shift_imm) | (r[rm] << (32 - shift_imm));
                    cpsr.carry = r[rm] & (1 << (shift_imm - 1));
                }
                return operand;
            }
            case 7:
            {
                //RORReg
                int rm = shift_operand & 0xf;
                int rs = (shift_operand >> 8) & 0xf;
                u32 operand = 0;
                if((r[rs] & 0xff) == 0) operand = r[rm];
                else if((r[rs] & 0x1f) == 0)
                {
                    operand = r[rm];
                    cpsr.carry = (r[rm] >> 31) & 1;
                }
                else
                {
                    operand = (r[rm] >> (r[rs] & 0x1f)) | (r[rm] << (32 - (r[rs] & 0x1f)));
                    cpsr.carry = r[rm] & (1 << ((r[rs] & 0x1f) - 1));
                }
                return operand;
            }
        }
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
        printf("ARM11 got FIQ!\n");
    }
    else if(irq)
    {
        r14_irq = r[14];
        r[14] = r[15] + 4;
        r[15] = 0x18;
        irq = false;
        printf("ARM11 got IRQ!\n");
    }

    u32 opcode = 0;
    //for(int i = 0; i < 14; i++)
    //{
    //    printf("R%d: %08x\n", i, r[i]);
    //}
    if(!cpsr.thumb)
    {
    opcode = rw(r[15]);
    printf("Opcode:%08x\nPC:%08x\nLR:%08x\nSP:%08x\nR0:%08x\n", opcode, r[15], r[14], r[13], r[0]);
    bool condition = false;
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
        case 0x8: condition = cpsr.carry && !cpsr.zero; break;
        case 0x9: condition = !cpsr.carry || cpsr.zero; break;
        case 0xa: condition = cpsr.sign == cpsr.overflow; break;
        case 0xb: condition = cpsr.sign != cpsr.overflow; break;
        case 0xc: condition = !cpsr.zero && (cpsr.sign == cpsr.overflow); break;
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
                    if(((opcode >> 5) & 3) || ((opcode >> 24) & 1))
                    {
                        printf("Unknown load and store extension!\n");
                    }
                    else
                    {
                        switch((opcode >> 21) & 7)
                        {
                            case 0:
                            {
                                printf("MUL\n");
                                int rm = opcode & 0xf;
                                int rs = (opcode >> 8) & 0xf;
                                int rd = (opcode >> 16) & 0xf;
                                bool s = (opcode >> 20) & 1;

                                r[rd] = r[rm] * r[rs];
                                if(s)
                                {
                                    if(!r[rd]) cpsr.zero = true;
                                    else cpsr.zero = false;
                                    if(r[rd] & 0x80000000) cpsr.sign = true;
                                    else cpsr.sign = false;
                                }
                                break;
                            }
                            case 1:
                            {
                                printf("MLA\n");
                                int rm = opcode & 0xf;
                                int rs = (opcode >> 8) & 0xf;
                                int rn = (opcode >> 12) & 0xf;
                                int rd = (opcode >> 16) & 0xf;
                                bool s = (opcode >> 20) & 1;

                                r[rd] = (r[rm] * r[rs]) + r[rn];
                                if(s)
                                {
                                    if(!r[rd]) cpsr.zero = true;
                                    else cpsr.zero = false;
                                    if(r[rd] & 0x80000000) cpsr.sign = true;
                                    else cpsr.sign = false;
                                }
                                break;
                            }
                            case 2:
                            {
                                printf("UMAAL\n");
                                int rm = opcode & 0xf;
                                int rs = (opcode >> 8) & 0xf;
                                int rdlo = (opcode >> 12) & 0xf;
                                int rdhi = (opcode >> 16) & 0xf;

                                u64 result = (u64)r[rm] * r[rs];
                                result += r[rdlo] + r[rdhi];
                                r[rdlo] = (u32)result;
                                r[rdhi] = result >> 32;
                                break;
                            }
                            case 4:
                            {
                                printf("UMULL\n");
                                int rm = opcode & 0xf;
                                int rs = (opcode >> 8) & 0xf;
                                int rdlo = (opcode >> 12) & 0xf;
                                int rdhi = (opcode >> 16) & 0xf;
                                bool s = (opcode >> 20) & 1;

                                u64 result = (u64)r[rm] * r[rs];
                                r[rdlo] = (u32)result;
                                r[rdhi] = result >> 32;

                                if(s)
                                {
                                    if(!result) cpsr.zero = true;
                                    else cpsr.zero = false;
                                    if(r[rdhi] & 0x80000000) cpsr.sign = true;
                                    else cpsr.sign = false;
                                }
                                break;
                            }
                            case 5:
                            {
                                printf("UMLAL\n");
                                int rm = opcode & 0xf;
                                int rs = (opcode >> 8) & 0xf;
                                int rdlo = (opcode >> 12) & 0xf;
                                int rdhi = (opcode >> 16) & 0xf;
                                bool s = (opcode >> 20) & 1;

                                u64 result = (u64)r[rm] * r[rs];
                                result += r[rdlo] | ((u64)r[rdhi] << 32);
                                r[rdlo] = (u32)result;
                                r[rdhi] = result >> 32;

                                if(s)
                                {
                                    if(!result) cpsr.zero = true;
                                    else cpsr.zero = false;
                                    if(r[rdhi] & 0x80000000) cpsr.sign = true;
                                    else cpsr.sign = false;
                                }
                                break;
                            }
                            case 6:
                            {
                                printf("SMULL\n");
                                int rm = opcode & 0xf;
                                int rs = (opcode >> 8) & 0xf;
                                int rdlo = (opcode >> 12) & 0xf;
                                int rdhi = (opcode >> 16) & 0xf;
                                bool s = (opcode >> 20) & 1;

                                s64 result = (s64)r[rm] * (s32)r[rs];
                                result += (s64)(r[rdlo] | ((u64)r[rdhi] << 32));
                                r[rdlo] = (u32)result;
                                r[rdhi] = result >> 32;

                                if(s)
                                {
                                    if(!result) cpsr.zero = true;
                                    else cpsr.zero = false;
                                    if(r[rdhi] & 0x80000000) cpsr.sign = true;
                                    else cpsr.sign = false;
                                }
                                break;
                            }
                            case 7:
                            {
                                printf("SMLAL\n");
                                int rm = opcode & 0xf;
                                int rs = (opcode >> 8) & 0xf;
                                int rdlo = (opcode >> 12) & 0xf;
                                int rdhi = (opcode >> 16) & 0xf;
                                bool s = (opcode >> 20) & 1;

                                u64 result = (u64)r[rm] * r[rs];
                                result += r[rdlo] | ((u64)r[rdhi] << 32);
                                r[rdlo] = (u32)result;
                                r[rdhi] = result >> 32;

                                if(s)
                                {
                                    if(!result) cpsr.zero = true;
                                    else cpsr.zero = false;
                                    if(r[rdhi] & 0x80000000) cpsr.sign = true;
                                    else cpsr.sign = false;
                                }
                                break;
                            }
                        }
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
                                        else
                                        {
                                            printf("BX\n");
                                            int rm = opcode & 0xf;
                                            if(rm != 15) r[15] = (r[rm] & 0xfffffffe);
                                            cpsr.thumb = r[rm] & 1;
                                            r[15] -= 4;
                                        }
                                        break;
                                    }
                                    case 2:
                                    {
                                        printf("BXJ\n");
                                        int rm = opcode & 0xf;
                                        if(rm != 15) r[15] = (r[rm] & 0xfffffffe);
                                        cpsr.thumb = r[rm] & 1;
                                        r[15] -= 4;
                                        break;
                                    }
                                    case 3:
                                    {
                                        printf("BLX_2\n");
                                        int rm = opcode & 0xf;

                                        r[14] = r[15] - 4;
                                        r[15] = r[rm] & 0xfffffffe;
                                        if(r[rm] & 1) cpsr.thumb = true;
                                        else cpsr.thumb = false;
                                        break;
                                    }
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
                                        else cpsr.sign = false;
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
                                        else cpsr.sign = false;
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
                                u64 result64 = (u64)r[rn] - shift_operand;
                                u32 result = (u32)result64;

                                if(s && (rd != 15))
                                {
                                    cpsr.carry = r[rn] < shift_operand;
                                    cpsr.overflow = ((r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000);
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
                                u64 result64 = (u64)shift_operand - r[rn];
                                u32 result = (u32)result64;

                                if(s && (rd != 15))
                                {
                                    cpsr.carry = (s64)result64 < 0x100000000;
                                    cpsr.overflow = ((shift_operand ^ r[rn]) & (shift_operand ^ result) & 0x80000000);
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
                                u64 result64 = (u64)r[rn] + shift_operand;
                                u32 result = (u32)result64;

                                if(s && (rd != 15))
                                {
                                    cpsr.carry = result64 & 0x100000000ULL;
                                    cpsr.overflow = (~(r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000);
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
                                u64 result64 = (u64)r[rn] + shift_operand + cpsr.carry;
                                u32 result = (u32)result64;

                                if(s && (rd != 15))
                                {
                                    cpsr.carry = result64 & 0x100000000ULL;
                                    cpsr.overflow = (~(r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000);
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
                                u64 result64 = (u64)r[rn] - shift_operand - ~cpsr.carry;
                                u32 result = (u32)result64;

                                if(s && (rd != 15))
                                {
                                    cpsr.carry = (s64)result64 < 0x100000000;
                                    cpsr.overflow = ((r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000);
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
                                u64 result64 = (u64)shift_operand - r[rn] - ~cpsr.carry;
                                u32 result = (u32)result64;

                                if(s && (rd != 15))
                                {
                                    cpsr.carry = (s64)result64 < 0x100000000;
                                    cpsr.overflow = ((shift_operand ^ r[rn]) & (shift_operand ^ result) & 0x80000000);
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
                                u64 result64 = (u64)r[rn] - shift_operand;
                                u32 result = (u32)result64;

                                cpsr.carry = (s64)result64 < 0x100000000;
                                cpsr.overflow = ((r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000);
                                cpsr.sign = result & 0x80000000;
                                cpsr.zero = !result;
                                break;
                            }
                            case 0xb:
                            {
                                printf("CMN\n");
                                int rn = (opcode >> 16) & 0xf;
                                u32 shift_operand = get_shift_operand(opcode, true);
                                u64 result64 = (u64)r[rn] + shift_operand;
                                u32 result = (u32)result64;

                                cpsr.carry = result64 & 0x100000000ULL;
                                cpsr.overflow = (~(r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000);
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
                                        else cpsr.sign = false;
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
                                        else cpsr.sign = false;
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
                                        else cpsr.sign = false;
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
                                        else cpsr.sign = false;
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
                        if((opcode >> 22) & 1)
                        {
                            printf("LDRB\n");
                            int rd = (opcode >> 12) & 0xf;
                            u32 addr = get_load_store_addr(opcode);
                            u32 data = rw(addr);
                            data >>= ((addr & 3) << 3);
                            data &= 0xff;
                            r[rd] = data;
                        }
                        else
                        {
                            printf("LDR\n");
                            int rd = (opcode >> 12) & 0xf;
                            u32 addr = get_load_store_addr(opcode);
                            u32 data = rw(addr & 0xfffffffc);
                            if(!cp15.control.unaligned_access_enable && ((addr & 3) != 0))
                            {
                                data = (data >> ((addr & 3) << 3)) | (data >> (32 - ((addr & 3) << 3)));
                            }

                            r[rd] = data;

                            if(rd == 15)
                            {
                                cpsr.thumb = data & 1;
                                r[15] &= 0xfffffffe;
                            }
                        }
                    }
                    else
                    {
                        if((opcode >> 22) & 1)
                        {
                            printf("STRB\n");
                            int rd = (opcode >> 12) & 0xf;
                            u32 addr = get_load_store_addr(opcode);
                            u32 data = rw(addr);
                            data &= ~(0xff << ((addr & 3) << 3));
                            data |= (r[rd] & 0xff) << ((addr & 3) << 3);
                            ww(addr, data);
                        }
                        else
                        {
                            printf("STR\n");
                            int rd = (opcode >> 12) & 0xf;
                            u32 addr = get_load_store_addr(opcode);
                            u32 data = rw(addr);
                            ww(addr, r[rd]);
                        }
                    }
                }
                break;
            }
            case 4:
            {
                if((opcode >> 20) & 1)
                {
                    printf("LDM\n");
                    u16 reg_list = opcode & 0xffff;
                    bool pc = reg_list & 0x8000;
                    int rn = (opcode >> 16) & 0xf;
                    bool w = (opcode >> 21) & 1;
                    bool s = (opcode >> 22) & 1;

                    arm_mode oldmode = cpsr.mode;
                    if(s && !pc) cpsr.mode = arm_mode::user;
                    u32 addr = get_load_store_multi_addr(opcode) & 0xfffffffc;
                    for(int i = 0; i < 15; i++)
                    {
                        if(reg_list & (1 << i))
                        {
                            r[i] = rw(addr);
                            addr += 4;
                        }
                    }

                    cpsr.mode = oldmode;

                    if(pc)
                    {
                        u32 data = rw(addr);
                        r[15] = data & 0xfffffffe;
                        cpsr.thumb = data & 1;
                        if(s)
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
                }
                else
                {
                    printf("STM\n");
                    u16 reg_list = opcode & 0xffff;
                    int rn = (opcode >> 16) & 0xf;
                    int w = (opcode >> 21) & 1;
                    int s = (opcode >> 22) & 1;
                    int u = (opcode >> 23) & 1;
                    int p = (opcode >> 24) & 1;

                    u32 addr;
                    if(u) addr = r[rn] + (p << 2);
                    else
                    {
                        addr = r[rn] + ((!p) << 2);
                        for(int i = 0; i < 16; i++)
                        {
                            if(reg_list & (1 << i)) addr -= 4;
                        }
                    }

                    u32 count = 0;
                    arm_mode oldmode = cpsr.mode;
                    if(s) cpsr.mode = arm_mode::user;
                    for(int i = 0; i < 16; i++)
                    {
                        if(reg_list & (1 << i))
                        {
                            ww(addr + count, r[i]);
                            count += 4;
                        }
                    }

                    cpsr.mode = oldmode;

                    if(w)
                    {
                        if(u) r[rn] += count;
                        else r[rn] -= count;
                    }
                }
                break;
            }
            case 5:
            {
                if((opcode >> 24) & 1) printf("BL\n");
                else printf("B\n");
                u32 addr = opcode & 0xffffff;
                if(addr & 0x800000) addr |= 0xff000000;
                
                if((opcode >> 24) & 1) r[14] = r[15];
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
                if((opcode >> 25) & 1)
                {
                    printf("BLX\n");
                    u32 offset = (opcode & 0xffffff);
                    if(opcode & 0x800000) offset |= 0xff000000;
                    int h = (opcode >> 24) & 1;
                    r[15] += (offset << 2) + (h << 1);
                }
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
    else
    {
        opcode = rw(r[15]);
        if(r[15] & 2) opcode >>= 16;
        else opcode &= 0xffff;
        printf("Opcode:%04x\nPC:%08x\nLR:%08x\nSP:%08x\nR0:%08x\n", opcode, r[15], r[14], r[13], r[0]);
        r[15] += 2;

        switch((opcode >> 13) & 7)
        {
            case 0:
            {
                switch((opcode >> 11) & 3)
                {
                    case 0: printf("Thumb LSL\n"); break;
                    case 1: printf("Thumb LSR\n"); break;
                    case 2: printf("Thumb ASR\n"); break;
                    case 3:
                    {
                        if((opcode >> 9) & 1)
                        {
                            if((opcode >> 10) & 1) printf("Thumb SUB\n");
                            else printf("Thumb SUB_3\n");
                        }
                        else
                        {
                            if((opcode >> 10) & 1) printf("Thumb ADD\n");
                            else printf("Thumb ADD_3\n");
                        }
                        break;
                    }
                }
                break;
            }
            case 1:
            {
                switch((opcode >> 11) & 3)
                {
                    case 0: printf("Thumb MOV\n"); break;
                    case 1: printf("Thumb CMP\n"); break;
                    case 2: printf("Thumb ADD_2\n"); break;
                    case 3: printf("Thumb SUB_2\n"); break;
                }
                break;
            }
            case 2:
            {
                if((opcode >> 12) & 1)
                {
                    switch((opcode >> 9) & 7)
                    {
                        case 0: printf("Thumb STR_2\n"); break;
                        case 1: printf("Thumb STRH_2\n"); break;
                        case 2: printf("Thumb STRB_2\n"); break;
                        case 3: printf("Thumb LDRSB\n"); break;
                        case 4: printf("Thumb LDR_2\n"); break;
                        case 5: printf("Thumb LDRH_2\n"); break;
                        case 6: printf("Thumb LDRB_2\n"); break;
                        case 7: printf("Thumb LDRSH\n"); break;
                    }
                }
                else
                {
                    if((opcode >> 11) & 1) printf("Thumb LDR_3\n");
                    else
                    {
                        if((opcode >> 10) & 1)
                        {
                            switch((opcode >> 8) & 3)
                            {
                                case 0: printf("Thumb ADD_4\n"); break;
                                case 1: printf("Thumb CMP_3\n"); break;
                                case 2: printf("Thumb CPY\n"); break;
                                case 3:
                                {
                                    if((opcode >> 7) & 1)
                                    {
                                        printf("Thumb BLX_2\n");
                                        int rm = (opcode >> 3) & 7;

                                        r[14] = (r[15] - 2) | 1;
                                        r[15] = r[rm] & 0xfffffffe;
                                        cpsr.thumb = r[rm] & 1;
                                    }
                                    else
                                    {
                                        printf("Thumb BX\n");
                                        int rm = (opcode >> 3) & 0xf;

                                        r[15] = r[rm] & 0xfffffffe;
                                        cpsr.thumb = r[rm] & 1;
                                    }
                                    break;
                                }
                            }
                        }
                        else
                        {
                            switch((opcode >> 6) & 0xf)
                            {
                                case 0x0: printf("Thumb AND\n"); break;
                                case 0x1: printf("Thumb EOR\n"); break;
                                case 0x2: printf("Thumb LSL_2\n"); break;
                                case 0x3: printf("Thumb LSR_2\n"); break;
                                case 0x4: printf("Thumb ASR_2\n"); break;
                                case 0x5: printf("Thumb ADC\n"); break;
                                case 0x6: printf("Thumb SBC\n"); break;
                                case 0x7: printf("Thumb ROR\n"); break;
                                case 0x8: printf("Thumb TST\n"); break;
                                case 0x9: printf("Thumb NEG\n"); break;
                                case 0xa: printf("Thumb CMP_2\n"); break;
                                case 0xb: printf("Thumb CMN\n"); break;
                                case 0xc: printf("Thumb ORR\n"); break;
                                case 0xd: printf("Thumb MUL\n"); break;
                                case 0xe: printf("Thumb BIC\n"); break;
                                case 0xf: printf("Thumb MVN\n"); break;
                            }
                        }
                    }
                }
                break;
            }
            case 3:
            {
                if((opcode >> 11) & 1)
                {
                    if((opcode >> 12) & 1) printf("Thumb LDRB\n");
                    else printf("Thumb LDR\n");
                }
                else
                {
                    if((opcode >> 12) & 1) printf("Thumb STRB\n");
                    else printf("Thumb STR\n");
                }
                break;
            }
            case 4:
            {
                if((opcode >> 12) & 1)
                {
                    if((opcode >> 11) & 1) printf("Thumb LDR_4\n");
                    else printf("Thumb STR_3\n");
                }
                else
                {
                    if((opcode >> 11) & 1) printf("Thumb LDRH\n");
                    else printf("Thumb STRH\n");
                }
                break;
            }
            case 5:
            {
                if((opcode >> 12) & 1)
                {
                    switch((opcode >> 9) & 3)
                    {
                        case 0:
                        {
                            if((opcode >> 7) & 1) printf("Thumb SUB_4\n");
                            else printf("Thumb ADD_7\n");
                            break;
                        }
                        case 1:
                        {
                            if((opcode >> 11) & 1)
                            {
                                switch((opcode >> 6) & 3)
                                {
                                    case 0: printf("Thumb REV\n"); break;
                                    case 1: printf("Thumb REV16\n"); break;
                                    case 3: printf("Thumb REVSH\n"); break;
                                }
                            }
                            else
                            {
                                switch((opcode >> 6) & 3)
                                {
                                    case 0: printf("Thumb SXTH\n"); break;
                                    case 1: printf("Thumb SXTB\n"); break;
                                    case 2: printf("Thumb UXTH\n"); break;
                                    case 3: printf("Thumb UXTB\n"); break;
                                }
                            }
                            break;
                        }
                        case 2:
                        {
                            if((opcode >> 11) & 1) printf("Thumb POP\n");
                            else printf("Thumb PUSH\n");
                            break;
                        }
                        case 3:
                        {
                            if((opcode >> 11) & 1) printf("Thumb BKPT\n");
                            else
                            {
                                if((opcode >> 5) & 1) printf("Thumb CPS\n");
                                else printf("Thumb SETEND\n");
                            }
                            break;
                        }
                    }
                }
                else
                {
                    if((opcode >> 11) & 1) printf("Thumb ADD_6\n");
                    else printf("Thumb ADD_5\n");
                }
                break;
            }
            case 6:
            {
                if((opcode >> 12) & 1)
                {
                    if(((opcode >> 8) & 0xf) == 0xf) printf("Thumb SWI\n");
                    else
                    {
                        printf("Thumb B\n");
                        s8 imm = opcode & 0xff;
                        int cond = (opcode >> 8) & 0xf;
                        bool cond_met;
                        switch(cond)
                        {
                            case 0x0: cond_met = cpsr.zero; break;
                            case 0x1: cond_met = !cpsr.zero; break;
                            case 0x2: cond_met = cpsr.carry; break;
                            case 0x3: cond_met = !cpsr.carry; break;
                            case 0x4: cond_met = cpsr.sign; break;
                            case 0x5: cond_met = !cpsr.sign; break;
                            case 0x6: cond_met = cpsr.overflow; break;
                            case 0x7: cond_met = !cpsr.overflow; break;
                            case 0x8: cond_met = cpsr.carry && !cpsr.zero; break;
                            case 0x9: cond_met = !cpsr.carry || cpsr.zero; break;
                            case 0xa: cond_met = cpsr.sign == cpsr.overflow; break;
                            case 0xb: cond_met = cpsr.sign != cpsr.overflow; break;
                            case 0xc: cond_met = !cpsr.zero && (cpsr.sign == cpsr.overflow); break;
                            case 0xd: cond_met = cpsr.zero || (cpsr.sign != cpsr.overflow); break;
                            case 0xe: cond_met = true; break;
                            case 0xf: cond_met = false; break;
                        }
                        if(cond_met) r[15] = (u32)(r[15] + ((s16)imm << 1));
                    }
                }
                else
                {
                    if((opcode >> 11) & 1) printf("Thumb LDMIA\n");
                    else printf("Thumb STMIA\n");
                }
                break;
            }
            case 7:
            {
                if(!((opcode >> 11) & 3))
                {
                    printf("Thumb B_2\n");
                    s32 imm = (opcode & 0x7ff) << 1;
                    if(imm & 0x800) imm |= 0xfffff000;

                    r[15] = (u32)(r[15] + imm);
                }
                else
                {
                    printf("Thumb BLX\n");
                    u32 imm = opcode & 0x7ff;
                    u32 h = (opcode >> 11) & 3;

                    if(h)
                    {
                        if(h != 2)
                        {
                            u32 lr = (r[15] - 2) | 1;
                            r[15] = r[14] + (imm << 1);
                            r[14] = lr;

                            if(h == 1)
                            {
                                r[15] &= 0xfffffffc;
                                cpsr.thumb = false;
                            }
                        }
                        else
                        {
                            if(imm & 0x400) imm |= 0xfffff800;
                            r[14] = (u32)(r[15] + (imm << 12));
                        }
                    } 
                }
                break;
            }
        }
    }
}

void arm_cpu::run(int insns)
{
    for(int i = 0;i<insns;i++)
    {
        tick();
    }
}