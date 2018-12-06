
#ifndef ABA_MFT_H_
#define ABA_MFT_H_

#include "types.h"
#include "declarations.h"

/*
 * Read from disk the sequence of bytes of a cluster
 * cluster: cluster to read
 * data: 	buffer to store the bytes
 */
extern s32 ReadCluster(u64 cluster, u8 *data, FILE *fp);
/*
 * Write a sequence of bytes ot a cluster
 * cluster: cluster to be written
 * data: 	buffer with the bytes to write
 */
extern s32 WriteCluster(u64 cluster, u8 *data, FILE *fp);
/*
 * Read a MFT record
 * record_number: 	the record number
 * record: 			struct to store the record data
 */
extern s32 MftReadRecord(u64 record_number, mft_file_record *record, FILE *fp);
/*
 * Write a MFT record
 * Ensure that the cluster is written and the remaining bytes are 0
 * record_number: 	record to be written
 * record: 			struct with the data to store
 */
extern s32 MftWriteRecord(u64 record_number, mft_file_record *record, FILE *fp);
/*
 * Add an entry to the directory
 * dir: 	directory to add the entry
 * entry: 	the entry
 */
extern s32 AddDirEntry(mft_file_record *dir, directory_entry *entry, FILE *fp);
/*
 * Remove an entry to the directory
 * dir: 	directory to remove the entry
 * entry: 	the entry
 */
extern s32 RemoveDirEntry(mft_file_record *dir, u64 record_number, FILE *fp);
/*
 * Find the entry corresponding to the name of the specified file
 * Return NULL if the entry is not found
 * dir:			directory in which to search for the entry
 * filename: 	name of the file to search
 * fp:			pointer to the disk file. If NULL is passed and the pointer is initialized
 *              inside the function, the corresponding memory of the pointer will not be released
 */
extern directory_entry* FindDirEntry(mft_file_record* dir, const char *filename, FILE *fp);
/*
 * Makes the attr data of the record non-resident
 */
extern s32 MakeNonResident(mft_file_record *record, FILE *fp);
/*
 * Search in the MFT an empty entry. Return the entry number.
 */
extern u64 FindEmptyRecord(FILE *fp);
/*
 * Find the corresponding record given a path and leave it in current
 * also leaves the parent directory of the file in the paremeter parent
 */
extern s32 FindMftRecordOfPath(const char *path, mft_file_record *parent, mft_file_record *actual, FILE *fp);
/*
 * Copy the contents of the src record to the dest
 */
extern s32 CopyMftRecord(mft_file_record *dest, mft_file_record *src);
/*
 * Release a record of the MFT
 */
extern s32 FreeMftRecord(mft_file_record *record, FILE *fp);
/*
 * Reserve a record of the MFT
 */
extern s32 ReserveMftRecord(u64 record, FILE *fp);
extern u64 FindCluster(u64 record, u64 requestCluster, FILE *fp);
extern s32 ChangeAllChild(u64 dataCluster, NON_RESIDENT_LEVEL nr_level, u32 *ndir_entry, u64 record, FILE *fp);

#endif /* ABA_MFT_H_ */
