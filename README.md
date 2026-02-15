<p align="center">
  <h1 align="center">🚀 NEVAARIZE</h1>
  <p align="center">
    <strong>Native JIT Performance • Zero Dependencies • Pure C++23</strong>
  </p>
  <p align="center">
    <a href="#benchmarks"><img src="https://img.shields.io/badge/Array_Push-781M_ops%2Fs-brightgreen?style=flat-square" alt="Array Push"></a>
    <a href="#benchmarks"><img src="https://img.shields.io/badge/Integer_Ops-772M_ops%2Fs-blue?style=flat-square" alt="Integer Ops"></a>
    <a href="#benchmarks"><img src="https://img.shields.io/badge/Memory-5.9MB_Peak-orange?style=flat-square" alt="Memory"></a>
    <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-yellow?style=flat-square" alt="License"></a>
  </p>
</p>

---

**Nevaarize** is a high-performance programming language with a **native JIT compiler** that generates x86-64 machine code at runtime. Built entirely in C++23 with zero external dependencies.

## ⚡ Performance Highlights

Nevaarize delivers **exceptional performance**, outperforming even native languages in specific workloads:

| Category | Performance | vs Rust | vs Zig | vs Node.js |
|----------|------------|---------|--------|------------|
| **Array Push** | **781M ops/sec** | 🏆 **2.2x faster** | 🏆 **1.8x faster** | 🏆 **22x faster** |
| **Integer Ops** | **772M ops/sec** | 0.22x | 0.21x | 0.65x |
| **Memory Usage** | **5.9 MB** | 🏆 **40% less** | 🏆 **25% less** | 🏆 **14x less** |

> 💡 **Key Achievement**: Nevaarize's JIT implementation of dynamic arrays is **faster than Rust's `Vec` and Zig's `ArrayList`**, while using significantly less memory!

---

## 📊 Benchmarks

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

```
================================================================================
                   ULTIMATE LANGUAGE BENCHMARK BATTLE
================================================================================
System: Intel i5-1135G7 @ 2.40GHz | Ubuntu 24.04 | 8GB RAM

┌─────────────────────────────────────────────────────────────────────────────┐
│  NEVAARIZE (JIT Compiled - x86-64 Native)                                   │
├─────────────────┬──────────────────┬─────────────────────────────────────────┤
│  Integer Add    │ 772,026,201 ops/sec │ 1.30s                               │
│  Double Arith   │ 485,909,763 ops/sec │ 0.21s                               │
│  String Concat  │  34,455,052 ops/sec │ 0.001s                              │
│  Array Push     │ 781,154,796 ops/sec │ 0.001s  ← #1 of ALL languages!      │
│  Struct Access  │ 454,984,406 ops/sec │ 0.11s                               │
├─────────────────┴──────────────────┴─────────────────────────────────────────┤
│  Peak RAM: 5,920 KB (LOWEST) │ Total Time: 1.61s                            │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Comparison Table

| Language | Integer Add | Double Arith | String Concat | Array Push | Struct Access | Peak RAM |
|----------|------------|--------------|---------------|------------|---------------|----------|
| **Nevaarize** | 772M | 486M | **34M** | **781M** 🥇 | 455M | **5.9MB** 🥇 |
| Zig (ReleaseFast) | 3,696M 🥇 | 766M | 664M | 419M | **3,753M** 🥇 | 7.8MB |
| Rust (LLVM -O) | 3,566M | 960M | 778M | 347M | 3,495M | 10.0MB |
| Go 1.21 | 3,357M | **3,373M** 🥇 | 0.4M | 67M | 3,012M | 46MB |
| Java 21 | 3,422M | 490M | 0.2M | 50M | 804M | 341MB |
| PHP 8 JIT | 2,832M | 932M | 81M | 65M | 391M | 49MB |
| C++ (GCC -O3) | 2,586M | 418M | 471M | 345M | 2,473M | 11.5MB |
| C (GCC -O3) | 2,621M | 430M | 547M | 342M | 2,139M | 9.5MB |
| Node.js (V8) | 1,195M | 935M | 13M | 35M | 964M | 82MB |
| LuaJIT | 851M | 894M | 0.7M | 205M | 969M | 12MB |
| Lua 5.4 | 186M | 123M | 0.7M | 58M | 85M | 19.5MB |
| PHP 8 Std | 410M | 222M | 69M | 52M | 89M | 46MB |
| Python 3 | 46M | 31M | 21M | 25M | 30M | 49MB |

> See full results in [`languagebench/battle_report.txt`](languagebench/battle_report.txt)

---

## 🛠 Quick Start

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
```

---

## 📁 Project Structure

```
nevaarize/
├── core/                    # JIT compiler core
│   ├── include/            # Headers
│   │   ├── Lexer.hpp       # Tokenizer
│   │   ├── Parser.hpp      # AST builder
│   │   ├── AST.hpp         # Abstract syntax tree
│   │   ├── JIT.hpp         # JIT compiler
│   │   └── Value.hpp       # Runtime values
│   └── src/
│       ├── Lexer.cpp
│       ├── Parser.cpp
│       ├── JIT.cpp         # x86-64 code generation
│       └── Main.cpp        # Entry point
├── stdlib/                  # Standard library
│   ├── include/
│   └── src/
│       ├── Math.cpp        # Math functions
│       ├── IO.cpp          # Input/Output
│       └── Time.cpp        # Timing functions
├── examples/                # Example programs
├── languagebench/           # Benchmark suite
│   ├── run_comparison.sh   # Run all benchmarks
│   ├── bench_*.nva/cpp/... # Language-specific benchmarks
│   └── battle_report.txt   # Latest results
├── docs/                    # Documentation
├── Makefile
└── README.md
```

---

## 🔧 Architecture

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

- **Zero-copy tokenization**: SIMD-accelerated lexer
- **Flattened AST**: Cache-optimized tree structure  
- **Direct code generation**: No interpreter, pure machine code
- **Type specialization**: Runtime type-based optimization
- **Nanosecond timing**: `clock_gettime(CLOCK_MONOTONIC)` syscall

---

## 📚 Standard Library

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

## 🎨 Code Style

Nevaarize enforces **clean code** through the **No-Underscore Policy**:

```nva
// ✅ Accepted
myVariable = 10
func calculateSum(a, b) { return a + b }

// ❌ Rejected at compile-time
my_variable = 10
func calculate_sum(a, b) { }
```

---

## 🏗 Build Targets

```bash
make              # Build optimized JIT compiler
make debug        # Build with debug symbols
make benchmark    # Run performance benchmarks
make clean        # Remove build artifacts
make install      # Install to /usr/local/bin
make uninstall    # Remove from system
```

---

## 🗺 Roadmap

- [x] **Core JIT Compiler** - x86-64 code generation
- [x] **Standard Library** - Math, IO, Time modules
- [x] **Structs** - User-defined data types
- [x] **Arrays** - Dynamic arrays with push/pop
- [x] **Benchmark Suite** - Multi-language comparison
- [ ] **Garbage Collector** - Generational GC
- [ ] **Async/Await** - Coroutine-based concurrency
- [ ] **Module System** - Package management
- [ ] **Windows Support** - Cross-platform builds
- [ ] **REPL Improvements** - Better interactive mode

---

## 🤝 Contributing

Contributions are welcome! Please ensure:

1. **Zero Dependencies** - No external libraries
2. **Performance First** - Benchmark your changes
3. **No Underscores** - Follow naming conventions
4. **Test Coverage** - Add tests for new features

---

## 📄 License

MIT License - See [LICENSE](LICENSE) for details.

---

## 🔗 Links

- **Repository**: [github.com/gtkrshnaaa/nevaarize](https://github.com/gtkrshnaaa/nevaarize)
- **Issues**: [github.com/gtkrshnaaa/nevaarize/issues](https://github.com/gtkrshnaaa/nevaarize/issues)
- **Discussions**: [github.com/gtkrshnaaa/nevaarize/discussions](https://github.com/gtkrshnaaa/nevaarize/discussions)

---

<p align="center">
  <strong>Nevaarize</strong> — Native JIT Performance, Zero Compromise
</p>
