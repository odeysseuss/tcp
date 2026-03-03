/*
* Pool allocator
*
* NOTE: To use this library define the following macro in exactly one file
* before inlcuding pool.h:
*   #define POOL_IMPLEMENTATION
*   #include "pool.h"
*/

#ifndef POOL_H
#define POOL_H

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Used for custom general purpose allocators
/// To use a custom allocators simply define them to the specific allocator's functions
/// Warning: The function signatures has to be the same as stdlib's allocator
/// Recommended Allocator: Microsoft's mimalloc
#define malloc_ malloc
#define calloc_ calloc
#define realloc_ realloc
#define free_ free

typedef struct {
    void *memory;
    void **free_list;
    size_t chunk_size;
} PoolAlloc;

PoolAlloc *poolInit(size_t chunk_size, size_t num_chunks);
void *poolAlloc(PoolAlloc *pool);
void poolFree(PoolAlloc *pool, void *chunk);
void poolDestroy(PoolAlloc *pool);

#ifdef POOL_IMPLEMENTATION

PoolAlloc *poolInit(size_t chunk_size, size_t num_chunks) {
    if (chunk_size == 0 || num_chunks == 0) {
        return NULL;
    }

    PoolAlloc *pool = malloc_(sizeof(PoolAlloc));
    memset(pool, 0, sizeof(PoolAlloc));
    pool->chunk_size = chunk_size;
    pool->memory = calloc(num_chunks, chunk_size);
    pool->free_list = pool->memory;

    char *chunk = (char *)pool->memory;
    for (size_t i = 0; i < num_chunks - 1; i++) {
        void **curr = (void **)(chunk + i * chunk_size);
        void **next = (void **)(chunk + (i + 1) * chunk_size);
        *curr = next;
    }

    void **last = (void **)(chunk + (num_chunks - 1) * chunk_size);
    *last = NULL;

    return pool;
}

void *poolAlloc(PoolAlloc *pool) {
    if (!pool || !pool->free_list) {
        return NULL;
    }

    void *chunk = pool->free_list;
    pool->free_list = *(void ***)chunk;

    return chunk;
}

void poolFree(PoolAlloc *pool, void *chunk) {
    if (!pool || !chunk) {
        return;
    }

    *(void **)chunk = pool->free_list;
    pool->free_list = (void **)chunk;
}

void poolDestroy(PoolAlloc *pool) {
    if (!pool) {
        return;
    }

    free_(pool->memory);
    free_(pool);
}

#endif // POOL_IMPLEMENTATION

#undef malloc_
#undef calloc_
#undef realloc_
#undef free_

#ifdef __cplusplus
}
#endif
#endif
