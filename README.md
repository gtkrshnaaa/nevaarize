<p align="center">
  <h1 align="center">🚀 NEVAARIZE</h1>
  <p align="center">
    <strong>Native JIT Performance • Zero Dependencies • Pure C++23</strong>
  </p>
  <p align="center">
    <a href="#benchmarks"><img src="https://img.shields.io/badge/Array_Push-775M_ops%2Fs-brightgreen?style=flat-square" alt="Array Push"></a>
    <a href="#benchmarks"><img src="https://img.shields.io/badge/Integer_Ops-750M_ops%2Fs-blue?style=flat-square" alt="Integer Ops"></a>
    <a href="#benchmarks"><img src="https://img.shields.io/badge/Memory-5.9MB_Peak-orange?style=flat-square" alt="Memory"></a>
    <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-yellow?style=flat-square" alt="License"></a>
  </p>
</p>

---

**Nevaarize** is a high-performance programming language with a **native JIT compiler** that generates x86-64 machine code at runtime. Built entirely in C++23 with zero external dependencies.

## ⚡ Performance Highlights

Nevaarize delivers **exceptional performance** in key areas:

| Category | Performance | vs Python | vs Node.js |
|----------|------------|-----------|------------|
| **Array Push** | **750M ops/sec** | 🏆 **32x faster** | 🏆 **36x faster** |
| **Integer Ops** | **752M ops/sec** | 17x faster | 0.64x |
| **Struct Access** | **453M ops/sec** | 15x faster | 0.45x |
| **Memory Usage** | **5.9 MB** | 🏆 **8x less** | 🏆 **14x less** |

> 💡 **Key Achievement**: Nevaarize beats C++ and Go in Array Push operations while using the least memory of all tested languages!

---

## 📊 Benchmarks

All benchmarks are **reproducible** and included in this repository. Run them yourself:

```bash
cd languagebench
./run_comparison.sh
```

### Full Benchmark Results (11 Languages)

```
================================================================================
                   ULTIMATE LANGUAGE BENCHMARK BATTLE
================================================================================
System: Intel i5-1135G7 @ 2.40GHz | Ubuntu 24.04 | 8GB RAM

┌─────────────────────────────────────────────────────────────────────────────┐
│  NEVAARIZE (JIT Compiled - x86-64 Native)                                   │
├─────────────────┬──────────────────┬─────────────────────────────────────────┤
│  Integer Add    │ 752,020,502 ops/sec │ 1.33s                               │
│  Double Arith   │ 445,531,330 ops/sec │ 0.22s                               │
│  String Concat  │  34,572,671 ops/sec │ 0.001s                              │
│  Array Push     │ 750,835,304 ops/sec │ 0.001s  ← #1 of ALL languages!      │
│  Struct Access  │ 453,322,875 ops/sec │ 0.11s                               │
├─────────────────┴──────────────────┴─────────────────────────────────────────┤
│  Peak RAM: 5,920 KB (LOWEST) │ Total Time: 1.66s                            │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Comparison Table

| Language | Integer Add | Array Push | Struct Access | Peak RAM |
|----------|------------|------------|---------------|----------|
| **Nevaarize** | 752M | **751M** 🥇 | 453M | **5.9MB** 🥇 |
| Go | 3,516M | 69M | 3,627M | 46MB |
| Java 21 | 3,342M | 40M | 901M | 356MB |
| PHP 8 JIT | 2,961M | 71M | 384M | 50MB |
| C++ (GCC -O3) | 2,315M | 328M | 2,360M | 11.5MB |
| C (GCC -O3) | 2,440M | 411M | 2,327M | 9.4MB |
| Node.js (V8) | 1,182M | 24M | 942M | 83MB |
| LuaJIT | 940M | 207M | 962M | 12MB |
| Lua 5.4 | 195M | 60M | 88M | 20MB |
| PHP 8 Std | 419M | 54M | 77M | 46MB |
| Python 3 | 45M | 26M | 32M | 49MB |

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
