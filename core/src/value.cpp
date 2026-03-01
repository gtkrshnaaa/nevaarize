/**
 * Value.cpp - Nevaarize Runtime Value Implementation
 *
 * Implementation of Value methods.
 */

#include "value.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>

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
            std::string s = oss.str();
            // If it uses scientific notation, or is a very small non-zero value, force fixed-point
            if (s.find('e') != std::string::npos || s.find('E') != std::string::npos) {
                std::ostringstream ossFixed;
                ossFixed << std::fixed << std::setprecision(20) << floatVal;
                s = ossFixed.str();
                // Trim trailing zeros
                s.erase(s.find_last_not_of('0') + 1, std::string::npos);
                if (s.back() == '.') s.pop_back();
            }
            return s;
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
            
        case ValueType::MAP: {
            if (!mapVal) return "{}";
            std::string result = "{";
            bool first = true;
            for (const auto& [key, val] : mapVal->entries) {
                if (!first) result += ", ";
                first = false;
                result += key.toString() + ": " + val.toString();
            }
            result += "}";
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
