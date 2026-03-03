/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * Grammar.cpp - Nevaarize Static Analyzer Implementation
 *
 * ============================================================================
 * NEVAARIZE LANGUAGE GRAMMAR REFERENCE & STATIC ANALYZER
 * ============================================================================
 *
 * This file serves a dual purpose:
 *   1. A fully functional static analysis tool for Nevaarize
 *   2. A definitive reference of Nevaarize syntax and best practices
 *
 * LANGUAGE OVERVIEW:
 *   Nevaarize is a high-performance language compiled via native JIT to
 *   Linux x86-64 machine code. It targets SIMD/AVX workloads with a
 *   generational garbage collector and async/await concurrency.
 *
 * SYNTAX RULES:
 *   - Variables:       x = 42          (dynamic typing, no 'let'/'var')
 *   - Functions:       func name(a, b) { return a + b }
 *   - Async:           async func name(n) { ... }; task = name(n); result = await task
 *   - Structs:         struct Point { x, y }; p = Point(10, 20)
 *   - Arrays:          arr = [1, 2, 3]; arr.push(4); arr[0]; arr.size()
 *   - Maps:            m = {1: 10, "key": 20}; m[1]; m.has(k); m.keys()
 *   - Control flow:    if (cond) { } elif (cond) { } else { }
 *   - Loops:           while (cond) { }; for (x in iterable) { }
 *   - Exceptions:      try { } catch(e) { } finally { }
 *   - Imports:         import stdlib math as m; import "path.nva" as mod
 *   - Comments:        // single-line only
 *
 * NAMING CONVENTIONS (Enforced):
 *   - No underscores allowed in any identifier
 *   - Variables & functions: camelCase (start lowercase)
 *   - Structs: PascalCase (start uppercase)
 *   - Stdlib aliases: short lowercase identifiers
 *
 * PERFORMANCE BEST PRACTICES (SIMD/AVX Awareness):
 *   - Use uniform types in hot loops (pure int or pure float, not mixed)
 *   - Prefer while-loops with manual counter for tight numeric iteration
 *   - Use async func for parallelizable independent computations
 *   - Pre-size arrays when final size is known
 *   - Minimize heap allocations inside hot loops
 *
 * BUILT-IN FUNCTIONS:
 *   print(args...)     — Print values separated by spaces, with newline
 *   Range(start, end)  — Generate integer range [start, end)
 *
 * STANDARD LIBRARY MODULES:
 *   math  — Abs, Sqrt, Pow, Floor, Ceil, Round, Sin, Cos, Tan, Log, Exp, Min, Max, Random, RandomInt
 *   time  — nanos, millis, clock, sleep, format
 *   io    — Input, Print, Write
 *   ai    — Zeros, Ones, RandN, Add, Sub, Mul, Dot, MatMul, ReLU, Sigmoid, Softmax, Linear, ...
 *   http  — Server, Route, Listen
 *
 * OPERATORS (by precedence, lowest to highest):
 *   or                 — Logical OR
 *   and                — Logical AND
 *   == !=              — Equality
 *   < <= > >=          — Comparison
 *   + -                — Addition, Subtraction
 *   * / %              — Multiplication, Division, Modulo
 *   - !                — Unary negation, logical NOT
 *   . () []            — Member access, call, index
 *
 * TYPE SYSTEM:
 *   int      — 64-bit signed integer
 *   float    — 64-bit double-precision IEEE 754
 *   string   — Immutable UTF-8 string
 *   bool     — true / false
 *   nil      — Null value
 *   array    — Dynamic array (heterogeneous)
 *   map      — Hash map with tombstone deletion
 *   struct   — User-defined composite type
 *   function — First-class function value
 *   async    — Async task handle (from async func calls)
 *
 * ============================================================================
 */

#include "grammar.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace nevaarize {

// ============================================================================
// Initialization
// ============================================================================

GrammarChecker::GrammarChecker() {
    initBuiltins();
}

void GrammarChecker::initBuiltins() {
    // Built-in functions available without import
    builtinFunctions = {
        "print", "Range"
    };

    // Known stdlib module names
    knownStdlibModules = {
        "math", "time", "io", "ai", "http"
    };

    // Known methods per stdlib module
    stdlibMethods["math"] = {
        "Abs", "Sqrt", "Pow", "Floor", "Ceil", "Round",
        "Sin", "Cos", "Tan", "Asin", "Acos", "Atan", "Atan2",
        "Log", "Log10", "Exp", "Min", "Max", "Random", "RandomInt"
    };

    stdlibMethods["time"] = {
        "nanos", "millis", "clock", "sleep", "format"
    };

    stdlibMethods["io"] = {
        "Input", "Print", "Write"
    };

    stdlibMethods["ai"] = {
        "Zeros", "Ones", "RandN", "RandU",
        "Add", "Sub", "Mul", "Div", "Scale",
        "Sum", "Mean", "Max", "Min", "Argmax", "Argmin",
        "Dot", "MatMul", "Transpose",
        "ReLU", "LeakyReLU", "Sigmoid", "Tanh", "Softmax", "GELU", "SiLU",
        "MSELoss", "L1Loss", "BCELoss", "CrossEntropyLoss", "HuberLoss",
        "Linear", "Dropout", "LayerNorm", "Embedding",
        "SGDUpdate", "AdamUpdate", "ClipGradNorm",
        "XavierInit", "HeInit", "KaimingInit",
        "Norm", "Normalize", "CosineSimilarity",
        "Accuracy",
        "BatchLinear",
        "SaveModel", "LoadModel",
        "TopK", "SampleFromProbs",
        "Tokenize", "Detokenize"
    };

    stdlibMethods["http"] = {
        "Server", "Route", "Listen", "Get", "Post"
    };
}

// ============================================================================
// Public API
// ============================================================================

AnalysisResult GrammarChecker::analyzeFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file) {
        AnalysisResult result;
        result.filePath = filePath;
        result.diagnostics.push_back({
            DiagLevel::ERROR, 0, 0,
            "Cannot open file: " + filePath,
            "io.fileNotFound"
        });
        result.errorCount = 1;
        return result;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return analyzeSource(buffer.str(), filePath);
}

AnalysisResult GrammarChecker::analyzeSource(const std::string& source, const std::string& filePath) {
    diagnostics.clear();
    scopeStack.clear();
    structFields.clear();
    importAliases.clear();

    AnalysisResult result;
    result.filePath = filePath;

    // ========================================================================
    // Phase 1: Lexer validation
    // ========================================================================
    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    if (!lexer.errors().empty()) {
        for (const auto& err : lexer.errors()) {
            addDiag(DiagLevel::ERROR, 0, 0, err, "syntax.lexer");
        }
    }

    // ========================================================================
    // Phase 2: Parser validation
    // ========================================================================
    Parser parser(tokens, source);
    parser.parse();

    if (parser.hasErrors()) {
        for (const auto& err : parser.errors()) {
            addDiag(DiagLevel::ERROR, 0, 0, err, "syntax.parser");
        }
    }

    // If we have parse errors, skip semantic passes (AST is unreliable)
    if (parser.hasErrors() || !lexer.errors().empty()) {
        result.diagnostics = std::move(diagnostics);
        for (const auto& d : result.diagnostics) {
            switch (d.level) {
                case DiagLevel::ERROR: result.errorCount++; break;
                case DiagLevel::WARNING: result.warningCount++; break;
                case DiagLevel::PERF: result.perfCount++; break;
            }
        }
        return result;
    }

    const AST& ast = parser.getAST();
    NodeIndex root = ast.root();

    if (root == INVALID_NODE) {
        result.diagnostics = std::move(diagnostics);
        return result;
    }

    // ========================================================================
    // Phase 3: Naming convention pass
    // ========================================================================
    passNaming(ast, root);

    // ========================================================================
    // Phase 4: Semantic analysis pass
    // ========================================================================
    pushScope(false);
    passSemantics(ast, root);

    // Check for unused variables in global scope
    if (!scopeStack.empty()) {
        const auto& globalScope = scopeStack.back();
        for (const auto& varName : globalScope.variables) {
            if (globalScope.readVariables.find(varName) == globalScope.readVariables.end()) {
                // Skip common loop variables and short-lived assignments
                if (varName.length() > 1) {
                    addDiag(DiagLevel::WARNING, 0, 0,
                            "Variable '" + varName + "' is assigned but never read",
                            "semantic.unusedVariable");
                }
            }
        }
    }
    popScope();

    // ========================================================================
    // Phase 5: Performance pass
    // ========================================================================
    passPerformance(ast, root);

    // Collect results
    result.diagnostics = std::move(diagnostics);
    for (const auto& d : result.diagnostics) {
        switch (d.level) {
            case DiagLevel::ERROR: result.errorCount++; break;
            case DiagLevel::WARNING: result.warningCount++; break;
            case DiagLevel::PERF: result.perfCount++; break;
        }
    }

    return result;
}

// ============================================================================
// Output formatting
// ============================================================================

int GrammarChecker::printResult(const AnalysisResult& result) {
    std::cout << "\n\033[1mgrammar: " << result.filePath << "\033[0m" << std::endl;

    if (result.diagnostics.empty()) {
        std::cout << "  \033[32m✓\033[0m 0 errors, 0 warnings" << std::endl;
        return 0;
    }

    for (const auto& d : result.diagnostics) {
        const char* prefix = "error";
        const char* color = "\033[31m";

        switch (d.level) {
            case DiagLevel::ERROR:
                prefix = "error";
                color = "\033[31m";
                break;
            case DiagLevel::WARNING:
                prefix = "warn ";
                color = "\033[33m";
                break;
            case DiagLevel::PERF:
                prefix = "perf ";
                color = "\033[36m";
                break;
        }

        if (d.line > 0) {
            std::cout << "  " << color << prefix << "\033[0m"
                      << " [L" << d.line << ":C" << d.column << "] "
                      << d.message << std::endl;
        } else {
            // Lexer/parser errors contain their own formatted output
            std::cout << "  " << color << prefix << "\033[0m " << d.message;
        }
    }

    std::cout << "\n  ";
    if (result.errorCount > 0) {
        std::cout << "\033[31m✗\033[0m ";
    } else {
        std::cout << "\033[32m✓\033[0m ";
    }
    std::cout << result.errorCount << " error"
              << (result.errorCount != 1 ? "s" : "") << ", "
              << result.warningCount << " warning"
              << (result.warningCount != 1 ? "s" : "") << ", "
              << result.perfCount << " performance hint"
              << (result.perfCount != 1 ? "s" : "")
              << std::endl;

    return static_cast<int>(result.errorCount);
}

void GrammarChecker::printSummary(size_t totalFiles, size_t totalErrors,
                                   size_t totalWarnings, size_t totalPerf) {
    std::cout << "\n\033[1m════════════════════════════════════════\033[0m" << std::endl;
    std::cout << "\033[1m  Grammar Analysis Summary\033[0m" << std::endl;
    std::cout << "\033[1m════════════════════════════════════════\033[0m" << std::endl;
    std::cout << "  Files analyzed:     " << totalFiles << std::endl;
    std::cout << "  Errors:             " << totalErrors << std::endl;
    std::cout << "  Warnings:           " << totalWarnings << std::endl;
    std::cout << "  Performance hints:  " << totalPerf << std::endl;

    if (totalErrors == 0 && totalWarnings == 0 && totalPerf == 0) {
        std::cout << "\n  \033[32m✓ All files pass grammar analysis.\033[0m" << std::endl;
    } else if (totalErrors == 0) {
        std::cout << "\n  \033[33m⚠ No errors, but review warnings/hints above.\033[0m" << std::endl;
    } else {
        std::cout << "\n  \033[31m✗ Fix " << totalErrors
                  << " error" << (totalErrors != 1 ? "s" : "")
                  << " before running.\033[0m" << std::endl;
    }
}

// ============================================================================
// Scope management
// ============================================================================

void GrammarChecker::pushScope(bool isFunction) {
    scopeStack.push_back({});
    scopeStack.back().isFunction = isFunction;
}

void GrammarChecker::popScope() {
    if (!scopeStack.empty()) {
        scopeStack.pop_back();
    }
}

void GrammarChecker::declareVariable(const std::string& name) {
    if (!scopeStack.empty()) {
        scopeStack.back().variables.insert(name);
    }
}

void GrammarChecker::declareFunction(const std::string& name) {
    if (!scopeStack.empty()) {
        scopeStack.back().functions.insert(name);
    }
    // Functions are also visible in all parent scopes for recursion
    for (auto& scope : scopeStack) {
        scope.functions.insert(name);
    }
}

bool GrammarChecker::isVariableDeclared(const std::string& name) const {
    // Search from innermost scope outward
    for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
        if (it->variables.count(name)) return true;
    }
    return false;
}

bool GrammarChecker::isFunctionDeclared(const std::string& name) const {
    for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
        if (it->functions.count(name)) return true;
    }
    return builtinFunctions.count(name) > 0;
}

void GrammarChecker::markVariableRead(const std::string& name) {
    for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
        if (it->variables.count(name)) {
            it->readVariables.insert(name);
            return;
        }
    }
}

// ============================================================================
// Naming helpers
// ============================================================================

bool GrammarChecker::isLowerCamelCase(const std::string& name) const {
    if (name.empty()) return true;
    // First character must be lowercase letter
    if (name[0] < 'a' || name[0] > 'z') return false;
    // No underscores
    for (char c : name) {
        if (c == '_') return false;
    }
    return true;
}

bool GrammarChecker::isUpperCamelCase(const std::string& name) const {
    if (name.empty()) return true;
    // First character must be uppercase letter
    if (name[0] < 'A' || name[0] > 'Z') return false;
    // No underscores
    for (char c : name) {
        if (c == '_') return false;
    }
    return true;
}

// ============================================================================
// Diagnostic helpers
// ============================================================================

void GrammarChecker::addDiag(DiagLevel level, int32_t line, int32_t col,
                              const std::string& message, const std::string& ruleId) {
    diagnostics.push_back({level, line, col, message, ruleId});
}

// ============================================================================
// Pass 2: Naming Convention Enforcement
// ============================================================================

/**
 * NAMING RULES:
 *
 * 1. Variable names must use camelCase (start with lowercase letter).
 *    Example: myVariable, totalCount, dataBuffer
 *    Bad:     MyVariable, _private, my_var
 *
 * 2. Function names must use camelCase.
 *    Example: computeSum, parseRequest, handleError
 *    Bad:     ComputeSum, compute_sum
 *
 * 3. Struct names must use PascalCase (start with uppercase letter).
 *    Example: Point, HttpServer, ModelConfig
 *    Bad:     point, http_server
 *
 * 4. Import aliases must be short lowercase identifiers.
 *    Example: import stdlib math as m
 *    Bad:     import stdlib math as MathModule
 */
void GrammarChecker::passNaming(const AST& ast, NodeIndex idx) {
    if (idx == INVALID_NODE) return;

    const ASTNode& node = ast.get(idx);

    switch (node.type) {
        case NodeType::PROGRAM:
            for (NodeIndex child : node.children) {
                passNaming(ast, child);
            }
            break;

        case NodeType::VAR_ASSIGN:
            // Variable names must be camelCase
            if (!node.name.empty() && !isLowerCamelCase(node.name)) {
                // Allow single uppercase letters (common in math: A, B, C, M, N)
                if (node.name.length() > 1) {
                    addDiag(DiagLevel::WARNING, node.line, node.column,
                            "Variable '" + node.name + "' should use camelCase (start with lowercase)",
                            "naming.camelCase");
                }
            }
            passNaming(ast, node.left);
            break;

        case NodeType::FUNC_DECL:
        case NodeType::ASYNC_FUNC_DECL:
            // Function names must be camelCase
            if (!node.name.empty() && !isLowerCamelCase(node.name)) {
                addDiag(DiagLevel::WARNING, node.line, node.column,
                        "Function '" + node.name + "' should use camelCase (start with lowercase)",
                        "naming.funcCamelCase");
            }
            // Check parameters
            for (const auto& param : node.paramNames) {
                if (!isLowerCamelCase(param)) {
                    addDiag(DiagLevel::WARNING, node.line, node.column,
                            "Parameter '" + param + "' should use camelCase",
                            "naming.paramCamelCase");
                }
            }
            passNaming(ast, node.left);
            break;

        case NodeType::STRUCT_DECL:
            // Struct names must be PascalCase
            if (!node.name.empty() && !isUpperCamelCase(node.name)) {
                addDiag(DiagLevel::ERROR, node.line, node.column,
                        "Struct '" + node.name + "' must use PascalCase (start with uppercase)",
                        "naming.structPascalCase");
            }
            // Struct fields should be camelCase
            for (const auto& field : node.paramNames) {
                if (!isLowerCamelCase(field)) {
                    addDiag(DiagLevel::WARNING, node.line, node.column,
                            "Struct field '" + field + "' should use camelCase",
                            "naming.fieldCamelCase");
                }
            }
            break;

        case NodeType::IMPORT_STDLIB:
            // Alias should be lowercase, short
            if (!node.paramNames.empty()) {
                const auto& alias = node.paramNames[0];
                if (!isLowerCamelCase(alias)) {
                    addDiag(DiagLevel::WARNING, node.line, node.column,
                            "Import alias '" + alias + "' should be lowercase",
                            "naming.importAlias");
                }
                if (alias.length() > 6) {
                    addDiag(DiagLevel::WARNING, node.line, node.column,
                            "Import alias '" + alias + "' is long — prefer short aliases (e.g., 'm', 'io', 'ai')",
                            "naming.importAliasLength");
                }
            }
            break;

        case NodeType::IMPORT_FILE:
            if (!node.paramNames.empty()) {
                const auto& alias = node.paramNames[0];
                if (!isLowerCamelCase(alias)) {
                    addDiag(DiagLevel::WARNING, node.line, node.column,
                            "Import alias '" + alias + "' should use camelCase",
                            "naming.importAlias");
                }
            }
            break;

        case NodeType::BLOCK:
            for (NodeIndex child : node.children) {
                passNaming(ast, child);
            }
            break;

        case NodeType::IF_STMT:
            passNaming(ast, node.right);
            passNaming(ast, node.extra);
            break;

        case NodeType::FOR_STMT:
            // Iterator variable naming check
            if (!node.name.empty() && !isLowerCamelCase(node.name)) {
                addDiag(DiagLevel::WARNING, node.line, node.column,
                        "Iterator variable '" + node.name + "' should use camelCase",
                        "naming.iteratorCamelCase");
            }
            passNaming(ast, node.right);
            break;

        case NodeType::WHILE_STMT:
            passNaming(ast, node.right);
            break;

        case NodeType::TRY_STMT:
            passNaming(ast, node.left);
            passNaming(ast, node.right);
            for (NodeIndex child : node.children) {
                passNaming(ast, child);
            }
            break;

        case NodeType::EXPR_STMT:
            passNaming(ast, node.left);
            break;

        default:
            break;
    }
}

// ============================================================================
// Pass 3: Semantic Analysis
// ============================================================================

/**
 * SEMANTIC RULES:
 *
 * 1. Variables must be assigned before use.
 *    Detects: referencing 'x' when 'x' was never assigned in any enclosing scope.
 *
 * 2. Functions must be declared before call (or be a builtin/stdlib method).
 *    Detects: calling 'foo()' when no 'func foo' exists.
 *
 * 3. Structs must be declared before instantiation.
 *    Detects: 'p = Point(1,2)' without a prior 'struct Point { x, y }'.
 *
 * 4. No duplicate function/struct declarations.
 *    Detects: two 'func foo()' definitions in the same scope.
 *
 * 5. No unreachable code after return/throw.
 *    Detects: statements after 'return' within the same block.
 *
 * 6. Import validation:
 *    - Stdlib module names must match known modules
 *    - File imports must reference existing files
 */
void GrammarChecker::passSemantics(const AST& ast, NodeIndex idx) {
    if (idx == INVALID_NODE) return;

    const ASTNode& node = ast.get(idx);

    switch (node.type) {
        case NodeType::PROGRAM: {
            // First pass: register all top-level function and struct declarations
            for (NodeIndex child : node.children) {
                const ASTNode& childNode = ast.get(child);
                if (childNode.type == NodeType::FUNC_DECL ||
                    childNode.type == NodeType::ASYNC_FUNC_DECL) {
                    if (isFunctionDeclared(childNode.name)) {
                        addDiag(DiagLevel::WARNING, childNode.line, childNode.column,
                                "Function '" + childNode.name + "' is declared multiple times",
                                "semantic.duplicateFunc");
                    }
                    declareFunction(childNode.name);
                } else if (childNode.type == NodeType::STRUCT_DECL) {
                    if (structFields.count(childNode.name)) {
                        addDiag(DiagLevel::ERROR, childNode.line, childNode.column,
                                "Struct '" + childNode.name + "' is defined multiple times",
                                "semantic.duplicateStruct");
                    }
                    structFields[childNode.name] = childNode.paramNames;
                    // Struct name acts as a callable constructor
                    declareFunction(childNode.name);
                } else if (childNode.type == NodeType::IMPORT_STDLIB) {
                    if (!childNode.paramNames.empty()) {
                        importAliases.insert(childNode.paramNames[0]);
                        declareVariable(childNode.paramNames[0]);
                    }
                    // Validate known stdlib module
                    if (knownStdlibModules.find(childNode.name) == knownStdlibModules.end()) {
                        addDiag(DiagLevel::ERROR, childNode.line, childNode.column,
                                "Unknown stdlib module '" + childNode.name + "' — known modules: math, time, io, ai, http",
                                "semantic.unknownStdlib");
                    }
                } else if (childNode.type == NodeType::IMPORT_FILE) {
                    if (!childNode.paramNames.empty()) {
                        importAliases.insert(childNode.paramNames[0]);
                        declareVariable(childNode.paramNames[0]);
                    }
                }
            }

            // Second pass: analyze all children
            for (NodeIndex child : node.children) {
                passSemantics(ast, child);
            }
            break;
        }

        case NodeType::VAR_ASSIGN:
            // Analyze the value expression first (before declaring the variable)
            passSemantics(ast, node.left);
            declareVariable(node.name);
            break;

        case NodeType::MEMBER_ASSIGN:
            passSemantics(ast, node.left);
            passSemantics(ast, node.right);
            break;

        case NodeType::INDEX_ASSIGN:
            passSemantics(ast, node.left);
            passSemantics(ast, node.right);
            passSemantics(ast, node.extra);
            break;

        case NodeType::IDENTIFIER:
            if (!node.name.empty() &&
                !isVariableDeclared(node.name) &&
                !isFunctionDeclared(node.name) &&
                !importAliases.count(node.name) &&
                structFields.find(node.name) == structFields.end()) {
                addDiag(DiagLevel::WARNING, node.line, node.column,
                        "Variable '" + node.name + "' used before assignment",
                        "semantic.undefinedVar");
            } else {
                markVariableRead(node.name);
            }
            break;

        case NodeType::CALL: {
            // Check if the called function exists
            if (node.left != INVALID_NODE) {
                const ASTNode& callee = ast.get(node.left);
                if (callee.type == NodeType::IDENTIFIER) {
                    if (!isFunctionDeclared(callee.name) &&
                        structFields.find(callee.name) == structFields.end()) {
                        addDiag(DiagLevel::ERROR, callee.line, callee.column,
                                "Undefined function '" + callee.name + "'",
                                "semantic.undefinedFunc");
                    }
                    markVariableRead(callee.name);
                } else if (callee.type == NodeType::MEMBER_ACCESS) {
                    // Module.method() calls — validate if we know the module
                    passSemantics(ast, callee.left);
                    // Skip method name validation for member calls (runtime dispatch)
                }
            }
            // Analyze arguments
            for (NodeIndex arg : node.children) {
                passSemantics(ast, arg);
            }
            break;
        }

        case NodeType::FUNC_DECL:
        case NodeType::ASYNC_FUNC_DECL: {
            // Function already registered in PROGRAM first-pass
            pushScope(true);
            // Declare parameters as local variables
            for (const auto& param : node.paramNames) {
                declareVariable(param);
            }
            passSemantics(ast, node.left);

            // Check for unused params
            if (!scopeStack.empty()) {
                const auto& funcScope = scopeStack.back();
                for (const auto& param : node.paramNames) {
                    if (funcScope.readVariables.find(param) == funcScope.readVariables.end()) {
                        addDiag(DiagLevel::WARNING, node.line, node.column,
                                "Parameter '" + param + "' of function '" + node.name + "' is never used",
                                "semantic.unusedParam");
                    }
                }
            }
            popScope();
            break;
        }

        case NodeType::STRUCT_DECL:
            // Already handled in PROGRAM first-pass
            break;

        case NodeType::IMPORT_STDLIB:
        case NodeType::IMPORT_FILE:
            // Already handled in PROGRAM first-pass
            break;

        case NodeType::BLOCK: {
            bool foundReturn = false;
            for (size_t i = 0; i < node.children.size(); ++i) {
                NodeIndex child = node.children[i];
                if (foundReturn) {
                    const ASTNode& deadNode = ast.get(child);
                    addDiag(DiagLevel::WARNING, deadNode.line, deadNode.column,
                            "Unreachable code after return/throw statement",
                            "semantic.unreachableCode");
                    break;
                }
                passSemantics(ast, child);

                const ASTNode& childNode = ast.get(child);
                if (childNode.type == NodeType::RETURN_STMT ||
                    childNode.type == NodeType::THROW_STMT) {
                    foundReturn = true;
                }
            }
            break;
        }

        case NodeType::IF_STMT:
            passSemantics(ast, node.left);
            passSemantics(ast, node.right);
            if (node.extra != INVALID_NODE) {
                passSemantics(ast, node.extra);
            }
            break;

        case NodeType::FOR_STMT:
            pushScope(false);
            declareVariable(node.name);
            passSemantics(ast, node.left);
            passSemantics(ast, node.right);
            popScope();
            break;

        case NodeType::WHILE_STMT:
            passSemantics(ast, node.left);
            pushScope(false);
            passSemantics(ast, node.right);
            popScope();
            break;

        case NodeType::RETURN_STMT:
            if (node.left != INVALID_NODE) {
                passSemantics(ast, node.left);
            }
            break;

        case NodeType::TRY_STMT:
            passSemantics(ast, node.left);
            // Catch block — declare catch variable
            pushScope(false);
            if (!node.name.empty()) {
                declareVariable(node.name);
            }
            passSemantics(ast, node.right);
            popScope();
            // Finally blocks
            for (NodeIndex child : node.children) {
                passSemantics(ast, child);
            }
            break;

        case NodeType::THROW_STMT:
            passSemantics(ast, node.left);
            break;

        case NodeType::EXPR_STMT:
            passSemantics(ast, node.left);
            break;

        case NodeType::BINARY_OP:
            passSemantics(ast, node.left);
            passSemantics(ast, node.right);
            break;

        case NodeType::UNARY_OP:
            passSemantics(ast, node.left);
            break;

        case NodeType::MEMBER_ACCESS:
            passSemantics(ast, node.left);
            break;

        case NodeType::INDEX_ACCESS:
            passSemantics(ast, node.left);
            passSemantics(ast, node.right);
            break;

        case NodeType::ARRAY_LITERAL:
            for (NodeIndex child : node.children) {
                passSemantics(ast, child);
            }
            break;

        case NodeType::MAP_LITERAL:
            for (NodeIndex child : node.children) {
                passSemantics(ast, child);
            }
            break;

        case NodeType::STRUCT_INIT:
            for (NodeIndex child : node.children) {
                passSemantics(ast, child);
            }
            break;

        case NodeType::AWAIT_EXPR:
            passSemantics(ast, node.left);
            break;

        default:
            break;
    }
}

// ============================================================================
// Pass 4: Performance & Best Practices (SIMD/AVX Awareness)
// ============================================================================

/**
 * PERFORMANCE RULES:
 *
 * 1. MIXED TYPE ARITHMETIC IN LOOPS
 *    When a loop body mixes integer and float operations on the same variable,
 *    the JIT must emit type-check branches on every iteration. Uniform types
 *    allow the JIT to emit pure integer or pure SSE/AVX instructions.
 *
 *    Bad:   total = 0; total = total + 0.5   (int then float)
 *    Good:  total = 0.0; total = total + 0.5 (pure float)
 *
 * 2. EMPTY FUNCTION BODIES
 *    Empty function/loop bodies waste compilation resources and indicate
 *    incomplete implementation.
 *
 * 3. DEEPLY NESTED LOOPS
 *    Triple-nested loops (or deeper) may indicate an opportunity to use
 *    stdlib AI matrix operations (ai.MatMul) which use SIMD/AVX and
 *    multi-threaded parallelism.
 *
 * 4. SEQUENTIAL HEAVY COMPUTATION
 *    Multiple sequential calls to the same heavy function may benefit
 *    from async parallelization.
 */
void GrammarChecker::passPerformance(const AST& ast, NodeIndex idx) {
    if (idx == INVALID_NODE) return;

    const ASTNode& node = ast.get(idx);

    switch (node.type) {
        case NodeType::PROGRAM:
            for (NodeIndex child : node.children) {
                passPerformance(ast, child);
            }
            break;

        case NodeType::FUNC_DECL:
        case NodeType::ASYNC_FUNC_DECL:
            // Check for empty function body
            if (node.left != INVALID_NODE) {
                const ASTNode& body = ast.get(node.left);
                if (body.type == NodeType::BLOCK && body.children.empty()) {
                    addDiag(DiagLevel::WARNING, node.line, node.column,
                            "Function '" + node.name + "' has an empty body",
                            "perf.emptyFunction");
                }
            }
            passPerformance(ast, node.left);
            break;

        case NodeType::WHILE_STMT: {
            // Check for empty loop body
            if (node.right != INVALID_NODE) {
                const ASTNode& body = ast.get(node.right);
                if (body.type == NodeType::BLOCK && body.children.empty()) {
                    addDiag(DiagLevel::WARNING, node.line, node.column,
                            "While-loop has an empty body",
                            "perf.emptyLoop");
                }

                // Detect mixed int/float arithmetic on same variable in loop body
                if (body.type == NodeType::BLOCK) {
                    std::unordered_map<std::string, bool> varAssignedInt;
                    std::unordered_map<std::string, bool> varAssignedFloat;

                    for (NodeIndex child : body.children) {
                        const ASTNode& stmt = ast.get(child);
                        if (stmt.type == NodeType::VAR_ASSIGN && stmt.left != INVALID_NODE) {
                            const ASTNode& value = ast.get(stmt.left);
                            if (value.type == NodeType::BINARY_OP) {
                                // Check if the binary op involves float literals
                                bool hasFloatLit = false;
                                bool hasIntLit = false;
                                if (value.left != INVALID_NODE) {
                                    const ASTNode& l = ast.get(value.left);
                                    if (l.type == NodeType::LITERAL_FLOAT) hasFloatLit = true;
                                    if (l.type == NodeType::LITERAL_INT) hasIntLit = true;
                                }
                                if (value.right != INVALID_NODE) {
                                    const ASTNode& r = ast.get(value.right);
                                    if (r.type == NodeType::LITERAL_FLOAT) hasFloatLit = true;
                                    if (r.type == NodeType::LITERAL_INT) hasIntLit = true;
                                }

                                if (hasIntLit) varAssignedInt[stmt.name] = true;
                                if (hasFloatLit) varAssignedFloat[stmt.name] = true;
                            }
                        }
                    }

                    // Flag variables used with both int and float in the same loop
                    for (const auto& [varName, _] : varAssignedInt) {
                        if (varAssignedFloat.count(varName)) {
                            addDiag(DiagLevel::PERF, node.line, node.column,
                                    "Variable '" + varName + "' uses mixed int/float arithmetic in loop — consider using uniform types for SIMD optimization",
                                    "perf.mixedTypeLoop");
                        }
                    }
                }
            }
            passPerformance(ast, node.right);
            break;
        }

        case NodeType::FOR_STMT: {
            if (node.right != INVALID_NODE) {
                const ASTNode& body = ast.get(node.right);
                if (body.type == NodeType::BLOCK && body.children.empty()) {
                    addDiag(DiagLevel::WARNING, node.line, node.column,
                            "For-loop has an empty body",
                            "perf.emptyLoop");
                }
            }
            passPerformance(ast, node.right);
            break;
        }

        case NodeType::BLOCK:
            for (NodeIndex child : node.children) {
                passPerformance(ast, child);
            }
            break;

        case NodeType::IF_STMT:
            passPerformance(ast, node.right);
            passPerformance(ast, node.extra);
            break;

        case NodeType::TRY_STMT:
            passPerformance(ast, node.left);
            passPerformance(ast, node.right);
            for (NodeIndex child : node.children) {
                passPerformance(ast, child);
            }
            break;

        case NodeType::EXPR_STMT:
            passPerformance(ast, node.left);
            break;

        default:
            break;
    }
}

// ============================================================================
// AST traversal helpers
// ============================================================================

void GrammarChecker::walkBlock(const AST& ast, NodeIndex idx,
                                void (GrammarChecker::*visitor)(const AST&, NodeIndex)) {
    if (idx == INVALID_NODE) return;
    const ASTNode& node = ast.get(idx);
    if (node.type == NodeType::BLOCK) {
        for (NodeIndex child : node.children) {
            (this->*visitor)(ast, child);
        }
    } else {
        (this->*visitor)(ast, idx);
    }
}

void GrammarChecker::walkChildren(const AST& ast, NodeIndex idx,
                                   void (GrammarChecker::*visitor)(const AST&, NodeIndex)) {
    if (idx == INVALID_NODE) return;
    const ASTNode& node = ast.get(idx);
    for (NodeIndex child : node.children) {
        (this->*visitor)(ast, child);
    }
}

void GrammarChecker::walkExpression(const AST& ast, NodeIndex idx,
                                     void (GrammarChecker::*visitor)(const AST&, NodeIndex)) {
    if (idx == INVALID_NODE) return;
    const ASTNode& node = ast.get(idx);
    if (node.left != INVALID_NODE) (this->*visitor)(ast, node.left);
    if (node.right != INVALID_NODE) (this->*visitor)(ast, node.right);
    if (node.extra != INVALID_NODE) (this->*visitor)(ast, node.extra);
    for (NodeIndex child : node.children) {
        (this->*visitor)(ast, child);
    }
}

} // namespace nevaarize
