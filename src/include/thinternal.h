#ifndef _THINTERNAL_H
#define _THINTERNAL_H

#define false 0
#define true 1

#define ATTR_READ_ONLY      0x01              
#define ATTR_HIDDEN         0x02
#define ATTR_SYSTEM         0x04       
#define ATTR_VOLUME_ID      0x08       
#define ATTR_DIRECTORY      0x10       
#define ATTR_ARCHIVE        0x20     
#define ATTR_LONG_NAME      (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

#pragma pack(push, 1)

// Starting at offset 36 into the BPB, this is the structure for a FAT12/16 FS
struct bpb_fat1x
{
    uint8_t     drive_number;           // 1
    uint8_t     reserved1;              // 1
    uint8_t     boot_sig;               // 1
    uint32_t    volume_id;              // 4
    uint8_t     volume_label[11];       // 11
    uint8_t     fs_type[8];             // 8
};

// Starting at offset 36 into the BPB, this is the structure for a FAT32 FS
struct bpb_fat32
{
    uint32_t    fat_size_32;            // 4 (unit: sectors)
    uint16_t    flags;                  // 2
    uint16_t    version;                // 2
    uint32_t    root_cluster;           // 4
    uint16_t    fs_info;                // 2
    uint16_t    backup_boot_sector;     // 2
    uint8_t     reserved1[12];          // 12
    uint8_t     drive_number;           // 1
    uint8_t     reserved2;              // 1
    uint8_t     boot_sig;               // 1
    uint32_t    volume_id;              // 4
    uint8_t     volume_label[11];       // 11
    uint8_t     fs_type[8];             // 8
};

struct bpb
{
    uint8_t     jump[3];                 // 3
    uint8_t     oem_name[8];             // 8
    uint16_t    bytes_per_sector;        // 2
    uint8_t     sectors_per_cluster;     // 1
    uint16_t    reserved_sectors;        // 2
    uint8_t     number_fats;             // 1
    uint16_t    root_entries;            // 2
    uint16_t    total_sectors_16;        // 2
    uint8_t     media;                   // 1
    uint16_t    fat_size_16;             // 2  (unit: sectors)
    uint16_t    sectors_per_track;       // 2
    uint16_t    number_heads;            // 2
    uint32_t    hidden_sectors;          // 4
    uint32_t    total_sectors_32;        // 4
    union
    {
        struct bpb_fat1x fat16;
        struct bpb_fat32 fat32;
    } fat_specific;
};

typedef struct short_name_dentry {
    uint8_t  name[8];
    uint8_t  extension[3];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_time;
    uint16_t first_cluster_hi;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_lo;
    uint32_t size;
};

typedef struct long_name_dentry {
    uint8_t sequence_number;
    uint16_t name1[5];      // 5 Chars of name (UTF 16???)
    uint8_t attributes;     // Always 0x0f
    uint8_t reserved;       // Always 0x00
    uint8_t checksum;       // Checksum of DOS Filename.  See Docs.
    uint16_t name2[6];      // 6 More chars of name (UTF-16)
        uint16_t firstCluster;  // Always 0x0000
    uint16_t name3[2];
};

typedef union dentry {
    struct short_name_dentry msdos;
    struct long_name_dentry lfn;
} dentry_t;

#pragma pack(pop)


// "Legacy" functions
uint32_t fat_size(struct bpb *bpb);
int total_sectors(struct bpb *bpb);
int root_dir_sectors(struct bpb *bpb);
int cluster_count(struct bpb *bpb);
int fat_type(struct bpb *bpb);
int first_data_sector(struct bpb *bpb);
int first_sector_of_cluster(struct bpb *bpb, int N);
int data_sectors(struct bpb *bpb);
int fat_sector_number(struct bpb *bpb, int N);
int fat_entry_offset(struct bpb *bpb, int N);
int fat_entry_for_cluster(struct bpb *bpb, uint8_t *buffer, int N);

#endif
