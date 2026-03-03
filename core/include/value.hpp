/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * Value.hpp - Nevaarize Runtime Value Representation
 *
 * Tagged union design with cold-value optimization for primitives.
 * Complex types (strings, arrays, structs) use heap allocation.
 */

#ifndef NEVAARIZE_VALUE_HPP
#define NEVAARIZE_VALUE_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <variant>

namespace nevaarize {

// Forward declarations
class Evaluator;
struct Value;
struct FunctionDef;
struct StructInstance;
struct MapInstance;

/**
 * Value type enumeration.
 */
enum class ValueType : uint8_t {
    NIL,
    BOOL,
    INT,
    FLOAT,
    STRING,
    ARRAY,
    MAP,
    STRUCT_INSTANCE,
    FUNCTION,
    NATIVE_FUNCTION,
    ASYNC_HANDLE
};

/**
 * Native function signature.
 */
using NativeFunction = std::function<Value(Evaluator&, const std::vector<Value>&)>;

/**
 * Function definition for user-defined functions.
 */
struct FunctionDef {
    std::string name;
    std::vector<std::string> params;
    uint32_t bodyIndex;
    bool isAsync;
    std::shared_ptr<class Environment> closure;
    std::shared_ptr<const class AST> moduleAST;
};

/**
 * Struct instance with named fields.
 */
struct StructInstance {
    std::string typeName;
    std::unordered_map<std::string, Value> fields;
};

/**
 * Runtime value representation.
 * Uses tagged union with smart pointers for complex types.
 */
struct Value {
    ValueType type;

    // Primitive values stored inline
    union {
        bool boolVal;
        int64_t intVal;
        double floatVal;
    };

    // Complex types use shared_ptr for reference semantics
    std::shared_ptr<std::string> stringVal;
    std::shared_ptr<std::vector<Value>> arrayVal;
    std::shared_ptr<MapInstance> mapVal;
    std::shared_ptr<StructInstance> structVal;
    std::shared_ptr<FunctionDef> funcVal;
    std::shared_ptr<NativeFunction> nativeVal;

    // Default constructor creates nil
    Value() : type(ValueType::NIL), intVal(0) {}

    // Factory methods for type safety
    static Value nil() {
        return Value();
    }

    static Value fromBool(bool b) {
        Value v;
        v.type = ValueType::BOOL;
        v.boolVal = b;
        return v;
    }

    static Value fromInt(int64_t i) {
        Value v;
        v.type = ValueType::INT;
        v.intVal = i;
        return v;
    }

    static Value fromFloat(double d) {
        Value v;
        v.type = ValueType::FLOAT;
        v.floatVal = d;
        return v;
    }

    static Value fromString(const std::string& s) {
        Value v;
        v.type = ValueType::STRING;
        v.stringVal = std::make_shared<std::string>(s);
        return v;
    }

    static Value fromString(std::string&& s) {
        Value v;
        v.type = ValueType::STRING;
        v.stringVal = std::make_shared<std::string>(std::move(s));
        return v;
    }

    static Value fromArray(const std::vector<Value>& arr) {
        Value v;
        v.type = ValueType::ARRAY;
        v.arrayVal = std::make_shared<std::vector<Value>>(arr);
        return v;
    }

    static Value fromArray(std::vector<Value>&& arr) {
        Value v;
        v.type = ValueType::ARRAY;
        v.arrayVal = std::make_shared<std::vector<Value>>(std::move(arr));
        return v;
    }

    static Value fromMap(std::shared_ptr<MapInstance> mapPtr) {
        Value v;
        v.type = ValueType::MAP;
        v.mapVal = std::move(mapPtr);
        return v;
    }

    static Value fromStruct(const StructInstance& si) {
        Value v;
        v.type = ValueType::STRUCT_INSTANCE;
        v.structVal = std::make_shared<StructInstance>(si);
        return v;
    }

    static Value fromFunction(const FunctionDef& fd) {
        Value v;
        v.type = ValueType::FUNCTION;
        v.funcVal = std::make_shared<FunctionDef>(fd);
        return v;
    }

    static Value fromNative(NativeFunction fn) {
        Value v;
        v.type = ValueType::NATIVE_FUNCTION;
        v.nativeVal = std::make_shared<NativeFunction>(std::move(fn));
        return v;
    }

    // Type checking methods
    bool isNil() const { return type == ValueType::NIL; }
    bool isBool() const { return type == ValueType::BOOL; }
    bool isInt() const { return type == ValueType::INT; }
    bool isFloat() const { return type == ValueType::FLOAT; }
    bool isNumber() const { return type == ValueType::INT || type == ValueType::FLOAT; }
    bool isString() const { return type == ValueType::STRING; }
    bool isArray() const { return type == ValueType::ARRAY; }
    bool isMap() const { return type == ValueType::MAP; }
    bool isStruct() const { return type == ValueType::STRUCT_INSTANCE; }
    bool isFunction() const { return type == ValueType::FUNCTION; }
    bool isNative() const { return type == ValueType::NATIVE_FUNCTION; }
    bool isCallable() const { return isFunction() || isNative(); }

    // Get numeric value as double (for mixed arithmetic)
    double asDouble() const {
        if (type == ValueType::INT) return static_cast<double>(intVal);
        if (type == ValueType::FLOAT) return floatVal;
        return 0.0;
    }

    // Truthiness check
    bool isTruthy() const {
        switch (type) {
            case ValueType::NIL: return false;
            case ValueType::BOOL: return boolVal;
            case ValueType::INT: return intVal != 0;
            case ValueType::FLOAT: return floatVal != 0.0;
            case ValueType::STRING: return stringVal && !stringVal->empty();
            case ValueType::ARRAY: return arrayVal && !arrayVal->empty();
            case ValueType::MAP: return mapVal != nullptr;
            default: return true;
        }
    }

    // Equality operator
    bool operator==(const Value& other) const {
        if (type != other.type) {
            if (isNumber() && other.isNumber()) {
                return asDouble() == other.asDouble();
            }
            return false;
        }
        switch (type) {
            case ValueType::NIL: return true;
            case ValueType::BOOL: return boolVal == other.boolVal;
            case ValueType::INT: return intVal == other.intVal;
            case ValueType::FLOAT: return floatVal == other.floatVal;
            case ValueType::STRING: return *stringVal == *other.stringVal;
            case ValueType::ARRAY: return arrayVal == other.arrayVal;
            case ValueType::MAP: return mapVal == other.mapVal;
            case ValueType::STRUCT_INSTANCE: return structVal == other.structVal;
            case ValueType::FUNCTION: return funcVal == other.funcVal;
            case ValueType::NATIVE_FUNCTION: return nativeVal == other.nativeVal;
            case ValueType::ASYNC_HANDLE: return false;
            default: return false;
        }
    }

    // String representation
    std::string toString() const;
};

/**
 * Convert value type to string.
 */
inline constexpr const char* valueTypeToString(ValueType type) {
    switch (type) {
        case ValueType::NIL: return "nil";
        case ValueType::BOOL: return "bool";
        case ValueType::INT: return "int";
        case ValueType::FLOAT: return "float";
        case ValueType::STRING: return "string";
        case ValueType::ARRAY: return "array";
        case ValueType::MAP: return "map";
        case ValueType::STRUCT_INSTANCE: return "struct";
        case ValueType::FUNCTION: return "function";
        case ValueType::NATIVE_FUNCTION: return "native";
        case ValueType::ASYNC_HANDLE: return "async";
        default: return "unknown";
    }
}

/**
 * Hash function for Value type.
 */
struct ValueHasher {
    size_t operator()(const Value& v) const {
        switch (v.type) {
            case ValueType::BOOL: return std::hash<bool>()(v.boolVal);
            case ValueType::INT: return std::hash<int64_t>()(v.intVal);
            case ValueType::FLOAT: return std::hash<double>()(v.floatVal);
            case ValueType::STRING: return v.stringVal ? std::hash<std::string>()(*v.stringVal) : 0;
            case ValueType::ARRAY: return std::hash<std::shared_ptr<std::vector<Value>>>()(v.arrayVal);
            case ValueType::MAP: return std::hash<std::shared_ptr<MapInstance>>()(v.mapVal);
            default: return 0;
        }
    }
};

/**
 * Map instance containing key-value pairs.
 */
struct MapInstance {
    std::unordered_map<Value, Value, ValueHasher> entries;
};

} // namespace nevaarize

#endif // NEVAARIZE_VALUE_HPP
