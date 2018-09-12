#ifndef __FAT32_UI_H
#define __FAT32_UI_H

#include <fat32.h>
#include "thinternal.h"

void printBPB(struct bpb *s);
void print_sector(unsigned char *sector);
void print_media(unsigned char mediatype);
//void print_TFFile(TFFile *fp);
//void print_tf_info(TFInfo *t);

void print_FatFileEntry(dentry_t *entry);
void print_FatFileLFN(struct long_name_dentry *entry);
void print_FatFile83(struct short_name_dentry *entry);

#endif
