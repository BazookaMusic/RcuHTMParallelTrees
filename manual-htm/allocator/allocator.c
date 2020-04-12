#include <stdlib.h>
#include "allocator.h"

#define ALIGNMENT 32


void *nalloc_init()
{
	pthread_mutex_init(&lock, NULL);
	

	return NULL;
}

void *nalloc_thread_init(int tid, size_t sz)
{
	pthread_mutex_lock(&lock);
	if (!pools) {
		alloc_index = -1;
		pools = malloc(MAX_THREADS*sizeof(allocator));

		for (int i = 0; i < MAX_THREADS; i++) pools[i].blocks = NULL;
	}

	
	alloc = &pools[tid];

	if (!alloc->blocks) {
		++alloc_index;
		alloc->blocks = aligned_alloc(ALIGNMENT,NR_NODES*sz);

		alloc->index = -1;
		alloc->shifts = 0;
		while (sz > 0) {
			alloc->shifts += 1;
			sz >>= 1;
		}
	}
	
	
	pthread_mutex_unlock(&lock);
    return NULL;
}

void *nalloc_alloc_node(void *_nalloc)
{
	// bound check
	if (alloc->index >= NR_NODES) {
		printf("OOB");
		exit(-1);
	}
	
	return &alloc->blocks[(++alloc->index) << alloc->shifts];
}

void nalloc_free_node(void *nalloc, void *node)
{
}

void nalloc_destroy() {
	// free blocks of nodes
	for (int i = 0; i < alloc_index; i++) {
		free(pools[i].blocks);
		pools[i].blocks = NULL;
	}

	// free thread pools
	free(pools);
	pools = NULL;
}
