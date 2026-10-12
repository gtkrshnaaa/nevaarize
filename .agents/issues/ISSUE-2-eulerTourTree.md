# ISSUE-2: feat(algorithms): implement eulerTourTreeDynamicForest in richcodesamples

## Objective
Implement self-contained Euler Tour Tree for dynamic tree subtree modifications and connectivity queries in `examples/richcodesample/eulerTourTreeDynamicForest.nva` with deterministic assertions and runtime benchmark.

## Target Files
- `examples/richcodesample/eulerTourTreeDynamicForest.nva`

## Architecture & Requirements
- Euler Tour Tree (ETT) data structure for dynamic forest representation
- Self-balancing randomized Treap backing with explicit parent pointers
- Logarithmic time dynamic edge links, cuts, and component rerooting
- Maintenance of dynamic tree components across arbitrary forest topologies
- O(log N) tree-level and subtree-level aggregate queries (sum and size)
- Point weight updates with bottom-up ancestor propagation in O(log N)
- Comprehensive test coverage:
  - Test Case 1: Initial Forest Isolation and Dynamic Linking
  - Test Case 2: Dynamic Cuts and Component Partition Invariants
  - Test Case 3: Subtree Aggregate Queries and Reroot Invariance
  - Test Case 4: Vertex Weight Point Updates & Dynamic Re-aggregates
  - Test Case 5: Randomized Stress Simulation against Adjacency List Oracle
- Deterministic assertions and runtime timing
- Strict camelCase naming conventions (No-Underscore Policy)
- Zero emojis and zero em dashes
