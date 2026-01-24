/**
 * Optimizer.hpp - Nevaarize IR Optimization Passes
 *
 * Optimization passes for the intermediate representation.
 * Includes constant folding, dead code elimination, and inline caching.
 */

#ifndef NEVAARIZE_OPTIMIZER_HPP
#define NEVAARIZE_OPTIMIZER_HPP

#include "IR.hpp"
#include <vector>

namespace nevaarize {

/**
 * Optimization level enum.
 */
enum class OptLevel : uint8_t {
    O0,
    O1,
    O2,
    O3
};

/**
 * IR optimization pass interface.
 */
class OptimizationPass {
public:
    virtual ~OptimizationPass() = default;
    virtual void run(IRFunction& func) = 0;
    virtual const char* name() const = 0;
};

/**
 * Constant folding optimization.
 * Evaluates constant expressions at compile time.
 */
class ConstantFoldingPass : public OptimizationPass {
public:
    void run(IRFunction& func) override;
    const char* name() const override { return "ConstantFolding"; }
};

/**
 * Dead code elimination.
 * Removes unused instructions and unreachable code.
 */
class DeadCodeEliminationPass : public OptimizationPass {
public:
    void run(IRFunction& func) override;
    const char* name() const override { return "DeadCodeElimination"; }
};

/**
 * Copy propagation.
 * Replaces uses of copied values with the original.
 */
class CopyPropagationPass : public OptimizationPass {
public:
    void run(IRFunction& func) override;
    const char* name() const override { return "CopyPropagation"; }
};

/**
 * Common subexpression elimination.
 * Removes redundant computations.
 */
class CSEPass : public OptimizationPass {
public:
    void run(IRFunction& func) override;
    const char* name() const override { return "CommonSubexpressionElimination"; }
};

/**
 * Optimizer pipeline manager.
 */
class Optimizer {
public:
    explicit Optimizer(OptLevel level = OptLevel::O2);

    /**
     * Optimize an IR function.
     */
    void optimize(IRFunction& func);

    /**
     * Optimize all functions in a module.
     */
    void optimize(IRModule& module);

    /**
     * Set optimization level.
     */
    void setLevel(OptLevel level);

    /**
     * Add a custom optimization pass.
     */
    void addPass(std::unique_ptr<OptimizationPass> pass);

private:
    OptLevel level;
    std::vector<std::unique_ptr<OptimizationPass>> passes;

    void setupPasses();
};

} // namespace nevaarize

#endif // NEVAARIZE_OPTIMIZER_HPP
