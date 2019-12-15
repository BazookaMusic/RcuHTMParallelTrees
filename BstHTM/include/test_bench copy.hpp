#ifndef TEST_BENCH_HPP
    #define TEST_BENCH_HPP


#include <iostream>
#include <array>
#include <thread>
#include <cstdlib>
#include <random>
#include <chrono>
#include <atomic>

#include "../include/catch.hpp"
#include "../include/TSXGuard.hpp"

template <class MapType>
class TestBench {
    public:

        ~TestBench() {
            delete [] threads;
            delete [] thread_stats;
        }

        // operation stats per thread
        struct t_ops {
            std::size_t n_ops;
            std::size_t i_ops;
            std::size_t r_ops;
            std::size_t l_ops;
            std::size_t light_ops_ins;
            std::size_t light_ops_rems;

            void reset() {
                n_ops = i_ops = r_ops = l_ops = 0;
            }
        };

         // test parameters
        struct experiment {
            int inserts;
            int removes;
            int lookups;

            experiment(int inserts,int removes, int lookups): inserts(inserts), removes(removes), lookups(lookups){}
        };

        static int THREADS;
        static TSX::SpinLock global_lock;
    

        static void setMaxThreads(int max_threads) {
            THREADS = max_threads;
        }

        
        
        // make a balanced tree
        static void binary_insert_map(int start,int end, MapType& map) {
            if (start > end) return;

            int mid = (start + end) / 2;

            map.insert(mid,1,0);

            binary_insert_map(start,mid-1,map);
            binary_insert_map(mid+1,end,map);
        }

        static bool isEven(int n) {
            return n % 2 == 0;
        }

        template <typename F>
        static void binary_insert_map_pred(int start,int end, MapType& map, F&& predicate) {
            if (start > end) return;

            
            int mid = (start + end) / 2;

            predicate(mid);
            if (mid % 2 == 0) {
                map.insert(mid,1,0);
            }
            

            binary_insert_map(start,mid-1,map);
            binary_insert_map(mid+1,end,map);
        }

        
        // // make a balanced tree
        static void binary_insert_map_even(int start,int end, MapType& map) {
            if (start > end) return;

            int mid = (start + end) / 2;

            if (mid % 2 == 0) {
                map.insert(mid,1,0);
            }
        

            binary_insert_map(start,mid-1,map);
            binary_insert_map(mid+1,end,map);
        }



        
        static std::thread* threads;
        static t_ops* thread_stats;

       

        // perform a test using the given operations
        static void test(experiment exp, const std::size_t RANGE_OF_KEYS, int start_threads = 1) {

            for (int i = 0; i < THREADS; i++) {
                thread_stats[i].reset();
            }

            static bool run;
            run = true;
            
            for (int max_threads = start_threads; max_threads <= THREADS; max_threads++) {
                    MapType aMap(nullptr,global_lock);
                    binary_insert_map_pred(0,RANGE_OF_KEYS, aMap, isEven); // insert even numbers only

                    run = true;

                    for (int i = 0; i < max_threads; i++) {
                        thread_stats[i].n_ops = 0;
                        thread_stats[i].i_ops = 0;
                        thread_stats[i].r_ops = 0;
                        thread_stats[i].l_ops = 0;

                        thread_stats[i].light_ops_ins = 0;
                        thread_stats[i].light_ops_rems = 0;
                    }

                    for (int i = 0; i < max_threads; i++) {
                            threads[i] = std::thread(rand_op, std::ref(run), std::ref(aMap), RANGE_OF_KEYS, i, std::ref(thread_stats[i]), 33,33,34);
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

                    run = false;

                    auto sum_ops = 0;

                    for (int i = 0; i < max_threads; i++) {
                        sum_ops += thread_stats[i].n_ops;
                    }

                    op_stats(thread_stats,max_threads,exp.inserts,exp.removes,exp.lookups);
                    aMap.lite_stat(max_threads);

                    for (int i = 0; i < max_threads; i++) {
                            threads[i].join();
                    }

                    //aMap.stat_report(max_threads);
                    //aMap.lite_stat(max_threads);

                    //aMap.print();

                    //std::cout << aMap.size() << std::endl;

                    //aMap.lite_stat(max_threads);
                    REQUIRE(aMap.isSorted());
                    aMap.longest_branch();
                    aMap.average_branch();
            }
        }

        static inline int intRand(const int & min, const int & max, const int t_id) {
            static thread_local std::mt19937 generator(t_id);
            std::uniform_int_distribution<int> distribution(min,max);
            return distribution(generator);
        }



        static void rand_op(bool& run,MapType& map, const int range, const int t_id,t_ops& t_op, const int ins_freq,const int rem_freq,const int look_freq) {
            const auto total = ins_freq + rem_freq + look_freq;
            while (run) {
                int rand_n = intRand(1, total, t_id);

                int key = intRand(0, range, t_id);

                if (rand_n <= ins_freq) {
                    if (!map.insert(key,1, t_id)) {
                        //std::cout << "I:" << key << std::endl;
                        ++t_op.light_ops_ins;
                        }
                    ++t_op.i_ops;
                } else if (rand_n <= ins_freq + rem_freq) {
                    if (!map.remove(key, t_id)) {
                        //std::cout << "R:" << key << std::endl;
                        ++t_op.light_ops_rems;
                        }
                    ++t_op.r_ops;
                } else {
                    map.lookup(key);
                    ++t_op.l_ops;
                }

                ++t_op.n_ops;
            } 
        }


        static void op_stats(t_ops* ops,int threads, const int ins_freq,const int rem_freq,const int look_freq) {
            std::size_t sum = 0;
            std::size_t sumi = 0;
            std::size_t sumr = 0;
            std::size_t suml = 0;
            std::size_t l_op_sum_ins = 0;
            std::size_t l_op_sum_rems = 0;

            for (int i = 0; i < threads; i++) {
                sum += ops[i].n_ops;
                sumi += ops[i].i_ops;
                sumr += ops[i].r_ops;
                suml += ops[i].l_ops;

                l_op_sum_ins += ops[i].light_ops_ins;
                l_op_sum_rems += ops[i].light_ops_rems;
            }

            double M_OPS = (1.0 * sum) / 5000000;


            std::cout << "THREADS: " << threads << " I: " << ins_freq << " R: " << rem_freq << " L: " << look_freq << " MOPS: " << M_OPS << std::endl;
        }

};

template <class MapType>
TSX::SpinLock TestBench<MapType>::global_lock{};

template <class MapType>
int TestBench<MapType>::THREADS = std::thread::hardware_concurrency();

template <class MapType>
std::thread* TestBench<MapType>::threads = new std::thread[THREADS];

template <class MapType>
typename TestBench<MapType>::t_ops* TestBench<MapType>::thread_stats = new t_ops[THREADS];

#endif