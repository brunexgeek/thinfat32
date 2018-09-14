#ifndef __FAT32_UI_H
#define __FAT32_UI_H

#include <fat32.h>
#include "thinternal.h"

void printBPB(struct bpb *s);
void print_sector(unsigned char *sector);
void print_media(unsigned char mediatype);
void print_fsinfo(
	struct fs_info *fsi );

#endif
