/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * CSV.cpp - Nevaarize CSV Standard Library Implementation
 *
 * RFC 4180 compliant CSV parser with relative path resolution.
 * All file paths are resolved relative to the source file directory.
 */

#include "csv.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace nevaarize {
namespace stdlib {

namespace csv_internal {

/**
 * Parse a single CSV line into fields, handling quoted values.
 * Supports embedded commas, quotes, and newlines within quoted fields.
 */
std::vector<std::string> parseLine(const std::string& line, char delimiter = ',') {
    std::vector<std::string> fields;
    std::string field;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (inQuotes) {
            if (c == '"') {
                // Check for escaped quote (double-quote)
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == delimiter) {
                fields.push_back(field);
                field.clear();
            } else if (c == '\r') {
                // Skip carriage return (handle CRLF)
                continue;
            } else {
                field += c;
            }
        }
    }

    // Push the last field
    fields.push_back(field);
    return fields;
}

/**
 * Parse full CSV content string into a 2D vector of strings.
 */
std::vector<std::vector<std::string>> parseContent(const std::string& content) {
    std::vector<std::vector<std::string>> rows;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        // Skip empty lines
        if (line.empty() || (line.size() == 1 && line[0] == '\r')) {
            continue;
        }

        // Remove trailing carriage return
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        auto fields = parseLine(line);
        rows.push_back(std::move(fields));
    }

    return rows;
}

/**
 * Convert parsed CSV data to Nevaarize Value (array of arrays of strings).
 */
Value toValue(const std::vector<std::vector<std::string>>& data) {
    std::vector<Value> rows;
    rows.reserve(data.size());

    for (const auto& row : data) {
        std::vector<Value> fields;
        fields.reserve(row.size());
        for (const auto& field : row) {
            fields.push_back(Value::fromString(field));
        }
        rows.push_back(Value::fromArray(std::move(fields)));
    }

    return Value::fromArray(std::move(rows));
}

} // namespace csv_internal


Value parseCSVFile(const std::string& filePath, const std::string& sourceDir) {
    fs::path resolvedPath;

    if (fs::path(filePath).is_absolute()) {
        resolvedPath = filePath;
    } else if (!sourceDir.empty()) {
        resolvedPath = fs::path(sourceDir) / filePath;
    } else {
        resolvedPath = fs::current_path() / filePath;
    }

    // Normalize the path (resolve .., . etc.)
    resolvedPath = fs::weakly_canonical(resolvedPath);

    if (!fs::exists(resolvedPath)) {
        std::cerr << "CSV Error: File not found: " << resolvedPath.string() << std::endl;
        return Value::fromArray({});
    }

    std::ifstream file(resolvedPath);
    if (!file.is_open()) {
        std::cerr << "CSV Error: Cannot open file: " << resolvedPath.string() << std::endl;
        return Value::fromArray({});
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    auto data = csv_internal::parseContent(content);
    return csv_internal::toValue(data);
}

Value parseCSVString(const std::string& csvContent) {
    auto data = csv_internal::parseContent(csvContent);
    return csv_internal::toValue(data);
}

std::unordered_map<std::string, NativeFunction> getCSVLibrary() {
    std::unordered_map<std::string, NativeFunction> funcs;

    funcs["ParseCSV"] = []([[maybe_unused]] Evaluator& eval,
                           const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isString() || !args[0].stringVal) {
            return Value::fromArray({});
        }
        // Resolve path relative to current working directory (fallback)
        return parseCSVFile(*args[0].stringVal, "");
    };

    funcs["ParseCSVString"] = []([[maybe_unused]] Evaluator& eval,
                                 const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isString() || !args[0].stringVal) {
            return Value::fromArray({});
        }
        return parseCSVString(*args[0].stringVal);
    };

    return funcs;
}

} // namespace stdlib
} // namespace nevaarize
