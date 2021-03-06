
#ifndef ABA_FORMATER_H_
#define ABA_FORMATER_H_

#include "types.h"
#include "declarations.h"

/*
 * Format the hdd
 * disk_filename: file that represent the hdd
 * disk_size: size of the hdd
 * cluster_size: cluster size
 */
int FormatDisk(char* disk_filename, const u64 disk_size, const u32 cluster_size);
void FillBootSector(const int disk_size, const int cluster_size, struct aba_boot_sector *bs);
mft_file_record* InitializeMft();
mft_file_record* InitializeRootDir();
mft_file_record* InitializeBitmap();
FILE* CreateDisk(const char* disk_filename, const u64 disk_size, const u32 cluster_size);

s32 ReadCluster(u64 cluster, u8 *data, FILE *fp);
s32 WriteCluster(u64 cluster, u8 *data, FILE *fp);
s32 MftReadRecord(u64 record_number, mft_file_record *record, FILE *fp);
s32 MftWriteRecord(u64 record_number, mft_file_record *record, FILE *fp);

mft_file_record* InitializeFile();

#endif /* ABA_FORMATER_H_ */
