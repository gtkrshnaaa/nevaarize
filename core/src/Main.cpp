/**
 * Main.cpp - Nevaarize Entry Point
 *
 * Command-line interface and REPL implementation.
 * Supports: nevaarize [script.nva]
 *           nevaarize model train script.nva to model.nmod
 *           nevaarize model run model.nmod
 */

#include "Lexer.hpp"
#include "Parser.hpp"
#include "JIT.hpp"
#include "Model.hpp"
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
        fs::path fullPath = fs::absolute(scriptPath);
        evaluator.execute(ast, fullPath);
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
    std::cout << "       " << program << " model train <script.nva> to <model.nmod>" << std::endl;
    std::cout << "       " << program << " model run <model.nmod>" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help     Show this help message" << std::endl;
    std::cout << "  -v, --version  Show version information" << std::endl;
    std::cout << std::endl;
    std::cout << "Model Commands:" << std::endl;
    std::cout << "  model train    Train a model from script and save to .nmod" << std::endl;
    std::cout << "  model run      Load and run inference on a .nmod file" << std::endl;
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

    // Check for model subcommand
    if (argc >= 2 && std::string(argv[1]) == "model") {
        if (argc < 3) {
            std::cerr << "Error: model subcommand requires train or run" << std::endl;
            printUsage(argv[0]);
            return 1;
        }
        
        std::string modelCmd = argv[2];
        
        // MODEL TRAIN: nevaarize model train script.nva to model.nmod
        if (modelCmd == "train") {
            if (argc < 6 || std::string(argv[4]) != "to") {
                std::cerr << "Usage: " << argv[0] << " model train <script.nva> to <model.nmod>" << std::endl;
                return 1;
            }
            
            std::string trainScript = argv[3];
            std::string outputPath = argv[5];
            
            std::cout << "Training model from: " << trainScript << std::endl;
            std::cout << "Output: " << outputPath << std::endl;
            std::cout << std::endl;
            
            // Run the training script
            Evaluator evaluator;
            int result = runScript(trainScript, evaluator);
            
            if (result != 0) {
                std::cerr << "Error: Training script failed" << std::endl;
                return result;
            }
            
            // The script should have called ai.exportModel() or ai.saveModel()
            // For now, we provide a message
            std::cout << std::endl;
            std::cout << "Training complete. Use ai.saveModel(model, \"" << outputPath << "\") in your script." << std::endl;
            
            return 0;
        }
        
        // MODEL RUN: nevaarize model run model.nmod
        if (modelCmd == "run") {
            if (argc < 4) {
                std::cerr << "Usage: " << argv[0] << " model run <model.nmod>" << std::endl;
                return 1;
            }
            
            std::string modelPath = argv[3];
            
            std::cout << "Loading model: " << modelPath << std::endl;
            
            auto model = Model::load(modelPath);
            if (!model) {
                std::cerr << "Error: Failed to load model" << std::endl;
                return 1;
            }
            
            std::cout << std::endl;
            std::cout << "Model info:" << std::endl;
            std::cout << "  Input size: " << model->getInputSize() << std::endl;
            std::cout << "  Output size: " << model->getOutputSize() << std::endl;
            std::cout << "  Epochs trained: " << model->getEpoch() << std::endl;
            std::cout << "  Final loss: " << model->getFinalLoss() << std::endl;
            std::cout << std::endl;
            
            // Check for input flag
            if (argc >= 6 && std::string(argv[4]) == "--input") {
                std::string inputStr = argv[5];
                
                // Parse input array [1.0, 2.0, ...]
                std::vector<float> input;
                size_t pos = 0;
                while ((pos = inputStr.find_first_of("0123456789.-", pos)) != std::string::npos) {
                    size_t endPos = inputStr.find_first_of(",]", pos);
                    std::string numStr = inputStr.substr(pos, endPos - pos);
                    input.push_back(std::stof(numStr));
                    pos = endPos;
                    if (pos == std::string::npos) break;
                    ++pos;
                }
                
                if (!input.empty()) {
                    std::cout << "Running inference with input size: " << input.size() << std::endl;
                    
                    auto output = model->predict(input);
                    
                    std::cout << "Output: [";
                    for (size_t i = 0; i < output.size(); ++i) {
                        if (i > 0) std::cout << ", ";
                        std::cout << output[i];
                    }
                    std::cout << "]" << std::endl;
                    
                    // Find argmax
                    auto maxIt = std::max_element(output.begin(), output.end());
                    std::cout << "Prediction: " << (maxIt - output.begin()) << std::endl;
                }
            } else {
                std::cout << "To run inference: " << argv[0] << " model run " << modelPath 
                          << " --input \"[1.0, 2.0, ...]\"" << std::endl;
            }
            
            return 0;
        }
        
        std::cerr << "Unknown model command: " << modelCmd << std::endl;
        printUsage(argv[0]);
        return 1;
    }

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
