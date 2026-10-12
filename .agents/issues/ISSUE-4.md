# ISSUE-4: test(regression): execute containerized test suite and sanitizer audit

## Objective
Execute full test runner inside isolated Docker container sandbox via `./test.sh run` and `./test.sh asan`, verify zero memory leaks, validate benchmark outputs, and ensure zero regressions across persistent data structures suite.

## Target Files
- `examples/richcodesample/persistentFenwickTree.nva`
- `examples/richcodesample/persistentDisjointSetUnion.nva`
- `examples/richcodesample/persistentLeftistHeap.nva`
- `examples/richcodesample/persistentSegmentTree.nva`
- `examples/richcodesample/persistentSegmentTreeLazy.nva`
- `examples/richcodesample/persistentTreapSnapshot.nva`
- `examples/richcodesample/persistentTrieString.nva`
- `examples/verification_report.txt`

## Verification Scope & Requirements
- Container sandbox verification under 50% CPU and RAM isolation limits via `./test.sh`
- Standard release execution validation via `./test.sh run` for newly implemented persistent data structures
- AddressSanitizer and LeakSanitizer execution validation via `./test.sh asan` for all persistent structures
- Memory leak detection and memory safety audit (zero heap leaks, zero use-after-free, zero stack overflows)
- Performance and stress benchmark validation (logarithmic operational scaling, timeline branching integrity)
- Full quarantine test suite regression audit across all rich code samples (`./test.sh quarantine`)
- Zero emojis and zero em dashes across all code, tests, and documentation
