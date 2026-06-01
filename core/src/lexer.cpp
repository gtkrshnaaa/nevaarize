/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * Lexer.cpp - Nevaarize Token Scanner Implementation
 *
 * Full implementation with underscore validation and fast keyword lookup.
 */

#include "lexer.hpp"
#include <charconv>
#include <array>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace nevaarize {

namespace {

// Keyword table for O(1) average lookup
struct KeywordEntry {
    std::string_view keyword;
    TokenType type;
};

constexpr std::array<KeywordEntry, 22> keywords = {{
    {"function", TokenType::FUNC},
    {"async", TokenType::ASYNC},
    {"await", TokenType::AWAIT},
    {"return", TokenType::RETURN},
    {"if", TokenType::IF},
    {"elif", TokenType::ELIF},
    {"else", TokenType::ELSE},
    {"for", TokenType::FOR},
    {"while", TokenType::WHILE},
    {"in", TokenType::IN},
    {"struct", TokenType::STRUCT},
    {"import", TokenType::IMPORT},
    {"as", TokenType::AS},
    {"stdlib", TokenType::STDLIB},
    {"true", TokenType::TRUE},
    {"false", TokenType::FALSE},
    {"and", TokenType::AND},
    {"or", TokenType::OR},
    {"try", TokenType::TRY},
    {"catch", TokenType::CATCH},
    {"throw", TokenType::THROW},
    {"finally", TokenType::FINALLY}
}};

} // anonymous namespace

Lexer::Lexer(std::string_view src)
    : source(src)
    , start(0)
    , current(0)
    , line(1)
    , column(1)
    , startColumn(1) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    tokens.reserve(source.size() / 4);
    
    while (true) {
        Token tok = nextToken();
        tokens.push_back(tok);
        if (tok.type == TokenType::ENDOFFILE) break;
    }
    
    return tokens;
}

bool Lexer::hasMore() const {
    return current < source.size();
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source[current];
}

char Lexer::peekNext() const {
    if (current + 1 >= source.size()) return '\0';
    return source[current + 1];
}

char Lexer::advance() {
    char c = source[current++];
    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    return c;
}

bool Lexer::isAtEnd() const {
    return current >= source.size();
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source[current] != expected) return false;
    advance();
    return true;
}

bool Lexer::isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool Lexer::isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z');
}

bool Lexer::isAlphaNumeric(char c) {
    return isAlpha(c) || isDigit(c);
}

Token Lexer::makeToken(TokenType type) const {
    std::string_view lexeme = source.substr(start, current - start);
    return Token(type, lexeme, line, startColumn);
}

std::string Lexer::formatError(const std::string& message, int32_t errLine, int32_t errCol, size_t offset) const {
    std::ostringstream oss;
    oss << "Error at [Line " << errLine << ", Col " << errCol << "]: " << message << "\n";
    
    // Find the start of the line
    size_t lineStart = offset;
    while (lineStart > 0 && source[lineStart - 1] != '\n') {
        lineStart--;
    }
    
    // Find the end of the line
    size_t lineEnd = offset;
    while (lineEnd < source.size() && source[lineEnd] != '\n') {
        lineEnd++;
    }
    
    std::string_view errorLineView = source.substr(lineStart, lineEnd - lineStart);
    
    // Format visual pointer
    oss << "  |\n";
    oss << errLine << " | " << errorLineView << "\n";
    oss << "  | ";
    
    // Calculate pointer position taking into account leading spaces
    for (size_t i = 0; i < (size_t)(errCol - 1); i++) {
        if (i < errorLineView.size() && errorLineView[i] == '\t') {
            oss << '\t';
        } else {
            oss << ' ';
        }
    }
    oss << "^\n";
    
    return oss.str();
}

Token Lexer::errorToken(const std::string& message) {
    std::string formattedMsg = formatError(message, line, column, current);
    errorMessages.push_back(formattedMsg);
    return Token::makeError(formattedMsg, line, column);
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                advance();
                break;
            case '/':
                if (peekNext() == '/') {
                    skipLineComment();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

void Lexer::skipLineComment() {
    while (!isAtEnd() && peek() != '\n') {
        advance();
    }
}

Token Lexer::scanString() {
    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\n') {
            return errorToken("Unterminated string: newline in string literal");
        }
        if (peek() == '\\' && peekNext() != '\0') {
            advance();
        }
        advance();
    }

    if (isAtEnd()) {
        return errorToken("Unterminated string: reached end of file");
    }

    advance();
    return makeToken(TokenType::STRING);
}

Token Lexer::scanNumber() {
    while (isDigit(peek())) advance();

    bool isFloat = false;
    if (peek() == '.' && isDigit(peekNext())) {
        isFloat = true;
        advance();
        while (isDigit(peek())) advance();
    }

    std::string_view lexeme = source.substr(start, current - start);

    if (isFloat) {
        double value = 0.0;
        auto result = std::from_chars(lexeme.data(), lexeme.data() + lexeme.size(), value);
        if (result.ec != std::errc()) {
            return errorToken("Invalid floating point number");
        }
        return Token::makeFloat(lexeme, value, line, startColumn);
    } else {
        int64_t value = 0;
        auto result = std::from_chars(lexeme.data(), lexeme.data() + lexeme.size(), value);
        if (result.ec != std::errc()) {
            return errorToken("Invalid integer number");
        }
        return Token::makeInt(lexeme, value, line, startColumn);
    }
}

TokenType Lexer::checkKeyword(std::string_view lexeme) {
    for (const auto& entry : keywords) {
        if (entry.keyword == lexeme) {
            return entry.type;
        }
    }
    return TokenType::IDENTIFIER;
}

Token Lexer::scanIdentifier() {
    while (isAlphaNumeric(peek()) || peek() == '_') {
        if (peek() == '_') {
            return errorToken("Underscore '_' is not allowed in identifiers (No-Underscore Policy)");
        }
        advance();
    }

    std::string_view lexeme = source.substr(start, current - start);
    TokenType type = checkKeyword(lexeme);
    
    return makeToken(type);
}

Token Lexer::nextToken() {
    skipWhitespace();

    start = current;
    startColumn = column;

    if (isAtEnd()) {
        return makeToken(TokenType::ENDOFFILE);
    }

    char c = advance();

    if (c == '\n') {
        return makeToken(TokenType::NEWLINE);
    }

    if (isDigit(c)) {
        return scanNumber();
    }

    if (isAlpha(c)) {
        return scanIdentifier();
    }

    if (c == '"') {
        return scanString();
    }

    switch (c) {
        case '(': return makeToken(TokenType::LPAREN);
        case ')': return makeToken(TokenType::RPAREN);
        case '{': return makeToken(TokenType::LBRACE);
        case '}': return makeToken(TokenType::RBRACE);
        case '[': return makeToken(TokenType::LBRACKET);
        case ']': return makeToken(TokenType::RBRACKET);
        case ',': return makeToken(TokenType::COMMA);
        case '.': return makeToken(TokenType::DOT);
        case ':': return makeToken(TokenType::COLON);
        case ';': return makeToken(TokenType::SEMICOLON);
        case '+': return makeToken(TokenType::PLUS);
        case '-': return makeToken(TokenType::MINUS);
        case '*': return makeToken(TokenType::STAR);
        case '/': return makeToken(TokenType::SLASH);
        case '%': return makeToken(TokenType::PERCENT);

        case '=':
            return makeToken(match('=') ? TokenType::EQUALEQUAL : TokenType::EQUAL);
        case '!':
            return makeToken(match('=') ? TokenType::BANGEQUAL : TokenType::BANG);
        case '<':
            return makeToken(match('=') ? TokenType::LESSEQUAL : TokenType::LESS);
        case '>':
            return makeToken(match('=') ? TokenType::GREATEREQUAL : TokenType::GREATER);

        case '_':
            return errorToken("Underscore '_' is not allowed as identifier start (No-Underscore Policy)");

        default:
            return errorToken("Unexpected character: " + std::string(1, c));
    }
}

} // namespace nevaarize
