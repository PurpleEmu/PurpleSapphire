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
    if(ac < 4)
    {
        printf("usage: %s [device] <path_to_bootrom> <path_to_nor>\n", av[0]);
        printf("device can be \"iphone2g\" or \"iphone3gs\". No other devices are supported at this time.\n");
        return 1;
    }

    std::string device = av[1];
    device_type dev_type;
    if(device == "iphone2g") dev_type = device_type::iphone2g;
    else if(device == "iphone3gs") dev_type = device::iphone3gs;
    else return 2;

    if(dev_type == device_type::iphone2g)
    {
        iphone2g dev;
        arm_cpu cpu;

        cpu.init();

        cpu.type = arm_type::arm11;

        dev.cpu = &cpu;

        dev.init();

        cpu.device = &dev;
    
        cpu.rw_real = iphone2g_rw;
        cpu.ww_real = iphone2g_ww;

        FILE* fp = fopen(av[2],"rb");
        if(!fp)
        {
            printf("unable to open %s, are you sure it exists?\n", av[2]);
            return 3;
        }
        if(fread(dev.bootrom, 1, 0x10000, fp) != 0x10000)
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
        if(fread(dev.nor, 1, 0x100000, fp) != 0x100000)
        {
            fclose(fp);
            return 4;
        }
        fclose(fp);


        for(int i = 0; i < 8000; i++)
        {
            cpu.run(1);
            dev.tick();
        }
    }
    else if(dev_type == device_type::iphone3gs)
    {
        iphone3gs dev;
        arm_cpu cpu;

        cpu.init();

        cpu.type = arm_type::cortex_a8;

        dev.cpu = &cpu;

        dev.init();

        cpu.device = &dev;
    
        cpu.rw_real = iphone3gs_rw;
        cpu.ww_real = iphone3gs_ww;

        FILE* fp = fopen(av[2],"rb");
        if(!fp)
        {
            printf("unable to open %s, are you sure it exists?\n", av[2]);
            return 3;
        }
        if(fread(dev.bootrom, 1, 0x10000, fp) != 0x10000)
        {
            fclose(fp);
            return 4;
        }
        fclose(fp);

        /*fp = fopen(av[3],"rb");
        if(!fp)
        {
            printf("unable to open %s, are you sure it exists?\n", av[3]);
            return 3;
        }
        if(fread(dev.nor, 1, 0x100000, fp) != 0x100000)
        {
            fclose(fp);
            return 4;
        }
        fclose(fp);*/


        for(int i = 0; i < 50; i++)
        {
            cpu.run(1);
            dev.tick();
        }
    }

    return 0;
}
