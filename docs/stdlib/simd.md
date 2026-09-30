# Hardware Acceleration & SIMD Architecture: `simd`

Nevaarize incorporates native SIMD vectorization routines implemented in C++23. Vector operations dynamically detect host hardware capabilities (`CPUID`) and route execution to AVX-512 (512-bit registers, 8 double-precision floats per cycle), AVX2 + FMA (256-bit registers, 4 double-precision floats per cycle), or scalar fallback.

## 1. Array Vector Methods

Dynamic arrays containing numeric elements expose high-performance native vector methods:

```nva
arr = [1.0, 2.5, 3.0, 4.5, 5.0, 6.5, 7.0, 8.5]

// Reductions
total = arr.sum()
average = arr.mean()
maximum = arr.max()
minimum = arr.min()

// Arg reductions
maxIdx = arr.argmax()
minIdx = arr.argmin()

// Geometry & Linear Algebra
magnitude = arr.norm()
length = arr.shape()
```

### Method Summary

| Method | Return Type | Description | Optimization Path |
|:---|:---|:---|:---|
| `arr.sum()` | `float` / `int` | Computes scalar sum of all elements | AVX2 / AVX-512 vector accumulation |
| `arr.mean()` | `float` | Computes arithmetic average | AVX vector sum with reciprocal scaling |
| `arr.max()` | `float` / `int` | Finds maximum element value | `_mm256_max_pd` / `_mm512_max_pd` |
| `arr.min()` | `float` / `int` | Finds minimum element value | `_mm256_min_pd` / `_mm512_min_pd` |
| `arr.argmax()` | `int` | Returns zero-based index of maximum element | Vector comparison with horizontal reduction |
| `arr.argmin()` | `int` | Returns zero-based index of minimum element | Vector comparison with horizontal reduction |
| `arr.norm()` | `float` | Computes Euclidean L2 vector norm | `sqrt(dot(arr, arr))` via FMA |
| `arr.shape()` | `int` | Returns number of elements in array | Instant header size lookup |

## 2. Linear Algebra Kernels

For array-to-array vector mathematics:
- `arrA.dot(arrB)`: Computes vector dot product using Fused Multiply-Add (`_mm256_fmadd_pd`).
- `arr.scale(factor)`: Multiplies every element by a scalar factor.
- `arrY.axpy(alpha, arrX)`: Computes BLAS Level 1 AXPY operation: `y = alpha * x + y`.

## 3. Register Preservation & Stack Bounds

When executing SIMD routines:
- All non-volatile XMM and general-purpose registers are preserved across calls.
- Alignment to 16-byte stack boundaries is strictly enforced prior to executing CALL instructions.
- Unaligned memory loads utilize `_mm256_loadu_pd` and `_mm512_loadu_pd` to prevent memory fault exceptions on misaligned heap allocations.
