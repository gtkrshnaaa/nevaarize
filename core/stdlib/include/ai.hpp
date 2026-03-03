/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * AI.hpp - Nevaarize AI Engineering Standard Library
 *
 * Core AI/ML primitives for neural network development.
 * SIMD-accelerated tensor operations, activation functions,
 * loss functions, and optimization utilities.
 * 
 * Model training and serving with .nmod format.
 */

#ifndef NEVAARIZE_STDLIB_AI_HPP
#define NEVAARIZE_STDLIB_AI_HPP

#include "value.hpp"
#include "model.hpp"
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <memory>

namespace nevaarize {
namespace stdlib {

/**
 * Get all AI library functions.
 */
std::unordered_map<std::string, NativeFunction> getAILibrary();

// Internal tensor utilities
namespace ai_internal {
    
    /**
     * Extract flat float array from Value (array of numbers).
     */
    std::vector<float> toFloatVector(const Value& val);
    
    /**
     * Convert float vector back to Value array.
     */
    Value fromFloatVector(const std::vector<float>& vec);
    
    /**
     * SIMD-accelerated dot product.
     */
    float simdDotProduct(const float* a, const float* b, size_t n);
    
    /**
     * SIMD-accelerated element-wise add.
     */
    void simdAdd(float* dst, const float* a, const float* b, size_t n);
    
    /**
     * SIMD-accelerated element-wise multiply.
     */
    void simdMul(float* dst, const float* a, const float* b, size_t n);
    
    /**
     * SIMD-accelerated sum reduction.
     */
    float simdSum(const float* data, size_t n);
    
    /**
     * Cache-blocked matrix multiplication.
     * C = A * B, where A is (m x k), B is (k x n), C is (m x n)
     */
    void simdMatMul(float* C, const float* A, const float* B, 
                    size_t m, size_t k, size_t n);
    
    /**
     * SIMD-accelerated ReLU.
     */
    void simdReLU(float* dst, const float* src, size_t n);
    
    /**
     * SIMD-accelerated Sigmoid.
     */
    void simdSigmoid(float* dst, const float* src, size_t n);
    
    /**
     * Numerically stable softmax.
     */
    void stableSoftmax(float* dst, const float* src, size_t n);

} // namespace ai_internal

} // namespace stdlib
} // namespace nevaarize

#endif // NEVAARIZE_STDLIB_AI_HPP
