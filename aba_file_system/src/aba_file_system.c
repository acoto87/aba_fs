
#define FUSE_USE_VERSION 25
#define _FILE_OFFSET_BITS 64

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "global.h"
#include "mft.h"
#include "mem_alloc.h"
#include "error.h"
#include "declarations.h"
#include "space_manager.h"

s64 AddDataClusterInternal(u64 cluster,u64 block,NON_RESIDENT_LEVEL nr_level,FILE *fp)
{
	s64 res=0;
	u8 *data_block=(u8*)xcalloc(1,BOOT_SECTOR->cluster_size);
	if((res = ReadCluster(cluster, data_block, fp)) != 0){
		free(data_block);
		return res;
	}
	if(nr_level == INDIRECT){
		u64 *data =(u64*)data_block;
		u32 i;
		for(i=0; i<BOOT_SECTOR->cluster_size/8; i+=2){
			if(data[i]==0)
			{
				if(i > 0 && (data[i-2] + data[i-1]) == block)
				{
					data[i-1]+=1;
				}
				else
				{
					data[i]=block;
					data[i+1]=1;
				}
				res = WriteCluster(cluster, data_block, fp);
				free(data_block);
				return res;
			}
		}
		u64 len=1;
		u64 newcluster=FindEmptyClusters(&len,fp);
		if(len<1){
			error(-EACCES, "Error, disco lleno");
			res=-EACCES;
		}
		else{
			u8 *newdata = (u8*)xcalloc(1,BOOT_SECTOR->cluster_size);
			if((res = ReadCluster(newcluster, newdata, fp)) != 0){
				free(newdata);
				free(data_block);
				return res;
			}
			memset(newdata,0,BOOT_SECTOR->cluster_size);
			u64 *newindirect=(u64*)newdata;
			newindirect[0]=block;
			newindirect[1]=1;
			if((res = WriteCluster(newcluster,newdata,fp)) != 0){
				free(newdata);
				free(data_block);
				return res;
			}
			if((res = ReserveCluster(newcluster, fp)) != 0){
				free(newdata);
				free(data_block);
				return res;
			}
			res=newcluster;
			free(newdata);
		}
	}
	else{
		u64 *data =(u64*)data_block;
		s32 i;
		for(i=(BOOT_SECTOR->cluster_size/8)-2; i>=0; i-=2)
		{
			if(data[i]!=0)
			{
				if((res=AddDataClusterInternal(data[i]+data[i+1]-1, block, nr_level-1, fp)) <= 0){
					free(data_block);
					return res;
				}
				break;
			}
		}
		if(data[i]+data[i+1] == res)
		{
			data[i+1]+=1;
			res=WriteCluster(cluster,data_block,fp);
			free(data_block);
			return res;
		}
		else if(i != (BOOT_SECTOR->cluster_size/8)-2)
		{
			i+=2;
			data[i]=res;
			data[i+1]=1;
			res=WriteCluster(cluster,data_block,fp);
			free(data_block);
			return res;
		}
		u64 len=1;
		u64 newcluster=FindEmptyClusters(&len,fp);
		if(len<1){
			error(-ENOSPC, "Error, disco lleno");
			res=-ENOSPC;
			free(data_block);
			return res;
		}
		s64 newClusterPos = res;
		u8 *newdata = (u8*)xcalloc(1,BOOT_SECTOR->cluster_size);
		if((res = ReadCluster(newcluster, newdata, fp)) != 0){
			free(newdata);
			free(data_block);
			return res;
		}
		memset(newdata,0,BOOT_SECTOR->cluster_size);
		u64 *newindirect=(u64*)newdata;
		newindirect[0]=newClusterPos;
		newindirect[1]=1;
		if((res=WriteCluster(newcluster,newdata,fp))!=0){
			free(newdata);
			free(data_block);
			return res;
		}
		if((res = ReserveCluster(newcluster, fp)) != 0){
			free(newdata);
			free(data_block);
			return res;
		}
		res=newcluster;
		free(newdata);
	}
	free(data_block);
	return res;
}
s64 AddDataCluster(mft_file_record *record,u64 block,FILE *fp)
{
	s64 res=0;
	if(record->nr_level==DIRECT){
		if(record->dataCluster==0){
			record->dataCluster=block;
			record->size += BOOT_SECTOR->cluster_size;
			if((res = MftWriteRecord(record->record_number,record,fp)) != 0){
				return res;
			}
		}
		else{
			if ((res = MakeNonResident(record,fp)) != 0){
				return res;
			}
			res = AddDataCluster(record,block,fp);
		}
		return res;
	}
	if ((res = AddDataClusterInternal(record->dataCluster, block, record->nr_level, fp)) == 0){
		record->size+=BOOT_SECTOR->cluster_size;
		if((res = MftWriteRecord(record->record_number, record, fp)) != 0){
			return res;
		}
	}
	else if(res > 0)
	{
		s64 newcluster = res;
		if ((res = MakeNonResident(record,fp)) != 0){
			return res;
		}
		u8 *data_block=(u8*)xcalloc(1,BOOT_SECTOR->cluster_size);
		if((res = ReadCluster(record->dataCluster, data_block, fp)) != 0){
			free(data_block);
			return res;
		}
		u64 *data=(u64*)data_block;
		if((data[0]+data[1]) == newcluster){
			data[1]+=1;
		}
		else{
			data[2]=newcluster;
			data[3]=1;
		}
		if((res=WriteCluster(record->dataCluster,data_block,fp))!=0){
			free(data_block);
			return res;
		}
		record->size+=BOOT_SECTOR->cluster_size;
		if((res=MftWriteRecord(record->record_number,record,fp))!=0){
			free(data_block);
			return res;
		}
		free(data_block);
	}
	return res;
}
s64 RemoveDataClusterInternal(u64 cluster,NON_RESIDENT_LEVEL nr_level,u64 *clustererased,FILE *fp)
{
	s64 res=0;
	u8 *data_block=(u8*)xcalloc(1,BOOT_SECTOR->cluster_size);
	if((res = ReadCluster(cluster,data_block,fp)) != 0){
		free(data_block);
		return res;
	}
	if(nr_level==INDIRECT){
		u64 *data =(u64*)data_block;
		s32 i;
		for(i=(BOOT_SECTOR->cluster_size/8)-2; i>=0; i-=2){
			if(data[i]!=0){
				u64 free_block=data[i]+data[i+1]-1;
				(*clustererased)=free_block;
				if((res = FreeCluster(free_block,fp)) != 0){
					free(data_block);
					return res;
				}
				data[i+1]-=1;
				if(data[i+1]==0){
					data[i]=0;
					if(i==0){
						res=1;
					}
				}
				s32 r;
				if((r = WriteCluster(cluster, data_block, fp)) != 0){
					free(data_block);
					return r;
				}
				free(data_block);
				return res;
			}
		}
		res=1;
	}
	else
	{
		u64 *data =(u64*)data_block;
		s32 i;
		for(i = (BOOT_SECTOR->cluster_size/8)-2; i>=0; i-=2){
			if(data[i] != 0){
				if((res = RemoveDataClusterInternal(data[i]+data[i+1]-1, nr_level-1, clustererased, fp)) <= 0){
					free(data_block);
					return res;
				}
				if(res==1){
					u64 free_block = data[i] + data[i+1] - 1;
					if((res = FreeCluster(free_block,fp)) != 0){
						free(data_block);
						return res;
					}
					data[i+1]-=1;
					if(data[i+1] == 0){
						data[i]=0;
						if(i==0){
							res=1;
						}
					}
					s32 r;
					if((r = WriteCluster(cluster, data_block, fp)) != 0){
						free(data_block);
						return r;
					}
				}
			}
		}
	}
	free(data_block);
	return res;
}
s64 RemoveDataCluster(mft_file_record *record,FILE *fp)
{
	s64 res=0;
	u64 clustererased=0;
	res=RemoveDataClusterInternal(record->dataCluster,record->nr_level,&clustererased,fp);
	if(res == 0){
		u8 *data_block = (u8*)xcalloc(1,BOOT_SECTOR->cluster_size);
		if((res = ReadCluster(record->dataCluster, data_block, fp)) != 0){
			free(data_block);
			return res;
		}
		u64 *data=(u64*)data_block;
		if (data[2] == 0 && data[1] == 1){
			if((res=FreeCluster(record->dataCluster,fp)) != 0){
				free(data_block);
				return res;
			}
			record->dataCluster=data[0];
			record->nr_level-=1;
		}
		record->size-=BOOT_SECTOR->cluster_size;
		if ((res = MftWriteRecord(record->record_number, record, fp)) != 0){
			free(data_block);
			return res;
		}
		free(data_block);
		return (s64)clustererased;
	}
	else if(res == 1){
		if((res=FreeCluster(record->dataCluster,fp)) != 0){
			return res;
		}
		record->dataCluster=0;
		record->nr_level = 0;
		record->size-=BOOT_SECTOR->cluster_size;
		if ((res = MftWriteRecord(record->record_number, record, fp)) != 0){
			return res;
		}
		return (s64)clustererased;
	}
	return res;
}
s32 SetZerosInFinalCluster(u64 cluster, NON_RESIDENT_LEVEL nr_level, u32 offset, FILE *fp)
{
	s32 res =0;
	u8 *data_block=(u8*)xcalloc(1,BOOT_SECTOR->cluster_size);
	if((res = ReadCluster(cluster, data_block, fp)) != 0){
		free(data_block);
		return res;
	}
	if(nr_level == DIRECT){
		memset(data_block + offset, 0, BOOT_SECTOR->cluster_size-offset);
		if ((res = WriteCluster(cluster, data_block, fp)) != 0){
			free(data_block);
			return res;
		}
	}
	else{
		u64 *data = (u64*)data_block;
		s32 i;
		for (i=(BOOT_SECTOR->cluster_size/8)-2; i>=0; i-=2){
			if(data[i]!=0){
				if ((res = SetZerosInFinalCluster(data[i]+data[i+1]-1, nr_level-1, offset, fp)) != 0){
					free(data_block);
					return res;
				}
				break;
			}
		}
	}
	free(data_block);
	return res;
}
s32 WriteInternal(u64 cluster, NON_RESIDENT_LEVEL nr_level, const char *buf, u32 *offsetbuf, off_t offset, u32 bytes_towrite, u64 cluster_init, u64 cluster_end, u64 *cluster_current, u32 offsetsize, FILE *fp)
{
	s32 res=0;
	u8 *data_block=(u8*)xcalloc(1,BOOT_SECTOR->cluster_size);
	if((res = ReadCluster(cluster, data_block, fp)) != 0){
		free(data_block);
		return res;
	}
	if(nr_level == DIRECT){
		memcpy(data_block + offset, buf + *offsetbuf, bytes_towrite);
		if((res = WriteCluster(cluster, data_block, fp)) != 0){
			free(data_block);
			return res;
		}
	}
	else if(nr_level == INDIRECT){
		u64 *data=(u64*)data_block;
		u32 i;
	    for(i=0; i<BOOT_SECTOR->cluster_size/8; i+=2){
			if(data[i]==0)
				break;
			u32 j;
			for(j=0; j<data[i+1]; j++){
				u32 write_init = 0;
				u32 write_size = BOOT_SECTOR->cluster_size - write_init;
				if(*cluster_current == cluster_end){
					if(cluster_init == cluster_end)
						write_init = offset % BOOT_SECTOR->cluster_size;
					write_size -= offsetsize % BOOT_SECTOR->cluster_size-write_init;
					if((res = WriteInternal(data[i]+j, nr_level-1, buf, offsetbuf, write_init, write_size, cluster_init, cluster_end, cluster_current, offsetsize, fp)) < 0){
						free(data_block);
						return res;
					}
					(*offsetbuf) += write_size;
					free(data_block);
					return res;
				}
				else if(*cluster_current == cluster_init){
					write_init = offset % BOOT_SECTOR->cluster_size;
					write_size -= write_init;
					if((res = WriteInternal(data[i]+j, nr_level-1, buf, offsetbuf, write_init, write_size, cluster_init, cluster_end, cluster_current, offsetsize, fp)) < 0){
						free(data_block);
						return res;
					}
					(*offsetbuf) += write_size;
				}
				else if(*cluster_current > cluster_init && *cluster_current < cluster_end){
					if((res = WriteInternal(data[i]+j, nr_level-1, buf, offsetbuf, write_init, write_size, cluster_init, cluster_end, cluster_current, offsetsize, fp)) < 0){
						free(data_block);
						return res;
					}
					(*offsetbuf) += BOOT_SECTOR->cluster_size;
				}
				(*cluster_current)++;
			}
		}
		res=1;
	}
	else{
		u64 *data =(u64*)data_block;
		u32 i;
		for(i=0; i<BOOT_SECTOR->cluster_size/8; i+=2){
			if(data[i]==0)
				break;
			u32 j;
			for(j=0; j<data[i+1]; j++){
				if((res = WriteInternal(data[i]+j, nr_level-1, buf, offsetbuf, offset, bytes_towrite, cluster_init, cluster_end, cluster_current, offsetsize, fp)) <= 0){
					free(data_block);
					return res;
				}
			}
		}
		res=1;
	}
	free(data_block);
	return res;
}
s32 ReadInternal(u64 cluster, NON_RESIDENT_LEVEL nr_level, char *buf, u32 *offsetbuf, off_t offset, u32 bytes_toread, u64 cluster_init, u64 cluster_end, u64 *cluster_current, u32 offsetsize,FILE *fp)
{
	s32 res=0;
	u8 *data_block = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
	if((res = ReadCluster(cluster, data_block, fp)) != 0){
		free(data_block);
		return res;
	}
	if(nr_level==DIRECT){
		memcpy(buf + *offsetbuf, data_block + offset, bytes_toread);
	}
	else if(nr_level==INDIRECT){
		u64 *data = (u64*)data_block;
		u32 i;
	    for(i=0; i<BOOT_SECTOR->cluster_size/8; i+=2){
			if(data[i] == 0)
				break;
			u32 j;
			for(j=0; j<data[i+1]; j++)
			{
				u32 read_init = 0;
				u32 read_size = BOOT_SECTOR->cluster_size;
				if(*cluster_current == cluster_end){
					if(cluster_init == cluster_end)
						read_init = offset % BOOT_SECTOR->cluster_size;
					read_size -= offsetsize % BOOT_SECTOR->cluster_size - read_init;
					if((res = ReadInternal(data[i]+j, nr_level-1, buf, offsetbuf, read_init, read_size, cluster_init, cluster_end, cluster_current, offsetsize, fp)) < 0){
						free(data_block);
						return res;
					}
					(*offsetbuf) += read_size;
					free(data_block);
					return res;
				}
				else if(*cluster_current == cluster_init){
					read_init = offset % BOOT_SECTOR->cluster_size;
					read_size -= read_init;
					if((res = ReadInternal(data[i]+j, nr_level-1, buf, offsetbuf, read_init, read_size, cluster_init, cluster_end, cluster_current, offsetsize, fp)) < 0){
						free(data_block);
						return res;
					}
					(*offsetbuf) += read_size;
				}
				else if(*cluster_current > cluster_init && *cluster_current < cluster_end){
					if((res = ReadInternal(data[i]+j, nr_level-1, buf, offsetbuf, read_init, read_size, cluster_init, cluster_end, cluster_current, offsetsize, fp)) < 0){
						free(data_block);
						return res;
					}
					(*offsetbuf) += BOOT_SECTOR->cluster_size;
				}
				(*cluster_current)++;
			}
		}
		res=1;
	}
	else
	{
		u64 *data =(u64*)data_block;
		u32 i;
		for(i=0; i<BOOT_SECTOR->cluster_size/8; i+=2){
			if(data[i] == 0)
				break;
			u32 j;
			for(j=0; j<data[i+1]; j++){
				 if((res = ReadInternal(data[i]+j, nr_level-1, buf, offsetbuf, offset, bytes_toread, cluster_init, cluster_end, cluster_current, offsetsize, fp)) <= 0){
					 free(data_block);
					 return res;
				 }
			}
		}
		res=1;
	}
	free(data_block);
	return res;
}
s32 ReadDirInternal(u64 cluster, NON_RESIDENT_LEVEL nr_level, void *buf, fuse_fill_dir_t filler, FILE *fp)
{
	s32 res=0;
	u8 *data_block= (u8*)xcalloc(1,BOOT_SECTOR->cluster_size);
	if((res = ReadCluster(cluster, data_block, fp)) != 0){
		free(data_block);
		return res;
	}
	if(nr_level == DIRECT){
	  u32 cluster_position=0;
	  while(cluster_position < BOOT_SECTOR->cluster_size){
		  cluster_position += 8;
		  u8 name_size = data_block[cluster_position];
		  if(name_size<=0)
			  break;
		  char *filename = (char*)xcalloc(1,name_size+1);
		  cluster_position++;
		  memcpy(filename, &data_block[cluster_position], name_size);
		  cluster_position += name_size;
		  filler(buf, filename, NULL, 0);
		  free(filename);
	  }
	}
	else{
		u64 *data = (u64*)data_block;
		u32 i;
		for(i=0; i<BOOT_SECTOR->cluster_size/8; i+=2){
			if(data[i]==0)
				break;
			u32 j;
			for(j=0; j<data[i+1]; j++){
				if((res = ReadDirInternal(data[i]+j, nr_level-1, buf, filler, fp)) != 0){
					free(data_block);
					return res;
				}
			}
		}
	}
	free(data_block);
	return res;
}
s32 DeleteInternal(u64 cluster, NON_RESIDENT_LEVEL nr_level, FILE *fp)
{
	s32 res=0;
	if(nr_level == DIRECT){
	 	if((res = FreeCluster(cluster, fp)) != 0){
	 		return res;
		}
	}
	else{
		u8 *data_block = (u8*)xcalloc(1, BOOT_SECTOR->cluster_size);
	  	if((res = ReadCluster(cluster, data_block, fp)) != 0){
			free(data_block);
	  		return res;
	  	}
		u64 *data = (u64*)data_block;
		s32 i;
    	for(i=(BOOT_SECTOR->cluster_size/8)-2; i>=0; i-=2){
			if(data[i]!=0){
				u32 j;
				for(j=0; j<data[i+1]; j++){
					if((res = DeleteInternal(data[i]+j, nr_level-1, fp)) != 0){
						free(data_block);
						return res;
					}
				}
			}
    	}
		free(data_block);
		if((res = FreeCluster(cluster, fp)) != 0)
			return res;
	}
	return res;
}

static int abafs_getattr(const char *path, struct stat *stbuf)
{
	FILE *fp;
	if((fp = fopen(disk_filename, "r")) == NULL){
		error(-EACCES, "Error tratando de abrir el fichero : %s\n", disk_filename);
		return -EACCES;
	}
   	int res = 0;
   	memset(stbuf, 0, sizeof(struct stat));

	mft_file_record *actual = (mft_file_record*)xmalloc(sizeof(mft_file_record));
   	if((res = MftReadRecord(1, actual, fp)) != 0){
    	error(res, "Error tratando de leer el directorio raiz : %s\n", disk_filename);
    	free(actual);
    	fclose(fp);
    	return res;
    }
   	mft_file_record *parent = (mft_file_record*)xmalloc(sizeof(mft_file_record));
   	if((res = FindMftRecordOfPath(path, parent, actual, fp)) != 0){
   		error(res, "El fichero o alguna parte de la ruta no existe");
		free(parent);
		free(actual);
	    fclose(fp);
	    return res;
   	}
   	stbuf->st_mode = actual->mode;
   	stbuf->st_nlink = actual->nlinks;
   	stbuf->st_uid = actual->uid;
   	stbuf->st_gid = actual->gid;
   	stbuf->st_size = actual->size;
//   	stbuf->st_atim = 0;
//   	stbuf->st_mtim = 0;
//   	stbuf->st_ctim = 0;


   	//EL PROBLEMA ERA QUE SOLO SE LE HACE free A UN PUNTERO CREADO CON malloc O calloc
   	//O ALGO POR EL ESTILO, A UN PUNTERO INICIALIZADO CON ASIGNACION DIRECTA NO SE LE
   	//PUEDE HACER free
   	free(parent);
	free(actual);
    fclose(fp);
    return res;
}
static int abafs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	FILE *fp;
	if((fp = fopen(disk_filename, "r")) == NULL){
		error(-EACCES, "Error tratando de abrir el fichero : %s\n", disk_filename);
		return -EACCES;
	}

    int res = 0;

	mft_file_record *actual = (mft_file_record*)xmalloc(sizeof(mft_file_record));
    if(MftReadRecord(1, actual, fp) != 0){
   		error(-EACCES, "Error tratando de leer el directorio raiz : %s\n", disk_filename);
   		free(actual);
   		fclose(fp);
   		return -EACCES;
   	}
	mft_file_record *parent = (mft_file_record*)xmalloc(sizeof(mft_file_record));

   	if(FindMftRecordOfPath(path, parent, actual, fp) != 0){
   		return -ENOENT;
	}
   	if(!S_ISDIR(actual->mode)){
   		error(-ENOTDIR, "El fichero no es un directorio\n");
   		free(actual);
   		free(parent);
   		fclose(fp);
   		return -ENOTDIR;
   	}

	(void) offset;
  	(void) fi;

  	filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

	if(actual->dataCluster > 0 && (res = ReadDirInternal(actual->dataCluster, actual->nr_level, buf, filler, fp)) != 0){
		return res;
	}

	free(parent);
	free(actual);
    fclose(fp);
    return res;
}
static int abafs_open(const char *path, struct fuse_file_info *fi)
{
	FILE *fp;
	if((fp = fopen(disk_filename, "r")) == NULL){
		error(-EACCES, "Error tratando de abrir el fichero : %s\n", disk_filename);
		return -EACCES;
	}
   	int res = 0;
   	mft_file_record *actual = (mft_file_record*)xmalloc(sizeof(mft_file_record));
   	if(MftReadRecord(1, actual, fp) != 0){
   		error(-ENOENT, "Error tratando de leer el directorio raiz : %s\n", disk_filename);
   		free(actual);
   		fclose(fp);
   	   	return -ENOENT;
   	}
	mft_file_record *parent = (mft_file_record*)xmalloc(sizeof(mft_file_record));
	if(FindMftRecordOfPath(path, parent, actual, fp) != 0){
		free(actual);
		free(parent);
		fclose(fp);
		return -ENOENT;
	}
	if(strcmp(path, "/") == 0 || strcmp(path, "/$mft") == 0){
		free(actual);
		free(parent);
		fclose(fp);
		return -EACCES;
	}
	//res = (actual->mode | fi->flags) == 0 ? -EACCES : 0;
	free(parent);
	free(actual);
    fclose(fp);
    return res;
}
static int abafs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	FILE *fp;
	if((fp = fopen(disk_filename, "r")) == NULL){
		error(-EACCES, "Error tratando de abrir el fichero : %s\n", disk_filename);
		return -EACCES;
	}
   	int res = 0;
	mft_file_record *actual = (mft_file_record*)xmalloc(sizeof(mft_file_record));
	if((res = MftReadRecord(1, actual, fp)) != 0){
    	error(res, "Error tratando de leer el directorio raiz : %s\n", disk_filename);
    	free(actual);
		fclose(fp);
    	return res;
    }
   	mft_file_record *parent = (mft_file_record*)xmalloc(sizeof(mft_file_record));
   	if((res = FindMftRecordOfPath(path, parent, actual, fp)) != 0){
   		error(res, "El fichero o alguna parte de la ruta no existe");
		free(parent);
		free(actual);
	    fclose(fp);
	    return res;
   	}

	if(actual->dataCluster == 0 || actual->size == 0){
		free(parent);
		free(actual);
		fclose(fp);
		return 0;
	}
	u32 offsetsize = offset + size;
	if(offsetsize == 0){
		error(-EINVAL,"El offset y el size dados no son validos para el fichero");
		free(parent);
		free(actual);
		fclose(fp);
		return-EINVAL ;
	}
	if(offsetsize > actual->size){
		offsetsize = actual->size;
	}
	u64 cluster_init = offset/BOOT_SECTOR->cluster_size+1;
	u64 cluster_end = offsetsize/BOOT_SECTOR->cluster_size;
	if(offsetsize % BOOT_SECTOR->cluster_size > 0)
		cluster_end++;
	u64 cluster_current = 1;
	u32 offsetbuf = 0;
	res = ReadInternal(actual->dataCluster, actual->nr_level, buf, &offsetbuf, offset, offsetsize-offset, cluster_init, cluster_end, &cluster_current, offsetsize, fp);
	free(parent);
	free(actual);
	fclose(fp);
	return (res < 0) ? res : offsetsize-offset;
}
static int abafs_mkdir(const char *path, mode_t mode)
{
	FILE *fp;
	if((fp = fopen(disk_filename, "rw+")) == NULL){
		error(-EACCES, "Error tratando de abrir el fichero : %s\n", disk_filename);
		return -EACCES;
	}
   	int res = 0;
	mft_file_record *actual = (mft_file_record*)xmalloc(sizeof(mft_file_record));
	if((res = MftReadRecord(1, actual, fp)) != 0){
	   	error(res, "Error tratando de leer el directorio raiz : %s\n", disk_filename);
	   	free(actual);
	   	fclose(fp);
	   	return res;
	}
	mft_file_record *parent = (mft_file_record*)xmalloc(sizeof(mft_file_record));
	if(FindMftRecordOfPath(path, parent, actual, fp) == 0){
		error(-EEXIST, "Error leyendo la entrada de directorio de %s o no se encontro el fichero\n", path+1);
		free(actual);
		free(parent);
		fclose(fp);
    	return -EEXIST;
	}
  	u32 length=strlen(path);
	u8 newlength=0;
	char *newrecord_name = (char*)xcalloc(1, MAX_LENGTH_NAME+1);

	//47 es el codigo ascii del '/'
	while(length>0 && path[length-1] != 47){
		length--;
		newlength++;
	}
	strcpy(newrecord_name, path + length);

	mft_file_record *newrecord = (mft_file_record*)xcalloc(1, sizeof(mft_file_record));
   	u64 record = FindEmptyRecord(fp); //encontrar un record vacio

	newrecord->mode = mode | S_IFDIR;
	newrecord->nlinks = 1;
	newrecord->record_number = record;
	newrecord->ndir_entry = 0;
	newrecord->nr_level = DIRECT;
	newrecord->size = 0;
	newrecord->record_parent = actual->record_number;
	newrecord->dataCluster = 0;

	if((res = MftWriteRecord(record, newrecord, fp)) != 0){
		free(actual);
		free(parent);
		free(newrecord);
		free(newrecord_name);
		return res;
	}
	if((res = ReserveMftRecord(record, fp)) != 0){
		free(actual);
		free(parent);
		free(newrecord);
		free(newrecord_name);
		return res;
	}

	directory_entry *entry = xcalloc(1, sizeof(directory_entry));
	entry->entry_number=record;
	entry->name_len=newlength;
	strcpy(entry->filename, newrecord_name);

	if((res = AddDirEntry(actual, entry, fp)) != 0){
		free(entry);
		free(newrecord_name);
		free(newrecord);
		free(parent);
		free(actual);
		fclose(fp);
		return res;
	}
	if((res = MftWriteRecord(actual->record_number, actual, fp)) != 0){
		free(entry);
		free(newrecord_name);
		free(newrecord);
		free(parent);
		free(actual);
		fclose(fp);
		return res;
	}
	free(entry);
	free(newrecord_name);
	free(newrecord);
	free(parent);
	free(actual);
    fclose(fp);
    return res;
}
static int abafs_mknod(const char *path, mode_t mode, dev_t dev)
{
	FILE *fp;
	if((fp = fopen(disk_filename, "rw+")) == NULL){
		error(-EACCES, "Error tratando de abrir el fichero : %s\n", disk_filename);
		return -EACCES;
	}
	int res = 0;
	mft_file_record *actual = (mft_file_record*)xmalloc(sizeof(mft_file_record));
	if((res = MftReadRecord(1, actual, fp)) != 0){
	   	error(res, "Error tratando de leer el directorio raiz : %s\n", disk_filename);
	   	free(actual);
	   	fclose(fp);
	   	return res;
	}
	mft_file_record *parent = (mft_file_record*)xmalloc(sizeof(mft_file_record));
	if(FindMftRecordOfPath(path, parent, actual, fp) == 0){
		error(-EEXIST, "Error leyendo la entrada de directorio de %s o no se encontro el fichero\n", path+1);
		free(actual);
		free(parent);
		fclose(fp);
    	return -EEXIST;
	}
	u32 length=strlen(path);
	u8 newlength=0;
	char *newrecord_name = (char*)xcalloc(1, MAX_LENGTH_NAME);

	while(length>0 && path[length-1] != 47){
		length--;
		newlength++;
	}
	strcpy(newrecord_name,path+length);

	mft_file_record *newrecord = (mft_file_record*)xmalloc(sizeof(mft_file_record));
   	u64 record = FindEmptyRecord(fp);

	newrecord->mode = mode | S_IFREG;
	newrecord->record_number = record;
	newrecord->ndir_entry = 0;
	newrecord->size = 0;
	newrecord->nr_level = DIRECT;
	newrecord->nlinks = 1;
	newrecord->record_parent = actual->record_number;
	newrecord->dataCluster = 0;

	if((res = MftWriteRecord(record, newrecord, fp)) != 0){
		free(actual);
		free(parent);
		free(newrecord);
		free(newrecord_name);
		fclose(fp);
		return res;
	}
	if((res = ReserveMftRecord(record, fp)) != 0){
		return res;
	}

	directory_entry *entry = xcalloc(1, sizeof(directory_entry));
	entry->entry_number=record;
	entry->name_len=newlength;
	strcpy(entry->filename, newrecord_name);

	if((res = AddDirEntry(actual, entry, fp)) != 0){
		free(entry);
		free(newrecord_name);
		free(newrecord);
		free(parent);
		free(actual);
		fclose(fp);
		return res;
	}
	if((res = MftWriteRecord(actual->record_number, actual, fp)) != 0){
		free(entry);
		free(newrecord_name);
		free(newrecord);
		free(parent);
		free(actual);
		fclose(fp);
		return res;
	}

	free(entry);
	free(newrecord_name);
	free(newrecord);
	free(parent);
	free(actual);
    fclose(fp);
    return res;
}
static int abafs_rename(const char *path, const char *newpath)
{
	FILE *fp;
	if((fp = fopen(disk_filename, "rw+")) == NULL){
		error(-EACCES, "Error tratando de abrir el fichero : %s\n", disk_filename);
		return -EACCES;
	}
	int res = 0;
	mft_file_record *actual = (mft_file_record*)xmalloc(sizeof(mft_file_record));
	if(MftReadRecord(1, actual, fp) != 0){
	   	error(-EIO, "Error tratando de leer el directorio raiz : %s\n", disk_filename);
	   	free(actual);
	   	fclose(fp);
	   	return -EIO;
	}

	mft_file_record *parent = (mft_file_record*)xmalloc(sizeof(mft_file_record));
   	if((res = FindMftRecordOfPath(path,parent,actual,fp)) != 0){
		error(res, "Error leyendo la entrada de directorio de %s o no se encontro el fichero\n", path+1);
		free(actual);
		free(parent);
		fclose(fp);
   		return res;
	}
	//obtengo el nuevo nombre del path nuevo
	u32 newpathlength = strlen(newpath);
	u8 newlength = 0;
	char *newname = (char*)xcalloc(1, MAX_LENGTH_NAME);
	while(newpathlength>0 && path[newpathlength-1] != 47){
		newpathlength--;
		newlength++;
	}
	strcpy(newname, newpath+newpathlength);
	if(FindDirEntry(parent, newname, fp) != NULL){
		error(-EEXIST,"Existe ya una entrada en el directorio de %s con ese nombre\n", path+1);
		free(actual);
		free(parent);
		free(newname);
		fclose(fp);
		return -EEXIST;
	}
	//elimino la entrada de directorio actual
	if((res = RemoveDirEntry(parent, actual->record_number, fp)) != 0){
		free(newname);
		free(parent);
		free(actual);
	    fclose(fp);
		return res;
	}
	directory_entry *newEntry = (directory_entry*)xcalloc(1, sizeof(directory_entry));
	newEntry->name_len = newlength;
	newEntry->entry_number = actual->record_number;
	strcpy(newEntry->filename, newname);
	//y la anado de nuevo al directorio padre
	if((res = AddDirEntry(parent, newEntry, fp)) != 0){
		free(newname);
		free(newEntry);
		free(parent);
		free(actual);
	    fclose(fp);
		return res;
	}
	res = MftWriteRecord(parent->record_number, parent, fp);
	free(newname);
	free(newEntry);
	free(parent);
	free(actual);
    fclose(fp);
    return res;
}
static int abafs_rmdir(const char *path)
{
	FILE *fp;
	if((fp = fopen(disk_filename, "rw+")) == NULL){
		error(-EACCES, "Error tratando de abrir el fichero : %s\n", disk_filename);
		return -EACCES;
	}
	int res = 0;
	mft_file_record *actual = (mft_file_record*)xmalloc(sizeof(mft_file_record));
	if((res = MftReadRecord(1, actual, fp)) != 0){
	   	error(res, "Error tratando de leer el directorio raiz : %s\n", disk_filename);
	   	free(actual);
	   	fclose(fp);
	   	return res;
	}
	mft_file_record *parent = (mft_file_record*)xmalloc(sizeof(mft_file_record));
   	if((res = FindMftRecordOfPath(path, parent, actual, fp)) != 0){
		error(res, "Error leyendo la entrada de directorio de %s o no se encontro el fichero\n", path+1);
		free(actual);
		free(parent);
		fclose(fp);
    	return res;
	}
   	if(actual->ndir_entry > 0){
   		free(actual);
   		free(parent);
   		fclose(fp);
   		return -ENOTEMPTY;
   	}
 	if((res = FreeMftRecord(actual, fp)) != 0){
		free(parent);
		free(actual);
		fclose(fp);
		return res;
	}
	free(parent);
	free(actual);
    fclose(fp);
    return res;
}
static int abafs_unlink(const char *path)
{
	FILE *fp;
	if((fp = fopen(disk_filename, "rw+")) == NULL){
		error(-EACCES, "Error tratando de abrir el fichero : %s\n", disk_filename);
		return -EACCES;
	}
	int res = 0;
	mft_file_record *actual = (mft_file_record*)xmalloc(sizeof(mft_file_record));
	if((res = MftReadRecord(1, actual, fp)) != 0){
	   	error(res, "Error tratando de leer el directorio raiz : %s\n", disk_filename);
	   	free(actual);
	   	fclose(fp);
	   	return res;
	}
	mft_file_record *parent = (mft_file_record*)xmalloc(sizeof(mft_file_record));
   	if((res = FindMftRecordOfPath(path, parent, actual, fp)) != 0){
		error(res, "Error leyendo la entrada de directorio de %s o no se encontro el fichero\n", path+1);
		free(actual);
		free(parent);
		fclose(fp);
    	return res;
	}
	if(S_ISREG(actual->mode) == FALSE){
		error(-EACCES, "Error abriendo %s  no es un fichero\n", path);
		free(actual);
		free(parent);
		fclose(fp);
		return -EACCES;
	}
	if(actual->dataCluster > 0 && (res = DeleteInternal(actual->dataCluster, actual->nr_level, fp)) != 0){
		free(parent);
		free(actual);
		fclose(fp);
		return res;
	}
	actual->dataCluster = 0;
	if((res = FreeMftRecord(actual, fp)) != 0){
		free(parent);
		free(actual);
		fclose(fp);
		return res;
	}
	free(parent);
	free(actual);
    fclose(fp);
    return res;
}
static int abafs_truncate(const char * path, off_t offset)
{
	FILE *fp;
	if((fp = fopen(disk_filename, "rw+")) == NULL){
		error(-EACCES, "Error tratando de abrir el fichero : %s\n", disk_filename);
		return -EACCES;
	}

   	mft_file_record *actual = (mft_file_record*)xmalloc(sizeof(mft_file_record));
   	if(MftReadRecord(1, actual, fp) != 0){
   		error(-ENOENT, "Error tratando de leer el directorio raiz : %s\n", disk_filename);
   		free(actual);
   		fclose(fp);
   	   	return -ENOENT;
   	}
	mft_file_record *parent = (mft_file_record*)xmalloc(sizeof(mft_file_record));
	if(FindMftRecordOfPath(path, parent, actual, fp) != 0){
		free(actual);
		free(parent);
		fclose(fp);
		return -ENOENT;
	}
	if(S_ISREG(actual->mode)==0)
	{
		free(actual);
		free(parent);
		fclose(fp);
		error(-EACCES,"Error abriendo %s  no es un fichero\n", path+1);
		return -EACCES;
	}
	s32 res=0;
	u64 newsize=offset;
	if(actual->size<newsize)
	{
		u32 offsetsize=0;
		if ((offsetsize=actual->size%BOOT_SECTOR->cluster_size)>0)
		{
			if((res=SetZerosInFinalCluster(actual->dataCluster,actual->nr_level,offsetsize,fp))!=0)
			{
				error(res,"Error tratando de acceder a la region de datos de: %s\n",path+1);
				free(parent);
				free(actual);
			    fclose(fp);
				return res;
			}
			actual->size+=BOOT_SECTOR->cluster_size-offsetsize;
			if ((res=MftWriteRecord(actual->record_number,actual,fp))!=0)
			{
				error(res,"Error tratando de acceder a la region de datos de: %s\n",path+1);
				free(parent);
				free(actual);
			    fclose(fp);
				return res;
			}

		}
		u64 extrasize=(offset-actual->size);
		u32 count=extrasize/BOOT_SECTOR->cluster_size;
		if ((offset-actual->size)%BOOT_SECTOR->cluster_size>0)
		{
			count++;
		}

		if(count>0)
		{
			u64 request=count;
			do
			{
				u64 first=FindEmptyClusters(&request,fp);
				if(request<1)
				{
					free(parent);
					free(actual);
					fclose(fp);
					error(-EACCES, "Error, disco lleno");
					return -EACCES;
				}
				u64 i;
				for(i=0 ;i<request;i++)
				{
					u8 *data=(u8*)xcalloc(1,BOOT_SECTOR->cluster_size);
					memset(data,0,BOOT_SECTOR->cluster_size);

					if((res=WriteCluster(first+i,data,fp))!=0)
					{
						error(res,"Error tratando de acceder a la region de datos de: %s\n",path+1);
						free(data);
						free(parent);
						free(actual);
					    fclose(fp);
						return res;
					}
					if((res = ReserveCluster(first+i, fp)) != 0){
						free(data);
						free(parent);
						free(actual);
						fclose(fp);
						return res;
					}
					if((res=AddDataCluster(actual,first+i,fp))!=0)
					{
						error(res,"Error tratando de acceder a la region de datos de: %s\n",path+1);
						free(data);
						free(parent);
						free(actual);
					    fclose(fp);
						return res;
					}
					if(extrasize<BOOT_SECTOR->cluster_size)
					{
						actual->size-=BOOT_SECTOR->cluster_size;
						actual->size+=extrasize;
						if ((res=MftWriteRecord(actual->record_number,actual,fp))!=0)
						{
							error(res,"Error tratando de acceder a la region de datos de: %s\n",path+1);
							free(data);
							free(parent);
							free(actual);
						    fclose(fp);
							return res;
						}
						break;
					}
					extrasize-=BOOT_SECTOR->cluster_size;
					free(data);
				}

				count=count-request;
				request=count;
			}while(request>0);
		}
	}
	else
	{

		u32 offsetsize=actual->size%BOOT_SECTOR->cluster_size;
		u32 increment=0;
		if(offsetsize>0)
		{
			increment=BOOT_SECTOR->cluster_size-offsetsize;
		}
		u32 countoff=offset/BOOT_SECTOR->cluster_size;
		if((offset%BOOT_SECTOR->cluster_size)>0)
		{
			countoff++;
		}
		u32 count=((actual->size+increment)/BOOT_SECTOR->cluster_size)-countoff;
		if (count>0)
		{
			if(increment>0)
			{
				if ((res=RemoveDataCluster(actual,fp))<0)
				{
					free(parent);
					free(actual);
					fclose(fp);
				}
				actual->size+=increment;
				if ((res=MftWriteRecord(actual->record_number,actual,fp))!=0)
				{
					error(res,"Error tratando de acceder a la region de datos de: %s\n",path+1);
					free(parent);
					free(actual);
				    fclose(fp);
					return res;
				}
				count--;
			}
			while (count>0)
			{
				if ((res=RemoveDataCluster(actual,fp))<0)
				{
					free(parent);
					free(actual);
					fclose(fp);
					return res;
				}
				count--;
			}

		}
		if ((offsetsize=offset%BOOT_SECTOR->cluster_size)>0)
		{
			if((res=SetZerosInFinalCluster(actual->dataCluster,actual->nr_level,offsetsize,fp))!=0)
			{
				error(res,"Error tratando de acceder a la region de datos de: %s\n",path+1);
				free(parent);
				free(actual);
			    fclose(fp);
				return res;
			}
			actual->size=offset;
			if ((res=MftWriteRecord(actual->record_number,actual,fp))!=0)
			{
				error(res,"Error tratando de acceder a la region de datos de: %s\n",path+1);
				free(parent);
				free(actual);
			    fclose(fp);
				return res;
			}
		}


	}
	free(parent);
	free(actual);
    fclose(fp);
    return res;
}
static int abafs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	FILE *fp;
	if((fp = fopen(disk_filename, "rw+")) == NULL){
		error(-EACCES, "Error tratando de abrir el fichero : %s\n", disk_filename);
		return -EACCES;
	}
   	mft_file_record *actual = (mft_file_record*)xmalloc(sizeof(mft_file_record));

   	if(MftReadRecord(1, actual, fp) != 0)
   	{
	 	error(-EIO, "Error tratando de leer el directorio raiz : %s\n", disk_filename);
   		free(actual);
   	   	fclose(fp);
		return -EIO;
   	}

	mft_file_record *parent = (mft_file_record*)xmalloc(sizeof(mft_file_record));

	if(FindMftRecordOfPath(path, parent, actual, fp) != 0)
	{
		error(-ENOENT, "Error leyendo la entrada de directorio de %s o no se encontro el fichero\n", path+1);
		free(parent);
		free(actual);
   	   	fclose(fp);
		return -ENOENT;
	}
	u32 offsetsize=offset+size;
	if(offsetsize==0)
 	{
		error(-EINVAL,"El offset y el size dados no son validos para el fichero");
		free(parent);
		free(actual);
   	   	fclose(fp);
		return-EINVAL ;
	}
	s32 res=0;
 	u32 extrasize=0;
 	u32 offsetbuf=0;
	if(offsetsize>actual->size)
	{
		extrasize=offsetsize-actual->size;
		offsetsize=actual->size;
		u32 tmp=actual->size%BOOT_SECTOR->cluster_size;
		if(tmp>0){
			extrasize-=BOOT_SECTOR->cluster_size;
			offsetsize+=BOOT_SECTOR->cluster_size;
		}
	}
	if (actual->dataCluster>0 && offset < actual->size)
	{
		u64 cluster_init=offset/BOOT_SECTOR->cluster_size + 1;
		u64 cluster_end=offsetsize/BOOT_SECTOR->cluster_size;
		if((offsetsize%BOOT_SECTOR->cluster_size>0))
		{
			cluster_end++;
		}
		u64 cluster_current=1;
		if((res=WriteInternal(actual->dataCluster,actual->nr_level,buf,&offsetbuf,offset,offsetsize-offset,cluster_init,cluster_end,&cluster_current,offsetsize,fp))!=0)
		{
			free(parent);
			free(actual);
	   	   	fclose(fp);
			return res;
		}
	}
	u32 count=(extrasize)/BOOT_SECTOR->cluster_size;
	if ((extrasize)%BOOT_SECTOR->cluster_size>0)
	{
		count++;
	}

	if(count>0)
	{
		u64 request=count;
		do
		{
			u64 first=FindEmptyClusters(&request,fp);
			if(request<1)
			{
				free(parent);
				free(actual);
				fclose(fp);
				error(-ENOSPC, "Error, disco lleno");
				return -ENOSPC;
			}
			u64 i;
			for(i=0 ;i<request;i++)
			{
				u8 *data=(u8*)xcalloc(1,BOOT_SECTOR->cluster_size);
				if(extrasize>BOOT_SECTOR->cluster_size)
				{
			    	memcpy(data,buf+offsetbuf,BOOT_SECTOR->cluster_size);
				}
				else
				{
			 	  memcpy(data,buf+offsetbuf,extrasize);
				}
				if((res = ReserveCluster(first+i, fp)) != 0){
					free(data);
					free(parent);
					free(actual);
					fclose(fp);
					return res;
				}
				if((res=WriteCluster(first+i,data,fp))!=0)
				{
					error(res,"Error tratando de acceder a la region de datos de: %s\n",path+1);
					free(data);
					free(parent);
					free(actual);
				    fclose(fp);
					return res;
				}
				if((res=AddDataCluster(actual,first+i,fp))!=0)
				{
					error(res,"Error tratando de acceder a la region de datos de: %s\n",path+1);
					free(data);
					free(parent);
					free(actual);
				    fclose(fp);
					return res;
				}
				if(extrasize<BOOT_SECTOR->cluster_size)
				{
					actual->size-=BOOT_SECTOR->cluster_size;
					actual->size+=extrasize;
					if ((res=MftWriteRecord(actual->record_number,actual,fp))!=0)
					{
						error(res,"Error tratando de acceder a la region de datos de: %s\n",path+1);
						free(data);
						free(parent);
						free(actual);
					    fclose(fp);
						return res;
					}
					break;
				}
				extrasize-=BOOT_SECTOR->cluster_size;
				offsetbuf+= BOOT_SECTOR->cluster_size;
				free(data);
			}

			count=count-request;
			request=count;
		}while(request>0);
	}

	free(parent);
	free(actual);
	fclose(fp);
	return size;
}
static int abafs_chmod(const char *path, mode_t mode)
{
	FILE *fp;
	if((fp = fopen(disk_filename, "rw+")) == NULL){
		error(-EACCES, "Error tratando de abrir el fichero : %s\n", disk_filename);
		return -EACCES;
	}
   	int res = 0;
   	mft_file_record *actual = (mft_file_record*)xmalloc(sizeof(mft_file_record));
   	if(MftReadRecord(1, actual, fp) != 0){
   		error(-ENOENT, "Error tratando de leer el directorio raiz : %s\n", disk_filename);
   		free(actual);
   		fclose(fp);
   	   	return -ENOENT;
   	}
	mft_file_record *parent = (mft_file_record*)xmalloc(sizeof(mft_file_record));
	if(FindMftRecordOfPath(path, parent, actual, fp) != 0)
	{
		error(-ENOENT,"Una parte del path %s no es correcto o no se encuentra\n",path+1);
		free(actual);
		free(parent);
		fclose(fp);
		return -ENOENT;
	}
	actual->mode = actual->mode|mode;
	if((res = MftWriteRecord(actual->record_number, actual, fp)) != 0)
	{
		free(parent);
		free(actual);
		fclose(fp);
		return res;
	}
	free(parent);
	free(actual);
    fclose(fp);
    return res;
}
static int abafs_chown(const char *path, uid_t uid, gid_t gid)
{
	FILE *fp;
	if((fp = fopen(disk_filename, "rw+")) == NULL){
		error(-EACCES, "Error tratando de abrir el fichero : %s\n", disk_filename);
		return -EACCES;
	}
   	int res = 0;
   	mft_file_record *actual = (mft_file_record*)xmalloc(sizeof(mft_file_record));
   	if(MftReadRecord(1, actual, fp) != 0){
   		error(-ENOENT, "Error tratando de leer el directorio raiz : %s\n", disk_filename);
   		free(actual);
   		fclose(fp);
   	   	return -ENOENT;
   	}
	mft_file_record *parent = (mft_file_record*)xmalloc(sizeof(mft_file_record));
	if(FindMftRecordOfPath(path, parent, actual, fp) != 0)
	{
		error(-ENOENT,"Una parte del path %s no es correcto o no se encuentra\n",path+1);
		free(actual);
		free(parent);
		fclose(fp);
		return -ENOENT;
	}

	actual->uid = uid;
	actual->gid = gid;

	if((res = MftWriteRecord(actual->record_number, actual, fp)) != 0)
	{
		free(parent);
		free(actual);
		fclose(fp);
		return res;
	}
	free(parent);
	free(actual);
    fclose(fp);
    return res;
}
static int abafs_release(const char* path, struct fuse_file_info *fi){
	FILE *fp;
	if((fp = fopen(disk_filename, "r")) == NULL){
		error(-EACCES, "Error tratando de abrir el fichero : %s\n", disk_filename);
		return -EACCES;
	}
   	int res = 0;
   	mft_file_record *actual = (mft_file_record*)xmalloc(sizeof(mft_file_record));
   	if(MftReadRecord(1, actual, fp) != 0){
   		error(-ENOENT, "Error tratando de leer el directorio raiz : %s\n", disk_filename);
   		free(actual);
   		fclose(fp);
   	   	return -ENOENT;
   	}
	mft_file_record *parent = (mft_file_record*)xmalloc(sizeof(mft_file_record));
	if(FindMftRecordOfPath(path, parent, actual, fp) != 0){
		free(actual);
		free(parent);
		fclose(fp);
		return -ENOENT;
	}
	if(strcmp(path, "/") == 0 || strcmp(path, "/$mft") == 0){
		free(actual);
		free(parent);
		fclose(fp);
		return -EACCES;
	}
	//res = (actual->mode | fi->flags) == 0 ? -EACCES : 0;
	free(parent);
	free(actual);
    fclose(fp);
    return res;
}
static int abafs_statfs(const char *path, struct statvfs *st_fs)
{
	//The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
	st_fs->f_bsize=BOOT_SECTOR->cluster_size;
	st_fs->f_blocks=BOOT_SECTOR->number_of_clusters;
	u64 free=0;
	u32 i;
	for (i = 0; i < 10; i++){
		free+=BuddyList[i]->count*powi(2,i);
	}
	st_fs->f_bavail=free;
	st_fs->f_namemax=MAX_LENGTH_NAME;
	return 0;
}
static int abafs_symlink (const char *oldname, const char *newname)
{
	FILE *fp;
	if((fp = fopen(disk_filename, "rw+")) == NULL){
		error(-EACCES, "Error tratando de abrir el fichero : %s\n", disk_filename);
		return -EACCES;
	}
	int res = 0;
	mft_file_record *actual = (mft_file_record*)xmalloc(sizeof(mft_file_record));
	if((res = MftReadRecord(1, actual, fp)) != 0){
	   	error(res, "Error tratando de leer el directorio raiz : %s\n", disk_filename);
	   	free(actual);
	   	fclose(fp);
	   	return res;
	}
	mft_file_record *parent = (mft_file_record*)xmalloc(sizeof(mft_file_record));
	if(FindMftRecordOfPath(newname, parent, actual, fp) == 0){
		error(-EEXIST, "Error leyendo la entrada de directorio de %s o no se encontro el fichero\n", newname+1);
		free(actual);
		free(parent);
		fclose(fp);
    	return -EEXIST;
	}
	u32 length=strlen(newname);
	u8 newlength=0;
	char *newrecord_name = (char*)xcalloc(1, MAX_LENGTH_NAME);

	while(length>0 && newname[length-1] != 47){
		length--;
		newlength++;
	}
	strcpy(newrecord_name,newname+length);

	mft_file_record *newrecord = (mft_file_record*)xmalloc(sizeof(mft_file_record));
   	u64 record = FindEmptyRecord(fp);

	newrecord->mode = S_IFLNK |0775|0777;
	newrecord->record_number = record;
	newrecord->ndir_entry = 0;
	newrecord->size = 0;
	newrecord->nr_level = DIRECT;
	newrecord->nlinks = 1;
	newrecord->record_parent = actual->record_number;
	newrecord->dataCluster = 0;

	if((res = MftWriteRecord(record, newrecord, fp)) != 0){
		free(actual);
		free(parent);
		free(newrecord);
		free(newrecord_name);
		fclose(fp);
		return res;
	}
	if((res = ReserveMftRecord(record, fp)) != 0){
		return res;
	}

	directory_entry *entry = xcalloc(1, sizeof(directory_entry));
	entry->entry_number=record;
	entry->name_len=newlength;
	strcpy(entry->filename, newrecord_name);

	if((res = AddDirEntry(actual, entry, fp)) != 0){
		free(entry);
		free(newrecord_name);
		free(newrecord);
		free(parent);
		free(actual);
		fclose(fp);
		return res;
	}
	if((res = MftWriteRecord(actual->record_number, actual, fp)) != 0){
		free(entry);
		free(newrecord_name);
		free(newrecord);
		free(parent);
		free(actual);
		fclose(fp);
		return res;
	}

	free(entry);
	free(newrecord_name);
	free(newrecord);

	char *buf = xcalloc(1,strlen(oldname)+1);
	memcpy(buf,oldname,strlen(oldname));
	fclose(fp);
	if((res=abafs_write(newname,buf,strlen(oldname),0,0))<0)
	{

		abafs_unlink(newname);
		free(buf);
		free(parent);
		free(actual);
		return res;
	}

	free(buf);
	free(parent);
	free(actual);
	res=0;
    return res;
}
static int abafs_readlink(const char *filename, char *buffer, size_t size)
{
	int res=0;
	res=abafs_read(filename,buffer,size,0,0);
	return res;
}

static struct fuse_operations abafs_oper= {
    .getattr    = abafs_getattr,
    .readdir    = abafs_readdir,
    .open	    = abafs_open,
    .read	    = abafs_read,
    .mkdir      = abafs_mkdir,
    .mknod     	= abafs_mknod,
    .rename     = abafs_rename,
    .rmdir      = abafs_rmdir,
    .truncate   = abafs_truncate,
    .unlink     = abafs_unlink,
    .write      = abafs_write,
    .chmod		= abafs_chmod,
    .chown		= abafs_chown,
    .release	= abafs_release,
    .statfs		= abafs_statfs,
    .symlink = abafs_symlink,
    .readlink = abafs_readlink,
};

int main(int argc, char *argv[])
{
	if(argc <= 2)
		return -1;
	if(argc == 4){
		disk_filename = argv[2];
	}
	else if(argc == 5){
		disk_filename = argv[3];
	}
	FILE *fp;
	if((fp = fopen(disk_filename, "r")) == NULL){
		error(-EACCES, "Error de acceso al disco\n");
		return -1;
	}
	u8 *boot = (u8*)xmalloc(atoi(argv[3]));
	if(fread(boot, atoi(argv[3]), 1, fp) < 1){
		error(-EIO, "Error de lectura de disco\n");
		return -EIO;
	}
	BOOT_SECTOR = (struct aba_boot_sector*)boot;
	LoadBuddySystem(fp);

	FILE *tmpfp;
	if((tmpfp = fopen("/home/alejandro/Documentos/buddy&bitmap", "w")) != NULL){
		PrintBuddySystem(tmpfp);
		fprintf(tmpfp, "\n");
		PrintBitmap(tmpfp, fp);
		fclose(tmpfp);
	}
	fclose(fp);

	FILE *log;
	if((log = fopen(log_filename, "w")) != NULL){
		fprintf(log, "Iniciando el sistema de ficheros...\n\n");
		fflush(log);
		fclose(log);
	}

	return fuse_main(argc-2, argv, &abafs_oper);
}
