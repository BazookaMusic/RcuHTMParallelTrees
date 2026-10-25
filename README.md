# RCU HTM Parallel Trees

A C++ library to make designing thread safe trees as easy as copying
the Geeks for Geeks guides ;)

## Committing without TSX: LLX/SCX

By default an operation validates its tree of copies and connects it inside a
hardware transaction (Intel TSX), falling back to a global lock. Compiling with

    -DCOMMIT_TYPE=LLX_SCX_COMMIT

commits with the lock-free LLX/SCX primitives of Brown, Ellen and Ruppert
instead (`include/LLXSCX.hpp`), so no TSX support is needed:

- the nodes the operation reads are snapshotted with LLX,
- the commit is one SCX which changes the pointer at the connection point only
  if none of those nodes changed, and finalizes the nodes that were replaced by
  copies, so operations that still use them fail and retry.

For this the nodes have to inherit from `SafeTree::NodeBase` (an empty struct
for TSX commits, so nodes can always inherit from it). Every test Makefile has a
`tests-llx` target which runs the tests with LLX/SCX commits.
