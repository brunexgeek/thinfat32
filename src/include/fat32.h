#ifndef MACHINA_FAT32_H
#define MACHINA_FAT32_H


#include <stdint.h>
#include <device.h>


//#define FAT32_ENABLE_HASH
//#define FAT32_ENABLE_LFN


#define TF_ERR_NO_ERROR 0
#define TF_ERR_BAD_BOOT_SIGNATURE 1
#define TF_ERR_BAD_FS_TYPE 2
#define TF_ERR_INVALID_SEEK 1

#define TF_TYPE_FAT16 0
#define TF_TYPE_FAT32 1

#define TYPE_FAT12 0
#define TYPE_FAT16 1
#define TYPE_FAT32 2

#define TF_MARK_BAD_CLUSTER32 0x0ffffff7
#define TF_MARK_EOC32 0x0fffffff

#define FAT32_MAX_PATH  260 // path + null terminator
#define FAT32_MAX_LFN   256 // path + null terminator
#define FAT32_MAX_SFN    (8+3)

#define ATTR_READ_ONLY      0x01
#define ATTR_HIDDEN         0x02
#define ATTR_SYSTEM         0x04
#define ATTR_VOLUME_ID      0x08
#define ATTR_DIRECTORY      0x10
#define ATTR_ARCHIVE        0x20
#define ATTR_LONG_NAME      (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

#define ATTR_LONG_NAME_MASK (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID | ATTR_DIRECTORY | ATTR_ARCHIVE)

/// Iterate through all valid entries (e.g. deleted and LFN)
#define FAT32_ITF_ANY    0x01

#define FAT32_MIN(a, b)   ( ((a) > (b)) ? (b) : (a) )

#ifdef DEBUG

#include <stdio.h>

	#define dbg_printf(...) printf(__VA_ARGS__)
	#define dbg_printHex(x,y) printHex(x,y)

#ifdef TF_DEBUG
typedef struct struct_TFStats {
	unsigned long sector_reads;
	unsigned long sector_writes;
} TFStats;

	#define tf_printf(...) printf(__VA_ARGS__)
	#define tf_printHex(x,y) printHex(x,y)
#else
	#define tf_printf(...)
	#define tf_printHex(x,y)
#endif  // TF_DEBUG

#else   // DEBUG
	#define dbg_printf(...)
	#define dbg_printHex(x,y)
	#define tf_printf(...)
	#define tf_printHex(x,y)
#endif  // DEBUG


#pragma pack(push, 1)

struct short_name_dentry
{
	uint8_t  name[11];
	//uint8_t  extension[3];
	uint8_t  attributes;
	#ifdef FAT32_ENABLE_HASH
	uint16_t hash;
	#else
	uint8_t  reserved;
	uint8_t  creation_time_tenth;
	#endif
	uint16_t creation_time;
	uint16_t creation_date;
	uint16_t access_date;
	uint16_t first_cluster_hi;
	uint16_t write_time;
	uint16_t write_date;
	uint16_t first_cluster_lo;
	uint32_t size;
};

struct long_name_dentry {
	uint8_t sequence_number;
	uint16_t name1[5];      // 5 chars of name (UCS-2)
	uint8_t attributes;     // always 0x0f
	uint8_t reserved;       // always 0x00
	uint8_t checksum;       // checksum of 8.3 file name
	uint16_t name2[6];      // 6 chars of name (UCS-2)
	uint16_t firstCluster;  // Always 0x0000
	uint16_t name3[2];      // 2 chars of name (UCS-2)
};

union dentry {
	struct short_name_dentry msdos;
	struct long_name_dentry lfn;
};

#pragma pack(pop)

struct fat32_descriptor
{
	// FILESYSTEM INFO PROPER
	uint8_t sectorsPerCluster;
	uint32_t firstDataSector;
	uint32_t totalSectors;
	uint16_t reservedSectors;
	// "LIVE" DATA
	uint32_t currentSector;
	uint8_t sectorFlags;
	uint32_t rootDirectorySize;
//    uint8_t buffer[512];
//    uint8_t *buffer; // always the same size of the cluster
	uint32_t *fat;
	uint32_t cluster_count;
	uint32_t clusterSize; // in bytes
	uint32_t sectorSize; // in bytes

	struct storage_device *device;
};


struct fat32_dentry
{
	uint32_t cluster;
	union dentry dentry;
};


struct dentry_iterator
{
	uint32_t cluster;
	uint32_t offset;
	struct fat32_descriptor *desc;
	uint8_t *buffer;
	char *fileName;
	uint16_t fileNameLen;
	uint32_t flags;
};


int fat32_mount(
	struct fat32_descriptor *desc,
	struct storage_device *device );

int fat32_umount(
	struct fat32_descriptor *desc );

int fat32_list_root(
	struct fat32_descriptor *desc );

int fat32_lookup(
	struct fat32_descriptor *desc,
	const char *path,
	union dentry *dentry );

int fat32_create_iterator(
	struct dentry_iterator *it,
	struct fat32_descriptor *desc,
	uint32_t cluster,
	uint32_t flags );

int fat32_reset_iterator(
    struct dentry_iterator *it,
    uint32_t cluster );

int fat32_destroy_iterator(
	struct dentry_iterator *it );

int fat32_iterate(
	struct dentry_iterator *it,
	union dentry **dentry,
	const char **fileName );

int fat32_first_cluster(
    const struct fat32_descriptor *desc,
    const union dentry *dentry,
    uint32_t *cluster );

int fat32_next_cluster(
    const struct fat32_descriptor *desc,
    uint32_t cluster,
    uint32_t *next );

int fat32_read_cluster(
    struct fat32_descriptor *desc,
    uint32_t cluster,
    uint8_t *buffer,
    size_t size );

int fat32_read(
    struct fat32_descriptor *desc,
    union dentry *dentry,
    uint8_t *buffer,
    uint32_t size,
    uint32_t offset );

#endif