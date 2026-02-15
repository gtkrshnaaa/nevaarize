/**
 * JIT.hpp - Nevaarize Native Compiler
 *
 * Compiles Nevaarize AST directly to x86-64 machine code.
 * This is the default execution engine for Nevaarize.
 */

#ifndef NEVAARIZE_JIT_HPP
#define NEVAARIZE_JIT_HPP

#include "AST.hpp"
#include "CodeGen.hpp"
#include "Value.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace nevaarize {

/**
 * Compiled function type.
 */
using CompiledFunc = int64_t (*)();

/**
 * Variable location in compiled code.
 */
struct VarLocation {
    int32_t stackOffset; // Base offset (value at offset, type at offset+8)
    bool isRegister;
    X64Reg reg;
};

/**
 * JIT Value representation during compilation.
 * Holds register for value bits and register for type tag.
 */
struct JITValue {
    X64Reg valueReg;
    X64Reg typeReg;
};

/**
 * Native Compiler for Nevaarize.
 * Compiles AST directly to executable x86-64 machine code.
 */
class JIT {
public:
    JIT();
    ~JIT();

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
    
    // User function storage
    struct FuncInfo {
        NodeIndex bodyIndex;
        std::vector<std::string> paramNames;
        size_t compiledOffset;
        bool isCompiled;
        bool isAsync;
        const AST* sourceAST;  // Which AST contains this function (nullptr = currentAST)
    };
    std::unordered_map<std::string, FuncInfo> userFunctions;
    
    // Struct storage
    struct StructInfo {
        std::vector<std::string> fieldNames;
        size_t size;
    };
    std::unordered_map<std::string, StructInfo> structs;
    
    // Module storage (for IMPORT_FILE)
    struct ModuleInfo {
        std::string filePath;
        std::unordered_map<std::string, NodeIndex> exportedFunctions;
        std::unordered_map<std::string, int32_t> exportedVariables;
    };
    std::unordered_map<std::string, ModuleInfo> modules;
    std::unordered_set<std::string> importedFiles;
    std::unordered_map<std::string, AST> importedASTs;  // Store imported ASTs
    
    std::unordered_map<std::string, std::string> stdlibAliases;
    std::unordered_set<std::string> currentlyCompiling;
    const AST* currentAST;
    bool inFunctionCall;

    // Compile-time type tracking for optimization
    std::unordered_set<std::string> knownIntVars;

    // Code generation helpers
    void emitPrologue();
    void emitEpilogue();
    X64Reg allocateReg();
    void freeReg(X64Reg reg);
    int32_t allocateStackSlot();

    // AST compilation - expressions
    JITValue compileExpr(const AST& ast, NodeIndex idx);
    bool isStaticInt(const AST& ast, NodeIndex idx) const;
    
    // AST compilation - statements
    void compileStatement(const AST& ast, NodeIndex idx);
    void compileAssignment(const AST& ast, NodeIndex idx);
    void compileBlock(const AST& ast, NodeIndex idx);
    void compileIf(const AST& ast, NodeIndex idx);
    void compileWhile(const AST& ast, NodeIndex idx);
    void compileFor(const AST& ast, NodeIndex idx);
    void compileReturn(const AST& ast, NodeIndex idx);
    void compileCall(const AST& ast, NodeIndex idx);
    void compileFuncDecl(const AST& ast, NodeIndex idx, bool isAsync = false);
    JITValue compileUserCall(const AST& ast, NodeIndex idx, const std::string& funcName);
    
    // Native function call emission
    void emitPrintInt(X64Reg valueReg);
    void emitPrintIntNoNewline(X64Reg valueReg);
    void emitPrintString(const std::string& str);
    void emitPrintStringNoNewline(const std::string& str);
    void emitPrintSpace();
    void emitPrintNewline();

    // Register allocation state
    bool regInUse[16];
};

} // namespace nevaarize

#endif // NEVAARIZE_JIT_HPP
