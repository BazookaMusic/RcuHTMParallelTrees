#include <memory>
#include <iostream>

#define NR_NODES 30000000

struct alignas(32) dummy_block {
    int i;
} ;

 template<class Object>
        struct memory_pool {
            using buffer_type = typename std::aligned_storage<sizeof(Object), alignof(Object)>::type;

            static_assert(sizeof(buffer_type) >= sizeof(Object), "wrong size");

            explicit memory_pool(std::size_t limit)
                    : objects_(new buffer_type[limit]),limit_(limit), used_(0) {
            }

            memory_pool(const memory_pool&) = delete;
            memory_pool(memory_pool&&) = default;
            memory_pool& operator=(const memory_pool&) = delete;
            memory_pool& operator=(memory_pool&&) = delete;

            template<class...Args>
            Object* create(Args &&...args) {
                if (used_ < limit_) {
                    auto candidate = new(std::addressof(objects_[used_])) Object(std::forward<Args>(args)...);
                    ++ used_;
                    return candidate;
                }
                else {
                    std::cerr << "OOM Buffer used: " << used_ << std::endl;
                    exit(-1);
                    return nullptr;
                }
            }


            buffer_type* reset() {
                used_ = 0;
                return objects_;
            }

            buffer_type* objects_;
            std::size_t limit_;
            std::size_t used_;
};

static thread_local memory_pool<dummy_block> pool(NR_NODES);

typedef struct {
	void *free_nodes;
	int index;
	int tid;
} tdata_t;


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
	return (void*)pool.create();
}

extern "C" void nalloc_free_node(void *nalloc, void *node)
{
}
