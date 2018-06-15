#include "common.h"
#include "arm.h"
#include "iphone2g.h"

using namespace std;

enum class device_type
{
    iphone2g
};

int main(int ac, char** av)
{   
    if(ac < 3)
    {
        printf("usage: purplesapphire [device] [path_to_bootrom]\n");
        printf("device can be \"iphone2g\". No other devices are supported at this time.\n");
        return 1;
    }

    string device = av[1];
    string bootrom_path = av[2];
    device_type dev_type;
    if(device == "iphone2g") dev_type = device_type::iphone2g;
    else return 2;

    iphone2g dev;
    arm_cpu cpu;

    cpu.init();

    dev.cpu = &cpu;

    dev.init();

    cpu.device = &dev;
    
    cpu.rw_real = iphone2g_rw;
    cpu.ww_real = iphone2g_ww;

    FILE* fp = fopen(bootrom_path,"rb");
    fread(dev.bootrom, 1, 0x10000, fp);
    fclose(fp);

    cpu.run(300);

    return 0;
}
