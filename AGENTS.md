# NEVAARIZE DEVELOPMENT AGENT RULES

## 1. MANDATORY DOCKER TEST SANDBOX FOR CORE LANGUAGE DEVELOPMENT

All compilation testing, test suite runs, example script executions, stress tests, and memory profiling conducted during the development of the Nevaarize programming language itself (the compiler, JIT engine, garbage collector, standard library, and runtime in `core/`) MUST ALWAYS be executed inside the isolated Docker container environment via `test.sh` or `docker-compose.yml`.

### Two-Tier Development and Verification Protocol
1. **Tier 1: Pre-Flight Safety Quarantine (MANDATORY DOCKER)**:
   - Applies to all active language development, unverified `.nva` scripts, new compiler features, bug fixes, and memory allocator modifications (`core/src/`, `core/include/`, `core/stdlib/`).
   - Must run inside the Docker container (`./test.sh`) constrained to 50% CPU and 50% physical RAM to catch leaks, segfaults, and infinite loops safely without endangering host stability.
2. **Tier 2: Peak Performance Benchmarking (NATIVE BARE METAL)**:
   - Once code has passed Tier 1 quarantine verification (zero memory leaks in ASan, zero faults in Valgrind), official performance benchmarks (`languagebench/runComparison.sh`, `battle_report.txt`, and `README.md` metrics) MUST be executed natively on bare metal.
   - Native execution ensures benchmarks measure true 100% unconstrained hardware throughput without cgroup throttling or container runtime overhead.
3. **Downstream Application Development (FREE CHOICE)**:
   - End-users developing standalone software or shipping consumer products with Nevaarize are free to run natively or containerized per their preference.

---

## 2. RATIONALE: HOST MEMORY SAFETY AND STABILITY

1. **JIT and Heap Volatility**:
   The Nevaarize core runtime generates native x86-64 machine instructions into dynamic executable memory pages (`mmap`), manages bump-pointer arenas, and performs custom garbage collection.
2. **Host Lockup Prevention**:
   Runaway allocations or memory leaks in C++ core code can exhaust physical RAM on development machines. Under bare metal execution, this triggers aggressive disk swap thrashing, freezing desktop environments and forcing hard hardware resets or thermal reboots.
3. **50% Hardware Boundary Isolation**:
   The sandbox script (`./test.sh`) dynamically enforces a hard cap of 50% host CPU cores and 50% physical RAM with zero extra swap overhead. If a memory leak or infinite allocation occurs, the Linux cgroup OOM killer terminates only the container process (Exit Code 137). The host operating system remains completely stable and responsive.

---

## 3. STANDARD EXECUTION WORKFLOW

Whenever running, verifying, or debugging code during development sessions, use the following commands exclusively:

| Task | Command | Description |
| :--- | :--- | :--- |
| Default Verification | `./test.sh all` | Cleans build, compiles release binary, runs baseline smoke tests in sandbox |
| Single Script Test | `./test.sh run <path.nva>` | Executes target script within 50% CPU/RAM constraints |
| Memory Leak Detection | `./test.sh asan <path.nva>` | Runs script under AddressSanitizer and LeakSanitizer (pinpoints source line) |
| Heap Validation | `./test.sh valgrind <path.nva>` | Runs script under Valgrind Memcheck for invalid read/writes and lost blocks |
| Interactive Debugging | `./test.sh shell` | Spawns interactive bash shell inside the container sandbox |
| Resource Monitoring | `./test.sh stats` | Streams live container CPU and memory metrics |

---

## 4. CODE REVIEW AND PREREQUISITE CHECKS

1. Direct execution of newly modified or unverified `.nva` scripts on the bare metal host without container sandboxing is strictly forbidden.
2. Changes to `core/include/gc.hpp`, `core/src/jit.cpp`, or dynamic memory structures must pass `./test.sh asan <script>` with zero reported leaks before finalizing changes.
