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


	// GNU's definitions of the attributes (http://www.gnu.org/software/libc/manual/html_node/Attribute-Meanings.html):
	// 		st_uid: 	The user ID of the file’s owner.
	//		st_gid: 	The group ID of the file.
	//		st_atime: 	This is the last access time for the file.
	//		st_mtime: 	This is the time of the last modification to the contents of the file.
	//		st_mode: 	Specifies the mode of the file. This includes file type information (see Testing File Type) and the file permission bits (see Permission Bits).
	//		st_nlink: 	The number of hard links to the file. This count keeps track of how many directories have entries for this file. If the count is ever decremented to zero, then the file itself is discarded as soon
	//						as no process still holds it open. Symbolic links are not counted in the total.
	//		st_size:	This specifies the size of a regular file in bytes. For files that are really devices this field isn’t usually meaningful. For symbolic links this specifies the length of the file name the link refers to.

	st->st_uid = getuid(); // The owner of the file/directory is the user who mounted the filesystem
	st->st_gid = getgid(); // The group of the file/directory is the same as the group of the user who mounted the filesystem
	st->st_atime = time( NULL ); // The last "a"ccess of the file/directory is right now
	st->st_mtime = time( NULL ); // The last "m"odification of the file/directory is right now

    if (dentry.msdos.attributes & ATTR_DIRECTORY)
    {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2; // Why "two" hardlinks instead of "one"? The answer is here: http://unix.stackexchange.com/a/101536
    }
    else
	{
		st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;
		st->st_size = dentry.msdos.size;
	}

	return 0;
}


static int do_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi )
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
        filler(buffer, fileName, NULL, 0);
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
/*
    uint32_t cluster = 0;
    if (fat32_first_cluster(&desc, &dentry, &cluster) != 0)
    {
        printf("readdir: '%s' points to invalud cluster\n", path );
        return -ENOENT;
    }

    size_t pending = FAT32_MIN(size, dentry.msdos.size);
    uint32_t jumps = (uint32_t) (offset / desc.clusterSize);
    while (jumps > 0)
    {
        if (fat32_next_cluster(&desc, cluster, &cluster) != 0)
        {
            printf("readdir: unexpected end of file\n");
            return 0;
        }
    }

    // compute the offset within the current cluster
    offset = offset - jumps * desc.clusterSize;
    // buffer for a cluster in memory
    uint8_t *page = (uint8_t*) malloc(desc.clusterSize);
    if (page == NULL) return 0;

    uint8_t *ptr = (uint8_t*) buffer;
    while (pending > 0)
    {
        // read the current cluster
        if (fat32_read_cluster(&desc, cluster, page, desc.clusterSize) != 0)
        {
            free(page);
            return 0;
        }
        // copy some data
        size_t rs = (size_t) FAT32_MIN(pending, desc.clusterSize - (uint32_t) offset);
        memcpy(ptr, page + offset, rs);
        printf("read: read %u bytes from cluster #%u (%u pending)\n", (uint32_t) rs, cluster, (uint32_t) pending);
        // update counters
        ptr += rs;
        pending -= rs;
        offset = 0;

        if (pending > 0) fat32_next_cluster(&desc, cluster, &cluster);
    }

    free(page);
	return (int) size;*/
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