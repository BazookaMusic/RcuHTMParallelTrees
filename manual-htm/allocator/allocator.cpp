#include "allocator.h"

void* pool_create() {
	return (void*)pool.create();
}

extern "C" void *nalloc_init()
{
	return NULL;
}

extern "C" void *nalloc_thread_init(int tid, size_t sz)
{
	(void)tid;
    (void)sz;
    return nullptr;
}

extern "C" void *nalloc_alloc_node(void *_nalloc)
{
	return pool_create();
	
}

extern "C" void nalloc_free_node(void *nalloc, void *node)
{
}
