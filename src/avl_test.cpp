// Copyright 2019 Sotiris Dragonas
#include <chrono>
#include <sstream>
#include <iostream>
#include <ctime>
#include <thread>

#include "include/avl.hpp"
#include "include/catch.hpp"
#include "include/SafeTree.hpp"
#include "include/urcu.hpp"

#include "utils/test_helpers.cpp"

// just used for testing, as a temp
// to avoid having to redeclare in every test
std::string test_string;

const unsigned int N_INSERT = 1000000;
const unsigned int N_REMOVES = 1000000;


bool avlCreation() {
    auto avlnode = new AVLNode<int>(12, 12, nullptr, nullptr);
    auto newNode = new AVLNode<int>(11, 12, nullptr, nullptr);

    avlnode->setL(newNode);


    return avlnode->getL() == newNode;

    
}




bool StackTest() {
    AVLNode<int> *node;
    auto stack = new TreePathStack<AVLNode<int>>();



    test_string = "TESTING PUSH:";

    std::cout << test_string;

    for (size_t i = 0; i < STACK_DEPTH; i++) {
        node = new AVLNode<int>(i, 12, NULL, NULL);
        stack->push(node);
    }
    okWithPadding(test_string);

    test_string = "TESTING POP:";

    std::cout << test_string;
    for (int i = 0; i < STACK_DEPTH; i++) {
        auto key = stack->pop()->key;

        if (key != (STACK_DEPTH - 1 - i)) {
            return false;
        }
    }

    okWithPadding(test_string);

    return true;
}



bool insertTest() {
    

    std::ostringstream testss;

    testss << "TESTING INSERTS "
            << "WITH NUMBER OF INSERTIONS = " << N_INSERT << ":";

    std::cout<< testss.str();


    auto tree = AVLTree<int>();
    for (unsigned int i = 0; i < N_INSERT; i++) {
         tree.insert(i, 1);
    }

    for (unsigned i = 0; i < N_INSERT; i++) {
        if (!tree.lookup(i)) return false;
    }


    if (!tree.isAVL()) {
        return false;
    }

    okWithPadding(testss.str());

    return true;
}


bool removeTest() {

    std::ostringstream testss;

    testss << "TESTING REMOVES "
        << "WITH NUMBER OF INSERTIONS = " <<  N_REMOVES / 2 << ":";

    std::cout << testss.str();
    auto tree = AVLTree<int>();
    for (unsigned int i = 0; i < N_REMOVES; i++) {
         tree.insert(i, 1);
    }

    for (unsigned i = 0; i < N_REMOVES; i++) {
        if (!tree.lookup(i)) return false;
    }


    for (unsigned int i = 0; i < N_REMOVES / 2; i++) {
        tree.remove(i);
    }

    for (unsigned i = 0; i < N_REMOVES / 2; i++) {
        if (tree.lookup(i)) return false;
    }

    for (unsigned i = N_REMOVES / 2; i < N_REMOVES; i++) {
        if (!tree.lookup(i)) return false;
    }

     if (!tree.isAVL()) {
        return false;
    }

    okWithPadding(testss.str());
    return true;
}



void job() {
    int i = 2;
    i++;
}

#define T_COUNT 200

void threadTest() {
    std::thread threads[T_COUNT];
    for (size_t i = 0; i < T_COUNT; i++) {
        threads[i] = std::thread(job);
    }

    for (size_t i = 0; i < T_COUNT; i++) {
        threads[i].join();
    }
}

typedef AVLNode<int> testNode;

// bool genericNodeTest() {
//     test_string = "Testing creation";
//     std::cout << test_string;
//     const unsigned int N_INSERT = 20;
//     auto tree = AVLTree<int>();
//     for (unsigned int i = 0; i < N_INSERT; i++)
//     {
//          tree.insert(i,1);
//     }

//     std::unique_ptr<TreePathStack<testNode>>
//                       myPath(new TreePathStack<testNode>());


//     InsPoint<testNode> in(tree.getRootPointer(), myPath);


//     auto firstnode = in.getHead();
//     auto first_tree_node = tree.getRoot();

//     if (firstnode->safeRef()->key != first_tree_node->key) {
//         return false;
//     }

//     okWithPadding(test_string);

//     return true;




// }

const int RCU_THREADS = 100;



// simulates a read critical area
void readArea(int thread_index, std::atomic<int> &counter, RCU& rcu) {
    auto sentinel = rcu.urcu_register_thread(thread_index);

    // lock read area
    RCULock locked = sentinel.urcu_read_lock();

    // read
    counter++;

    // add small delay to make sure that
    // wait thread starts before end
    // fuzzy test, should only fish out weird issues
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

// simulates a waiting thread
void waitForReaders(int thread_index, std::atomic<int> &counter, 
                        RCU& rcu, bool &success) {
    auto sentinel = rcu.urcu_register_thread(thread_index);

    sentinel.urcu_synchronize();

    if (counter == RCU_THREADS - 1) {
        success = true;
    }
}


// should fail if wait thread does not properly wait for read threads
bool RCUTest() {
    test_string = "TESTING RCU MECHANISM:";
    std::cout<< test_string;
    RCU rcu(RCU_THREADS);

    std::thread threads[RCU_THREADS];

    std::atomic<int> readCounter;

    readCounter = 0;

    bool success = false;



    for (int i = 0; i < RCU_THREADS - 1; i++) {
        threads[i] =
            std::thread(readArea, i, std::ref(readCounter), std::ref(rcu));
    }

    threads[RCU_THREADS-1] =
    std::thread(waitForReaders, RCU_THREADS-1,
    std::ref(readCounter),
    std::ref(rcu),
    std::ref(success));


    for (int i = 0; i < RCU_THREADS; i++) {
         threads[i].join();
    }

    if (success) {
        okWithPadding(test_string);
    } else {
        std::cout << std::endl;
    }

    return success;
}




TEST_CASE("STACK TEST", "[stack]") {
    std::cout << "STACK TEST" << std::endl;
    REQUIRE(StackTest() == true);
}



TEST_CASE("Insert Test", "[insert]") {
    std::cout << "AVL TEST" << std::endl;
    REQUIRE_NOTHROW(avlCreation());
    REQUIRE(insertTest() == true);
}


TEST_CASE("Remove Test", "[remove]") {
    REQUIRE_NOTHROW(avlCreation());
    REQUIRE(removeTest() == true);
}


TEST_CASE("Thread Test", "[thread]") {
    std::cout << "THREAD TEST" << std::endl;
    REQUIRE_NOTHROW(threadTest());
}


TEST_CASE("RCU Test", "[rcu]") {
    std::cout << "RCU TEST" <<std::endl;
    REQUIRE(RCUTest());

    
}