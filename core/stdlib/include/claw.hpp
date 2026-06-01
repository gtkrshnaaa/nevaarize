/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * claw.hpp - Nevaarize Crawling and HTML Parsing Library
 */

#ifndef NEVAARIZE_STDLIB_CLAW_HPP
#define NEVAARIZE_STDLIB_CLAW_HPP

#include "value.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace nevaarize {
namespace stdlib {

/**
 * Represents a single parsed HTML element.
 */
struct ParsedElement {
    std::string tag;
    std::string ownText;
    std::string text;
    std::string html;
    std::unordered_map<std::string, std::string> attrs;
    int parentId = -1;
    int idIndex = -1;
    size_t startPos = 0;
    size_t endPos = 0;

    /**
     * Converts the parsed element into a Nevaarize Value (Map representation).
     */
    Value toValue() const;
};

/**
 * Condition for attribute selector matching.
 */
struct AttrCond {
    std::string name;
    std::string value;
    bool hasValue = false;
};

/**
 * Part of a hierarchical CSS selector chain.
 */
struct SelectorPart {
    std::string tag;
    std::string id;
    std::vector<std::string> classes;
    std::vector<AttrCond> attrs;
};

/**
 * Parse an HTML document file relative to source path directory.
 */
Value parseHTMLFile(const std::string& filePath, const std::string& sourceDir);

/**
 * Parse a raw HTML document string.
 */
Value parseHTMLString(const std::string& htmlContent);

/**
 * Query elements using a CSS selector.
 */
Value selectElements(const Value& elementsArray, const std::string& selectorStr);

/**
 * Write value representation to a CSV file.
 */
bool writeCSVFile(const std::string& filePath, const Value& data, const std::string& sourceDir);

/**
 * Write value representation to a JSON file.
 */
bool writeJSONFile(const std::string& filePath, const Value& data, const std::string& sourceDir);

/**
 * Retrieves the claw standard library methods registration map.
 */
std::unordered_map<std::string, NativeFunction> getClawLibrary();

} // namespace stdlib
} // namespace nevaarize

#endif // NEVAARIZE_STDLIB_CLAW_HPP
