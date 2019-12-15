#include <iostream>

#include "bst-serial.hpp"
#include "../include/catch.hpp"

#include <cstdlib>     /* srand, rand */
#include <ctime> 

static constexpr int BASE_SIZE = 1000000;

TEST_CASE("BST Insert") {
    BSTSerial<int> aMap;

    for (int i = 0; i < 1000; i++) {
        aMap.insert(i, 0);
    }

    for (int i = 0; i < 1000; i++) {
        if (!aMap.contains(i)) {
            REQUIRE(aMap.contains(i));
        }
    }
}

TEST_CASE("BST Remove") {
    BSTSerial<int> aMap;

    for (int i = 0; i < 1000; i++) {
        aMap.insert(i, 0);
    }

    for (int i = 0; i < 1000; i+=2) {
        aMap.remove(i);
    }

     for (int i = 0; i < 1000; i++) {
         if (i % 2 == 0) {
             REQUIRE(!aMap.contains(i));
         } else {
             REQUIRE(aMap.contains(i));
         }
    }
}

// make a balanced tree
void binary_insert_map(int start,int end, BSTSerial<int>& map) {
    if (start > end) return;

    int mid = (start + end) / 2;
    
    map.insert(mid,1);

    binary_insert_map(start,mid-1,map);
    binary_insert_map(mid+1,end,map);
}

TEST_CASE("OP TESTS") {
    srand (time(NULL));

    BSTSerial<int> aMap;


    binary_insert_map(0,BASE_SIZE / 2, aMap);

    SECTION("INSERTS REMOVES 50-50") {
        BENCHMARK_ADVANCED("INSERTS REMOVES 50-50")(Catch::Benchmark::Chronometer meter){

            meter.measure([&] {
                for (int i = 0; i < BASE_SIZE; i++) {
                    int remove_or_insert = rand() % 2;
                    int which = rand() % (BASE_SIZE + 1);

                    if (remove_or_insert) {
                        aMap.insert(which,1); 
                    } else {
                        aMap.remove(which);
                    }
                } 
            });
            
        };
    }

    SECTION("INSERTS REMOVES 80-20") {

        BENCHMARK_ADVANCED("INSERTS REMOVES 80-20")(Catch::Benchmark::Chronometer meter){
            
            meter.measure([&] {
                for (int i = 0; i < BASE_SIZE; i++) {
                    int remove_or_insert = rand() % 10;
                    int which = rand() % (BASE_SIZE + 1);

                    if (remove_or_insert < 8) {
                        aMap.insert(which,1); 
                    } else {
                        aMap.remove(which);
                    }
                }
                
                
            });
        };

    }
   

    SECTION("INSERTS REMOVES 20-80") {
        BENCHMARK_ADVANCED("INSERTS REMOVES 20-80")(Catch::Benchmark::Chronometer meter){


            meter.measure([&] {
                for (int i = 0; i < BASE_SIZE; i++) {
                    int remove_or_insert = rand() % 10;
                    int which = rand() % (BASE_SIZE + 1);

                    if (remove_or_insert >= 8) {
                        aMap.insert(which,1); 
                    } else {
                        aMap.remove(which);
                    }
                }
            });

        };
    }

    SECTION("INSERTS LOOKUPS REMOVES 33-33-33") {
         BENCHMARK_ADVANCED("INSERTS LOOKUPS REMOVES 33-33-33")(Catch::Benchmark::Chronometer meter){
            meter.measure([&] {
                for (int i = 0; i < BASE_SIZE; i++) {
                    int remove_insert_lookup = rand() % 3;
                    int which = rand() % (BASE_SIZE + 1);

                    if (remove_insert_lookup == 0) {
                        aMap.insert(which,1); 
                    } else if (remove_insert_lookup == 1) {
                        aMap.remove(which);
                    } else {
                        aMap.contains(which);
                    }
                }
                
            });
    
        };
    }

    
    SECTION("INSERTS 100") {
        BENCHMARK_ADVANCED("INSERTS 100")(Catch::Benchmark::Chronometer meter){
            meter.measure([&] {
                for (int i = 0; i < BASE_SIZE; i++) {
                    int which = rand() % (BASE_SIZE + 1);
                    aMap.insert(which,1); 
                }
                
            });
    
        };
    }
   

    SECTION("REMOVES 100") {
        BENCHMARK_ADVANCED("REMOVES 100")(Catch::Benchmark::Chronometer meter){
            meter.measure([&] {
                for (int i = 0; i < BASE_SIZE; i++) {
                    int which = rand() % ((BASE_SIZE / 2) + 1);
                    aMap.remove(which); 
                }
            });
    
        };
    }

    
}





