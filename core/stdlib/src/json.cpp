/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * JSON.cpp - Nevaarize JSON Standard Library Implementation
 */

#include "json.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <cctype>

namespace fs = std::filesystem;

namespace nevaarize {
namespace stdlib {

namespace json_internal {

class JSONParser {
public:
    explicit JSONParser(const std::string& source) : source(source), pos(0) {}

    Value parse() {
        skipWhitespace();
        return parseValue();
    }

private:
    std::string source;
    size_t pos;

    void skipWhitespace() {
        while (pos < source.size() && std::isspace(source[pos])) {
            pos++;
        }
    }

    char peek() {
        return pos < source.size() ? source[pos] : '\0';
    }

    char consume() {
        return pos < source.size() ? source[pos++] : '\0';
    }

    Value parseValue() {
        skipWhitespace();
        char c = peek();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return parseString();
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') return parseNull();
        if (std::isdigit(c) || c == '-') return parseNumber();
        return Value::nil();
    }

    Value parseObject() {
        consume(); // {
        auto mapInst = std::make_shared<MapInstance>();
        skipWhitespace();
        if (peek() == '}') {
            consume();
            return Value::fromMap(mapInst);
        }

        while (true) {
            skipWhitespace();
            Value key = parseString();
            skipWhitespace();
            if (consume() != ':') break;
            Value val = parseValue();
            mapInst->entries[key] = val;

            skipWhitespace();
            char next = peek();
            if (next == '}') {
                consume();
                break;
            }
            if (consume() != ',') break;
        }
        return Value::fromMap(mapInst);
    }

    Value parseArray() {
        consume(); // [
        std::vector<Value> array;
        skipWhitespace();
        if (peek() == ']') {
            consume();
            return Value::fromArray(std::move(array));
        }

        while (true) {
            array.push_back(parseValue());
            skipWhitespace();
            char next = peek();
            if (next == ']') {
                consume();
                break;
            }
            if (consume() != ',') break;
        }
        return Value::fromArray(std::move(array));
    }

    Value parseString() {
        consume(); // "
        std::string result;
        while (pos < source.size() && peek() != '"') {
            char c = consume();
            if (c == '\\') {
                char esc = consume();
                switch (esc) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: result += esc; break;
                }
            } else {
                result += c;
            }
        }
        consume(); // "
        return Value::fromString(result);
    }

    Value parseNumber() {
        size_t start = pos;
        bool isFloat = false;
        if (peek() == '-') consume();
        while (std::isdigit(peek())) consume();
        if (peek() == '.') {
            isFloat = true;
            consume();
            while (std::isdigit(peek())) consume();
        }
        std::string numStr = source.substr(start, pos - start);
        if (isFloat) return Value::fromFloat(std::stod(numStr));
        return Value::fromInt(std::stoll(numStr));
    }

    Value parseBool() {
        if (source.substr(pos, 4) == "true") {
            pos += 4;
            return Value::fromBool(true);
        }
        if (source.substr(pos, 5) == "false") {
            pos += 5;
            return Value::fromBool(false);
        }
        return Value::nil();
    }

    Value parseNull() {
        if (source.substr(pos, 4) == "null") {
            pos += 4;
            return Value::nil();
        }
        return Value::nil();
    }
};

} // namespace json_internal

Value parseJSONFile(const std::string& filePath, const std::string& sourceDir) {
    fs::path resolvedPath;
    if (fs::path(filePath).is_absolute()) {
        resolvedPath = filePath;
    } else if (!sourceDir.empty()) {
        resolvedPath = fs::path(sourceDir) / filePath;
    } else {
        resolvedPath = fs::current_path() / filePath;
    }
    resolvedPath = fs::weakly_canonical(resolvedPath);

    if (!fs::exists(resolvedPath)) {
        std::cerr << "JSON Error: File not found: " << resolvedPath.string() << std::endl;
        return Value::nil();
    }

    std::ifstream file(resolvedPath);
    if (!file.is_open()) {
        std::cerr << "JSON Error: Cannot open file: " << resolvedPath.string() << std::endl;
        return Value::nil();
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return parseJSONString(content);
}

Value parseJSONString(const std::string& jsonContent) {
    json_internal::JSONParser parser(jsonContent);
    return parser.parse();
}

std::unordered_map<std::string, NativeFunction> getJSONLibrary() {
    std::unordered_map<std::string, NativeFunction> funcs;
    funcs["ParseJSON"] = []([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isString() || !args[0].stringVal) return Value::nil();
        return parseJSONFile(*args[0].stringVal, "");
    };
    funcs["ParseJSONString"] = []([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isString() || !args[0].stringVal) return Value::nil();
        return parseJSONString(*args[0].stringVal);
    };
    return funcs;
}

} // namespace stdlib
} // namespace nevaarize
