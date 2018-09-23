#ifndef MACHINA_QUARK_H
#define MACHINA_QUARK_H


#include <stdint.h>
#include <device.h>


/**
 * Pointers are divide in two parts: 29-bits for the cluster
 * index and 3-bits for the number of sequential clusters.
 */
#define QUARK_CLT_MASK          0x1FFFFFFF   // 29-bits
#define QUARK_SEQ_MASK          0xE0000000   // 3-bits
#define QUARK_SEQ(x)            ( ((x) & QUARK_SEQ_MASK) >> 29 )

/**
 * Maximum indirect size. This limit is due the coverage
 * counter, which is 16-bits. If the indirect size is bigger
 * than 32K, the amount of clusters mapped in one indirect
 * could not fit in a 16-bits field.
 *
 * indirect_entries = (cluster_size - 8) / 4
 *
 * In a fully fragmented file, a indirect can map up to
 * 'indirect_entries' clusters (each entry maps to one cluster).
 * This number of clusters fit easily in 16-bits. But if we have
 * a lot of contiguous clusters (which we desire) each entry can
 * map to 6 contiguous clusters.
 *
 * coverage = indirect_entries * 6
 *
 * The coverage field only affect the maximum file size.
 */
#define QUARK_MAX_INDIRECT_SIZE           32768


struct quark_superblock
{
    /*  0 */ uint32_t signature;
    /*  4 */ uint32_t hash;              // hash of the superblock data
    /*  8 */ uint8_t  serial[8];         // partition serial number
    /* 16 */ uint16_t version;           // file system version
    /* 18 */ uint16_t sector_size;       // sector size in bytes (default to 512)
    /* 20 */ uint32_t cluster_count;     // number of clusters in data region
    /* 24 */ uint16_t cluster_size;      // cluster size in bytes (default to 1024)
    /* 26 */ uint16_t indirect_size;     // sector in which the table starts
    /* 30 */ uint16_t bitmap_offset;     // sector in which the bitmap starts
    /* 32 */ uint16_t bitmap_sectors;    // number of sectors used by the bitmap
    /* 36 */ uint32_t root_offset;       // cluster in which the root directory starts
    /* 40 */ uint8_t  label[24];         // UTF-8 volume label (NULL-terminated if smaller than 24)
    /* 64 */ uint32_t data_offset;       // sector in which the data clusters start (1K aligned)
    /* 60 */ uint8_t  reserved3[64];
};


#define QI_SIGNATURE       0x5523FF32


#if 1

#define QI_BLOCK_MASK            0x0FFFFFFF  // bits used as block index
#define QI_COUNT_MASK            0xF0000000  // bits used as block count
#define QI_GET_BLOCK(x)          ( (x) & QI_BLOCK_MASK )
#define QI_GET_COUNT(x)          ( ((x) & QI_COUNT_MASK) >> 28 )

struct quark_indirect
{
    uint32_t signature;
    uint32_t count;        // number of pointers
    uint32_t next;         // next block in which the indirect continues
    uint32_t coverage;     // how many blocks this indirect maps
    uint32_t *pointers[];  // pointer array (out of the structure)
};

#else

struct quark_slot
{
    uint32_t coverage;
    uint32_t pointer;
};

struct quark_indirect
{
    uint32_t signature;
    uint16_t count;                 // number of pointers
    uint16_t reserved;
    uint32_t coverage;             // how many clusters this indirect maps
    struct quark_slot *pointers[];  // pointer array (out of the structure)
};

#endif


/**
 * Number of inode pointers. Each pointer have the index
 * of the data block (or first block of a indirect) and
 * the amount of sequential blocks from that index. Use
 * QI_GET_BLOCK and QI_GET_COUNT macros to retrieve the
 * desired information.
 */
#define QD_DIR_SLOTS   4
#define QD_IND_SLOTS   1
#define QD_MAX_SLOTS   (QD_DIR_SLOTS + QD_IND_SLOTS)


struct quark_inode
{
    /*   0 */ uint32_t size;       // effective file size
    /*   4 */ uint32_t write_time;
    /*   8 */ uint16_t bits;       // permissions (9 bits) and flags (7 bits)
    /*  10 */ uint16_t owner;      // 7 MSB for user, 9 LSB for group
    /*  12 */ uint32_t pointers[QD_MAX_SLOTS];
    /*  32 */
};


#if 1

struct quark_dentry
{
    /*  0 */ uint32_t inode;
    /*  4 */ uint16_t name_hash;  // hash of the entire file name
    /*  6 */ uint8_t  name_length;
    /*  7 */ uint8_t  name[25];   // UTF-8 file name (NULL-terminated if smaller than 25)
    /* 32 */
};

#else

struct quark_dentry
{
    /*   0 */ uint32_t size;       // effective file size
    /*   4 */ uint32_t write_time;
    /*   8 */ uint16_t bits;       // permissions (9 bits) and flags (7 bits)
    /*  10 */ uint16_t owner;      // 7 MSB for user, 9 LSB for group
    /*  12 */ struct quark_slot slots[QD_MAX_SLOTS];
    /*  92 */ uint32_t reserved;
    /*  96 */ uint16_t name_hash;  // hash of the entire file name
    /*  98 */ uint8_t  name_length;
    /*  99 */ uint8_t  name[29];   // UTF-8 file name (NULL-terminated if smaller than 29)
};

#endif

/**
 * High-level structure to represent a mounted Quark in memory.
 */
struct quark_descriptor
{
    struct quark_superblock super;
    uint32_t *bitmap;
    struct storage_device *device;
    uint32_t data_offset;
    uint32_t sector_per_cluster;
};


/**
 * High-level structure to represent a Quark directory iterator.
 */
struct dentry_iterator
{
    struct quark_dentry *parent;
	uint32_t cluster;
	uint32_t offset;
	struct quark_descriptor *desc;
	uint8_t *buffer;
	char *fileName;  // with QD_MAX_NAME + 1 bytes
	uint32_t flags;
};


#define QC_FREE      0x00000000
#define QC_BAD       0x0FFFFFFD
#define QC_DELETE    0x0FFFFFFE
#define QC_EOF       0x0FFFFFFF

#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)

#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IRWXG (S_IRGRP | S_IWGRP | S_IXGRP)

#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001
#define S_IRWXO (S_IROTH | S_IWOTH | S_IXOTH)

#define AT_HIDDEN      0x0200  // readdir do not show the file (but file can be accessed)
#define AT_DIRECTORY   0x0400  // dentry is a directory
#define AT_REGULAR     0x0800  // dentry is a regular file
#define AT_SYMLINK     0x1000  // dentry is a symbolic link
#define AT_DELETE      0x2000  // entry marked for deletion
#define AT_UNUSED2     0x4000
#define AT_UNUSED3     0x8000

#define MFS_SB_SIGNATURE       0xDEADBEEF
#define MFS_SB_VERSION         0x0100  // 1.0

#define QUARK_MAX_PATH         256
#define QUARK_MAX_NAME         (12 + 32 + 32)
#define QUARK_DENTRY_SIZE      ( (uint32_t) sizeof(struct quark_dentry) )

#define QUARK_MIN(a, b)   ( ((a) > (b)) ? (b) : (a) )


int quark_format(
    struct storage_device *device,
    uint32_t diskSize );

int quark_mount(
	struct quark_descriptor *desc,
	struct storage_device *device );

int quark_umount(
	struct quark_descriptor *desc );

int quark_next_iteration(
    struct dentry_iterator *it );

int quark_create_iterator(
    struct dentry_iterator *it,
    struct quark_descriptor *desc,
    struct quark_dentry *parent,
    uint32_t flags );

int quark_reset_iterator(
    struct dentry_iterator *it,
    uint32_t cluster );

int quark_destroy_iterator(
    struct dentry_iterator *it );

int quark_iterate(
    struct dentry_iterator *it,
    struct quark_dentry **dentry,
	const char **fileName );

int quark_list_root(
    struct quark_descriptor *desc );

int quark_lookup(
    struct quark_descriptor *desc,
    const char *path,
    struct quark_dentry *dentry);

int quark_read(
    struct quark_descriptor *desc,
    struct quark_dentry *dentry,
    uint8_t *buffer,
    uint32_t size,
    uint32_t offset );

#endif // MACHINA_QUARK_H
