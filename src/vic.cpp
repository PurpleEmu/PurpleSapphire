#include "vic.h"

void vic::init()
{
    for(int i = 0; i < 32; i++)
    {
        vect_priority[i] = 0xf;
    }
    sw_priority_mask = 0xffff;
    daisy_priority = 0xf;
    current_intr = 33;
    current_highest_intr = 33;
    stack_i = 0;
    priority_stack[0] = 0x10;
    irq_stack[0] = 33;
    priority = 0x10;

    id[0] = 0x00041192;
    id[1] = 0xb105f00d;

    daisy = nullptr;
}

void vic::raise(bool fiq)
{
    if(fiq)
    {
        cpu->fiq = true;
        if(daisy) daisy->raise(fiq);
    }
    else
    {
        cpu->irq = true;
        if(daisy)
        {
            daisy->daisy_vect_addr = address;
            daisy->daisy_input = 1;
            daisy->update();
        }
    }
}

void vic::lower(bool fiq)
{
    //if(fiq) cpu->lower_fiq();
    //else cpu->lower_irq();

    if(daisy)
    {
        if(!fiq)
        {
            daisy_input = 0;
            daisy->update();
        }
        //else daisy->lower(fiq);
    }
}

int vic::priority_sorter()
{
    u32 prio_irq[16];

    for(int i = 0; i < 16; i++)
    {
        prio_irq[i] = 33;
    }
    if(daisy_input) prio_irq[daisy_priority] = 32;
    for(int i = 31; i >= 0; i--)
    {
        if(irq_status & (1 << i)) prio_irq[vect_priority[i]] = i;
    }
    for(int i = 0; i < 16; i++)
    {
        if((sw_priority_mask & (1 << i)) && prio_irq[i] <= 32) return prio_irq[i];
    }
    return 33;
}

void vic::update()
{
    irq_status = (raw_intr | soft_int) & int_enable & ~int_select;
    fiq_status = (raw_intr | soft_int) & int_enable & int_select;

    if(fiq_status) raise(1);
    else lower(1);

    if(irq_status || daisy_input)
    {
        current_highest_intr = priority_sorter();
        if(current_highest_intr < 32) address = vect_addr[current_highest_intr];
        else address = daisy_vect_addr;

        if(current_highest_intr != current_intr)
        {
            if(current_highest_intr < 32)
            {
                if(vect_priority[current_highest_intr] >= priority) return;
            }
            if(current_highest_intr == 32)
            {
                if(daisy_priority >= priority) return;
            }
            if(current_highest_intr <= 32) raise(0);
            else lower(0);
        }
    }
    else
    {
        current_highest_intr = 33;
        lower(0);
    }
}

void vic::mask_priority()
{
    if(stack_i >= 32)
    {
        printf("VIC has detected something seriously wrong\n");
        exit(EXIT_FAILURE); /* something has gone horribly wrong and to save the world we must kill this for the greater good */
    }
    stack_i++;
    if(current_intr == 32) priority = daisy_priority;
    else priority = vect_priority[current_intr];

    priority_stack[stack_i] = priority;
    irq_stack[stack_i] = current_intr;
}

void vic::unmask_priority()
{
    if(stack_i < 1)
    {
        printf("VIC has detected something seriously wrong\n");
        exit(EXIT_FAILURE); /* see above */
    }
    stack_i--;
    priority = priority_stack[stack_i];
    current_intr = irq_stack[stack_i];
}

u32 vic::irq_ack()
{
    bool is_daisy = current_highest_intr == 32;
    u32 res = address;

    mask_priority();
    if(is_daisy)
    {
        daisy->mask_priority();
    }

    update();
    return res;
}

void vic::irq_fin()
{
    bool is_daisy = current_intr == 32;

    unmask_priority();
    if(is_daisy)
    {
        daisy->unmask_priority();
    }

    update();
}

u32 vic::rw(u32 addr)
{
    if(addr & 3) return 0;

    if(addr >= 0xfe0 && addr < 0x1000) return id[(addr - 0xfe0) >> 2];
    if(addr >= 0x100 && addr < 0x180) return vect_addr[(addr & 0x7f) >> 2];
    if(addr >= 0x200 && addr < 0x280) return vect_priority[(addr & 0x7f) >> 2];

    switch(addr & 0xfff)
    {
        case 0x000: return irq_status;
        case 0x004: return fiq_status;
        case 0x008: return raw_intr;
        case 0x00c: return int_select;
        case 0x010: return int_enable;
        case 0x018: return soft_int;
        case 0x020: return protection;
        case 0x024: return sw_priority_mask;
        case 0x028: return daisy_priority;
        case 0xf00: return irq_ack(); break;
        default: return 0;
    }
}

void vic::ww(u32 addr, u32 data)
{
    if(addr & 3) return;

    if(addr >= 0x100 && addr < 0x180)
    {
        vect_addr[(addr & 0x7f) >> 2] = data;
        update();
    }
    if(addr >= 0x200 && addr < 0x280)
    {
        vect_priority[(addr & 0x7f) >> 2] = data;
        update();
    }

    switch(addr & 0xfff)
    {
        case 0x00c: int_select = data; break;
        case 0x010: int_enable |= data; break;
        case 0x014: int_enable &= ~data; break;
        case 0x018: soft_int |= data; break;
        case 0x01c: soft_int &= ~data; break;
        case 0x020: protection = data & 1; break;
        case 0x024: sw_priority_mask = data & 0xffff; break;
        case 0x028: daisy_priority = data & 0xf; break;
        case 0xf00: irq_fin(); break;
    }

    update();
}
