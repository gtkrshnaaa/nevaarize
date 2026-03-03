/**
 * Main.cpp - Nevaarize Entry Point
 *
 * Command-line interface for the Nevaarize JIT Compiler.
 * All execution uses native Linux x86-64 JIT compilation.
 */

#include "lexer.hpp"
#include "parser.hpp"
#include "jit.hpp"
#include "model.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

namespace nevaarize {

/**
 * Read entire file content.
 */
std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

/**
 * Run a script file with native JIT compilation.
 */
int runScript(const std::string& scriptPath) {
    std::string source;
    try {
        source = readFile(scriptPath);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    if (!lexer.errors().empty()) {
        for (const auto& err : lexer.errors()) {
            std::cerr << err << std::endl;
        }
        return 1;
    }

    Parser parser(tokens, source);
    parser.parse();

    if (parser.hasErrors()) {
        for (const auto& err : parser.errors()) {
            std::cerr << err << std::endl;
        }
        return 1;
    }

    try {
        auto ast = std::make_shared<AST>(std::move(parser.getAST()));
        
        JIT jit;
        fs::path scriptDir = fs::path(scriptPath).parent_path();
        if (scriptDir.empty()) {
            scriptDir = fs::current_path();
        } else if (!scriptDir.is_absolute()) {
            scriptDir = fs::current_path() / scriptDir;
        }
        jit.setSourceDir(scriptDir.string());
        auto compiledFn = jit.compile(*ast);
        jit.execute(compiledFn);
        
    } catch (const std::exception& e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

/**
 * Print usage information.
 */
void printUsage(const char* program) {
    std::cout << "Nevaarize - Native JIT Compiler" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: " << program << " <script.nva>" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help     Show this help message" << std::endl;
    std::cout << "  -v, --version  Show version information" << std::endl;
}

/**
 * Print version information.
 */
void printVersion() {
    std::cout << "Nevaarize v0.1.5" << std::endl;
    std::cout << "Native JIT Compiler for the Nevaarize Programming Language" << std::endl;
    std::cout << "Built with C++23, Zero External Dependencies" << std::endl;
}

} // namespace nevaarize

int main(int argc, char* argv[]) {
    using namespace nevaarize;

    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string arg1 = argv[1];

    // Handle options
    if (arg1 == "-h" || arg1 == "--help") {
        printUsage(argv[0]);
        return 0;
    }
    
    if (arg1 == "-v" || arg1 == "--version") {
        printVersion();
        return 0;
    }

    // Run script
    return runScript(arg1);
}
