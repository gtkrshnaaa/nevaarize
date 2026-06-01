/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * claw.cpp - Nevaarize HTML Parsing, Selector Matching and Data Exporting
 */

#include "claw.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace fs = std::filesystem;

namespace nevaarize {
namespace stdlib {

namespace claw_internal {

/**
 * Normalizes a path against the execution source directory.
 */
std::string resolvePath(const std::string& filePath, const std::string& sourceDir) {
    fs::path resolvedPath;
    if (fs::path(filePath).is_absolute()) {
        resolvedPath = filePath;
    } else if (!sourceDir.empty()) {
        resolvedPath = fs::path(sourceDir) / filePath;
    } else {
        resolvedPath = fs::current_path() / filePath;
    }
    return fs::weakly_canonical(resolvedPath).string();
}

/**
 * Escapes characters for CSV cells.
 */
std::string escapeCSV(const std::string& str) {
    bool needsQuotes = false;
    if (str.find(',') != std::string::npos ||
        str.find('"') != std::string::npos ||
        str.find('\n') != std::string::npos ||
        str.find('\r') != std::string::npos) {
        needsQuotes = true;
    }
    if (!needsQuotes) return str;

    std::string res = "\"";
    for (char c : str) {
        if (c == '"') {
            res += "\"\"";
        } else {
            res += c;
        }
    }
    res += "\"";
    return res;
}

/**
 * Stringifies a Nevaarize Value into JSON formatting.
 */
std::string valueToJSONString(const Value& val) {
    if (val.isNil()) return "null";
    if (val.isBool()) return val.boolVal ? "true" : "false";
    if (val.isInt()) return std::to_string(val.intVal);
    if (val.isFloat()) return std::to_string(val.floatVal);
    if (val.isString() && val.stringVal) {
        std::string res = "\"";
        for (char c : *val.stringVal) {
            if (c == '"') res += "\\\"";
            else if (c == '\\') res += "\\\\";
            else if (c == '\n') res += "\\n";
            else if (c == '\r') res += "\\r";
            else if (c == '\t') res += "\\t";
            else res += c;
        }
        res += "\"";
        return res;
    }
    if (val.isArray() && val.arrayVal) {
        std::string res = "[";
        const auto& arr = *val.arrayVal;
        for (size_t i = 0; i < arr.size(); ++i) {
            res += valueToJSONString(arr[i]);
            if (i + 1 < arr.size()) res += ",";
        }
        res += "]";
        return res;
    }
    if (val.isMap() && val.mapVal) {
        std::string res = "{";
        bool first = true;
        for (const auto& [k, v] : val.mapVal->entries) {
            if (!first) res += ",";
            first = false;
            res += "\"" + k.toString() + "\":" + valueToJSONString(v);
        }
        res += "}";
        return res;
    }
    return "null";
}

/**
 * Parses element attributes from a tag content substring.
 */
std::unordered_map<std::string, std::string> parseAttributes(const std::string& attrStr) {
    std::unordered_map<std::string, std::string> attrs;
    size_t i = 0;
    while (i < attrStr.size()) {
        while (i < attrStr.size() && std::isspace(attrStr[i])) ++i;
        if (i >= attrStr.size()) break;

        size_t nameStart = i;
        while (i < attrStr.size() && !std::isspace(attrStr[i]) && attrStr[i] != '=' && attrStr[i] != '/' && attrStr[i] != '>') {
            ++i;
        }
        std::string name = attrStr.substr(nameStart, i - nameStart);
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        while (i < attrStr.size() && std::isspace(attrStr[i])) ++i;

        std::string value;
        if (i < attrStr.size() && attrStr[i] == '=') {
            ++i;
            while (i < attrStr.size() && std::isspace(attrStr[i])) ++i;

            if (i < attrStr.size() && (attrStr[i] == '"' || attrStr[i] == '\'')) {
                char quote = attrStr[i];
                ++i;
                size_t valStart = i;
                while (i < attrStr.size() && attrStr[i] != quote) {
                    ++i;
                }
                value = attrStr.substr(valStart, i - valStart);
                if (i < attrStr.size()) ++i;
            } else {
                size_t valStart = i;
                while (i < attrStr.size() && !std::isspace(attrStr[i]) && attrStr[i] != '/' && attrStr[i] != '>') {
                    ++i;
                }
                value = attrStr.substr(valStart, i - valStart);
            }
        } else {
            value = name;
        }

        if (!name.empty()) {
            attrs[name] = value;
        }
    }
    return attrs;
}

/**
 * Parses raw HTML string into structured ParsedElements.
 */
std::vector<ParsedElement> parseHTML(const std::string& html) {
    std::vector<ParsedElement> elements;
    std::vector<size_t> openStack;
    size_t i = 0;

    while (i < html.size()) {
        if (html[i] == '<') {
            if (i + 4 <= html.size() && html.substr(i, 4) == "<!--") {
                size_t endComment = html.find("-->", i + 4);
                if (endComment == std::string::npos) {
                    i = html.size();
                } else {
                    i = endComment + 3;
                }
                continue;
            }

            if (i + 1 < html.size() && html[i + 1] == '/') {
                size_t endTag = html.find('>', i + 2);
                if (endTag == std::string::npos) {
                    ++i;
                    continue;
                }
                std::string tagName = html.substr(i + 2, endTag - (i + 2));
                while (!tagName.empty() && std::isspace(tagName.back())) tagName.pop_back();
                while (!tagName.empty() && std::isspace(tagName.front())) tagName.erase(tagName.begin());
                std::transform(tagName.begin(), tagName.end(), tagName.begin(), ::tolower);

                int matchIdx = -1;
                for (int s = (int)openStack.size() - 1; s >= 0; --s) {
                    if (elements[openStack[s]].tag == tagName) {
                        matchIdx = s;
                        break;
                    }
                }

                if (matchIdx != -1) {
                    for (size_t s = openStack.size() - 1; s >= (size_t)matchIdx; --s) {
                        size_t elIdx = openStack[s];
                        elements[elIdx].endPos = endTag + 1;
                        elements[elIdx].html = html.substr(elements[elIdx].startPos, elements[elIdx].endPos - elements[elIdx].startPos);
                    }
                    openStack.resize(matchIdx);
                }

                i = endTag + 1;
                continue;
            }

            if (i + 1 < html.size() && (html[i + 1] == '!' || html[i + 1] == '?')) {
                size_t endTag = html.find('>', i + 2);
                if (endTag == std::string::npos) {
                    i = html.size();
                } else {
                    i = endTag + 1;
                }
                continue;
            }

            size_t endTag = html.find('>', i + 1);
            if (endTag == std::string::npos) {
                ++i;
                continue;
            }

            std::string tagContent = html.substr(i + 1, endTag - (i + 1));
            bool isSelfClosing = false;
            if (!tagContent.empty() && tagContent.back() == '/') {
                isSelfClosing = true;
                tagContent.pop_back();
            }

            size_t nameEnd = 0;
            while (nameEnd < tagContent.size() && !std::isspace(tagContent[nameEnd])) {
                ++nameEnd;
            }
            std::string tagName = tagContent.substr(0, nameEnd);
            std::transform(tagName.begin(), tagName.end(), tagName.begin(), ::tolower);

            if (tagName.empty()) {
                ++i;
                continue;
            }

            std::string attrStr = (nameEnd < tagContent.size()) ? tagContent.substr(nameEnd) : "";
            auto attrs = parseAttributes(attrStr);

            ParsedElement el;
            el.tag = tagName;
            el.attrs = attrs;
            el.startPos = i;
            el.idIndex = (int)elements.size();
            if (!openStack.empty()) {
                el.parentId = (int)openStack.back();
            }

            static const std::unordered_set<std::string> voidTags = {
                "area", "base", "br", "col", "embed", "hr", "img", "input",
                "link", "meta", "param", "source", "track", "wbr"
            };
            if (voidTags.count(tagName)) {
                isSelfClosing = true;
            }

            elements.push_back(el);
            size_t elIdx = elements.size() - 1;

            if (isSelfClosing) {
                elements[elIdx].endPos = endTag + 1;
                elements[elIdx].html = html.substr(elements[elIdx].startPos, elements[elIdx].endPos - elements[elIdx].startPos);
            } else {
                openStack.push_back(elIdx);
            }

            i = endTag + 1;
        } else {
            size_t nextLt = html.find('<', i);
            if (nextLt == std::string::npos) nextLt = html.size();
            std::string textVal = html.substr(i, nextLt - i);

            if (!openStack.empty()) {
                size_t elIdx = openStack.back();
                elements[elIdx].ownText += textVal;
            }

            i = nextLt;
        }
    }

    for (size_t elIdx : openStack) {
        elements[elIdx].endPos = html.size();
        elements[elIdx].html = html.substr(elements[elIdx].startPos, elements[elIdx].endPos - elements[elIdx].startPos);
    }

    for (int j = (int)elements.size() - 1; j >= 0; --j) {
        std::string ownTrimmed;
        bool lastWasSpace = true;
        for (char c : elements[j].ownText) {
            if (std::isspace(c)) {
                if (!lastWasSpace) {
                    ownTrimmed += ' ';
                    lastWasSpace = true;
                }
            } else {
                ownTrimmed += c;
                lastWasSpace = false;
            }
        }
        if (!ownTrimmed.empty() && ownTrimmed.back() == ' ') ownTrimmed.pop_back();
        if (!ownTrimmed.empty() && ownTrimmed.front() == ' ') ownTrimmed.erase(ownTrimmed.begin());
        elements[j].ownText = ownTrimmed;
        elements[j].text = elements[j].ownText;
    }

    for (auto& el : elements) {
        std::string fullText;
        size_t cursor = el.startPos;
        while (cursor < el.endPos) {
            if (html[cursor] == '<') {
                size_t tagEnd = html.find('>', cursor);
                if (tagEnd == std::string::npos) break;
                cursor = tagEnd + 1;
            } else {
                size_t nextLt = html.find('<', cursor);
                if (nextLt == std::string::npos || nextLt > el.endPos) nextLt = el.endPos;
                std::string t = html.substr(cursor, nextLt - cursor);
                fullText += t;
                cursor = nextLt;
            }
        }

        std::string textTrimmed;
        bool lastWasSpace = true;
        for (char c : fullText) {
            if (std::isspace(c)) {
                if (!lastWasSpace) {
                    textTrimmed += ' ';
                    lastWasSpace = true;
                }
            } else {
                textTrimmed += c;
                lastWasSpace = false;
            }
        }
        if (!textTrimmed.empty() && textTrimmed.back() == ' ') textTrimmed.pop_back();
        if (!textTrimmed.empty() && textTrimmed.front() == ' ') textTrimmed.erase(textTrimmed.begin());
        el.text = textTrimmed;
    }

    return elements;
}

/**
 * Splits a comma-separated selector group.
 */
std::vector<std::string> splitSelector(const std::string& selectorStr) {
    std::vector<std::string> selectors;
    size_t start = 0;
    while (start < selectorStr.size()) {
        size_t comma = selectorStr.find(',', start);
        if (comma == std::string::npos) {
            selectors.push_back(selectorStr.substr(start));
            break;
        }
        selectors.push_back(selectorStr.substr(start, comma - start));
        start = comma + 1;
    }
    for (auto& s : selectors) {
        while (!s.empty() && std::isspace(s.back())) s.pop_back();
        while (!s.empty() && std::isspace(s.front())) s.erase(s.begin());
    }
    return selectors;
}

/**
 * Parses space-separated tokens of a descendant selector.
 */
std::vector<SelectorPart> parseSelectorChain(const std::string& selectorStr) {
    std::vector<SelectorPart> chain;
    std::istringstream iss(selectorStr);
    std::string partStr;

    while (iss >> partStr) {
        SelectorPart part;
        size_t idx = 0;

        if (idx < partStr.size() && partStr[idx] != '.' && partStr[idx] != '#' && partStr[idx] != '[') {
            size_t start = idx;
            while (idx < partStr.size() && partStr[idx] != '.' && partStr[idx] != '#' && partStr[idx] != '[') {
                ++idx;
            }
            part.tag = partStr.substr(start, idx - start);
            std::transform(part.tag.begin(), part.tag.end(), part.tag.begin(), ::tolower);
        }

        while (idx < partStr.size()) {
            if (partStr[idx] == '.') {
                ++idx;
                size_t start = idx;
                while (idx < partStr.size() && partStr[idx] != '.' && partStr[idx] != '#' && partStr[idx] != '[') {
                    ++idx;
                }
                part.classes.push_back(partStr.substr(start, idx - start));
            } else if (partStr[idx] == '#') {
                ++idx;
                size_t start = idx;
                while (idx < partStr.size() && partStr[idx] != '.' && partStr[idx] != '#' && partStr[idx] != '[') {
                    ++idx;
                }
                part.id = partStr.substr(start, idx - start);
            } else if (partStr[idx] == '[') {
                ++idx;
                size_t start = idx;
                while (idx < partStr.size() && partStr[idx] != ']' && partStr[idx] != '=') {
                    ++idx;
                }
                std::string attrName = partStr.substr(start, idx - start);
                std::transform(attrName.begin(), attrName.end(), attrName.begin(), ::tolower);

                AttrCond cond;
                cond.name = attrName;

                if (idx < partStr.size() && partStr[idx] == '=') {
                    cond.hasValue = true;
                    ++idx;
                    if (idx < partStr.size() && (partStr[idx] == '"' || partStr[idx] == '\'')) {
                        char quote = partStr[idx];
                        ++idx;
                        size_t valStart = idx;
                        while (idx < partStr.size() && partStr[idx] != quote) {
                            ++idx;
                        }
                        cond.value = partStr.substr(valStart, idx - valStart);
                        if (idx < partStr.size()) ++idx;
                    } else {
                        size_t valStart = idx;
                        while (idx < partStr.size() && partStr[idx] != ']') {
                            ++idx;
                        }
                        cond.value = partStr.substr(valStart, idx - valStart);
                    }
                }
                if (idx < partStr.size() && partStr[idx] == ']') {
                    ++idx;
                }
                part.attrs.push_back(cond);
            } else {
                ++idx;
            }
        }
        chain.push_back(part);
    }
    return chain;
}

/**
 * Checks if a parsed element matches selector conditions.
 */
bool matchPart(const ParsedElement& el, const SelectorPart& part) {
    if (!part.tag.empty() && el.tag != part.tag) {
        return false;
    }
    if (!part.id.empty()) {
        auto it = el.attrs.find("id");
        if (it == el.attrs.end() || it->second != part.id) {
            return false;
        }
    }
    if (!part.classes.empty()) {
        auto it = el.attrs.find("class");
        if (it == el.attrs.end()) return false;
        std::istringstream iss(it->second);
        std::string cls;
        std::unordered_set<std::string> elClasses;
        while (iss >> cls) elClasses.insert(cls);

        for (const auto& c : part.classes) {
            if (!elClasses.count(c)) return false;
        }
    }
    for (const auto& cond : part.attrs) {
        auto it = el.attrs.find(cond.name);
        if (it == el.attrs.end()) return false;
        if (cond.hasValue && it->second != cond.value) return false;
    }
    return true;
}

/**
 * Checks matches for full selector descendant chain.
 */
bool matchChain(const std::vector<ParsedElement>& elements, int elIdx, const std::vector<SelectorPart>& chain) {
    if (chain.empty()) return false;
    if (!matchPart(elements[elIdx], chain.back())) {
        return false;
    }
    if (chain.size() == 1) return true;

    int chainIdx = (int)chain.size() - 2;
    int currParentId = elements[elIdx].parentId;

    while (currParentId != -1 && chainIdx >= 0) {
        if (matchPart(elements[currParentId], chain[chainIdx])) {
            --chainIdx;
        }
        currParentId = elements[currParentId].parentId;
    }

    return chainIdx < 0;
}

/**
 * Runs the CSS selector matching on elements list.
 */
std::vector<ParsedElement> select(const std::vector<ParsedElement>& elements, const std::string& selectorStr) {
    auto selectors = splitSelector(selectorStr);
    std::vector<std::vector<SelectorPart>> chains;
    for (const auto& sel : selectors) {
        if (!sel.empty()) {
            chains.push_back(parseSelectorChain(sel));
        }
    }

    std::vector<ParsedElement> matched;
    for (size_t i = 0; i < elements.size(); ++i) {
        for (const auto& chain : chains) {
            if (matchChain(elements, (int)i, chain)) {
                matched.push_back(elements[i]);
                break;
            }
        }
    }
    return matched;
}

/**
 * Converts a parsed elements list to standard Nevaarize Value list.
 */
Value toValue(const std::vector<ParsedElement>& elements) {
    std::vector<Value> arr;
    arr.reserve(elements.size());
    for (const auto& el : elements) {
        arr.push_back(el.toValue());
    }
    return Value::fromArray(std::move(arr));
}

} // namespace claw_internal

Value ParsedElement::toValue() const {
    auto mapInst = std::make_shared<MapInstance>();
    mapInst->entries[Value::fromString("tag")] = Value::fromString(tag);
    mapInst->entries[Value::fromString("ownText")] = Value::fromString(ownText);
    mapInst->entries[Value::fromString("text")] = Value::fromString(text);
    mapInst->entries[Value::fromString("html")] = Value::fromString(html);

    auto attrsMap = std::make_shared<MapInstance>();
    for (const auto& [k, v] : attrs) {
        attrsMap->entries[Value::fromString(k)] = Value::fromString(v);
    }
    mapInst->entries[Value::fromString("attrs")] = Value::fromMap(attrsMap);

    if (parentId != -1) {
        mapInst->entries[Value::fromString("parentId")] = Value::fromInt(parentId);
    } else {
        mapInst->entries[Value::fromString("parentId")] = Value::nil();
    }
    mapInst->entries[Value::fromString("id_index")] = Value::fromInt(idIndex);

    return Value::fromMap(mapInst);
}

Value parseHTMLFile(const std::string& filePath, const std::string& sourceDir) {
    std::string resolved = claw_internal::resolvePath(filePath, sourceDir);

    if (!fs::exists(resolved)) {
        std::cerr << "Claw Error: File not found: " << resolved << std::endl;
        return Value::fromArray({});
    }

    std::ifstream file(resolved);
    if (!file.is_open()) {
        std::cerr << "Claw Error: Cannot open file: " << resolved << std::endl;
        return Value::fromArray({});
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    auto elements = claw_internal::parseHTML(content);
    return claw_internal::toValue(elements);
}

Value parseHTMLString(const std::string& htmlContent) {
    auto elements = claw_internal::parseHTML(htmlContent);
    return claw_internal::toValue(elements);
}

Value selectElements(const Value& elementsArray, const std::string& selectorStr) {
    if (!elementsArray.isArray() || !elementsArray.arrayVal) {
        return Value::fromArray({});
    }

    std::vector<ParsedElement> elements;
    elements.reserve(elementsArray.arrayVal->size());

    for (const auto& val : *elementsArray.arrayVal) {
        if (!val.isMap() || !val.mapVal) continue;
        ParsedElement el;

        auto tagIt = val.mapVal->entries.find(Value::fromString("tag"));
        if (tagIt != val.mapVal->entries.end() && tagIt->second.isString()) {
            el.tag = *tagIt->second.stringVal;
        }
        auto textIt = val.mapVal->entries.find(Value::fromString("text"));
        if (textIt != val.mapVal->entries.end() && textIt->second.isString()) {
            el.text = *textIt->second.stringVal;
        }
        auto ownTextIt = val.mapVal->entries.find(Value::fromString("ownText"));
        if (ownTextIt != val.mapVal->entries.end() && ownTextIt->second.isString()) {
            el.ownText = *ownTextIt->second.stringVal;
        }
        auto htmlIt = val.mapVal->entries.find(Value::fromString("html"));
        if (htmlIt != val.mapVal->entries.end() && htmlIt->second.isString()) {
            el.html = *htmlIt->second.stringVal;
        }

        auto pIdIt = val.mapVal->entries.find(Value::fromString("parentId"));
        if (pIdIt != val.mapVal->entries.end() && pIdIt->second.isInt()) {
            el.parentId = (int)pIdIt->second.intVal;
        }
        auto idIdxIt = val.mapVal->entries.find(Value::fromString("id_index"));
        if (idIdxIt != val.mapVal->entries.end() && idIdxIt->second.isInt()) {
            el.idIndex = (int)idIdxIt->second.intVal;
        }

        auto attrsIt = val.mapVal->entries.find(Value::fromString("attrs"));
        if (attrsIt != val.mapVal->entries.end() && attrsIt->second.isMap() && attrsIt->second.mapVal) {
            for (const auto& [k, v] : attrsIt->second.mapVal->entries) {
                if (k.isString() && v.isString()) {
                    el.attrs[*k.stringVal] = *v.stringVal;
                }
            }
        }
        elements.push_back(el);
    }

    auto matched = claw_internal::select(elements, selectorStr);
    return claw_internal::toValue(matched);
}

bool writeCSVFile(const std::string& filePath, const Value& data, const std::string& sourceDir) {
    std::string resolved = claw_internal::resolvePath(filePath, sourceDir);

    std::ofstream file(resolved);
    if (!file.is_open()) {
        std::cerr << "CSV Write Error: Cannot open file: " << resolved << std::endl;
        return false;
    }

    if (!data.isArray() || !data.arrayVal) {
        std::cerr << "CSV Write Error: Data must be an array" << std::endl;
        file.close();
        return false;
    }

    const auto& rows = *data.arrayVal;
    if (rows.empty()) {
        file.close();
        return true;
    }

    if (rows[0].isMap() && rows[0].mapVal) {
        std::vector<std::string> headers;
        for (const auto& [k, v] : rows[0].mapVal->entries) {
            if (k.isString()) {
                headers.push_back(*k.stringVal);
            }
        }

        for (size_t i = 0; i < headers.size(); ++i) {
            file << claw_internal::escapeCSV(headers[i]);
            if (i + 1 < headers.size()) file << ",";
        }
        file << "\n";

        for (const auto& row : rows) {
            if (!row.isMap() || !row.mapVal) continue;
            for (size_t i = 0; i < headers.size(); ++i) {
                Value keyVal = Value::fromString(headers[i]);
                auto it = row.mapVal->entries.find(keyVal);
                if (it != row.mapVal->entries.end()) {
                    file << claw_internal::escapeCSV(it->second.toString());
                }
                if (i + 1 < headers.size()) file << ",";
            }
            file << "\n";
        }
    } else {
        for (const auto& row : rows) {
            if (!row.isArray() || !row.arrayVal) continue;
            const auto& cols = *row.arrayVal;
            for (size_t i = 0; i < cols.size(); ++i) {
                file << claw_internal::escapeCSV(cols[i].toString());
                if (i + 1 < cols.size()) file << ",";
            }
            file << "\n";
        }
    }

    file.close();
    return true;
}

bool writeJSONFile(const std::string& filePath, const Value& data, const std::string& sourceDir) {
    std::string resolved = claw_internal::resolvePath(filePath, sourceDir);

    std::ofstream file(resolved);
    if (!file.is_open()) {
        std::cerr << "JSON Write Error: Cannot open file: " << resolved << std::endl;
        return false;
    }
    file << claw_internal::valueToJSONString(data);
    file.close();
    return true;
}

std::unordered_map<std::string, NativeFunction> getClawLibrary() {
    std::unordered_map<std::string, NativeFunction> funcs;
    // Registration maps are kept for interpreter fallback stub compatibility.
    funcs["CrawlFile"] = []([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isString() || !args[0].stringVal) return Value::fromArray({});
        return parseHTMLFile(*args[0].stringVal, "");
    };
    funcs["CrawlString"] = []([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isString() || !args[0].stringVal) return Value::fromArray({});
        return parseHTMLString(*args[0].stringVal);
    };
    funcs["Select"] = []([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || !args[1].isString() || !args[1].stringVal) return Value::fromArray({});
        return selectElements(args[0], *args[1].stringVal);
    };
    funcs["SaveCSV"] = []([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || !args[0].isString() || !args[0].stringVal) return Value::fromBool(false);
        return Value::fromBool(writeCSVFile(*args[0].stringVal, args[1], ""));
    };
    funcs["SaveJSON"] = []([[maybe_unused]] Evaluator& eval, const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || !args[0].isString() || !args[0].stringVal) return Value::fromBool(false);
        return Value::fromBool(writeJSONFile(*args[0].stringVal, args[1], ""));
    };
    return funcs;
}

} // namespace stdlib
} // namespace nevaarize
