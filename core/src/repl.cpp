/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * Repl.cpp - Nevaarize Interactive REPL Engine
 */

#include "repl.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <iostream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace nevaarize {

Repl::Repl() {
    jit.setReplMode(true);
    jit.setSourceDir(fs::current_path().string());
}

void Repl::printBanner() {
    std::cout << "\033[1mNevaarize v0.2.3 Interactive REPL (x86-64 Native JIT)\033[0m" << std::endl;
    std::cout << "Type 'exit' or press Ctrl+D to exit. Type 'help' for commands." << std::endl;
    std::cout << std::endl;
}

void Repl::printHelp() {
    std::cout << "\033[1mNevaarize REPL Commands:\033[0m" << std::endl;
    std::cout << "  exit, quit     Exit the interactive REPL session" << std::endl;
    std::cout << "  help           Display this help summary" << std::endl;
    std::cout << "  clear          Clear the terminal screen" << std::endl;
    std::cout << "  reset          Reset execution state and declared variables" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mLanguage Features:\033[0m" << std::endl;
    std::cout << "  - Variables:   x = 10; y = 20.5; name = \"nevaarize\"" << std::endl;
    std::cout << "  - Arrays:      arr = [1, 2, 3]; arr.max(); arr.sum()" << std::endl;
    std::cout << "  - Functions:   func add(a, b) { return a + b; }" << std::endl;
    std::cout << "  - Loops:       for i in Range(0, 5) { print(i); }" << std::endl;
    std::cout << "  - Modules:     import stdlib math as m; m.Sqrt(16)" << std::endl;
    std::cout << "  - AI Models:   m = ai.Sequential(); m.predict([1.0, 2.0])" << std::endl;
    std::cout << std::endl;
}

bool Repl::isInputIncomplete(const std::string& buffer) {
    int braces = 0;
    int parens = 0;
    int brackets = 0;
    bool inString = false;

    for (size_t i = 0; i < buffer.size(); ++i) {
        char c = buffer[i];

        if (inString) {
            if (c == '\\' && i + 1 < buffer.size()) {
                ++i;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }

        if (c == '/' && i + 1 < buffer.size() && buffer[i + 1] == '/') {
            while (i < buffer.size() && buffer[i] != '\n') {
                ++i;
            }
            continue;
        }

        if (c == '"') inString = true;
        else if (c == '{') braces++;
        else if (c == '}') braces = std::max(0, braces - 1);
        else if (c == '(') parens++;
        else if (c == ')') parens = std::max(0, parens - 1);
        else if (c == '[') brackets++;
        else if (c == ']') brackets = std::max(0, brackets - 1);
    }

    if (inString || braces > 0 || parens > 0 || brackets > 0) {
        return true;
    }

    // Check trailing continuation operators
    size_t lastNonSpace = buffer.find_last_not_of(" \t\r\n");
    if (lastNonSpace != std::string::npos) {
        char lastChar = buffer[lastNonSpace];
        if (lastChar == '+' || lastChar == '-' || lastChar == '*' ||
            lastChar == '/' || lastChar == '%' || lastChar == '=' ||
            lastChar == ',' || lastChar == '<' || lastChar == '>') {
            return true;
        }
    }

    return false;
}

void Repl::evalLine(const std::string& input) {
    Lexer lexer(input);
    auto tokens = lexer.tokenize();

    if (!lexer.errors().empty()) {
        for (const auto& err : lexer.errors()) {
            std::cerr << err;
        }
        return;
    }

    Parser parser(tokens, input);
    parser.parse();

    if (parser.hasErrors()) {
        for (const auto& err : parser.errors()) {
            std::cerr << err;
        }
        return;
    }

    try {
        auto ast = std::make_shared<AST>(std::move(parser.getAST()));
        jit.retainReplAST(ast);
        auto compiledFn = jit.compile(*ast);
        jit.execute(compiledFn);
    } catch (const std::exception& e) {
        std::cerr << "\033[1;31mRuntime error:\033[0m " << e.what() << std::endl;
    }
}

int Repl::run() {
    printBanner();
    std::string accumulated;
    int consecutiveEmptyLines = 0;

    while (running) {
        if (accumulated.empty()) {
            std::cout << "\033[1;32mnevaarize>\033[0m ";
        } else {
            std::cout << "\033[1;34m... \033[0m";
        }
        std::cout.flush();

        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << std::endl;
            break;
        }

        std::string trimmedLine = line;
        size_t fLine = trimmedLine.find_first_not_of(" \t\r\n");
        if (fLine != std::string::npos) {
            size_t lLine = trimmedLine.find_last_not_of(" \t\r\n");
            trimmedLine = trimmedLine.substr(fLine, lLine - fLine + 1);
        } else {
            trimmedLine.clear();
        }

        if (trimmedLine == "exit" || trimmedLine == "quit") {
            break;
        } else if (trimmedLine == "help") {
            printHelp();
            accumulated.clear();
            continue;
        } else if (trimmedLine == "clear") {
            std::cout << "\033[2J\033[H";
            std::cout.flush();
            accumulated.clear();
            continue;
        } else if (trimmedLine == "reset") {
            jit.resetReplState();
            std::cout << "\033[33mREPL state and variables reset.\033[0m" << std::endl;
            accumulated.clear();
            continue;
        }

        if (line.empty()) {
            if (!accumulated.empty()) {
                if (!isInputIncomplete(accumulated) || consecutiveEmptyLines >= 1) {
                    if (isInputIncomplete(accumulated)) {
                        std::cout << "\033[33mContinuation cancelled.\033[0m" << std::endl;
                        accumulated.clear();
                    } else {
                        evalLine(accumulated);
                        accumulated.clear();
                    }
                    consecutiveEmptyLines = 0;
                    continue;
                }
                consecutiveEmptyLines++;
            }
            continue;
        } else {
            consecutiveEmptyLines = 0;
        }

        if (accumulated.empty()) {
            accumulated = line;
        } else {
            accumulated += "\n" + line;
        }

        if (isInputIncomplete(accumulated)) {
            continue;
        }

        evalLine(accumulated);
        accumulated.clear();
    }

    return 0;
}

} // namespace nevaarize
