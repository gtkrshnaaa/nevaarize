/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * Repl.hpp - Nevaarize Interactive REPL Engine
 *
 * Provides a Read-Eval-Print Loop for interactive line-by-line execution,
 * multi-line continuation, persistent environment, and colored diagnostics.
 */

#ifndef NEVAARIZE_REPL_HPP
#define NEVAARIZE_REPL_HPP

#include "jit.hpp"
#include <string>
#include <memory>

namespace nevaarize {

class Repl {
public:
    Repl();
    ~Repl() = default;

    /**
     * Start the interactive REPL loop.
     * Returns process exit code.
     */
    int run();

    /**
     * Evaluate a single multi-line buffer in REPL state.
     */
    void evalLine(const std::string& input);

    /**
     * Check if an input buffer is incomplete and requires continuation.
     */
    static bool isInputIncomplete(const std::string& buffer);

private:
    JIT jit;
    bool running = true;

    void printBanner();
    void printHelp();
};

} // namespace nevaarize

#endif // NEVAARIZE_REPL_HPP
