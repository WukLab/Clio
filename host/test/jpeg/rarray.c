#include "rarray.h"
#include "rmem.h"
#include "test.h"

int rarray_read(struct remote_mem *mem, void * lbuf, uint64_t index, size_t size, int buffer_index, int thread_id)
{
    int cell_size = config.array_cell_size;
    size_t realsize = (size == RARRAY_SZ_CELL) ? cell_size : size;
    return rread(mem, lbuf, index * cell_size, realsize, buffer_index, thread_id);
}

int rarray_write(struct remote_mem *mem, void * lbuf, size_t index, size_t size, int buffer_index, int thread_id)
{
    int cell_size = config.array_cell_size;
    size_t realsize = (size == RARRAY_SZ_CELL) ? cell_size : size;
    return rwrite(mem, lbuf, index * cell_size, realsize, buffer_index, thread_id);
}
