<p align="center">
  <h1 align="center">NEVAARIZE</h1>
  <p align="center">
    <strong>Native JIT Performance • Zero Dependencies • Pure C++23</strong>
  </p>
  <p align="center">
    <a href="#benchmarks"><img src="https://img.shields.io/badge/Array_Push-3730M_ops%2Fs-brightgreen?style=flat-square" alt="Array Push"></a>
    <a href="#benchmarks"><img src="https://img.shields.io/badge/Integer_Ops-4089M_ops%2Fs-blue?style=flat-square" alt="Integer Ops"></a>
    <a href="#benchmarks"><img src="https://img.shields.io/badge/Memory-4.98MB_Peak-orange?style=flat-square" alt="Memory"></a>
    <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-yellow?style=flat-square" alt="License"></a>
  </p>
</p>

---

**Nevaarize** is a high-performance programming language with a **native JIT compiler** that generates x86-64 machine code at runtime. Features include **async/await** concurrency, an integrated **generational garbage collector**, and a modular standard library. Built entirely in C++23 with zero external dependencies.

## Performance Highlights

Nevaarize delivers **exceptional performance**, outperforming even native languages in specific workloads:

| Category | Performance | vs Rust | vs Zig | vs Node.js |
|----------|------------|---------|--------|------------|
| **Array Push** | **3730M ops/sec** | **12.5x faster** | **11.0x faster** | **77x faster** |
| **Integer Ops** | **4089M ops/sec** | **1.05x faster** | 0.98x | **3.2x faster** |
| **Memory Usage** | **4.98 MB** | **50% less** | **36% less** | **14x less** |

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
> Executed on: **Sat Feb 28 11:01:46 PM WIB 2026**
> Commit Hash: [`a1a21aa539583a94c8b7ae01d3596ea3f61d1ff5`](https://github.com/gtkrshnaaa/nevaarize/commit/a1a21aa539583a94c8b7ae01d3596ea3f61d1ff5)
> System: **Intel i5-1135G7 @ 2.40GHz | Ubuntu 24.04.4 LTS | 8GB RAM**

```
================================================================================
                   ULTIMATE LANGUAGE BENCHMARK BATTLE
================================================================================
System: Intel i5-1135G7 @ 2.40GHz | Ubuntu 24.04 | 8GB RAM

┌─────────────────────────────────────────────────────────────────────────────┐
│  NEVAARIZE (JIT Compiled - x86-64 Native)                                   │
├─────────────────┬──────────────────┬─────────────────────────────────────────┤
│  Integer Add    │ 4,089,721,370 ops/sec │ 0.244s                             │
│  Double Arith   │ 2,056,728,310 ops/sec │ 0.048s  ← Fast FP pipeline!        │
│  String Concat  │  87,224,718 ops/sec │  0.0005s                             │
│  Array Push     │ 3,730,355,017 ops/sec │ 0.0002s ← #1 of ALL languages!      │
│  Struct Access  │ 1,370,579,733 ops/sec │ 0.036s                             │
├─────────────────┴──────────────────┴─────────────────────────────────────────┤
│  Peak RAM: 4,980 KB (LOWEST) │ Total Time: 0.33s                            │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Comparison Table

| Language | Integer Add | Double Arith | String Concat | Array Push | Struct Access | Peak RAM |
|----------|------------|--------------|---------------|------------|---------------|----------|
| **Nevaarize** | **4,089M** | **2,056M** | 87M | **3,730M** | 1,370M | **4.98MB** |
| Zig (ReleaseFast) | 4,149M | 1,038M | 861M | 338M | **4,151M** | 7.8MB |
| Rust (LLVM -O) | 3,882M | 1,040M | 1,156M | 297M | 3,861M | 10.0MB |
| Go 1.21 | 3,867M | **3,550M** | 0.4M | 90M | 3,722M | 46.2MB |
| Java 21 | 3,726M | 518M | 0.2M | 43M | 987M | 417MB |
| PHP 8 JIT | 3,261M | 1,013M | 97M | 71M | 420M | 49.4MB |
| Node.js (V8) | 1,251M | 1,011M | 15M | 48M | 1,112M | 71.9MB |
| LuaJIT | 1,028M | 965M | 0.8M | 242M | 1,045M | 12.2MB |
| C++ (GCC -O3) | 3,131M | 461M | 547M | 370M | 2,630M | 11.4MB |
| C (GCC -O3) | 2,780M | 462M | 1,218M | 405M | 2,411M | 9.5MB |
| Lua 5.4 | 208M | 147M | 0.8M | 70M | 98M | 19.6MB |
| PHP 8 Std | 464M | 236M | 75M | 58M | 97M | 46.0MB |
| Python 3 | 48M | 34M | 23M | 28M | 34M | 49.0MB |

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
