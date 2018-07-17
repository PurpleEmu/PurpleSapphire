#include "arm.h"
#include "iphone2g.h"
#include "iphone3gs.h"

#define printf(...)

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

    switch(type)
    {
        case arm_type::arm11:
        {
            fpsid.whole = 0x41012083;
            mvfr0.whole = 0x11111111;
            mvfr1_arm11.whole = 0x00000000;
            break;
        }
        case arm_type::cortex_a8:
        {
            fpsid.whole = 0x410330c3;
            mvfr0.whole = 0x11110222;
            mvfr1_cortex_a8.whole = 0x00011111;
            break;
        }
    }

    fpscr.whole = 0;
    fpexc.whole = 0;

    fpinst = 0xee000a00;
    fpinst2 = 0;

    cp15.cpu = this;
    cp15.type = type;
    cp15.init();

    do_print = true;
    cp15.do_print = true;

    just_branched = false;

    opcode = 0;
}

void arm_cpu::init_hle(bool print)
{
    hle = true;
    if(!print)
    {
        do_print = false;
        cp15.do_print = false;
    }
}

u32 arm_cpu::rw(u32 addr)
{
    return rw_real(device, addr);
}

void arm_cpu::ww(u32 addr, u32 data)
{
    ww_real(device, addr, data);
}

u32 arm_cpu::get_load_store_addr()
{
    int rn = (opcode >> 16) & 0xf;
    bool w = (opcode >> 21) & 1;
    bool u = (opcode >> 23) & 1;
    bool p = (opcode >> 24) & 1;
    bool i = (opcode >> 25) & 1;

    arm_mode old_mode = cpsr.mode;
    if(!p && w) cpsr.mode = mode_user;
    u32 offset = 0;
    if(i)
    {
        int rm = opcode & 0xf;
        int shift_op = (opcode >> 5) & 3;
        int shift_imm = (opcode >> 7) & 0x1f;

        switch(shift_op)
        {
        case 0:
            offset = r[rm] << shift_imm;
            break;
        case 1:
            offset = (shift_imm != 0) ? r[rm] >> shift_imm : 0;
            break;
        case 2:
        {
            if(shift_imm == 0) offset = (r[rm] >> 31) ? 0xffffffff : 0;
            else offset = r[rm] >> shift_imm;
            break;
        }
        case 3:
        {
            if(shift_imm == 0) offset = ((u32)cpsr.carry << 31) | (r[rm] >> 1);
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

    cpsr.mode = old_mode;
    return addr;
}

u32 arm_cpu::get_load_store_multi_addr()
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

u32 arm_cpu::get_shift_operand(bool s)
{
    bool i = (opcode >> 25) & 1;
    u32 shift_operand = opcode & 0xfff;
    int rm = shift_operand & 0xf;

    if(i)
    {
        //Immediate
        u8 imm = shift_operand & 0xff;
        u8 rotate_imm = ((shift_operand >> 8) & 0xf) << 1;
        u32 operand = imm;
        if(rotate_imm != 0) operand = (imm >> rotate_imm) | (imm << (32 - rotate_imm));
        if(s && (rotate_imm != 0)) cpsr.carry = (operand >> 31) & 1;
        return operand;
    }
    else if(!((shift_operand >> 4) & 0xff))
    {
        //Register
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
            u32 operand;
            if(shift_imm == 0) operand = r[rm];
            else
            {
                operand = r[rm] << shift_imm;
                if(s) cpsr.carry = (r[rm] & (1 << (32 - shift_imm))) ? 1 : 0;
            }
            return operand;
        }
        case 1:
        {
            //LSLReg
            int rs = (shift_operand >> 8) & 0xf;
            u32 operand = 0;
            if((r[rs] & 0xff) == 0) operand = r[rm];
            else if((r[rs] & 0xff) < 32)
            {
                operand = r[rm] << (r[rs] & 0xff);
                if(s) cpsr.carry = (r[rm] & (1 << (32 - (r[rs] & 0xff)))) ? 1 : 0;
            }
            else if((r[rs] & 0xff) == 32)
            {
                operand = 0;
                if(s) cpsr.carry = r[rm] & 1;
            }
            else
            {
                operand = 0;
                if(s) cpsr.carry = false;
            }
            return operand;
        }
        case 2:
        {
            //LSRImm
            int shift_imm = (shift_operand >> 7) & 0x1f;
            u32 operand;
            if(shift_imm == 0)
            {
                operand = 0;
                if(s) cpsr.carry = ((r[rm] >> 31) & 1);
            }
            else
            {
                operand = r[rm] >> shift_imm;
                if(s) cpsr.carry = (r[rm] & (1 << (32 - shift_imm))) ? 1 : 0;
            }
            return operand;
        }
        case 3:
        {
            //LSRReg
            int rs = (shift_operand >> 8) & 0xf;
            r[rm] += 4;
            u32 operand = 0;
            if((r[rs] & 0xff) == 0) operand = r[rm];
            else if((r[rs] & 0xff) < 32)
            {
                operand = r[rm] >> (r[rs] & 0xff);
                if(s) cpsr.carry = (r[rm] & (1 << (32 - (r[rs] & 0xff)))) ? 1 : 0;
            }
            else if((r[rs] & 0xff) == 32)
            {
                operand = 0;
                if(s) cpsr.carry = (r[rm] >> 31) & 1;
            }
            else
            {
                operand = 0;
                if(s) cpsr.carry = false;
            }
            return operand;
        }
        case 4:
        {
            //ASRImm
            int shift_imm = (shift_operand >> 7) & 0x1f;
            u32 operand;
            if(shift_imm == 0)
            {
                if(((r[rm] >> 31) & 1) == 0) operand = 0;
                else operand = 0xffffffff;
                if(s) cpsr.carry = ((r[rm] >> 31) & 1);
            }
            else
            {
                operand = (s32)r[rm] >> shift_imm;
                if(s) cpsr.carry = (r[rm] & (1 << (32 - shift_imm))) ? 1 : 0;
            }
            return operand;
        }
        case 5:
        {
            //ASRReg
            int rs = (shift_operand >> 8) & 0xf;
            r[rm] += 4;
            u32 operand = 0;
            if((r[rs] & 0xff) == 0) operand = r[rm];
            else if((r[rs] & 0xff) < 32)
            {
                operand = (s32)r[rm] >> (r[rs] & 0xff);
                if(s) cpsr.carry = (r[rm] & (1 << (32 - (r[rs] & 0xff)))) ? 1 : 0;
            }
            else
            {
                if(((r[rm] >> 31) & 1) == 0) operand = 0;
                else operand = 0xffffffff;
                if(s) cpsr.carry = (r[rm] >> 31) & 1;
            }
            return operand;
        }
        case 6:
        {
            //RORImm
            int shift_imm = (shift_operand >> 7) & 0x1f;
            u32 operand;
            if(shift_imm == 0)
            {
                operand = ((u32)cpsr.carry << 31) | (r[rm] >> 1);
                if(s) cpsr.carry = r[rm] & 1;
            }
            else
            {
                operand = (r[rm] >> shift_imm) | (r[rm] << (32 - shift_imm));
                if(s) cpsr.carry = (r[rm] & (1 << (32 - shift_imm))) ? 1 : 0;
            }
            return operand;
        }
        case 7:
        {
            //RORReg
            int rs = (shift_operand >> 8) & 0xf;
            r[rm] += 4;
            u32 operand = 0;
            if((r[rs] & 0xff) == 0) operand = r[rm];
            else if((r[rs] & 0x1f) == 0)
            {
                operand = r[rm];
                if(s) cpsr.carry = (r[rm] >> 31) & 1;
            }
            else
            {
                operand = (r[rm] >> (r[rs] & 0x1f)) | (r[rm] << (32 - (r[rs] & 0x1f)));
                if(s) cpsr.carry = (r[rm] & (1 << (32 - (r[rs] & 0xff)))) ? 1 : 0;
            }
            return operand;
        }
        }
    }
    return 0; //Shuts up GCC.
}

void arm_cpu::tick_media(u32 opcode)
{
    switch((opcode >> 23) & 3)
    {
        case 0:
        {
            switch((opcode >> 20) & 7)
            {
                case 1:
                {
                    switch((opcode >> 5) & 7)
                    {
                        case 0: printf("SADD16\n");
                        case 1: printf("SADDSUBX\n");
                        case 2: printf("SSUBADDX\n");
                        case 3: printf("SSUB16\n");
                        case 4: printf("SADD8\n");
                        case 7: printf("SSUB8\n");
                    }
                    break;
                }
                case 2:
                {
                    switch((opcode >> 5) & 7)
                    {
                        case 0: printf("QADD16\n");
                        case 1: printf("QADDSUBX\n");
                        case 2: printf("QSUBADDX\n");
                        case 3: printf("QSUB16\n");
                        case 4: printf("QADD8\n");
                        case 7: printf("QSUB8\n");
                    }
                    break;
                }
                case 3:
                {
                    switch((opcode >> 5) & 7)
                    {
                        case 0: printf("SHADD16\n");
                        case 1: printf("SHADDSUBX\n");
                        case 2: printf("SHSUBADDX\n");
                        case 3: printf("SHSUB16\n");
                        case 4: printf("SHADD8\n");
                        case 7: printf("SHSUB8\n");
                    }
                    break;
                }
                case 5:
                {
                    switch((opcode >> 5) & 7)
                    {
                        case 0: printf("UADD16\n");
                        case 1: printf("UADDSUBX\n");
                        case 2: printf("USUBADDX\n");
                        case 3: printf("USUB16\n");
                        case 4: printf("UADD8\n");
                        case 7: printf("USUB8\n");
                    }
                    break;
                }
                case 6:
                {
                    switch((opcode >> 5) & 7)
                    {
                        case 0: printf("UQADD16\n");
                        case 1: printf("UQADDSUBX\n");
                        case 2: printf("UQSUBADDX\n");
                        case 3: printf("UQSUB16\n");
                        case 4: printf("UQADD8\n");
                        case 7: printf("UQSUB8\n");
                    }
                    break;
                }
                case 7:
                {
                    switch((opcode >> 5) & 7)
                    {
                        case 0: printf("UHADD16\n");
                        case 1: printf("UHADDSUBX\n");
                        case 2: printf("UHSUBADDX\n");
                        case 3: printf("UHSUB16\n");
                        case 4: printf("UHADD8\n");
                        case 7: printf("UHSUB8\n");
                    }
                    break;
                }
            }
            break;
        }
        case 1:
        {
            if((opcode >> 5) & 1)
            {
                switch((opcode >> 6) & 3)
                {
                    case 0:
                    {
                        switch((opcode >> 20) & 3)
                        {
                            case 2:
                            {
                                if((opcode >> 2) & 1) printf("USAT16\n");
                                else printf("SSAT16\n");
                                break;
                            }
                            case 3: printf("REV\n"); break;
                        }
                        break;
                    }
                    case 1:
                    {
                        bool r15 = ((opcode >> 16) & 0xf) == 0xf;

                        switch((opcode >> 20) & 7)
                        {
                            case 0:
                            {
                                if(r15)
                                {
                                    printf("SXTB16\n");
                                    int rm = opcode & 0xf;
                                    int rotate = ((opcode >> 10) & 3) << 3;
                                    int rd = (opcode >> 12) & 0xf;

                                    u32 operand = (r[rm] >> rotate) | (r[rm] << (32 - rotate));
                                    r[rd] = (u16)(s16)(operand & 0xffff);
                                    r[rd] |= (u32)(((s16)operand >> 16) << 16);
                                }
                                else printf("SXTAB16\n");
                                break;
                            }
                            case 2:
                            {
                                if(r15)
                                {
                                    printf("SXTB\n");
                                    int rm = opcode & 0xf;
                                    int rotate = ((opcode >> 10) & 3) << 3;
                                    int rd = (opcode >> 12) & 0xf;

                                    s8 operand = (r[rm] >> rotate) | (r[rm] << (32 - rotate));
                                    r[rd] = (u32)(s32)operand;
                                }
                                else printf("SXTAB\n");
                                break;
                            }
                            case 3:
                            {
                                if(r15) printf("SXTH\n");
                                else printf("SXTAH\n");
                                break;
                            }
                            case 4:
                            {
                                if(r15) printf("UXTB16\n");
                                else printf("UXTAB16\n");
                                break;
                            }
                            case 6:
                            {
                                if(r15) printf("UXTB\n");
                                else printf("UXTAB\n");
                                break;
                            }
                            case 7:
                            {
                                if(r15) printf("UXTH16\n");
                                else printf("UXTAH16\n");
                                break;
                            }
                        }
                        break;
                    }
                    case 2:
                    {
                        switch((opcode >> 20) & 7)
                        {
                            case 0: printf("SEL\n"); break;
                            case 3: printf("REV16\n"); break;
                            case 7: printf("REVSH\n"); break;
                        }
                        break;
                    }
                }
            }
            else
            {
                if((opcode >> 21) & 1)
                {
                    if((opcode >> 22) & 1) printf("USAT\n");
                    else printf("SSAT\n");
                }
                else
                {
                    if((opcode >> 6) & 1) printf("PKHTB\n");
                    else printf("PKHBT\n");
                }
            }
            break;
        }
        case 2:
        {
            switch((opcode >> 20) & 7)
            {
                case 0:
                {
                    bool r15 = ((opcode >> 12) & 0xf) == 0xf;

                    switch((opcode >> 6) & 3)
                    {
                        case 0:
                        {
                            if(r15) printf("SMUAD\n");
                            else printf("SMLAD\n");
                            break;
                        }
                        case 1:
                        {
                            if(r15) printf("SMUAD\n");
                            else printf("SMLAD\n");
                            break;
                        }
                    }
                    break;
                }
                case 4:
                {
                    switch((opcode >> 6) & 3)
                    {
                        case 0: printf("SMLALD\n"); break;
                        case 1: printf("SMLSLD\n"); break;
                    }
                    break;
                }
                case 5:
                {
                    switch((opcode >> 6) & 3)
                    {
                        case 0:
                        {
                            if(((opcode >> 12) & 0xf) == 0xf) printf("SMMUL\n");
                            else printf("SMMLA\n");
                            break;
                        }
                        case 3: printf("SMMLS\n"); break;
                    }
                    break;
                }
            }
            break;
        }
        case 3:
        {
            if(((opcode >> 12) & 0xf) == 0xf) printf("USADA8\n");
            else printf("USAD8\n");
            break;
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
    //makes logs shorter when doing hle
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
    if(hle && ((true_r15 == 0x18012f5e) || (true_r15 == 0x00012f5e)))
    {
        iphone2g* dev2g = nullptr;
        iphone3gs* dev3gs = nullptr;
        if(type == arm_type::arm11)
        {
            dev2g = (iphone2g*)device;
        }
        else if(type == arm_type::cortex_a8)
        {
            dev3gs = (iphone3gs*)device;
        }
        do_print = true;
        cp15.do_print = true;
        if(type == arm_type::arm11) dev2g->do_print = true;
        else if(type == arm_type::cortex_a8) dev3gs->do_print = true;
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
        switch(type)
        {
            case arm_type::arm11: cp15.control_arm11.high_vectors ? r[15] = 0xffff0000 : r[15] = 0x00; break;
            case arm_type::cortex_a8: cp15.control_cortex_a8.high_vectors ? r[15] = 0xffff0000 : r[15] = 0x00; break;
        }
        switch(type)
        {
            case arm_type::arm11: cpsr.endianness = cp15.control_arm11.endian_on_exception; break;
            case arm_type::cortex_a8: cpsr.endianness = cp15.control_cortex_a8.endian_on_exception; break;
        }
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
        switch(type)
        {
            case arm_type::arm11: cp15.control_arm11.high_vectors ? r[15] = 0xffff0010 : r[15] = 0x10; break;
            case arm_type::cortex_a8: cp15.control_cortex_a8.high_vectors ? r[15] = 0xffff0010 : r[15] = 0x10; break;
        }
        switch(type)
        {
            case arm_type::arm11: cpsr.endianness = cp15.control_arm11.endian_on_exception; break;
            case arm_type::cortex_a8: cpsr.endianness = cp15.control_cortex_a8.endian_on_exception; break;
        }
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
        switch(type)
        {
            case arm_type::arm11: cp15.control_arm11.high_vectors ? r[15] = 0xffff001c : r[15] = 0x1c; break;
            case arm_type::cortex_a8: cp15.control_cortex_a8.high_vectors ? r[15] = 0xffff001c : r[15] = 0x1c; break;
        }
        cpsr.fiq_disable = true;
        switch(type)
        {
            case arm_type::arm11: cpsr.endianness = cp15.control_arm11.endian_on_exception; break;
            case arm_type::cortex_a8: cpsr.endianness = cp15.control_cortex_a8.endian_on_exception; break;
        }
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
        switch(type)
        {
            case arm_type::arm11: cp15.control_arm11.high_vectors ? r[15] = 0xffff0018 : r[15] = 0x18; break;
            case arm_type::cortex_a8: cp15.control_cortex_a8.high_vectors ? r[15] = 0xffff0018 : r[15] = 0x18; break;
        }
        cpsr.irq_disable = true;
        switch(type)
        {
            case arm_type::arm11: cpsr.endianness = cp15.control_arm11.endian_on_exception; break;
            case arm_type::cortex_a8: cpsr.endianness = cp15.control_cortex_a8.endian_on_exception; break;
        }
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
        switch(type)
        {
            case arm_type::arm11: cp15.control_arm11.high_vectors ? r[15] = 0xffff0004 : r[15] = 0x04; break;
            case arm_type::cortex_a8: cp15.control_cortex_a8.high_vectors ? r[15] = 0xffff0004 : r[15] = 0x04; break;
        }
        switch(type)
        {
            case arm_type::arm11: cpsr.endianness = cp15.control_arm11.endian_on_exception; break;
            case arm_type::cortex_a8: cpsr.endianness = cp15.control_cortex_a8.endian_on_exception; break;
        }
        cpsr.thumb = false;
        cpsr.jazelle = false;
        cpsr.irq_disable = true;
        undefined_enable = false;
    }

    if(!irq) irq_enable = true;
    if(!fiq) fiq_enable = true;
    if(!data_abort) abort_enable = true;
    if(!undefined) undefined_enable = true;

    //for(int i = 0; i < 14; i++)
    //{
    //    printf("R%d: %08x\n", i, r[i]);
    //}
    if(!cpsr.thumb)
    {
        bool old_just_branched = just_branched;
        opcode = next_opcode;
        next_opcode = rw(r[15]);
        r[15] += 4;
        just_branched = false;
#undef printf
        printf("Opcode:%08x\nPC:%08x\n", opcode, true_r15);
        for(int i = 0; i < 15; i++)
        {
            printf("R%d:%08x\n", i, r[i]);
        }
#define printf(...)
        bool condition = false;
        switch(opcode >> 28)
        {
        case 0x0:
            condition = cpsr.zero;
            break;
        case 0x1:
            condition = !cpsr.zero;
            break;
        case 0x2:
            condition = cpsr.carry;
            break;
        case 0x3:
            condition = !cpsr.carry;
            break;
        case 0x4:
            condition = cpsr.sign;
            break;
        case 0x5:
            condition = !cpsr.sign;
            break;
        case 0x6:
            condition = cpsr.overflow;
            break;
        case 0x7:
            condition = !cpsr.overflow;
            break;
        case 0x8:
            condition = cpsr.carry && !cpsr.zero;
            break;
        case 0x9:
            condition = !cpsr.carry || cpsr.zero;
            break;
        case 0xa:
            condition = cpsr.sign == cpsr.overflow;
            break;
        case 0xb:
            condition = cpsr.sign != cpsr.overflow;
            break;
        case 0xc:
            condition = !cpsr.zero && (cpsr.sign == cpsr.overflow);
            break;
        case 0xd:
            condition = cpsr.zero || (cpsr.sign != cpsr.overflow);
            break;
        case 0xe:
            condition = true;
            break;
        case 0xf:
            condition = false;
            break;
        }

        if(condition)
        {
            switch((opcode >> 25) & 7)
            {
            case 0:
            case 1:
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

                            u64 result = (u64)r[rm] * (u64)r[rs];
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
                            if(!((opcode >> 16) & 0xf))
                            {
                                if(!((opcode >> 8) & 0xf))
                                {
                                    if(!((opcode >> 22) & 1))
                                    {
                                        if((opcode & 0xff) == 0) printf("NOP\n");
                                        else if((opcode & 0xff) == 1) printf("YIELD\n");
                                    }
                                }
                            }
                            else
                            {
                                printf("MSR\n");
                                int rm = opcode & 0xf;
                                u32 mask = (opcode >> 16) & 0xf;
                                bool spsr = (opcode >> 22) & 1;
                                bool i = (opcode >> 25) & 1;
                                u32 operand;

                                if (i)
                                {
                                    operand = ((opcode & 0xff) >> ((opcode >> 8) & 0xf)) | ((opcode & 0xff) << (32 - ((opcode >> 8) & 0xf)));
                                }
                                else operand = r[rm];

                                if(operand & 0x06f0fc00) printf("Unpredictable!\n");

                                u32 byte_mask = 0;
                                if(mask & 1) byte_mask |= 0x000000ff;
                                if(mask & 2) byte_mask |= 0x0000ff00;
                                if(mask & 4) byte_mask |= 0x00ff0000;
                                if(mask & 8) byte_mask |= 0xff000000;

                                if(!spsr)
                                {
                                    if(operand & 0x01000020) printf("Unpredictable!\n");
                                    else mask = byte_mask & (0xf80f0200 | 0x000001df);

                                    arm_mode old_mode = cpsr.mode;

                                    cpsr.whole = (cpsr.whole & ~mask) | (operand & mask);

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
                                }
                                else
                                {
                                    mask = byte_mask & (0xf80f0200 | 0x000001df | 0x01000020);
                                    switch(cpsr.mode)
                                    {
                                    case mode_fiq:
                                        spsr_fiq.whole = (spsr_fiq.whole & ~mask) | (operand & mask);
                                        break;
                                    case mode_irq:
                                        spsr_irq.whole = (spsr_irq.whole & ~mask) | (operand & mask);
                                        break;
                                    case mode_supervisor:
                                        spsr_svc.whole = (spsr_svc.whole & ~mask) | (operand & mask);
                                        break;
                                    case mode_abort:
                                        spsr_abt.whole = (spsr_abt.whole & ~mask) | (operand & mask);
                                        break;
                                    case mode_undefined:
                                        spsr_und.whole = (spsr_und.whole & ~mask) | (operand & mask);
                                        break;
                                    default:
                                        printf("Unpredictable!\n");
                                        break;
                                    }
                                }
                            }
                        }
                        else
                        {
                            if(((opcode >> 7) & 1) && (type >= arm_type::arm9))
                            {
                                switch((opcode >> 21) & 3)
                                {
                                case 0:
                                    printf("SMLA\n");
                                    break;
                                case 1:
                                {
                                    if((opcode >> 5) & 1) printf("SMULW\n");
                                    else printf("SMLAW\n");
                                    break;
                                }
                                case 2:
                                    printf("SMLAL\n");
                                    break;
                                case 3:
                                    printf("SMUL\n");
                                    break;
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
                                        int rm = opcode & 0xf;
                                        u32 mask = (opcode >> 16) & 0xf;
                                        bool spsr = (opcode >> 22) & 1;
                                        bool i = (opcode >> 25) & 1;
                                        u32 operand;

                                        if (i)
                                        {
                                            operand = ((opcode & 0xff) >> ((opcode >> 8) & 0xf)) | ((opcode & 0xff) << (32 - ((opcode >> 8) & 0xf)));
                                        }
                                        else operand = r[rm];

                                        if(operand & 0x06f0fc00) printf("Unpredictable!\n");

                                        u32 byte_mask = 0;
                                        if(mask & 1) byte_mask |= 0x000000ff;
                                        if(mask & 2) byte_mask |= 0x0000ff00;
                                        if(mask & 4) byte_mask |= 0x00ff0000;
                                        if(mask & 1) byte_mask |= 0xff000000;

                                        if(!spsr)
                                        {
                                            if(operand & 0x01000020) printf("Unpredictable!\n");
                                            else mask = byte_mask & (0xf80f0200 | 0x000001df);
                                            arm_mode old_mode = cpsr.mode;

                                            cpsr.whole = (cpsr.whole & ~mask) | (operand & mask);

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
                                        }
                                        else
                                        {
                                            mask = byte_mask & (0xf80f0200 | 0x000001df | 0x01000020);
                                            switch(cpsr.mode)
                                            {
                                            case mode_fiq:
                                                spsr_fiq.whole = (spsr_fiq.whole & ~mask) | (operand & mask);
                                                break;
                                            case mode_irq:
                                                spsr_irq.whole = (spsr_irq.whole & ~mask) | (operand & mask);
                                                break;
                                            case mode_supervisor:
                                                spsr_svc.whole = (spsr_svc.whole & ~mask) | (operand & mask);
                                                break;
                                            case mode_abort:
                                                spsr_abt.whole = (spsr_abt.whole & ~mask) | (operand & mask);
                                                break;
                                            case mode_undefined:
                                                spsr_und.whole = (spsr_und.whole & ~mask) | (operand & mask);
                                                break;
                                            default:
                                                printf("Unpredictable!\n");
                                                break;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        printf("MRS\n");
                                        int rd = (opcode >> 12) & 0xf;
                                        bool spsr = (opcode >> 22) & 1;

                                        switch(cpsr.mode)
                                        {
                                        case mode_fiq:
                                            r[rd] = spsr ? spsr_fiq.whole : cpsr.whole;
                                            break;
                                        case mode_irq:
                                            r[rd] = spsr ? spsr_irq.whole : cpsr.whole;
                                            break;
                                        case mode_supervisor:
                                            r[rd] = spsr ? spsr_svc.whole : cpsr.whole;
                                            break;
                                        case mode_abort:
                                            r[rd] = spsr ? spsr_abt.whole : cpsr.whole;
                                            break;
                                        case mode_undefined:
                                            r[rd] = spsr ? spsr_und.whole : cpsr.whole;
                                            break;
                                        }
                                    }
                                    break;
                                }
                                case 1:
                                {
                                    if(((opcode >> 22) & 1) && (type >= arm_type::arm9)) printf("CLZ\n");
                                    else
                                    {
                                        printf("BX\n");
                                        int rm = opcode & 0xf;
                                        if(condition)
                                        {
                                            u32 real_rm = (rm == 15) ? true_r15 : r[rm];
                                            r[15] = (real_rm & 0xfffffffe);
                                            cpsr.thumb = r[rm] & 1;
                                            //if(cpsr.thumb) r[15] -= 2;
                                            just_branched = true;
                                        }
                                    }
                                    break;
                                }
                                case 2:
                                {
                                    printf("BXJ\n");
                                    int rm = opcode & 0xf;
                                    if(condition)
                                    {
                                        r[15] = (r[rm] & 0xfffffffe);
                                        if(rm == 15) r[15] += 8;
                                        cpsr.thumb = r[rm] & 1;
                                        just_branched = true;
                                    }
                                    break;
                                }
                                case 3:
                                {
                                    if(type >= arm_type::arm9)
                                    {
                                    printf("BLX_2\n");
                                    int rm = opcode & 0xf;

                                    u32 oldpc = r[15];
                                    r[15] = r[rm] & 0xfffffffe;
                                    just_branched = true;
                                    cpsr.thumb = r[rm] & 1;
                                    r[14] = oldpc - 4;
                                    }
                                    break;
                                }
                                case 5:
                                {
                                    if(type >= arm_type::arm9)
                                    {
                                    switch((opcode >> 21) & 3)
                                    {
                                    case 0:
                                        printf("QADD\n");
                                        break;
                                    case 1:
                                        printf("QSUB\n");
                                        break;
                                    case 2:
                                        printf("QDADD\n");
                                        break;
                                    case 3:
                                        printf("QDSUB\n");
                                        break;
                                    }
                                    }
                                    break;
                                }
                                case 7:
                                    if(type >= arm_type::arm9) printf("BKPT\n");
                                    break;
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
                            u32 shift_operand = get_shift_operand(s && (rd != 15));

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
                                    case mode_fiq:
                                        cpsr = spsr_fiq;
                                        break;
                                    case mode_irq:
                                        cpsr = spsr_irq;
                                        break;
                                    case mode_supervisor:
                                        cpsr = spsr_svc;
                                        break;
                                    case mode_abort:
                                        cpsr = spsr_abt;
                                        break;
                                    case mode_undefined:
                                        cpsr = spsr_und;
                                        break;
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
                            u32 shift_operand = get_shift_operand(s && (rd != 15));

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
                                    case mode_fiq:
                                        cpsr = spsr_fiq;
                                        break;
                                    case mode_irq:
                                        cpsr = spsr_irq;
                                        break;
                                    case mode_supervisor:
                                        cpsr = spsr_svc;
                                        break;
                                    case mode_abort:
                                        cpsr = spsr_abt;
                                        break;
                                    case mode_undefined:
                                        cpsr = spsr_und;
                                        break;
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
                            u32 shift_operand = get_shift_operand(s && (rd != 15));
                            u64 result64 = (u64)r[rn] - shift_operand;
                            u32 result = (u32)result64;

                            if(s && (rd != 15))
                            {
                                cpsr.carry = result64 < 0x100000000LL;
                                cpsr.overflow = ((r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000);
                                cpsr.sign = (result & 0x80000000) >> 31;
                                cpsr.zero = !result;
                            }

                            r[rd] = result;

                            if(s && (rd == 15))
                            {
                                switch(cpsr.mode)
                                {
                                case mode_fiq:
                                    cpsr = spsr_fiq;
                                    break;
                                case mode_irq:
                                    cpsr = spsr_irq;
                                    break;
                                case mode_supervisor:
                                    cpsr = spsr_svc;
                                    break;
                                case mode_abort:
                                    cpsr = spsr_abt;
                                    break;
                                case mode_undefined:
                                    cpsr = spsr_und;
                                    break;
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
                            u32 shift_operand = get_shift_operand(s && (rd != 15));
                            u64 result64 = (u64)shift_operand - r[rn];
                            u32 result = (u32)result64;

                            if(s && (rd != 15))
                            {
                                cpsr.carry = result64 < 0x100000000LL;
                                cpsr.overflow = ((shift_operand ^ r[rn]) & (shift_operand ^ result) & 0x80000000);
                                cpsr.sign = (result & 0x80000000) >> 31;
                                cpsr.zero = !result;
                            }

                            r[rd] = result;

                            if(s && (rd == 15))
                            {
                                switch(cpsr.mode)
                                {
                                case mode_fiq:
                                    cpsr = spsr_fiq;
                                    break;
                                case mode_irq:
                                    cpsr = spsr_irq;
                                    break;
                                case mode_supervisor:
                                    cpsr = spsr_svc;
                                    break;
                                case mode_abort:
                                    cpsr = spsr_abt;
                                    break;
                                case mode_undefined:
                                    cpsr = spsr_und;
                                    break;
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
                            u32 shift_operand = get_shift_operand(s && (rd != 15));
                            u64 result64 = (u64)r[rn] + shift_operand;
                            u32 result = (u32)result64;

                            if(s && (rd != 15))
                            {
                                cpsr.carry = result64 & 0x100000000ULL;
                                cpsr.overflow = (~(r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000);
                                cpsr.sign = (result & 0x80000000) >> 31;
                                cpsr.zero = !result;
                            }

                            r[rd] = result;

                            if(s && (rd == 15))
                            {
                                switch(cpsr.mode)
                                {
                                case mode_fiq:
                                    cpsr = spsr_fiq;
                                    break;
                                case mode_irq:
                                    cpsr = spsr_irq;
                                    break;
                                case mode_supervisor:
                                    cpsr = spsr_svc;
                                    break;
                                case mode_abort:
                                    cpsr = spsr_abt;
                                    break;
                                case mode_undefined:
                                    cpsr = spsr_und;
                                    break;
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
                            u32 shift_operand = get_shift_operand(s && (rd != 15));
                            u64 result64 = (u64)r[rn] + shift_operand + cpsr.carry;
                            u32 result = (u32)result64;

                            if(s && (rd != 15))
                            {
                                cpsr.carry = result64 & 0x100000000ULL;
                                cpsr.overflow = (~(r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000);
                                cpsr.sign = (result & 0x80000000) >> 31;
                                cpsr.zero = !result;
                            }

                            r[rd] = result;

                            if(s && (rd == 15))
                            {
                                switch(cpsr.mode)
                                {
                                case mode_fiq:
                                    cpsr = spsr_fiq;
                                    break;
                                case mode_irq:
                                    cpsr = spsr_irq;
                                    break;
                                case mode_supervisor:
                                    cpsr = spsr_svc;
                                    break;
                                case mode_abort:
                                    cpsr = spsr_abt;
                                    break;
                                case mode_undefined:
                                    cpsr = spsr_und;
                                    break;
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
                            u32 shift_operand = get_shift_operand(s && (rd != 15));
                            u64 result64 = (u64)r[rn] - shift_operand - ~cpsr.carry;
                            u32 result = (u32)result64;

                            if(s && (rd != 15))
                            {
                                cpsr.carry = result64 < 0x100000000LL;
                                cpsr.overflow = ((r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000);
                                cpsr.sign = (result & 0x80000000) >> 31;
                                cpsr.zero = !result;
                            }

                            r[rd] = result;

                            if(s && (rd == 15))
                            {
                                switch(cpsr.mode)
                                {
                                case mode_fiq:
                                    cpsr = spsr_fiq;
                                    break;
                                case mode_irq:
                                    cpsr = spsr_irq;
                                    break;
                                case mode_supervisor:
                                    cpsr = spsr_svc;
                                    break;
                                case mode_abort:
                                    cpsr = spsr_abt;
                                    break;
                                case mode_undefined:
                                    cpsr = spsr_und;
                                    break;
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
                            u32 shift_operand = get_shift_operand(s && (rd != 15));
                            u64 result64 = (u64)shift_operand - r[rn] - ~cpsr.carry;
                            u32 result = (u32)result64;

                            if(s && (rd != 15))
                            {
                                cpsr.carry = result64 < 0x100000000LL;
                                cpsr.overflow = ((shift_operand ^ r[rn]) & (shift_operand ^ result) & 0x80000000);
                                cpsr.sign = (result & 0x80000000) >> 31;
                                cpsr.zero = !result;
                            }

                            r[rd] = result;

                            if(s && (rd == 15))
                            {
                                switch(cpsr.mode)
                                {
                                case mode_fiq:
                                    cpsr = spsr_fiq;
                                    break;
                                case mode_irq:
                                    cpsr = spsr_irq;
                                    break;
                                case mode_supervisor:
                                    cpsr = spsr_svc;
                                    break;
                                case mode_abort:
                                    cpsr = spsr_abt;
                                    break;
                                case mode_undefined:
                                    cpsr = spsr_und;
                                    break;
                                }
                            }
                            break;
                        }
                        case 0x8:
                        {
                            printf("TST\n");
                            int rn = (opcode >> 16) & 0xf;
                            u32 shift_operand = get_shift_operand(true);
                            u32 result = r[rn] & shift_operand;

                            cpsr.sign = (result & 0x80000000) >> 31;
                            cpsr.zero = !result;
                            break;
                        }
                        case 0x9:
                        {
                            printf("TEQ\n");
                            int rn = (opcode >> 16) & 0xf;
                            u32 shift_operand = get_shift_operand(true);

                            u32 result = r[rn] ^ shift_operand;

                            cpsr.sign = (result & 0x80000000) >> 31;
                            cpsr.zero = !result;
                            break;
                        }
                        case 0xa:
                        {
                            printf("CMP\n");
                            int rn = (opcode >> 16) & 0xf;
                            u32 shift_operand = get_shift_operand(true);
                            u64 result64 = (u64)r[rn] - shift_operand;
                            u32 result = (u32)result64;

                            cpsr.carry = ((r[rn] >> 31) & ~(shift_operand >> 31)) | ((r[rn] >> 31) & ~(result >> 31)) | (~(shift_operand >> 31) & ~(result >> 31));
                            cpsr.overflow = ((r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000) >> 31;
                            cpsr.sign = (result & 0x80000000) >> 31;
                            cpsr.zero = !result;
                            break;
                        }
                        case 0xb:
                        {
                            printf("CMN\n");
                            int rn = (opcode >> 16) & 0xf;
                            u32 shift_operand = get_shift_operand(true);
                            u64 result64 = (u64)r[rn] + shift_operand;
                            u32 result = (u32)result64;

                            cpsr.carry = result64 & 0x100000000ULL;
                            cpsr.overflow = (~(r[rn] ^ shift_operand) & (r[rn] ^ result) & 0x80000000);
                            cpsr.sign = (result & 0x80000000) >> 31;
                            cpsr.zero = !result;
                            break;
                        }
                        case 0xc:
                        {
                            printf("ORR\n");
                            int rd = (opcode >> 12) & 0xf;
                            int rn = (opcode >> 16) & 0xf;
                            bool s = (opcode >> 20) & 1;
                            u32 shift_operand = get_shift_operand(s && (rd != 15));

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
                                    case mode_fiq:
                                        cpsr = spsr_fiq;
                                        break;
                                    case mode_irq:
                                        cpsr = spsr_irq;
                                        break;
                                    case mode_supervisor:
                                        cpsr = spsr_svc;
                                        break;
                                    case mode_abort:
                                        cpsr = spsr_abt;
                                        break;
                                    case mode_undefined:
                                        cpsr = spsr_und;
                                        break;
                                    }
                                }
                            }
                            break;
                        }
                        case 0xd:
                        {
                            printf("MOV\n");
                            if(r[15] == 0x000001c4)
                            {
                                printf("NOT ANOTHER FUCKING BRANCHING ERROR\n");
                            }
                            int rm = opcode & 0xf;
                            int rd = (opcode >> 12) & 0xf;
                            bool s = (opcode >> 20) & 1;
                            u32 shift_operand = get_shift_operand(s && (rd != 15));

                            r[rd] = shift_operand;

                            if(rd == 15)
                            {
                                just_branched = true;
                            }

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
                                    case mode_fiq:
                                        cpsr = spsr_fiq;
                                        break;
                                    case mode_irq:
                                        cpsr = spsr_irq;
                                        break;
                                    case mode_supervisor:
                                        cpsr = spsr_svc;
                                        break;
                                    case mode_abort:
                                        cpsr = spsr_abt;
                                        break;
                                    case mode_undefined:
                                        cpsr = spsr_und;
                                        break;
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
                            u32 shift_operand = get_shift_operand(s && (rd != 15));

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
                                    case mode_fiq:
                                        cpsr = spsr_fiq;
                                        break;
                                    case mode_irq:
                                        cpsr = spsr_irq;
                                        break;
                                    case mode_supervisor:
                                        cpsr = spsr_svc;
                                        break;
                                    case mode_abort:
                                        cpsr = spsr_abt;
                                        break;
                                    case mode_undefined:
                                        cpsr = spsr_und;
                                        break;
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
                            u32 shift_operand = get_shift_operand(s && (rd != 15));

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
                                    case mode_fiq:
                                        cpsr = spsr_fiq;
                                        break;
                                    case mode_irq:
                                        cpsr = spsr_irq;
                                        break;
                                    case mode_supervisor:
                                        cpsr = spsr_svc;
                                        break;
                                    case mode_abort:
                                        cpsr = spsr_abt;
                                        break;
                                    case mode_undefined:
                                        cpsr = spsr_und;
                                        break;
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
            case 2:
            case 3:
            {
                if((((opcode >> 4) & 1) && ((opcode >> 25) & 1)) && (type >= arm_type::arm11))
                {
                    //printf("Unknown media!\n");
                    tick_media(opcode);
                }
                else
                {
                    if((opcode >> 20) & 1)
                    {
                        if((opcode >> 22) & 1)
                        {
                            printf("LDRB\n");
                            int rd = (opcode >> 12) & 0xf;
                            u32 addr = get_load_store_addr();
                            u32 data = rw(addr);
                            data >>= ((addr & 3) << 3);
                            data &= 0xff;
                            r[rd] = data;
                        }
                        else
                        {
                            printf("LDR\n");
                            int rd = (opcode >> 12) & 0xf;
                            u32 addr = get_load_store_addr();
                            u32 data = rw(addr & 0xfffffffc);
                            if(!cp15.control_arm11.unaligned_access_enable && ((addr & 3) != 0) && (type == arm_type::arm11))
                            {
                                data = (data >> ((addr & 3) << 3)) | (data << (32 - ((addr & 3) << 3)));
                            }
                            if(cp15.control_arm11.strict_alignment && ((addr & 3) != 0) && (type == arm_type::arm11))
                            {
                                data_abort = true;
                                return;
                            }

                            r[rd] = data;

                            if(rd == 15)
                            {
                                cpsr.thumb = data & 1;
                                r[15] &= 0xfffffffe;
                                just_branched = true;
                            }
                        }
                    }
                    else
                    {
                        if((opcode >> 22) & 1)
                        {
                            printf("STRB\n");
                            int rd = (opcode >> 12) & 0xf;
                            u32 addr = get_load_store_addr();
                            u32 data = rw(addr);
                            data &= ~(0xff << ((addr & 3) << 3));
                            data |= (r[rd] & 0xff) << ((addr & 3) << 3);
                            ww(addr, data);
                        }
                        else
                        {
                            printf("STR\n");
                            int rd = (opcode >> 12) & 0xf;
                            u32 addr = get_load_store_addr();
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

                    if(condition)
                    {
                        arm_mode oldmode = cpsr.mode;
                        if(s && !pc) cpsr.mode = mode_user;
                        u32 addr = get_load_store_multi_addr() & 0xfffffffc;
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
                            just_branched = true;
                            cpsr.thumb = data & 1;
                            if(s)
                            {
                                switch(cpsr.mode)
                                {
                                case mode_fiq:
                                    cpsr = spsr_fiq;
                                    break;
                                case mode_irq:
                                    cpsr = spsr_irq;
                                    break;
                                case mode_supervisor:
                                    cpsr = spsr_svc;
                                    break;
                                case mode_abort:
                                    cpsr = spsr_abt;
                                    break;
                                case mode_undefined:
                                    cpsr = spsr_und;
                                    break;
                                }
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
                    if(s) cpsr.mode = mode_user;
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

                if(condition)
                {
                    if((opcode >> 24) & 1)
                    {
                        r[14] = r[15] - 4;
                        just_branched = true;
                    }
                    r[15] += addr << 2;
                    just_branched = true;
                }
                break;
            }
            case 6:
            case 7:
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

                                if(cp_index == 10)
                                {
                                    switch(cp15.coprocessor_access_control.cp10)
                                    {
                                        case cp15_coprocessor_no_access:
                                        case cp15_coprocessor_reserved:
                                        {
                                            undefined = true;
                                            return;
                                        }
                                        case cp15_coprocessor_privileged_only:
                                        {
                                            switch(cpsr.mode)
                                            {
                                                case mode_user:
                                                case mode_fiq:
                                                case mode_irq:
                                                case mode_abort:
                                                {
                                                    undefined = true;
                                                    return;
                                                }
                                                case mode_supervisor:
                                                case mode_monitor:
                                                case mode_system:
                                                {
                                                    break;
                                                }
                                            }
                                            break;
                                        }
                                        case cp15_coprocessor_privileged_and_user: break;
                                    }
                                    int vfp_reg_select = (opcode >> 7) & 1;
                                    switch(opcode1)
                                    {
                                        case 0:
                                        {
                                            printf("FMRS\n");
                                            r[rd] = vfp_r[CRn].w[vfp_reg_select];
                                            break;
                                        }
                                        case 7:
                                        {
                                            printf("FMRX\n");
                                            switch(type)
                                            {
                                                case arm_type::arm11:
                                                {
                                                    if(!fpexc.vfp_enable)
                                                    {
                                                        switch(cpsr.mode)
                                                        {
                                                        case mode_user:
                                                        case mode_fiq:
                                                        case mode_irq:
                                                        case mode_abort:
                                                        {
                                                            undefined = true;
                                                            return;
                                                        }
                                                        case mode_supervisor:
                                                        case mode_monitor:
                                                        case mode_undefined:
                                                        case mode_system:
                                                        {
                                                            break;
                                                        }
                                                        }
                                                    }
                                                    switch(CRn)
                                                    {
                                                        case 0:
                                                        {
                                                            r[rd] = fpsid.whole;
                                                            break;
                                                        }
                                                        case 1:
                                                        {
                                                            r[rd] = fpscr.whole;
                                                            break;
                                                        }
                                                        case 6:
                                                        {
                                                            r[rd] = mvfr1_arm11.whole;
                                                            break;
                                                        }
                                                        case 7:
                                                        {
                                                            r[rd] = mvfr0.whole;
                                                            break;
                                                        }
                                                        case 8:
                                                        {
                                                            switch(cpsr.mode)
                                                            {
                                                            case mode_user:
                                                            case mode_fiq:
                                                            case mode_irq:
                                                            case mode_abort:
                                                            {
                                                                undefined = true;
                                                                return;
                                                            }
                                                            case mode_supervisor:
                                                            case mode_monitor:
                                                            case mode_system:
                                                            case mode_undefined:
                                                            {
                                                                break;
                                                            }
                                                            }
                                                            r[rd] = fpexc.whole;
                                                            break;
                                                        }
                                                        case 9:
                                                        {
                                                            switch(cpsr.mode)
                                                            {
                                                            case mode_user:
                                                            case mode_fiq:
                                                            case mode_irq:
                                                            case mode_abort:
                                                            {
                                                                undefined = true;
                                                                return;
                                                            }
                                                            case mode_supervisor:
                                                            case mode_monitor:
                                                            case mode_system:
                                                            case mode_undefined:
                                                            {
                                                                break;
                                                            }
                                                            }
                                                            r[rd] = fpinst;
                                                            break;
                                                        }
                                                        case 10:
                                                        {
                                                            switch(cpsr.mode)
                                                            {
                                                            case mode_user:
                                                            case mode_fiq:
                                                            case mode_irq:
                                                            case mode_abort:
                                                            {
                                                                undefined = true;
                                                                return;
                                                            }
                                                            case mode_supervisor:
                                                            case mode_monitor:
                                                            case mode_system:
                                                            {
                                                                break;
                                                            }
                                                            }
                                                            r[rd] = fpinst2;
                                                            break;
                                                        }
                                                    }
                                                    break;
                                                }
                                                case arm_type::cortex_a8:
                                                {
                                                    switch(CRn)
                                                    {
                                                        case 0:
                                                        {
                                                            switch(cpsr.mode)
                                                            {
                                                            case mode_user:
                                                            case mode_fiq:
                                                            case mode_irq:
                                                            case mode_abort:
                                                            {
                                                                undefined = true;
                                                                return;
                                                            }
                                                            case mode_supervisor:
                                                            case mode_monitor:
                                                            case mode_undefined:
                                                            case mode_system:
                                                            {
                                                                break;
                                                            }
                                                            }
                                                            r[rd] = fpsid.whole;
                                                            break;
                                                        }
                                                        case 1:
                                                        {
                                                            if(!fpexc.vfp_enable)
                                                            {
                                                                undefined = true;
                                                                return;
                                                            }
                                                            r[rd] = fpscr.whole;
                                                            break;
                                                        }
                                                        case 6:
                                                        {
                                                            switch(cpsr.mode)
                                                            {
                                                            case mode_user:
                                                            case mode_fiq:
                                                            case mode_irq:
                                                            case mode_abort:
                                                            {
                                                                undefined = true;
                                                                return;
                                                            }
                                                            case mode_supervisor:
                                                            case mode_monitor:
                                                            case mode_undefined:
                                                            case mode_system:
                                                            {
                                                                break;
                                                            }
                                                            }
                                                            r[rd] = mvfr1_cortex_a8.whole;
                                                            break;
                                                        }
                                                        case 7:
                                                        {
                                                            switch(cpsr.mode)
                                                            {
                                                            case mode_user:
                                                            case mode_fiq:
                                                            case mode_irq:
                                                            case mode_abort:
                                                            {
                                                                undefined = true;
                                                                return;
                                                            }
                                                            case mode_supervisor:
                                                            case mode_monitor:
                                                            case mode_undefined:
                                                            case mode_system:
                                                            {
                                                                break;
                                                            }
                                                            }
                                                            r[rd] = mvfr0.whole;
                                                            break;
                                                        }
                                                        case 8:
                                                        {
                                                            switch(cpsr.mode)
                                                            {
                                                            case mode_user:
                                                            case mode_fiq:
                                                            case mode_irq:
                                                            case mode_abort:
                                                            {
                                                                undefined = true;
                                                                return;
                                                            }
                                                            case mode_supervisor:
                                                            case mode_monitor:
                                                            case mode_system:
                                                            case mode_undefined:
                                                            {
                                                                break;
                                                            }
                                                            }
                                                            r[rd] = fpexc.whole;
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
                                if(cp_index == 11)
                                {
                                    switch(cp15.coprocessor_access_control.cp11)
                                    {
                                        case cp15_coprocessor_no_access:
                                        case cp15_coprocessor_reserved:
                                        {
                                            undefined = true;
                                            return;
                                        }
                                        case cp15_coprocessor_privileged_only:
                                        {
                                            switch(cpsr.mode)
                                            {
                                                case mode_user:
                                                case mode_fiq:
                                                case mode_irq:
                                                case mode_abort:
                                                    undefined = true;
                                                    return;
                                                case mode_supervisor:
                                                case mode_monitor:
                                                case mode_system:
                                                    break;
                                            }
                                            break;
                                        }
                                        case cp15_coprocessor_privileged_and_user: break;
                                    }
                                    switch(opcode1)
                                    {
                                    case 0:
                                    {
                                        printf("FMRDL\n");
                                        r[rd] = vfp_r[CRn].w[0];
                                        break;
                                    }
                                    case 1:
                                    {
                                        printf("FMRDH\n");
                                        r[rd] = vfp_r[CRn].w[1];
                                        break;
                                    }
                                    }
                                }
                                else if(cp_index == 15)
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

                                if(cp_index == 10)
                                {
                                    switch(cp15.coprocessor_access_control.cp10)
                                    {
                                        case cp15_coprocessor_no_access:
                                        case cp15_coprocessor_reserved:
                                        {
                                            undefined = true;
                                            return;
                                        }
                                        case cp15_coprocessor_privileged_only:
                                        {
                                            switch(cpsr.mode)
                                            {
                                                case mode_user:
                                                case mode_fiq:
                                                case mode_irq:
                                                case mode_abort:
                                                    undefined = true;
                                                    return;
                                                case mode_supervisor:
                                                case mode_monitor:
                                                case mode_system:
                                                    break;
                                            }
                                            break;
                                        }
                                        case cp15_coprocessor_privileged_and_user: break;
                                    }
                                    int vfp_reg_select = (opcode >> 7) & 1;
                                    switch(opcode1)
                                    {
                                        case 0:
                                        {
                                            printf("FMSR\n");
                                            vfp_r[crn].w[vfp_reg_select] = r[rd];
                                            break;
                                        }
                                        case 7:
                                        {
                                            printf("FMXR\n");
                                            switch(type)
                                            {
                                                case arm_type::arm11:
                                                {
                                                    if(!fpexc.vfp_enable)
                                                    {
                                                        switch(cpsr.mode)
                                                        {
                                                        case mode_user:
                                                        case mode_fiq:
                                                        case mode_irq:
                                                        case mode_abort:
                                                        {
                                                            undefined = true;
                                                            return;
                                                        }
                                                        case mode_supervisor:
                                                        case mode_monitor:
                                                        case mode_undefined:
                                                        case mode_system:
                                                        {
                                                            break;
                                                        }
                                                        }
                                                    }
                                                    switch(crn)
                                                    {
                                                        case 1:
                                                        {
                                                            fpscr.whole = r[rd];
                                                            break;
                                                        }
                                                        case 8:
                                                        {
                                                            switch(cpsr.mode)
                                                            {
                                                            case mode_user:
                                                            case mode_fiq:
                                                            case mode_irq:
                                                            case mode_abort:
                                                            {
                                                                undefined = true;
                                                                return;
                                                            }
                                                            case mode_supervisor:
                                                            case mode_monitor:
                                                            case mode_system:
                                                            case mode_undefined:
                                                            {
                                                                break;
                                                            }
                                                            }
                                                            fpexc.whole = r[rd];
                                                            break;
                                                        }
                                                        case 9:
                                                        {
                                                            switch(cpsr.mode)
                                                            {
                                                            case mode_user:
                                                            case mode_fiq:
                                                            case mode_irq:
                                                            case mode_abort:
                                                            {
                                                                undefined = true;
                                                                return;
                                                            }
                                                            case mode_supervisor:
                                                            case mode_monitor:
                                                            case mode_system:
                                                            case mode_undefined:
                                                            {
                                                                break;
                                                            }
                                                            }
                                                            fpinst = r[rd];
                                                            break;
                                                        }
                                                        case 10:
                                                        {
                                                            switch(cpsr.mode)
                                                            {
                                                            case mode_user:
                                                            case mode_fiq:
                                                            case mode_irq:
                                                            case mode_abort:
                                                            {
                                                                undefined = true;
                                                                return;
                                                            }
                                                            case mode_supervisor:
                                                            case mode_monitor:
                                                            case mode_system:
                                                            {
                                                                break;
                                                            }
                                                            }
                                                            fpinst2 = r[rd];
                                                            break;
                                                        }
                                                    }
                                                    break;
                                                }
                                                case arm_type::cortex_a8:
                                                {
                                                    switch(crn)
                                                    {
                                                        case 1:
                                                        {
                                                            if(!fpexc.vfp_enable)
                                                            {
                                                                undefined = true;
                                                                return;
                                                            }
                                                            fpscr.whole = r[rd];
                                                            break;
                                                        }
                                                        case 8:
                                                        {
                                                            switch(cpsr.mode)
                                                            {
                                                            case mode_user:
                                                            case mode_fiq:
                                                            case mode_irq:
                                                            case mode_abort:
                                                            {
                                                                undefined = true;
                                                                return;
                                                            }
                                                            case mode_supervisor:
                                                            case mode_monitor:
                                                            case mode_system:
                                                            case mode_undefined:
                                                            {
                                                                break;
                                                            }
                                                            }
                                                            fpexc.whole = r[rd];
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
                                if(cp_index == 11)
                                {
                                    switch(cp15.coprocessor_access_control.cp11)
                                    {
                                        case cp15_coprocessor_no_access:
                                        case cp15_coprocessor_reserved:
                                        {
                                            undefined = true;
                                            return;
                                        }
                                        case cp15_coprocessor_privileged_only:
                                        {
                                            switch(cpsr.mode)
                                            {
                                                case mode_user:
                                                case mode_fiq:
                                                case mode_irq:
                                                case mode_abort:
                                                {
                                                    undefined = true;
                                                    return;
                                                }
                                                case mode_supervisor:
                                                case mode_monitor:
                                                case mode_system:
                                                {
                                                    break;
                                                }
                                            }
                                            break;
                                        }
                                        case cp15_coprocessor_privileged_and_user: break;
                                    }
                                    switch(opcode1)
                                    {
                                    case 0:
                                    {
                                        printf("FMDLR\n");
                                        vfp_r[crn].w[0] = r[rd];
                                        break;
                                    }
                                    case 1:
                                    {
                                        printf("FMDHR\n");
                                        vfp_r[crn].w[1] = r[rd];
                                        break;
                                    }
                                    }
                                }
                                if(cp_index == 15) cp15.write(opcode1, opcode2, crn, crm, r[rd]);
                            }
                        }
                        else printf("CDP\n");
                    }
                }
                else if(type >= arm_type::arm9)
                {
                    if((opcode >> 20) & 1) printf("MRRC\n");
                    else printf("MCRR\n");
                }
                break;
            }
            }
        }
        if(((opcode >> 28) == 0xf) && (type >= arm_type::arm9))
        {
            switch((opcode >> 26) & 3)
            {
            case 0:
            {
                if((opcode >> 16) & 1)
                {
                    printf("SETEND\n");
                    cpsr.endianness = (opcode >> 9) & 1;
                }
                else
                {
                    printf("CPS\n");
                    int imod = (opcode >> 18) & 3;
                    int mmod = (opcode >> 17) & 1;
                    int mode = opcode & 0x1f;
                    int a = (opcode >> 8) & 1;
                    int i = (opcode >> 7) & 1;
                    int f = (opcode >> 6) & 1;

                    if(imod & 2)
                    {
                        if(a) cpsr.abort_disable = imod & 1;
                        if(i) cpsr.irq_disable = imod & 1;
                        if(f) cpsr.fiq_disable = imod & 1;
                    }
                    else if(mmod)
                    {
                        arm_mode old_mode = cpsr.mode;

                        cpsr.mode = (arm_mode)mode;

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
                    }
                }
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
                    cpsr.thumb = true;
                    just_branched = true;
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
    }
    else
    {
        bool old_just_branched = just_branched;
        opcode = next_opcode;
        next_opcode = rw(r[15]);
        if(r[15] & 2) next_opcode >>= 16;
        else next_opcode &= 0xffff;
        r[15] += 2;
        u32 opcode_2 = 0;
        just_branched = false;
        if((((opcode >> 13) & 7) == 7) && (type == arm_type::cortex_a8))
        {
            if((opcode >> 11) & 3)
            {
                opcode_2 = rw((r[15] + 2) & 0xfffffffc);
                if((r[15] + 2) & 2) opcode_2 >>= 16;
                else opcode_2 &= 0xffff;
                printf("Opcode:%04x\nOpcode 2:%04x\nPC:%08x\nLR:%08x\nSP:%08x\nR0:%08x\nR1:%08x\nR2:%08x\nR12:%08x\nCPSR:%08x\n", opcode, opcode_2, true_r15, r[14], r[13], r[0], r[1], r[2], r[12], cpsr.whole);
            }
            else printf("Opcode:%04x\nPC:%08x\nLR:%08x\nSP:%08x\nR0:%08x\nR1:%08x\nR2:%08x\nR3:%08x\nR4:%08x\nR6:%08x\nR12:%08x\nCPSR:%08x\n", opcode, true_r15, r[14], r[13], r[0], r[1], r[2], r[3], r[4], r[6], r[12], cpsr.whole);
        }
#undef printf
        else printf("Opcode:%04x\nPC:%08x\n", opcode, true_r15);
        for(int i = 0; i < 15; i++)
        {
            printf("R%d:%08x\n", i, r[i]);
        }
#define printf(...)
        switch((opcode >> 13) & 7)
        {
        case 0:
        {
            switch((opcode >> 11) & 3)
            {
            case 0:
            {
                printf("Thumb LSL\n");
                u32 imm = (opcode >> 6) & 0x1f;
                int rm = (opcode >> 3) & 7;
                int rd = opcode & 7;

                if(imm == 0)
                {
                    r[rd] = r[rm];
                }
                else
                {
                    cpsr.carry = r[rm] & (1 << (32 - imm));
                    r[rd] = r[rm] << imm;
                }
                cpsr.sign = r[rd] & (1 << 31);
                cpsr.zero = r[rd] == 0;
                break;
            }
            case 1:
            {
                printf("Thumb LSR\n");
                u32 imm = (opcode >> 6) & 0x1f;
                int rm = (opcode >> 3) & 7;
                int rd = opcode & 7;

                if(imm == 0)
                {
                    cpsr.carry = r[rm] & (1 << 31);
                    r[rd] = 0;
                }
                else
                {
                    cpsr.carry = r[rm] & (1 << (imm - 1));
                    r[rd] = r[rm] >> imm;
                }
                cpsr.sign = r[rd] & (1 << 31);
                cpsr.zero = r[rd] == 0;
                break;
            }
            case 2:
            {
                printf("Thumb ASR\n");
                u32 imm = (opcode >> 6) & 0x1f;
                int rm = (opcode >> 3) & 7;
                int rd = opcode & 7;

                if(!imm)
                {
                    cpsr.carry = r[rm] & 0x80000000;
                    if(!(r[rm] & 0x80000000)) r[rd] = 0;
                    else r[rd] = 0xffffffff;
                }
                else
                {
                    cpsr.carry = r[rm] & (1 << (imm - 1));
                    r[rd] = (s32)r[rm] >> imm;
                }
                cpsr.sign = r[rd] & (1 << 31);
                cpsr.zero = r[rd] == 0;
                break;
            }
            case 3:
            {
                if((opcode >> 9) & 1)
                {
                    if((opcode >> 10) & 1) printf("Thumb SUB\n");
                    else printf("Thumb SUB_3\n");
                }
                else
                {
                    if((opcode >> 10) & 1)
                    {
                        printf("Thumb ADD\n");
                        int rd = opcode & 7;
                        int rn = (opcode >> 3) & 7;
                        u32 imm = (opcode >> 6) & 7;
                        u64 result64 = (u64)r[rn] + imm;
                        r[rd] = (u32)result64;
                        cpsr.zero = r[rd] == 0;
                        cpsr.sign = r[rd] & 0x80000000;
                        cpsr.carry = result64 & 0x100000000ULL;
                        cpsr.overflow = (~(r[rn] ^ imm) & (r[rn] ^ r[rd]) & 0x80000000);
                    }
                    else
                    {
                        printf("Thumb ADD_3\n");
                        int rm = (opcode >> 6) & 7;
                        int rn = (opcode >> 3) & 7;
                        int rd = opcode & 7;
                        u64 result64 = (u64)r[rn] + r[rm];

                        r[rd] = (u32)result64;
                        cpsr.sign = r[rd] & 0x80000000;
                        cpsr.zero = r[rd] == 0;
                        cpsr.carry = result64 & 0x100000000ULL;
                        cpsr.overflow = (~(r[rn] ^ r[rm]) & (r[rn] ^ r[rd]) & 0x80000000);
                    }
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
            case 0:
            {
                printf("Thumb MOV\n");
                u8 imm = opcode & 0xff;
                int rd = (opcode >> 8) & 7;
                r[rd] = imm;
                if(!r[rd]) cpsr.zero = true;
                else cpsr.zero = false;
                if(r[rd] & 0x80000000) cpsr.sign = true;
                else cpsr.sign = false;
                break;
            }
            case 1:
            {
                printf("Thumb CMP\n");
                u32 imm = opcode & 0xff;
                int rn = (opcode >> 8) & 7;
                u64 result64 = (u64)r[rn] - imm;
                u32 result = (u32)result64;
                cpsr.sign = result & (1 << 31);
                cpsr.zero = result == 0;
                cpsr.carry = result64 < 0x100000000LL;
                cpsr.overflow = (~(r[rn] ^ imm) & (r[rn] ^ result) & 0x80000000);
                break;
            }
            case 2:
            {
                printf("Thumb ADD_2\n");
                int rd = (opcode >> 8) & 7;
                u32 imm = opcode & 0xff;
                u64 result64 = r[rd] + imm;
                u32 oldrd = r[rd];
                r[rd] = (u32)result64;

                cpsr.sign = r[rd] & 0x80000000;
                cpsr.zero = r[rd] == 0;
                cpsr.carry = result64 & 0x100000000ULL;
                cpsr.overflow = (~(oldrd ^ imm) & (oldrd ^ r[rd]) & 0x80000000);
                break;
            }
            case 3:
            {
                printf("Thumb SUB_2\n");
                int rd = (opcode >> 8) & 7;
                u32 imm = opcode & 0xff;
                u64 result64 = r[rd] - imm;
                u32 oldrd = r[rd];
                r[rd] = (u32)result64;

                cpsr.sign = r[rd] & 0x80000000;
                cpsr.zero = r[rd] == 0;
                cpsr.carry = result64 < 0x100000000ULL;
                cpsr.overflow = (~(oldrd ^ imm) & (oldrd ^ r[rd]) & 0x80000000);
                break;
            }
            }
            break;
        }
        case 2:
        {
            if((opcode >> 12) & 1)
            {
                switch((opcode >> 9) & 7)
                {
                case 0:
                {
                    printf("Thumb STR_2\n");
                    break;
                }
                case 1:
                {
                    printf("Thumb STRH_2\n");
                    break;
                }
                case 2:
                {
                    printf("Thumb STRB_2\n");
                    int rd = opcode & 7;
                    int rn = (opcode >> 3) & 7;
                    int rm = (opcode >> 6) & 7;
                    
                    u32 addr = r[rn] + r[rm];

                    u32 data = rw(addr);
                    data &= ~(0xff << ((addr & 3) << 3));
                    data |= (r[rd] & 0xff) << ((addr & 3) << 3);
                    ww(addr, data);
                    break;
                }
                case 3:
                {
                    printf("Thumb LDRSB\n");
                    break;
                }
                case 4:
                {
                    printf("Thumb LDR_2\n");
                    int rd = opcode & 7;
                    int rn = (opcode >> 3) & 7;
                    int rm = (opcode >> 6) & 7;

                    u32 addr = r[rn] + r[rm];
                    u32 data = rw(addr & 0xfffffffc);
                    r[rd] = (data >> ((addr & 3) << 3)) | (data << (32 - ((addr & 3) << 3)));
                    break;
                }
                case 5:
                {
                    printf("Thumb LDRH_2\n");
                    break;
                }
                case 6:
                {
                    printf("Thumb LDRB_2\n");
                    int rm = (opcode >> 6) & 7;
                    int rn = (opcode >> 3) & 7;
                    int rd = opcode & 7;
                    u32 addr = r[rm] + r[rn];
                    u32 data = rw(addr);
                    data >>= ((addr & 3) << 3);
                    data &= 0xff;
                    r[rd] = data;
                    break;
                }
                case 7:
                {
                    printf("Thumb LDRSH\n");
                    break;
                }
                }
            }
            else
            {
                if((opcode >> 11) & 1)
                {
                    printf("Thumb LDR_3\n");
                    int rd = (opcode >> 8) & 7;
                    u32 imm = opcode & 0xff;
                    u32 addr = (r[15] & 0xfffffffc) + (imm << 2);
                    u32 data = rw(addr & 0xfffffffc);
                    r[rd] = (data >> ((addr & 3) << 3)) | (data << (32 - ((addr & 3) << 3)));
                }
                else
                {
                    if((opcode >> 10) & 1)
                    {
                        switch((opcode >> 8) & 3)
                        {
                        case 0:
                        {
                            printf("Thumb ADD_4\n");
                            int h1 = (opcode >> 7) & 1;
                            int h2 = (opcode >> 6) & 1;
                            int rm = ((opcode >> 3) & 7) | (h2 << 3);
                            int rd = (opcode & 7) | (h1 << 3);
                            r[rd] += r[rm];
                            break;
                        }
                        case 1:
                        {
                            printf("Thumb CMP_3\n");
                            int h1 = (opcode >> 7) & 1;
                            int h2 = (opcode >> 6) & 1;
                            int rm = ((opcode >> 3) & 7) | (h2 << 3);
                            int rn = (opcode & 7) | (h1 << 3);
                            u64 result64 = (u64)r[rn] - r[rm];
                            u32 result = (u32)result64;
                            cpsr.sign = result & (1 << 31);
                            cpsr.zero = result == 0;
                            cpsr.carry = r[rn] < r[rm];
                            cpsr.overflow = (~(r[rn] ^ r[rm]) & (r[rn] ^ result) & 0x80000000);
                            break;
                        }
                        case 2:
                        {
                            printf("Thumb MOV_3\n");
                            int h1 = (opcode >> 7) & 1;
                            int h2 = (opcode >> 6) & 1;
                            int rd = (opcode & 7) | (h1 << 3);
                            int rm = ((opcode >> 3) & 7) | (h2 << 3);
                            r[rd] = r[rm];
                            break;
                        }
                        case 3:
                        {
                            if(((opcode >> 7) & 1) && (type >= arm_type::arm9))
                            {
                                printf("Thumb BLX_2\n");
                                int rm = (opcode >> 3) & 7;

                                r[14] = (r[15] + 2) | 1;
                                r[15] = r[rm] & 0xfffffffe;
                                cpsr.thumb = r[rm] & 1;
                                just_branched = true;
                            }
                            else
                            {
                                printf("Thumb BX\n");
                                int rm = (opcode >> 3) & 0xf;
                                if(rm != 15)
                                {
                                    cpsr.thumb = r[rm] & 1;
                                    r[15] = r[rm] & 0xfffffffe;
                                    just_branched = true;
                                }
                                else
                                {
                                    cpsr.thumb = r[15] & 1;
                                    u32 oldpc = r[15];
                                    r[15] = r[15] & 0xfffffffe;
                                    just_branched = true;
                                }
                                just_branched = true;
                            }
                            break;
                        }
                        }
                    }
                    else
                    {
                        switch((opcode >> 6) & 0xf)
                        {
                        case 0x0:
                        {
                            printf("Thumb AND\n");
                            int rm = (opcode >> 3) & 7;
                            int rd = opcode & 7;
                            r[rd] &= r[rm];
                            cpsr.sign = r[rd] & 0x80000000;
                            cpsr.zero = r[rd] == 0;
                            break;
                        }
                        case 0x1:
                        {
                            printf("Thumb EOR\n");
                            int rm = (opcode >> 3) & 7;
                            int rd = opcode & 7;
                            r[rd] ^= r[rm];
                            cpsr.sign = r[rd] & 0x80000000;
                            cpsr.zero = r[rd] == 0;
                            break;
                        }
                        case 0x2:
                        {
                            printf("Thumb LSL_2\n");
                            int rs = (opcode >> 3) & 7;
                            int rd = opcode & 7;

                            if((r[rs] & 0xff) < 32)
                            {
                                cpsr.carry = r[rd] & (1 << (32 - (r[rs] & 0xff)));
                                r[rd] = r[rd] << (r[rs] & 0xff);
                            }
                            else if((r[rs] & 0xff) == 32)
                            {
                                cpsr.carry = r[rd] & 1;
                                r[rd] = 0;
                            }
                            else
                            {
                                cpsr.carry = false;
                                r[rd] = 0;
                            }
                            cpsr.sign = r[rd] & (1 << 31);
                            cpsr.zero = r[rd] == 0;
                            break;
                        }
                        case 0x3:
                        {
                            printf("Thumb LSR_2\n");
                            int rs = (opcode >> 3) & 7;
                            int rd = opcode & 7;

                            if((r[rs] & 0xff) < 32)
                            {
                                cpsr.carry = r[rd] & (1 << ((r[rs] & 0xff) - 1));
                                r[rd] = r[rd] >> (r[rs] & 0xff);
                            }
                            else if((r[rs] & 0xff) == 32)
                            {
                                cpsr.carry = r[rd] & 0x80000000;
                                r[rd] = 0;
                            }
                            else
                            {
                                cpsr.carry = false;
                                r[rd] = 0;
                            }
                            cpsr.sign = r[rd] & (1 << 31);
                            cpsr.zero = r[rd] == 0;
                            break;
                        }
                        case 0x4:
                        {
                            printf("Thumb ASR_2\n");
                            int rs = (opcode >> 3) & 7;
                            int rd = opcode & 7;

                            if((r[rs] & 0xff) < 32)
                            {
                                cpsr.carry = r[rd] & (1 << ((r[rs] & 0xff) - 1));
                                r[rd] = (s32)r[rd] >> (r[rs] & 0xff);
                            }
                            else
                            {
                                cpsr.carry = r[rd] & 0x80000000;
                                if(!(r[rd] & 0x80000000)) r[rd] = 0;
                                else r[rd] = 0xffffffff;
                            }
                            cpsr.sign = r[rd] & (1 << 31);
                            cpsr.zero = r[rd] == 0;
                            break;
                        }
                        case 0x5:
                        {
                            printf("Thumb ADC\n");
                            int rm = (opcode >> 3) & 7;
                            int rd = opcode & 7;
                            u32 oldrd = r[rd];
                            u64 result64 = (u64)r[rd] + r[rm] + cpsr.carry;
                            r[rd] = (u32)result64;
                            cpsr.sign = r[rd] & 0x80000000;
                            cpsr.zero = r[rd] == 0;
                            cpsr.carry = result64 & 0x100000000ULL;
                            cpsr.overflow = (~(oldrd ^ r[rm]) & (oldrd ^ r[rd]) & 0x80000000);
                            break;
                        }
                        case 0x6:
                            printf("Thumb SBC\n");
                            break;
                        case 0x7:
                            printf("Thumb ROR\n");
                            break;
                        case 0x8:
                            printf("Thumb TST\n");
                            break;
                        case 0x9:
                            printf("Thumb NEG\n");
                            break;
                        case 0xa:
                        {
                            printf("Thumb CMP_2\n");
                            int rm = (opcode >> 3) & 7;
                            int rn = opcode & 7;
                            u64 result64 = (u64)r[rn] - r[rm];
                            u32 result = (u32)result64;
                            cpsr.sign = result & (1 << 31);
                            cpsr.zero = result == 0;
                            cpsr.carry = result64 < 0x100000000LL;
                            cpsr.overflow = (~(r[rn] ^ r[rm]) & (r[rn] ^ result) & 0x80000000);
                            break;
                        }
                        case 0xb:
                        {
                            printf("Thumb CMN\n");
                            int rm = (opcode >> 3) & 7;
                            int rn = opcode & 7;
                            u64 result64 = (u64)r[rn] + r[rm];
                            u32 result = (u32)result64;
                            cpsr.sign = (result & 0x80000000) >> 31;
                            cpsr.zero = result == 0;
                            cpsr.carry = result64 & 0x100000000ULL;
                            cpsr.overflow = (~(r[rn] ^ r[rm]) & (r[rn] ^ result) & 0x80000000);
                            break;
                        }
                        case 0xc:
                        {
                            printf("Thumb ORR\n");
                            int rm = (opcode >> 3) & 7;
                            int rd = opcode & 7;
                            r[rd] |= r[rm];
                            cpsr.sign = r[rd] & 0x80000000;
                            cpsr.zero = r[rd] == 0;
                            break;
                        }
                        case 0xd:
                            printf("Thumb MUL\n");
                            break;
                        case 0xe:
                        {
                            printf("Thumb BIC\n");
                            int rm = (opcode >> 3) & 7;
                            int rd = opcode & 7;
                            r[rd] &= ~r[rm];
                            cpsr.sign = r[rd] & 0x80000000;
                            cpsr.zero = r[rd] == 0;
                            break;
                        }
                        case 0xf:
                            printf("Thumb MVN\n");
                            break;
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
                if((opcode >> 12) & 1)
                {
                    printf("Thumb LDRB\n");
                    int rd = opcode & 7;
                    int rn = (opcode >> 3) & 7;
                    u32 imm = (opcode >> 6) & 0x1f;
                    u32 addr = r[rn] + imm;
                    u32 data = rw(addr);
                    data >>= ((addr & 3) << 3);
                    data &= 0xff;
                    r[rd] = data;
                }
                else
                {
                    printf("Thumb LDR\n");
                    int rd = opcode & 7;
                    int rn = (opcode >> 3) & 7;
                    u32 imm = (opcode >> 6) & 0x1f;

                    u32 addr = r[rn] + (imm << 2);
                    u32 data = rw(addr & 0xfffffffc);
                    r[rd] = (data >> ((addr & 3) << 3)) | (data << (32 - ((addr & 3) << 3)));
                }
            }
            else
            {
                if((opcode >> 12) & 1)
                {
                    printf("Thumb STRB\n");
                    int rd = opcode & 7;
                    int rn = (opcode >> 3) & 7;
                    u32 imm = (opcode >> 6) & 0x1f;

                    u32 addr = r[rn] + (imm << 2);
                    u32 data = rw(addr);
                    data &= ~(0xff << ((addr & 3) << 3));
                    data |= (r[rd] & 0xff) << ((addr & 3) << 3);
                    ww(addr, data);
                }
                else
                {
                    printf("Thumb STR\n");
                    int rd = opcode & 7;
                    int rn = (opcode >> 3) & 7;
                    u32 imm = (opcode >> 6) & 0x1f;

                    u32 addr = r[rn] + (imm << 2);
                    ww(addr, r[rd]);
                }
            }
            break;
        }
        case 4:
        {
            if((opcode >> 12) & 1)
            {
                if((opcode >> 11) & 1)
                {
                    printf("Thumb LDR_4\n");
                    int rd = (opcode >> 8) & 7;
                    u32 imm = opcode & 0xff;
                    u32 addr = r[13] + (imm << 2);
                    u32 data = rw(addr & 0xfffffffc);
                    r[rd] = (data >> ((addr & 3) << 3)) | (data << (32 - ((addr & 3) << 3)));
                }
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
                    if((opcode >> 7) & 1)
                    {
                        printf("Thumb SUB_4\n");
                        u32 imm = (opcode & 0x7f) << 2;
                        r[13] = r[13] - imm;
                    }
                    else
                    {
                        printf("Thumb ADD_7\n");
                        u32 imm = (opcode & 0x7f) << 2;
                        r[13] = r[13] + imm;
                    }
                    break;
                }
                case 1:
                {
                    if((opcode >> 11) & 1)
                    {
                        switch((opcode >> 6) & 3)
                        {
                        case 0:
                            printf("Thumb REV\n");
                            break;
                        case 1:
                            printf("Thumb REV16\n");
                            break;
                        case 3:
                            printf("Thumb REVSH\n");
                            break;
                        }
                    }
                    else
                    {
                        switch((opcode >> 6) & 3)
                        {
                        case 0:
                            printf("Thumb SXTH\n");
                            break;
                        case 1:
                            printf("Thumb SXTB\n");
                            break;
                        case 2:
                            printf("Thumb UXTH\n");
                            break;
                        case 3:
                            printf("Thumb UXTB\n");
                            break;
                        }
                    }
                    break;
                }
                case 2:
                {
                    if((opcode >> 11) & 1)
                    {
                        printf("Thumb POP\n");
                        for(int i = 0; i < 8; i++)
                        {
                            if(opcode & (1 << i))
                            {
                                r[i] = rw(r[13]);
                                r[13] += 4;
                            }
                        }

                        if((opcode >> 8) & 1)
                        {
                            u32 data = rw(r[13]);
                            r[15] = data & 0xfffffffe;
                            cpsr.thumb = data & 1;
                            r[13] += 4;
                            just_branched = true;
                        }
                    }
                    else
                    {
                        printf("Thumb PUSH\n");
                        if((opcode >> 8) & 1)
                        {
                            r[13] -= 4;
                            ww(r[13],r[14]);
                        }

                        for(int i = 7; i >= 0; i--)
                        {
                            if(opcode & (1 << i))
                            {
                                r[13] -= 4;
                                ww(r[13],r[i]);
                            }
                        }
                    }
                    break;
                }
                case 3:
                {
                    if((opcode >> 11) & 1)
                    {
                        printf("Thumb BKPT\n");
                        r14_abt = r[15] + 4;
                        spsr_abt.whole = cpsr.whole;
                        cpsr.mode = mode_abort;
                        cpsr.thumb = false;
                        cpsr.irq_disable = true;
                        cpsr.abort_disable = true;
                        switch(type)
                        {
                            case arm_type::arm11: cpsr.endianness = cp15.control_arm11.endian_on_exception; break;
                            case arm_type::cortex_a8: cpsr.endianness = cp15.control_cortex_a8.endian_on_exception; break;
                        }
                        switch(type)
                        {
                            case arm_type::arm11: r[15] = cp15.control_arm11.high_vectors ? 0xffff000c : 0x0c; break;
                            case arm_type::cortex_a8: r[15] = cp15.control_cortex_a8.high_vectors ? 0xffff000c : 0x0c; break;
                        }
                        just_branched = true;
                    }
                    else
                    {
                        if((opcode >> 5) & 1)
                        {
                            printf("Thumb CPS\n");
                            int imod = (opcode >> 4) & 1;
                            int a = (opcode >> 2) & 1;
                            int i = (opcode >> 1) & 1;
                            int f = opcode & 1;
                            if(cpsr.mode != mode_user)
                            {
                                if(a) cpsr.abort_disable = imod;
                                if(i) cpsr.irq_disable = imod;
                                if(f) cpsr.fiq_disable = imod;
                            }
                        }
                        else
                        {
                            printf("Thumb SETEND\n");
                            cpsr.endianness = (opcode >> 3) & 1;
                        }
                    }
                    break;
                }
                }
            }
            else
            {
                if((opcode >> 11) & 1)
                {
                    printf("Thumb ADD_6\n");
                    u32 imm = opcode & 0xff;
                    int rd = (opcode >> 8) & 7;
                    r[rd] = r[13] + (imm << 2);
                }
                else
                {
                    printf("Thumb ADD_5\n");
                    u32 imm = (opcode & 0xff) << 2;
                    int rd = (opcode >> 8) & 7;
                    r[rd] = (r[15] & 0xfffffffc) + imm;

                }
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
                    bool cond_met = false;
                    if(cond == 0xf)
                    {
                        printf("Thumb SWI\n");
                    }
                    else
                    {
                    switch(cond)
                    {
                    case 0x0:
                        cond_met = cpsr.zero;
                        break;
                    case 0x1:
                        cond_met = !cpsr.zero;
                        break;
                    case 0x2:
                        cond_met = cpsr.carry;
                        break;
                    case 0x3:
                        cond_met = !cpsr.carry;
                        break;
                    case 0x4:
                        cond_met = cpsr.sign;
                        break;
                    case 0x5:
                        cond_met = !cpsr.sign;
                        break;
                    case 0x6:
                        cond_met = cpsr.overflow;
                        break;
                    case 0x7:
                        cond_met = !cpsr.overflow;
                        break;
                    case 0x8:
                        cond_met = cpsr.carry && !cpsr.zero;
                        break;
                    case 0x9:
                        cond_met = !cpsr.carry || cpsr.zero;
                        break;
                    case 0xa:
                        cond_met = cpsr.sign == cpsr.overflow;
                        break;
                    case 0xb:
                        cond_met = cpsr.sign != cpsr.overflow;
                        break;
                    case 0xc:
                        cond_met = !cpsr.zero && (cpsr.sign == cpsr.overflow);
                        break;
                    case 0xd:
                        cond_met = cpsr.zero || (cpsr.sign != cpsr.overflow);
                        break;
                    case 0xe:
                        cond_met = true;
                        break;
                    }
                    if(cond_met)
                    {
                        r[15] = (u32)(r[15] + ((s32)(s8)imm << 1));
                        just_branched = true;
                    }
                    }
                }
            }
            else
            {
                if((opcode >> 11) & 1)
                {
                    printf("Thumb LDMIA\n");
                    u8 reg_list = opcode & 0xff;
                    int rn = (opcode >> 8) & 7;

                    u32 count = 0;
                    for(int i = 0; i < 8; i++)
                    {
                        if(reg_list & (1 << i)) count += 4;
                    }

                    u32 addr = r[rn];
                    r[rn] += count;

                    for(int i = 0; i < 8; i++)
                    {
                        if(reg_list & (1 << i))
                        {
                            r[i] = rw(addr);
                            addr += 4;
                        }
                    }
                }
                else
                {
                    printf("Thumb STMIA\n");
                    u8 reg_list = opcode & 0xff;
                    int rn = (opcode >> 8) & 7;

                    u32 count = 0;
                    for(int i = 0; i < 8; i++)
                    {
                        if(reg_list & (1 << i)) count++;
                    }

                    u32 addr = r[rn];
                    r[rn] += count << 2;

                    for(int i = 0; i < 8; i++)
                    {
                        if(reg_list & (1 << i))
                        {
                            ww(addr, r[i]);
                            addr += 4;
                        }
                    }
                }
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
                just_branched = true;
            }
            else if(type == arm_type::arm11)
            {
                printf("Thumb BLX\n");
                u32 imm = opcode & 0x7ff;
                u32 h = (opcode >> 11) & 3;

                if(h)
                {
                    switch(h)
                    {
                        case 1:
                        {
                            u32 oldpc = r[15];
                            r[15] = (r[14] + (imm << 1)) & 0xfffffffc;
                            r[14] = (oldpc - 2) | 1;
                            cpsr.thumb = false;
                            just_branched = true;
                            break;
                        }
                        case 2:
                        {
                            if(imm & 0x400) imm |= 0xfffff800;
                            r[14] = r[15] + (imm << 12);
                            break;
                        }
                        case 3:
                        {
                            u32 oldpc = r[15];
                            r[15] = (r[14] + (imm << 1));
                            r[14] = (oldpc - 2) | 1;
                            just_branched = true;
                            break;
                        }
                    }
                }
            }
            else if(type == arm_type::cortex_a8)
            {
                //32-bit thumb instruction
                switch((opcode >> 11) & 3)
                {
                    case 1:
                    {
                        break;
                    }
                    case 2:
                    {
                        if((opcode_2 >> 15) & 1)
                        {
                            switch((opcode_2 >> 12) & 7)
                            {
                                case 0:
                                {
                                    if(((opcode >> 7) & 7) != 7)
                                    {
                                        printf("Thumb2 B\n");
                                    }
                                    else
                                    {
                                        switch((opcode >> 5) & 0x3f)
                                        {
                                            case 0b011100:
                                            {
                                                printf("Thumb2 MSR\n");
                                                break;
                                            }
                                            case 0b011101:
                                            {
                                                if((opcode >> 4) & 1)
                                                {
                                                    switch((opcode_2 >> 4) & 0xf)
                                                    {
                                                        case 0:
                                                        {
                                                            printf("ThumbEE LEAVEX\n");
                                                            break;
                                                        }
                                                        case 1:
                                                        {
                                                            printf("ThumbEE ENTERX\n");
                                                            break;
                                                        }
                                                        case 2:
                                                        {
                                                            printf("Thumb CLREX\n");
                                                            break;
                                                        }
                                                        case 4:
                                                        {
                                                            printf("Thumb DSB\n");
                                                            break;
                                                        }
                                                        case 5:
                                                        {
                                                            printf("Thumb DMB\n");
                                                            break;
                                                        }
                                                        case 6:
                                                        {
                                                            printf("Thumb ISB\n");
                                                            break;
                                                        }
                                                    }
                                                }
                                                else
                                                {
                                                    if((opcode_2 >> 8) & 7) printf("Thumb2 CPS\n");
                                                    else
                                                    {
                                                        switch(opcode_2 & 0xff)
                                                        {
                                                            default:
                                                            case 0x00:
                                                            {
                                                                printf("Thumb2 NOP\n");
                                                                break;
                                                            }
                                                            case 0x01:
                                                               {
                                                                printf("Thumb YIELD\n");
                                                                break;
                                                            }
                                                            case 0x02:
                                                            {
                                                                printf("Thumb2 WFE\n");
                                                                break;
                                                            }
                                                            case 0x03:
                                                            {
                                                                printf("Thumb2 WFI\n");
                                                                break;
                                                            }
                                                            case 0x04:
                                                            {
                                                                printf("Thumb2 SEV\n");
                                                                break;
                                                            }
                                                            case 0xf0: case 0xf1: case 0xf2: case 0xf3:
                                                            case 0xf4: case 0xf5: case 0xf6: case 0xf7:
                                                            case 0xf8: case 0xf9: case 0xfa: case 0xfb:
                                                            case 0xfc: case 0xfd: case 0xfe: case 0xff:
                                                            {
                                                                printf("Thumb DBG\n");
                                                            }
                                                        }
                                                    }
                                                }
                                                break;
                                            }
                                            case 0b011110:
                                            {
                                                if(!((opcode >> 4) & 1)) printf("Thumb2 BXJ\n");
                                                else
                                                {
                                                    if(!(opcode_2 & 0xff)) printf("Thumb2 ERET\n");
                                                    else printf("Thumb2 SUBS PC, LR\n");
                                                }
                                                break;
                                            }
                                            case 0b011111:
                                            {
                                                printf("Thumb2 MRS\n");
                                                break;
                                            }
                                            case 0b111111:
                                            {
                                                if((opcode >> 4) & 1) printf("Thumb SMC\n");
                                                else printf("Thumb HVC\n");
                                                break;
                                            }
                                        }
                                    }
                                    break;
                                }
                                case 1: case 3:
                                {
                                    printf("Thumb2 B\n");
                                    break;
                                }
                                case 2:
                                {
                                    if(((opcode >> 7) & 7) != 7)
                                    {
                                        printf("Thumb2 B\n");
                                    }
                                    else
                                    {
                                        switch((opcode >> 5) & 0x3f)
                                        {
                                            case 0b011100:
                                            {
                                                printf("Thumb2 MSR\n");
                                                break;
                                            }
                                            case 0b011101:
                                            {
                                                if((opcode >> 4) & 1)
                                                {
                                                    switch((opcode_2 >> 4) & 0xf)
                                                    {
                                                        case 0:
                                                        {
                                                            printf("ThumbEE LEAVEX\n");
                                                            break;
                                                        }
                                                        case 1:
                                                        {
                                                            printf("ThumbEE ENTERX\n");
                                                            break;
                                                        }
                                                        case 2:
                                                        {
                                                            printf("Thumb CLREX\n");
                                                            break;
                                                        }
                                                        case 4:
                                                        {
                                                            printf("Thumb DSB\n");
                                                            break;
                                                        }
                                                        case 5:
                                                        {
                                                            printf("Thumb DMB\n");
                                                            break;
                                                        }
                                                        case 6:
                                                        {
                                                            printf("Thumb ISB\n");
                                                            break;
                                                        }
                                                    }
                                                }
                                                else
                                                {
                                                    if((opcode_2 >> 8) & 7) printf("Thumb2 CPS\n");
                                                    else
                                                    {
                                                        switch(opcode_2 & 0xff)
                                                        {
                                                            default:
                                                            case 0x00:
                                                            {
                                                                printf("Thumb2 NOP\n");
                                                                break;
                                                            }
                                                            case 0x01:
                                                               {
                                                                printf("Thumb YIELD\n");
                                                                break;
                                                            }
                                                            case 0x02:
                                                            {
                                                                printf("Thumb2 WFE\n");
                                                                break;
                                                            }
                                                            case 0x03:
                                                            {
                                                                printf("Thumb2 WFI\n");
                                                                break;
                                                            }
                                                            case 0x04:
                                                            {
                                                                printf("Thumb2 SEV\n");
                                                                break;
                                                            }
                                                            case 0xf0: case 0xf1: case 0xf2: case 0xf3:
                                                            case 0xf4: case 0xf5: case 0xf6: case 0xf7:
                                                            case 0xf8: case 0xf9: case 0xfa: case 0xfb:
                                                            case 0xfc: case 0xfd: case 0xfe: case 0xff:
                                                            {
                                                                printf("Thumb DBG\n");
                                                            }
                                                        }
                                                    }
                                                }
                                                break;
                                            }
                                            case 0b011110:
                                            {
                                                if(!((opcode >> 4) & 1)) printf("Thumb2 BXJ\n");
                                                else
                                                {
                                                    if(!(opcode_2 & 0xff)) printf("Thumb2 ERET\n");
                                                    else printf("Thumb2 SUBS PC, LR\n");
                                                }
                                                break;
                                            }
                                            case 0b011111:
                                            {
                                                printf("Thumb2 MRS\n");
                                                break;
                                            }
                                            case 0b111111:
                                            {
                                                printf("Thumb2 UNDEFINED\n");
                                                break;
                                            }
                                        }
                                    }
                                    break;
                                }
                                case 4: case 6:
                                {
                                    printf("Thumb2 BLX\n");
                                    int j1 = (opcode_2 >> 13) & 1;
                                    int j2 = (opcode_2 >> 11) & 1;
                                    int s = (opcode >> 10) & 1;

                                    int i1 = !(j1 ^ s);
                                    int i2 = !(j2 ^ s);
                                    u32 imm_10h = opcode & 0x3ff;
                                    u32 imm_10l = (opcode_2 >> 1) & 0x3ff;
                                    int h = opcode_2 & 1;
                                    if(h)
                                    {
                                        printf("This instruction is UNDEFINED\n");
                                        return;
                                    }
                                    u32 imm_32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm_10h << 12) | (imm_10l << 2);
                                    if(imm_32 & (1 << 24)) imm_32 |= 0xfe000000;
                                    r[14] = ((r[15] + 2) & 0xfffffffe) | 1;
                                    r[15] += (s32)imm_32;
                                    r[15] &= 0xfffffffc;
                                    cpsr.thumb = false;
                                    just_branched = true;
                                    break;
                                }
                                case 5: case 7:
                                {
                                    printf("Thumb2 BL\n");
                                    int j1 = (opcode_2 >> 13) & 1;
                                    int j2 = (opcode_2 >> 11) & 1;
                                    int s = (opcode >> 10) & 1;

                                    int i1 = !(j1 ^ s);
                                    int i2 = !(j2 ^ s);
                                    u32 imm_10 = opcode & 0x3ff;
                                    u32 imm_11 = opcode_2 & 0x7ff;
                                    u32 imm_32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm_10 << 12) | (imm_11 << 1);
                                    if(imm_32 & (1 << 24)) imm_32 |= 0xfe000000;
                                    r[14] = ((r[15] + 2) & 0xfffffffe) | 1;
                                    r[15] += (s32)imm_32;
                                    just_branched = true;
                                    break;
                                }
                            }
                        }
                        break;
                    }
                    case 3:
                    {
                        break;
                    }
                }
            }
            break;
        }
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