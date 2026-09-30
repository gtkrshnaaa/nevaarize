# Nevaarize JIT Architecture and Runtime Internals

## 1. Architectural Overview

Nevaarize compiles source code directly from an Abstract Syntax Tree (AST) into executable Linux x86-64 machine code at runtime. The compiler emits raw machine instructions into memory pages allocated via `mmap`, adjusts page permissions using `mprotect`, and executes compiled subroutines without intermediary bytecode or virtual machine interpreters.

```
+--------------------+      +--------------------+      +--------------------+
|  Nevaarize Source  | ---> |   Lexer & Parser   | ---> |        AST         |
+--------------------+      +--------------------+      +--------------------+
                                                                  |
                                                                  v
+--------------------+      +--------------------+      +--------------------+
| Native x86-64 Exec | <--- |  Executable Memory | <--- |    JIT Compiler    |
| (Direct CPU Run)   |      | (mprotect RX pages)|      |   (codeGen.cpp)    |
+--------------------+      +--------------------+      +--------------------+
```

---

## 2. Dual-Word Stack Frame Layout

All variables and expression intermediates in Nevaarize stack frames follow a dual-word 16-byte representation. Each variable slot contains:
1. **Value Word (`[rbp - offset]`)**: 64-bit payload containing a signed integer, an IEEE-754 double-precision float bitcast, or a heap pointer to a heap-allocated structure.
2. **Type Tag Word (`[rbp - offset + 8]`)**: 64-bit integer tag defining the active runtime type.

### Type Tag Reference

| Type Tag ID | Type Name | Value Representation |
|:---|:---|:---|
| `0` | `INT` | 64-bit signed integer |
| `1` | `FLOAT` | 64-bit IEEE-754 double precision bits |
| `2` | `BOOL` | `0` (false) or `1` (true) |
| `3` | `NIL` | `0` (null reference) |
| `4` | `STRING` | Pointer to `JITString` data buffer |
| `5` | `ARRAY` | Pointer to `JITArray` elements buffer |
| `6` | `MAP` | Pointer to `JITMap` instance |
| `7` | `STRUCT` | Pointer to `JITStruct` instance |
| `8` | `FUNCTION`| Pointer to compiled function subroutine |
| `9` | `FUTURE` | Pointer to `JITFuture` async task handle |

### Stack Frame Structure

```
                  +-------------------------------+
                  |  Return Address               |  [rbp + 8]
                  +-------------------------------+
       RBP ---->  |  Saved Caller RBP             |  [rbp + 0]
                  +-------------------------------+
                  |  Slot 0: Value (64 bits)      |  [rbp - 8]
                  |  Slot 0: Type Tag (64 bits)   |  [rbp - 16]
                  +-------------------------------+
                  |  Slot 1: Value (64 bits)      |  [rbp - 24]
                  |  Slot 1: Type Tag (64 bits)   |  [rbp - 32]
                  +-------------------------------+
                  |  ... Local Variable Slots ... |
                  +-------------------------------+
       RSP ---->  |  Temporary Spill / Arg Area   |  [Aligned to 16 bytes]
                  +-------------------------------+
```

The function prologue emits:
```assembly
push rbp
mov  rbp, rsp
sub  rsp, <stack_size_placeholder>
```
The stack allocation size is computed during AST compilation and backpatched into the prologue operand once all local variables and temporary expression slots have been resolved.

---

## 3. Register Allocator & Loop Optimizations

The JIT compiler manages general-purpose registers (GPRs) and Streaming SIMD Extensions registers (XMM0 through XMM7).

### Register Allocator Strategy
- Available scratch GPRs: `RAX`, `RCX`, `RDX`, `RSI`, `RDI`, `R8`, `R9`, `R10`, `R11`.
- Callee-preserved GPRs: `RBX`, `R12`, `R13`, `R14`, `R15`.
- Call ABI: Follows System V AMD64 ABI convention. Integer and pointer arguments are mapped to `RDI`, `RSI`, `RDX`, `RCX`, `R8`, `R9`. Floating-point arguments are passed in `XMM0` through `XMM7`.

### Loop Optimizations
1. **GPR Register Pinning**: Hot loop counters and induction variables are analyzed before loop code generation. The most frequently accessed integer variables are pinned to dedicated GPRs (e.g. `R12`, `R13`) throughout the loop body, eliminating stack reads and writes.
2. **XMM Float Pinning**: Floating-point loop accumulators and frequently updated float variables are pinned to `XMM` registers (`XMM1` through `XMM7`), keeping computations in hardware vector registers.
3. **Constant Hoisting**: Floating-point literals referenced within loops are preloaded into `XMM` registers outside the loop pre-header, preventing redundant memory lookups.
4. **Shadow Accumulators**: For reduction loops, independent shadow accumulators are allocated to break loop carry dependency chains, enabling out-of-order execution pipelines.

---

## 4. Garbage Collection Integration

Nevaarize employs a generational, mark-sweep garbage collector designed for zero-overhead JIT execution.

### Memory Organization
- **Young Generation (`MemoryRegion`, 1 MB)**: Fast bump-pointer allocation arena where short-lived temporary strings, arrays, and map nodes are allocated.
- **Old Generation (`MemoryRegion`, 16 MB)**: Long-lived objects surviving collections.
- **Shared Generation (`SharedMemoryRegion`, 64 MB)**: Thread-safe arena for cross-thread asynchronous task communication. Objects allocated here are immortal (`hdr->marked = 1`).

### Conservative Stack Root Scanning
Because JIT-compiled machine code does not emit explicit GC safepoint metadata tables, the garbage collector performs conservative stack scanning:

1. **Stack Boundary Registration**: Before calling compiled code, the runtime records the top of the stack:
   ```cpp
   jitGC.setStackTop(__builtin_frame_address(0));
   ```
2. **CPU Register Preservation**: During collection, `setjmp` spills active caller and callee CPU registers to the stack:
   ```cpp
   jmp_buf cpuRegs;
   setjmp(cpuRegs);
   ```
3. **Stack Walking**: The GC inspects memory between current `RSP` and `stackTop`. Every aligned 8-byte value is tested against the bounds of known GC memory arenas (`contains(candidate)`).
4. **Transitive Marking**: Valid heap pointers resolve to enclosing `GCHeader` metadata headers. Reachable objects are marked, and their internal reference pointers (array elements, map keys and values) are traced recursively.
5. **Sweep**: Unmarked young generation objects are reclaimed by resetting the bump pointer or sweeping dead headers.

---

## 5. Hardware Trap Recovery & Signal Handling

To provide reliable language-level exception handling for memory faults and mathematical anomalies, the runtime installs POSIX signal handlers on an alternate signal stack (`sigaltstack`):

- `SIGSEGV`: Catches invalid memory dereferences, out-of-bounds heap indexing, and null pointer traversals.
- `SIGFPE`: Catches integer division by zero and floating-point exceptions.

### Safe Transition Protocol (`JITExecutionGuard`)
When JIT code faults, the signal handler performs `siglongjmp` back to the execution boundary, converting the hardware fault into a Nevaarize runtime exception.

To ensure foreign C++ runtime allocations and RAII destructors are never bypassed during JIT-to-runtime transitions, external FFI and standard library calls are wrapped in `JITExecutionGuard`:

```cpp
struct JITExecutionGuard {
    bool was_in_jit;
    JITExecutionGuard() {
        was_in_jit = in_jit_execution;
        in_jit_execution = false;
    }
    ~JITExecutionGuard() {
        in_jit_execution = was_in_jit;
    }
};
```
Inside standard library subroutines, `in_jit_execution` is disabled, allowing native C++ exceptions and operating system signals to follow standard handler unwinding paths without stack corruption.

---

## 6. Interactive REPL Execution Model

In REPL mode, Nevaarize maintains execution state across interactive turns:
1. **Persistent Symbol Table**: Global variables declared in previous evaluations are preserved in `replVariables`.
2. **State Synchronization**: On each evaluation turn, prologue code reloads persistent variables into their designated stack slots, and epilogue code writes modified values back into the persistent symbol map.
3. **Memory Write Unprotection**: Compiled executable pages are kept writable (`PROT_READ | PROT_WRITE`) during compilation and code generation, then toggled to executable (`PROT_READ | PROT_EXEC`) prior to CPU invocation.
4. **AST Retention**: User function definitions are retained across turns via `retainReplAST()` to prevent dangling references in JIT function descriptors.
