/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * SIMD.hpp - SIMD Detection and Intrinsics Wrapper
 *
 * Detects CPU SIMD capabilities and provides cross-platform
 * intrinsics wrapper for AVX2 and AVX-512.
 */

#ifndef NEVAARIZE_SIMD_HPP
#define NEVAARIZE_SIMD_HPP

#include <cstdint>
#include <cstddef>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

namespace nevaarize {

/**
 * SIMD capability levels.
 */
enum class SIMDLevel : uint8_t {
    NONE = 0,
    SSE2 = 1,
    SSE4 = 2,
    AVX = 3,
    AVX2 = 4,
    AVX512 = 5
};

/**
 * Detect CPU SIMD capabilities at runtime.
 */
SIMDLevel detectSIMD();

/**
 * Get string name for SIMD level.
 */
const char* simdLevelToString(SIMDLevel level);

/**
 * Check if specific SIMD level is available.
 */
bool hasSIMD(SIMDLevel level);

/**
 * SIMD constants.
 */
constexpr size_t AVX2_FLOAT_WIDTH = 8;      // 256 bits / 32 bits
constexpr size_t AVX2_DOUBLE_WIDTH = 4;     // 256 bits / 64 bits
constexpr size_t AVX2_INT64_WIDTH = 4;      // 256 bits / 64 bits

constexpr size_t AVX512_FLOAT_WIDTH = 16;   // 512 bits / 32 bits
constexpr size_t AVX512_DOUBLE_WIDTH = 8;   // 512 bits / 64 bits
constexpr size_t AVX512_INT64_WIDTH = 8;    // 512 bits / 64 bits

/**
 * Memory alignment for SIMD.
 */
constexpr size_t SIMD_ALIGNMENT = 32;       // AVX2 alignment

/**
 * Allocate aligned memory for SIMD operations.
 */
void* simdAlloc(size_t size);

/**
 * Free aligned memory.
 */
void simdFree(void* ptr);

/**
 * Check if pointer is aligned for SIMD.
 */
inline bool isAligned(const void* ptr, size_t alignment = SIMD_ALIGNMENT) {
    return (reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0;
}

} // namespace nevaarize

#endif // NEVAARIZE_SIMD_HPP
