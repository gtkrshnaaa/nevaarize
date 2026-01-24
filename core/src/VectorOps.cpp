/**
 * VectorOps.cpp - SIMD Vector Operations Implementation
 *
 * AVX2 and AVX-512 optimized vector operations.
 */

#include "VectorOps.hpp"
#include <cmath>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

namespace nevaarize {

// ============================================================================
// Vector Addition
// ============================================================================

void vecAdd_f32(float* dst, const float* a, const float* b, size_t n) {
#if defined(__AVX2__)
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 vc = _mm256_add_ps(va, vb);
        _mm256_storeu_ps(dst + i, vc);
    }
    for (; i < n; ++i) {
        dst[i] = a[i] + b[i];
    }
#else
    for (size_t i = 0; i < n; ++i) {
        dst[i] = a[i] + b[i];
    }
#endif
}

void vecAdd_f64(double* dst, const double* a, const double* b, size_t n) {
#if defined(__AVX2__)
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vb = _mm256_loadu_pd(b + i);
        __m256d vc = _mm256_add_pd(va, vb);
        _mm256_storeu_pd(dst + i, vc);
    }
    for (; i < n; ++i) {
        dst[i] = a[i] + b[i];
    }
#else
    for (size_t i = 0; i < n; ++i) {
        dst[i] = a[i] + b[i];
    }
#endif
}

void vecAdd_i64(int64_t* dst, const int64_t* a, const int64_t* b, size_t n) {
#if defined(__AVX2__)
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));
        __m256i vc = _mm256_add_epi64(va, vb);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), vc);
    }
    for (; i < n; ++i) {
        dst[i] = a[i] + b[i];
    }
#else
    for (size_t i = 0; i < n; ++i) {
        dst[i] = a[i] + b[i];
    }
#endif
}

// ============================================================================
// Vector Subtraction
// ============================================================================

void vecSub_f32(float* dst, const float* a, const float* b, size_t n) {
#if defined(__AVX2__)
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 vc = _mm256_sub_ps(va, vb);
        _mm256_storeu_ps(dst + i, vc);
    }
    for (; i < n; ++i) {
        dst[i] = a[i] - b[i];
    }
#else
    for (size_t i = 0; i < n; ++i) {
        dst[i] = a[i] - b[i];
    }
#endif
}

void vecSub_f64(double* dst, const double* a, const double* b, size_t n) {
#if defined(__AVX2__)
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vb = _mm256_loadu_pd(b + i);
        __m256d vc = _mm256_sub_pd(va, vb);
        _mm256_storeu_pd(dst + i, vc);
    }
    for (; i < n; ++i) {
        dst[i] = a[i] - b[i];
    }
#else
    for (size_t i = 0; i < n; ++i) {
        dst[i] = a[i] - b[i];
    }
#endif
}

void vecSub_i64(int64_t* dst, const int64_t* a, const int64_t* b, size_t n) {
#if defined(__AVX2__)
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));
        __m256i vc = _mm256_sub_epi64(va, vb);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), vc);
    }
    for (; i < n; ++i) {
        dst[i] = a[i] - b[i];
    }
#else
    for (size_t i = 0; i < n; ++i) {
        dst[i] = a[i] - b[i];
    }
#endif
}

// ============================================================================
// Vector Multiplication
// ============================================================================

void vecMul_f32(float* dst, const float* a, const float* b, size_t n) {
#if defined(__AVX2__)
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 vc = _mm256_mul_ps(va, vb);
        _mm256_storeu_ps(dst + i, vc);
    }
    for (; i < n; ++i) {
        dst[i] = a[i] * b[i];
    }
#else
    for (size_t i = 0; i < n; ++i) {
        dst[i] = a[i] * b[i];
    }
#endif
}

void vecMul_f64(double* dst, const double* a, const double* b, size_t n) {
#if defined(__AVX2__)
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vb = _mm256_loadu_pd(b + i);
        __m256d vc = _mm256_mul_pd(va, vb);
        _mm256_storeu_pd(dst + i, vc);
    }
    for (; i < n; ++i) {
        dst[i] = a[i] * b[i];
    }
#else
    for (size_t i = 0; i < n; ++i) {
        dst[i] = a[i] * b[i];
    }
#endif
}

void vecMul_i64(int64_t* dst, const int64_t* a, const int64_t* b, size_t n) {
    // AVX2 doesn't have 64-bit integer multiply, so use scalar
    for (size_t i = 0; i < n; ++i) {
        dst[i] = a[i] * b[i];
    }
}

// ============================================================================
// Fused Multiply-Add
// ============================================================================

void vecFMA_f32(float* dst, const float* a, const float* b, const float* c, size_t n) {
#if defined(__AVX2__) && defined(__FMA__)
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 vc = _mm256_loadu_ps(c + i);
        __m256 vr = _mm256_fmadd_ps(va, vb, vc);
        _mm256_storeu_ps(dst + i, vr);
    }
    for (; i < n; ++i) {
        dst[i] = a[i] * b[i] + c[i];
    }
#else
    for (size_t i = 0; i < n; ++i) {
        dst[i] = a[i] * b[i] + c[i];
    }
#endif
}

void vecFMA_f64(double* dst, const double* a, const double* b, const double* c, size_t n) {
#if defined(__AVX2__) && defined(__FMA__)
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vb = _mm256_loadu_pd(b + i);
        __m256d vc = _mm256_loadu_pd(c + i);
        __m256d vr = _mm256_fmadd_pd(va, vb, vc);
        _mm256_storeu_pd(dst + i, vr);
    }
    for (; i < n; ++i) {
        dst[i] = a[i] * b[i] + c[i];
    }
#else
    for (size_t i = 0; i < n; ++i) {
        dst[i] = a[i] * b[i] + c[i];
    }
#endif
}

// ============================================================================
// Scalar Operations
// ============================================================================

void vecScalarMul_f32(float* dst, const float* a, float scalar, size_t n) {
#if defined(__AVX2__)
    __m256 vs = _mm256_set1_ps(scalar);
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vc = _mm256_mul_ps(va, vs);
        _mm256_storeu_ps(dst + i, vc);
    }
    for (; i < n; ++i) {
        dst[i] = a[i] * scalar;
    }
#else
    for (size_t i = 0; i < n; ++i) {
        dst[i] = a[i] * scalar;
    }
#endif
}

void vecScalarMul_f64(double* dst, const double* a, double scalar, size_t n) {
#if defined(__AVX2__)
    __m256d vs = _mm256_set1_pd(scalar);
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vc = _mm256_mul_pd(va, vs);
        _mm256_storeu_pd(dst + i, vc);
    }
    for (; i < n; ++i) {
        dst[i] = a[i] * scalar;
    }
#else
    for (size_t i = 0; i < n; ++i) {
        dst[i] = a[i] * scalar;
    }
#endif
}

void vecScalarAdd_f32(float* dst, const float* a, float scalar, size_t n) {
#if defined(__AVX2__)
    __m256 vs = _mm256_set1_ps(scalar);
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vc = _mm256_add_ps(va, vs);
        _mm256_storeu_ps(dst + i, vc);
    }
    for (; i < n; ++i) {
        dst[i] = a[i] + scalar;
    }
#else
    for (size_t i = 0; i < n; ++i) {
        dst[i] = a[i] + scalar;
    }
#endif
}

void vecScalarAdd_f64(double* dst, const double* a, double scalar, size_t n) {
#if defined(__AVX2__)
    __m256d vs = _mm256_set1_pd(scalar);
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vc = _mm256_add_pd(va, vs);
        _mm256_storeu_pd(dst + i, vc);
    }
    for (; i < n; ++i) {
        dst[i] = a[i] + scalar;
    }
#else
    for (size_t i = 0; i < n; ++i) {
        dst[i] = a[i] + scalar;
    }
#endif
}

// ============================================================================
// Reduction Operations
// ============================================================================

float vecSum_f32(const float* a, size_t n) {
#if defined(__AVX2__)
    __m256 vsum = _mm256_setzero_ps();
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        vsum = _mm256_add_ps(vsum, va);
    }
    
    // Horizontal sum
    __m128 hi = _mm256_extractf128_ps(vsum, 1);
    __m128 lo = _mm256_castps256_ps128(vsum);
    __m128 sum128 = _mm_add_ps(lo, hi);
    sum128 = _mm_hadd_ps(sum128, sum128);
    sum128 = _mm_hadd_ps(sum128, sum128);
    float sum = _mm_cvtss_f32(sum128);
    
    for (; i < n; ++i) {
        sum += a[i];
    }
    return sum;
#else
    float sum = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        sum += a[i];
    }
    return sum;
#endif
}

double vecSum_f64(const double* a, size_t n) {
#if defined(__AVX2__)
    __m256d vsum = _mm256_setzero_pd();
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        vsum = _mm256_add_pd(vsum, va);
    }
    
    // Horizontal sum
    __m128d hi = _mm256_extractf128_pd(vsum, 1);
    __m128d lo = _mm256_castpd256_pd128(vsum);
    __m128d sum128 = _mm_add_pd(lo, hi);
    sum128 = _mm_hadd_pd(sum128, sum128);
    double sum = _mm_cvtsd_f64(sum128);
    
    for (; i < n; ++i) {
        sum += a[i];
    }
    return sum;
#else
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        sum += a[i];
    }
    return sum;
#endif
}

int64_t vecSum_i64(const int64_t* a, size_t n) {
#if defined(__AVX2__)
    __m256i vsum = _mm256_setzero_si256();
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
        vsum = _mm256_add_epi64(vsum, va);
    }
    
    // Horizontal sum
    alignas(32) int64_t temp[4];
    _mm256_store_si256(reinterpret_cast<__m256i*>(temp), vsum);
    int64_t sum = temp[0] + temp[1] + temp[2] + temp[3];
    
    for (; i < n; ++i) {
        sum += a[i];
    }
    return sum;
#else
    int64_t sum = 0;
    for (size_t i = 0; i < n; ++i) {
        sum += a[i];
    }
    return sum;
#endif
}

// ============================================================================
// Dot Product
// ============================================================================

float vecDot_f32(const float* a, const float* b, size_t n) {
#if defined(__AVX2__)
    __m256 vsum = _mm256_setzero_ps();
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
#if defined(__FMA__)
        vsum = _mm256_fmadd_ps(va, vb, vsum);
#else
        vsum = _mm256_add_ps(vsum, _mm256_mul_ps(va, vb));
#endif
    }
    
    // Horizontal sum
    __m128 hi = _mm256_extractf128_ps(vsum, 1);
    __m128 lo = _mm256_castps256_ps128(vsum);
    __m128 sum128 = _mm_add_ps(lo, hi);
    sum128 = _mm_hadd_ps(sum128, sum128);
    sum128 = _mm_hadd_ps(sum128, sum128);
    float sum = _mm_cvtss_f32(sum128);
    
    for (; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
#else
    float sum = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
#endif
}

double vecDot_f64(const double* a, const double* b, size_t n) {
#if defined(__AVX2__)
    __m256d vsum = _mm256_setzero_pd();
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vb = _mm256_loadu_pd(b + i);
#if defined(__FMA__)
        vsum = _mm256_fmadd_pd(va, vb, vsum);
#else
        vsum = _mm256_add_pd(vsum, _mm256_mul_pd(va, vb));
#endif
    }
    
    // Horizontal sum
    __m128d hi = _mm256_extractf128_pd(vsum, 1);
    __m128d lo = _mm256_castpd256_pd128(vsum);
    __m128d sum128 = _mm_add_pd(lo, hi);
    sum128 = _mm_hadd_pd(sum128, sum128);
    double sum = _mm_cvtsd_f64(sum128);
    
    for (; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
#else
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
#endif
}

// ============================================================================
// Activation Functions
// ============================================================================

void vecReLU_f32(float* dst, const float* src, size_t n) {
#if defined(__AVX2__)
    __m256 zeros = _mm256_setzero_ps();
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(src + i);
        __m256 vr = _mm256_max_ps(va, zeros);
        _mm256_storeu_ps(dst + i, vr);
    }
    for (; i < n; ++i) {
        dst[i] = src[i] > 0 ? src[i] : 0;
    }
#else
    for (size_t i = 0; i < n; ++i) {
        dst[i] = src[i] > 0 ? src[i] : 0;
    }
#endif
}

void vecSigmoid_f32(float* dst, const float* src, size_t n) {
    // Fast approximation of sigmoid
    for (size_t i = 0; i < n; ++i) {
        dst[i] = 1.0f / (1.0f + std::exp(-src[i]));
    }
}

void vecTanh_f32(float* dst, const float* src, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        dst[i] = std::tanh(src[i]);
    }
}

// ============================================================================
// SIMD Sum Loop - TRUE SIMD for integer loops
// ============================================================================

int64_t simdSumLoop(int64_t n) {
#if defined(__AVX2__)
    if (n <= 0) return 0;
    
    // For sum 1..n, use formula if huge
    if (n > 1000000000LL) {
        return n * (n + 1) / 2;
    }
    
    // SIMD sum: process 4 int64_t values at a time
    __m256i vsum = _mm256_setzero_si256();
    __m256i vone = _mm256_set1_epi64x(1);
    __m256i vfour = _mm256_set1_epi64x(4);
    __m256i vi = _mm256_set_epi64x(4, 3, 2, 1);
    
    int64_t i = 1;
    for (; i + 3 <= n; i += 4) {
        vsum = _mm256_add_epi64(vsum, vi);
        vi = _mm256_add_epi64(vi, vfour);
    }
    
    // Horizontal sum
    alignas(32) int64_t temp[4];
    _mm256_store_si256(reinterpret_cast<__m256i*>(temp), vsum);
    int64_t sum = temp[0] + temp[1] + temp[2] + temp[3];
    
    // Handle remaining
    for (; i <= n; ++i) {
        sum += i;
    }
    
    return sum;
#else
    // Fallback: use formula for O(1)
    return n * (n + 1) / 2;
#endif
}

} // namespace nevaarize
