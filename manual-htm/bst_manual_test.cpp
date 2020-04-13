#include <iostream>
#include <array>
#include <thread>
#include <cstdlib>
#include <random>
#include <chrono>
#include <atomic>

#include "../include/catch2/catch.hpp"
#include "../include/test_bench_manual.hpp"
#include "../include/test_bench_conf.hpp"
#include "bst_manual.h"


#ifndef HACI3COMP
const static int THREADS = std::thread::hardware_concurrency();
#else
const static int THREADS = 28;
#endif
//constexpr static int OPERATION_MULTIPLIER = 10000;

using TestBenchType = TestBench<Map>;


TEST_CASE("THROUGHPUT TESTS","[tp]") {
    const int OPERATION_MULTIPLIERS[] = {TREE_SIZE};
    std::cout << TREE_SIZE << std::endl;
    std::cout << THREAD_COUNT << std::endl;


    for (int i = 0; i < 1; i++) {
        std::cout << "Start of tests for tree size: " << TREE_SIZE << std::endl;
        const std::size_t RANGE_OF_KEYS = 2 * OPERATION_MULTIPLIERS[i]; // RANGE IS 1 TO RANGE_OF_KEYS

        std::vector<int> threads_to_use = {THREAD_COUNT};
        TestBenchType::experiment exp1(I,100-I-L,L);
        TestBenchType::test(exp1,THREADS,RANGE_OF_KEYS,threads_to_use);
 
    }
}


