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
        case 0x1:
        {
            switch(opcode2)
            {
                case 0: return control.whole;
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
            switch(opcode2)
            {
                case 0: control.whole = data;
            }
            break;
        }
    }
}