/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * string.cpp - Nevaarize Standard Library
 *
 * Provides string manipulation functions for the Nevaarize interpreter fallback.
 */

#include "string.hpp"
#include <cctype>
#include <sstream>

namespace nevaarize {
namespace stdlib {

// Helper untuk C++ string to upper
static std::string strToUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return s;
}

// Helper untuk C++ string to lower
static std::string strToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Helper trim dari kiri
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// Helper trim dari kanan
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// Helper trim
static inline std::string strTrim(std::string s) {
    rtrim(s);
    ltrim(s);
    return s;
}

// Fallbacks native C++ functions
static Value stringToUpper([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) {
    if (args.empty() || !args[0].isString() || !args[0].stringVal) return Value::nil();
    return Value::fromString(strToUpper(*args[0].stringVal));
}

static Value stringToLower([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) {
    if (args.empty() || !args[0].isString() || !args[0].stringVal) return Value::nil();
    return Value::fromString(strToLower(*args[0].stringVal));
}

static Value stringTrim([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) {
    if (args.empty() || !args[0].isString() || !args[0].stringVal) return Value::nil();
    return Value::fromString(strTrim(*args[0].stringVal));
}

static Value stringSplit([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) {
    if (args.size() < 2 || !args[0].isString() || !args[1].isString() || !args[0].stringVal || !args[1].stringVal) {
        std::vector<Value> empty;
        return Value::fromArray(empty);
    }

    std::string text = *args[0].stringVal;
    std::string delim = *args[1].stringVal;
    std::vector<Value> arr;

    size_t pos = 0;
    while ((pos = text.find(delim)) != std::string::npos) {
        std::string token = text.substr(0, pos);
        arr.push_back(Value::fromString(token));
        text.erase(0, pos + delim.length());
    }
    arr.push_back(Value::fromString(text));

    return Value::fromArray(arr);
}

static Value stringReplace([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) {
    if (args.size() < 3 || !args[0].isString() || !args[1].isString() || !args[2].isString() ||
        !args[0].stringVal || !args[1].stringVal || !args[2].stringVal) {
        return args.empty() ? Value::nil() : args[0];
    }

    std::string text = *args[0].stringVal;
    std::string from = *args[1].stringVal;
    std::string to = *args[2].stringVal;

    if (from.empty()) return Value::fromString(text);

    size_t start_pos = 0;
    while ((start_pos = text.find(from, start_pos)) != std::string::npos) {
        text.replace(start_pos, from.length(), to);
        start_pos += to.length(); 
    }
    return Value::fromString(text);
}

static Value stringSubstring([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) {
    if (args.size() < 3 || !args[0].isString() || !args[0].stringVal || !args[1].isInt() || !args[2].isInt()) {
        return Value::fromString("");
    }

    std::string text = *args[0].stringVal;
    int64_t start = args[1].intVal;
    int64_t length = args[2].intVal;

    if (start < 0) start = 0;
    if (start >= (int64_t)text.length()) return Value::fromString("");
    if (length < 0) length = 0;

    return Value::fromString(text.substr(start, length));
}

static Value stringContains([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) {
    if (args.size() < 2 || !args[0].isString() || !args[1].isString() || !args[0].stringVal || !args[1].stringVal) {
        return Value::fromInt(0);
    }
        
    std::string text = *args[0].stringVal;
    std::string sub = *args[1].stringVal;
    int64_t res = (text.find(sub) != std::string::npos) ? 1 : 0;
    return Value::fromInt(res);
}

static Value stringIndexOf([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) {
    if (args.size() < 2 || !args[0].isString() || !args[1].isString() || !args[0].stringVal || !args[1].stringVal) {
        return Value::fromInt(-1);
    }
        
    std::string text = *args[0].stringVal;
    std::string sub = *args[1].stringVal;
    size_t pos = text.find(sub);
    return Value::fromInt(static_cast<int64_t>((pos == std::string::npos) ? -1 : pos));
}

static Value stringStartsWith([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) {
    if (args.size() < 2 || !args[0].isString() || !args[1].isString() || !args[0].stringVal || !args[1].stringVal) {
        return Value::fromInt(0);
    }
        
    std::string text = *args[0].stringVal;
    std::string prefix = *args[1].stringVal;
    
    if (prefix.length() > text.length()) return Value::fromInt(0);
    int64_t res = std::equal(prefix.begin(), prefix.end(), text.begin()) ? 1 : 0;
    return Value::fromInt(res);
}

static Value stringEndsWith([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) {
    if (args.size() < 2 || !args[0].isString() || !args[1].isString() || !args[0].stringVal || !args[1].stringVal) {
        return Value::fromInt(0);
    }
        
    std::string text = *args[0].stringVal;
    std::string suffix = *args[1].stringVal;
    
    if (suffix.length() > text.length()) return Value::fromInt(0);
    int64_t res = std::equal(suffix.rbegin(), suffix.rend(), text.rbegin()) ? 1 : 0;
    return Value::fromInt(res);
}

static Value stringLength([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) {
    if (args.empty() || !args[0].isString() || !args[0].stringVal) return Value::fromInt(0);
    std::string text = *args[0].stringVal;
    return Value::fromInt(text.length());
}

std::unordered_map<std::string, NativeFunction> getStringLibrary() {
    std::unordered_map<std::string, NativeFunction> lib;
    
    lib["toUpperCase"] = stringToUpper;
    lib["toLowerCase"] = stringToLower;
    lib["trim"] = stringTrim;
    lib["split"] = stringSplit;
    lib["replace"] = stringReplace;
    lib["substring"] = stringSubstring;
    lib["contains"] = stringContains;
    lib["indexOf"] = stringIndexOf;
    lib["startsWith"] = stringStartsWith;
    lib["endsWith"] = stringEndsWith;
    lib["length"] = stringLength;
    
    return lib;
}

} // namespace stdlib
} // namespace nevaarize
