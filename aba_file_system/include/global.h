
#ifndef ABA_GLOBAL_H_
#define ABA_GLOBAL_H_

#include "types.h"
#include "declarations.h"

/*
 * Global variables
 */
char *disk_filename;							// file path of the disk file
char *log_filename;								// file path of the log file
mft_file_record *cache[MAX_CACHE_SIZE];			// cache of entries of the MFT

#endif /* ABA_GLOBAL_H_ */
