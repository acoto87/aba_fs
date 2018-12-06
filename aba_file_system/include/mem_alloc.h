
#ifndef ABA_MEM_ALLOC_H_
#define ABA_MEM_ALLOC_H_

#define FUSE_USE_VERSION 25
#define _FILE_OFFSET_BITS 64

#include "types.h"
#include "error.h"

/*
 * reserva un espacio en la memoria de tamano size
 */
static __inline__ void* xmalloc (size_t size)
{
  register void *value = malloc (size);
  if (value == 0)
    error (-ENOMEM, "Virtual memory exhausted");
  return value;
}

/*
 * reserva un espacio en memoria con nmemb elementos
 * de tamano size incializados en 0
 */
static __inline__ void* xcalloc(size_t nmemb, size_t size){
	size_t len = nmemb*size;
	register void *value = xmalloc(len);
	memset(value, 0, len);
	return value;
}

/*
 * reserva un nuevo espacio de tamano size en memoria para el puntero ptr
 */
static __inline__ void* xrealloc (void *ptr, size_t size)
{
  register void *value = realloc (ptr, size);
  if (value == 0)
    error(-ENOMEM, "Virtual memory exhausted");
  return value;
}

#endif /* ABA_MEM_ALLOC_H_ */
