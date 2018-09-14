#include <fat32_ui.h>
#include <fat32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int fat_type( struct bpb *bpb)
{
	(void) bpb;
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


void print_fsinfo(
	struct fs_info *fsi )
{
	printf("\n==== FILE SYSTEM INFORMATION ====\n");
	printf("     Load signature: 0x%8x\n", fsi->lead_signature);
	printf("   Struct signature: 0x%8x\n", fsi->struct_signature);
	printf("      Free clusters: %d\n", fsi->free_count);
	printf("  Next free cluster: %d\n", fsi->next_free);
	printf("     Tail signature: 0x%8x\n", fsi->trail_signature);
}


