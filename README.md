<p align="center">
  <img src="docs/assets/img/nvatextlogo.svg" alt="NEVAARIZE" width="900">
</p>

---

**Nevaarize** is a high-performance programming language with a **native JIT compiler** that generates Linux x86-64 machine code at runtime. Features include **async/await** concurrency, an integrated **generational garbage collector**, and a modular standard library. Built entirely in C++23 with zero external dependencies.

## Performance Highlights

Nevaarize delivers **exceptional performance**, outperforming even native languages in specific workloads:

| Category | Performance | vs Rust | vs Zig | vs Node.js |
|----------|------------|---------|--------|------------|
| **Integer Add** | **4117M ops/sec** | **1.05x faster** | **1.01x faster** | **3.2x faster** |
| **Double Arith** | **2077M ops/sec** | **1.99x faster** | **1.99x faster** | **2.0x faster** |
| **Array Push** | **3742M ops/sec** | **12.7x faster** | **10.8x faster** | **65x faster** |
| **Memory Usage** | **4.82 MB** | **53% less** | **38% less** | **21x less** |

> **Key Achievement**: Nevaarize's JIT implementation of dynamic arrays is **faster than Rust's `Vec` and Zig's `ArrayList`**, while using significantly less memory!

---

## Benchmarks

All benchmarks are **reproducible** and included in this repository. Run them yourself:

```bash
cd languagebench
./run_comparison.sh
```

### Benchmark Methodology

Each benchmark is designed to test a specific aspect of the language runtime. The tests are consistent across all languages (C++, Rust, Go, Node.js, Zig, etc.).

| Benchmark | Iterations | Description |
|-----------|------------|-------------|
| **Integer Add** | 1,000,000,000 | A tight loop performing 1 billion integer increments. Tests basic loop overhead and integer arithmetic performance. |
| **Double Arith** | 100,000,000 | 100 million floating-point additions. Tests FPU performance and JIT handling of float types. |
| **String Concat** | 500,000 | Appends a character to a string 500k times. Tests dynamic string allocation and resizing strategies. |
| **Array Push** | 1,000,000 | Pushes 1 million integers into a dynamic array. Tests memory allocator efficiency and array growth algorithms. |
| **Struct Access** | 50,000,000 | Writes and reads a field in a struct 50 million times. Tests property access overhead and inline caching. |

### Full Benchmark Results (13 Languages)

> **Verified Performance Data**
> Executed on: **Sat Mar  7 04:59:24 PM WIB 2026**
> Commit Hash: [`9824cfdbf6d459535e72fb4aebbfa62887170d7a`](https://github.com/gtkrshnaaa/nevaarize/commit/9824cfdbf6d459535e72fb4aebbfa62887170d7a)
> System: **Intel i5-1135G7 @ 2.40GHz | Ubuntu 24.04.4 LTS | 8GB RAM**

```
================================================================================
                   ULTIMATE LANGUAGE BENCHMARK BATTLE
================================================================================
System: Intel i5-1135G7 @ 2.40GHz | Ubuntu 24.04 | 8GB RAM

┌─────────────────────────────────────────────────────────────────────────────┐
│  NEVAARIZE (JIT Compiled - Linux x86-64 Native)                                   │
├─────────────────┬──────────────────┬─────────────────────────────────────────┤
│  Integer Add    │ 4,117,254,568 ops/sec │ 0.242s                             │
│  Double Arith   │ 2,077,788,495 ops/sec │ 0.048s  ← Insane FP throughput!    │
│  String Concat  │   474,236,171 ops/sec │ 0.0010s                            │
│  Array Push     │ 3,742,767,102 ops/sec │ 0.0002s ← #1 of ALL languages!      │
│  Struct Access  │ 1,457,649,325 ops/sec │ 0.034s                             │
├─────────────────┴──────────────────┴─────────────────────────────────────────┤
│  Peak RAM: 4,820 KB (LOWEST) │ Total Time: 0.32s                            │
└─────────────────────────────────────────────────────────────────────────────┘
```

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
- **Linux x86-64 CPU**: SSE4.2+ required
- **Linux**: Primary platform (macOS support planned)

### Installation

```bash
# Clone repository
git clone https://github.com/gtkrshnaaa/nevaarize.git
cd nevaarize

# Build
make

# Install globally (optional)
sudo make install

# Run a script
nevaarize examples/basics/helloWorld.nva

# Or use from local build
./bin/nevaarize examples/basics/helloWorld.nva
```

### Hello World

```nva
print("Hello, Nevaarize!")
```

### Core Syntax

```nva
x = 42
y = 3.14
z = x + y
print("Result:", z)
```

### Functions & Recursion

```nva
func factorial(n) {
    if (n <= 1) { return 1 }
    return n * factorial(n - 1)
}
print("5! =", factorial(5))
```

### Data Structures

```nva
// Structs
struct Point { x, y }
p = Point(10, 20)
p.x = 100
print("Point:", p.x, p.y)

// Arrays
arr = [1, 2, 3, 4, 5]
arr.push(6)
for (val in arr) {
    print(val)
}
```

### High-Performance Async

```nva
import stdlib time as t

async func computeSum(n) {
    total = 0
    for (i in Range(0, n)) {
        total = total + i
    }
    return total
}

start = t.nanos()
result = await computeSum(1000000)
elapsed = (0.0 + (t.nanos() - start)) / 1000000000.0

print("Sum:", result)
print("Time:", elapsed, "seconds")
```

---

## Getting Started

To get started with Nevaarize, follow the [Installation Guide](docs/pages/installation.html) and then try the [Quick Start](docs/pages/quickstart.html).

## Algorithm Suite

Nevaarize's JIT capabilities are verified through a comprehensive suite of algorithms in `examples/algorithm/`:

| Domain | Algorithms | Status |
|--------|------------|--------|
| **Sorting** | Bubble Sort, Insertion Sort, Selection Sort, Cocktail Shaker | ✅ PASS |
| **Searching** | Binary Search, Linear Search, Sentinel Search, Bounds | ✅ PASS |
| **Math** | Factorial, Fibonacci, Power, GCD, LCM, isPrime | ✅ PASS |
| **Data Structures** | Stack (LIFO), Queue (FIFO), Matrix Ops (3x3) | ✅ PASS |
| **Advanced** | Kadane's Algorithm, Longest Increasing Subsequence, Two Pointers | ✅ PASS |
| **Numerical** | Newton's Square Root, Integer Square Root, ModPow | ✅ PASS |
| **Geometry** | Euclidean/Manhattan Distance, Triangle Area, AABB | ✅ PASS |
| **Bitwise** | Popcount, Power of 2, Integer to Binary, Log2 | ✅ PASS |

> Run the suite: `bash examples/runExamples.sh`

---

## Architecture

### JIT Compilation Pipeline

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

### Key Features

- **SIMD-accelerated lexer**
- **Flattened AST**: Cache-optimized tree structure  
- **Direct code generation**: No interpreter, pure machine code
- **Type specialization**: Runtime type-based optimization
- **Nanosecond timing**: `clock_gettime(CLOCK_MONOTONIC)` syscall
- **Generational GC**: Bump-pointer allocation with young/old generation collection, integrated transparently into all heap allocations (arrays, strings)
- **Async/Await**: Task-based concurrency model with `async func` declarations and `await` expressions, available by default with no flags required

---

## Standard Library

### Math Module

```nva
import stdlib math as m

m.Sqrt(16)      // 4.0
m.Pow(2, 10)    // 1024
m.Sin(3.14159)  // ~0
m.Abs(-42)      // 42
```

### Time Module

```nva
import stdlib time as t

t.nanos()       // Nanosecond timestamp
t.millis()      // Millisecond timestamp
t.clock()       // High-resolution clock (double)
t.sleep(1000)   // Sleep for 1000ms
t.format("%Y-%m-%d")  // "2026-02-01"
```

### IO Module

```nva
import stdlib io as io

name = io.Input("Your name: ")
io.Print("Hello,", name)
io.Write("No newline")
```

### AI Module

```nva
import stdlib ai as ai

// SIMD-accelerated tensor operations
// activations, loss functions, and inference
```

### HTTP Module

```nva
import stdlib http as http

// High-performance HTTP server for model serving
```

---

## Code Style

Nevaarize enforces **clean code** through the **No-Underscore Policy**:

```nva
// Accepted
myVariable = 10
func calculateSum(a, b) { return a + b }

// Rejected at compile-time
my_variable = 10
func calculate_sum(a, b) { }
```

---

## Build Targets

```bash
make              # Build optimized JIT compiler
make debug        # Build with debug symbols
make benchmark    # Run performance benchmarks
make clean        # Remove build artifacts
make install      # Install to /usr/local/bin
make uninstall    # Remove from system
```

---

## Roadmap

- [x] **Core JIT Compiler** - Linux x86-64 code generation
- [x] **Standard Library** - Math, IO, Time modules
- [x] **Structs** - User-defined data types
- [x] **Arrays** - Dynamic arrays with push/pop
- [x] **Benchmark Suite** - Multi-language comparison
- [x] **AI Core** - Tensor operations and model serving
- [x] **Garbage Collector** - Generational GC integrated into JIT allocations
- [x] **Async/Await** - Task-based concurrency with async functions

---

## Contributing

Contributions are welcome! Please ensure:

1. **Zero Dependencies** - No external libraries
2. **Performance First** - Benchmark your changes
3. **No Underscores** - Follow naming conventions
4. **Test Coverage** - Add tests for new features

---

## License

MIT License - See [LICENSE](LICENSE) for details.

---

## Links

- **Repository**: [github.com/gtkrshnaaa/nevaarize](https://github.com/gtkrshnaaa/nevaarize)
- **Issues**: [github.com/gtkrshnaaa/nevaarize/issues](https://github.com/gtkrshnaaa/nevaarize/issues)
- **Discussions**: [github.com/gtkrshnaaa/nevaarize/discussions](https://github.com/gtkrshnaaa/nevaarize/discussions)

---

<p align="center">
  <strong>Nevaarize</strong> — Native JIT Performance, Zero Compromise
</p>
