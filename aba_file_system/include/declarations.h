
#ifndef ABA_DECLARATIONS_H_
#define ABA_DECLARATIONS_H_

#include "types.h"

#define MFT_RECORD_SIZE 			sizeof(mft_file_record)
#define MAX_LENGTH_NAME 			255
#define MAX_DIR_ENTRY 				BOOT_SECTOR->cluster_size/(8 + 1 + 1)
#define MAX_CACHE_SIZE 				20

#define UID_SUPERUSER 		0			// super user privileges
#define UID_AMINISTRATOR	1			// admin privileges
#define UID_USER			2			// user privileges

#define NON_RESIDENT_LEVEL u8
#define DIRECT 0
#define INDIRECT 1
#define DOUBLE_INDIRECT 2
#define TRIPLE_INDIRECT 3

typedef struct{
	u64 entry_number;					// entry of the file
	u8 name_len;						// length of the name of the file
	char filename[MAX_LENGTH_NAME];		// name of the file
}directory_entry;

typedef struct{
	u32 mode;						    // file type, protection modes
	u32 nlinks;						    // number of links
	u16 uid;						    // id of the owner of the file
	u16 gid;						    // group of the owner of the file
	u64 size;						    // length in bytes of the file
	u32 atime;						    // date/time of the last access
	u32 mtime;						    // date/time of the last modification
	u32 ctime;						    // date/time of the creation
	u64 record_number;				    // record number in the MFT
	u64 record_parent;				    // record number in the MFT of the parent directory
	u32 ndir_entry;					    // number of entries in the directory, if isn't a directory this value is 0
	NON_RESIDENT_LEVEL nr_level;	    // non-resident level of the data attr
	u64 dataCluster;				    // cluster number that contains the contents of the file
}mft_file_record;

struct aba_boot_sector{
	u64 disk_size;					    // length of the partition in bytes
	u32 cluster_size;				    // length of the cluster in bytes
	u64 number_of_clusters;			    // number of clusters in the partition (partition_size/cluster_size)
	u32 logical_mft_cluster;		    // entry in the MFT of the root directory
	u32 mft_zone_clusters;			    // number of clusters initially reserved for the MFT
	u64 mft_record_count;			    // number of records of the MFT
}*BOOT_SECTOR;

#endif /* ABA_DECLARATIONS_H_ */
