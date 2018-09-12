#include "device.h"
#include <stdio.h>


// USERLAND
int device_read(
	struct storage_device *device,
	uint32_t sector,
    uint8_t *buffer )
{
	if (device == NULL || buffer == NULL || buffer == NULL) return 1;
	fseek((FILE*)device->pointer, sector * device->sectorSize, 0);
	fread(buffer, 1, device->sectorSize, device->pointer);
	device->currentSector = sector;
	return 0;
}

int device_write(
	struct storage_device *device,
	uint32_t sector,
    const uint8_t *buffer )
{
	if (device == NULL || device->pointer == NULL || buffer == NULL) return 1;
	fseek((FILE*)device->pointer, sector * device->sectorSize, 0);
	fwrite(buffer, 1, device->sectorSize, device->pointer);
	device->currentSector = sector;
	return 0;
}


struct storage_device *device_open(
	struct storage_device *device,
	const char *path )
{
	if (path == NULL || device == NULL) return NULL;
	device->pointer = fopen(path, "r+b");
	device->currentSector = 0xFFFFFFFF;
    device->sectorSize = 512;

    return device;
}


void device_close(
	struct storage_device *device )
{
	if (device == NULL) return;
	if (device->pointer != NULL) fclose( (FILE*) device->pointer);
	device->pointer = NULL;
	device->currentSector = 0xFFFFFFFF;
}
