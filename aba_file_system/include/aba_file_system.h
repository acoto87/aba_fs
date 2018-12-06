
#ifndef ABAFILESYSTEM_H_
#define ABAFILESYSTEM_H_

s64 AddDataCluster(mft_file_record *record,u64 block,FILE *fp);
s64 RemoveDataCluster(mft_file_record *record,FILE *fp);
s32 DeleteInternal(u64 cluster, NON_RESIDENT_LEVEL nr_level, FILE *fp);

#endif /* ABAFILESYSTEM_H_ */
