/**
 * Value.cpp - Nevaarize Runtime Value Implementation
 *
 * Implementation of Value methods.
 */

#include "value.hpp"
#include <sstream>

namespace nevaarize {

std::string Value::toString() const {
    switch (type) {
        case ValueType::NIL:
            return "nil";
            
        case ValueType::BOOL:
            return boolVal ? "true" : "false";
            
        case ValueType::INT:
            return std::to_string(intVal);
            
        case ValueType::FLOAT: {
            std::ostringstream oss;
            oss << floatVal;
            return oss.str();
        }
            
        case ValueType::STRING:
            return stringVal ? *stringVal : "";
            
        case ValueType::ARRAY: {
            if (!arrayVal) return "[]";
            std::string result = "[";
            for (size_t i = 0; i < arrayVal->size(); ++i) {
                if (i > 0) result += ", ";
                result += (*arrayVal)[i].toString();
            }
            result += "]";
            return result;
        }
            
        case ValueType::STRUCT_INSTANCE: {
            if (!structVal) return "{}";
            std::string result = structVal->typeName + " {";
            bool first = true;
            for (const auto& [key, val] : structVal->fields) {
                if (!first) result += ", ";
                first = false;
                result += key + ": " + val.toString();
            }
            result += "}";
            return result;
        }
            
        case ValueType::FUNCTION:
            return funcVal ? "<function " + funcVal->name + ">" : "<function>";
            
        case ValueType::NATIVE_FUNCTION:
            return "<native function>";
            
        case ValueType::ASYNC_HANDLE:
            return "<async handle>";
            
        default:
            return "<unknown>";
    }
}

} // namespace nevaarize
