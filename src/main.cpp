#include "common.h"
#include "arm.h"
#include "iphone2g.h"
#include "iphone3gs.h"

enum class device_type
{
    iphone2g, iphone3gs
};

int main(int ac, char** av)
{   
    if(ac < 5)
    {
        printf("usage: %s [device] <path_to_bootrom> <path_to_nor> <path_to_iboot>\n", av[0]);
        printf("device can be \"iphone2g\", \"iphone2giboot\", \"iphone3gs\", or \"iphone3gsiboot\". No other devices are supported at this time.\n");
        return 1;
    }

    std::string device = av[1];
    device_type dev_type;
    bool bootromhle = false;
    if(device == "iphone2g")
    {
        dev_type = device_type::iphone2g;
        bootromhle = false;
    }
    else if(device == "iphone2giboot")
    {
        dev_type = device_type::iphone2g;
        bootromhle = true;
    }
    else if(device == "iphone3gs")
    {
        dev_type = device_type::iphone3gs;
        bootromhle = false;
    }
    else if(device == "iphone3gsiboot")
    {
        dev_type = device_type::iphone3gs;
        bootromhle = true;
    }
    else return 2;

    if(dev_type == device_type::iphone2g)
    {
        iphone2g* dev = (iphone2g*)malloc(sizeof(iphone2g));
        arm_cpu cpu;

        cpu.type = arm_type::arm11;

        cpu.init();

        dev->cpu = &cpu;

        dev->init();

        cpu.device = dev;
    
        cpu.rw_real = iphone2g_rw;
        cpu.ww_real = iphone2g_ww;

        u8* iboot_img3 = nullptr;

        FILE* fp = fopen(av[2],"rb");
        if(!fp)
        {
            printf("unable to open %s, are you sure it exists?\n", av[2]);
            return 3;
        }
        if(fread(dev->bootrom, 1, 0x10000, fp) != 0x10000)
        {
            fclose(fp);
            return 4;
        }
        fclose(fp);

        fp = fopen(av[3],"rb");
        if(!fp)
        {
            printf("unable to open %s, are you sure it exists?\n", av[3]);
            return 3;
        }
        if(fread(dev->nor, 1, 0x100000, fp) != 0x100000)
        {
            fclose(fp);
            return 4;
        }
        fclose(fp);

        fp = fopen(av[4],"rb");
        if(!fp)
        {
            printf("unable to open %s, are you sure it exists?\n", av[3]);
            return 3;
        }

        if(bootromhle)
        {
            fseek(fp, 0, SEEK_END);
            u64 filesize = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            if(fread(dev->iboot, 1, filesize, fp) != filesize)
            {
                fclose(fp);
                return 4;
            }
            fclose(fp);
            
            dev->init_hle();
        }

        if(!bootromhle)
        {
            for(int i = 0; i < 40000; i++)
            {
                cpu.run(1);
                dev->tick();
            }
        }
        else
        {
            for(int i = 0; i < 300000; i++)
            {
                cpu.run(1);
                dev->tick();
            }
        }
    }
    else if(dev_type == device_type::iphone3gs)
    {
        iphone3gs* dev = (iphone3gs*)malloc(sizeof(iphone3gs));
        arm_cpu cpu;

        cpu.type = arm_type::cortex_a8;

        cpu.init();

        dev->cpu = &cpu;

        dev->init();

        cpu.device = dev;
    
        cpu.rw_real = iphone3gs_rw;
        cpu.ww_real = iphone3gs_ww;

        FILE* fp = fopen(av[2],"rb");
        if(!fp)
        {
            printf("unable to open %s, are you sure it exists?\n", av[2]);
            return 3;
        }
        if(fread(dev->bootrom, 1, 0x10000, fp) != 0x10000)
        {
            fclose(fp);
            return 4;
        }
        fclose(fp);

        fp = fopen(av[3],"rb");
        if(!fp)
        {
            printf("unable to open %s, are you sure it exists?\n", av[3]);
            return 3;
        }
        if(fread(dev->nor, 1, 0x100000, fp) != 0x100000)
        {
            fclose(fp);
            return 4;
        }
        fclose(fp);


        for(int i = 0; i < 30000; i++)
        {
            cpu.run(1);
            dev->tick();
        }
    }

    return 0;
}
