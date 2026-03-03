/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * JSON.hpp - Nevaarize JSON Standard Library
 *
 * High-performance JSON parsing with zero external dependencies.
 * Supports parsing JSON into Nevaarize native Map and Array structures.
 */

#ifndef NEVAARIZE_STDLIB_JSON_HPP
#define NEVAARIZE_STDLIB_JSON_HPP

#include "value.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace nevaarize {
namespace stdlib {

/**
 * Get all JSON library functions.
 */
std::unordered_map<std::string, NativeFunction> getJSONLibrary();

/**
 * Parse JSON file content into Nevaarize Value.
 * Resolves relative paths against the provided source directory.
 */
Value parseJSONFile(const std::string& filePath, const std::string& sourceDir);

/**
 * Parse a JSON string directly into Nevaarize Value.
 */
Value parseJSONString(const std::string& jsonContent);

} // namespace stdlib
} // namespace nevaarize

#endif // NEVAARIZE_STDLIB_JSON_HPP
