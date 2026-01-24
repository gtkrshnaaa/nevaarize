# Phase 2: AI Core

## Priority: HIGH
## Target: Native tensor operations for AI/ML

---

## Overview

AI engineering requires efficient tensor (multi-dimensional array) operations. This phase adds native support for matrices and tensors with JIT-compiled operations.

---

## Components

### 2.1 Tensor Type
```nva
// Target syntax
t = Tensor([2, 3, 4])           // 2x3x4 tensor
m = Matrix(1024, 1024, Float32) // 1024x1024 float matrix
v = Vector(1000, Int64)         // 1D vector
```

### 2.2 Matrix Operations
```nva
C = A @ B          // Matrix multiply (JIT-compiled SIMD)
D = A + B          // Element-wise add
E = A * 2.5        // Scalar multiply
F = A.T            // Transpose
```

### 2.3 Activation Functions
```nva
// All JIT-inlined for zero overhead
y = relu(x)        // max(0, x)
y = sigmoid(x)     // 1 / (1 + exp(-x))
y = tanh(x)        // Native tanh
y = softmax(x)     // Normalized exponentials
```

---

## Implementation Plan

### Files to Create

```
core/include/Tensor.hpp    - Tensor data structure
core/src/Tensor.cpp        - Tensor implementation
core/include/MatMul.hpp    - Matrix multiply algorithms
core/src/MatMul.cpp        - SIMD + cache-blocked matmul
stdlib/include/Neural.hpp  - Activation functions
stdlib/src/Neural.cpp      - JIT-inlined activations
```

### 2.4 Cache-Blocked Matrix Multiply

```
Standard:  O(n³) random memory access
Blocked:   O(n³) but cache-friendly

Block size = L1 cache size / 3 (for A, B, C blocks)
Typically 32x32 or 64x64 blocks
```

```cpp
void matmul_blocked(float* C, const float* A, const float* B,
                    int M, int N, int K, int blockSize) {
    for (int i = 0; i < M; i += blockSize) {
        for (int j = 0; j < N; j += blockSize) {
            for (int k = 0; k < K; k += blockSize) {
                // Compute block C[i:i+bs, j:j+bs] += A[i:i+bs, k:k+bs] @ B[k:k+bs, j:j+bs]
                matmul_block_simd(...);
            }
        }
    }
}
```

---

## Performance Targets

| Operation | Size | Target | Notes |
|-----------|------|--------|-------|
| MatMul | 1024×1024 | 100 GFLOPS | With AVX2 |
| Element Add | 1M elements | 50 GB/s | Memory bound |
| ReLU | 1M elements | 200 GB/s | Trivial compute |
| Softmax | 1000 classes | < 1ms | For inference |

---

## Memory Layout

```
Tensor {
    float* data;          // Contiguous, aligned to 32 bytes
    int64_t* shape;       // [dim0, dim1, dim2, ...]
    int64_t* strides;     // For efficient indexing
    int ndim;
    size_t size;          // Total elements
    bool ownsData;        // For memory management
}
```

### Memory Alignment
- All tensors 32-byte aligned (AVX2) or 64-byte aligned (AVX-512)
- Enables zero-copy SIMD loads

---

## Success Criteria

- [ ] Tensor type with shape support
- [ ] Matrix multiply with SIMD
- [ ] Cache-blocked matmul for large matrices
- [ ] ReLU, Sigmoid inlined in JIT
- [ ] Benchmark: 50+ GFLOPS on matmul
