/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * CSV.hpp - Nevaarize CSV Standard Library
 *
 * High-performance CSV parsing with zero external dependencies.
 * Supports RFC 4180 compliant parsing including quoted fields,
 * embedded commas, and multiline values.
 */

#ifndef NEVAARIZE_STDLIB_CSV_HPP
#define NEVAARIZE_STDLIB_CSV_HPP

#include "value.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace nevaarize {
namespace stdlib {

/**
 * Get all CSV library functions.
 */
std::unordered_map<std::string, NativeFunction> getCSVLibrary();

/**
 * Parse CSV file content into a 2D array (array of rows, each row is array of strings).
 * Resolves relative paths against the provided source directory.
 */
Value parseCSVFile(const std::string& filePath, const std::string& sourceDir);

/**
 * Parse a CSV string directly into a 2D array.
 */
Value parseCSVString(const std::string& csvContent);

} // namespace stdlib
} // namespace nevaarize

#endif // NEVAARIZE_STDLIB_CSV_HPP
