#include <fat32.h>
#include <fat32_ui.h>
#include <string.h>
#include <stdbool.h>


#define FIRST_SECTOR(desc, cluster) \
    ( (cluster - 2) * (desc)->sectorsPerCluster + (desc)->firstDataSector )


int fat32_mount(
    struct fat32_descriptor *desc,
    struct storage_device *device )
{
    if (desc == NULL || device == NULL) return 1;

    struct bpb *bpb;
    uint32_t fat_size, root_dir_sectors, data_sectors;

    // Initialize the runtime portion of the TFInfo structure, and read sec0
    desc->currentSector = 0xFFFFFFFF;
    desc->sectorFlags = 0;
    desc->buffer = NULL;
    desc->device = device;
    uint8_t sectorData[512];
    device_read(device, 0, sectorData);
    #ifdef TF_DEBUG
    printBPB( (struct bpb*)sectorData );
    #endif

    // Cast to a BPB, so we can extract relevant data
    bpb = (struct bpb *) sectorData;

    /* Some sanity checks to make sure we're really dealing with FAT here
     * see fatgen103.pdf pg. 9ff. for details */

    /* BS_jmpBoot needs to contain specific instructions */
    if (!(bpb->jump[0] == 0xEB && bpb->jump[2] == 0x90) && !(bpb->jump[0] == 0xE9))
    {
        dbg_printf("  tf_init FAILED: stupid jmp instruction isn't exactly right...");
        return TF_ERR_BAD_FS_TYPE;
    }

    /* Only specific bytes per sector values are allowed
     * FIXME: Only 512 bytes are supported by thinfat at the moment */
    if (bpb->bytes_per_sector != 512)
    {
        dbg_printf("  tf_init() FAILED: Bad Filesystem Type (!=512 bytes/sector)\r\n");
        return TF_ERR_BAD_FS_TYPE;
    }

    if (bpb->reserved_sectors == 0)
    {
        dbg_printf("  tf_init() FAILED: reserved_sectors == 0!!\r\n");
        return TF_ERR_BAD_FS_TYPE;
    }

    /* Valid media types */
    if (bpb->media != 0xF0 && bpb->media < 0xF8)
    {
        dbg_printf("  tf_init() FAILED: Invalid Media Type!  (0xf0, or 0xf8 <= type <= 0xff)\r\n");
        return TF_ERR_BAD_FS_TYPE;
    }

    // for now only FAT32 is supported
    if (bpb->fat_size_16 != 0 || bpb->fat_specific.fat32.fat_size_32 == 0 || bpb->total_sectors_16 != 0 || bpb->total_sectors_32 == 0)
    {
        dbg_printf("  tf_init() FAILED: Only FAT32 is supported\r\n");
        return TF_ERR_BAD_FS_TYPE;
    }

    // number of sectors used by each FAT
    fat_size                = (bpb->fat_size_16 != 0) ? bpb->fat_size_16 : bpb->fat_specific.fat32.fat_size_32;
    // number of sectors used by root directory (FAT32: always zero)
    root_dir_sectors        = 0;
    desc->totalSectors      = (bpb->total_sectors_16 != 0) ? bpb->total_sectors_16 : bpb->total_sectors_32;
    data_sectors              = desc->totalSectors - bpb->reserved_sectors - (bpb->number_fats * fat_size) - root_dir_sectors;
    desc->sectorsPerCluster = bpb->sectors_per_cluster;
    desc->cluster_count     = data_sectors / bpb->sectors_per_cluster;
    desc->reservedSectors   = bpb->reserved_sectors;
    desc->firstDataSector   = bpb->reserved_sectors + (bpb->number_fats * fat_size) + root_dir_sectors;
    desc->clusterSize       = (uint32_t) bpb->bytes_per_sector * (uint32_t) bpb->sectors_per_cluster;

    // Now that we know the total count of clusters, we can compute the FAT type
    if(desc->cluster_count < 65525)
    {
        dbg_printf("  tf_init() FAILED: Invalid FAT32 (cluster count smaller than 65525)\r\n");
        return TF_ERR_BAD_FS_TYPE;
    }
    else
        desc->type = TF_TYPE_FAT32;

    #ifdef TF_DEBUG
    //desc->sector_reads = 0;
    //desc->sector_writes = 0;
    #endif

    // TODO ADD SANITY CHECKING HERE (CHECK THE BOOT SIGNATURE, ETC... ETC...)
    desc->rootDirectorySize = 0xffffffff;
    desc->buffer = (uint8_t*) malloc( desc->clusterSize );

    // load FAT to memory
    size_t bytesPerSector = bpb->bytes_per_sector;
    bpb = NULL;
    desc->fat = (uint32_t*) malloc( fat_size * bytesPerSector );
    for (size_t i = 0; i < fat_size; ++i)
    {
        device_read(device, (uint32_t) (desc->reservedSectors + i), sectorData);
        memcpy( (uint8_t*) desc->fat + i * bytesPerSector, sectorData, bytesPerSector);
    }
    dbg_printf("Read %d sectors from FAT\n", fat_size);

    dbg_printf("\r\ntf_init() successful...\r\n");
    return 0;
}

int fat32_umount(
    struct fat32_descriptor *desc )
{
    if (desc == NULL) return 1;
    if (desc->fat != NULL) free(desc->fat);
    memset(desc, 0, sizeof(struct fat32_descriptor));
    return 0;
}


static int fat32_read_cluster(
    struct fat32_descriptor *desc,
    struct storage_device *device,
    uint32_t cluster,
    uint8_t *buffer,
    size_t size )
{
    if (device == NULL || buffer == NULL || size != desc->clusterSize) return 1;

    uint32_t sector = FIRST_SECTOR(desc, cluster);

    size_t i = 0;
    for (; i < desc->clusterSize / 512; ++i)
    {
        device_read(device, sector + i, buffer + i * 512);
    }

    dbg_printf("Read cluster #%d (%d sectors from sector #%d)\n", cluster, i, sector);

    return 0;
}


static int fat32_next_cluster(
    struct fat32_descriptor *desc,
    uint32_t cluster,
    uint32_t *next )
{
    cluster = (uint32_t) (cluster & 0x0FFFFFFF);

    if (cluster < 2 || cluster >= TF_MARK_BAD_CLUSTER32) return 1;

    // cluster index starts from 2
    cluster -= 2;
    if (cluster >= desc->cluster_count) return 1;

    return desc->fat[cluster];
}


/**
 * Retrieve a file name from a LFN directory entriy.
 *
 */
static int fat32_parse_lfn(
    struct fat32_descriptor *desc,
    union dentry *dentry,
    char *buffer )
{
    if (buffer == NULL) return 1;

    int offset = 0;

    if ((dentry->lfn.sequence_number & 0xF0) != 0x40)
        offset = strlen(buffer);

    for (int i = sizeof(dentry->lfn.name3) / sizeof(uint16_t) - 1; offset < 255 && i >= 0; --i)
        if (dentry->lfn.name3[i] != 0 && dentry->lfn.name3[i] != 0xFFFF)
            buffer[offset++] = dentry->lfn.name3[i] & 0x00FF;
    for (int i = sizeof(dentry->lfn.name2) / sizeof(uint16_t) - 1; offset < 255 && i >= 0; --i)
        if (dentry->lfn.name2[i] != 0 && dentry->lfn.name2[i] != 0xFFFF)
            buffer[offset++] = dentry->lfn.name2[i] & 0x00FF;
    for (int i = sizeof(dentry->lfn.name1) / sizeof(uint16_t) - 1; offset < 255 && i >= 0; --i)
        if (dentry->lfn.name1[i] != 0 && dentry->lfn.name1[i] != 0xFFFF)
            buffer[offset++] = dentry->lfn.name1[i] & 0x00FF;

    buffer[offset] = 0;

    if ((dentry->lfn.sequence_number & 0x0F) == 1)
    {
        for (size_t i = 0, t = strlen(buffer); i < t / 2; ++i)
        {
            char c = buffer[i];
            buffer[i] = buffer[t - 1 - i];
            buffer[t - 1 - i] = c;
        }
    }

//dbg_printf("LFN #%d: %s\n", dentry->lfn.sequence_number & 0x0F, buffer);

    return 0;
}


static int fat32_list_dentry(
    struct fat32_descriptor *desc,
    uint32_t cluster,
    const char *parent )
{
    uint8_t *buffer = (uint8_t*) malloc( desc->clusterSize );
    if (fat32_read_cluster(desc, desc->device, cluster, buffer, desc->clusterSize) != 0) return 1;

    union dentry *entries = (union dentry *) buffer;
    char fileName[256];
    bool complete = false;

    for (size_t i = 0; i < desc->clusterSize / 32; ++i)
    {
        if (entries[i].msdos.name[0] == 0) break;
        // skip the volume ID inside root directory
        if (entries[i].msdos.attributes != ATTR_LONG_NAME && entries[i].msdos.attributes & ATTR_VOLUME_ID) continue;

        if (entries[i].msdos.attributes == ATTR_LONG_NAME)
        {
            fat32_parse_lfn(desc, entries +i, fileName);
        }
        else
        {
            if (fileName[0] == '.' || entries[i].msdos.name[0] == '.') continue;
            // print file name
            if (fileName[0] != 0)
                printf("%s/%s\n", parent, fileName);
            else
                printf("%s/%.*s\n", parent, 8 + 3, entries[i].msdos.name);

            // if the current entry is a directory, recusively list its files
            if (entries[i].msdos.attributes & ATTR_DIRECTORY)
            {
                if (entries[i].msdos.name[0] == '.') continue;

                char current[256] = { 0 };
                strcpy(current, parent);
                strcat(current, "/");
                strcat(current, fileName);

                uint32_t next = (uint32_t) ((entries[i].msdos.first_cluster_hi << 16) | entries[i].msdos.first_cluster_lo );
                fat32_list_dentry(desc, next, current);
            }

            fileName[0] = 0;
        }
    }

    free(buffer);
    return 0;
}


int fat32_list_root(
    struct fat32_descriptor *desc )
{
    return fat32_list_dentry(desc, 2, "");
}
