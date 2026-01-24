/**
 * TrueJIT.hpp - True JIT Compiler for Nevaarize
 *
 * Compiles Nevaarize AST directly to x86-64 machine code.
 * This is REAL JIT - not pre-written assembly.
 */

#ifndef NEVAARIZE_TRUE_JIT_HPP
#define NEVAARIZE_TRUE_JIT_HPP

#include "AST.hpp"
#include "CodeGen.hpp"
#include "Value.hpp"
#include <memory>
#include <unordered_map>

namespace nevaarize {

/**
 * Compiled function type.
 */
using CompiledFunc = int64_t (*)();

/**
 * Variable location in compiled code.
 */
struct VarLocation {
    int32_t stackOffset;
    bool isRegister;
    X64Reg reg;
};

/**
 * True JIT Compiler for Nevaarize.
 * Compiles AST directly to executable x86-64 machine code.
 */
class TrueJIT {
public:
    TrueJIT();
    ~TrueJIT();

    /**
     * Compile a for loop to native code.
     * Returns a function pointer to execute.
     */
    CompiledFunc compileForLoop(const AST& ast, NodeIndex forNode,
                                 int64_t start, int64_t end);

    /**
     * Compile an expression to native code.
     */
    CompiledFunc compileExpression(const AST& ast, NodeIndex exprNode);

    /**
     * Execute compiled code and return result.
     */
    int64_t execute(CompiledFunc fn);

    /**
     * Check if a loop can be JIT compiled.
     */
    bool canCompileLoop(const AST& ast, NodeIndex forNode);

private:
    std::unique_ptr<ExecutableMemory> execMem;
    CodeGenerator codegen;
    std::unordered_map<std::string, VarLocation> variables;
    int32_t stackSize;
    int32_t nextStackSlot;

    // Code generation helpers
    void emitPrologue();
    void emitEpilogue();
    X64Reg allocateReg();
    void freeReg(X64Reg reg);
    int32_t allocateStackSlot();

    // AST compilation
    X64Reg compileExpr(const AST& ast, NodeIndex idx);
    void compileAssignment(const AST& ast, NodeIndex idx);
    void compileBlock(const AST& ast, NodeIndex idx);
    void compileIf(const AST& ast, NodeIndex idx);

    // Register allocation state
    bool regInUse[16];
};

/**
 * JIT-enabled evaluator that hot-compiles loops.
 */
class JITEvaluator {
public:
    JITEvaluator();

    /**
     * Execute AST with JIT compilation for hot paths.
     */
    Value execute(std::shared_ptr<const AST> ast);

    /**
     * Get performance statistics.
     */
    struct Stats {
        int64_t interpretedOps;
        int64_t compiledOps;
        double compiledPercentage;
    };
    Stats getStats() const;

private:
    TrueJIT jit;
    int64_t interpretedOps;
    int64_t compiledOps;
};

} // namespace nevaarize

#endif // NEVAARIZE_TRUE_JIT_HPP
