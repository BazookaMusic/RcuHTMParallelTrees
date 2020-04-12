#ifndef ALLOCATOR_ALLOCATOR_H
    #define ALLOCATOR_ALLOCATOR_H
#include <memory>
#include <iostream>
#include <mutex>
#include <new>
#include <cassert>
#include <thread>

#define NR_NODES 15000000

static constexpr int MAX_THREADS = 200;

struct alignas(32) dummy_block {
    unsigned char contents[64];
} ;

 template<class Object>
        struct memory_pool {
            using buffer_type = typename std::aligned_storage<sizeof(Object), alignof(Object)>::type;

            static_assert(sizeof(buffer_type) >= sizeof(Object), "wrong size");

            explicit memory_pool(std::size_t limit)
                    : objects_(nullptr),limit_(limit), used_(0) {
                        std::cout << "constructed" << std::endl;
            }

            memory_pool(const memory_pool&) = delete;
            memory_pool(memory_pool&&) = default;
            memory_pool& operator=(const memory_pool&) = delete;
            memory_pool& operator=(memory_pool&&) = delete;

            template<class...Args>
            Object* create(Args &&...args) {
                if (used_ < limit_) {
                    std::cout << &objects_[used_] << std::endl;
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
            const std::size_t limit_;
            std::size_t used_;
};

 template<class Object>
        struct memory_pool_tracked {

            using buffer_type = typename std::aligned_storage<sizeof(Object), alignof(Object)>::type;
            
            explicit memory_pool_tracked(std::size_t limit): pool_(limit) {
                fill_pool();
            }

            void fill_pool() {
                pool_lock_.lock();
                pool_.objects_ = new buffer_type[pool_.limit_];
                //std::cout << pool_.objects_ << " allocated " << std::endl;
                ++index_;
                std::cout << index_ << ". " << pool_.objects_ << std::endl;
                thread_buffers_[index_] = pool_.objects_;
                pool_lock_.unlock();
            }

            template<class...Args>
            Object* create(Args &&...args) {
                return pool_.create(args...);
            }
            
            void hard_reset() {
                //std::cout << "DELETING" << std::endl;
                for (auto i = 0; i <= index_; i++) {
                    std::cout << i << ". "<< thread_buffers_[i] << std::endl;
                    delete [] thread_buffers_[i];
                    thread_buffers_[i] = nullptr;
                }

                pool_.used_ = 0;
                pool_.objects_ = nullptr;

                index_ = -1;

            }

            void set_checkpoint() {
                checkpoint_ = pool_.used_;
            }

            void reset_to_checkpoint() {
                pool_.used_ = checkpoint_;
            }

            std::size_t checkpoint_;

            memory_pool<Object> pool_;

            static std::mutex pool_lock_;
            static typename memory_pool_tracked<Object>::buffer_type** thread_buffers_;
            static int index_;

        };

        template<class Object>
        std::mutex memory_pool_tracked<Object>::pool_lock_;

        template<class Object>
        typename memory_pool_tracked<Object>::buffer_type** memory_pool_tracked<Object>::thread_buffers_ = new buffer_type*[MAX_THREADS];

        template<class Object>
        int memory_pool_tracked<Object>::index_ = -1;





static thread_local memory_pool_tracked<dummy_block> pool(NR_NODES);


#endif