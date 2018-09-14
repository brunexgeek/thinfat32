#include <fat32.h>
#include <fat32_ui.h>
#include <string.h>
#include <stdbool.h>


#define FIRST_SECTOR(desc, cluster) \
    ( (cluster - 2) * (desc)->sectorsPerCluster + (desc)->firstDataSector )

#define IS_VALID_CLUSTER(desc, cluster) \
    ( cluster >= 2 && cluster < TF_MARK_BAD_CLUSTER32 && cluster < (desc)->cluster_count)

#define TO_LOWER_CASE(x) \
    ( ( (x) >= 'A' && (x) <= 'Z' ) ? (x) + 32 : (x) )

static uint16_t fat32_hash(
    const uint8_t *fileName );


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
    desc->sectorSize        = (uint32_t) bpb->bytes_per_sector;

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

    // read FSInfo sector
    device_read(device, bpb->fat_specific.fat32.fs_info, sectorData);
    print_fsinfo((struct fs_info*)sectorData);

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
    dbg_printf("Mounted FAT32 volume!\n");

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
    uint32_t cluster,
    uint8_t *buffer,
    size_t size )
{
    if (desc == NULL || buffer == NULL || size != desc->clusterSize) return 1;

    uint32_t sector = FIRST_SECTOR(desc, cluster);

    size_t i = 0;
    for (; i < desc->clusterSize / desc->sectorSize; ++i)
    {
        device_read(desc->device, (uint32_t) (sector + i), buffer + i * desc->sectorSize);
    }

    dbg_printf("Read cluster #%d (%d sectors from sector #%d)\n", cluster, (uint32_t) i, sector);

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

    *next = desc->fat[cluster];
    return 0;
}


#ifdef FAT32_ENABLE_LFN

/**
 * Retrieve a file name from a LFN directory entriy.
 *
 */
static int fat32_parse_lfn(
    union dentry *dentry,
    char *buffer )
{

    if (buffer == NULL) return 1;

    int offset = 0;

    if ((dentry->lfn.sequence_number & 0xF0) != 0x40)
        offset = (int) strlen(buffer);

    for (int i = sizeof(dentry->lfn.name3) / sizeof(uint16_t) - 1; offset < 255 && i >= 0; --i)
        if (dentry->lfn.name3[i] != 0 && dentry->lfn.name3[i] != 0xFFFF)
            buffer[offset++] = (char) (dentry->lfn.name3[i] & 0x00FF);
    for (int i = sizeof(dentry->lfn.name2) / sizeof(uint16_t) - 1; offset < 255 && i >= 0; --i)
        if (dentry->lfn.name2[i] != 0 && dentry->lfn.name2[i] != 0xFFFF)
            buffer[offset++] = (char) (dentry->lfn.name2[i] & 0x00FF);
    for (int i = sizeof(dentry->lfn.name1) / sizeof(uint16_t) - 1; offset < 255 && i >= 0; --i)
        if (dentry->lfn.name1[i] != 0 && dentry->lfn.name1[i] != 0xFFFF)
            buffer[offset++] = (char) (dentry->lfn.name1[i] & 0x00FF);

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

#endif // FAT32_ENABLE_LFN


static int fat32_format_sfn(
    const union dentry *dentry,
    char *buffer,
    size_t size )
{
    // buffer should fit the name, a null terminator and the dot
    if (size < FAT32_MAX_SFN + 2) return 1;
    size_t i = 0, j = 0;

    while (i < FAT32_MAX_SFN)
    {
        if (i == 8) buffer[j++] = '.';
        if (dentry->msdos.name[i] != 0x20)
            buffer[j++] = TO_LOWER_CASE(dentry->msdos.name[i]);
        ++i;
    }
    if (j > 0 && buffer[j - 1] == '.') --j;
    buffer[j] = 0;

    return 0;
}


static int fat32_list_dentry(
    struct fat32_descriptor *desc,
    uint32_t cluster,
    const char *parent )
{
    struct dentry_iterator it;
    fat32_create_iterator(&it, desc, cluster, 0);

    union dentry *dentry = NULL;
    char *fileName = NULL;

    while (fat32_iterate(&it, &dentry, &fileName) == 0)
    {
        #ifdef FAT32_ENABLE_HASH
        dbg_printf("[0x%04x] [0x%04x] %s/%s\n", fat32_hash(dentry->msdos.name), dentry->msdos.hash, parent, fileName);
        #else
        dbg_printf("%s/%s\n", parent, fileName);
        #endif

        // if the current entry is a directory, recusively list its files
        if (dentry->msdos.attributes & ATTR_DIRECTORY)
        {
            char current[256] = { 0 };
            strcpy(current, parent);
            strcat(current, "/");
            strcat(current, fileName);

            uint32_t next = (uint32_t) ((dentry->msdos.first_cluster_hi << 16) | dentry->msdos.first_cluster_lo );
            fat32_list_dentry(desc, next, current);
        }
    }

    fat32_destroy_iterator(&it);
    return 0;
}


int fat32_list_root(
    struct fat32_descriptor *desc )
{
    return fat32_list_dentry(desc, 2, "");
}


#ifdef FAT32_ENABLE_HASH

static uint16_t fat32_hash(
    const uint8_t *fileName )
{
    size_t i = 0;
    uint32_t hash = 0;
    while (i != FAT32_MAX_SFN)
    {
        hash += fileName[i++];
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;

    return (uint16_t) ( ((hash & 0xFFFF0000) >> 16) ^ (hash & 0xFFFF) );
}

#endif // FAT32_ENABLE_HASH


int fat32_create_iterator(
    struct dentry_iterator *it,
    struct fat32_descriptor *desc,
    uint32_t cluster,
    uint32_t flags )
{
    #ifdef FAT32_ENABLE_LFN
    it->fileNameLen = FAT32_MAX_LFN;
    it->buffer = (uint8_t*) malloc(desc->clusterSize + it->fileNameLen);
    #else
    it->fileNameLen = FAT32_MAX_SFN + 2;
    it->buffer = (uint8_t*) malloc(desc->clusterSize + it->fileNameLen);
    #endif
    if (it->buffer == NULL) return 1;
    it->fileName = (char*) it->buffer + desc->clusterSize;
    it->cluster = cluster;
    it->offset = 0;
    it->desc = desc;
    it->flags = flags;

    if (fat32_read_cluster(it->desc, it->cluster, it->buffer, it->desc->clusterSize) != 0)
    {
        free(it->buffer);
        return 1;
    }

    return 0;
}


int fat32_destroy_iterator(
    struct dentry_iterator *it )
{
    free(it->buffer);
    it->desc = NULL;
    it->buffer = NULL;
    it->fileName = NULL;
    it->cluster = 0;
    it->offset = 0;
    it->flags = 0;
    return 0;
}


int fat32_iterate(
    struct dentry_iterator *it,
    union dentry **dentry,
	const char **fileName )
{
    if (dentry == NULL || it == NULL || it->buffer == NULL || fileName == NULL) return 1;
    it->fileName[0] = 0;

    do {

        if (it->offset >= it->desc->clusterSize)
        {
            it->offset = 0;
            it->cluster = it->desc->fat[it->cluster];
            if (!IS_VALID_CLUSTER(it->desc, it->cluster)) return 1;
            fat32_read_cluster(it->desc, it->cluster, it->buffer, it->desc->clusterSize);
        }

        union dentry *entries = (union dentry *) it->buffer;

        for (size_t i = it->offset / 32; i < it->desc->clusterSize / 32; ++i)
        {
            it->offset += 32;
            // check whether we reached the end of the list
            if (entries[i].msdos.name[0] == 0) return 1;
            // volume ID inside root directory and deleted entries
            if (entries[i].msdos.attributes == ATTR_VOLUME_ID || entries[i].msdos.name[0] == 0xE5)
            {
                if ((it->flags & FAT32_ITF_ANY) == 0) continue;
                *fileName = NULL;
                *dentry = entries + i;
                return 0;
            }

            if ((entries[i].msdos.attributes & ATTR_LONG_NAME_MASK) == ATTR_LONG_NAME)
            {
                if ((it->flags & FAT32_ITF_ANY) == 0) continue;

                #ifdef FAT32_ENABLE_LFN
                fat32_parse_lfn(entries +i, it->fileName);
                #endif // FAT32_ENABLE_LFN
                *fileName = NULL;
                *dentry = entries + i;
                return 0;
            }
            else
            {
                #ifdef FAT32_ENABLE_LFN
                if (it->fileName[0] == '.' || entries[i].msdos.name[0] == '.')
                #else
                if (entries[i].msdos.name[0] == '.')
                #endif
                {
                    if ((it->flags & FAT32_ITF_ANY) == 0) continue;
                }

                #ifdef FAT32_ENABLE_LFN
                if (it->fileName[0] == 0)
                #endif
                {
                    fat32_format_sfn(entries +i, it->fileName, it->fileNameLen);
                }

                *fileName = it->fileName;
                *dentry = entries + i;
                return 0;
            }
        }
    } while (1);
}


int fat32_lookup(
    struct fat32_descriptor *desc,
    const char *path,
    struct fat32_dentry *dentry )
{
    if (desc == NULL || path == NULL || path[0] != '/' || dentry == NULL) return 1;
    if (strlen(path) > FAT32_MAX_PATH) return 1;

    char component[FAT32_MAX_LFN];
    union dentry *current = NULL;

    uint8_t *buffer = (uint8_t*) malloc(desc->clusterSize);
    if (buffer == NULL) return 1;

    uint32_t cluster = 2; // start from root directory
    path++; // discard the starting slash

    struct dentry_iterator it;
    fat32_create_iterator(&it, desc, cluster, 0);

    while (*path != 0)
    {
        // extract the current path component
        int i = 0;
        component[0] = 0;
        while (*path != '/' && *path != 0 && i + 1 < sizeof(component)) component[i++] = *path++;
        component[i] = 0;
        if (i == 0) break;
        ++path;

        dbg_printf("Looking for '%s'\n", component);
        char *fileName;
        union dentry *dentry;
        while (1)
        {
            if (fat32_iterate(&it, &dentry, &fileName) != 0) break;
            dbg_printf("Found file '%s'\n", fileName);
        }
        break;
    }

    fat32_destroy_iterator(&it);
    return 0;
}