#ifdef __APPLE__
#error "Apple is vehemently anti-dev, and refuses to support modern OpenGL, OpenCL, or any Vulkan natively. Therefore, this software does not support MacOS or Apple."
#endif

#include "common.h"
#include "arm.h"
#include "iphone2g.h"

enum class device_type
{
    iphone2g, iphone3gs
};

enum class emu_mode_t
{
    full_lle, load_iboot, load_kernel
};

int main(int ac, char** av)
{
#ifdef __APPLE__
    printf("Really? You were smart enough to disable the first check, but still stupid enough to use an Apple operating system? Fucking shame on you, I'm out.\n");
    return -1;
#endif
    if(ac < 6)
    {
        printf("usage: %s [device] [emulation_mode] <path_to_bootrom> <path_to_nor> <path_to_iboot>\n", av[0]);
        printf("device can be \"iphone2g\". No other devices are supported at this time.\n");
        printf("emulation_mode can be \"full_lle\", \"load_iboot\", or \"load_kernel\".\n");
        return 1;
    }

    std::string device = av[1];
    std::string emu_mode_str = av[2];
    device_type dev_type;
    emu_mode_t emu_mode = emu_mode_t::full_lle;
    
    if(device == "iphone2g")
    {
        dev_type = device_type::iphone2g;
    }
    else return 2;

    if(emu_mode_str == "full_lle")
    {
        emu_mode = emu_mode_t::full_lle;
    }
    else if(emu_mode_str == "load_iboot")
    {
        emu_mode = emu_mode_t::load_iboot;
    }
    else if(emu_mode_str == "load_kernel")
    {
        emu_mode = emu_mode_t::load_kernel;
        printf("The Load Kernel option isn't implemented right now. ABORT!\n");
        return 3;
    }
    else return 4;

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

        FILE* fp = fopen(av[3],"rb");
        if(!fp)
        {
            printf("unable to open %s, are you sure it exists?\n", av[2]);
            return 5;
        }
        if(fread(dev->bootrom, 1, 0x10000, fp) != 0x10000)
        {
            fclose(fp);
            return 6;
        }
        fclose(fp);

        fp = fopen(av[4],"rb");
        if(!fp)
        {
            printf("unable to open %s, are you sure it exists?\n", av[3]);
            return 5;
        }
        if(fread(dev->nor, 1, 0x100000, fp) != 0x100000)
        {
            fclose(fp);
            return 6;
        }
        fclose(fp);

        if(emu_mode == emu_mode_t::load_iboot)
        {
            fp = fopen(av[5],"rb");
            if(!fp)
            {
                printf("unable to open %s, are you sure it exists?\n", av[3]);
                return 5;
            }
            fseek(fp, 0, SEEK_END);
            s64 filesize = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            if(filesize == -1)
            {
                fclose(fp);
                return 7;
            }
            if(fread(dev->iboot, 1, filesize, fp) != filesize)
            {
                fclose(fp);
                return 6;
            }
            fclose(fp);

            dev->init_hle();
        }
        else
        {
            memcpy(dev->lowram, dev->bootrom, 0x10000);
        }

        for(int i = 0; i < 300000; i++)
        {
            cpu.run(1);
            dev->tick();
        }

        dev->exit();
        free(dev);
    }

    return 0;
}
