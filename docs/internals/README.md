# Nevaarize Runtime Internals & Contributor Guide

## Overview

This directory contains technical documentation describing the architecture, execution engine, compiler pipeline, and garbage collector of the Nevaarize programming language runtime.

## Table of Contents

- [JIT Architecture & Stack Conventions](./jit-architecture.md): Deep dive into x86-64 machine code generation, dual-word stack frames, register allocation, hardware trap recovery, and conservative GC stack root scanning.
- [Language Grammar & Lexical Specification](../spec/grammar-syntax.md): Lexical conventions, operator precedence, and statement grammar rules.
- [Type System & Heap Object Layouts](../spec/type-system.md): In-memory representations of numbers, strings, arrays, maps, and structures.
- [Concurrency & Thread Safety Model](../spec/concurrency.md): Thread pools, async tasks, and cross-thread heap isolation.

---

## Contributor Guide

### 1. Build and Test Prerequisites

All development workflows and tests must be executed inside the containerized Docker sandbox to guarantee environment reproducibility:

```bash
# Execute quarantine test suite
./test.sh quarantine

# Execute full test suite
./test.sh all

# Build native compiler binary
./test.sh build
```

### 2. Code Organization

- `core/include/`: C++23 header definitions for lexer, parser, AST, JIT compiler, code generator, and GC.
- `core/src/`: Core implementation files (`jit.cpp`, `codeGen.cpp`, `lexer.cpp`, `parser.cpp`, `gc.cpp`, `main.cpp`).
- `core/stdlib/`: Native C++ implementations of the standard library (`ai.cpp`, `math.cpp`, `time.cpp`, `io.cpp`, `http.cpp`, `json.cpp`, `csv.cpp`, `claw.cpp`, `string.cpp`).
- `docs/`: Comprehensive language specification, standard library manuals, and internal architecture guides.
- `test/quarantine/`: Automated integration test scripts verifying interpreter and JIT behavior.

### 3. JIT Contribution Guidelines

When adding new operations or standard library bridges to the JIT engine:

1. **Maintain 16-Byte Stack Alignment**: The System V AMD64 ABI requires `RSP` to be 16-byte aligned before any `call` instruction.
2. **Preserve Dual-Word Stack Representation**: Always keep the 64-bit value at `[rbp - offset]` and the 64-bit type tag at `[rbp - offset + 8]`.
3. **Register FFI Bridges via `JITExecutionGuard`**: Protect standard library functions from bypassing C++ destructors on unexpected hardware faults.
4. **Register GC Roots**: When allocating heap memory in JIT runtime bridges, ensure pointers are visible to the stack scanner or registered via `jitGC.addRoot()`.
