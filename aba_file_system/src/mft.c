
#define FUSE_USE_VERSION 25
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <errno.h>
#include "mft.h"
#include "declarations.h"
#include "global.h"
#include "error.h"
#include "mem_alloc.h"
#include "bitmap.h"
#include "space_manager.h"
#include "AbaFileSystem.h"

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
	u64 request = record_number/(BOOT_SECTOR->cluster_size/MFT_RECORD_SIZE)+1;
	u64 clusterToRead = FindCluster(0, request, fp);
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
	u64 request = record_number/(BOOT_SECTOR->cluster_size/MFT_RECORD_SIZE)+1;
	u64 clusterToWrite = FindCluster(0, request, fp);
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
	if((errorCode = ReserveCluster(record->dataCluster, fp)) != 0){
		return errorCode;
	}
	free(record_data);
	return 0;
}
directory_entry* FindDirEntryInternal(u64 cluster, NON_RESIDENT_LEVEL nr_level, u32 *ndir_entry, const char *filename, FILE *fp){
	directory_entry *entry = NULL;
	u8 *dataCluster = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
	if(ReadCluster(cluster, dataCluster, fp) != 0)
		return NULL;
	if(nr_level == DIRECT){
		entry = (directory_entry*)xcalloc(1, sizeof(directory_entry));
		u32 currentByte=0;
		while(*ndir_entry > 0 && currentByte < BOOT_SECTOR->cluster_size - 8 - 1){
			memcpy(entry, &dataCluster[currentByte], 8 + 1 + dataCluster[currentByte+8]);
			if(entry->name_len <= 0 || strcmp(entry->filename, filename) == 0)
				break;
			memset(entry, 0, sizeof(directory_entry));
			currentByte += 8 + 1 + dataCluster[currentByte+8];
			(*ndir_entry)--;
		}
		if(entry->name_len > 0)
			return entry;
		free(entry);
	}
	else{
		u64 *data = (u64*)dataCluster;
		u32 i;
		for(i=0; i<BOOT_SECTOR->cluster_size/8; i+=2){
			if(data[i] == 0)
				break;
			u32 j;
			for(j=0; j<data[i+1]; j++){
				entry = FindDirEntryInternal(data[i] + j, nr_level-1, ndir_entry, filename, fp);
				if(entry != NULL)
					return entry;
			}
		}
	}
	free(dataCluster);
	return NULL;
}
directory_entry* FindDirEntry(mft_file_record* dir, const char *filename, FILE *fp){
	u32 ndir_entry = dir->ndir_entry;
	return FindDirEntryInternal(dir->dataCluster, dir->nr_level, &ndir_entry, filename, fp);
}
/*
 * Funcion que resuelve recursivamente el anadir una entrada de directorio a un directorio
 * Devuelve 0 si fue bien
 * Devuelve 1 si no pudo insertar, o sea tiene que seguir buscando algun hueco
 * Devuelve >1 si tuvo que coger otro cluster para insertar
 * Devuelve <0 si hubo algun error
 */
s64 AddDirEntryInternal(u64 cluster, NON_RESIDENT_LEVEL nr_level, u32 *ndir_entry, directory_entry *entry, FILE *fp){
	s32 res=0;
	u8 *dataCluster = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
	if((res = ReadCluster(cluster, dataCluster, fp)) != 0){
		return res;
    }
	if(nr_level == DIRECT){
		u32 currentByte=0;
		while(*ndir_entry >= 0 && currentByte < BOOT_SECTOR->cluster_size - 8 - 1){
			u64 namelen = (u64)dataCluster[currentByte+8];
			if(namelen <= 0 || *ndir_entry == 0){
				if(BOOT_SECTOR->cluster_size - currentByte > 8 + 1 + entry->name_len){
					memcpy(dataCluster + currentByte, entry, 8 + 1 + entry->name_len);
				}
				else
					res = 1;
				break;
			}
			else{
				(*ndir_entry)--;
				currentByte += 8 + 1 + dataCluster[currentByte + 8];
			}
		}
		if(currentByte > BOOT_SECTOR->cluster_size - 8 - 1 - entry->name_len){
			if(*ndir_entry == 0){
				u64 len = 1;
				u64 newcluster = FindEmptyClusters(&len, fp);
				if(len < 1){
					error(-ENOSPC, "Disco lleno");
					return -ENOSPC;
				}
				if((res = CleanCluster(newcluster, fp)) != 0){
					return res;
				}
				res = AddDirEntryInternal(newcluster, nr_level, ndir_entry, entry, fp);
				if(res == 0)
					res = newcluster;
			}
			else res = 1;
		}
	}
	else{
		u64 *data = (u64*)dataCluster;
		u32 i;
		for(i=0; i<BOOT_SECTOR->cluster_size/8; i+=2){
			if(data[i] == 0)
				break;
			u32 j;
			for(j=0; j<data[i+1]; j++){
				res = AddDirEntryInternal(data[i] + j, nr_level-1, ndir_entry, entry, fp);
				if(res <= 0){
					free(dataCluster);
					return res;
				}
				if(res > 1){
					if(i < BOOT_SECTOR->cluster_size/8){
						// if can join the last assigned cluster
						if(res == data[i] + data[i+1]){
							data[i+1] += 1;
						}
						else if(i < BOOT_SECTOR->cluster_size/8 - 2){
							data[i+2] = res;
							data[i+3] = 1;
						}
						if((res = WriteCluster(cluster, dataCluster, fp)) != 0){
							return res;
						}
						free(dataCluster);
						return res;
					}
					else{
						u64 newClusterReturned = res;
						u64 len = 1;
						u64 newcluster = FindEmptyClusters(&len, fp);
						if(len < 1){
							error(-ENOSPC, "Disco lleno");
							return -ENOSPC;
						}
						if((res = CleanCluster(newcluster, fp)) != 0){
							return res;
						}
						u64 *dataNewCluster = (u64*)xcalloc(1, BOOT_SECTOR->cluster_size);
						if((res = ReadCluster(newcluster, (u8*)dataNewCluster, fp)) != 0)
							return res;
						data[0] = newClusterReturned;
						data[1] = 1;
						if((res = WriteCluster(newcluster, (u8*)dataNewCluster, fp)) != 0)
							return res;
						if((res = WriteCluster(cluster, dataCluster, fp)) != 0){
							return res;
						}
						free(dataCluster);
						return newcluster;
					}
				}
				else
					res = -1;
			}
		}
	}
	s32 r=0;
	if((r = WriteCluster(cluster, dataCluster, fp)) != 0){
		return r;
	}
	free(dataCluster);
	return res;
}
s32 AddDirEntry(mft_file_record *dir, directory_entry *entry, FILE *fp){
	s64 res=0;
	
	if(dir->dataCluster == 0){
		u64 len=1;
		dir->dataCluster = FindEmptyClusters(&len, fp);
		if(len < 1){
			error(-ENOSPC, "Error, full disk");
			return -ENOSPC;
		}
		if((res = CleanCluster(dir->dataCluster, fp)) != 0){
			return res;
		}
	}
	u32 ndir_entry = dir->ndir_entry;
	res = AddDirEntryInternal(dir->dataCluster, dir->nr_level, &ndir_entry, entry, fp);
	if(res > 1){
		u64 newCluster = res;
		if((res = MakeNonResident(dir, fp)) != 0)
			return res;
		u8* data = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
		s32 result=0;
		if((result = ReadCluster(dir->dataCluster, data, fp)) != 0)
			return result;
		u64 *dataCluster = (u64*)data;
		if(newCluster == dataCluster[0] + dataCluster[1]){
			dataCluster[1]++;
		}
		else{
			dataCluster[2] = newCluster;
			dataCluster[3] = 1;
		}
		if((result = WriteCluster(dir->dataCluster, data, fp)) != 0)
			return result;
	}
	else if(res < 0)
		return res;
	dir->ndir_entry++;
	return res;
}
s64 RemoveDirEntryInternal(u64 cluster, NON_RESIDENT_LEVEL nr_level, u32 *ndir_entry, u64 record_number, FILE *fp){
	s64 res=0;
	u8 *dataCluster = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
	if((res = ReadCluster(cluster, dataCluster, fp)) != 0){
		return res;
	}
	if(nr_level == DIRECT){
		directory_entry *currententry = (directory_entry*)xcalloc(1, sizeof(directory_entry));
		u32 currentByte=0;
		while(*ndir_entry > 0 && currentByte < BOOT_SECTOR->cluster_size - 8 - 1){
			memcpy(currententry, dataCluster + currentByte, 8 + 1 + dataCluster[currentByte + 8]);
			if(currententry->name_len <= 0 || currententry->entry_number == record_number){
				break;
			}
			memset(currententry, 0, sizeof(directory_entry));
			currentByte += 8 + 1 + dataCluster[currentByte+8];
			(*ndir_entry)--;
		}
		
		if(currententry->entry_number == record_number){
			while(currentByte < BOOT_SECTOR->cluster_size - 8 - 1 - currententry->name_len){
				u64 nextentry = currentByte + 8 + 1 + currententry->name_len;
				memset(dataCluster + currentByte, 0, 8 + 1 + currententry->name_len);
				memcpy(dataCluster + currentByte, dataCluster + nextentry, 8 + 1 + dataCluster[nextentry + 8]);
				if(dataCluster[currentByte + 8] <= 0)
					break;
				currentByte += 8 + 1 + dataCluster[currentByte + 8];
			}
		}
		else
			res = 1;
	}
	else{
		u64 *data = (u64*)dataCluster;
		u32 i;
		for(i=0; i<BOOT_SECTOR->cluster_size/8; i+=2){
			if(data[i] == 0)
				break;
			u32 j;
			for(j=0; j<data[i+1]; j++){
				if((res = RemoveDirEntryInternal(data[i] + j, nr_level-1, ndir_entry, record_number, fp)) <= 0){
					free(dataCluster);
					return res;
				}
			}
		}
	}
	s32 r=0;
	if((r = WriteCluster(cluster, dataCluster, fp)) != 0){
		free(dataCluster);
		return r;
	}
	return res;
}
s32 RemoveDirEntry(mft_file_record *dir, u64 record_number, FILE *fp){
	u32 ndir_entry = dir->ndir_entry;
	s64 res=0;
	if((res = RemoveDirEntryInternal(dir->dataCluster, dir->nr_level, &ndir_entry, record_number, fp)) == 0){
		dir->ndir_entry--;
		if(dir->ndir_entry == 0){
			if((res = DeleteInternal(dir->dataCluster, dir->nr_level, fp)) != 0){
				return res;
			}
			dir->dataCluster = 0;
			dir->nr_level = DIRECT;
		}
	}
	return res;
}
s32 ModifyDirEntry(u64 cluster, NON_RESIDENT_LEVEL nr_level, u32 *ndir_entry, u64 record_number, u64 new_record_number, FILE *fp){
	s32 res=0;
	u8 *dataCluster = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
	if((res = ReadCluster(cluster, dataCluster, fp)) != 0){
		free(dataCluster);
		return res;
	}
	if(nr_level == DIRECT)
	{
		directory_entry *entry = (directory_entry*)xcalloc(1, sizeof(directory_entry));
		u32 currentByte=0;
		while(*ndir_entry > 0 && currentByte < BOOT_SECTOR->cluster_size - 8 - 1)
		{
			memcpy(entry, dataCluster + currentByte, 8 + 1 + dataCluster[currentByte+8]);
			if(entry->name_len == 0 || entry->entry_number == record_number)
				break;
			memset(entry, 0, sizeof(directory_entry));
			currentByte += 8 + 1 + dataCluster[currentByte+8];
			(*ndir_entry)--;
		}
		if(entry->entry_number == record_number)
		{
			memcpy(dataCluster + currentByte, &new_record_number,8);
		}
		else{
			// return 1 to continue the cycle and keep looking
			res = 1;
		}
		free(entry);
	}
	else{
		u64 *data = (u64*)dataCluster;
		u32 i;
		for (i=0; i<BOOT_SECTOR->cluster_size/8; i+=2){
			if(data[i] == 0)
				break;
			u32 j;
			for(j=0; j<data[i+1]; j++){
				if((res = ModifyDirEntry(data[i] + j, nr_level-1, ndir_entry, record_number, new_record_number, fp)) <= 0){
					free(dataCluster);
					return res;
				}
			}
		}
	}
	s32 r = WriteCluster(cluster, dataCluster, fp);
	if(r < 0){
		free(dataCluster);
		return r;
	}
	free(dataCluster);
	return res;
}
s32 ChangeAllChild(u64 cluster, NON_RESIDENT_LEVEL nr_level, u32 *ndir_entry, u64 record, FILE *fp){
	s32 res=0;
	u8 *dataCluster = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
	if((res = ReadCluster(cluster, dataCluster, fp)) != 0){
		free(dataCluster);
		return res;
	}
	if(nr_level == DIRECT)
	{
		directory_entry *entry = (directory_entry*)xcalloc(1, sizeof(directory_entry));
		mft_file_record *child = (mft_file_record*)xcalloc(1, MFT_RECORD_SIZE);
		u32 currentByte=0;
		while(*ndir_entry > 0 && currentByte < BOOT_SECTOR->cluster_size - 8 - 1)
		{
			if(dataCluster[currentByte + 8] <= 0)
				break;
			memcpy(entry, dataCluster + currentByte, 8 + 1 + dataCluster[currentByte+8]);
			if((res = MftReadRecord(entry->entry_number, child, fp)) != 0){
				free(entry);
				free(child);
				return res;
			}
			child->record_parent = record;
			if((res = MftWriteRecord(entry->entry_number, child, fp)) != 0){
				free(entry);
				free(child);
				return res;
			}
			memset(child, 0, MFT_RECORD_SIZE);
			memset(entry, 0, sizeof(directory_entry));
			currentByte += 8 + 1 + dataCluster[currentByte+8];
			(*ndir_entry)--;
		}
		res = (*ndir_entry == 0) ? 0 : 1;
	}
	else
	{
		u64 *data = (u64*)dataCluster;
		u32 i;
		for (i=0; i<BOOT_SECTOR->cluster_size/8; i+=2)
		{
			if(data[i] == 0)
				break;
			u32 j;
			for(j=0; j<data[i+1]; j++)
			{
				if((res = ChangeAllChild(data[i] + j, nr_level-1, ndir_entry, record, fp)) <= 0){
					free(dataCluster);
					return res;
				}
			}
		}
	}
	s32 r = WriteCluster(cluster, dataCluster, fp);
	if(r < 0){
		free(dataCluster);
		return r;
	}
	free(dataCluster);
	return res;
 }
s32 MakeNonResident(mft_file_record *record, FILE *fp){
	// download the data attribute for a cluster
    // put the pointers in the data attribute
    // to the new data clusters
    // put the residence flag in INDIRECT
    // set to 0 what remains of the unfilled data attribute
	u64 len=1;
	u64 cluster = FindEmptyClusters(&len, fp);
	if(len < 1){
		error(-EACCES, "Error, disk is full");
		return -EACCES;
	}
	u64 *data = (u64*)xcalloc(1, BOOT_SECTOR->cluster_size);
	data[0] = record->dataCluster;
	data[1] = 1;
	s32 errorCode=0;
	if((errorCode = WriteCluster(cluster, (u8*)data, fp)) != 0){
		free(data);
		return errorCode;
	}
	if((errorCode = ReserveCluster(cluster, fp)) != 0){
		free(data);
		return errorCode;
	}
	record->dataCluster = cluster;
	record->nr_level += 1;
	errorCode = MftWriteRecord(record->record_number, record, fp);
	free(data);
	return errorCode;
}
u64 FindEmptyRecord(FILE *fp){
	u64 cluster=0;
	if(BOOT_SECTOR->mft_record_count > BOOT_SECTOR->mft_zone_clusters*(BOOT_SECTOR->cluster_size/MFT_RECORD_SIZE)){
		if(BOOT_SECTOR->mft_record_count % (BOOT_SECTOR->cluster_size/MFT_RECORD_SIZE) == 0){
			u64 len = 1;
			cluster = FindEmptyClusters(&len, fp);
			if(len < 1){
				error(-ENOSPC, "Disco lleno");
				return -ENOSPC;
			}
			u8 *data = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
			if(ReadCluster(cluster, data, fp) != 0){
				free(data);
				return -1;
			}
			memset(data, 0, BOOT_SECTOR->cluster_size);
			if(WriteCluster(cluster, data, fp) != 0){
				free(data);
				return -1;
			}
			free(data);
			mft_file_record *mft = (mft_file_record*)xcalloc(1, MFT_RECORD_SIZE);
			s32 errorCode=0;
			if((errorCode = MftReadRecord(0, mft, fp)) < 0){
				return errorCode;
			}
			if((errorCode = AddDataCluster(mft, cluster, fp)) < 0){
				free(mft);
				return errorCode;
			}
			free(mft);
		}
	}
	else if(BOOT_SECTOR->mft_record_count % (BOOT_SECTOR->cluster_size/MFT_RECORD_SIZE) == 0){
		mft_file_record *mft = (mft_file_record*)xcalloc(1, MFT_RECORD_SIZE);
		s32 errorCode=0;
		if((errorCode = MftReadRecord(0, mft, fp)) < 0){
			return errorCode;
		}
		cluster = BOOT_SECTOR->mft_record_count/(BOOT_SECTOR->cluster_size/MFT_RECORD_SIZE)+1;
		if((errorCode = AddDataCluster(mft, BOOT_SECTOR->logical_mft_cluster + cluster - 1, fp)) < 0){
			free(mft);
			return errorCode;
		}
		free(mft);
	}
	return BOOT_SECTOR->mft_record_count;
}
s32 FindMftRecordOfPath(const char *path, mft_file_record *parent, mft_file_record *actual, FILE *fp){
	u32 len = strlen(path);
	char current[MAX_LENGTH_NAME];
	memset(current, 0, MAX_LENGTH_NAME);

	u32 i, j=0;
	for(i=0; i<=len; i++){
		//47 is the ascii value of "/"
		if(path[i] == 47 || path[i] == 0){
			if(strlen(current) != 0){
				directory_entry *direntry = FindDirEntry(actual, current, fp);
				if(direntry == NULL){
					error(-ENOENT, "Error leyendo la entrada de directorio de %s o no se encontro el fichero\nFuncion: FindMftRecordOfPath\n", path);
					return -ENOENT;
				}
				memcpy(parent, actual, MFT_RECORD_SIZE);
				if(MftReadRecord(direntry->entry_number, actual, fp) != 0)
					return -EACCES;
				memset(current, 0, MAX_LENGTH_NAME);
				j=0;
			}
		}
		else{
			current[j] = path[i];
			j++;
		}
	}
	return 0;
}
s32 FreeMftRecord(mft_file_record *record, FILE *fp){
	s32 res = 0;
	mft_file_record *lastRecord = (mft_file_record*)xcalloc(1, MFT_RECORD_SIZE);
	if((res = MftReadRecord(BOOT_SECTOR->mft_record_count-1, lastRecord, fp)) != 0){
		free(lastRecord);
		return res;
	}
	mft_file_record *parentLastRecord = (mft_file_record*)xcalloc(1, MFT_RECORD_SIZE);
	if((res = MftReadRecord(lastRecord->record_parent, parentLastRecord, fp)) != 0){
		free(parentLastRecord);
		free(lastRecord);
		return res;
	}
	// If I'm going to remove the last one
	if(record->record_number == lastRecord->record_number){
		if((res = RemoveDirEntry(parentLastRecord, record->record_number, fp)) != 0){
			free(parentLastRecord);
			free(lastRecord);
			return res;
		}
		if((res = MftWriteRecord(parentLastRecord->record_number, parentLastRecord, fp)) != 0){
			free(parentLastRecord);
			free(lastRecord);
			return res;
		}
		// erase it and write it, so that it is physically eliminated
		u64 lastRecordNumber = lastRecord->record_number;
		memset(lastRecord, 0, MFT_RECORD_SIZE);
		if((res = MftWriteRecord(lastRecordNumber, lastRecord, fp)) != 0){
			free(parentLastRecord);
			free(lastRecord);
			return res;
		}
		// if the cluster remained empty, I eliminate it from the mft
		if(lastRecordNumber%(BOOT_SECTOR->cluster_size*MFT_RECORD_SIZE) == 0){
			mft_file_record *mft = (mft_file_record*)xcalloc(1, MFT_RECORD_SIZE);
			if((res = MftReadRecord(0, mft, fp)) != 0){
				free(mft);
				return res;
			}
			if((res = RemoveDataCluster(mft, fp)) <= 0){
				free(mft);
				return res;
			}
			free(mft);
		}
	}
	else{
		mft_file_record *parent = (mft_file_record*)xcalloc(1, MFT_RECORD_SIZE);
		if((res = MftReadRecord(record->record_parent, parent, fp)) != 0){
			free(parent);
			free(parentLastRecord);
			free(lastRecord);
			return res;
		}
		if(parent->record_number == parentLastRecord->record_number){
			if((res = RemoveDirEntry(parentLastRecord, record->record_number, fp)) != 0){
				return res;
			}
			u32 ndir_entry = parentLastRecord->ndir_entry;
			if((res = ModifyDirEntry(parentLastRecord->dataCluster, parentLastRecord->nr_level, &ndir_entry, lastRecord->record_number, record->record_number,fp)) != 0){
				free(parentLastRecord);
				free(lastRecord);
				free(parent);
				return res;
			}
			if((res = MftWriteRecord(parentLastRecord->record_number, parentLastRecord, fp)) != 0){
				free(parentLastRecord);
				free(lastRecord);
				free(parent);
				return res;
			}
			if(lastRecord->ndir_entry > 0){
				ndir_entry = lastRecord->ndir_entry;
				if((res = ChangeAllChild(lastRecord->dataCluster, lastRecord->nr_level, &ndir_entry, record->record_number, fp)) != 0){
					return res;
				}
			}
			u64 lastRecordNumber = lastRecord->record_number;
			lastRecord->record_number = record->record_number;
			if((res = MftWriteRecord(lastRecord->record_number, lastRecord, fp)) != 0){
				free(parentLastRecord);
				free(lastRecord);
				free(parent);
				return res;
			}
			memset(lastRecord, 0, MFT_RECORD_SIZE);
			if((res = MftWriteRecord(lastRecordNumber, lastRecord, fp)) != 0){
				free(parentLastRecord);
				free(lastRecord);
				free(parent);
				return res;
			}
			// if the cluster remained empty, I eliminate it from the mft
			if(lastRecordNumber%(BOOT_SECTOR->cluster_size*MFT_RECORD_SIZE) == 0){
				mft_file_record *mft = (mft_file_record*)xcalloc(1, MFT_RECORD_SIZE);
				if((res = MftReadRecord(0, mft, fp)) != 0){
					free(mft);
					return res;
				}
				if((res = RemoveDataCluster(mft, fp)) <= 0){
					free(mft);
					return res;
				}
				free(mft);
			}
		}
		else if(lastRecord->record_number == record->record_parent){
			u32 ndir_entry = parentLastRecord->ndir_entry;
			if((res = ModifyDirEntry(parentLastRecord->dataCluster, parentLastRecord->nr_level, &ndir_entry, lastRecord->record_number, record->record_number,fp)) != 0){
				free(parentLastRecord);
				free(lastRecord);
				free(parent);
				return res;
			}
			if((res = MftWriteRecord(parentLastRecord->record_number, parentLastRecord, fp)) != 0){
				free(parentLastRecord);
				free(lastRecord);
				free(parent);
				return res;
			}
			if((res = RemoveDirEntry(lastRecord, record->record_number, fp)) != 0){
				free(parentLastRecord);
				free(lastRecord);
				free(parent);
				return res;
			}
			if(lastRecord->ndir_entry > 0){
				ndir_entry = lastRecord->ndir_entry;
				if((res = ChangeAllChild(lastRecord->dataCluster, lastRecord->nr_level, &ndir_entry, record->record_number, fp)) != 0){
					free(parentLastRecord);
					free(lastRecord);
					free(parent);
					return res;
				}
			}
			u64 lastRecordNumber = lastRecord->record_number;
			lastRecord->record_number = record->record_number;
			if((res = MftWriteRecord(lastRecord->record_number, lastRecord, fp)) != 0){
				free(parentLastRecord);
				free(lastRecord);
				free(parent);
				return res;
			}
			// erase it and write it, so that it is physically eliminated
			memset(lastRecord, 0, MFT_RECORD_SIZE);
			if((res = MftWriteRecord(lastRecordNumber, lastRecord, fp)) != 0){
				free(parentLastRecord);
				free(lastRecord);
				free(parent);
				return res;
			}
			// if the cluster remained empty, I eliminate it from the mft
			if(lastRecordNumber%(BOOT_SECTOR->cluster_size*MFT_RECORD_SIZE) == 0){
				mft_file_record *mft = (mft_file_record*)xcalloc(1, MFT_RECORD_SIZE);
				if((res = MftReadRecord(0, mft, fp)) != 0){
					free(mft);
					return res;
				}
				if((res = RemoveDataCluster(mft, fp)) <= 0){
					free(mft);
					return res;
				}
				free(mft);
			}
		}
		else{
			if((res = RemoveDirEntry(parent, record->record_number, fp)) != 0){
				free(parentLastRecord);
				free(lastRecord);
				free(parent);
				return res;
			}
			if((res = MftWriteRecord(parent->record_number, parent, fp)) != 0){
				free(parentLastRecord);
				free(lastRecord);
				free(parent);
				return res;
			}
			u32 ndir_entry = parentLastRecord->ndir_entry;
			if((res = ModifyDirEntry(parentLastRecord->dataCluster, parentLastRecord->nr_level, &ndir_entry, lastRecord->record_number, record->record_number,fp)) != 0){
				free(parentLastRecord);
				free(lastRecord);
				free(parent);
				return res;
			}
			if((res = MftWriteRecord(parentLastRecord->record_number, parentLastRecord, fp)) != 0){
				free(parentLastRecord);
				free(lastRecord);
				free(parent);
				return res;
			}
			if(lastRecord->ndir_entry > 0){
				ndir_entry = lastRecord->ndir_entry;
				if((res = ChangeAllChild(lastRecord->dataCluster, lastRecord->nr_level, &ndir_entry, record->record_number, fp)) != 0){
					free(parentLastRecord);
					free(lastRecord);
					free(parent);
					return res;
				}
			}
			u64 lastRecordNumber = lastRecord->record_number;
			lastRecord->record_number = record->record_number;
			if((res = MftWriteRecord(lastRecord->record_number, lastRecord, fp)) != 0){
				free(parentLastRecord);
				free(lastRecord);
				free(parent);
				return res;
			}
			// erase it and write it, so that it is physically eliminated
			memset(lastRecord, 0, MFT_RECORD_SIZE);
			if((res = MftWriteRecord(lastRecordNumber, lastRecord, fp)) != 0){
				free(parentLastRecord);
				free(lastRecord);
				free(parent);
				return res;
			}
			// if the cluster remained empty, I eliminate it from the mft
			if(lastRecordNumber%(BOOT_SECTOR->cluster_size*MFT_RECORD_SIZE) == 0){
				mft_file_record *mft = (mft_file_record*)xcalloc(1, MFT_RECORD_SIZE);
				if((res = MftReadRecord(0, mft, fp)) != 0){
					free(mft);
					return res;
				}
				if((res = RemoveDataCluster(mft, fp)) <= 0){
					free(mft);
					return res;
				}
				free(mft);
			}
		}
		free(parent);
	}
	BOOT_SECTOR->mft_record_count--;
	res = WriteCluster(0, (u8*)BOOT_SECTOR, fp);
	free(lastRecord);
	free(parentLastRecord);
	return res;
}
s32 ReserveMftRecord(u64 record, FILE *fp){
	s32 res=0;
	u64 request = record/(BOOT_SECTOR->cluster_size/MFT_RECORD_SIZE)+1;
	u64 clusterToReserve = FindCluster(0, request, fp);
	if((res = ReserveCluster(clusterToReserve, fp)) != 0)
		return res;
	BOOT_SECTOR->mft_record_count++;
	res = WriteCluster(0, (u8*)BOOT_SECTOR, fp);
	return res;
}
u64 FindClusterInternal(u64 dataCluster, NON_RESIDENT_LEVEL nr_level, u64 requestCluster, u64 *currentCluster, FILE *fp){
	u32 res=0;
	u64 *data = (u64*)xcalloc(1, BOOT_SECTOR->cluster_size);
	if((res = ReadCluster(dataCluster, (u8*)data, fp)) != 0){
		free(data);
		return res;
	}
	if(nr_level == DIRECT){
		if(requestCluster == 1){
			free(data);
			return dataCluster;
		}
	}
	else if(nr_level == INDIRECT){
		u32 i;
		for (i = 0; i < BOOT_SECTOR->cluster_size/8; i+=2) {
			if(data[i] == 0)
				break;
			if(*currentCluster + data[i+1] > requestCluster){
				res = data[i] + (requestCluster - *currentCluster);
				free(data);
				return res;
			}
			*currentCluster += data[i+1];
		}
		res = 0;
	}
	else{
		u32 i;
		for (i = 0; i < BOOT_SECTOR->cluster_size/8; i+=2) {
			u32 j;
			for (j = 0; j < data[i+1]; j++) {
				res = FindClusterInternal(data[i-2] + data[i-1] - 1, nr_level-1, requestCluster, currentCluster, fp);
				if(res > 0 || res < 0){
					free(data);
					return res;
				}
			}
		}
	}
	free(data);
	return res;
}
u64 FindCluster(u64 record, u64 requestCluster, FILE *fp){
	s32 errorCode=0;
	mft_file_record *rec = (mft_file_record*)xcalloc(1, MFT_RECORD_SIZE);
	if(record == 0)
	{
		u8 *record_data = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
		if((errorCode = ReadCluster(BOOT_SECTOR->logical_mft_cluster, record_data, fp)) < 0){
			return errorCode;
		}
		memcpy(rec, record_data, MFT_RECORD_SIZE);
		free(record_data);
	}
	else
	{
		if((errorCode = MftReadRecord(record, rec, fp)) != 0)
		{
			free(rec);
			return -1;
		}
	}
	u64 current=1;
	errorCode = FindClusterInternal(rec->dataCluster, rec->nr_level, requestCluster, &current, fp);
	free(rec);
	return errorCode;
}


