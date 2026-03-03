/**
 * Optimizer.cpp - JIT Optimization Passes
 *
 * Implements constant folding, dead code elimination,
 * strength reduction, and loop-invariant code motion.
 */

#include "optimizer.hpp"
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace nevaarize {

// Constant Folding: evaluate constant expressions at compile time
bool OptimizationPass::constantFold(IRFunction& func) {
    bool changed = false;

    // Track known constant values: IR virtual register -> constant instruction
    std::unordered_map<IRIndex, IRInst*> constants;

    for (auto& block : func.blocks) {
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            IRInst& inst = block.instructions[i];

            // Register constant definitions
            if (inst.opcode == IROpcode::CONST_INT ||
                inst.opcode == IROpcode::CONST_FLOAT) {
                constants[inst.dest] = &inst;
                continue;
            }

            // Fold binary arithmetic on two known constants
            if (inst.opcode == IROpcode::ADD || inst.opcode == IROpcode::SUB ||
                inst.opcode == IROpcode::MUL || inst.opcode == IROpcode::DIV ||
                inst.opcode == IROpcode::MOD) {

                auto itL = constants.find(inst.src1);
                auto itR = constants.find(inst.src2);
                if (itL == constants.end() || itR == constants.end()) continue;

                IRInst* left = itL->second;
                IRInst* right = itR->second;

                // Integer folding
                if (left->opcode == IROpcode::CONST_INT &&
                    right->opcode == IROpcode::CONST_INT) {

                    int64_t a = left->intVal;
                    int64_t b = right->intVal;
                    int64_t result = 0;

                    switch (inst.opcode) {
                        case IROpcode::ADD: result = a + b; break;
                        case IROpcode::SUB: result = a - b; break;
                        case IROpcode::MUL: result = a * b; break;
                        case IROpcode::DIV: result = (b != 0) ? a / b : 0; break;
                        case IROpcode::MOD: result = (b != 0) ? a % b : 0; break;
                        default: continue;
                    }

                    inst.opcode = IROpcode::CONST_INT;
                    inst.intVal = result;
                    inst.src1 = IR_INVALID;
                    inst.src2 = IR_INVALID;
                    constants[inst.dest] = &inst;
                    changed = true;
                }
                // Float folding
                else if (left->opcode == IROpcode::CONST_FLOAT &&
                         right->opcode == IROpcode::CONST_FLOAT) {

                    double a = left->floatVal;
                    double b = right->floatVal;
                    double result = 0.0;

                    switch (inst.opcode) {
                        case IROpcode::ADD: result = a + b; break;
                        case IROpcode::SUB: result = a - b; break;
                        case IROpcode::MUL: result = a * b; break;
                        case IROpcode::DIV: result = (b != 0.0) ? a / b : 0.0; break;
                        default: continue;
                    }

                    inst.opcode = IROpcode::CONST_FLOAT;
                    inst.floatVal = result;
                    inst.src1 = IR_INVALID;
                    inst.src2 = IR_INVALID;
                    constants[inst.dest] = &inst;
                    changed = true;
                }
            }

            // Fold comparisons on known constants
            if (inst.opcode == IROpcode::EQ || inst.opcode == IROpcode::NEQ ||
                inst.opcode == IROpcode::LT || inst.opcode == IROpcode::LTE ||
                inst.opcode == IROpcode::GT || inst.opcode == IROpcode::GTE) {

                auto itL = constants.find(inst.src1);
                auto itR = constants.find(inst.src2);
                if (itL == constants.end() || itR == constants.end()) continue;

                IRInst* left = itL->second;
                IRInst* right = itR->second;

                if (left->opcode == IROpcode::CONST_INT &&
                    right->opcode == IROpcode::CONST_INT) {

                    int64_t a = left->intVal;
                    int64_t b = right->intVal;
                    bool result = false;

                    switch (inst.opcode) {
                        case IROpcode::EQ:  result = (a == b); break;
                        case IROpcode::NEQ: result = (a != b); break;
                        case IROpcode::LT:  result = (a < b);  break;
                        case IROpcode::LTE: result = (a <= b); break;
                        case IROpcode::GT:  result = (a > b);  break;
                        case IROpcode::GTE: result = (a >= b); break;
                        default: continue;
                    }

                    inst.opcode = IROpcode::CONST_INT;
                    inst.intVal = result ? 1 : 0;
                    inst.src1 = IR_INVALID;
                    inst.src2 = IR_INVALID;
                    constants[inst.dest] = &inst;
                    changed = true;
                }
            }
        }
    }

    return changed;
}

// Dead Code Elimination: remove instructions whose results are never used
bool OptimizationPass::deadCodeElimination(IRFunction& func) {
    bool changed = false;

    // Collect all used source operands
    std::unordered_set<IRIndex> usedDefs;
    for (auto& block : func.blocks) {
        for (auto& inst : block.instructions) {
            if (inst.src1 != IR_INVALID) usedDefs.insert(inst.src1);
            if (inst.src2 != IR_INVALID) usedDefs.insert(inst.src2);
        }
    }

    for (auto& block : func.blocks) {
        auto newEnd = std::remove_if(block.instructions.begin(),
                                      block.instructions.end(),
                                      [&usedDefs](const IRInst& inst) {
                                          // Remove NOPs
                                          if (inst.opcode == IROpcode::NOP) return true;

                                          // Remove pure computations with unused results
                                          if (inst.dest != IR_INVALID &&
                                              usedDefs.find(inst.dest) == usedDefs.end()) {

                                              // Only remove side-effect-free instructions
                                              if (inst.opcode == IROpcode::CONST_INT ||
                                                  inst.opcode == IROpcode::CONST_FLOAT ||
                                                  inst.opcode == IROpcode::CONST_BOOL ||
                                                  inst.opcode == IROpcode::ADD ||
                                                  inst.opcode == IROpcode::SUB ||
                                                  inst.opcode == IROpcode::MUL ||
                                                  inst.opcode == IROpcode::DIV ||
                                                  inst.opcode == IROpcode::MOD ||
                                                  inst.opcode == IROpcode::NEG) {
                                                  return true;
                                              }
                                          }
                                          return false;
                                      });
        if (newEnd != block.instructions.end()) {
            block.instructions.erase(newEnd, block.instructions.end());
            changed = true;
        }
    }

    return changed;
}

// Strength Reduction: replace expensive ops with cheaper equivalents
bool OptimizationPass::strengthReduction(IRFunction& func) {
    bool changed = false;

    // Track known constant values
    std::unordered_map<IRIndex, IRInst*> constants;

    for (auto& block : func.blocks) {
        for (auto& inst : block.instructions) {
            if (inst.opcode == IROpcode::CONST_INT) {
                constants[inst.dest] = &inst;
            }

            if (inst.opcode == IROpcode::MUL) {
                // Check if either operand is a known constant
                auto itR = constants.find(inst.src2);
                if (itR != constants.end() && itR->second->opcode == IROpcode::CONST_INT) {
                    int64_t val = itR->second->intVal;

                    // x * 0 -> CONST_INT 0
                    if (val == 0) {
                        inst.opcode = IROpcode::CONST_INT;
                        inst.intVal = 0;
                        inst.src1 = IR_INVALID;
                        inst.src2 = IR_INVALID;
                        changed = true;
                        continue;
                    }

                    // x * 1 -> NOP (propagate x)
                    if (val == 1) {
                        inst.opcode = IROpcode::NOP;
                        changed = true;
                        continue;
                    }

                    // x * 2 -> x + x (cheaper on most architectures)
                    if (val == 2) {
                        inst.opcode = IROpcode::ADD;
                        inst.src2 = inst.src1;
                        changed = true;
                        continue;
                    }

                    // x * power_of_2 -> left shift (handled at codegen level)
                    if (val > 0 && (val & (val - 1)) == 0) {
                        // Keep as MUL but mark for shift conversion at codegen
                        // (IR doesn't have shift opcode yet)
                    }
                }
            }

            if (inst.opcode == IROpcode::DIV) {
                auto itR = constants.find(inst.src2);
                if (itR != constants.end() && itR->second->opcode == IROpcode::CONST_INT) {
                    int64_t val = itR->second->intVal;

                    // x / 1 -> NOP (propagate x)
                    if (val == 1) {
                        inst.opcode = IROpcode::NOP;
                        changed = true;
                        continue;
                    }
                }
            }

            if (inst.opcode == IROpcode::MOD) {
                auto itR = constants.find(inst.src2);
                if (itR != constants.end() && itR->second->opcode == IROpcode::CONST_INT) {
                    int64_t val = itR->second->intVal;

                    // x % 1 -> CONST_INT 0
                    if (val == 1) {
                        inst.opcode = IROpcode::CONST_INT;
                        inst.intVal = 0;
                        inst.src1 = IR_INVALID;
                        inst.src2 = IR_INVALID;
                        changed = true;
                    }
                }
            }
        }
    }

    return changed;
}

// Loop-Invariant Code Motion: move invariant code out of loops
bool OptimizationPass::loopInvariantCodeMotion(IRFunction& func) {
    bool changed = false;

    // Identify loop headers via back edges (simplified: blocks that jump backward)
    for (size_t blockIdx = 0; blockIdx < func.blocks.size(); ++blockIdx) {
        auto& block = func.blocks[blockIdx];

        // Collect all variables defined in this block
        std::unordered_set<IRIndex> blockDefs;
        for (auto& inst : block.instructions) {
            if (inst.dest != IR_INVALID) {
                blockDefs.insert(inst.dest);
            }
        }

        // Find instructions whose operands are NOT defined within this block
        // These are candidates for hoisting
        std::vector<size_t> hoistable;
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            auto& inst = block.instructions[i];

            // Only hoist pure computations
            if (inst.opcode != IROpcode::ADD && inst.opcode != IROpcode::SUB &&
                inst.opcode != IROpcode::MUL && inst.opcode != IROpcode::DIV &&
                inst.opcode != IROpcode::CONST_INT && inst.opcode != IROpcode::CONST_FLOAT) {
                continue;
            }

            bool src1Invariant = (inst.src1 == IR_INVALID || blockDefs.find(inst.src1) == blockDefs.end());
            bool src2Invariant = (inst.src2 == IR_INVALID || blockDefs.find(inst.src2) == blockDefs.end());

            if (src1Invariant && src2Invariant) {
                hoistable.push_back(i);
            }
        }

        // Move hoistable instructions to the previous block if one exists
        if (!hoistable.empty() && blockIdx > 0) {
            auto& predBlock = func.blocks[blockIdx - 1];
            for (auto it = hoistable.rbegin(); it != hoistable.rend(); ++it) {
                predBlock.instructions.push_back(block.instructions[*it]);
                block.instructions[*it].opcode = IROpcode::NOP;
                changed = true;
            }
        }
    }

    return changed;
}

// Run all optimization passes
void Optimizer::optimize(IRFunction& func, OptLevel level) {
    if (level == OptLevel::O0) return;

    bool changed = true;
    int iterations = 0;
    const int maxIterations = 10;

    while (changed && iterations < maxIterations) {
        changed = false;
        ++iterations;

        // Always run these basic optimizations
        changed |= OptimizationPass::constantFold(func);
        changed |= OptimizationPass::deadCodeElimination(func);

        if (level >= OptLevel::O2) {
            changed |= OptimizationPass::strengthReduction(func);
        }

        if (level >= OptLevel::O3) {
            changed |= OptimizationPass::loopInvariantCodeMotion(func);
        }
    }
}

// Optimize all functions in module
void Optimizer::optimize(IRModule& module, OptLevel level) {
    for (auto& func : module.getFunctions()) {
        optimize(func, level);
    }
}

// Get optimization statistics
OptStats Optimizer::getStats() const {
    return stats;
}

void Optimizer::resetStats() {
    stats = OptStats{};
}

} // namespace nevaarize
