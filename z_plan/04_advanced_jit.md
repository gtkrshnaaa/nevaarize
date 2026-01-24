# Phase 4: Advanced JIT Optimizations

## Priority: MEDIUM
## Target: Maximize JIT performance with advanced optimizations

---

## Overview

Take JIT from 505M ops/sec to multi-billion ops/sec with advanced compiler optimizations.

---

## Optimization Pipeline

```
Nevaarize Source
      ↓
    Parser
      ↓
     AST
      ↓
  IR Generator ← (Phase 4.1: SSA Form)
      ↓
  Optimizer    ← (Phase 4.2: Optimization Passes)
      ↓
 Register Alloc ← (Phase 4.3: Linear Scan)
      ↓
  Code Gen     ← (Phase 4.4: Pattern Matching)
      ↓
 Native Code
```

---

## 4.1 SSA Form (Static Single Assignment)

Convert IR to SSA for easier optimization:
```
Before:          After SSA:
x = 1            x1 = 1
x = x + 2        x2 = x1 + 2
x = x * 3        x3 = x2 * 3
```

### Benefits
- Easier constant propagation
- Dead code elimination
- Common subexpression elimination

---

## 4.2 Optimization Passes

### Constant Folding
```nva
x = 2 + 3     →     x = 5
```

### Dead Code Elimination
```nva
x = expensive()
// x never used
// → Remove the call
```

### Loop-Invariant Code Motion
```nva
for i in Range(0, n) {
    y = a + b          // Moved outside loop
    sum = sum + y * i
}
```

### Strength Reduction
```nva
x * 2     →     x << 1
x * 8     →     x << 3
x / 4     →     x >> 2
```

### Function Inlining
```nva
func add(a, b) { return a + b }
x = add(1, 2)
// Becomes:
x = 1 + 2
```

---

## 4.3 Register Allocation

### Current: Simple allocation
### Target: Linear Scan Register Allocation

```
Variables: a, b, c, d, e
Registers: RAX, RCX, RDX, R8, R9

Live ranges:
a: [0--5]      → RAX
b: [2----8]    → RCX  
c: [3--6]      → RDX
d: [7----10]   → RAX (reuse after 'a' dies)
e: [4----9]    → R8
```

---

## 4.4 Peephole Optimization

Pattern matching on generated code:
```asm
; Before:
mov rax, rcx
mov rcx, rax
; After:
mov rax, rcx       ; Second instruction eliminated
```

---

## Performance Impact

| Optimization | Expected Gain |
|--------------|---------------|
| Constant Folding | 5-10% |
| DCE | 5-15% |
| Inlining | 20-50% |
| LICM | 10-30% |
| Better RegAlloc | 10-20% |

---

## Success Criteria

- [ ] SSA form IR
- [ ] Constant folding pass
- [ ] Dead code elimination
- [ ] Loop-invariant code motion
- [ ] Function inlining for small functions
- [ ] Linear scan register allocation
- [ ] Benchmark improvement: 2-3x
