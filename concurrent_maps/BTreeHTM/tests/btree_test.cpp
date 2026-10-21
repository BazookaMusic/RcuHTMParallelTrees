#include <iostream>
#include <array>
#include <vector>
#include <map>
#include <thread>
#include <cstdlib>
#include <random>
#include <chrono>
#include <atomic>
#include <algorithm>

#include "../include/btree.hpp"
#include "../../../include/catch2/catch.hpp"
#include "../../../include/TSXGuard.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#include "../../../include/test_bench.hpp"
#pragma GCC diagnostic pop

using namespace SafeTree;

// at least 4 threads, even on smaller machines,
// to get some contention
static const int THREADS = std::max(4u, std::thread::hardware_concurrency());

// small degree, lots of splits and merges
using BTree2 = BTree<int, int, 2>;
using BTree3 = BTree<int, int, 3>;
// default degree
using BTree4 = BTree<int, int, 4>;

using TestBenchType = TestBench<BTree4>;

TSX::SpinLock& lock = TestBenchType::global_lock;


static int value_of(int key) {
    return key * 7 + 1;
}

static std::vector<int> shuffled_range(int start, int end, unsigned int seed) {
    std::vector<int> keys;
    for (int i = start; i < end; i++) {
        keys.push_back(i);
    }

    std::mt19937 gen(seed);
    std::shuffle(keys.begin(), keys.end(), gen);

    return keys;
}

// leaf in which key would be inserted
template <class Tree>
static typename Tree::TreeNode* leaf_for(Tree& tree, int key) {
    auto curr = tree.getRoot();
    while (curr && !curr->isLeaf()) {
        curr = curr->getChild(curr->findIndex(key));
    }

    return curr;
}


TEST_CASE("BTree Init Test", "[init]") {
    BTree4 tree(nullptr, lock);

    REQUIRE(tree.size() == 0);
    REQUIRE(tree.height() == 0);
    REQUIRE(tree.isValid());
    REQUIRE_FALSE(tree.lookup(1).found);
    REQUIRE_FALSE(tree.remove(1, 0));
}


TEST_CASE("BTree Node Test", "[node]") {
    using Node = BTree4::TreeNode;

    REQUIRE(Node::maxKeys() == 7);
    REQUIRE(Node::minKeys() == 3);
    REQUIRE(Node::maxChildren() == 8);

    BTree4 tree(nullptr, lock);
    tree.insert(20, 1, 0);
    tree.insert(10, 1, 0);
    tree.insert(30, 1, 0);

    auto root = tree.getRoot();

    REQUIRE(root->isLeaf());
    REQUIRE(root->size() == 3);
    REQUIRE(root->getKey(0) == 10);
    REQUIRE(root->getKey(1) == 20);
    REQUIRE(root->getKey(2) == 30);

    REQUIRE(root->findIndex(5) == 0);
    REQUIRE(root->findIndex(10) == 0);
    REQUIRE(root->findIndex(15) == 1);
    REQUIRE(root->findIndex(30) == 2);
    REQUIRE(root->findIndex(35) == 3);

    REQUIRE(root->hasKey(20));
    REQUIRE_FALSE(root->hasKey(25));
    REQUIRE(root->nextChild(25) == 2);
    // leaf, so traversal is always done
    REQUIRE(root->traversalDone(25));
}


TEMPLATE_TEST_CASE("BTree Insert Test", "[insert]", BTree2, BTree3, BTree4) {
    std::cout << "SINGLE THREADED INSERT" << std::endl;
    TestType tree(nullptr, lock);

    const int N = 100000;
    // set by the sections inserting 0..N-1 with value_of values
    bool check_full = false;

    SECTION("small inserts") {
        REQUIRE(tree.insert(1, 2, 0));
        REQUIRE(tree.lookup(1).found);
        REQUIRE(tree.lookup(1).val == 2);

        REQUIRE(tree.insert(2, 1, 0));
        REQUIRE(tree.insert(-1, 1, 0));
        REQUIRE(tree.insert(4, 1, 0));
        REQUIRE(tree.insert(5, 1, 0));
        REQUIRE(tree.insert(6, 1, 0));

        for (int k : {-1, 1, 2, 4, 5, 6}) {
            REQUIRE(tree.lookup(k).found);
        }

        REQUIRE_FALSE(tree.lookup(10).found);
        REQUIRE_FALSE(tree.lookup(3).found);
        REQUIRE(tree.size() == 6);
        REQUIRE(tree.isValid());
    }

    SECTION("duplicate inserts are rejected") {
        REQUIRE(tree.insert(5, 1, 0));
        REQUIRE_FALSE(tree.insert(5, 2, 0));
        REQUIRE(tree.lookup(5).val == 1);

        for (int i = 0; i < 1000; i++) {
            tree.insert(i, value_of(i), 0);
        }

        for (int i = 0; i < 1000; i++) {
            REQUIRE_FALSE(tree.insert(i, -1, 0));
        }

        REQUIRE(tree.size() == 1000);
        REQUIRE(tree.lookup(5).val == 1);
        REQUIRE(tree.isValid());
    }

    SECTION("ascending inserts") {
        for (int i = 0; i < N; i++) {
            REQUIRE(tree.insert(i, value_of(i), 0));
        }
        check_full = true;
    }

    SECTION("descending inserts") {
        for (int i = N - 1; i >= 0; i--) {
            REQUIRE(tree.insert(i, value_of(i), 0));
        }
        check_full = true;
    }

    SECTION("random inserts") {
        for (int k : shuffled_range(0, N, 42)) {
            REQUIRE(tree.insert(k, value_of(k), 0));
        }
        check_full = true;
    }

    SECTION("balanced inserts") {
        TestBench<TestType>::binary_insert_map(0, N - 1, tree);

        // values are all 1
        for (int i = 0; i < N; i++) {
            REQUIRE(tree.lookup(i).found);
        }

        REQUIRE(tree.size() == N);
        REQUIRE(tree.isValid());
    }

    if (check_full) {
        REQUIRE(tree.isValid());

        for (int i = 0; i < N; i++) {
            auto res = tree.lookup(i);
            REQUIRE(res.found);
            REQUIRE(res.val == value_of(i));
        }

        REQUIRE_FALSE(tree.lookup(-1).found);
        REQUIRE_FALSE(tree.lookup(N).found);
        REQUIRE(tree.key_sum() == static_cast<std::size_t>(N) * (N - 1) / 2);
    }
}


TEMPLATE_TEST_CASE("BTree Remove Test", "[remove]", BTree2, BTree3, BTree4) {
    std::cout << "SINGLE THREADED REMOVE" << std::endl;
    TestType tree(nullptr, lock);

    SECTION("root remove") {
        tree.insert(1, 1, 0);

        REQUIRE(tree.remove(1, 0));
        REQUIRE_FALSE(tree.lookup(1).found);
        REQUIRE(tree.getRoot() == nullptr);
        REQUIRE_FALSE(tree.remove(1, 0));

        // tree can be reused
        REQUIRE(tree.insert(1, 3, 0));
        REQUIRE(tree.lookup(1).val == 3);
    }

    SECTION("missing keys") {
        for (int i = 0; i < 1000; i += 2) {
            tree.insert(i, value_of(i), 0);
        }

        for (int i = 1; i < 1000; i += 2) {
            REQUIRE_FALSE(tree.remove(i, 0));
        }

        REQUIRE_FALSE(tree.remove(-10, 0));
        REQUIRE_FALSE(tree.remove(5000, 0));
        REQUIRE(tree.size() == 500);
        REQUIRE(tree.isValid());
    }

    SECTION("remove keys of internal nodes") {
        const int N = 5000;
        for (int k : shuffled_range(0, N, 7)) {
            tree.insert(k, value_of(k), 0);
        }

        // always remove a key of the root,
        // which is internal while the tree has more than one level
        int removed = 0;
        while (tree.getRoot()) {
            auto root = tree.getRoot();
            const int k = root->getKey(root->size() / 2);

            REQUIRE(tree.remove(k, 0));
            REQUIRE_FALSE(tree.lookup(k).found);
            ++removed;

            REQUIRE(tree.size() == N - removed);

            if (removed % 50 == 0) {
                REQUIRE(tree.isValid());
            }
        }

        REQUIRE(removed == N);
    }

    SECTION("random removes") {
        const int N = 50000;
        for (int k : shuffled_range(0, N, 1)) {
            tree.insert(k, value_of(k), 0);
        }

        auto order = shuffled_range(0, N, 2);
        for (int i = 0; i < N; i++) {
            REQUIRE(tree.remove(order[i], 0));
            REQUIRE_FALSE(tree.lookup(order[i]).found);

            if (i % 1000 == 0) {
                REQUIRE(tree.isValid());
                REQUIRE(tree.size() == N - i - 1);

                // remaining keys are there with the right values
                for (int j = i + 1; j < N; j += 97) {
                    auto res = tree.lookup(order[j]);
                    REQUIRE(res.found);
                    REQUIRE(res.val == value_of(order[j]));
                }
            }
        }

        REQUIRE(tree.getRoot() == nullptr);
    }

    SECTION("batch remove of evens") {
        const int N = 100000;
        TestBench<TestType>::binary_insert_map(0, N - 1, tree);

        for (int i = 0; i < N; i += 2) {
            REQUIRE(tree.remove(i, 0));
        }

        for (int i = 0; i < N; i++) {
            REQUIRE(tree.lookup(i).found == (i % 2 == 1));
        }

        REQUIRE(tree.size() == N / 2);
        REQUIRE(tree.isValid());
    }
}


TEMPLATE_TEST_CASE("BTree Random Operations Against std::map", "[random]", BTree2, BTree3, BTree4) {
    std::cout << "SINGLE THREADED RANDOM OPS" << std::endl;
    TestType tree(nullptr, lock);
    std::map<int, int> reference;

    std::mt19937 gen(1234);
    std::uniform_int_distribution<int> key_dist(0, 2000);
    std::uniform_int_distribution<int> op_dist(0, 2);

    for (int i = 0; i < 200000; i++) {
        const int k = key_dist(gen);

        switch (op_dist(gen)) {
            case 0: {
                const bool inserted = reference.insert(std::make_pair(k, i)).second;
                REQUIRE(tree.insert(k, i, 0) == inserted);
                break;
            }
            case 1: {
                const bool removed = reference.erase(k) == 1;
                REQUIRE(tree.remove(k, 0) == removed);
                break;
            }
            default: {
                auto it = reference.find(k);
                auto res = tree.lookup(k);
                REQUIRE(res.found == (it != reference.end()));
                if (res.found) {
                    REQUIRE(res.val == it->second);
                }
            }
        }

        if (i % 997 == 0) {
            REQUIRE(tree.isValid());
            REQUIRE(tree.size() == static_cast<int>(reference.size()));
        }
    }

    REQUIRE(tree.isValid());
    REQUIRE(tree.size() == static_cast<int>(reference.size()));

    for (auto& entry : reference) {
        auto res = tree.lookup(entry.first);
        REQUIRE(res.found);
        REQUIRE(res.val == entry.second);
    }
}


TEST_CASE("BTree Root Is Not Replaced On Every Update", "[root]") {
    using Node = BTree4::TreeNode;
    BTree4 tree(nullptr, lock);

    // multiple levels, only even keys
    for (int k : shuffled_range(0, 20000, 3)) {
        tree.insert(2 * k, value_of(2 * k), 0);
    }

    REQUIRE(tree.height() >= 3);

    SECTION("insert into a non full leaf copies only the leaf") {
        int checked = 0;

        for (int k = 1; k < 40000; k += 2) {
            Node* leaf = leaf_for(tree, k);
            Node* root = tree.getRoot();

            const bool leaf_full = leaf->size() == Node::maxKeys();
            REQUIRE(leaf != root);

            REQUIRE(tree.insert(k, value_of(k), 0));

            if (!leaf_full) {
                ++checked;
                // root and leaf parent kept, only the leaf was replaced
                REQUIRE(tree.getRoot() == root);
                REQUIRE(leaf_for(tree, k) != leaf);
            }
        }

        REQUIRE(checked > 0);
        REQUIRE(tree.isValid());
    }

    SECTION("root replacements are rare") {
        int root_changes = 0;
        int inserts = 0;

        for (int k = 1; k < 40000; k += 2) {
            Node* root = tree.getRoot();
            tree.insert(k, value_of(k), 0);
            ++inserts;

            if (tree.getRoot() != root) {
                ++root_changes;
            }
        }

        std::cout << "Root replaced " << root_changes << " times in " << inserts << " inserts" << std::endl;
        REQUIRE(root_changes * 20 < inserts);
        REQUIRE(tree.isValid());
    }

    SECTION("remove from a leaf with spare keys copies only the leaf") {
        int checked = 0;

        for (int k = 0; k < 40000; k += 2) {
            Node* leaf = leaf_for(tree, k);
            Node* root = tree.getRoot();

            const bool in_leaf = leaf->hasKey(k);
            const bool spare_keys = leaf->size() > Node::minKeys();

            REQUIRE(tree.remove(k, 0));

            // a leaf root is of course replaced
            if (in_leaf && spare_keys && leaf != root) {
                ++checked;
                REQUIRE(tree.getRoot() == root);
            }
        }

        REQUIRE(checked > 0);
        REQUIRE(tree.getRoot() == nullptr);
    }
}


template <class Tree>
static void run_threads(int n_threads, Tree& tree, void (*func)(Tree&, int)) {
    std::vector<std::thread> threads;

    for (int i = 0; i < n_threads; i++) {
        threads.emplace_back(func, std::ref(tree), i);
    }

    for (auto& t : threads) {
        t.join();
    }
}

constexpr static int MT_KEYS_PER_THREAD = 50000;

// keys of threads are interleaved,
// so that threads conflict on the same leaves
template <class Tree>
static void interleaved_insert(Tree& tree, int t_id) {
    for (int j = 0; j < MT_KEYS_PER_THREAD; j++) {
        const int k = j * THREADS + t_id;
        tree.insert(k, value_of(k), t_id);
    }
}

template <class Tree>
static void interleaved_even_remove(Tree& tree, int t_id) {
    for (int j = 0; j < MT_KEYS_PER_THREAD; j++) {
        const int k = j * THREADS + t_id;
        if (k % 2 == 0) {
            tree.remove(k, t_id);
        }
    }
}


TEMPLATE_TEST_CASE("BTree Multithreaded Insert Test", "[mt_insert]", BTree2, BTree4) {
    std::cout << "MULTITHREADED INSERT" << std::endl;
    const int N = THREADS * MT_KEYS_PER_THREAD;

    for (int round = 0; round < 10; round++) {
        TestType tree(nullptr, lock);

        run_threads<TestType>(THREADS, tree, interleaved_insert<TestType>);

        for (int i = 0; i < N; i++) {
            auto res = tree.lookup(i);
            if (!res.found || res.val != value_of(i)) {
                std::cout << "MISSING " << i << std::endl;
                REQUIRE(res.found);
                REQUIRE(res.val == value_of(i));
            }
        }

        REQUIRE(tree.size() == N);
        REQUIRE(tree.isValid());
    }
}


TEMPLATE_TEST_CASE("BTree Multithreaded Remove Test", "[mt_remove]", BTree2, BTree4) {
    std::cout << "MULTITHREADED REMOVE" << std::endl;
    const int N = THREADS * MT_KEYS_PER_THREAD;

    for (int round = 0; round < 10; round++) {
        TestType tree(nullptr, lock);
        TestBench<TestType>::binary_insert_map(0, N - 1, tree);

        run_threads<TestType>(THREADS, tree, interleaved_even_remove<TestType>);

        for (int i = 0; i < N; i++) {
            if (tree.lookup(i).found != (i % 2 == 1)) {
                std::cout << "WRONG " << i << std::endl;
                REQUIRE(tree.lookup(i).found == (i % 2 == 1));
            }
        }

        REQUIRE(tree.size() == N / 2);
        REQUIRE(tree.isValid());
    }
}


TEMPLATE_TEST_CASE("BTree Multithreaded Mixed Operations Test", "[mt_mixed]", BTree2, BTree3, BTree4) {
    std::cout << "MULTITHREADED MIXED OPERATIONS" << std::endl;

    // small range, high contention
    for (const int range : {64, 1024, 20000}) {
        TestType tree(nullptr, lock);

        std::vector<bool> initially_present(range, false);
        for (int k = 0; k < range; k += 2) {
            tree.insert(k, value_of(k), 0);
            initially_present[k] = true;
        }

        // successful inserts - successful removes per key and thread
        std::vector<std::vector<int>> net(THREADS, std::vector<int>(range, 0));
        std::vector<std::thread> threads;

        for (int t = 0; t < THREADS; t++) {
            threads.emplace_back([&tree, &net, range, t]() {
                std::mt19937 gen(100 + t);
                std::uniform_int_distribution<int> key_dist(0, range - 1);
                std::uniform_int_distribution<int> op_dist(0, 2);

                for (int i = 0; i < 100000; i++) {
                    const int k = key_dist(gen);
                    switch (op_dist(gen)) {
                        case 0:
                            if (tree.insert(k, value_of(k), t)) {
                                ++net[t][k];
                            }
                            break;
                        case 1:
                            if (tree.remove(k, t)) {
                                --net[t][k];
                            }
                            break;
                        default:
                            tree.lookup(k);
                    }
                }
            });
        }

        for (auto& th : threads) {
            th.join();
        }

        int expected_size = 0;
        std::size_t expected_sum = 0;

        for (int k = 0; k < range; k++) {
            int present = initially_present[k] ? 1 : 0;
            for (int t = 0; t < THREADS; t++) {
                present += net[t][k];
            }

            // a key can't be inserted twice or removed twice in a row
            REQUIRE((present == 0 || present == 1));

            auto res = tree.lookup(k);
            if (res.found != (present == 1)) {
                std::cout << "WRONG " << k << std::endl;
                REQUIRE(res.found == (present == 1));
            }

            if (res.found) {
                REQUIRE(res.val == value_of(k));
                ++expected_size;
                expected_sum += k;
            }
        }

        REQUIRE(tree.size() == expected_size);
        REQUIRE(tree.key_sum() == expected_sum);
        REQUIRE(tree.isValid());
    }
}


TEMPLATE_TEST_CASE("BTree Lookups During Updates Test", "[mt_lookup]", BTree2, BTree4) {
    std::cout << "MULTITHREADED LOOKUPS DURING UPDATES" << std::endl;

    const int range = 20000;
    TestType tree(nullptr, lock);

    // even keys are never touched
    for (int k : shuffled_range(0, range, 5)) {
        if (k % 2 == 0) {
            tree.insert(k, value_of(k), 0);
        }
    }

    std::atomic<bool> done(false);
    std::atomic<long long> missing(0);
    std::atomic<long long> lookups(0);

    const int writers = THREADS / 2;
    const int readers = THREADS - writers;

    std::vector<std::thread> threads;

    for (int t = 0; t < writers; t++) {
        threads.emplace_back([&tree, t]() {
            std::mt19937 gen(t);
            std::uniform_int_distribution<int> key_dist(0, range / 2 - 1);

            for (int i = 0; i < 200000; i++) {
                const int k = 2 * key_dist(gen) + 1;
                if (i % 2) {
                    tree.insert(k, value_of(k), t);
                } else {
                    tree.remove(k, t);
                }
            }
        });
    }

    for (int t = 0; t < readers; t++) {
        threads.emplace_back([&tree, &done, &missing, &lookups, readers, t]() {
            long long local_missing = 0;
            long long local_lookups = 0;

            for (int k = 2 * t; !done.load(); k = (k + 2 * readers) % range) {
                auto res = tree.lookup(k);
                if (!res.found || res.val != value_of(k)) {
                    ++local_missing;
                }
                ++local_lookups;
            }

            missing += local_missing;
            lookups += local_lookups;
        });
    }

    for (int t = 0; t < writers; t++) {
        threads[t].join();
    }

    done = true;

    for (int t = writers; t < THREADS; t++) {
        threads[t].join();
    }

    std::cout << "Lookups during updates: " << lookups.load() << std::endl;

    REQUIRE(lookups.load() > 0);
    REQUIRE(missing.load() == 0);
    REQUIRE(tree.isValid());

    for (int k = 0; k < range; k += 2) {
        REQUIRE(tree.lookup(k).found);
    }
}


// hidden, run with: ./btree_test "[tp]"
TEST_CASE("THROUGHPUT TESTS", "[.][tp]") {
    const int OPERATION_MULTIPLIERS[] = {1000, 10000, 1000000};

    std::vector<int> threads_to_use;
    for (int t : {1, 2, 4, 7, 14, 20, 28}) {
        if (t <= TestBenchType::THREADS) {
            threads_to_use.push_back(t);
        }
    }

    for (int multiplier : OPERATION_MULTIPLIERS) {
        std::cout << "Start of tests for tree size: " << multiplier << std::endl;
        const std::size_t RANGE_OF_KEYS = 2 * multiplier;

        // RANDOM OPS
        TestBenchType::experiment exp1(33, 33, 34);
        TestBenchType::test(exp1, TestBenchType::THREADS, RANGE_OF_KEYS, threads_to_use);

        // 10 - 10 - 80
        TestBenchType::experiment exp2(10, 10, 80);
        TestBenchType::test(exp2, TestBenchType::THREADS, RANGE_OF_KEYS, threads_to_use);

        // 100% LOOKUPS
        TestBenchType::experiment exp3(0, 0, 100);
        TestBenchType::test(exp3, TestBenchType::THREADS, RANGE_OF_KEYS, threads_to_use);

        // 50-50 UPDATES
        TestBenchType::experiment exp4(50, 50, 0);
        TestBenchType::test(exp4, TestBenchType::THREADS, RANGE_OF_KEYS, threads_to_use);

        // 25-25 UPDATES, 50 LOOKUPS
        TestBenchType::experiment exp5(25, 25, 50);
        TestBenchType::test(exp5, TestBenchType::THREADS, RANGE_OF_KEYS, threads_to_use);
    }
}
