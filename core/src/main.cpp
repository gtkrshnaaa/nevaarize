/**
 * Main.cpp - Nevaarize Entry Point
 *
 * Command-line interface for the Nevaarize JIT Compiler.
 * All execution uses native x86-64 JIT compilation.
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
    std::cout << "       " << program << " model run <model.nmod>" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help     Show this help message" << std::endl;
    std::cout << "  -v, --version  Show version information" << std::endl;
}

/**
 * Print version information.
 */
void printVersion() {
    std::cout << "Nevaarize v0.1.4" << std::endl;
    std::cout << "Native JIT Compiler for the Nevaarize Programming Language" << std::endl;
    std::cout << "Built with C++23, Zero External Dependencies" << std::endl;
}

/**
 * Handle model run command.
 */
int handleModelRun(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " model run <model.nmod>" << std::endl;
        return 1;
    }
    
    try {
        std::string modelPath = argv[3];
        
        if (!fs::exists(modelPath)) {
            std::cerr << "Error: Model file not found: " << modelPath << std::endl;
            return 1;
        }
        
        std::cout << "Loading model: " << modelPath << std::endl;
        
        Model model;
        if (!model.load(modelPath)) {
            std::cerr << "Error: Failed to load model" << std::endl;
            return 1;
        }
        
        std::cout << "Model info:" << std::endl;
        std::cout << "  Input size: " << model.getInputSize() << std::endl;
        std::cout << "  Output size: " << model.getOutputSize() << std::endl;
        std::cout << std::endl;
        
        // Check for --input flag
        std::vector<float> inputData;
        for (int i = 4; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--input" && i + 1 < argc) {
                std::string inputStr = argv[++i];
                
                if (!inputStr.empty() && inputStr.front() == '[') {
                    inputStr = inputStr.substr(1);
                }
                if (!inputStr.empty() && inputStr.back() == ']') {
                    inputStr.pop_back();
                }
                
                std::stringstream ss(inputStr);
                std::string token;
                while (std::getline(ss, token, ',')) {
                    try {
                        inputData.push_back(std::stof(token));
                    } catch (...) {
                        std::cerr << "Warning: Could not parse value: " << token << std::endl;
                    }
                }
            }
        }
        
        if (!inputData.empty()) {
            std::cout << "Running inference..." << std::endl;
            auto output = model.forward(inputData);
            
            std::cout << "Output: [";
            for (size_t i = 0; i < output.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << output[i];
            }
            std::cout << "]" << std::endl;
            
            int maxIdx = 0;
            for (size_t i = 1; i < output.size(); ++i) {
                if (output[i] > output[maxIdx]) maxIdx = static_cast<int>(i);
            }
            std::cout << "Prediction: " << maxIdx << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
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

    // Handle model commands
    if (arg1 == "model" && argc >= 3) {
        std::string modelCmd = argv[2];
        
        if (modelCmd == "run") {
            return handleModelRun(argc, argv);
        }
        
        std::cerr << "Unknown model command: " << modelCmd << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // Run script
    return runScript(arg1);
}
