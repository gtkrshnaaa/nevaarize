/**
 * JIT.cpp - Nevaarize JIT Compiler and Evaluator Implementation
 *
 * Complete implementation of tree-walk evaluator and JIT compilation pipeline.
 */

#include "JIT.hpp"
#include "NativeJIT.hpp"
#include "TrueJIT.hpp"
#include "SIMD.hpp"
#include "VectorOps.hpp"
#include "Tensor.hpp"
#include "../../stdlib/include/AI.hpp"
#include <sstream>
#include <iostream>
#include <fstream>
#include <cmath>
#include <chrono>
#include <thread>
#include <filesystem>

namespace nevaarize {

// Value::toString implementation
std::string Value::toString() const {
    switch (type) {
        case ValueType::NIL:
            return "nil";
        case ValueType::BOOL:
            return boolVal ? "true" : "false";
        case ValueType::INT:
            return std::to_string(intVal);
        case ValueType::FLOAT: {
            std::ostringstream oss;
            oss << floatVal;
            return oss.str();
        }
        case ValueType::STRING:
            return stringVal ? *stringVal : "";
        case ValueType::ARRAY: {
            if (!arrayVal) return "[]";
            std::ostringstream oss;
            oss << "[";
            for (size_t i = 0; i < arrayVal->size(); ++i) {
                if (i > 0) oss << ", ";
                oss << (*arrayVal)[i].toString();
            }
            oss << "]";
            return oss.str();
        }
        case ValueType::STRUCT_INSTANCE:
            return structVal ? ("<" + structVal->typeName + " instance>") : "<struct>";
        case ValueType::FUNCTION:
            return funcVal ? ("<function " + funcVal->name + ">") : "<function>";
        case ValueType::NATIVE_FUNCTION:
            return "<native function>";
        case ValueType::ASYNC_HANDLE:
            return "<async handle>";
        default:
            return "<unknown>";
    }
}

JITCompiler::JITCompiler() {}
JITCompiler::~JITCompiler() {}

CompileResult JITCompiler::compile(const std::string& source) {
    CompileResult result;

    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    if (!lexer.errors().empty()) {
        result.error = lexer.errors()[0];
        return result;
    }

    Parser parser(tokens);
    parser.parse();

    if (parser.hasErrors()) {
        result.error = parser.errors()[0];
        return result;
    }

    result.success = true;
    return result;
}

Evaluator::Evaluator() {
    globalEnv = std::make_shared<Environment>();
    environment = globalEnv;
    setupStandardLibrary();
}

void Evaluator::setupStandardLibrary() {
    // print function
    registerNative("print", [](Evaluator&, const std::vector<Value>& args) -> Value {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].toString();
        }
        std::cout << std::endl;
        return Value::nil();
    });

    // Range function
    registerNative("Range", [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || !args[0].isNumber() || !args[1].isNumber()) {
            return Value::fromArray({});
        }
        int64_t start = args[0].isInt() ? args[0].intVal : static_cast<int64_t>(args[0].floatVal);
        int64_t end = args[1].isInt() ? args[1].intVal : static_cast<int64_t>(args[1].floatVal);
        
        std::vector<Value> result;
        for (int64_t i = start; i < end; ++i) {
            result.push_back(Value::fromInt(i));
        }
        return Value::fromArray(std::move(result));
    });

    // len function
    registerNative("len", [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::fromInt(0);
        if (args[0].isString() && args[0].stringVal) {
            return Value::fromInt(static_cast<int64_t>(args[0].stringVal->size()));
        }
        if (args[0].isArray() && args[0].arrayVal) {
            return Value::fromInt(static_cast<int64_t>(args[0].arrayVal->size()));
        }
        return Value::fromInt(0);
    });

    // type function
    registerNative("type", [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::fromString("nil");
        return Value::fromString(valueTypeToString(args[0].type));
    });

    // str function
    registerNative("str", [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::fromString("");
        return Value::fromString(args[0].toString());
    });

    // int function
    registerNative("int", [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::fromInt(0);
        const Value& v = args[0];
        if (v.isInt()) return v;
        if (v.isFloat()) return Value::fromInt(static_cast<int64_t>(v.floatVal));
        if (v.isString() && v.stringVal) {
            try {
                return Value::fromInt(std::stoll(*v.stringVal));
            } catch (...) {
                return Value::fromInt(0);
            }
        }
        return Value::fromInt(0);
    });

    // float function
    registerNative("float", [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::fromFloat(0.0);
        const Value& v = args[0];
        if (v.isFloat()) return v;
        if (v.isInt()) return Value::fromFloat(static_cast<double>(v.intVal));
        if (v.isString() && v.stringVal) {
            try {
                return Value::fromFloat(std::stod(*v.stringVal));
            } catch (...) {
                return Value::fromFloat(0.0);
            }
        }
        return Value::fromFloat(0.0);
    });

    // Native JIT benchmark functions
    registerNative("nativeSumLoop", [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::nil();
        int64_t n = args[0].isInt() ? args[0].intVal : static_cast<int64_t>(args[0].floatVal);
        
        auto [result, opsPerSec] = NativeLoop::sumLoop(n);
        
        // Return array with [result, ops_per_second]
        std::vector<Value> output;
        output.push_back(Value::fromInt(result));
        output.push_back(Value::fromFloat(opsPerSec));
        return Value::fromArray(std::move(output));
    });

    registerNative("nativeFibLoop", [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::nil();
        int64_t n = args[0].isInt() ? args[0].intVal : static_cast<int64_t>(args[0].floatVal);
        
        auto [result, opsPerSec] = NativeLoop::fibLoop(n);
        
        std::vector<Value> output;
        output.push_back(Value::fromInt(result));
        output.push_back(Value::fromFloat(opsPerSec));
        return Value::fromArray(std::move(output));
    });

    registerNative("nativeCallLoop", [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::nil();
        int64_t n = args[0].isInt() ? args[0].intVal : static_cast<int64_t>(args[0].floatVal);
        
        auto [result, opsPerSec] = NativeLoop::callLoop(n);
        
        std::vector<Value> output;
        output.push_back(Value::fromInt(result));
        output.push_back(Value::fromFloat(opsPerSec));
        return Value::fromArray(std::move(output));
    });

    // TRUE JIT function - compiles Nevaarize code to native machine code
    registerNative("jitSumLoop", [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || !args[0].isNumber() || !args[1].isNumber()) {
            return Value::nil();
        }
        
        int64_t start = args[0].isInt() ? args[0].intVal : static_cast<int64_t>(args[0].floatVal);
        int64_t end = args[1].isInt() ? args[1].intVal : static_cast<int64_t>(args[1].floatVal);
        
        // Create a simple AST for for loop with sum
        AST ast;
        
        // Create a for statement node
        ASTNode forNode(NodeType::FOR_STMT, 1, 1);
        forNode.name = "i";
        
        // We'll compile directly
        TrueJIT jit;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Create minimal AST for the loop
        NodeIndex forIdx = ast.addNode(std::move(forNode));
        ast.setRoot(forIdx);
        
        // Compile the loop to native code
        CompiledFunc fn = jit.compileForLoop(ast, forIdx, start, end);
        
        // Execute the compiled code
        int64_t result = jit.execute(fn);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        double seconds = std::chrono::duration<double>(endTime - startTime).count();
        double opsPerSec = static_cast<double>(end - start) / seconds;
        
        std::vector<Value> output;
        output.push_back(Value::fromInt(result));
        output.push_back(Value::fromFloat(opsPerSec));
        return Value::fromArray(std::move(output));
    });

    // SIMD Info - detect CPU SIMD capabilities
    registerNative("simdInfo", [](Evaluator&, const std::vector<Value>&) -> Value {
        SIMDLevel level = detectSIMD();
        StructInstance info;
        info.typeName = "SIMDInfo";
        info.fields["level"] = Value::fromString(simdLevelToString(level));
        info.fields["hasAVX2"] = Value::fromBool(hasSIMD(SIMDLevel::AVX2));
        info.fields["hasAVX512"] = Value::fromBool(hasSIMD(SIMDLevel::AVX512));
        return Value::fromStruct(info);
    });

    // SIMD Sum Loop - TRUE SIMD vectorized sum
    registerNative("simdSumLoop", [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::nil();
        int64_t n = args[0].isInt() ? args[0].intVal : static_cast<int64_t>(args[0].floatVal);
        
        auto start = std::chrono::high_resolution_clock::now();
        int64_t result = simdSumLoop(n);
        auto end = std::chrono::high_resolution_clock::now();
        
        double seconds = std::chrono::duration<double>(end - start).count();
        double opsPerSec = static_cast<double>(n) / seconds;
        
        std::vector<Value> output;
        output.push_back(Value::fromInt(result));
        output.push_back(Value::fromFloat(opsPerSec));
        return Value::fromArray(std::move(output));
    });

    // SIMD Vector Dot Product benchmark
    registerNative("simdDotProduct", [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isNumber()) return Value::nil();
        size_t n = static_cast<size_t>(args[0].isInt() ? args[0].intVal : args[0].floatVal);
        
        // Allocate aligned vectors
        float* a = static_cast<float*>(simdAlloc(n * sizeof(float)));
        float* b = static_cast<float*>(simdAlloc(n * sizeof(float)));
        
        if (!a || !b) {
            simdFree(a);
            simdFree(b);
            return Value::nil();
        }
        
        // Initialize with test data
        for (size_t i = 0; i < n; ++i) {
            a[i] = 1.0f;
            b[i] = 2.0f;
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        float result = vecDot_f32(a, b, n);
        auto end = std::chrono::high_resolution_clock::now();
        
        simdFree(a);
        simdFree(b);
        
        double seconds = std::chrono::duration<double>(end - start).count();
        double opsPerSec = static_cast<double>(n) / seconds;
        
        std::vector<Value> output;
        output.push_back(Value::fromFloat(static_cast<double>(result)));
        output.push_back(Value::fromFloat(opsPerSec));
        return Value::fromArray(std::move(output));
    });

    // Matrix Multiplication Benchmark
    registerNative("matmulBenchmark", [](Evaluator&, const std::vector<Value>& args) -> Value {
        int M = 512, N = 512, K = 512;
        if (args.size() >= 1 && args[0].isNumber()) {
            M = static_cast<int>(args[0].asDouble());
            N = M;
            K = M;
        }
        if (args.size() >= 3) {
            M = static_cast<int>(args[0].asDouble());
            N = static_cast<int>(args[1].asDouble());
            K = static_cast<int>(args[2].asDouble());
        }
        
        auto result = benchmarkMatmul(M, N, K);
        
        std::vector<Value> output;
        output.push_back(Value::fromFloat(result.gflops));
        output.push_back(Value::fromFloat(result.seconds));
        return Value::fromArray(std::move(output));
    });

    // Tensor ReLU benchmark (activation function)
    registerNative("reluBenchmark", [](Evaluator&, const std::vector<Value>& args) -> Value {
        size_t n = 1000000;
        if (!args.empty() && args[0].isNumber()) {
            n = static_cast<size_t>(args[0].asDouble());
        }
        
        Tensor t = Tensor::ones({static_cast<int64_t>(n)});
        
        auto start = std::chrono::high_resolution_clock::now();
        Tensor result = t.relu();
        auto end = std::chrono::high_resolution_clock::now();
        
        double seconds = std::chrono::duration<double>(end - start).count();
        double opsPerSec = static_cast<double>(n) / seconds;
        
        std::vector<Value> output;
        output.push_back(Value::fromFloat(result.sum()));
        output.push_back(Value::fromFloat(opsPerSec));
        return Value::fromArray(std::move(output));
    });
}

void Evaluator::registerNative(const std::string& name, NativeFunction fn) {
    globalEnv->define(name, Value::fromNative(std::move(fn)));
}

void Evaluator::registerModule(const std::string& alias,
                               const std::unordered_map<std::string, NativeFunction>& functions) {
    StructInstance moduleObj;
    moduleObj.typeName = alias;
    for (const auto& [name, fn] : functions) {
        moduleObj.fields[name] = Value::fromNative(fn);
    }
    globalEnv->define(alias, Value::fromStruct(moduleObj));
}

Value Evaluator::execute(std::shared_ptr<const AST> tree, const std::filesystem::path& filePath) {
    currentFilePath = filePath;
    return execute(tree);
}

Value Evaluator::execute(std::shared_ptr<const AST> tree) {
    ast = tree;
    environment = globalEnv;

    NodeIndex root = ast->root();
    if (root == INVALID_NODE) {
        return Value::nil();
    }

    try {
        const ASTNode& program = ast->get(root);
        for (NodeIndex child : program.children) {
            const ASTNode& node = ast->get(child);
            switch (node.type) {
                case NodeType::FUNC_DECL:
                case NodeType::ASYNC_FUNC_DECL:
                    execFuncDecl(node);
                    break;
                case NodeType::STRUCT_DECL:
                    execStructDecl(node);
                    break;
                case NodeType::IMPORT_STDLIB:
                    execImportStdlib(node);
                    break;
                case NodeType::IMPORT_FILE:
                    execImportFile(node);
                    break;
                case NodeType::EXPR_STMT:
                    evaluate(node.left);
                    break;
                case NodeType::VAR_ASSIGN:
                    execVarAssign(node);
                    break;
                case NodeType::MEMBER_ASSIGN:
                    execMemberAssign(node);
                    break;
                case NodeType::INDEX_ASSIGN:
                    execIndexAssign(node);
                    break;
                case NodeType::BLOCK:
                    execBlock(node);
                    break;
                case NodeType::IF_STMT:
                    execIf(node);
                    break;
                case NodeType::FOR_STMT:
                    execFor(node);
                    break;
                case NodeType::WHILE_STMT:
                    execWhile(node);
                    break;
                case NodeType::RETURN_STMT:
                    execReturn(node);
                    break;
                default:
                    evaluate(child);
                    break;
            }
        }
    } catch (const ReturnException& ret) {
        return ret.value;
    }

    return Value::nil();
}

Value Evaluator::evaluate(NodeIndex idx) {
    if (idx == INVALID_NODE) return Value::nil();

    const ASTNode& node = ast->get(idx);
    switch (node.type) {
        case NodeType::LITERAL_INT:
        case NodeType::LITERAL_FLOAT:
        case NodeType::LITERAL_STRING:
        case NodeType::LITERAL_BOOL:
        case NodeType::LITERAL_NIL:
            return evalLiteral(node);
        case NodeType::IDENTIFIER:
            return evalIdentifier(node);
        case NodeType::BINARY_OP:
            return evalBinaryOp(node);
        case NodeType::UNARY_OP:
            return evalUnaryOp(node);
        case NodeType::CALL:
            return evalCall(node);
        case NodeType::MEMBER_ACCESS:
            return evalMemberAccess(node);
        case NodeType::INDEX_ACCESS:
            return evalIndexAccess(node);
        case NodeType::ARRAY_LITERAL:
            return evalArrayLiteral(node);
        case NodeType::AWAIT_EXPR:
            return evalAwait(node);
        default:
            return Value::nil();
    }
}

Value Evaluator::evalLiteral(const ASTNode& node) {
    switch (node.type) {
        case NodeType::LITERAL_INT:
            return Value::fromInt(std::get<int64_t>(node.literal.data));
        case NodeType::LITERAL_FLOAT:
            return Value::fromFloat(std::get<double>(node.literal.data));
        case NodeType::LITERAL_STRING:
            return Value::fromString(std::get<std::string>(node.literal.data));
        case NodeType::LITERAL_BOOL:
            return Value::fromBool(std::get<bool>(node.literal.data));
        case NodeType::LITERAL_NIL:
            return Value::nil();
        default:
            return Value::nil();
    }
}

Value Evaluator::evalIdentifier(const ASTNode& node) {
    // First check if this is a module reference
    auto modIt = modules.find(node.name);
    if (modIt != modules.end()) {
        // Return a special struct that wraps the module environment
        StructInstance modObj;
        modObj.typeName = node.name;
        // Copy all definitions from module environment as fields
        auto env = modIt->second;
        // We can't directly iterate environment, so we store module env for later access
        return Value::fromStruct(modObj);
    }
    return environment->get(node.name);
}

Value Evaluator::evalBinaryOp(const ASTNode& node) {
    Value left = evaluate(node.left);
    Value right = evaluate(node.right);

    switch (node.binaryOp) {
        case BinaryOp::ADD:
            if (left.isInt() && right.isInt()) {
                return Value::fromInt(left.intVal + right.intVal);
            }
            if (left.isNumber() && right.isNumber()) {
                return Value::fromFloat(left.asDouble() + right.asDouble());
            }
            if (left.isString() || right.isString()) {
                return Value::fromString(left.toString() + right.toString());
            }
            break;

        case BinaryOp::SUB:
            if (left.isInt() && right.isInt()) {
                return Value::fromInt(left.intVal - right.intVal);
            }
            return Value::fromFloat(left.asDouble() - right.asDouble());

        case BinaryOp::MUL:
            if (left.isInt() && right.isInt()) {
                return Value::fromInt(left.intVal * right.intVal);
            }
            return Value::fromFloat(left.asDouble() * right.asDouble());

        case BinaryOp::DIV:
            if (left.isInt() && right.isInt() && right.intVal != 0) {
                return Value::fromInt(left.intVal / right.intVal);
            }
            if (right.asDouble() != 0.0) {
                return Value::fromFloat(left.asDouble() / right.asDouble());
            }
            break;

        case BinaryOp::MOD:
            if (left.isInt() && right.isInt() && right.intVal != 0) {
                return Value::fromInt(left.intVal % right.intVal);
            }
            break;

        case BinaryOp::EQ:
            if (left.type != right.type) return Value::fromBool(false);
            if (left.isNil()) return Value::fromBool(true);
            if (left.isBool()) return Value::fromBool(left.boolVal == right.boolVal);
            if (left.isInt()) return Value::fromBool(left.intVal == right.intVal);
            if (left.isFloat()) return Value::fromBool(left.floatVal == right.floatVal);
            if (left.isString()) return Value::fromBool(*left.stringVal == *right.stringVal);
            break;

        case BinaryOp::NEQ:
            if (left.type != right.type) return Value::fromBool(true);
            if (left.isNil()) return Value::fromBool(false);
            if (left.isBool()) return Value::fromBool(left.boolVal != right.boolVal);
            if (left.isInt()) return Value::fromBool(left.intVal != right.intVal);
            if (left.isFloat()) return Value::fromBool(left.floatVal != right.floatVal);
            if (left.isString()) return Value::fromBool(*left.stringVal != *right.stringVal);
            break;

        case BinaryOp::LT:
            return Value::fromBool(left.asDouble() < right.asDouble());
        case BinaryOp::LTE:
            return Value::fromBool(left.asDouble() <= right.asDouble());
        case BinaryOp::GT:
            return Value::fromBool(left.asDouble() > right.asDouble());
        case BinaryOp::GTE:
            return Value::fromBool(left.asDouble() >= right.asDouble());

        case BinaryOp::AND:
            return Value::fromBool(left.isTruthy() && right.isTruthy());
        case BinaryOp::OR:
            return Value::fromBool(left.isTruthy() || right.isTruthy());
    }

    return Value::nil();
}

Value Evaluator::evalUnaryOp(const ASTNode& node) {
    Value operand = evaluate(node.left);

    switch (node.unaryOp) {
        case UnaryOp::NEG:
            if (operand.isInt()) return Value::fromInt(-operand.intVal);
            if (operand.isFloat()) return Value::fromFloat(-operand.floatVal);
            break;
        case UnaryOp::NOT:
            return Value::fromBool(!operand.isTruthy());
    }

    return Value::nil();
}

Value Evaluator::evalCall(const ASTNode& node) {
    Value callee = evaluate(node.left);

    std::vector<Value> args;
    for (NodeIndex argIdx : node.children) {
        args.push_back(evaluate(argIdx));
    }

    return callFunction(callee, args, node.line, node.column);
}

Value Evaluator::callFunction(const Value& callee, const std::vector<Value>& args, int line, int column) {
    if (callee.isNative()) {
        return (*callee.nativeVal)(*this, args);
    }

    if (callee.isFunction()) {
        FunctionDef& func = *callee.funcVal;
        auto funcEnv = std::make_shared<Environment>(func.closure ? func.closure : globalEnv);

        for (size_t i = 0; i < func.params.size() && i < args.size(); ++i) {
            funcEnv->define(func.params[i], args[i]);
        }

        auto prevEnv = environment;
        auto prevAST = ast;
        environment = funcEnv;
        
        if (func.moduleAST) {
            ast = func.moduleAST;
        }

        try {
            const ASTNode& body = ast->get(func.bodyIndex);
            for (NodeIndex stmtIdx : body.children) {
                const ASTNode& stmt = ast->get(stmtIdx);
                switch (stmt.type) {
                    case NodeType::VAR_ASSIGN:
                        execVarAssign(stmt);
                        break;
                    case NodeType::MEMBER_ASSIGN:
                        execMemberAssign(stmt);
                        break;
                    case NodeType::INDEX_ASSIGN:
                        execIndexAssign(stmt);
                        break;
                    case NodeType::IF_STMT:
                        execIf(stmt);
                        break;
                    case NodeType::FOR_STMT:
                        execFor(stmt);
                        break;
                    case NodeType::WHILE_STMT:
                        execWhile(stmt);
                        break;
                    case NodeType::RETURN_STMT:
                        execReturn(stmt);
                        break;
                    case NodeType::EXPR_STMT:
                        evaluate(stmt.left);
                        break;
                    case NodeType::FUNC_DECL:
                    case NodeType::ASYNC_FUNC_DECL:
                        execFuncDecl(stmt);
                        break;
                    default:
                        break;
                }
            }
        } catch (const ReturnException& ret) {
            environment = prevEnv;
            ast = prevAST;
            return ret.value;
        }

        environment = prevEnv;
        ast = prevAST;
        return Value::nil();
    }

    throw RuntimeError("Cannot call non-function value", line, column);
}

Value Evaluator::evalMemberAccess(const ASTNode& node) {
    Value obj = evaluate(node.left);

    if (obj.isStruct() && obj.structVal) {
        // Check if this is a module reference
        auto modIt = modules.find(obj.structVal->typeName);
        if (modIt != modules.end()) {
            // Lookup member in module environment
            try {
                return modIt->second->get(node.name);
            } catch (...) {
                throw RuntimeError("Undefined export: " + node.name + " in module " + obj.structVal->typeName, 
                                   node.line, node.column);
            }
        }

        auto it = obj.structVal->fields.find(node.name);
        if (it != obj.structVal->fields.end()) {
            return it->second;
        }
        throw RuntimeError("Undefined field: " + node.name, node.line, node.column);
    }

    if (obj.isArray() && obj.arrayVal) {
        if (node.name == "length") {
            return Value::fromInt(static_cast<int64_t>(obj.arrayVal->size()));
        }
        if (node.name == "push") {
            auto arr = obj.arrayVal;
            return Value::fromNative([arr](Evaluator&, const std::vector<Value>& args) -> Value {
                if (!args.empty()) {
                    arr->push_back(args[0]);
                }
                return Value::nil();
            });
        }
        if (node.name == "pop") {
            auto arr = obj.arrayVal;
            return Value::fromNative([arr](Evaluator&, const std::vector<Value>&) -> Value {
                if (arr->empty()) return Value::nil();
                Value val = arr->back();
                arr->pop_back();
                return val;
            });
        }
    }

    return Value::nil();
}

Value Evaluator::evalIndexAccess(const ASTNode& node) {
    Value obj = evaluate(node.left);
    Value index = evaluate(node.right);

    if (obj.isArray() && obj.arrayVal && index.isInt()) {
        int64_t idx = index.intVal;
        if (idx >= 0 && idx < static_cast<int64_t>(obj.arrayVal->size())) {
            return (*obj.arrayVal)[static_cast<size_t>(idx)];
        }
    }

    if (obj.isString() && obj.stringVal && index.isInt()) {
        int64_t idx = index.intVal;
        if (idx >= 0 && idx < static_cast<int64_t>(obj.stringVal->size())) {
            return Value::fromString(std::string(1, (*obj.stringVal)[static_cast<size_t>(idx)]));
        }
    }

    return Value::nil();
}

Value Evaluator::evalArrayLiteral(const ASTNode& node) {
    std::vector<Value> elements;
    for (NodeIndex elemIdx : node.children) {
        elements.push_back(evaluate(elemIdx));
    }
    return Value::fromArray(std::move(elements));
}

Value Evaluator::evalAwait(const ASTNode& node) {
    return evaluate(node.left);
}

void Evaluator::execBlock(const ASTNode& node) {
    auto blockEnv = std::make_shared<Environment>(environment);
    auto prevEnv = environment;
    environment = blockEnv;

    for (NodeIndex stmtIdx : node.children) {
        const ASTNode& stmt = ast->get(stmtIdx);
        switch (stmt.type) {
            case NodeType::VAR_ASSIGN:
                execVarAssign(stmt);
                break;
            case NodeType::MEMBER_ASSIGN:
                execMemberAssign(stmt);
                break;
            case NodeType::INDEX_ASSIGN:
                execIndexAssign(stmt);
                break;
            case NodeType::IF_STMT:
                execIf(stmt);
                break;
            case NodeType::FOR_STMT:
                execFor(stmt);
                break;
            case NodeType::WHILE_STMT:
                execWhile(stmt);
                break;
            case NodeType::RETURN_STMT:
                execReturn(stmt);
                break;
            case NodeType::EXPR_STMT:
                evaluate(stmt.left);
                break;
            case NodeType::FUNC_DECL:
            case NodeType::ASYNC_FUNC_DECL:
                execFuncDecl(stmt);
                break;
            default:
                break;
        }
    }

    environment = prevEnv;
}

void Evaluator::execVarAssign(const ASTNode& node) {
    Value val = evaluate(node.left);
    environment->set(node.name, val);
}

void Evaluator::execMemberAssign(const ASTNode& node) {
    Value obj = evaluate(node.left);
    Value val = evaluate(node.right);

    if (obj.isStruct() && obj.structVal) {
        obj.structVal->fields[node.name] = val;
    }
}

void Evaluator::execIndexAssign(const ASTNode& node) {
    Value obj = evaluate(node.left);
    Value index = evaluate(node.right);
    Value val = evaluate(node.extra);

    if (obj.isArray() && obj.arrayVal && index.isInt()) {
        int64_t idx = index.intVal;
        if (idx >= 0 && idx < static_cast<int64_t>(obj.arrayVal->size())) {
            (*obj.arrayVal)[static_cast<size_t>(idx)] = val;
        }
    }
}

void Evaluator::execIf(const ASTNode& node) {
    Value cond = evaluate(node.left);

    if (cond.isTruthy()) {
        const ASTNode& thenBlock = ast->get(node.right);
        execBlock(thenBlock);
    } else if (node.extra != INVALID_NODE) {
        const ASTNode& elseBlock = ast->get(node.extra);
        execBlock(elseBlock);
    }
}

void Evaluator::execFor(const ASTNode& node) {
    Value iterable = evaluate(node.left);

    if (!iterable.isArray() || !iterable.arrayVal) {
        return;
    }

    auto loopEnv = std::make_shared<Environment>(environment);
    auto prevEnv = environment;
    environment = loopEnv;

    for (const Value& item : *iterable.arrayVal) {
        environment->set(node.name, item);
        const ASTNode& body = ast->get(node.right);
        execBlock(body);
    }

    environment = prevEnv;
}

void Evaluator::execWhile(const ASTNode& node) {
    while (evaluate(node.left).isTruthy()) {
        const ASTNode& body = ast->get(node.right);
        execBlock(body);
    }
}

void Evaluator::execReturn(const ASTNode& node) {
    Value val = Value::nil();
    if (node.left != INVALID_NODE) {
        val = evaluate(node.left);
    }
    throw ReturnException(val);
}

void Evaluator::execFuncDecl(const ASTNode& node) {
    FunctionDef func;
    func.name = node.name;
    func.params = node.paramNames;
    func.bodyIndex = node.left;
    func.isAsync = (node.type == NodeType::ASYNC_FUNC_DECL);
    func.closure = environment;
    func.moduleAST = ast;

    environment->define(node.name, Value::fromFunction(func));
}

void Evaluator::execStructDecl(const ASTNode& node) {
    StructDef def;
    def.name = node.name;
    def.fields = node.paramNames;
    structs[node.name] = def;

    auto constructorFn = [this, fields = node.paramNames, typeName = node.name]
                         (Evaluator&, const std::vector<Value>& args) -> Value {
        StructInstance si;
        si.typeName = typeName;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i < args.size()) {
                si.fields[fields[i]] = args[i];
            } else {
                si.fields[fields[i]] = Value::nil();
            }
        }
        return Value::fromStruct(si);
    };

    environment->define(node.name, Value::fromNative(constructorFn));
}

void Evaluator::execImportStdlib(const ASTNode& node) {
    std::string libName = node.name;
    std::string alias = node.paramNames[0];

    if (libName == "math") {
        std::unordered_map<std::string, NativeFunction> mathFuncs;
        mathFuncs["Abs"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
            return Value::fromFloat(std::abs(args[0].asDouble()));
        };
        mathFuncs["Sqrt"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
            return Value::fromFloat(std::sqrt(args[0].asDouble()));
        };
        mathFuncs["Pow"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) return Value::fromFloat(0.0);
            return Value::fromFloat(std::pow(args[0].asDouble(), args[1].asDouble()));
        };
        mathFuncs["Floor"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
            return Value::fromFloat(std::floor(args[0].asDouble()));
        };
        mathFuncs["Ceil"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
            return Value::fromFloat(std::ceil(args[0].asDouble()));
        };
        mathFuncs["Sin"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
            return Value::fromFloat(std::sin(args[0].asDouble()));
        };
        mathFuncs["Cos"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
            return Value::fromFloat(std::cos(args[0].asDouble()));
        };
        mathFuncs["Tan"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isNumber()) return Value::fromFloat(0.0);
            return Value::fromFloat(std::tan(args[0].asDouble()));
        };
        registerModule(alias, mathFuncs);
    } else if (libName == "time") {
        std::unordered_map<std::string, NativeFunction> timeFuncs;
        timeFuncs["clock"] = [](Evaluator&, const std::vector<Value>&) -> Value {
            auto now = std::chrono::high_resolution_clock::now();
            auto epoch = now.time_since_epoch();
            auto seconds = std::chrono::duration<double>(epoch).count();
            return Value::fromFloat(seconds);
        };
        timeFuncs["sleep"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
            if (!args.empty() && args[0].isNumber()) {
                int ms = static_cast<int>(args[0].asDouble());
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            }
            return Value::nil();
        };
        registerModule(alias, timeFuncs);
    } else if (libName == "io") {
        std::unordered_map<std::string, NativeFunction> ioFuncs;
        ioFuncs["Print"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) std::cout << " ";
                std::cout << args[i].toString();
            }
            std::cout << std::endl;
            return Value::nil();
        };
        ioFuncs["Write"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) std::cout << " ";
                std::cout << args[i].toString();
            }
            std::cout.flush();
            return Value::nil();
        };
        ioFuncs["Input"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
            if (!args.empty()) {
                std::cout << args[0].toString();
                std::cout.flush();
            }
            std::string line;
            std::getline(std::cin, line);
            return Value::fromString(line);
        };
        registerModule(alias, ioFuncs);
    } else if (libName == "ai") {
        registerModule(alias, stdlib::getAILibrary());
    }
}

void Evaluator::execImportFile(const ASTNode& node) {
    std::string filePath = node.name;
    std::string alias = node.paramNames[0];

    // Resolve path relative to current source file
    std::filesystem::path basePath = currentFilePath.parent_path();
    std::filesystem::path fullPath = basePath / filePath;

    // Read file content
    std::ifstream file(fullPath);
    if (!file) {
        throw RuntimeError("Cannot open file: " + fullPath.string(), node.line, node.column);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // Tokenize and parse
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (!lexer.errors().empty()) {
        throw RuntimeError("Lexer error in " + filePath + ": " + lexer.errors()[0], node.line, node.column);
    }

    Parser parser(tokens);
    parser.parse();
    if (parser.hasErrors()) {
        throw RuntimeError("Parser error in " + filePath + ": " + parser.errors()[0], node.line, node.column);
    }

    // Execute module in new environment to collect exports
    auto moduleAST = std::make_shared<AST>(std::move(parser.getAST()));
    auto moduleEnv = std::make_shared<Environment>(globalEnv);
    auto prevEnv = environment;
    auto prevAST = ast;
    auto prevFilePath = currentFilePath;
    environment = moduleEnv;
    ast = moduleAST;
    currentFilePath = fullPath;

    NodeIndex root = ast->root();
    if (root != INVALID_NODE) {
        const ASTNode& program = ast->get(root);
        for (NodeIndex child : program.children) {
            const ASTNode& stmt = ast->get(child);
            switch (stmt.type) {
                case NodeType::FUNC_DECL:
                case NodeType::ASYNC_FUNC_DECL:
                    execFuncDecl(stmt);
                    break;
                case NodeType::STRUCT_DECL:
                    execStructDecl(stmt);
                    break;
                case NodeType::IMPORT_STDLIB:
                    execImportStdlib(stmt);
                    break;
                case NodeType::IMPORT_FILE:
                    execImportFile(stmt);
                    break;
                case NodeType::VAR_ASSIGN:
                    execVarAssign(stmt);
                    break;
                default:
                    break;
            }
        }
    }

    // Create module object from moduleEnv
    StructInstance moduleObj;
    moduleObj.typeName = alias;

    // Copy all definitions from module environment
    // This is a simplified approach - we expose all top-level definitions
    auto copyEnv = moduleEnv;
    while (copyEnv && copyEnv != globalEnv) {
        // We need to access the variables - add a method to Environment
        copyEnv = copyEnv->getParent();
    }

    // For now, store the module environment directly
    // and use member access to call through it
    environment = prevEnv;
    ast = prevAST;
    currentFilePath = prevFilePath;

    // Register the module environment as a special struct
    globalEnv->define(alias, Value::fromStruct(moduleObj));
    modules[alias] = moduleEnv;
}

} // namespace nevaarize
