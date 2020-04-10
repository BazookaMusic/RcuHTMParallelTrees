#include <iostream>
#include "map_if.h"
#include "../include/TSXGuard.hpp"

void* t_datas[50];

template <class T>
struct ResultType {
    bool found;
    T* item;

    ResultType(bool _found_, T* item__): found(_found_), item(item__){}
};

class Map {
    private:
        void* root_;
        void* map_;
        static thread_local void* t_data;
        TSX::SpinLock& lock_;



    public:
        Map(void* root, TSX::SpinLock& lock): root_(root), lock_(lock){
            map_ = map_new();
            t_data = map_tdata_new(0);
            (void)root_;
            (void)lock_;
        }

        void setup(int t_id) {
            t_data = map_tdata_new(t_id);
        }

        bool insert(int k, int val, int t_id) {
            (void)t_id;
            (void)val;
            return map_insert(map_, t_data,k, nullptr) != 0;
        }

        bool remove(int k, int t_id) {
            (void)t_id;
            return map_delete(map_, t_data,k);
        }

        ResultType<int> lookup(int k) {
            int res = map_lookup(map_, nullptr, k);

            ResultType<int> final_res = ResultType<int>(res != 0, nullptr);

            return final_res;
        }
};

thread_local void* Map::t_data = nullptr;