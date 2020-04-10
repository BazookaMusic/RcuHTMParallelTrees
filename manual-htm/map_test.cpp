#include <iostream>
#include <array>
#include <thread>
#include <cstdlib>
#include <random>
#include <chrono>
#include <atomic>

#include "../include/catch2/catch.hpp"
#include "../include/TSXGuard.hpp"
#include "../include/test_bench.hpp"
#include "map.h"


#ifndef HACI3COMP
const static int THREADS = std::thread::hardware_concurrency();
#else
const static int THREADS = 28;
#endif
//constexpr static int OPERATION_MULTIPLIER = 10000;

using TestBenchType = TestBench<Map>;

TSX::SpinLock& lock = TestBenchType::global_lock;


TEST_CASE("THROUGHPUT TESTS","[tp]") {
    const int OPERATION_MULTIPLIERS[] = {1000};

    for (int i = 0; i < 4; i++) {
        std::cout << "Start of tests for tree size: " << OPERATION_MULTIPLIERS[i] << std::endl;
        const std::size_t RANGE_OF_KEYS = 2 * OPERATION_MULTIPLIERS[i]; // RANGE IS 1 TO RANGE_OF_KEYS

        std::vector<int> threads_to_use = {1,2,4,7,14,20,28};
        // RANDOM OPS
        TestBenchType::experiment exp1(33,33,34);
        TestBenchType::test(exp1,THREADS,RANGE_OF_KEYS,threads_to_use);
        
        // 10 - 10 -80
        TestBenchType::experiment exp2(10,10,80);
        TestBenchType::test(exp2,THREADS,RANGE_OF_KEYS,threads_to_use);

        // 100% LOOKUPS
        TestBenchType::experiment exp3(0,0,100);
        TestBenchType::test(exp3,THREADS,RANGE_OF_KEYS, threads_to_use);
        

        // 50-50 UPDATES
        TestBenchType::experiment exp4(50,50,0);
        TestBenchType::test(exp4,THREADS,RANGE_OF_KEYS,threads_to_use);  
    
        // 25-25 UPDATES, 50 LOOKUPS
        TestBenchType::experiment exp5(25,25,50);
        TestBenchType::test(exp5,THREADS,RANGE_OF_KEYS,threads_to_use);
        
    }
}




