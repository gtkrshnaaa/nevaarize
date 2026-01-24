/**
 * Main.cpp - Nevaarize Entry Point
 *
 * Command-line interface and REPL implementation.
 */

#include "Lexer.hpp"
#include "Parser.hpp"
#include "JIT.hpp"
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
 * Run a script file with path resolution.
 */
int runScript(const std::string& scriptPath, Evaluator& evaluator) {
    std::string source;
    try {
        source = readFile(scriptPath);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    // Set the base path for relative imports
    fs::path basePath = fs::absolute(scriptPath).parent_path();

    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    if (!lexer.errors().empty()) {
        for (const auto& err : lexer.errors()) {
            std::cerr << "Lexer error: " << err << std::endl;
        }
        return 1;
    }

    Parser parser(tokens);
    parser.parse();

    if (parser.hasErrors()) {
        for (const auto& err : parser.errors()) {
            std::cerr << "Parser error: " << err << std::endl;
        }
        return 1;
    }

    try {
        auto ast = std::make_shared<AST>(std::move(parser.getAST()));
        evaluator.execute(ast);
    } catch (const RuntimeError& e) {
        std::cerr << "Runtime error";
        if (e.line > 0) {
            std::cerr << " at line " << e.line;
        }
        std::cerr << ": " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

/**
 * Run REPL mode.
 */
int runREPL(Evaluator& evaluator) {
    std::cout << "Nevaarize v0.1.0 - Native JIT Compiler" << std::endl;
    std::cout << "Type 'exit' to quit, 'help' for information." << std::endl;
    std::cout << std::endl;

    std::string line;
    std::string accumulated;

    while (true) {
        std::cout << (accumulated.empty() ? ">>> " : "... ");
        std::cout.flush();

        if (!std::getline(std::cin, line)) {
            break;
        }

        if (line == "exit" || line == "quit") {
            break;
        }

        if (line == "help") {
            std::cout << "Nevaarize REPL Commands:" << std::endl;
            std::cout << "  exit, quit  - Exit the REPL" << std::endl;
            std::cout << "  help        - Show this help" << std::endl;
            std::cout << "  clear       - Clear accumulated input" << std::endl;
            std::cout << std::endl;
            continue;
        }

        if (line == "clear") {
            accumulated.clear();
            continue;
        }

        accumulated += line + "\n";

        // Try to parse and execute
        Lexer lexer(accumulated);
        auto tokens = lexer.tokenize();

        if (!lexer.errors().empty()) {
            // Check if it's an incomplete input
            bool maybeIncomplete = false;
            for (const auto& err : lexer.errors()) {
                if (err.find("end of file") != std::string::npos) {
                    maybeIncomplete = true;
                    break;
                }
            }
            if (!maybeIncomplete) {
                for (const auto& err : lexer.errors()) {
                    std::cerr << "Error: " << err << std::endl;
                }
                accumulated.clear();
            }
            continue;
        }

        Parser parser(tokens);
        parser.parse();

        if (parser.hasErrors()) {
            bool maybeIncomplete = false;
            for (const auto& err : parser.errors()) {
                if (err.find("Expected") != std::string::npos) {
                    maybeIncomplete = true;
                    break;
                }
            }
            if (!maybeIncomplete) {
                for (const auto& err : parser.errors()) {
                    std::cerr << "Error: " << err << std::endl;
                }
                accumulated.clear();
            }
            continue;
        }

        try {
            auto ast = std::make_shared<AST>(std::move(parser.getAST()));
            Value result = evaluator.execute(ast);
            
            // Print result if not nil
            if (!result.isNil()) {
                std::cout << result.toString() << std::endl;
            }
        } catch (const RuntimeError& e) {
            std::cerr << "Runtime error: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }

        accumulated.clear();
    }

    std::cout << std::endl << "Goodbye!" << std::endl;
    return 0;
}

/**
 * Print usage information.
 */
void printUsage(const char* program) {
    std::cout << "Nevaarize - Native JIT Compiler" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: " << program << " [options] [script.nva]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help     Show this help message" << std::endl;
    std::cout << "  -v, --version  Show version information" << std::endl;
    std::cout << std::endl;
    std::cout << "If no script is provided, starts in REPL mode." << std::endl;
}

/**
 * Print version information.
 */
void printVersion() {
    std::cout << "Nevaarize v0.1.0" << std::endl;
    std::cout << "Native JIT Compiler for the Nevaarize Programming Language" << std::endl;
    std::cout << "Built with C++23, Zero External Dependencies" << std::endl;
}

} // namespace nevaarize

int main(int argc, char* argv[]) {
    using namespace nevaarize;

    // Parse command line arguments
    std::string scriptPath;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        
        if (arg == "-v" || arg == "--version") {
            printVersion();
            return 0;
        }

        if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }

        scriptPath = arg;
    }

    Evaluator evaluator;

    if (scriptPath.empty()) {
        return runREPL(evaluator);
    } else {
        return runScript(scriptPath, evaluator);
    }
}
