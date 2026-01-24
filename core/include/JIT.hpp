/**
 * JIT.hpp - Nevaarize JIT Compiler Interface
 *
 * Main JIT compilation pipeline coordinating lexer, parser,
 * IR generation, optimization, and native code generation.
 */

#ifndef NEVAARIZE_JIT_HPP
#define NEVAARIZE_JIT_HPP

#include "Lexer.hpp"
#include "Parser.hpp"
#include "AST.hpp"
#include "IR.hpp"
#include "CodeGen.hpp"
#include "Runtime.hpp"
#include "Value.hpp"
#include <string>
#include <memory>
#include <unordered_map>
#include <filesystem>

namespace nevaarize {

/**
 * Compilation result from JIT.
 */
struct CompileResult {
    bool success;
    std::string error;
    void* code;
    size_t codeSize;

    CompileResult() : success(false), code(nullptr), codeSize(0) {}
};

/**
 * JIT compiler for Nevaarize.
 * Handles the complete compilation pipeline.
 */
class JITCompiler {
public:
    JITCompiler();
    ~JITCompiler();

    /**
     * Compile source code to native machine code.
     */
    CompileResult compile(const std::string& source);

    /**
     * Compile a single function from IR.
     */
    CompileResult compileFunction(const IRFunction& func);

    /**
     * Execute compiled code.
     */
    Value execute(void* code);

    /**
     * Get the evaluator for tree-walk interpretation.
     */
    RuntimeContext& getRuntime() { return runtime; }

private:
    RuntimeContext runtime;
    std::vector<std::unique_ptr<ExecutableMemory>> compiledCode;
    std::unordered_map<std::string, void*> compiledFunctions;
};

/**
 * Evaluator for tree-walk interpretation.
 * Used as fallback and for debugging.
 */
class Evaluator {
public:
    Evaluator();

    /**
     * Execute an AST.
     */
    Value execute(std::shared_ptr<const AST> tree);

    /**
     * Execute an AST with source file path for import resolution.
     */
    Value execute(std::shared_ptr<const AST> tree, const std::filesystem::path& filePath);

    /**
     * Register a native function.
     */
    void registerNative(const std::string& name, NativeFunction fn);

    /**
     * Register a module with functions.
     */
    void registerModule(const std::string& alias,
                        const std::unordered_map<std::string, NativeFunction>& functions);

    /**
     * Get the global environment.
     */
    std::shared_ptr<Environment> getGlobalEnv() { return globalEnv; }

    /**
     * Get the current source file path (for relative path resolution).
     */
    std::filesystem::path getCurrentFilePath() const { return currentFilePath; }

private:
    std::shared_ptr<const AST> ast;
    std::shared_ptr<Environment> globalEnv;
    std::shared_ptr<Environment> environment;
    std::unordered_map<std::string, StructDef> structs;
    std::unordered_map<std::string, std::shared_ptr<Environment>> modules;
    std::filesystem::path currentFilePath;

    void setupStandardLibrary();
    Value evaluate(NodeIndex idx);
    Value evalLiteral(const ASTNode& node);
    Value evalIdentifier(const ASTNode& node);
    Value evalBinaryOp(const ASTNode& node);
    Value evalUnaryOp(const ASTNode& node);
    Value evalCall(const ASTNode& node);
    Value evalMemberAccess(const ASTNode& node);
    Value evalIndexAccess(const ASTNode& node);
    Value evalArrayLiteral(const ASTNode& node);
    Value evalAwait(const ASTNode& node);
    void execBlock(const ASTNode& node);
    void execVarAssign(const ASTNode& node);
    void execMemberAssign(const ASTNode& node);
    void execIndexAssign(const ASTNode& node);
    void execIf(const ASTNode& node);
    void execFor(const ASTNode& node);
    void execWhile(const ASTNode& node);
    void execReturn(const ASTNode& node);
    void execFuncDecl(const ASTNode& node);
    void execStructDecl(const ASTNode& node);
    void execImportStdlib(const ASTNode& node);
    void execImportFile(const ASTNode& node);
    Value callFunction(const Value& callee, const std::vector<Value>& args, int line, int column);
};

} // namespace nevaarize

#endif // NEVAARIZE_JIT_HPP
