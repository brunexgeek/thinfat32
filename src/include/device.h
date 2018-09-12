#ifndef MACHINA_DEVICE_H
#define MACHINA_DEVICE_H


#include <stdint.h>
#include <stdlib.h>


struct storage_device 
{
	void *pointer;
	uint32_t currentSector;
    uint16_t sectorSize;
};


int device_read(
	struct storage_device *device,
	uint32_t sector,
    uint8_t *buffer );

int device_write(
	struct storage_device *device,
	uint32_t sector,
    const uint8_t *buffer );

struct storage_device *device_open(
	struct storage_device *device,
	const char *path );

void device_close(
	struct storage_device *device );


#endif // MACHINA_DEVICE_H