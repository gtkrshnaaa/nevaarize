# ISSUE-3: feat(algorithms): implement heavyLightDecompositionEngine in richcodesamples

## Objective
Implement self-contained Heavy-Light Decomposition engine for path aggregation and subtree updates on weighted trees in `examples/richcodesample/heavyLightDecompositionEngine.nva` with deterministic assertions and runtime benchmark.

## Target Files
- `examples/richcodesample/heavyLightDecompositionEngine.nva`

## Architecture & Requirements
- Heavy-Light Decomposition (HLD) partitioning tree into vertex-disjoint heavy paths
- Logarithmic path jumping via chain heads: O(log^2 N) path queries and updates
- Contiguous DFS layout mapping each subtree to a contiguous interval [pos[u], pos[u] + size[u] - 1] in O(log N)
- Integrated Segment Tree with lazy propagation supporting point and range updates (add, set) and range aggregates (sum, max, min)
- Lowest Common Ancestor (LCA) and tree distance queries in O(log N)
- Comprehensive test coverage:
  - Test Case 1: Tree Construction, Heavy-Light Decomposition & LCA Invariants
  - Test Case 2: Path Aggregation Queries and Point Updates
  - Test Case 3: Subtree Range Updates with Lazy Propagation
  - Test Case 4: Boundary Cases & Degenerate Topologies (Stars, Line Chains, Singletons)
  - Test Case 5: Randomized Stress Benchmark against Naive Tree Traversal Oracle
- Deterministic assertions and runtime timing
- Strict camelCase naming conventions (No-Underscore Policy)
- Zero emojis and zero em dashes
