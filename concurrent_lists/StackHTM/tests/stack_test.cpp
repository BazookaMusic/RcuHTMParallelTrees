#include <thread>
#include <vector>
#include "../include/stack.hpp"
#include "../../../include/catch2/catch.hpp"

constexpr int N_ITEMS = 10000;
constexpr int THREADS = 6;


TEST_CASE("Stack Init Test","[init]") {
    Stack<int> stack;
    (void)stack;
}

TEST_CASE("Stack Insert Test","[init]") {
    Stack<int> stack;

    stack.push(1);

    REQUIRE(stack.top()->getItem() == 1);

    stack.push(2);

    REQUIRE(stack.top()->getItem() == 2);

    stack.push(3);

    REQUIRE(stack.top()->getItem() == 3);

    for (int i = 0; i < N_ITEMS; ++i) {
        stack.push(i);
        REQUIRE(stack.top()->getItem() == i);
    }
}

TEST_CASE("Stack Pop Test","[init]") {
    Stack<int> stack;

    for (int i = 0; i < N_ITEMS; ++i) {
        stack.push(i);
        REQUIRE(stack.top()->getItem() == i);
    }

     for (int i = N_ITEMS; i > 0; --i) {
        REQUIRE(stack.top()->getItem() == i - 1);
        stack.pop();
     }
}

void insert(int i, Stack<int>& stack) {
    const int start = i*N_ITEMS;
    for (int j = start; j < start + N_ITEMS; j++) {
        stack.push(j);
    }
}

TEST_CASE("Stack Multithreaded Push Test","[mt_push]") {
    std::cout << "MULTITHREADED PUSH" << std::endl;

    bool exists[N_ITEMS*THREADS];

    for (int j = 0; j < 1; j++) { 
            Stack<int> stack;
            std::thread threads[THREADS];

            for (int i = 0; i < N_ITEMS*THREADS;i++) {
                exists[i] = false;
            }

            for (int i = 0; i < THREADS; i++) {
                threads[i] = std::thread(insert, i, std::ref(stack));
            }

            for (int i = 0; i < THREADS; i++) {
                threads[i].join();
            }

            auto item = stack.top();
            while ((item = stack.top()))
            {
                exists[item->getItem()] = true;
                stack.pop();
            }

            for (int i = 0; i < N_ITEMS*THREADS; i++) {
                REQUIRE(exists[i]);
            }
    }
}

void pop(std::vector<int>* popped, Stack<int>& stack) {
    const StackItem<int>* item;
    while ((item = stack.pop())) {
        popped->push_back(item->getItem());
    }
}

TEST_CASE("Stack Multithreaded Pop Test","[mt_push]") {
    std::cout << "MULTITHREADED POP" << std::endl;

    for (int j = 0; j < 1; j++) {
            Stack<int> stack;
            std::thread threads[THREADS];
            std::vector<int> popped[THREADS];

            for (int i = 0; i < N_ITEMS*THREADS;i++) {
                stack.push(i);
            }

            for (int i = 0; i < THREADS; i++) {
                threads[i] = std::thread(pop, &popped[i], std::ref(stack));
            }

            for (int i = 0; i < THREADS; i++) {
                threads[i].join();
            }

            // every item should have been popped exactly once
            std::vector<int> times_popped(N_ITEMS*THREADS, 0);
            for (int i = 0; i < THREADS; i++) {
                for (auto n : popped[i]) {
                    ++times_popped[n];
                }
            }

            for (int i = 0; i < N_ITEMS*THREADS; i++) {
                if (times_popped[i] != 1) {
                    std::cerr << i << " popped " << times_popped[i] << " times" << std::endl;
                }
                REQUIRE(times_popped[i] == 1);
            }
    }
}






