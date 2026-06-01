<p align="center">
  <img src="docs/assets/img/nvatextlogo.svg" alt="NEVAARIZE" width="900">
</p>

---

**Nevaarize** is a high-performance programming language with a **native JIT compiler** that generates Linux x86-64 machine code at runtime. Features include **async/await** concurrency, an integrated **generational garbage collector**, and a modular standard library. Built entirely in C++23 with zero external dependencies.

> **⚠️ Early Development:** Nevaarize is under active development. Only scripts inside the `examples/` directory have been verified to run correctly. Scripts outside the provided examples may encounter unexpected behavior or unsupported edge cases. Features described here reflect the language's current capabilities as demonstrated by the example programs.

---

## A Taste of Nevaarize

This single example demonstrates most of the language's features — structs, functions, arrays, control flow, stdlib imports, and async/await:

```nva
import stdlib time as t

// Struct definition
struct Task {
    name,
    priority
}

// Function with logic
func filterHighPriority(tasks) {
    result = []
    for (task in tasks) {
        if (task.priority >= 3) {
            result.push(task.name)
        }
    }
    return result
}

// Async computation
async func processQueue(tasks) {
    high = filterHighPriority(tasks)
    print("High priority tasks:", len(high))
    for (name in high) {
        print(" -", name)
    }
    return len(high)
}

// Main flow
tasks = [
    Task("Deploy", 5),
    Task("Review", 3),
    Task("Coffee", 1),
    Task("Hotfix", 4)
]

start = t.clock()
count = await processQueue(tasks)
elapsed = t.clock() - start

print("Processed", count, "tasks in", elapsed, "seconds")
```

---

## Performance Highlights

> Benchmarks executed on: **Intel i5-1135G7 @ 2.40GHz | Ubuntu 24.04.4 LTS | 8GB RAM**
> Performance numbers are indicative and vary by hardware. See [`languagebench/battle_report.txt`](languagebench/battle_report.txt) for full results.

| Category | Performance | vs Rust | vs Zig | vs Node.js |
|----------|------------|---------|--------|------------|
| **Integer Add** | **4117M ops/sec** | **1.05x faster** | **1.01x faster** | **3.2x faster** |
| **Double Arith** | **2077M ops/sec** | **1.99x faster** | **1.99x faster** | **2.0x faster** |
| **Array Push** | **3742M ops/sec** | **12.7x faster** | **10.8x faster** | **65x faster** |
| **Memory Usage** | **4.82 MB** | **53% less** | **38% less** | **21x less** |

### Comparison Table

| Languages | Integer Add | Double Arith | String Concat | Array Push | Struct Access | Peak RAM |
|-----------|-------------|--------------|---------------|------------|---------------|----------|
| **Nevaarize** | **4,117M** | **2,077M** | 474M | **3,742M** | 1,457M | **4.82MB** |
| Zig (ReleaseFast) | 4,044M | 1,041M | 1,413M | 344M | **4,053M** | 7.8MB |
| Rust (LLVM -O) | 3,904M | 1,042M | 993M | 294M | 3,816M | 10.4MB |
| Go 1.21 | 3,870M | **3,762M** | 0.03M | 88M | 3,766M | 46.3MB |
| Node.js (V8) | 1,257M | 1,017M | 15M | 57M | 1,119M | 102MB |
| PHP 8 JIT | 3,266M | 1,019M | 110M | 81M | 421M | 48.8MB |
| Java 21 | 3,725M | 518M | 0.06M | 56M | 990M | 626MB |
| LuaJIT | 1,026M | 966M | 0.01M | 243M | 1,043M | 14.3MB |
| PHP 8 Std | 463M | 247M | 75M | 62M | 101M | 45.4MB |
| Lua 5.4 | 208M | 146M | 0.04M | 65M | 98M | 21.6MB |
| Python 3 | 49M | 34M | 24M | 28M | 34M | 49.6MB |

> See full results in [`languagebench/battle_report.txt`](languagebench/battle_report.txt)

---

## Quick Start

### Prerequisites

- **C++23 Compiler**: GCC 13+ or Clang 16+
- **Linux x86-64 CPU**: SSE4.2+ required, AVX2 recommended
- **Linux**: Primary supported platform

### Build & Run

```bash
git clone https://github.com/gtkrshnaaa/nevaarize.git
cd nevaarize
make

# Run an example
./bin/nevaarize examples/basics.nva
```

---

## Examples

Nevaarize ships with **64 verified example programs**. See `examples/verification_report.txt` for full test results.

| Directory | Count | Description |
|-----------|-------|-------------|
| `algorithm/` | 15 | Sorting, searching, math, geometry, DP |
| `richcodesample/` | 27 | Game of Life, neural network, crypto, sudoku solver, etc. |
| `modules/` | 6 | Multi-file module import system |
| `benchmarks/` | 1 | Full benchmark suite |
| `stdlibtest/` | 3 | CSV, JSON, String stdlib tests |
| *(standalone)* | 8 | Basics, async, GC, maps, exceptions, etc. |

```bash
# Run the full example suite
bash examples/runExamples.sh
```

---

## Architecture

```
Source Code (.nva)
       ↓
   [Lexer]     → Token stream
       ↓
   [Parser]    → Abstract Syntax Tree
       ↓
   [JIT]       → Linux x86-64 machine code
       ↓
   [Execute]   → Direct CPU execution
```

**Key internals:**
- **Direct code generation** — no interpreter, pure machine code
- **Type specialization** — runtime type-based optimization
- **Generational GC** — bump-pointer allocation with young/old generation collection
- **Async/Await** — task-based concurrency, no flags required
- **SIMD acceleration** — AVX2 intrinsics for AI/tensor operations
- **Zero dependencies** — no LLVM, no external libraries

---

## Standard Library

| Module | Import | Functions |
|--------|--------|-----------|
| **Math** | `import stdlib math as m` | `m.Sqrt()`, `m.Pow()`, `m.Sin()`, `m.Abs()`, etc. |
| **Time** | `import stdlib time as t` | `t.clock()`, `t.nanos()`, `t.sleep()`, `t.format()` |
| **IO** | `import stdlib io as io` | `io.Input()`, `io.Println()`, `io.Write()` |
| **AI** | `import stdlib ai as ai` | Tensor ops, layers, optimizers, model persistence |

---

## Code Style

Nevaarize enforces a **No-Underscore Policy** — use `camelCase`:

```nva
myVariable = 10              // OK
func calculateSum(a, b) {}   // OK

my_variable = 10             // Rejected
func calculate_sum(a, b) {}  // Rejected
```

---

## Documentation

- **[Installation Guide](https://gtkrshnaaa.github.io/nevaarize/pages/installation.html)**
- **[Quick Start](https://gtkrshnaaa.github.io/nevaarize/pages/quickstart.html)**
- **[Language Reference](https://gtkrshnaaa.github.io/nevaarize/pages/basics.html)**
- **[Examples](https://gtkrshnaaa.github.io/nevaarize/pages/examples.html)**

---

## License

MIT License — See [LICENSE](LICENSE) for details.

---

<p align="center">
  <strong>Nevaarize</strong> — Native JIT Performance, Zero Compromise
</p>
