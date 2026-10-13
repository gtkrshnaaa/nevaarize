# ISSUE-1: feat(algorithms): implement leftistHeapMeldableQueue in richcodesamples

## Objective
Implement self-contained Leftist Heap priority queue with logarithmic meld and extract-min operations in `examples/richcodesample/leftistHeapMeldableQueue.nva` with deterministic assertions and runtime benchmark.

## Target Files
- `examples/richcodesample/leftistHeapMeldableQueue.nva`

## Architecture & Requirements
- Arena-allocated mutable Leftist Heap priority queue
- Null Path Length (NPL / dist / s-value) tracking per node
- Min-heap property invariant: key(parent) <= key(child)
- Leftist property invariant: dist(left) >= dist(right)
- Subtree size maintenance: size(node) = 1 + size(left) + size(right)
- Logarithmic meld operation: O(log N) time
- Priority queue operations: insert, findMin, extractMin, meld, isEmpty, size
- Multi-heap meld (merging disjoint queues into a single consolidated queue)
- Edge cases: empty heap extraction, duplicate keys, negative keys, single-element melds
- Deterministic assertions across all properties and randomized stress benchmark
- Strict camelCase naming conventions without underscores
- Zero emojis and zero em dashes
