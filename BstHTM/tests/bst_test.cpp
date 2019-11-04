#include <iostream>
#include <array>
#include <thread>

#include "../include/bst.hpp"
#include "../include/catch.hpp"
#include "../include/TSXGuard.hpp"

constexpr static int THREADS = 4;
constexpr static int OPERATION_MULTIPLIER = 1000000;
TSX::SpinLock lock{};

TEST_CASE("BST Init Test","[init]") {
    BST<int> someMap(nullptr, lock);
    (void)someMap;
}

void insert(int i, BST<int>& map, int t_id) {
    for (int j = i * OPERATION_MULTIPLIER + 1; j <= (i+1)*OPERATION_MULTIPLIER; j++) {
         map.insert(j,1,t_id);
    }
}



// make a balanced tree
void binary_insert_map(int start,int end, BST<int>& map) {
    if (start > end) return;

    int mid = (start + end) / 2;

    map.insert(mid,1,0);

    binary_insert_map(start,mid-1,map);
    binary_insert_map(mid+1,end,map);
}

// add values for balanced tree to vector
void binary_vec(int start,int end, std::vector<int>& vec) {
    
    if (start > end) return;

    int mid = (start + end) / 2;

    vec.push_back(mid);

    binary_vec(start,mid-1,vec);
    binary_vec(mid+1,end,vec);
}

void vec_insert(std::vector<int>& vec, BST<int>& map, int t_id) {
    for (int j = t_id * OPERATION_MULTIPLIER; j <= (t_id + 1)*OPERATION_MULTIPLIER; j++) {
         map.insert(vec[j],1,t_id);
    }
}



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
            REQUIRE_NOTHROW(someMap.insert(3,1,0));
            REQUIRE(someMap.lookup(3).found);
        }

         SECTION("inserts all work together") {
            REQUIRE_NOTHROW(someMap.insert(4,1,0));
            REQUIRE_NOTHROW(someMap.insert(5,1,0));
            REQUIRE_NOTHROW(someMap.insert(6,1,0));
            REQUIRE(someMap.lookup(4).found);
            REQUIRE(someMap.lookup(5).found);
            REQUIRE(someMap.lookup(6).found);
            REQUIRE_FALSE(someMap.lookup(10).found);
        }

        SECTION("too many inserts") {
            binary_insert_map(0,THREADS*OPERATION_MULTIPLIER - 1,someMap);

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

    std::vector<int> data;
    data.reserve(THREADS*OPERATION_MULTIPLIER);
    binary_vec(0,THREADS*OPERATION_MULTIPLIER, data);



    for (int j = 0; j < 2; j++) { 
            BST<int> someMap(nullptr,lock);
            std::thread threads[THREADS];

            for (int i = 0; i < THREADS; i++) {
                threads[i] = std::thread(vec_insert, std::ref(data), std::ref(someMap), i);
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
    }
}



void even_remove(int i, BST<int>& map, int t_id) {
    for (int j = i * OPERATION_MULTIPLIER; j < (i+1)*OPERATION_MULTIPLIER; j++) {
        if ((j % 2) == 0) {
            map.remove(j,t_id);
        }
    }
}



TEST_CASE("BST Remove Test","[remove]") {
    SECTION("INTRO") {
        std::cout << "SINGLE THREADED REMOVE" << std::endl;
    }
    
    BST<int> someMap(nullptr, lock);

    SECTION("root remove") {
        someMap.insert(1,1,0);

        REQUIRE(someMap.lookup(1).found);

        someMap.remove(1,0);

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


        //someMap.print();

        

        someMap.remove(6,0);

        //std::cout << "7 IS "<< someMap.getRoot()->getChild(1)->getChild(0) << std::endl;
        
        //someMap.print();

        REQUIRE(!someMap.lookup(6).found);
        REQUIRE(someMap.lookup(3).found);
        REQUIRE(someMap.lookup(9).found);
        REQUIRE(someMap.lookup(7).found);
        REQUIRE(someMap.lookup(10).found);



    }

    SECTION("batch remove") {
        binary_insert_map(0, THREADS*OPERATION_MULTIPLIER - 1,someMap);

        for (int i = 0; i < THREADS*OPERATION_MULTIPLIER; i++) {
            if ((i % 2) == 0) {
                someMap.remove(i,0);
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

    BST<int> someMap(nullptr, lock);
    std::thread threads[THREADS];
    

    SECTION("mt remove") {
        for (int j = 0; j < 5; j++) {
            binary_insert_map(0, THREADS*OPERATION_MULTIPLIER - 1,someMap);
           // someMap.print();

            for (int i = 0; i < THREADS; i++) {
                threads[i] = std::thread(even_remove, THREADS-i-1 , std::ref(someMap), i);
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

        }

        someMap.stat_report(THREADS);
        
    }








}






