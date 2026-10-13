# ISSUE-2: feat(algorithms): implement blockCutTreeBiconnectivityDecomp in richcodesamples

## Objective
Implement self-contained Block-Cut Tree for 2-connected components and articulation point decomposition in `examples/richcodesample/blockCutTreeBiconnectivityDecomp.nva` with deterministic assertions and runtime benchmark.

## Target Files
- `examples/richcodesample/blockCutTreeBiconnectivityDecomp.nva`

## Architecture & Requirements
- Forward-star and dynamic adjacency graph representation for zero GC overhead
- Tarjan's biconnected components algorithm with iterative explicit stack frames
- Block-Cut Tree (BCT) bipartite graph construction connecting block nodes and cut nodes
- Query interfaces for articulation points, 2-connected blocks, bridge edges, and cut vertex paths
- Comprehensive test coverage:
  - Test Case 1: Butterfly / Bowtie Graph and Single Cut Vertex Decomposition
  - Test Case 2: Multi-Block Cactus and Chain of Biconnected Components
  - Test Case 3: Boundary Topologies and Edge Cases (Complete K4, Trees, Disconnected Forest)
  - Test Case 4: Bridge Detection and Articulation Point Separation Queries
  - Test Case 5: Randomized Stress Simulation against Naive Vertex Removal Oracle
- Deterministic assertions and runtime timing
- Strict camelCase naming conventions (No-Underscore Policy)
- Zero emojis and zero em dashes
