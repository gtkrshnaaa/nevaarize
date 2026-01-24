/**
 * NativeJIT.hpp - True Native JIT Compiler
 *
 * Compiles Nevaarize code directly to x86-64 machine code.
 * Executes native code for maximum performance.
 */

#ifndef NEVAARIZE_NATIVE_JIT_HPP
#define NEVAARIZE_NATIVE_JIT_HPP

#include "AST.hpp"
#include "CodeGen.hpp"
#include <cstdint>
#include <memory>
#include <functional>

namespace nevaarize {

/**
 * Native function signature for JIT-compiled code.
 */
using JITFunc = int64_t (*)();
using JITFuncWithArgs = int64_t (*)(int64_t);

/**
 * Native JIT Compiler.
 * Compiles expressions and loops directly to x86-64 machine code.
 */
class NativeJIT {
public:
    NativeJIT();
    ~NativeJIT();

    /**
     * Compile and execute an integer loop benchmark.
     * Returns: [result, iterations_per_second]
     */
    std::pair<int64_t, double> runIntegerLoopBenchmark(int64_t iterations);

    /**
     * Compile and execute a function call benchmark.
     */
    std::pair<int64_t, double> runFunctionCallBenchmark(int64_t iterations);

    /**
     * Compile a simple arithmetic expression to native code.
     */
    JITFunc compileExpression(int64_t a, int64_t b, char op);

    /**
     * Check if JIT is available on this platform.
     */
    static bool isAvailable();

private:
    std::unique_ptr<ExecutableMemory> execMem;
    CodeGenerator codegen;

    // Native loop implementation
    void emitIntegerLoopCode(int64_t iterations);
    void emitFunctionCallCode(int64_t iterations);
};

/**
 * Native integer loop - pure x86-64 implementation.
 * This demonstrates true JIT performance.
 */
class NativeLoop {
public:
    /**
     * Run a native integer sum loop.
     * Compiles: for i in 0..n: sum += i
     */
    static std::pair<int64_t, double> sumLoop(int64_t n);

    /**
     * Run native fibonacci benchmark.
     */
    static std::pair<int64_t, double> fibLoop(int64_t n);

    /**
     * Run native function call benchmark.
     */
    static std::pair<int64_t, double> callLoop(int64_t n);
};

} // namespace nevaarize

#endif // NEVAARIZE_NATIVE_JIT_HPP
