<p align="center">
  <h1 align="center">NEVAARIZE</h1>
  <p align="center">
    <strong>Native JIT Performance • Zero Dependencies • Pure C++23</strong>
  </p>
  <p align="center">
    <a href="#benchmarks"><img src="https://img.shields.io/badge/Array_Push-3753M_ops%2Fs-brightgreen?style=flat-square" alt="Array Push"></a>
    <a href="#benchmarks"><img src="https://img.shields.io/badge/Integer_Ops-4100M_ops%2Fs-blue?style=flat-square" alt="Integer Ops"></a>
    <a href="#benchmarks"><img src="https://img.shields.io/badge/Memory-5.08MB_Peak-orange?style=flat-square" alt="Memory"></a>
    <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-yellow?style=flat-square" alt="License"></a>
  </p>
</p>

---

**Nevaarize** is a high-performance programming language with a **native JIT compiler** that generates x86-64 machine code at runtime. Features include **async/await** concurrency, an integrated **generational garbage collector**, and a modular standard library. Built entirely in C++23 with zero external dependencies.

## Performance Highlights

Nevaarize delivers **exceptional performance**, outperforming even native languages in specific workloads:

| Category | Performance | vs Rust | vs Zig | vs Node.js |
|----------|------------|---------|--------|------------|
| **Integer Add** | **4100M ops/sec** | **1.05x faster** | 0.99x | **3.1x faster** |
| **Double Arith** | **2080M ops/sec** | **2.08x faster** | **1.99x faster** | **2.0x faster** |
| **Array Push** | **3753M ops/sec** | **10.9x faster** | **10.5x faster** | **62x faster** |
| **Memory Usage** | **5.08 MB** | **50% less** | **35% less** | **16x less** |

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
| **String Concat** | 50,000 | Appends a character to a string 50k times. Tests dynamic string allocation and resizing strategies. |
| **Array Push** | 1,000,000 | Pushes 1 million integers into a dynamic array. Tests memory allocator efficiency and array growth algorithms. |
| **Struct Access** | 50,000,000 | Writes and reads a field in a struct 50 million times. Tests property access overhead and inline caching. |

### Full Benchmark Results (13 Languages)

> **Verified Performance Data**
> Executed on: **Sun Mar 1 08:30:00 AM WIB 2026**
> Commit Hash: [`e1fc8574ce6aeca125e622483329b6bd7b20e387`](https://github.com/gtkrshnaaa/nevaarize/commit/e1fc8574ce6aeca125e622483329b6bd7b20e387)
> System: **Intel i5-1135G7 @ 2.40GHz | Ubuntu 24.04.4 LTS | 8GB RAM**

```
================================================================================
                   ULTIMATE LANGUAGE BENCHMARK BATTLE
================================================================================
System: Intel i5-1135G7 @ 2.40GHz | Ubuntu 24.04 | 8GB RAM

┌─────────────────────────────────────────────────────────────────────────────┐
│  NEVAARIZE (JIT Compiled - x86-64 Native)                                   │
├─────────────────┬──────────────────┬─────────────────────────────────────────┤
│  Integer Add    │ 4,100,310,588 ops/sec │ 0.244s                             │
│  Double Arith   │ 2,080,411,828 ops/sec │ 0.048s  ← Insane FP throughput!    │
│  String Concat  │    88,115,043 ops/sec │ 0.0005s                            │
│  Array Push     │ 3,753,739,663 ops/sec │ 0.0002s ← #1 of ALL languages!      │
│  Struct Access  │ 1,368,417,402 ops/sec │ 0.036s                             │
├─────────────────┴──────────────────┴─────────────────────────────────────────┤
│  Peak RAM: 5,080 KB (LOWEST) │ Total Time: 0.33s                            │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Comparison Table

| Languages | Integer Add | Double Arith | String Concat | Array Push | Struct Access | Peak RAM |
|-----------|-------------|--------------|---------------|------------|---------------|----------|
| **Nevaarize** | **4,100M** | **2,080M** | 88M | **3,753M** | 1,368M | **5.08MB** |
| Zig (ReleaseFast) | 4,137M | 1,042M | 718M | 355M | **4,175M** | 7.8MB |
| Rust (LLVM -O) | 3,878M | 997M | 917M | 342M | 3,857M | 10.0MB |
| Go 1.21 | 3,888M | **3,465M** | 0.4M | 90M | 3,732M | 46.0MB |
| Node.js (V8) | 1,296M | 1,023M | 15M | 60M | 1,111M | 81.3MB |
| PHP 8 JIT | 3,264M | 1,042M | 98M | 71M | 423M | 49.4MB |
| Java 21 | 3,721M | 516M | 0.2M | 44M | 984M | 423MB |
| LuaJIT | 1,027M | 965M | 0.8M | 243M | 1,045M | 12.1MB |
| PHP 8 Std | 464M | 248M | 73M | 57M | 100M | 46.1MB |
| Lua 5.4 | 208M | 146M | 0.8M | 69M | 98M | 19.6MB |
| Python 3 | 49M | 34M | 22M | 28M | 34M | 49.0MB |

> See full results in [`languagebench/battle_report.txt`](languagebench/battle_report.txt)

---

## Quick Start

### Prerequisites

- **C++23 Compiler**: GCC 13+ or Clang 16+
- **x86-64 CPU**: SSE4.2+ required
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

### More Examples

```nva
// Variables and arithmetic
x = 42
y = 3.14
z = x + y
print("Result:", z)

// Functions
func factorial(n) {
    if (n <= 1) {
        return 1
    }
    return n * factorial(n - 1)
}
print("5! =", factorial(5))

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

// Timing
import stdlib time as t
start = t.nanos()
// ... computation ...
elapsed = (0.0 + (t.nanos() - start)) / 1000000000.0
print("Elapsed:", elapsed, "seconds")

// Async/Await
async func computeSum(n) {
    total = 0
    i = 0
    while (i < n) {
        total = total + i
        i = i + 1
    }
    return total
}
task = computeSum(1000)
result = await task
print("Sum:", result)
```

---

## Project Structure

```
nevaarize/
├── core/                    # JIT compiler core
│   ├── include/            # Headers
│   └── src/
├── stdlib/                  # Standard library
├── examples/                # Example programs
│   ├── algorithm/          # Comprehensive algorithm suite (26/26 PASS)
│   ├── basics.nva
│   ├── advanced.nva
│   └── asyncAwait.nva
├── languagebench/           # Benchmark suite
├── Makefile
└── README.md
```

## Algorithm Suite (26/26 PASS)

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
   [JIT]       → x86-64 machine code
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

- [x] **Core JIT Compiler** - x86-64 code generation
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
