#include <quark.h>
#include <string.h>
#include <stdio.h>


static void printSuperblock(
    struct quark_superblock *sb )
{
    printf("     Signature: 0x%08X\n", sb->signature);
    printf("        Serial:");
    for (size_t i = 0 ; i < 8; ++i)
        printf(" %02X", (int) sb->serial[i]);
    printf("\n       Version: %u.%u\n", sb->version >> 8, sb->version & 0xFF );
    printf("   Sector size: %u bytes\n", sb->sector_size);
    printf(" Cluster count: %u\n", sb->cluster_count);
    printf("  Cluster size: %u bytes\n", sb->cluster_size);
    printf("  Table offset: sector %u\n", sb->table_offset);
    printf(" Table sectors: %u\n", sb->table_sectors);
    printf(" Bitmap offset: sector %u\n", sb->bitmap_offset);
    printf("Bitmap sectors: %u\n", sb->bitmap_sectors);
    printf("   Root offset: cluster %u\n", sb->root_offset);
    printf("         Label: %.*s\n", ((sb->label[23] == 0) ? strlen(sb->label) : 24), sb->label);
    printf("   Data offset: sector %u\n", sb->table_offset + sb->table_sectors);
}


/**
 * Adapted version of Bob Jenkins' "one at a time" 32 bits hash function.
 */
static uint16_t quark_hash(
    const uint8_t *fileName,
    size_t length )
{
    size_t i = 0;
    uint32_t hash = 0;
    while (i != length)
    {
        hash += fileName[i++];
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;

    return (uint16_t) ( (hash >> 16) ^ (hash & 0xFFFF) );
}


int quark_format(
    struct storage_device *device,
    uint32_t diskSize )
{
    uint8_t sector[512];
    memset(sector, 0, sizeof(sector));
    struct quark_superblock *sb = (struct quark_superblock *) &sector;

    static const uint32_t SECTOR_SIZE = 512;
    static const uint32_t CLUSTER_SIZE = 1024;

    //                              table estimative     sector 0
    uint32_t clusters = (diskSize - (diskSize / 256) - SECTOR_SIZE) / CLUSTER_SIZE;

    // write superblock
    sb->signature      = MFS_SB_SIGNATURE;
    sb->version        = MFS_SB_VERSION;
    sb->sector_size    = SECTOR_SIZE;
    sb->cluster_size   = CLUSTER_SIZE;
    sb->cluster_count  = clusters;
    sb->table_offset   = 1;
    sb->table_sectors  = ((clusters * sizeof(uint32_t) + (SECTOR_SIZE-1)) & (~(SECTOR_SIZE-1))) / SECTOR_SIZE;
    sb->bitmap_offset  = 0;
    sb->bitmap_sectors = 0;
    sb->root_offset    = 1;
    strcpy(sb->label, "MACHINA");
    device_write(device, 0, sector);
    uint32_t clusterCount = sb->cluster_count;
    // print superblock
    printSuperblock(sb);
    device_write(device, 0, sector);

    uint16_t tableSectors = sb->table_sectors;
    uint16_t tableOffset  = sb->table_offset;
    sb = NULL;

    // write table
    memset(sector, 0, sizeof(sector));
    uint32_t *table = (uint32_t*) sector;
    table[1] = QC_EOF; // only one cluster for root directory
    device_write(device, tableOffset, sector);
    table[1] = QC_FREE;
    for (size_t i = 1; i < tableSectors; ++i)
        device_write(device, i + tableOffset, sector);
    table = NULL;

    // clean root directory cluster
    uint32_t dataOffset = sb->table_offset + sb->table_sectors;
    memset(sector, 0, sizeof(sector));
    for (size_t i = 0; i < CLUSTER_SIZE / SECTOR_SIZE; ++i)
        device_write(device, i + dataOffset, sector);
    // write root directory
    struct quark_dentry *dentry = (struct quark_dentry*) sector;
    dentry->name_hash = quark_hash(".", 1);
    dentry->bits =

    return 0;
}


int quark_mount(
	struct quark_descriptor *desc,
	struct storage_device *device )
{
    if (desc == NULL || device == NULL) return 1;

    uint8_t buffer[512];
    device_read(device, 0, buffer);
    memcpy(&desc->superblock, buffer, sizeof(struct quark_superblock));

    uint32_t tableSectors = ((desc->superblock.cluster_count * sizeof(uint32_t)) + (desc->superblock.sector_size-1) & (~(desc->superblock.sector_size-1))) / desc->superblock.sector_size;
    desc->bitmap = desc->table = NULL;
    desc->device = device;
    desc->data_offset = tableSectors + desc->superblock.table_offset;

    desc->table = (uint32_t*) malloc(desc->superblock.table_sectors * desc->superblock.sector_size);

    return 0;
}


int quark_umount(
	struct quark_descriptor *desc )
{
    return 0;
}