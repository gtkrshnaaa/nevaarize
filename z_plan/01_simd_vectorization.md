# Phase 1: SIMD Vectorization

## Priority: CRITICAL
## Target: 8-16x performance boost on loops

---

## Overview

SIMD (Single Instruction, Multiple Data) allows processing multiple data elements in parallel using special CPU registers (YMM for AVX2, ZMM for AVX-512).

```
Without SIMD:  1 float × 1 = 1 operation/cycle
With AVX2:     8 floats × 1 = 8 operations/cycle
With AVX-512: 16 floats × 1 = 16 operations/cycle
```

---

## Implementation Plan

### 1.1 CPU Feature Detection
```cpp
// core/include/SIMD.hpp
namespace nevaarize {

enum class SIMDLevel {
    NONE,
    SSE4,
    AVX2,
    AVX512
};

SIMDLevel detectSIMD();

}
```

### 1.2 Vector Operations Header
```cpp
// core/include/VectorOps.hpp

// AVX2 intrinsics for 8 floats at once
void vec_add_f32(float* dst, const float* a, const float* b, size_t n);
void vec_mul_f32(float* dst, const float* a, const float* b, size_t n);
void vec_fma_f32(float* dst, const float* a, const float* b, const float* c, size_t n);

// For integers
void vec_add_i64(int64_t* dst, const int64_t* a, const int64_t* b, size_t n);
```

### 1.3 JIT SIMD Code Generation
Modify TrueJIT to emit SIMD instructions:

```asm
; Instead of:
;   add rax, rcx

; Generate:
;   vaddpd ymm0, ymm1, ymm2  ; 4 doubles at once
```

---

## Target Instructions

| Operation | Scalar | AVX2 (8 floats) | AVX-512 (16 floats) |
|-----------|--------|-----------------|---------------------|
| Add | `addss` | `vaddps ymm` | `vaddps zmm` |
| Multiply | `mulss` | `vmulps ymm` | `vmulps zmm` |
| FMA | N/A | `vfmadd132ps` | `vfmadd132ps` |
| Load | `movss` | `vmovaps ymm` | `vmovaps zmm` |

---

## Expected Performance

| Benchmark | Current | With AVX2 | With AVX-512 |
|-----------|---------|-----------|--------------|
| Sum Loop (100M) | 505M/s | 4B/s | 8B/s |
| Float Add | 1.8M/s | 14M/s | 28M/s |
| Matrix Multiply | N/A | 50 GFLOPS | 100 GFLOPS |

---

## Files to Create

1. `core/include/SIMD.hpp` - Detection & intrinsics wrapper
2. `core/src/SIMD.cpp` - Implementation
3. `core/include/VectorOps.hpp` - High-level vector operations
4. `core/src/VectorOps.cpp` - SIMD implementations

---

## Success Criteria

- [ ] AVX2 detection working
- [ ] SIMD loop adds 8 floats at once
- [ ] Benchmark shows 8x improvement
- [ ] Auto-vectorization for simple loops
