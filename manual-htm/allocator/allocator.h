#ifndef M_ALLOCATOR_H
    #define M_ALLOCATOR_H
#include <pthread.h>

#define NR_NODES 30000000
#define MAX_THREADS 100



typedef struct allocator {
    void* blocks;
    long index;
    int shifts;
} allocator;


static __thread allocator* alloc;

pthread_mutex_t lock;

static allocator* pools;
static int alloc_index;

static void** extra_blocks;
static int extra_index;

#endif












