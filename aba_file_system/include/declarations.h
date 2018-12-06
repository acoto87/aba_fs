
#ifndef ABA_DECLARATIONS_H_
#define ABA_DECLARATIONS_H_

#include "types.h"

#define MFT_RECORD_SIZE 			sizeof(mft_file_record)
#define MAX_LENGTH_NAME 			255
#define MAX_DIR_ENTRY 				BOOT_SECTOR->cluster_size/(8 + 1 + 1)
#define MAX_CACHE_SIZE 				20

#define UID_SUPERUSER 		0			//privilgios de superusuario
#define UID_AMINISTRATOR	1			//privilegios de administracion
#define UID_USER			2			//privilegios de usuario

#define NON_RESIDENT_LEVEL u8
#define DIRECT 0
#define INDIRECT 1
#define DOUBLE_INDIRECT 2
#define TRIPLE_INDIRECT 3

typedef struct{
	u64 entry_number;					//entrada en la que se encuentra el fichero
	u8 name_len;						//longitud del nombre
	char filename[MAX_LENGTH_NAME];		//nombre del fichero;
}directory_entry;

typedef struct{
	u32 mode;						//tipo de fichero, modos de proteccion
	u32 nlinks;						//numero de enlaces
	u16 uid;						//id del propietario del fichero
	u16 gid;						//grupo del propietario del fichero
	u64 size;						//tamano en bytes del fichero
	u32 atime;						//fecha/hora del ultimo acceso
	u32 mtime;						//fecha/hora de la ultima modificacion
	u32 ctime;						//fecha/hora de la creacion del fichero
	u64 record_number;				//numero del record en la mft
	u64 record_parent;				//numero del record del directorio padre en la mft
	u32 ndir_entry;					//cantidad de entradas de directorio, si no es un directorio su valor es 0
	NON_RESIDENT_LEVEL nr_level;	//nivel de no-residencia del atributo 'data'
	u64 dataCluster;				//numero del cluster que contiene los datos del fichero
}mft_file_record;

struct aba_boot_sector{
	u64 disk_size;					//tamano de la particion en bytes
	u32 cluster_size;				//tamano del cluster en bytes
	u64 number_of_clusters;			//cantidad de clusters en la particion (partition_size/cluster_size)
	u32 logical_mft_cluster;		//entrada de la mft en la que se encuentra el directorio raiz
	u32 mft_zone_clusters;			//cantidad de clusters reservados originalmente para la mft
	u64 mft_record_count;			//cantidad de records de la mft
}*BOOT_SECTOR;

#endif /* ABA_DECLARATIONS_H_ */
