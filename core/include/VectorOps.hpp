/**
 * VectorOps.hpp - SIMD Vector Operations
 *
 * High-performance vector operations using AVX2/AVX-512.
 * These are used by the JIT for auto-vectorized loops.
 */

#ifndef NEVAARIZE_VECTOR_OPS_HPP
#define NEVAARIZE_VECTOR_OPS_HPP

#include "SIMD.hpp"
#include <cstddef>

namespace nevaarize {

/**
 * Vector addition: dst[i] = a[i] + b[i]
 */
void vecAdd_f32(float* dst, const float* a, const float* b, size_t n);
void vecAdd_f64(double* dst, const double* a, const double* b, size_t n);
void vecAdd_i64(int64_t* dst, const int64_t* a, const int64_t* b, size_t n);

/**
 * Vector subtraction: dst[i] = a[i] - b[i]
 */
void vecSub_f32(float* dst, const float* a, const float* b, size_t n);
void vecSub_f64(double* dst, const double* a, const double* b, size_t n);
void vecSub_i64(int64_t* dst, const int64_t* a, const int64_t* b, size_t n);

/**
 * Vector multiplication: dst[i] = a[i] * b[i]
 */
void vecMul_f32(float* dst, const float* a, const float* b, size_t n);
void vecMul_f64(double* dst, const double* a, const double* b, size_t n);
void vecMul_i64(int64_t* dst, const int64_t* a, const int64_t* b, size_t n);

/**
 * Fused multiply-add: dst[i] = a[i] * b[i] + c[i]
 */
void vecFMA_f32(float* dst, const float* a, const float* b, const float* c, size_t n);
void vecFMA_f64(double* dst, const double* a, const double* b, const double* c, size_t n);

/**
 * Scalar operations on vectors: dst[i] = a[i] op scalar
 */
void vecScalarMul_f32(float* dst, const float* a, float scalar, size_t n);
void vecScalarMul_f64(double* dst, const double* a, double scalar, size_t n);
void vecScalarAdd_f32(float* dst, const float* a, float scalar, size_t n);
void vecScalarAdd_f64(double* dst, const double* a, double scalar, size_t n);

/**
 * Reduction operations.
 */
float vecSum_f32(const float* a, size_t n);
double vecSum_f64(const double* a, size_t n);
int64_t vecSum_i64(const int64_t* a, size_t n);

/**
 * Dot product: sum(a[i] * b[i])
 */
float vecDot_f32(const float* a, const float* b, size_t n);
double vecDot_f64(const double* a, const double* b, size_t n);

/**
 * Activation functions (for AI).
 */
void vecReLU_f32(float* dst, const float* src, size_t n);
void vecSigmoid_f32(float* dst, const float* src, size_t n);
void vecTanh_f32(float* dst, const float* src, size_t n);

/**
 * Fast integer sum loop: sum from 1 to n.
 * Uses SIMD to add multiple values at once.
 */
int64_t simdSumLoop(int64_t n);

} // namespace nevaarize

#endif // NEVAARIZE_VECTOR_OPS_HPP
