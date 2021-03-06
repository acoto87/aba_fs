
#ifndef ABA_BITMAP_H_
#define ABA_BITMAP_H_

#include "types.h"

/*
 * set the specified bit with the new_value argument
 */
static void __inline__ bit_set(u8 *bitmap, const u64 bit, const u8 new_value){
	if (!bitmap || new_value > 1)
		return;
	if (!new_value)
		bitmap[bit >> 3] &= ~(1 << (bit & 7));
	else
		bitmap[bit >> 3] |= (1 << (bit & 7));
}
/*
 * return the value of the specified bit (0 or 1)
 */
static u8 __inline__ bit_get(const u8 *bitmap, const u64 bit){
	if (!bitmap)
		return -1;
	return (bitmap[bit >> 3] >> (bit & 7)) & 1;
}

#endif /* ABA_BITMAP_H_ */
