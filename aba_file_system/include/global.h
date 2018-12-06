
#ifndef ABA_GLOBAL_H_
#define ABA_GLOBAL_H_

#include "types.h"
#include "declarations.h"

/*
 * Variables globales
 */
char *disk_filename;							//path del fichero disco
char *log_filename;								//path del fichero log
mft_file_record *cache[MAX_CACHE_SIZE];			//cache de entradas de mft

#endif /* ABA_GLOBAL_H_ */
