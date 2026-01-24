# NEVAARIZE

> **"Native JIT Performance, Zero Dependencies, Pure C++23"**

**Nevaarize** is a high-performance programming language built from the ground up with a **native JIT compiler** at its core. Written entirely in C++23 with zero external dependencies, Nevaarize delivers blazing-fast execution through direct machine code generation.

---

## Philosophy

Nevaarize is engineered for **absolute speed** without compromise:

- **Native JIT Compilation**: Direct x86-64 machine code generation at runtime
- **Zero Dependencies**: Pure C++23 implementation, no LLVM, no external libraries
- **Optimal Algorithms**: State-of-the-art compiler techniques from day one
- **Cache-Friendly Design**: Memory layouts optimized for modern CPU architectures
- **Aesthetic Purity**: Clean syntax enforced through the **No-Underscore Policy**

---

## Architecture

### JIT Compilation Pipeline

```
Source Code (.nva)
    ↓
[Lexer] → Zero-copy tokenization
    ↓
[Parser] → Flattened AST (cache-optimized)
    ↓
[IR Generator] → Platform-independent intermediate representation
    ↓
[JIT Compiler] → Native x86-64 machine code
    ↓
[Direct Execution] → No interpretation overhead
```

### Core Components

- **Lexer**: SIMD-accelerated tokenization with zero-copy string views
- **Parser**: Recursive descent with operator precedence climbing
- **IR Layer**: SSA-based intermediate representation for optimization
- **Code Generator**: Direct x86-64 assembly emission with register allocation
- **Runtime**: Custom memory allocator with generational garbage collection
- **Optimizer**: Inline caching, constant folding, dead code elimination

---

## Performance Goals

Nevaarize targets **native C++ performance** for computational workloads:

- **Integer Operations**: 1+ billion ops/sec (CPU-bound)
- **Floating Point**: Near-native SIMD utilization
- **Function Calls**: Zero-overhead inline caching
- **Memory Access**: Cache-line aligned allocations
- **Startup Time**: Sub-millisecond JIT compilation for hot paths

---

## Language Features

### Dynamic Typing with JIT Specialization

```nva
// Type-specialized machine code generated at runtime
x = 42              // JIT: int64_t fast path
y = 3.14            // JIT: double fast path
z = x + y           // JIT: optimized mixed-type arithmetic
```

### First-Class Functions

```nva
func add(a, b) {
    return a + b
}

func apply(fn, x, y) {
    return fn(x, y)
}

result = apply(add, 10, 20)  // JIT: inline cached dispatch
```

### Structs with Inline Caching

```nva
struct Vector3 {
    x
    y
    z
}

v = Vector3()
v.x = 10            // JIT: direct memory offset, no lookup
v.y = 20
v.z = 30
```

### Arrays with Specialized Operations

```nva
arr = [1, 2, 3, 4, 5]
arr.push(6)         // JIT: type-specialized array growth
sum = 0
for (val in arr) {
    sum = sum + val // JIT: unrolled loop with SIMD
}
```

### Async/Await (Future)

```nva
async func fetchData(url) {
    response = await httpGet(url)
    return parseJson(response)
}

data = await fetchData("https://api.example.com")
```

---

## Path Resolution Rules

> **CRITICAL:** All file paths in Nevaarize are resolved **relative to the source file**, not the working directory.

This is identical to `#include` in C++ and `<a href>` in HTML.

```nva
// File: /project/src/main.nva
import "utils.nva" as utils           // → /project/src/utils.nva
import "lib/helper.nva" as helper     // → /project/src/lib/helper.nva
import "../config.nva" as config      // → /project/config.nva
```

**Working Directory Independence:**
```bash
# All produce the SAME result:
cd /project && nevaarize src/main.nva
cd /project/src && nevaarize main.nva
```

In both cases, `import "utils.nva"` resolves to `/project/src/utils.nva`.

See [Language Reference](docs/mddocs/languageReference.md#path-resolution-rules) for complete details.

---

## Standard Library

### Built-in Functions

| Function | Description | JIT Optimization |
|----------|-------------|------------------|
| `print(...)` | Output to stdout | Buffered I/O |
| `Range(start, end)` | Generate sequences | Lazy evaluation |
| `len(obj)` | Get length | Constant-time field access |
| `type(val)` | Runtime type | Tag bit inspection |
| `str(val)` | String conversion | Inline formatting |
| `int(val)` | Integer conversion | SIMD-accelerated parsing |
| `float(val)` | Float conversion | Fast float parsing |

### Math Module

```nva
import stdlib math as m

result = m.Sqrt(16)     // JIT: inline sqrtsd instruction
angle = m.Sin(3.14159)  // JIT: SIMD trigonometry
power = m.Pow(2, 10)    // JIT: optimized exponentiation
```

**Functions**: `Abs`, `Sqrt`, `Pow`, `Floor`, `Ceil`, `Sin`, `Cos`, `Tan`, `Log`, `Exp`

### IO Module

```nva
import stdlib io as io

name = io.Input("Enter name: ")
io.Print("Hello,", name)
io.Write("Loading...")  // No newline
```

### Time Module

```nva
import stdlib time as t

start = t.clock()
// ... computation ...
elapsed = t.clock() - start
t.sleep(1000)  // Sleep 1 second
```

---

## Build & Installation

### Prerequisites

- **C++23 Compiler**: GCC 13+ or Clang 16+
- **x86-64 CPU**: SSE4.2+ required, AVX2 recommended
- **Linux/macOS**: Primary platforms (Windows support planned)

### Quick Start

```bash
# Clone repository
git clone https://github.com/yourusername/nevaarize.git
cd nevaarize

# Build release version
make

# Install globally
sudo make install

# Run REPL
nevaarize

# Execute script
nevaarize examples/basics/helloWorld.nva

# Run benchmarks
make benchmark
```

### Build Targets

```bash
make                # Build optimized JIT compiler
make debug          # Build with debug symbols
make benchmark      # Run performance benchmarks
make test           # Run test suite
make clean          # Remove build artifacts
make install        # Install to /usr/local/bin
make uninstall      # Remove from system
```

---

## Project Structure

```
nevaarize/
├── core/                       # Core JIT compiler
│   ├── include/
│   │   ├── Lexer.hpp          # Tokenizer
│   │   ├── Parser.hpp         # AST builder
│   │   ├── AST.hpp            # Abstract syntax tree
│   │   ├── IR.hpp             # Intermediate representation
│   │   ├── JIT.hpp            # JIT compiler interface
│   │   ├── CodeGen.hpp        # x86-64 code generator
│   │   ├── Optimizer.hpp      # IR optimization passes
│   │   ├── Runtime.hpp        # Runtime system
│   │   ├── GC.hpp             # Garbage collector
│   │   └── Value.hpp          # Runtime value representation
│   └── src/
│       ├── Lexer.cpp
│       ├── Parser.cpp
│       ├── IR.cpp
│       ├── JIT.cpp
│       ├── CodeGen.cpp        # Native code emission
│       ├── Optimizer.cpp
│       ├── Runtime.cpp
│       ├── GC.cpp
│       └── Main.cpp           # Entry point & REPL
├── stdlib/                     # Standard library
│   ├── include/
│   │   ├── Math.hpp
│   │   ├── IO.hpp
│   │   └── Time.hpp
│   └── src/
│       ├── Math.cpp
│       ├── IO.cpp
│       └── Time.cpp
├── examples/                   # Example programs
│   ├── basics/
│   ├── controlflow/
│   ├── functions/
│   ├── datastructures/
│   ├── algorithms/
│   ├── benchmarks/
│   └── modules/
├── docs/                       # Documentation
│   ├── mddocs/                # Markdown documentation
│   │   ├── getting-started.md
│   │   ├── language-reference.md
│   │   ├── jit-internals.md
│   │   ├── stdlib-reference.md
│   │   └── performance-guide.md
│   ├── index.html             # HTML documentation
│   ├── getting-started.html
│   ├── language-reference.html
│   ├── jit-internals.html
│   ├── stdlib-reference.html
│   └── performance-guide.html
├── tests/                      # Test suite
│   ├── unit/
│   ├── integration/
│   └── benchmark/
├── Makefile                    # Build system
├── README.md                   # This file
└── LICENSE                     # MIT License
```

---

## Aesthetic Principles

### No-Underscore Policy

Nevaarize enforces **Aesthetic Purity** through strict identifier rules. Underscores `_` are **forbidden** in all user-defined names to maintain clean, consistent code.

```nva
// ❌ REJECTED at compile-time
my_variable = 10
func calculate_sum(a, b) { }

// ✅ ACCEPTED
myVariable = 10
func calculateSum(a, b) { }
```

This policy ensures:
- **Visual Consistency**: All code follows camelCase convention
- **Readability**: No mixing of naming styles
- **Professionalism**: Clean, modern aesthetic

---

## JIT Implementation Details

### Register Allocation

- **Algorithm**: Linear scan with second-chance binpacking
- **Registers**: Full x86-64 register set (rax-r15, xmm0-xmm15)
- **Spilling**: Stack-based with minimal overhead

### Inline Caching

- **Polymorphic Inline Caches**: Up to 4 type specializations per call site
- **Megamorphic Fallback**: Hash table dispatch for >4 types
- **Invalidation**: Lazy deoptimization on type shape changes

### Garbage Collection

- **Algorithm**: Generational copying collector
- **Young Generation**: Bump-pointer allocation, fast collection
- **Old Generation**: Mark-compact for long-lived objects
- **Write Barriers**: Card-marking for cross-generation references

### SIMD Optimization

- **Auto-vectorization**: Loop unrolling with SSE4.2/AVX2
- **Packed Operations**: Batch arithmetic on arrays
- **Alignment**: 16/32-byte aligned allocations

---

## Benchmarks (Target Performance)

| Benchmark | Nevaarize (JIT) | Python 3.11 | Node.js V8 | Target |
|-----------|-----------------|-------------|------------|--------|
| Integer Loop | TBD | 1x | 50x | **100x** |
| Float Math | TBD | 1x | 40x | **80x** |
| Function Calls | TBD | 1x | 30x | **60x** |
| Array Operations | TBD | 1x | 20x | **50x** |
| Struct Access | TBD | 1x | 25x | **70x** |

*Baseline: Python 3.11 = 1x*

---

## Roadmap

### Phase 1: Foundation (Current)
- [x] Project structure
- [x] README and documentation plan
- [ ] Lexer implementation
- [ ] Parser implementation
- [ ] Basic AST

### Phase 2: JIT Core
- [ ] IR design and generation
- [ ] x86-64 code generator
- [ ] Register allocator
- [ ] Basic runtime system

### Phase 3: Optimization
- [ ] Inline caching
- [ ] Type specialization
- [ ] SIMD vectorization
- [ ] Garbage collector

### Phase 4: Standard Library
- [ ] Built-in functions
- [ ] Math module
- [ ] IO module
- [ ] Time module

### Phase 5: Advanced Features
- [ ] Async/await with coroutines
- [ ] Module system
- [ ] Debugging support
- [ ] Profiler integration

---

## Contributing

Nevaarize is built for **performance excellence**. Contributions should:

1. **Maintain Zero Dependencies**: No external libraries
2. **Optimize for Speed**: Profile before and after changes
3. **Follow Aesthetic Policy**: No underscores in identifiers
4. **Document JIT Impact**: Explain code generation implications
5. **Benchmark**: Include performance measurements

---

## License

MIT License - See LICENSE file for details

---

## Contact

- **GitHub**: [github.com/yourusername/nevaarize](https://github.com/yourusername/nevaarize)
- **Issues**: [github.com/yourusername/nevaarize/issues](https://github.com/yourusername/nevaarize/issues)
- **Discussions**: [github.com/yourusername/nevaarize/discussions](https://github.com/yourusername/nevaarize/discussions)

---

**Nevaarize** - *Native JIT Performance, Zero Compromise*
