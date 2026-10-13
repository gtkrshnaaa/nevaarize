# ISSUE-3: feat(algorithms): implement pairingHeapPriorityQueueOps in richcodesamples

## Objective
Implement self-contained Pairing Heap priority queue supporting two-pass multipass pairing meld, insert, decrease-key, and delete-min in `examples/richcodesample/pairingHeapPriorityQueueOps.nva` with deterministic assertions and runtime benchmark.

## Target Files
- `examples/richcodesample/pairingHeapPriorityQueueOps.nva`

## Architecture & Requirements
- Arena-allocated mutable Pairing Heap priority queue using multi-way tree first-child / next-sibling representation
- Flat parallel arrays for cache locality and zero GC pressure: `hKey`, `hVal`, `hChild`, `hSibling`, `hPrev`
- Core operations:
  - `createPairingHeapArena()`: initializes node storage
  - `allocPairingNode(arena, key, val)`: allocates new node in arena
  - `findMin(arena, root)`: retrieves minimum key and value in O(1)
  - `meldPairingHeaps(arena, h1, h2)`: compares root keys, links larger as leftmost child of smaller in O(1)
  - `insertPairingNode(arena, root, key, val)`: allocates and melds new node into root
  - `twoPassPairing(arena, treeList)`: standard two-pass pairing meld (Pass 1 left-to-right pairs, Pass 2 right-to-left accumulation)
  - `multiPassPairing(arena, treeList)`: multipass pairing meld (iterative pairwise reduction)
  - `deleteMin(arena, root)`: extracts min and consolidates children via two-pass pairing meld
  - `deleteMinMultiPass(arena, root)`: extracts min and consolidates children via multipass pairing meld
  - `decreaseKey(arena, root, nodeId, newKey)`: updates key and unlinks node via `hPrev` pointer in O(1) before melding with root
  - `verifyPairingHeapInvariants(arena, root)`: validates min-heap order and bidirectional child/sibling/prev pointer integrity
  - `countPairingHeapNodes(arena, root)`: tree traversal node counter
- Comprehensive test coverage:
  - Test Case 1: Sequential Inserts, Priority Order Extraction, and Min-Heap Invariants
  - Test Case 2: Multi-Pass and Two-Pass Consolidation Parity and Direct Meld Operations
  - Test Case 3: Decrease-Key Operations, Priority Promotion, and Pointer Relink Integrity
  - Test Case 4: Boundary Cases, Empty Heaps, Duplicate Keys, and Negative Priorities
  - Test Case 5: Randomized Stress Simulation against Naive Priority Queue Oracle
- Deterministic assertions and runtime benchmark
- Strict camelCase naming conventions without underscores
- Zero emojis and zero em dashes
