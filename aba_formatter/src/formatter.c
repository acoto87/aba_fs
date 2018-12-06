
#define FUSE_USE_VERSION 25
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <math.h>
#include "formater.h"
#include "declarations.h"
#include "error.h"
#include "mem_alloc.h"
#include "bitmap.h"
#include "global.h"

int FormatDisk(char *disk_filename, const u64 disk_size, const u32 cluster_size){
	FILE *fp;
	if((fp = fopen(disk_filename, "r")) == NULL){
		fp = CreateDisk(disk_filename, disk_size, cluster_size);
	}
	if((fp = freopen(disk_filename, "rw+", fp)) == NULL){
		error(-EACCES, "Error formateando la particion");
		return -1;
	}
	//rellenar la estructura boot_sector
	BOOT_SECTOR = (struct aba_boot_sector*)xmalloc(sizeof(struct aba_boot_sector));
	FillBootSector(disk_size, cluster_size, BOOT_SECTOR);
	if(WriteCluster(0, (u8*)BOOT_SECTOR, fp) != 0){
		error(-EIO, "Error de escritura en disco, no se puedo escribir el BOOT_SECTOR");
		free(BOOT_SECTOR);
		fclose(fp);
		return -EIO;
	}
	//rellenar los clusters de datos del bitmap
	u64 bmp_clusters_count = (BOOT_SECTOR->disk_size/BOOT_SECTOR->cluster_size)/(BOOT_SECTOR->cluster_size*8) + 1;
	u64 bmp_size = (BOOT_SECTOR->disk_size/BOOT_SECTOR->cluster_size)/8;
	u8 *bitmap_data = (u8*)xcalloc(1, bmp_size);
	u64 i;
	//setear el bitmap
	for(i=0; i<1 + bmp_clusters_count + 1; i++){
		bit_set(bitmap_data, i, 1);
	}
	//crear el fichero de la Mft
	mft_file_record *mft = InitializeMft();
	bit_set(bitmap_data, mft->dataCluster, 1);
	u8 *mftdata = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
	if(ReadCluster(mft->dataCluster, mftdata, fp) != 0){
		error(-EIO, "Error de lectura en disco");
		free(bitmap_data);
		free(mft);
		free(BOOT_SECTOR);
		fclose(fp);
		return -EIO;
	}
	memset(mftdata, 0, BOOT_SECTOR->cluster_size);
	u64 start = BOOT_SECTOR->logical_mft_cluster;
	u64 count = 1;
	memcpy(mftdata, &start, 8);
	memcpy(mftdata + 8, &count, 8);
	if(WriteCluster(mft->dataCluster, mftdata, fp) != 0){
		error(-EIO, "Error de lectura en disco");
		free(bitmap_data);
		free(mft);
		free(BOOT_SECTOR);
		fclose(fp);
		return -EIO;
	}
	free(mftdata);
	if(MftWriteRecord(mft->record_number, mft, fp) != 0){
		error(-EIO, "Error de lectura en disco");
		free(bitmap_data);
		free(mft);
		free(BOOT_SECTOR);
		fclose(fp);
		return -EIO;
	}

	//crear el directorio Root
	mft_file_record *root = InitializeRootDir();
	bit_set(bitmap_data, root->dataCluster, 1);
	u8 *rootdata = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
	if(ReadCluster(root->dataCluster, rootdata, fp) != 0){
		error(-EIO, "Error de lectura en disco");
		free(bitmap_data);
		free(mft);
		free(root);
		free(BOOT_SECTOR);
		fclose(fp);
		return -EIO;
	}
	memset(rootdata, 0, BOOT_SECTOR->cluster_size);
	directory_entry *mftentry = (directory_entry*)xmalloc(sizeof(directory_entry));
	mftentry->entry_number = 0;
	mftentry->name_len = 4;
	char *name = "$mft";
	memcpy(mftentry->filename, name, 4);
	memcpy(rootdata, mftentry, 8 + 1 + mftentry->name_len);
	if(WriteCluster(root->dataCluster, rootdata, fp) != 0){
		error(-EIO, "Error de lectura en disco");
		free(bitmap_data);
		free(mft);
		free(root);
		free(BOOT_SECTOR);
		fclose(fp);
		return -EIO;
	}
	free(rootdata);
	if(MftWriteRecord(root->record_number, root, fp) != 0){
		error(-EIO, "Error de lectura en disco");
		free(bitmap_data);
		free(mft);
		free(root);
		free(BOOT_SECTOR);
		fclose(fp);
		return -EIO;
	}

	//guardar el bitmap
	for(i=0; i<bmp_clusters_count; i++){
		if(WriteCluster(1 + i, bitmap_data + BOOT_SECTOR->cluster_size*i, fp) != 0)
			break;
	}
	free(bitmap_data);
	free(mft);
	free(root);
	free(BOOT_SECTOR);
	fclose(fp);
	return 0;
}

FILE* CreateDisk(const char* disk_filename, const u64 disk_size, const u32 cluster_size){
	FILE *fp;
	if((fp = fopen(disk_filename, "w")) == NULL){
		error(-EACCES, "Error formateando la particion");
		return NULL;
	}
	void *zero = xcalloc(cluster_size, 1);
	u64 i;
	for(i=0; i<disk_size/(128*1024); i++){
		if(fwrite(zero, cluster_size, 128, fp) < 1){
			error(-EIO, "Error de escritura en disco");
			return NULL;
		}
	}
	return fp;
}

void FillBootSector(const int disk_size, const int cluster_size, struct aba_boot_sector *bs){
	bs->disk_size = disk_size;
	bs->cluster_size = cluster_size;
	bs->number_of_clusters = bs->disk_size/bs->cluster_size;
	bs->logical_mft_cluster = 1 + (BOOT_SECTOR->disk_size/BOOT_SECTOR->cluster_size)/(BOOT_SECTOR->cluster_size*8) + 1;
	bs->mft_zone_clusters = ((disk_size*12.5)/100)/cluster_size;
	bs->mft_record_count = 2;
}

mft_file_record* InitializeMft(){
	mft_file_record *mft = (mft_file_record*)xmalloc(sizeof(mft_file_record));
	mft->mode = S_IFREG;
	mft->nlinks = 1;
	mft->uid = UID_SUPERUSER;
	mft->gid = 0;
	mft->size = 128;
	mft->ctime = 0;
	mft->atime = 0;
	mft->mtime = 0;
	mft->record_number = 0;
	mft->ndir_entry = 0;
	mft->nr_level = INDIRECT;
	mft->dataCluster = BOOT_SECTOR->logical_mft_cluster + BOOT_SECTOR->mft_zone_clusters;
	return mft;
}

mft_file_record* InitializeRootDir(){
	mft_file_record *root = (mft_file_record*)xmalloc(sizeof(mft_file_record));
	root->mode = S_IFDIR | S_ISUID | S_ISGID | S_IRUSR | S_IWUSR;
	root->nlinks = 0;
	root->uid = UID_SUPERUSER;
	root->gid = 0;
	root->size = 128;
	root->ctime = 0;
	root->atime = 0;
	root->mtime = 0;
	root->record_number = 1;
	root->ndir_entry = 1;
	root->nr_level = DIRECT;
	root->dataCluster = BOOT_SECTOR->logical_mft_cluster + BOOT_SECTOR->mft_zone_clusters + 1;
	return root;
}

s32 ReadCluster(u64 cluster, u8 *data, FILE *fp){
	if(cluster < 0 || data == NULL || fp == NULL)
		return -1;
	u64 byteToRead = cluster*BOOT_SECTOR->cluster_size;
	if(fseek(fp, byteToRead, 0) == -1){
		error(-EACCES, "Error de acceso al disco, leyendo el cluster %l\nFuncion: ReadCluster\n", cluster);
		return -EACCES;
	}
	if(fread(data, BOOT_SECTOR->cluster_size, 1, fp) < 1){
		error(-EIO, "Error de lectura de disco, leyendo el cluster %l\nFuncion: ReadCluster\n", cluster);
		return -EIO;
	}
	return 0;
}

s32 WriteCluster(u64 cluster, u8 *data, FILE *fp){
	int res=0;
	if(cluster < 0 || data == NULL || fp == NULL)
		return -1;
	u64 byteToRead = cluster*BOOT_SECTOR->cluster_size;
	if(fseek(fp, byteToRead, 0) == -1){
		error(-EACCES, "Error de acceso al disco\nFuncion: WriteCluster\n");
		return -EACCES;
	}
	if((res = fwrite(data, BOOT_SECTOR->cluster_size, 1, fp)) < 1){
		error(-EIO, "Error de lectura de disco\nFuncion: WriteCluster\n");
		return -EIO;
	}
	return 0;
}

s32 MftReadRecord(u64 record_number, mft_file_record *record, FILE *fp){
	u64 clusterToRead = BOOT_SECTOR->logical_mft_cluster + record_number/(BOOT_SECTOR->cluster_size/MFT_RECORD_SIZE);
	u32 offset = record_number%(BOOT_SECTOR->cluster_size/MFT_RECORD_SIZE);

	u8 *record_data = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
	s32 errorCode=0;
	if((errorCode = ReadCluster(clusterToRead, record_data, fp)) < 0){
		return errorCode;
	}
	memcpy(record, record_data + MFT_RECORD_SIZE*offset, MFT_RECORD_SIZE);

	free(record_data);
	return 0;
}

s32 MftWriteRecord(u64 record_number, mft_file_record *record, FILE *fp){
	u64 clusterToWrite = BOOT_SECTOR->logical_mft_cluster + record_number/(BOOT_SECTOR->cluster_size/MFT_RECORD_SIZE);
	u32 offset = record_number%(BOOT_SECTOR->cluster_size/MFT_RECORD_SIZE);

	u8 *record_data = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
	s32 errorCode=0;
	if((errorCode = ReadCluster(clusterToWrite, record_data, fp)) != 0){
		return errorCode;
	}
	memcpy(record_data + MFT_RECORD_SIZE*offset, record, sizeof(mft_file_record));
	if((errorCode = WriteCluster(clusterToWrite, record_data, fp)) != 0){
		return errorCode;
	}
	free(record_data);
	return 0;
}
