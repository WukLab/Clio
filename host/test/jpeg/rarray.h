#ifndef _RARRAY_H_
#define _RARRAY_H_

#include "rmem.h"

// array apis
#define RARRAY_SZ_CELL 0

int rarray_read(struct remote_mem *mem, void * lbuf, size_t index, size_t size);
int rarray_write(struct remote_mem *mem, void * lbuf, size_t index, size_t size);

#endif
