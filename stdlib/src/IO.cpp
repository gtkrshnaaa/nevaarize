/**
 * IO.cpp - Nevaarize IO Standard Library Implementation
 *
 * Input/Output functions.
 */

#include "IO.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

namespace nevaarize {
namespace stdlib {

std::unordered_map<std::string, NativeFunction> getIOLibrary() {
    std::unordered_map<std::string, NativeFunction> funcs;

    funcs["Print"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].toString();
        }
        std::cout << std::endl;
        return Value::nil();
    };

    funcs["Write"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].toString();
        }
        std::cout.flush();
        return Value::nil();
    };

    funcs["Input"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (!args.empty()) {
            std::cout << args[0].toString();
            std::cout.flush();
        }
        std::string line;
        std::getline(std::cin, line);
        return Value::fromString(line);
    };

    funcs["ReadLine"] = [](Evaluator&, const std::vector<Value>&) -> Value {
        std::string line;
        if (std::getline(std::cin, line)) {
            return Value::fromString(line);
        }
        return Value::nil();
    };

    funcs["ReadFile"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isString()) {
            return Value::nil();
        }
        
        std::ifstream file(*args[0].stringVal);
        if (!file) {
            return Value::nil();
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return Value::fromString(buffer.str());
    };

    funcs["WriteFile"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
            return Value::fromBool(false);
        }
        
        std::ofstream file(*args[0].stringVal);
        if (!file) {
            return Value::fromBool(false);
        }
        
        file << *args[1].stringVal;
        return Value::fromBool(true);
    };

    funcs["AppendFile"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
            return Value::fromBool(false);
        }
        
        std::ofstream file(*args[0].stringVal, std::ios::app);
        if (!file) {
            return Value::fromBool(false);
        }
        
        file << *args[1].stringVal;
        return Value::fromBool(true);
    };

    funcs["FileExists"] = [](Evaluator&, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isString()) {
            return Value::fromBool(false);
        }
        
        std::ifstream file(*args[0].stringVal);
        return Value::fromBool(file.good());
    };

    return funcs;
}

} // namespace stdlib
} // namespace nevaarize
