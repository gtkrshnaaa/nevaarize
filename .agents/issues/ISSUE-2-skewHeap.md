# ISSUE-2: feat(algorithms): implement skewHeapSelfAdjustingMeld in richcodesamples

## Objective
Implement self-contained Skew Heap priority queue with self-adjusting unconditional child swapping meld in `examples/richcodesample/skewHeapSelfAdjustingMeld.nva` with deterministic assertions and runtime benchmark.

## Target Files
- `examples/richcodesample/skewHeapSelfAdjustingMeld.nva`

## Architecture & Requirements
- Arena-allocated mutable Skew Heap priority queue
- Self-adjusting unconditional child swap on every meld operation without balance tracking
- Amortized logarithmic complexity: O(log N) amortized per meld, insert, and extractMin
- Min-heap property invariant: key(parent) <= key(child)
- Subtree size maintenance: size(node) = 1 + size(left) + size(right)
- Priority queue operations: insert, findMin, extractMin, meld, isEmpty, size
- Multi-heap tournament meld (consolidating multiple disjoint priority queues)
- Comprehensive test coverage:
  - Test Case 1: Sequential Inserts, Priority Order Extraction, and Min-Heap Invariants
  - Test Case 2: Direct Meld Operations, Subtree Merging, and Size Invariants
  - Test Case 3: Boundary Cases, Duplicate Keys, and Monotonic Input Sequences
  - Test Case 4: Multi-Heap Tournament Meld and Forest Processing
  - Test Case 5: Randomized Stress Simulation against Priority Queue Oracle
- Deterministic assertions and runtime benchmark
- Strict camelCase naming conventions without underscores
- Zero emojis and zero em dashes
