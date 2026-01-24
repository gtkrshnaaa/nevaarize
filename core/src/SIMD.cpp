/**
 * SIMD.cpp - SIMD Detection Implementation
 *
 * Runtime detection of CPU SIMD capabilities using CPUID.
 */

#include "SIMD.hpp"
#include <cstdlib>
#include <cstring>

#if defined(__x86_64__) || defined(_M_X64)
#include <cpuid.h>
#endif

namespace nevaarize {

namespace {

// Cached SIMD level
SIMDLevel cachedLevel = SIMDLevel::NONE;
bool levelDetected = false;

#if defined(__x86_64__) || defined(_M_X64)

void cpuid(int info[4], int leaf, int subleaf = 0) {
    __cpuid_count(leaf, subleaf, info[0], info[1], info[2], info[3]);
}

SIMDLevel detectSIMDInternal() {
    int info[4];
    
    // Check basic CPUID
    cpuid(info, 0);
    int maxLeaf = info[0];
    if (maxLeaf < 1) return SIMDLevel::NONE;
    
    // Get feature flags (leaf 1)
    cpuid(info, 1);
    bool hasSSE2 = (info[3] & (1 << 26)) != 0;
    bool hasSSE4_1 = (info[2] & (1 << 19)) != 0;
    bool hasAVX = (info[2] & (1 << 28)) != 0;
    bool hasOSXSAVE = (info[2] & (1 << 27)) != 0;
    
    // Check if OS supports saving YMM registers
    bool osSupportsAVX = false;
    if (hasOSXSAVE) {
        unsigned long long xcr0 = 0;
        __asm__ volatile("xgetbv" : "=a"(xcr0) : "c"(0) : "%edx");
        osSupportsAVX = ((xcr0 & 0x6) == 0x6);
    }
    
    // Get extended features (leaf 7)
    bool hasAVX2 = false;
    bool hasAVX512F = false;
    if (maxLeaf >= 7) {
        cpuid(info, 7, 0);
        hasAVX2 = (info[1] & (1 << 5)) != 0;
        hasAVX512F = (info[1] & (1 << 16)) != 0;
    }
    
    // Determine SIMD level
    if (hasAVX512F && osSupportsAVX) {
        // Check OS supports ZMM registers
        unsigned long long xcr0 = 0;
        __asm__ volatile("xgetbv" : "=a"(xcr0) : "c"(0) : "%edx");
        if ((xcr0 & 0xE6) == 0xE6) {
            return SIMDLevel::AVX512;
        }
    }
    
    if (hasAVX2 && osSupportsAVX) return SIMDLevel::AVX2;
    if (hasAVX && osSupportsAVX) return SIMDLevel::AVX;
    if (hasSSE4_1) return SIMDLevel::SSE4;
    if (hasSSE2) return SIMDLevel::SSE2;
    
    return SIMDLevel::NONE;
}

#else

SIMDLevel detectSIMDInternal() {
    return SIMDLevel::NONE;
}

#endif

} // anonymous namespace

SIMDLevel detectSIMD() {
    if (!levelDetected) {
        cachedLevel = detectSIMDInternal();
        levelDetected = true;
    }
    return cachedLevel;
}

const char* simdLevelToString(SIMDLevel level) {
    switch (level) {
        case SIMDLevel::NONE: return "None";
        case SIMDLevel::SSE2: return "SSE2";
        case SIMDLevel::SSE4: return "SSE4.1";
        case SIMDLevel::AVX: return "AVX";
        case SIMDLevel::AVX2: return "AVX2";
        case SIMDLevel::AVX512: return "AVX-512";
        default: return "Unknown";
    }
}

bool hasSIMD(SIMDLevel level) {
    SIMDLevel detected = detectSIMD();
    return static_cast<uint8_t>(detected) >= static_cast<uint8_t>(level);
}

void* simdAlloc(size_t size) {
    void* ptr = nullptr;
#if defined(_WIN32)
    ptr = _aligned_malloc(size, SIMD_ALIGNMENT);
#else
    if (posix_memalign(&ptr, SIMD_ALIGNMENT, size) != 0) {
        ptr = nullptr;
    }
#endif
    return ptr;
}

void simdFree(void* ptr) {
    if (ptr) {
#if defined(_WIN32)
        _aligned_free(ptr);
#else
        free(ptr);
#endif
    }
}

} // namespace nevaarize
