#include <iostream>
#include <array>
#include <thread>
#include <cstdlib>
#include <random>
#include <chrono>
#include <atomic>

#include "../include/bst.hpp"
#include "../../../include/catch2/catch.hpp"
#include "../../../include/TSXGuard.hpp"
#include "../../../include/test_bench.hpp"


using namespace SafeTree;

#ifndef HACI3COMP
const static int THREADS = std::thread::hardware_concurrency();
#else
const static int THREADS = 28;
#endif

constexpr static int OPERATION_MULTIPLIER = 10000;

using TestBenchType = TestBench<BST<int>>;

TSX::SpinLock& lock = TestBenchType::global_lock;

TEST_CASE("BST Init Test","[init]") {
    BST<int> someMap(nullptr, lock);
    (void)someMap;
}


void insert(int i, BST<int>& map, int t_id) {
    for (int j = i * OPERATION_MULTIPLIER; j < (i+1)*OPERATION_MULTIPLIER; j++) {
         map.insert(j,1,t_id);
    }
}


void even_remove(BST<int>& map, int i) {
    for (int j = i * OPERATION_MULTIPLIER; j < (i+1)*OPERATION_MULTIPLIER; j++) {
        if (j % 2 == 0) {
            map.remove(j,i);
        }
    }
}


// // add values for balanced tree to vector
// void binary_vec(int start,int end, std::vector<int>& vec) {
    
//     if (start > end) return;

//     int mid = (start + end) / 2;

//     vec.push_back(mid);

//     binary_vec(start,mid-1,vec);
//     binary_vec(mid+1,end,vec);
// }

// void vec_insert(std::vector<int>& vec, BST<int>& map, int t_id) {
//     for (int j = t_id * OPERATION_MULTIPLIER; j <= (t_id + 1)*OPERATION_MULTIPLIER; j++) {
//          map.insert(vec[j],1,t_id);
//     }
// }



TEST_CASE("BST Find Test") {
    BST<int> someMap(nullptr, lock);
    someMap.setRoot(new BSTNode<int>(100,0, new BSTNode<int>(50,0,nullptr,nullptr),new BSTNode<int>(150,0,nullptr,nullptr)));

    REQUIRE(someMap.getRoot()->nextChild(120) == 1);
    REQUIRE(someMap.getRoot()->traversalDone(120) == false);
}


TEST_CASE("BST Insert Test","[insert]") {
    std::cout << "SINGLE THREADED INSERT" << std::endl;
    BST<int> someMap(nullptr, lock);

    SECTION("empty insert") {
        REQUIRE_NOTHROW(someMap.insert(1,2,0));
        REQUIRE(someMap.lookup(1).found);

        SECTION("in the middle insert") {
            REQUIRE_NOTHROW(someMap.insert(2,1,0));
            REQUIRE(someMap.lookup(2).found);
        }

        
        SECTION("another in the middle insert") {
            REQUIRE_NOTHROW(someMap.insert(-1,1,0));
            REQUIRE(someMap.lookup(-1).found);
        }

         SECTION("inserts all work together") {
            REQUIRE_NOTHROW(someMap.insert(4,1,0));
            REQUIRE(someMap.lookup(4).found);
            //someMap.print();
            REQUIRE_NOTHROW(someMap.insert(5,1,0));
            //someMap.print();
            REQUIRE(someMap.lookup(5).found);
            REQUIRE_NOTHROW(someMap.insert(6,1,0));
            REQUIRE(someMap.lookup(6).found);
            REQUIRE(someMap.lookup(4).found);
            REQUIRE(someMap.lookup(5).found);
            REQUIRE(someMap.lookup(6).found);
            REQUIRE_FALSE(someMap.lookup(10).found);
        }

        SECTION("too many inserts") {
            TestBenchType::binary_insert_map(0,THREADS*OPERATION_MULTIPLIER - 1,someMap);

            for (int i = 0; i < THREADS*OPERATION_MULTIPLIER; i++ ) {
                if (!someMap.lookup(i).found) {
                    std::cout << i << std::endl;
                }
                REQUIRE(someMap.lookup(i).found);
            }
        }
    }
}


TEST_CASE("BST Multithreaded Insert Test","[mt_insert]") {
    std::cout << "MULTITHREADED INSERT" << std::endl;

    for (int j = 0; j < 1; j++) { 
            BST<int> someMap(nullptr,lock);
            std::thread threads[THREADS];

            for (int i = 0; i < THREADS; i++) {
                threads[i] = std::thread(insert, i, std::ref(someMap), i);
            }

            for (int i = 0; i < THREADS; i++) {
                threads[i].join();
            }

            //someMap.print();

            for (int i = 0; i < THREADS*OPERATION_MULTIPLIER; i++) {
                if (!someMap.lookup(i).found) {
                    std::cout << "NOT " << i << std::endl;
                    REQUIRE(someMap.lookup(i).found);
                } 
            }

            REQUIRE(someMap.isSorted()); 

    }
}



// void even_remove( BST<int>& map, int t_id) {
//     const auto start = t_id * OPERATION_MULTIPLIER;
//     for (int k = 0; k < OPERATION_MULTIPLIER; k++) {
//             auto index = start + k;
//             if (index % 2 == 0) {
//                 map.remove(index,t_id);
//                 //REQUIRE(!map.lookup(index).found);
//             }
//     }
// }



TEST_CASE("BST Remove Test","[remove]") {
    SECTION("INTRO") {
        std::cout << "SINGLE THREADED REMOVE" << std::endl;
    }
    
    BST<int> someMap(nullptr, lock);

    SECTION("root remove") {
        someMap.insert(1,1,0);

        REQUIRE(someMap.lookup(1).found);

        REQUIRE(someMap.remove(1,0));

        REQUIRE(!someMap.lookup(1).found);
        
    }

    SECTION("leaf tree remove") {
        someMap.insert(1,1,0);
        someMap.insert(2,1,0);
        someMap.insert(3,1,0);

        

        someMap.remove(1,0);

        REQUIRE(!someMap.lookup(1).found);
        REQUIRE(someMap.lookup(2).found);
        REQUIRE(someMap.lookup(3).found);
    }

    SECTION("remove by replacing leftest subtree") {
        someMap.insert(6,1,0);
        someMap.insert(3,1,0);
        someMap.insert(9,1,0);
        someMap.insert(7,1,0);
        someMap.insert(10,1,0);
        someMap.insert(1,1,0);
        someMap.insert(4,1,0);
        someMap.insert(8,1,0);

        

        someMap.remove(6,0);

        REQUIRE(!someMap.lookup(6).found);
        REQUIRE(someMap.lookup(3).found);
        REQUIRE(someMap.lookup(9).found);
        REQUIRE(someMap.lookup(7).found);
        REQUIRE(someMap.lookup(10).found);
        REQUIRE(someMap.lookup(1).found);
        REQUIRE(someMap.lookup(4).found);
        REQUIRE(someMap.lookup(8).found);

    }

    SECTION("batch remove") {
        TestBenchType::binary_insert_map(0, THREADS*OPERATION_MULTIPLIER - 1,someMap);
        //someMap.print();
        for (int i = 0; i < THREADS*OPERATION_MULTIPLIER; i++) {
            if ((i % 2) == 0) {
                someMap.remove(i,0);
                if (someMap.lookup(i).found) {
                    std::cout << "SHOULDNT " << i << std::endl;
                    someMap.print();
                }
                REQUIRE(!someMap.lookup(i).found);
            } else {
                REQUIRE(someMap.lookup(i).found);
            }
            
        }
     }
    
}





TEST_CASE("BST MULTITHREADED Remove Test","[remove_mt]") {
    SECTION("intro") {
        std::cout << "MULTITHREADED Remove Test" << std::endl;
    }

    
    std::thread threads[THREADS];
    

    SECTION("mt remove") {
        for (int j = 0; j < 10; j++) {
            BST<int> someMap(nullptr, lock);
            TestBenchType::binary_insert_map(0, THREADS*OPERATION_MULTIPLIER - 1,someMap);

            for (int i = 0; i < THREADS; i++) {
                threads[i] = std::thread(even_remove, std::ref(someMap), i);
            }

            for (int i = 0; i < THREADS; i++) {
                threads[i].join();
            }

            for (int i = 0; i < THREADS*OPERATION_MULTIPLIER; i++) {
                if (i % 2 == 0) {
                    REQUIRE(!someMap.lookup(i).found);
                } else {
                    if (!someMap.lookup(i).found) {
                        std::cerr << "MISSING " << i << std::endl;
                        REQUIRE(someMap.lookup(i).found);
                    }
                }
            }

            REQUIRE(someMap.isSorted());

        }
        
    }
}


TEST_CASE("THROUGHPUT TESTS","[tp]") {
    const std::size_t RANGE_OF_KEYS = 2*OPERATION_MULTIPLIER; // RANGE IS 1 TO RANGE_OF_KEYS
   

    srand(time(NULL));


    SECTION("Totally random access") {
        TestBenchType::experiment exp(33,33,34);
        TestBenchType::test(exp,THREADS,RANGE_OF_KEYS,1);
    }

    SECTION("80% lookups") {
        TestBenchType::experiment exp(10,10,80);
        TestBenchType::test(exp,THREADS,RANGE_OF_KEYS,1);
    }

    SECTION("100% lookups") {
        TestBenchType::experiment exp(0,0,100);
        TestBenchType::test(exp,THREADS,RANGE_OF_KEYS,1);
    }


    SECTION("50-50 Insert Remove ") {
        TestBenchType::experiment exp(50,50,0);
        TestBenchType::test(exp,THREADS,RANGE_OF_KEYS,1);  
    }

    SECTION("50% lookups") {
        TestBenchType::experiment exp(25,25,50);
        TestBenchType::test(exp,THREADS,RANGE_OF_KEYS,1);
    }



}









