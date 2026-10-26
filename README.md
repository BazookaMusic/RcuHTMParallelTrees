# RCU HTM Parallel Trees

A C++ library to make designing thread safe trees as easy as copying
the Geeks for Geeks guides ;)

SafeTree generalises RCU-HTM [[1]](#references), a technique by Siakavaras et
al. for building fast concurrent binary search trees. RCU-HTM combines
Read-Copy-Update [[2]](#references) with hardware transactional memory, so
readers never synchronise and writers only use a short transaction to
publish their changes. SafeTree turns the technique into a library that works
for any tree data structure. You write an operation as if it were sequential
code, using the library's wrappers to read and modify nodes, and the library
makes it thread safe:

- **Lookups** traverse the tree directly. They take no locks and never retry.
- **Updates** never modify a node that is reachable from the root. They build
  private copies of the nodes they change, then a short hardware
  transaction (Intel TSX) validates that nothing they read has changed and
  swaps a single child pointer to publish the copies. Without TSX, the same
  commit can be done lock-free with LLX/SCX [[6]](#references), see
  [Committing without TSX](#committing-without-tsx-llxscx).

Related work includes a performance study of concurrent red-black trees on
HTM [[3]](#references) and a general technique for non-blocking trees
[[4]](#references).

The library and its evaluation are described in the thesis
[SafeTree_Thesis_English.pdf](SafeTree_Thesis_English.pdf) [[5]](#references).
The thesis shows the earlier `TM_SAFE_OPERATION_START`/`END` macros; operations
are now written with `SafeTree::safe_operation`, as shown below.

## Requirements

- An x86-64 CPU. The default TSX commits need an Intel CPU with RTM (TSX-NI)
  enabled: check with `grep -w rtm /proc/cpuinfo`. Many CPUs ship with TSX
  disabled by microcode, and without RTM the first transaction crashes with an
  illegal instruction. LLX/SCX commits work on any x86-64 CPU.
- A C++11 compiler. The Makefiles use `clang++`.
- Linux. The benchmarks pin threads to cores with `pthread_setaffinity_np`.

The library is header only: include `include/SafeTree.hpp`.

## Repository layout

| Path | Contents |
| --- | --- |
| `include/SafeTree.hpp` | The library: `SafeNode`, `ConnPoint`, `PathTracker`, `safe_operation`, search helpers |
| `include/TSXGuard.hpp`, `include/rtm.h` | HTM transactions, the fallback lock and transaction statistics |
| `include/LLXSCX.hpp` | The LLX/SCX primitives used for commits without TSX |
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
   (`retries`), it runs under a global fallback lock. With LLX/SCX commits,
   this check and swap is a single SCX instead.

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
modifying them, and it has to inherit publicly from `SafeTree::NodeBase`. That
class is empty for TSX commits and holds the LLX/SCX fields otherwise.

### Reading child pointers

Once a node is reachable from the root, the only field that changes is a child
pointer, when an update swaps it to connect its copies. The library publishes
that pointer with a release store. Code which reads child or root pointers
while updates may be running has to use an acquire load, so that it also sees
the contents of the nodes they point to:

```cpp
Node* curr = read_pointer(&root);   // the root pointer
curr = read_child(curr, i);         // child i of a node in the tree
```

This applies to your lookups and traversals, and to the copy constructor,
which copies nodes that are in the tree:

```cpp
Node(const Node& other): key(other.key), value(other.value) {
    children[0] = read_pointer(&other.children[0]);
    children[1] = read_pointer(&other.children[1]);
}
```

On x86 these are ordinary loads and stores. On ARM they add the ordering that
x86 gives for free.

Search trees (`TREE_TYPE SEARCH_TREE`, the default) also need the following,
which lets the library find nodes and validate the connection point by key:

```cpp
using KeyType = ...;
bool hasKey(KeyType key);                 // the node contains key
bool traversalDone(KeyType key);          // stop the search at this node
int nextChild(KeyType key);               // child to follow when looking for key
int nextChild(const Node* target);        // child to follow when looking for target
```

### Operations

An operation is a lambda passed to `safe_operation`. A search tree insert finds
the connection point by key and connects the new node below it (from
[`BST::insert_impl`](concurrent_maps/BstHTM/include/bst.hpp)):

```cpp
bool insert(int k, ValueType val) {
    return safe_operation(30, [&]() -> bool {
        // find the parent of the position of k
        auto conn_point_snapshot = find_conn_point<TreeNode>(k, &root);

        if (conn_point_snapshot.found()) {
            return false;
        }

        ConnPoint<TreeNode> conn(conn_point_snapshot);

        // the new node becomes the root of the tree of copies,
        // connected below the connection point
        conn.setRoot(conn.create_safe(ConnPoint<TreeNode>::create_new_node(k, val, nullptr, nullptr)));

        return true;
    });
}
```

To change a node's contents, modify the private copy returned by `rwRef()`:

```cpp
ConnPoint<TreeNode> conn(conn_point_snapshot);

// the root of the tree of copies is the node with the key
conn.getRoot()->rwRef()->setValue(new_value);
```

Rebalancing walks back up the path with `pop_path()`. Each call makes the
parent of the current tree of copies its new root, and stops once nothing else
changes (from [`AVLTree::insert_impl`](concurrent_maps/AVLHTM/include/avl.hpp)):

```cpp
for (SafeNode<TreeNode>* n = conn.pop_path(); n != nullptr; n = conn.pop_path()) {
    auto n_values = n->rwRef();
    int height_old = n_values->height;

    // rotations work on SafeNodes exactly as on plain nodes
    n = rebalance_ins(n, k, rotation_happened);
    conn.setRoot(n);

    if (height_old == n_values->height && !rotation_happened) {
        break;
    }
}
```

The complete implementations are good starting points:

| Structure | Implementation | Shows |
| --- | --- | --- |
| Binary search tree | [`bst.hpp`](concurrent_maps/BstHTM/include/bst.hpp) | Insert and remove with `find_conn_point` |
| AVL tree | [`avl.hpp`](concurrent_maps/AVLHTM/include/avl.hpp) | Rotations and rebalancing with `pop_path()` |
| AVL tree, general | [`avl.hpp`](concurrent_maps/AVLHTMGeneric/include/avl.hpp) | The same tree with a `PathTracker` |
| B-Tree | [`btree.hpp`](concurrent_maps/BTreeHTM/include/btree.hpp) | Nodes with many children, splits and merges propagating upwards |
| Stack, queue | [`stack.hpp`](concurrent_lists/StackHTM/include/stack.hpp), [`queue.hpp`](concurrent_lists/QueueHTM/include/queue.hpp) | Operations which return a value |

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
| `COMMIT_TYPE` | `HTM_COMMIT` | `HTM_COMMIT` (TSX) or `LLX_SCX_COMMIT`, see below |
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
make tests-llx    # the same tests with LLX/SCX commits
make throughput   # benchmarks: 5 seconds per thread count, workload and tree size
make clean
```

`make throughput` is available for the map structures. The benchmarks use up to
28 threads and skip thread counts above the machine's hardware threads.

## Committing without TSX: LLX/SCX

By default an operation validates its tree of copies and connects it inside a
hardware transaction (Intel TSX), falling back to a global lock. Compiling with

    -DCOMMIT_TYPE=LLX_SCX_COMMIT

commits with the lock-free LLX/SCX primitives of Brown, Ellen and Ruppert
[[6]](#references) instead (`include/LLXSCX.hpp`), so no TSX support is needed:

- the nodes the operation reads are snapshotted with LLX,
- the commit is one SCX which changes the pointer at the connection point only
  if none of those nodes changed, and finalizes the nodes that were replaced by
  copies, so operations that still use them fail and retry.

Every node which is removed from the tree has to be copied (`getChild`,
`rwRef`) or deleted (`subtreeDelete`, `clipTree`) through its `SafeNode`, so
that it gets finalized.

## Limitations

- **No memory reclamation.** Replaced and removed nodes are never freed. Safe
  reclamation is the main item of future work in the thesis.
- **Shared node pools.** Copies are allocated from per-thread pools that are
  shared by all trees with the same node type. Destroying a tree resets these
  pools, so only one tree per node type should be alive at a time.
- **x86-64 only.** LLX/SCX commits remove the need for TSX, but the library
  still includes x86 intrinsics.

## References

1. D. Siakavaras et al. "RCU-HTM: Combining RCU
   with HTM to Implement Highly Efficient Concurrent Binary Search Trees." In
   *2017 26th International Conference on Parallel Architectures and
   Compilation Techniques (PACT)*, 2017, pp. 1–13.
   [doi:10.1109/PACT.2017.17](https://doi.org/10.1109/PACT.2017.17)
2. M. Desnoyers et al. "User-Level Implementations of Read-Copy Update."
   *IEEE Transactions on Parallel and Distributed Systems* 23, 2012,
   pp. 375–382.
   [doi:10.1109/TPDS.2011.159](https://doi.org/10.1109/TPDS.2011.159)
3. D. Siakavaras et al. "Performance Analysis of Concurrent Red-Black Trees on
   HTM Platforms." 2015.
4. T. Brown, F. Ellen and E. Ruppert. "A General Technique for Non-blocking
   Trees." 2017. [arXiv:1712.06687](https://arxiv.org/abs/1712.06687)
5. S. P. Dragonas. *SafeTree – A System for the Automatic Parallelization of
   Tree Data Structures.* Diploma thesis, National Technical University of
   Athens, 2020.
   [SafeTree_Thesis_English.pdf](SafeTree_Thesis_English.pdf)
6. T. Brown, F. Ellen and E. Ruppert. "Pragmatic Primitives for Non-blocking
   Data Structures." In *Proceedings of the 2013 ACM Symposium on Principles
   of Distributed Computing (PODC)*, 2013.
