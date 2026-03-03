# Nevaarize 2.0: The Path to 1.5 Billion Ops/Sec
**Status:** DRAFT | **Target:** Monster Speed (>1.5B/s) | **Stack:** Pure C++23 (No external deps)

## Executive Summary
Current Nevaarize JIT (~870M ops/sec) operates as a **Single-Pass AST-to-MachineCode** compiler. This architecture hits a hard ceiling because it cannot perform global analysis (e.g., keeping a variable in a register across an entire function or loop).

To breach the **1.5 Billion ops/sec** barrier (equivalent to GCC `-O2`/`-O3`), we must transition to a **Multi-Pass Architecture** centered around **Intermediate Representation (IR)** with **Static Single Assignment (SSA)** form and **Global Register Allocation**.

---

## Architectural Evolution

### Current Architecture (v0.1.8)
`AST -> CodeGen (Single Pass) -> Linux x86-64 Machine Code`
*   **Pros:** Compiles instantly, simple codebase.
*   **Cons:** Stack-based (variables live in memory), redundant loads/stores, limited optimization scope.
*   **Limit:** ~900M ops/sec (L1 Cache latency bottleneck).

### New Architecture (v0.2 "Monster")
`AST -> IR Generator -> SSA Construction -> Optimization Passes -> Register Allocation -> Code Emission`
*   **Pros:** Variables live in registers (CPU Native speed), loop invariant code motion, zero-cost abstractions.
*   **Cons:** Higher compilation latency, significantly more complex codebase (~2000+ LOC).
*   **Target:** >1.5B ops/sec (Sub-nanosecond operations).

---

## Core Components Roadmap

### 1. The Intermediate Representation (IR)
We need a linear, instruction-based IR (not tree-based).
*   **Format**: 3-Address Code (e.g., `v3 = ADD v1, v2`).
*   **Structure**: Basic Blocks (sequences of instructions with single entry/exit) linked by Control Flow Graph (CFG).
*   **Registers**: Infinite Virtual Registers (vRegs).

```cpp
// Example IR Structure
struct Instruction {
    OpCode op;
    int dest; // vReg
    int src1; // vReg
    int src2; // vReg / Immediate
};

struct BasicBlock {
    int id;
    std::vector<Instruction> instrs;
    std::vector<BasicBlock*> successors;
    std::vector<BasicBlock*> predecessors;
};
```

### 2. Static Single Assignment (SSA)
Variables must be versioned (`x1`, `x2`...) so each is assigned exactly once.
*   **Phi Functions**: Handle control flow merges (e.g., `x3 = PHI(x1, x2)` at loop heads).
*   **Benefit**: Enables powerful optimizations like Constant Propagation and Dead Code Elimination effortlessly.

### 3. Optimization Passes
Once in SSA/IR, we run passes before generating code:
*   **DCE (Dead Code Elimination)**: Remove instructions whose results are unused.
*   **LICM (Loop Invariant Code Motion)**: Move constants calculation out of loops.
*   **GVN (Global Value Numbering)**: Eliminate redundant calculations (`a+b` computed once).
*   **Inlining**: Inline small functions to remove call overhead.

### 4. Global Register Allocation
The most critical component for speed.
*   **Goal**: Map infinite `vRegs` to finite physical `x64 Regs` (RAX, RBX, etc.).
*   **Algorithm**: **Linear Scan Allocator** (Fast JIT standard) or **Graph Coloring** (Optimized standard).
    *   *Recommendation*: **Linear Scan** with Lifetime Intervals is sufficient for JIT and much faster to compile.
*   **Spilling**: If out of registers, intelligently spill least-used variables to stack.

### 5. Code Emission (Backend)
Translates optimized LIR (Low-Level IR) to machine bytes.
*   Much simpler than current JIT.cpp because decisions (Reg vs Stack) are already made by the Allocator.
*   Focuses purely on encoding instructions (`MOV`, `ADD`, `JMP`).

---

## Implementation Plan

### Phase 1: IR & Basic Blocks Framework
- [ ] Define `IR.hpp` (Instructions, Opcodes).
- [ ] Implement `CFGBuilder` (Construct Basic Blocks from AST).
- [ ] Visualization tool (Dump IR to text for debugging).

### Phase 2: SSA Construction
- [ ] Implement Dominator Tree analysis.
- [ ] Insert Phi nodes.
- [ ] Variable Renaming algorithm.

### Phase 3: Codegen from IR
- [ ] Implement a naive code generator (1 vReg = 1 Stack Slot).
- [ ] Verify correctness (Run existing benchmarks). *Performance will be low initially.*

### Phase 4: Register Allocation (The Speedup)
- [ ] Implement **Liveness Analysis** (Calculate live ranges of variables).
- [ ] Implement **Linear Scan Allocator**.
- [ ] Integrate with CodeGen.
- [ ] **Milestone**: Speed should jump from ~400M to >1.5B here.

### Phase 5: Optimization Passes
- [ ] Implement LICM (Crucial for loops).
- [ ] Implement Constant Folding.

---

## Risk Assessment
*   **Complexity**: High. Debugging JIT bugs involving Register Allocation is notoriously difficult (e.g. Heisenbugs).
*   **Compile Time**: Will increase. Is it acceptable for a script engine? (Yes, if runtime speed is paramount).
*   **Maintenance**: Requires rigorous unit testing of the IR layer.

## Conclusion
This plan transitions Nevaarize from a "Toy JIT" to a **Production-Grade Compiler Engine**. It adheres to the "Pure C++" philosophy but adopts standard compiler engineering principles used by V8, LuaJIT, and JVM.
