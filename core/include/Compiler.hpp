/**
 * Compiler.hpp - Nevaarize Native Compiler
 *
 * Compiles Nevaarize AST directly to x86-64 machine code.
 * This is the default execution engine for Nevaarize.
 */

#ifndef NEVAARIZE_COMPILER_HPP
#define NEVAARIZE_COMPILER_HPP

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
 * Native Compiler for Nevaarize.
 * Compiles AST directly to executable x86-64 machine code.
 */
class Compiler {
public:
    Compiler();
    ~Compiler();

    /**
     * Compile a full program to native code.
     * This is the main entry point for compilation.
     */
    CompiledFunc compile(const AST& ast);

    /**
     * Compile a for loop to native code.
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
     * Check if a loop can be compiled directly.
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

    // AST compilation - expressions
    X64Reg compileExpr(const AST& ast, NodeIndex idx);
    
    // AST compilation - statements
    void compileStatement(const AST& ast, NodeIndex idx);
    void compileAssignment(const AST& ast, NodeIndex idx);
    void compileBlock(const AST& ast, NodeIndex idx);
    void compileIf(const AST& ast, NodeIndex idx);
    void compileWhile(const AST& ast, NodeIndex idx);
    void compileReturn(const AST& ast, NodeIndex idx);
    void compileCall(const AST& ast, NodeIndex idx);
    
    // Native function call emission
    void emitPrintInt(X64Reg valueReg);

    // Register allocation state
    bool regInUse[16];
};

} // namespace nevaarize

#endif // NEVAARIZE_COMPILER_HPP
