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


static void fill_stat(
    union dentry *dentry,
    struct stat *st )
{
	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime = time( NULL );
	st->st_mtime = time( NULL );

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