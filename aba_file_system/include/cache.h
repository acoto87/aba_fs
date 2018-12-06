
#ifndef ABA_CACHE_H_
#define ABA_CACHE_H_

#include "types.h"
#include "declarations.h"


void InitializeCache();
mft_file_record* RetrieveFromCache(u32 zone_number, u16 record_number);
void InsertInCache(mft_file_record *record);
void shift_cache(u32 index);

#endif /* ABA_CACHE_H_ */
