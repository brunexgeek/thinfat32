#include <quark.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define FIRST_SECTOR(desc, cluster) \
    ( (cluster - 1) * (desc)->sector_per_cluster + (desc)->data_offset )

#define ALIGN_X(value, x)  \
    (uint32_t) ( ( (uint32_t)(value) + ( (uint32_t)(x) - 1) ) & ( ~( (uint32_t)(x) - 1) ) )

#define IS_VALID_CLUSTER(desc, cluster) \
    ( cluster != QC_FREE && cluster < QC_BAD && (cluster - 1) < (desc)->super.cluster_count)


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
    printf(" Indirect size: %u bytes\n", sb->indirect_size);
    printf(" Bitmap offset: sector %u\n", sb->bitmap_offset);
    printf("Bitmap sectors: %u\n", sb->bitmap_sectors);
    printf("   Root offset: cluster %u\n", sb->root_offset);
    printf("         Label: %.*s\n", ((sb->label[23] == 0) ? (int) strlen( (char*) sb->label ) : 24), (char*) sb->label);
    printf("   Data offset: sector %u\n", sb->data_offset);
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


int quark_read_cluster(
    struct quark_descriptor *desc,
    uint32_t cluster,
    uint8_t *buffer,
    size_t size )
{
    if (desc == NULL || buffer == NULL || size != desc->super.cluster_size) return 1;
    if (!IS_VALID_CLUSTER(desc, cluster)) return 1;

    uint32_t sector = FIRST_SECTOR(desc, cluster);

    size_t i = 0;
    for (; i < desc->super.cluster_size / desc->super.sector_size; ++i)
    {
        device_read(desc->device, (uint32_t) (sector + i), buffer + i * desc->super.sector_size);
    }

    printf("Read cluster #%d (%d sectors from sector #%d)\n", cluster, (uint32_t) i, sector);

    return 0;
}


int quark_format(
    struct storage_device *device,
    uint32_t diskSize )
{
    static const uint32_t SECTOR_SIZE = 512;
    static const uint32_t CLUSTER_SIZE = 1024;

    uint8_t sector[SECTOR_SIZE];
    memset(sector, 0, sizeof(sector));
    struct quark_superblock *sb = (struct quark_superblock *) &sector;

    //                              table estimative     sector 0
    uint32_t clusters = (diskSize - CLUSTER_SIZE) / CLUSTER_SIZE;

    // write superblock
    sb->signature      = MFS_SB_SIGNATURE;
    sb->version        = MFS_SB_VERSION;
    sb->bitmap_offset  = 1;
    sb->bitmap_sectors = clusters / 8 / SECTOR_SIZE + 1;
    sb->sector_size    = (uint16_t) SECTOR_SIZE;
    sb->cluster_size   = (uint16_t) CLUSTER_SIZE;
    sb->cluster_count  = clusters - ALIGN_X(sb->bitmap_sectors, CLUSTER_SIZE / SECTOR_SIZE);
    sb->indirect_size  = CLUSTER_SIZE;
    sb->root_offset    = 1;
    sb->data_offset    = sb->bitmap_offset + sb->bitmap_sectors;
    strcpy((char*) sb->label, "MACHINA");
    device_write(device, 0, sector);
    // print superblock
    printSuperblock(sb);
    device_write(device, 0, sector);

    uint16_t bitmapSectors = sb->bitmap_sectors;
    uint16_t bitmapOffset  = sb->bitmap_offset;
    sb = NULL;

    // write the bitmap
    memset(sector, 0, sizeof(sector));
    uint32_t *bitmap = (uint32_t*) sector;
    bitmap[0] = 0x00000001; // only one cluster for root directory
    device_write(device, bitmapOffset, sector);
    bitmap[0] = 0;
    for (size_t i = 1; i < bitmapSectors; ++i)
        device_write(device, (uint32_t) i + bitmapOffset, sector);
    bitmap = NULL;

    // clean root directory cluster
    uint32_t dataOffset = (uint32_t) bitmapOffset + (uint32_t) bitmapSectors;
    memset(sector, 0, sizeof(sector));
    for (uint32_t i = 0; i < CLUSTER_SIZE / SECTOR_SIZE; ++i)
        device_write(device, i + dataOffset, sector);
    // write root directory
    struct quark_dentry *dentry = (struct quark_dentry*) sector;
    dentry->name_hash = quark_hash( (const uint8_t*)"test.txt", 1);
    dentry->bits = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    dentry->bits |= AT_REGULAR;
    dentry->write_time = time(NULL);
    strcpy(dentry->name, "test.txt");

    device_write(device, dataOffset, sector);

    return 0;
}


int quark_mount(
	struct quark_descriptor *desc,
	struct storage_device *device )
{
    if (desc == NULL || device == NULL) return 1;

    uint8_t buffer[512];
    device_read(device, 0, buffer);
    memcpy(&desc->super, buffer, sizeof(struct quark_superblock));

    desc->device = device;
    desc->data_offset = desc->super.data_offset;

    desc->bitmap = (uint32_t*) malloc( (size_t) desc->super.bitmap_sectors * (size_t) desc->super.sector_size);

    return 0;
}


int quark_umount(
	struct quark_descriptor *desc )
{
    (void) desc;
    return 0;
}

/*
int quark_next_cluster(
    const struct quark_descriptor *desc,
    uint32_t cluster,
    uint32_t *next )
{
    cluster = (uint32_t) (cluster & 0x0FFFFFFF);

    if (cluster < 1 || cluster >= QC_BAD) return 1;

    // cluster index starts from 1
    if ((cluster - 1) >= desc->super.cluster_count) return 1;

    *next = desc->table[cluster];
    return 0;
}*/


int quark_create_iterator(
    struct dentry_iterator *it,
    struct quark_descriptor *desc,
    struct quark_dentry *parent,
    uint32_t flags )
{
    it->buffer = (uint8_t*) malloc(desc->super.cluster_size + QD_MAX_NAME + 1);
    if (it->buffer == NULL) return 1;
    it->fileName = (char*) it->buffer + desc->super.cluster_size;
    it->parent = parent;
    it->cluster = parent->slots[0].pointer;
    it->offset = 0;
    it->desc = desc;
    it->flags = flags;

    if (quark_read_cluster(it->desc, it->cluster, it->buffer, it->desc->super.cluster_size) != 0)
    {
        quark_destroy_iterator(it);
        return 1;
    }

    return 0;
}

/*
int quark_reset_iterator(
    struct dentry_iterator *it )
{
    it->fileName[0] = 0;
    it->cluster = cluster;
    it->offset = 0;

    if (quark_read_cluster(it->desc, it->cluster, it->buffer, it->desc->super.cluster_size) != 0)
    {
        quark_destroy_iterator(it);
        return 1;
    }
    return 0;
}*/


int quark_destroy_iterator(
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


int quark_next_iteration(
    struct dentry_iterator *it )
{
    for (size_t i = 1; i < QD_DIR_SLOTS; ++i)
    {
        if (it->parent->slots[i - 1].pointer == it->cluster)
        {
            it->cluster = it->parent->slots[i].pointer;
            quark_read_cluster(it->desc, it->cluster, it->buffer, it->desc->super.cluster_size);
            return 0;
        }
    }

    it->cluster = 0;
    return 1;
}


int quark_iterate(
    struct dentry_iterator *it,
    struct quark_dentry **dentry,
	const char **fileName )
{
    if (dentry == NULL || it == NULL || it->buffer == NULL || fileName == NULL) return 1;
    it->fileName[0] = 0;

    do {

        if (it->offset >= it->desc->super.cluster_size)
        {
            it->offset -= it->desc->super.cluster_size;
            quark_next_iteration(it);
        }

        struct quark_dentry *entries = (struct quark_dentry *) it->buffer;

        for (size_t i = it->offset / QUARK_DENTRY_SIZE; i < it->desc->super.cluster_size / QUARK_DENTRY_SIZE; ++i)
        {
            it->offset += QUARK_DENTRY_SIZE;
            // check whether we reached the end of the list
            if (entries[i].name[0] == 0) return 1;
            // handle deleted entries
            if (entries[i].bits & AT_DELETE)
            {
                continue;
                /*
                if ((it->flags & FAT32_ITF_ANY) == 0) continue;
                *fileName = NULL;
                *dentry = entries + i;
                return 0;*/
            }

            // Machina Long File name entry (ATTR_VOLUME_ID | ATTR_SYSTEM)
            if ( (entries[i].bits & AT_REGULAR) || (entries[i].bits & AT_DIRECTORY) )
            {
                strncpy(it->fileName, entries[i].name, 12);
                it->fileName[12] = 0;
                *fileName = it->fileName;
                *dentry = entries + i;
                return 0;
            }
        }
    } while (1);
}


static int quark_list_dentry(
    struct quark_descriptor *desc,
    struct quark_dentry *parent,
    const char *path )
{
    struct dentry_iterator it;
    quark_create_iterator(&it, desc, parent, 0);

    struct quark_dentry *dentry = NULL;
    const char *fileName = NULL;

    while (quark_iterate(&it, &dentry, &fileName) == 0)
    {
        #ifdef FAT32_ENABLE_HASH
        printf("[0x%04x] [0x%04x] %s/%s\n", fat32_hash(dentry->msdos.name), dentry->msdos.hash, path, fileName);
        #else
        printf("%s/%s\n", path, fileName);
        #endif

        // if the current entry is a directory, recusively list its files
        if (dentry->bits & AT_DIRECTORY)
        {
            char current[256] = { 0 };
            strcpy(current, path);
            strcat(current, "/");
            strcat(current, fileName);

            quark_list_dentry(desc, dentry, current);
        }
    }

    quark_destroy_iterator(&it);
    return 0;
}


int quark_list_root(
    struct quark_descriptor *desc )
{

    return quark_list_dentry(desc, desc->super.root_offset, "");
}


int quark_lookup(
    struct quark_descriptor *desc,
    const char *path,
    struct quark_dentry *dentry)
{
    if (desc == NULL || path == NULL || path[0] != '/' || dentry == NULL) return 1;
    if (strlen(path) > QUARK_MAX_PATH) return 1;

    char component[QUARK_MAX_NAME];

    uint8_t *buffer = (uint8_t*) malloc(desc->super.cluster_size);
    if (buffer == NULL) return 1;

    uint32_t cluster = desc->super.root_offset; // start from root directory
    path++; // discard the starting slash

    struct quark_dentry *ptr = NULL;
    struct dentry_iterator it;
    quark_create_iterator(&it, desc, cluster, 0);

    while (*path != 0)
    {
        ptr = NULL;

        // extract the current path component
        size_t i = 0;
        component[0] = 0;
        while (*path != '/' && *path != 0 && i + 1 < sizeof(component)) component[i++] = *path++;
        if (i == 0) break;
        if (*path == '/') ++path;
        component[i] = 0;

        printf("Looking for '%s'\n", component);
        const char *fileName;

        while (ptr == NULL)
        {
            if (quark_iterate(&it, &ptr, &fileName) != 0) break;

            if (strcmp(fileName, component) == 0)
            {
                printf("  --'%s'? Yep\n", fileName);

                // we have more components to find?
                if (*path != 0)
                {
                    // go to next cluster
                    quark_next_iteration(&it);
                }
                else
                {
                    // copy the current to output
                    memcpy(dentry, ptr, sizeof(struct quark_dentry));
                }
            }
            else
            {
                printf("  --'%s'? Nope\n", fileName);
                ptr = NULL;
            }
        }
    }

    quark_destroy_iterator(&it);
    return (ptr != NULL) ? 0 : 1;
}


int quark_read(
    struct quark_descriptor *desc,
    struct quark_dentry *dentry,
    uint8_t *buffer,
    uint32_t size,
    uint32_t offset )
{
    return 1;
    /*
    if (desc == NULL || dentry == NULL || buffer == NULL || size == 0) return 1;

    uint32_t cluster = dentry->slots[0].pointer;

    size_t pending = QUARK_MIN(size, dentry->size);
    uint32_t jumps = (uint32_t) (offset / desc->super.cluster_size);
    while (jumps > 0)
    {
        if (quark_next_cluster(desc, cluster, &cluster) != 0)
        {
            printf("unexpected end of file\n");
            return 0;
        }
    }

    // compute the offset within the current cluster
    offset = offset - jumps * desc->super.cluster_size;
    // buffer for a cluster in memory
    uint8_t *page = (uint8_t*) malloc(desc->super.cluster_size);
    if (page == NULL) return 0;

    uint8_t *ptr = (uint8_t*) buffer;
    while (pending > 0)
    {
        // read the current cluster
        if (quark_read_cluster(desc, cluster, page, desc->super.cluster_size) != 0)
        {
            free(page);
            return 0;
        }
        // copy some data
        size_t rs = (size_t) QUARK_MIN(pending, desc->super.cluster_size - (uint32_t) offset);
        memcpy(ptr, page + offset, rs);
        printf("read %u bytes from cluster #%u (%u pending)\n", (uint32_t) rs, cluster, (uint32_t) pending);
        // update counters
        ptr += rs;
        pending -= rs;
        offset = 0;

        if (pending > 0) quark_next_cluster(desc, cluster, &cluster);
    }

    free(page);
	return (int) size;*/
}

