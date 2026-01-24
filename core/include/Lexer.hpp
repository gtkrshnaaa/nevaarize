/**
 * Lexer.hpp - Nevaarize Token Scanner
 *
 * High-performance tokenizer with zero-copy string views
 * and No-Underscore Policy enforcement.
 */

#ifndef NEVAARIZE_LEXER_HPP
#define NEVAARIZE_LEXER_HPP

#include "Token.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace nevaarize {

/**
 * Lexer scans source code and produces a stream of tokens.
 * Enforces the No-Underscore Policy for all identifiers.
 */
class Lexer {
public:
    explicit Lexer(std::string_view source);

    /**
     * Tokenize the entire source and return all tokens.
     */
    std::vector<Token> tokenize();

    /**
     * Get the next token from source.
     */
    Token nextToken();

    /**
     * Check if there are more tokens to scan.
     */
    bool hasMore() const;

    /**
     * Get all errors encountered during scanning.
     */
    const std::vector<std::string>& errors() const { return errorMessages; }

private:
    std::string_view source;
    size_t start;
    size_t current;
    int32_t line;
    int32_t column;
    int32_t startColumn;
    std::vector<std::string> errorMessages;

    // Character navigation
    char peek() const;
    char peekNext() const;
    char advance();
    bool isAtEnd() const;
    bool match(char expected);

    // Character classification
    static bool isDigit(char c);
    static bool isAlpha(char c);
    static bool isAlphaNumeric(char c);

    // Token production
    Token makeToken(TokenType type) const;
    Token errorToken(const std::string& message);

    // Scanning helpers
    void skipWhitespace();
    void skipLineComment();
    Token scanString();
    Token scanNumber();
    Token scanIdentifier();

    // Keyword lookup
    static TokenType checkKeyword(std::string_view lexeme);
};

} // namespace nevaarize

#endif // NEVAARIZE_LEXER_HPP
