
#ifndef ABA_ERROR_H_
#define ABA_ERROR_H_

#define FAILED -1

#include "types.h"
#include "global.h"

/*
 * This function handles and log the file system errors.
 * This function should be called any time an error occurs.
 */
static __inline__ void error(u32 e_code, const char *error, ...){
	FILE *log;
	if((log = fopen(log_filename, "a")) != NULL){
		fprintf(log, "Error code : %i, %s\n", e_code, error);
		fflush(log);
		fclose(log);
	}
};

#endif /* ABA_ERROR_H_ */
