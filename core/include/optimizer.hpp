/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * Optimizer.hpp - Nevaarize IR Optimization Passes
 *
 * Optimization passes for the intermediate representation.
 * Includes constant folding, dead code elimination, and inline caching.
 */

#ifndef NEVAARIZE_OPTIMIZER_HPP
#define NEVAARIZE_OPTIMIZER_HPP

#include "ir.hpp"
#include <vector>
#include <memory>

namespace nevaarize {

/**
 * Optimization level enum.
 */
enum class OptLevel : uint8_t {
    O0 = 0,  // No optimization
    O1 = 1,  // Basic optimizations
    O2 = 2,  // Standard optimizations
    O3 = 3   // Aggressive optimizations
};

/**
 * Optimization statistics.
 */
struct OptStats {
    int constantsFolded = 0;
    int deadCodeRemoved = 0;
    int strengthReduced = 0;
    int codeMotioned = 0;
    int passesRun = 0;
};

/**
 * Static optimization passes.
 */
class OptimizationPass {
public:
    static bool constantFold(IRFunction& func);
    static bool deadCodeElimination(IRFunction& func);
    static bool strengthReduction(IRFunction& func);
    static bool loopInvariantCodeMotion(IRFunction& func);
};

/**
 * Optimizer pipeline manager.
 */
class Optimizer {
public:
    /**
     * Optimize an IR function.
     */
    static void optimize(IRFunction& func, OptLevel level = OptLevel::O2);

    /**
     * Optimize all functions in a module.
     */
    static void optimize(IRModule& module, OptLevel level = OptLevel::O2);

    /**
     * Get optimization statistics.
     */
    OptStats getStats() const;

    /**
     * Reset statistics.
     */
    void resetStats();

private:
    OptStats stats;
};

} // namespace nevaarize

#endif // NEVAARIZE_OPTIMIZER_HPP
