#include <thread>
#include <vector>
#include "../include/queue.hpp"
#include "../../../include/catch2/catch.hpp"

constexpr int N_ITEMS = 1000;
constexpr int THREADS = 6;


TEST_CASE("Queue Init Test","[init]") {
    Queue<int> queue;
    (void)queue;
}

TEST_CASE("Queue Insert Test","[init]") {
    Queue<int> queue;

    queue.enqueue(1);

    REQUIRE(queue.next()->getItem() == 1);

    queue.enqueue(2);

    REQUIRE(queue.next()->getItem() == 1);

    queue.enqueue(3);

    REQUIRE(queue.next()->getItem() == 1);

    for (int i = 0; i < N_ITEMS; ++i) {
        queue.enqueue(i);
        REQUIRE(queue.next()->getItem() == 1);
    }
}

TEST_CASE("Queue Pop Test","[init]") {
    Queue<int> queue;

    for (int i = 0; i < N_ITEMS; ++i) {
        queue.enqueue(i);
        REQUIRE(queue.next()->getItem() == 0);
    }

     for (int i = N_ITEMS; i > 0; --i) {
        REQUIRE(queue.next()->getItem() == N_ITEMS - i);
        queue.dequeue();
     }
}

void insert(int i, Queue<int>& queue) {
    const int start = i*N_ITEMS;
    for (int j = start; j < start + N_ITEMS; j++) {
        queue.enqueue(j);
    }
}

TEST_CASE("Queue Multithreaded Push Test","[mt_enqueue]") {
    std::cout << "MULTITHREADED PUSH" << std::endl;

    bool exists[N_ITEMS*THREADS];

    for (int j = 0; j < 1; j++) { 
            Queue<int> queue;
            std::thread threads[THREADS];

            for (int i = 0; i < N_ITEMS*THREADS;i++) {
                exists[i] = false;
            }

            for (int i = 0; i < THREADS; i++) {
                threads[i] = std::thread(insert, i, std::ref(queue));
            }

            for (int i = 0; i < THREADS; i++) {
                threads[i].join();
            }

            auto item = queue.next();
            while ((item = queue.next()))
            {
                exists[item->getItem()] = true;
                queue.dequeue();
            }

            for (int i = 0; i < N_ITEMS*THREADS; i++) {
                REQUIRE(exists[i]);
            }
    }
}

void dequeue(std::vector<int>* popped, Queue<int>& queue) {
    const QueueItem<int>* item;
    while ((item = queue.dequeue())) {
        popped->push_back(item->getItem());
    }
}

TEST_CASE("Queue Multithreaded Pop Test","[mt_enqueue]") {
    std::cout << "MULTITHREADED POP" << std::endl;

    for (int j = 0; j < 1; j++) {
            Queue<int> queue;
            std::thread threads[THREADS];
            std::vector<int> popped[THREADS];

            for (int i = 0; i < N_ITEMS*THREADS;i++) {
                queue.enqueue(i);
            }

            for (int i = 0; i < THREADS; i++) {
                threads[i] = std::thread(dequeue, &popped[i], std::ref(queue));
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






