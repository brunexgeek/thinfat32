#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fat32.h>
#include <fat32_ui.h>
#include <quark.h>


int main(int argc, char **argv)
{
    struct storage_device device;
    device_open(&device, "test.quark");

    quark_format(&device, 64 * 1024 * 1024);

    device_close(&device);

#if 0
    struct storage_device device;
    device_open(&device, "test.fat32");

    printf("\r\nFAT32 Filesystem Test");
    printf("\r\n-----------------------");
    struct fat32_descriptor desc;
    fat32_mount(&desc, &device);
    fat32_list_root(&desc);
	fat32_umount(&desc);
    device_close(&device);
#endif
    return 0;
}


