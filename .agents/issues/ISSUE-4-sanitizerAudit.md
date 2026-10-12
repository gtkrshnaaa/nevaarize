# ISSUE-4: test(regression): execute containerized test suite and sanitizer audit

## Objective
Execute full test runner inside isolated Docker container sandbox via `./test.sh run` and `./test.sh asan`, verify zero memory leaks, validate benchmark outputs, and ensure zero regressions across algorithms suite.

## Target Files
- `examples/richcodesample/persistentDisjointSetStack.nva`
- `examples/richcodesample/eulerTourTreeDynamicForest.nva`
- `examples/richcodesample/heavyLightDecompositionEngine.nva`
- `examples/richcodesample/persistentDisjointSetUnion.nva`
- `examples/richcodesample/persistentFenwickTree.nva`
- `examples/richcodesample/persistentLeftistHeap.nva`
- `examples/verification_report.txt`

## Verification Scope & Requirements
- Container sandbox verification under 50% CPU and RAM isolation limits via `./test.sh`
- Standard release execution validation via `./test.sh run` for newly implemented data structures:
  - `persistentDisjointSetStack.nva` (Disjoint Set Union with rollback stack)
  - `eulerTourTreeDynamicForest.nva` (Euler Tour Tree with dynamic cuts/links)
  - `heavyLightDecompositionEngine.nva` (Heavy-Light Decomposition with segment tree)
- AddressSanitizer and LeakSanitizer execution validation via `./test.sh asan`
- Memory safety audit (zero heap leaks, zero use-after-free, zero stack/buffer overflows)
- Performance and stress benchmark validation (deterministic runtime timing, zero oracle mismatches)
- Full quarantine regression test suite execution across all rich code samples (`./test.sh quarantine`)
- Zero emojis and zero em dashes across all code, tests, and documentation
