# ISSUE-3: feat(algorithms): implement waveletMatrixLargeAlphabet in richcodesamples

## Objective
Implement self-contained Wavelet Matrix for large alphabet range quantile, rank, and select queries in `examples/richcodesample/waveletMatrixLargeAlphabet.nva` with deterministic assertions and runtime benchmark.

## Target Files
- `examples/richcodesample/waveletMatrixLargeAlphabet.nva`

## Architecture & Requirements
- Wavelet Matrix compact representation over L bit levels without explicit tree pointer structures
- Large alphabet support handling arbitrary 30-bit integers, non-uniform distributions, and duplicate keys
- Prefix-sum-based bitvectors at each level with O(1) rank0 and rank1 operations
- Range Quantile (k-th smallest element in range [l, r]) in O(L) time
- Range Rank (frequency of target value in range [l, r]) in O(L) time
- Range Interval Counting (number of elements in [l, r] with value < upperVal and in [lo, hi]) in O(L) time
- Select query (original index of k-th occurrence of target value) via upward traversal in O(L log N) or O(L)
- Comprehensive test coverage:
  - Test Case 1: Bitvector Construction, Level Partitioning & Range Quantile Invariants
  - Test Case 2: Range Rank, Range Frequency & Range Interval Counting
  - Test Case 3: Select Queries & Bidirectional Index Mapping
  - Test Case 4: Boundary Cases & Large Alphabet Extremes (Huge Values, All Identical, Singletons, Negative/Shifted)
  - Test Case 5: Randomized Stress Benchmark against Brute-Force Array Oracle
- Deterministic assertions and runtime timing
- Strict camelCase naming conventions (No-Underscore Policy)
- Zero emojis and zero em dashes
