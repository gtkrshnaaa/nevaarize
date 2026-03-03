/**
 * Grammar.hpp - Nevaarize Static Analyzer
 *
 * Static analysis tool for Nevaarize source code.
 * Validates syntax correctness, naming conventions, semantic integrity,
 * and provides SIMD/AVX-aware performance optimization hints.
 *
 * Usage: nevaarize -grammar file.nva
 *        nevaarize -grammar folder/*.nva
 */

#ifndef NEVAARIZE_GRAMMAR_HPP
#define NEVAARIZE_GRAMMAR_HPP

#include "ast.hpp"
#include "token.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace nevaarize {

/**
 * Diagnostic severity levels.
 */
enum class DiagLevel : uint8_t {
    ERROR,      // Must be fixed — code will not run correctly
    WARNING,    // Should be fixed — potential bugs or bad practices
    PERF        // Performance hint — SIMD/AVX optimization opportunity
};

/**
 * Single diagnostic message produced by analysis.
 */
struct Diagnostic {
    DiagLevel level;
    int32_t line;
    int32_t column;
    std::string message;
    std::string ruleId;     // Machine-readable rule identifier (e.g., "naming.camelCase")
};

/**
 * Analysis result for a single file.
 */
struct AnalysisResult {
    std::string filePath;
    std::vector<Diagnostic> diagnostics;
    size_t errorCount = 0;
    size_t warningCount = 0;
    size_t perfCount = 0;
};

/**
 * GrammarChecker - Static analyzer for Nevaarize source code.
 *
 * Performs multi-pass analysis without executing the code:
 *   Pass 1: Syntax & structure validation
 *   Pass 2: Naming convention enforcement
 *   Pass 3: Semantic analysis (scope, types, reachability)
 *   Pass 4: Performance & best practices (SIMD/AVX awareness)
 */
class GrammarChecker {
public:
    GrammarChecker();

    /**
     * Analyze a single source file.
     * Returns the analysis result with all diagnostics.
     */
    AnalysisResult analyzeFile(const std::string& filePath);

    /**
     * Analyze source code from a string (for testing/stdin).
     */
    AnalysisResult analyzeSource(const std::string& source, const std::string& filePath = "<stdin>");

    /**
     * Print formatted diagnostics to stdout.
     * Returns the total error count.
     */
    static int printResult(const AnalysisResult& result);

    /**
     * Print a summary line for batch mode.
     */
    static void printSummary(size_t totalFiles, size_t totalErrors,
                             size_t totalWarnings, size_t totalPerf);

private:
    // Built-in function names that do not require declaration
    std::unordered_set<std::string> builtinFunctions;

    // Known stdlib module names
    std::unordered_set<std::string> knownStdlibModules;

    // Known stdlib method names per module
    std::unordered_map<std::string, std::unordered_set<std::string>> stdlibMethods;

    // Scope tracking for semantic analysis
    struct Scope {
        std::unordered_set<std::string> variables;
        std::unordered_set<std::string> functions;
        std::unordered_set<std::string> readVariables;
        bool isFunction = false;
    };

    std::vector<Scope> scopeStack;
    std::unordered_map<std::string, std::vector<std::string>> structFields;
    std::unordered_set<std::string> importAliases;
    std::vector<Diagnostic> diagnostics;

    // Initialization
    void initBuiltins();

    // Analysis passes
    void passNaming(const AST& ast, NodeIndex idx);
    void passSemantics(const AST& ast, NodeIndex idx);
    void passPerformance(const AST& ast, NodeIndex idx);

    // Scope management
    void pushScope(bool isFunction = false);
    void popScope();
    void declareVariable(const std::string& name);
    void declareFunction(const std::string& name);
    bool isVariableDeclared(const std::string& name) const;
    bool isFunctionDeclared(const std::string& name) const;
    void markVariableRead(const std::string& name);

    // Naming helpers
    bool isLowerCamelCase(const std::string& name) const;
    bool isUpperCamelCase(const std::string& name) const;

    // Diagnostics
    void addDiag(DiagLevel level, int32_t line, int32_t col,
                 const std::string& message, const std::string& ruleId);

    // AST traversal helpers
    void walkBlock(const AST& ast, NodeIndex idx,
                   void (GrammarChecker::*visitor)(const AST&, NodeIndex));
    void walkChildren(const AST& ast, NodeIndex idx,
                      void (GrammarChecker::*visitor)(const AST&, NodeIndex));
    void walkExpression(const AST& ast, NodeIndex idx,
                        void (GrammarChecker::*visitor)(const AST&, NodeIndex));
};

} // namespace nevaarize

#endif // NEVAARIZE_GRAMMAR_HPP
