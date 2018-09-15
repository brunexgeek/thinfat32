#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION  30

#include <device.h>
#include <fat32.h>
#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>


static struct fat32_descriptor desc;


static uint32_t make_time(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t hour,
    uint32_t minute,
    uint32_t second )
{
    if (day < 1 || day > 31 || month < 1 || month > 12 || year < 1970 || year > 2037) return 0;

    year -= (month <= 2);
    const uint32_t era = year / 400;
    const uint32_t yoe = (uint32_t) (year - era * 400);      // [0, 399]
    const uint32_t doy = (153* ( (month > 2 ? month - 3 : month + 9)) + 2)/5 + day - 1;  // [0, 365]
    const uint32_t doe = yoe * 365 + yoe/4 - yoe/100 + doy;         // [0, 146096]
    uint32_t output = era * 146097 + (uint32_t) (doe) - 719468; // total days
    return (output * 24 * 60 * 60) + (hour * 60 * 60) + (minute * 60) + second;
}


#define FAT32_DAY(x)    ( (x) & 0x001F )
#define FAT32_MONTH(x)  ( ((x) & 0x01E0) >> 5 )
#define FAT32_YEAR(x)   ( (((x) & 0xFE00) >> 9 ) + 1980 )
#define FAT32_SECOND(x) ( (x) & 0x001F )
#define FAT32_MINUTE(x) ( ((x) & 0x07E0) >> 5 )
#define FAT32_HOUR(x)   ( ((x) & 0xF800) >> 11 )


static void fill_stat(
    union dentry *dentry,
    struct stat *st )
{
	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime = (time_t) make_time(
        FAT32_YEAR(dentry->msdos.access_date),
        FAT32_MONTH(dentry->msdos.access_date),
        FAT32_DAY(dentry->msdos.access_date),
        0, 0, 0 );
	st->st_mtime = (time_t) make_time(
        FAT32_YEAR(dentry->msdos.write_date),
        FAT32_MONTH(dentry->msdos.write_date),
        FAT32_DAY(dentry->msdos.write_date),
        FAT32_HOUR(dentry->msdos.write_time),
        FAT32_MINUTE(dentry->msdos.write_time),
        FAT32_SECOND(dentry->msdos.write_time) );

    if (dentry->msdos.attributes & ATTR_DIRECTORY)
    {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2; // Why "two" hardlinks instead of "one"? The answer is here: http://unix.stackexchange.com/a/101536
    }
    else
	{
		st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;
		st->st_size = dentry->msdos.size;
	}
}


static int do_getattr(
    const char *path,
    struct stat *st )
{
	printf("getattr: attributes of '%s' requested\n", path );

    if (path[0] == '/' && path[1] == 0)
    {
        st->st_uid = getuid();
        st->st_gid = getgid();
        st->st_atime = time( NULL );
	    st->st_mtime = time( NULL );
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        return 0;
    }

    union dentry dentry;
    if (fat32_lookup(&desc, path, &dentry) != 0)
    {
        printf("getattr: file '%s' not found\n", path );
        return -ENOENT;
    }

    fill_stat(&dentry, st);

	return 0;
}

/*
static uint32_t make_time(
    int year,
    int month,
    int day,
    int hour,
    int minute,
    int second )
{
    year -= (month <= 2);
    const uint32_t era = (year >= 0 ? year : year - 399) / 400;
    const uint32_t yoe = (uint32_t) (year - era * 400);      // [0, 399]
    const uint32_t doy = (153*(month + (month > 2 ? -3 : 9)) + 2)/5 + day - 1;  // [0, 365]
    const uint32_t doe = yoe * 365 + yoe/4 - yoe/100 + doy;         // [0, 146096]
    uint32_t output = era * 146097 + (uint32_t) (doe) - 719468; // total days
    return (output * 24 * 60 * 60) + (hour * 60 * 60) + (minute * 60) + second;
}*/


static int do_readdir(
    const char *path,
    void *buffer,
    fuse_fill_dir_t filler,
    off_t offset,
    struct fuse_file_info *fi )
{
    (void) offset;
    (void) fi;

	printf("readdir: listing '%s'\n", path );

    uint32_t cluster = 2; // root directory cluster

    // we need to find the corresponding dentry if path is not '/'
    if (path[0] == '/' && path[1] != 0)
    {
        union dentry dentry;
        if (fat32_lookup(&desc, path, &dentry) != 0)
        {
            printf("readdir: directory '%s' not found\n", path );
            return -ENOENT;
        }
        if ((dentry.msdos.attributes & ATTR_DIRECTORY) == 0)
        {
            printf("readdir: '%s' is not a directory\n", path );
            return -ENOENT;
        }

        if (fat32_first_cluster(&desc, &dentry, &cluster) != 0)
        {
            printf("readdir: '%s' points to invalud cluster\n", path );
            return -ENOENT;
        }
    }

    struct dentry_iterator it;
    if (fat32_create_iterator(&it, &desc, cluster, 0) != 0)
    {
        printf("readdir: unable to create a dentry iterator\n" );
        return -ENOENT;
    }

	filler(buffer, ".", NULL, 0);
	filler(buffer, "..", NULL, 0);

    union dentry *current = NULL;
    const char *fileName = NULL;
    int count = 0;
    while (fat32_iterate(&it, &current, &fileName) == 0)
    {
        ++count;
        struct stat st;
        fill_stat(current, &st);
        filler(buffer, fileName, &st, 0);
    }

    fat32_destroy_iterator(&it);

    printf("readdir: returned %d entries\n", count );

	return 0;
}


static int do_read( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi )
{
    (void) fi;

    union dentry dentry;
    if (fat32_lookup(&desc, path, &dentry) != 0)
    {
        printf("readdir: directory '%s' not found\n", path );
        return -ENOENT;
    }
    if ((dentry.msdos.attributes & ATTR_DIRECTORY) != 0)
    {
        printf("readdir: cannot read a directory\n");
        return -ENOENT;
    }

    printf( "read: reading %u bytes of '%s' starting from %u\n",
        (uint32_t) FAT32_MIN(size, dentry.msdos.size), path, (uint32_t) offset);

    return fat32_read(&desc, &dentry, (uint8_t*) buffer, (uint32_t) size, (uint32_t) offset);
}


static void do_destroy()
{
    printf("destroy: unmounting");
    fat32_umount(&desc);
}


static struct fuse_operations operations = {
    .getattr	= do_getattr,
    .readdir	= do_readdir,
    .read		= do_read,
    .destroy    = do_destroy,
};


int main( int argc, char *argv[] )
{
    struct storage_device device;
    device_open(&device, "test.fat32");

    fat32_mount(&desc, &device);

	return fuse_main( argc, argv, &operations, NULL );
}