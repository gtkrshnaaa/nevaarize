/**
 * Runtime.hpp - Nevaarize Runtime System
 *
 * Execution environment, environment chain for scoping,
 * and runtime error handling.
 */

#ifndef NEVAARIZE_RUNTIME_HPP
#define NEVAARIZE_RUNTIME_HPP

#include "Value.hpp"
#include "AST.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>

namespace nevaarize {

/**
 * Runtime error exception.
 */
class RuntimeError : public std::runtime_error {
public:
    int32_t line;
    int32_t column;

    RuntimeError(const std::string& message, int32_t ln = 0, int32_t col = 0)
        : std::runtime_error(message), line(ln), column(col) {}
};

/**
 * Return value exception for function returns.
 */
class ReturnException {
public:
    Value value;
    explicit ReturnException(Value v) : value(std::move(v)) {}
};

/**
 * Environment for variable scoping.
 * Implements lexical scoping with parent chain.
 */
class Environment : public std::enable_shared_from_this<Environment> {
public:
    Environment() : parent(nullptr) {}
    explicit Environment(std::shared_ptr<Environment> p) : parent(std::move(p)) {}

    /**
     * Define a new variable in this scope.
     */
    void define(const std::string& name, Value value) {
        variables[name] = std::move(value);
    }

    /**
     * Get a variable value, searching up the scope chain.
     */
    Value get(const std::string& name) const {
        auto it = variables.find(name);
        if (it != variables.end()) {
            return it->second;
        }
        if (parent) {
            return parent->get(name);
        }
        throw RuntimeError("Undefined variable: " + name);
    }

    /**
     * Set a variable value, searching up the scope chain.
     * Creates in current scope if not found.
     */
    void set(const std::string& name, Value value) {
        auto it = variables.find(name);
        if (it != variables.end()) {
            it->second = std::move(value);
            return;
        }
        if (parent) {
            if (parent->has(name)) {
                parent->set(name, std::move(value));
                return;
            }
        }
        variables[name] = std::move(value);
    }

    /**
     * Check if variable exists in any scope.
     */
    bool has(const std::string& name) const {
        if (variables.find(name) != variables.end()) {
            return true;
        }
        if (parent) {
            return parent->has(name);
        }
        return false;
    }

    /**
     * Get parent environment.
     */
    std::shared_ptr<Environment> getParent() const {
        return parent;
    }

private:
    std::unordered_map<std::string, Value> variables;
    std::shared_ptr<Environment> parent;
};

/**
 * Struct definition registry.
 */
struct StructDef {
    std::string name;
    std::vector<std::string> fields;
};

/**
 * Runtime context for execution.
 */
class RuntimeContext {
public:
    RuntimeContext() 
        : globalEnv(std::make_shared<Environment>()) {}

    std::shared_ptr<Environment> globalEnv;
    std::unordered_map<std::string, StructDef> structs;
    std::unordered_map<std::string, std::unordered_map<std::string, NativeFunction>> modules;
};

} // namespace nevaarize

#endif // NEVAARIZE_RUNTIME_HPP
