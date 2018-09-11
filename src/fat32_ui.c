#include "thinfat32.h"
#include "fat32_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: Reimplement this function correctly
int fat_type(struct bpb *s)
{
	return TYPE_FAT32;
}

void printBPB(struct bpb *s) {
	int i,j;

	printf("\n==== BIOS PARAMETER BLOCK ====\n");
	printf("   Boot Instruction: 0x%02x%02x%02x\n", s->jump[0], s->jump[1], s->jump[2]);
	printf("           OEM Name: '%8s'\n", s->oem_name);
	printf("   Bytes per Sector: %d\n", s->bytes_per_sector);
	printf("Sectors per Cluster: %d\n", s->sectors_per_cluster);
	printf("   Reserved Sectors: %d\n", s->reserved_sectors);
	printf("               FATS: %d\n", s->number_fats);
	printf("       Root Entries: %d\n", s->root_entries);
	printf("Total Sectors (16b): %d\n", s->total_sectors_16);
	printf("              Media: "); print_media(s->media); printf("\n");
	printf("     Fat Size (16b): %d\n", s->fat_size_16);
	printf("  Sectors per Track: %d\n", s->sectors_per_track);
	printf("    Number of Heads: %d\n", s->number_heads);
	printf("     Hidden Sectors: %d\n", s->hidden_sectors);
	printf("Total Sectors (32b): %d\n", s->total_sectors_32);
	
	printf("\n");
	switch(fat_type(s)) 
	{
		case TYPE_FAT12: printf("           FAT Type: FAT12\n"); break;
		case TYPE_FAT16: printf("           FAT Type: FAT16\n"); break;
		case TYPE_FAT32: printf("           FAT Type: FAT32\n"); break;
		default: printf("FAT TYPE UNRECOGNIZED!\n"); break;
	}
	if((fat_type(s) == TYPE_FAT12) | (fat_type(s) == TYPE_FAT16)) 
	{
		printf("       Drive Number: %d\n", s->fat_specific.fat16.drive_number);
		printf("     Boot Signature: 0x%02x\n", s->fat_specific.fat16.boot_sig);
		printf("          Volume ID: 0x%04x\n", s->fat_specific.fat16.volume_id);
		printf("       Volume Label: '%.*s'\n", 11, s->fat_specific.fat16.volume_label);
		printf("     FileSystemType: '%.*s'\n", 8, s->fat_specific.fat16.fs_type);
	}
	else 
	{
		printf("     FAT Size (32b): %d\n", s->fat_specific.fat32.fat_size_32);
	   	printf("          FAT Flags: 0x%04x\n", s->fat_specific.fat32.flags);
		printf("         FS Version: 0x%04x\n", s->fat_specific.fat32.version);
		printf("       Root Cluster: %d\n", s->fat_specific.fat32.root_cluster);
		printf("      FSINFO Sector: %d\n", s->fat_specific.fat32.fs_info);
		printf("Bkup BootRec Sector: %d\n", s->fat_specific.fat32.backup_boot_sector);
		j=0;
		for(i=0; i<12; i++) {
			if(s->fat_specific.fat32.reserved1[i] != 0x00) j=1;
		}
		if(j) printf("      Reserved Area: NONZERO!\n");
		else  printf("      Reserved Area: Ok\n");
		printf("       Drive Number: %d\n", s->fat_specific.fat32.drive_number);
		printf("     Boot Signature: 0x%02x\n", s->fat_specific.fat32.boot_sig);
		printf("          Volume ID: 0x%04x\n", s->fat_specific.fat32.volume_id);
		printf("       Volume Label: '%.*s'\n", 11, s->fat_specific.fat32.volume_label);
		printf("     FileSystemType: '%.*s'\n", 8, s->fat_specific.fat32.fs_type);
	}

	printf("\n");
}

void print_media(unsigned char mediatype) {
	switch(mediatype) {
		case 0xf0: printf("Generic media type (0xf0)"); break;
		case 0xf8: printf("Hard disk (0xf8)"); break;
		case 0xf9: printf("3.5\" double sided 720k or 5.25 double sided 1.2MB (0xf9)"); break;
		case 0xfa: printf("5.25\" single sided 320k (0xfa)"); break;
		case 0xfb: printf("3.5\" double sided 640k (0xfb)"); break;
		case 0xfc: printf("5.25\" single sided 180k (0xfc)"); break;
		case 0xfd: printf("5.25\" double sided 360k (0xfd)"); break;
		case 0xfe: printf("5.25\" single sided 180k (0xfe)"); break;
		case 0xff: printf("5.25\" double sided 320k (0xff)"); break;
		default: printf("Unknown (0x%02x)",mediatype); break;
	}
}

void print_sector(unsigned char *sector) {
       int i;

	for(i=0; i<(512-8); i+=8) {
		printf(" %02x%02x%02x%02x %02x%02x%02x%02x        %c%c%c%c %c%c%c%c\n", sector[i], sector[i+1], sector[i+2], sector[i+3], sector[i+4], sector[i+5], sector[i+6], sector[i+7], sector[i], sector[i+1], sector[i+2], sector[i+3], sector[i+4], sector[i+5], sector[i+6], sector[i+7]);
	}
}

void print_tf_info(TFInfo *t) {
	printf("    TFInfo Structure\n    ----------------\n");
	switch(t->type) {
		case TF_TYPE_FAT16: printf("               Type: FAT16\n"); break;
		case TF_TYPE_FAT32: printf("               Type: FAT32\n"); break;
		default: printf("               Type: UNRECOGNIZED! (0x%02x)\n", t->type); break;
	}
	printf("Sectors Per Cluster: %d\n", t->sectorsPerCluster);
	printf("      Total Sectors: %d\n", t->totalSectors);
	printf("   Reserved Sectors: %d\n", t->reservedSectors);
	printf("  First Data Sector: %d\n", t->firstDataSector);
}

void print_TFFile(TFFile *fp) {
	printf("     TFFile Structure\n    ----------------\n");
	printf("    currentCluster: %d\n", fp->currentCluster);
	printf(" currentClusterIdx: %d\n", fp->currentClusterIdx);
	printf("parentStartCluster: %d\n", fp->parentStartCluster);
	printf("      startCluster: %d\n", fp->startCluster);
	printf("     currentSector: %d\n", fp->currentSector);
	printf("       currentByte: %d\n", fp->currentByte);
	printf("               pos: %d\n", fp->pos);
	printf("             flags: 0x%02x\n", fp->flags);
	printf("              size: %d bytes\n", fp->size);
}

void print_FatFileEntry(dentry_t *entry) {
	printf("    FatFile Structure\n    ---------------\n");
	if(entry->msdos.attributes == 0x0f) {
		print_FatFileLFN(&(entry->lfn));
	}
	else {
		print_FatFile83(&(entry->msdos));
	}
}
void print_FatFile83(struct short_name_dentry *entry) {
	printf("         Type: 8.3 Filename\n");
	printf("     Filename: %.*s\n", 8, entry->name);
	printf("    Extension: %.*s\n", 3,  entry->extension);
	printf("   Attributes: 0x%02x\n", entry->attributes);
	printf("First Cluster: %d\n", ((entry->first_cluster_hi & 0xffff) << 16) | ((entry->first_cluster_lo & 0xffff)));
	printf("Creation Time: %d/%d/%d\n", ((entry->creation_date & 0xfe00) >> 9) + 1980, ((entry->creation_date & 0x1e0) >> 5), (entry->creation_date & 0xf)*2);
}


void print_FatFileLFN(struct long_name_dentry *entry) {
	printf("         Type: Long Filename (LFN)\n");
}
