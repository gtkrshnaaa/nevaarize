<p align="center">
  <h1 align="center">NEVAARIZE</h1>
  <p align="center">
    <strong>Native JIT Performance • Zero Dependencies • Pure C++23</strong>
  </p>
  <p align="center">
    <a href="#benchmarks"><img src="https://img.shields.io/badge/Array_Push-897M_ops%2Fs-brightgreen?style=flat-square" alt="Array Push"></a>
    <a href="#benchmarks"><img src="https://img.shields.io/badge/Integer_Ops-973M_ops%2Fs-blue?style=flat-square" alt="Integer Ops"></a>
    <a href="#benchmarks"><img src="https://img.shields.io/badge/Memory-5.05MB_Peak-orange?style=flat-square" alt="Memory"></a>
    <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-yellow?style=flat-square" alt="License"></a>
  </p>
</p>

---

**Nevaarize** is a high-performance programming language with a **native JIT compiler** that generates x86-64 machine code at runtime. Features include **async/await** concurrency, an integrated **generational garbage collector**, and a modular standard library. Built entirely in C++23 with zero external dependencies.

## Performance Highlights

Nevaarize delivers **exceptional performance**, outperforming even native languages in specific workloads:

| Category | Performance | vs Rust | vs Zig | vs Node.js |
|----------|------------|---------|--------|------------|
| **Array Push** | **897M ops/sec** | **2.8x faster** | **2.4x faster** | **21x faster** |
| **Integer Ops** | **973M ops/sec** | 0.26x | 0.24x | 0.78x |
| **Memory Usage** | **5.05 MB** | **49% less** | **35% less** | **16x less** |

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
> Executed on: **Sat Feb 21 08:35:27 AM WIB 2026**
> Commit Hash: [`6ebde9081c1272302f8627a6c5e0c5838ae695ae`](https://github.com/gtkrshnaaa/nevaarize/commit/6ebde9081c1272302f8627a6c5e0c5838ae695ae)
> System: **Intel i5-1135G7 @ 2.40GHz | Ubuntu 24.04.4 LTS | 8GB RAM**

```
================================================================================
                   ULTIMATE LANGUAGE BENCHMARK BATTLE
================================================================================
System: Intel i5-1135G7 @ 2.40GHz | Ubuntu 24.04 | 8GB RAM

┌─────────────────────────────────────────────────────────────────────────────┐
│  NEVAARIZE (JIT Compiled - x86-64 Native)                                   │
├─────────────────┬──────────────────┬─────────────────────────────────────────┤
│  Integer Add    │ 973,344,039 ops/sec │ 1.027s                              │
│  Double Arith   │ 508,132,312 ops/sec │ 0.196s                              │
│  String Concat  │  67,675,207 ops/sec │ 0.0007s                             │
│  Array Push     │ 897,709,583 ops/sec │ 0.001s  ← #1 of ALL languages!      │
│  Struct Access  │ 658,106,603 ops/sec │ 0.075s                              │
├─────────────────┴──────────────────┴─────────────────────────────────────────┤
│  Peak RAM: 5,052 KB (LOWEST) │ Total Time: 1.30s                            │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Comparison Table

| Language | Integer Add | Double Arith | String Concat | Array Push | Struct Access | Peak RAM |
|----------|------------|--------------|---------------|------------|---------------|----------|
| **Nevaarize** | 973M | **508M** | **67M** | **897M** | 658M | **5.05MB** |
| Zig (ReleaseFast) | 4,046M | 1,007M | 504M | 370M | **4,008M** | 7.8MB |
| Rust (LLVM -O) | 3,772M | 1,010M | 762M | 321M | 3,697M | 9.9MB |
| Go 1.21 | 3,740M | **3,695M** | 0.4M | 90M | 3,712M | 46.2MB |
| Java 21 | 3,607M | 497M | 0.2M | 57M | 956M | 383MB |
| PHP 8 JIT | 3,151M | 1,013M | 95M | 73M | 406M | 49.6MB |
| C++ (GCC -O3) | 2,790M | 439M | 415M | 320M | 2,550M | 11.5MB |
| C (GCC -O3) | 2,781M | 441M | 1,047M | 404M | 2,317M | 9.3MB |
| Node.js (V8) | 1,246M | 981M | 14M | 42M | 1,068M | 82.6MB |
| LuaJIT | 883M | 915M | 0.8M | 218M | 1,012M | 12.1MB |
| Lua 5.4 | 202M | 145M | 0.8M | 61M | 95M | 19.6MB |
| PHP 8 Std | 442M | 237M | 69M | 58M | 96M | 46.2MB |
| Python 3 | 47M | 33M | 23M | 25M | 32M | 49.1MB |

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
│   │   ├── Lexer.hpp       # Tokenizer
│   │   ├── Parser.hpp      # AST builder
│   │   ├── AST.hpp         # Abstract syntax tree
│   │   ├── JIT.hpp         # JIT compiler
│   │   ├── GC.hpp          # Generational garbage collector
│   │   ├── Value.hpp       # Runtime values
│   │   ├── CodeGen.hpp     # x86-64 code buffer
│   │   └── IR.hpp          # Intermediate representation
│   └── src/
│       ├── Lexer.cpp
│       ├── Parser.cpp
│       ├── JIT.cpp         # x86-64 code generation + async/GC runtime
│       └── Main.cpp        # Entry point
├── stdlib/                  # Standard library
│   ├── include/
│   └── src/
│       ├── Math.cpp        # Math functions
│       ├── IO.cpp          # Input/Output
│       ├── Time.cpp        # Timing functions
│       ├── AI.cpp          # Tensor ops & model serving
│       └── HTTP.cpp        # HTTP server
├── examples/                # Example programs
│   ├── basics.nva          # Language basics
│   ├── advanced.nva        # Algorithms & data structures
│   ├── asyncAwait.nva      # Async/await concurrency
│   └── gcDemo.nva          # Garbage collector demo
├── languagebench/           # Benchmark suite
│   ├── run_comparison.sh   # Run all benchmarks
│   ├── bench_*.nva/cpp/... # Language-specific benchmarks
│   └── battle_report.txt   # Latest results
├── Makefile
└── README.md
```

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
