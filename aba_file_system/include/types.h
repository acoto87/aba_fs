
#ifndef ABA_TYPES_H_
#define ABA_TYPES_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fuse.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

/*
 * basic unsigned types
 */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

/*
 * basic signed types
 */
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

/*
 * type BOOL
 */
typedef enum {
#ifndef FALSE
		FALSE = 0,
#endif
#ifndef NO
		NO = 0,
#endif
#ifndef ZERO
		ZERO = 0,
#endif
#ifndef TRUE
		TRUE = 1,
#endif
#ifndef YES
		YES = 1,
#endif
#ifndef ONE
		ONE = 1,
#endif
} BOOL;

#endif /* ABA_TYPES_H_ */
