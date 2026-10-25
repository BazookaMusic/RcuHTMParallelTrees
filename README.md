# RCU HTM Parallel Trees

A C++ library to make designing thread safe trees as easy as copying
the Geeks for Geeks guides ;)

SafeTree applies the RCU-HTM technique to any tree data structure. You write
an operation as if it were sequential code, using the library's wrappers to
read and modify nodes, and the library makes it thread safe:

- **Lookups** traverse the tree directly. They take no locks and never retry.
- **Updates** never modify a node that is reachable from the root. They build
  private copies of the nodes they change, then a short hardware
  transaction (Intel TSX) validates that nothing they read has changed and
  swaps a single child pointer to publish the copies.

The library and its evaluation are described in the thesis
[SafeTree_Thesis_English.pdf](SafeTree_Thesis_English.pdf). The thesis shows the
earlier `TM_SAFE_OPERATION_START`/`END` macros; operations are now written with
`SafeTree::safe_operation`, as shown below.

## Requirements

- An x86-64 Intel CPU with RTM (TSX-NI) enabled. Check with
  `grep -w rtm /proc/cpuinfo`. Many CPUs ship with TSX disabled by microcode.
  Without RTM, the first transaction crashes with an illegal instruction.
- A C++11 compiler. The Makefiles use `clang++`.
- Linux. The benchmarks pin threads to cores with `pthread_setaffinity_np`.

The library is header only: include `include/SafeTree.hpp`.

## Repository layout

| Path | Contents |
| --- | --- |
| `include/SafeTree.hpp` | The library: `SafeNode`, `ConnPoint`, `PathTracker`, `safe_operation`, search helpers |
| `include/TSXGuard.hpp`, `include/rtm.h` | HTM transactions, the fallback lock and transaction statistics |
| `include/test_bench.hpp` | Throughput benchmark used by the map tests |
| `include/catch2/` | Bundled Catch2 test framework |
| `concurrent_maps/BstHTM` | Unbalanced binary search tree (search tree) |
| `concurrent_maps/AVLHTM` | AVL tree (search tree) |
| `concurrent_maps/AVLHTMGeneric` | The same AVL tree written with `PathTracker` (general tree) |
| `concurrent_maps/BTreeHTM` | B-Tree with a configurable minimum degree (general tree) |
| `concurrent_lists/StackHTM`, `concurrent_lists/QueueHTM` | A stack and a queue, built as trees with one child per node |

Each structure has its implementation in `include/` and Catch2 tests in `tests/`.

## How an update works

1. **Find phase.** Traverse the tree and record the path to the place where the
   modification starts. The node above that place is the *connection point*,
   the node whose child pointer will be swapped.
2. **Copy phase.** Create a `ConnPoint` from the recorded path. Its root is a
   `SafeNode`, a wrapper around a tree node, and every node you reach through
   it is wrapped as well. Wrapped nodes are copied the first time you modify
   them, so the original tree is never touched. To change nodes higher up the
   tree (for example when rebalancing), `pop_path()` moves the connection
   point one level up.
3. **Commit.** When the operation finishes, a hardware transaction checks that
   the connection point is still in the tree and that no node you read through
   a `SafeNode` has had its children changed. If both hold, it swaps the
   connection point's child pointer to the root of your copies. If validation
   fails, the operation runs again. After a number of failed transactions
   (`retries`), it runs under a global fallback lock.

`SafeTree::safe_operation(retries, op)` runs the operation `op` (a lambda) through
this loop. It returns `op`'s result only after the commit has succeeded, so `op`
can `return` at any point.

## Writing a data structure

### Nodes

Every node type needs:

```cpp
Node* getChild(int i);
void setChild(int i, Node* n);
Node** getChildPointer(int i);           // where child i is stored
static constexpr int maxChildren();      // maximum number of children
```

It also needs a copy constructor, because the library copies nodes before
modifying them.

Search trees (`TREE_TYPE SEARCH_TREE`, the default) also need the following,
which lets the library find nodes and validate the connection point by key:

```cpp
using KeyType = ...;
bool hasKey(KeyType key);                 // the node contains key
bool traversalDone(KeyType key);          // stop the search at this node
int nextChild(KeyType key);               // child to follow when looking for key
int nextChild(const Node* target);        // child to follow when looking for target
```

### Example: a binary search tree

```cpp
#include "include/SafeTree.hpp"

using namespace SafeTree;

class Node {
    public:
        using KeyType = int;

        int key;
        int value;
        Node* children[2];

        Node(int key, int value): key(key), value(value), children{nullptr, nullptr} {}

        Node* getChild(int i) { return children[i]; }
        void setChild(int i, Node* n) { children[i] = n; }
        Node** getChildPointer(int i) { return &children[i]; }
        static constexpr int maxChildren() { return 2; }

        bool hasKey(int k) { return key == k; }
        bool traversalDone(int k) const { return key == k; }
        int nextChild(int k) const { return k < key ? 0 : 1; }
        int nextChild(const Node* target) const { return target->key < key ? 0 : 1; }
};

class Tree {
        Node* root = nullptr;

    public:
        bool insert(int key, int value) {
            return safe_operation(30, [&]() -> bool {
                // find the parent of the position of key
                auto data = find_conn_point<Node>(key, &root);

                if (data.found()) {
                    return false;
                }

                ConnPoint<Node> conn(data);

                // the new node becomes the root of the tree of copies,
                // connected below the connection point
                conn.setRoot(conn.create_safe(new Node(key, value)));

                return true;
            });
        }

        bool update(int key, int value) {
            return safe_operation(30, [&]() -> bool {
                auto data = find_conn_point<Node>(key, &root);

                if (!data.found()) {
                    return false;
                }

                ConnPoint<Node> conn(data);

                // getRoot() is the node with the key, rwRef() gives a
                // private copy which is safe to modify
                conn.getRoot()->rwRef()->value = value;

                return true;
            });
        }

        bool lookup(int key) {
            return find<Node>(root, key) != nullptr;
        }
};
```

`concurrent_maps/BstHTM` and `concurrent_maps/AVLHTM` are complete versions of
this, with removal and AVL rebalancing (`pop_path()` and rotations on `SafeNode`s).

### The main operations

| Call | Purpose |
| --- | --- |
| `SafeNode::getChild(i)` | Wrap child `i` for modification, copying the parent |
| `SafeNode::setChild(i, n)` | Link a `SafeNode` (or `nullptr`) as child `i` |
| `SafeNode::rwRef()` | Private copy of the node, safe to modify |
| `SafeNode::peekChild(i)`, `peekOriginal()` | Read without copying |
| `ConnPoint::getRoot()`, `setRoot(n)` | Read or replace the root of the tree of copies |
| `ConnPoint::pop_path()` | Move the connection point one level up, returning the new root |
| `ConnPoint::create_safe(node)` | Wrap a newly created node |
| `ConnPoint::wrap_safe(node)` | Wrap a node of the original tree so it is validated |

### General trees

Structures where the path cannot be derived from a key (B-Trees, lists, or
trees with your own traversal logic) set `#define TREE_TYPE GENERAL_TREE`
before including `SafeTree.hpp`. They record the path with a `PathTracker`
instead of `find_conn_point`:

```cpp
PathTracker<Node> tracker(&root);

while (Node* curr = tracker.getNode()) {
    // ...decide where to go...
    tracker.moveToChild(next);  // or tracker.moveUp(n)
}

ConnPoint<Node> conn(tracker.connectHere());
```

`connectHere()` makes the node above the tracker's current position the
connection point. See `concurrent_maps/BTreeHTM` and
`concurrent_maps/AVLHTMGeneric`.

`TREE_TYPE` is a macro, so all structures in one translation unit share it.

## Configuration

These are set with `#define`s before including `SafeTree.hpp` (or with `-D`):

| Macro | Default | Effect |
| --- | --- | --- |
| `TREE_TYPE` | `SEARCH_TREE` | `SEARCH_TREE` or `GENERAL_TREE`, see above |
| `RCU_HTM_MAX_THREADS` | `100` | Maximum number of node pools (one per thread and tree) |
| `TM_EARLY_ABORT` | off | Retry before the transaction when `pop_path()` sees a conflict |
| `TM_EARLY_ABORT_ON_COPY` | off | Also retry early when a copied node has changed |

Fixed limits are set in `SafeTree.hpp`:

- paths of at most `PATH_MAX_LEN` (10000) nodes;
- at most 100 `SafeNode`s per operation;
- a node pool of 50 million nodes per thread. This is virtual memory, which
  Linux only commits as it is used.

## Building and running the tests

Each structure has a Makefile in its `tests` directory:

```sh
cd concurrent_maps/BTreeHTM/tests
make tests        # build and run the correctness tests
make throughput   # benchmarks: 5 seconds per thread count, workload and tree size
make clean
```

`make throughput` is available for the map structures. The benchmarks use up to
28 threads and skip thread counts above the machine's hardware threads.

## Limitations

- **No memory reclamation.** Replaced and removed nodes are never freed. Safe
  reclamation is the main item of future work in the thesis.
- **Shared node pools.** Copies are allocated from per-thread pools that are
  shared by all trees with the same node type. Destroying a tree resets these
  pools, so only one tree per node type should be alive at a time.
- **Intel TSX only.** The library has no software fallback for CPUs without
  RTM.
