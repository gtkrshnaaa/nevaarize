/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * Token.hpp - Nevaarize Token Definitions
 *
 * Defines token types and token structure for the lexer.
 * Zero-copy design using string_view for maximum performance.
 */

#ifndef NEVAARIZE_TOKEN_HPP
#define NEVAARIZE_TOKEN_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace nevaarize {

/**
 * Token type enumeration.
 * Ordered for efficient switch dispatch.
 */
enum class TokenType : uint8_t {
    // Literals
    INTEGER,
    FLOAT,
    STRING,
    TRUE,
    FALSE,

    // Identifiers and Keywords
    IDENTIFIER,
    FUNC,
    ASYNC,
    AWAIT,
    RETURN,
    IF,
    ELIF,
    ELSE,
    FOR,
    WHILE,
    IN,
    STRUCT,
    IMPORT,
    AS,
    STDLIB,
    TRY,
    CATCH,
    THROW,
    FINALLY,

    // Operators
    PLUS,
    MINUS,
    STAR,
    SLASH,
    PERCENT,
    EQUAL,
    EQUALEQUAL,
    BANG,
    BANGEQUAL,
    LESS,
    LESSEQUAL,
    GREATER,
    GREATEREQUAL,
    AND,
    OR,
    NOT,

    // Delimiters
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    COMMA,
    DOT,
    COLON,
    SEMICOLON,

    // Special
    NEWLINE,
    ENDOFFILE,
    ERROR
};

/**
 * Token structure.
 * Designed for cache efficiency with minimal size.
 */
struct Token {
    TokenType type;
    std::string_view lexeme;
    int32_t line;
    int32_t column;

    union {
        int64_t intValue;
        double floatValue;
    };

    Token() 
        : type(TokenType::ENDOFFILE)
        , lexeme()
        , line(0)
        , column(0)
        , intValue(0) {}

    Token(TokenType t, std::string_view lex, int32_t ln, int32_t col)
        : type(t)
        , lexeme(lex)
        , line(ln)
        , column(col)
        , intValue(0) {}

    static Token makeInt(std::string_view lex, int64_t val, int32_t ln, int32_t col) {
        Token tok(TokenType::INTEGER, lex, ln, col);
        tok.intValue = val;
        return tok;
    }

    static Token makeFloat(std::string_view lex, double val, int32_t ln, int32_t col) {
        Token tok(TokenType::FLOAT, lex, ln, col);
        tok.floatValue = val;
        return tok;
    }

    static Token makeError(std::string_view msg, int32_t ln, int32_t col) {
        return Token(TokenType::ERROR, msg, ln, col);
    }
};

/**
 * Convert token type to string for debugging.
 */
inline constexpr const char* tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::INTEGER: return "INTEGER";
        case TokenType::FLOAT: return "FLOAT";
        case TokenType::STRING: return "STRING";
        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::FUNC: return "FUNC";
        case TokenType::ASYNC: return "ASYNC";
        case TokenType::AWAIT: return "AWAIT";
        case TokenType::RETURN: return "RETURN";
        case TokenType::IF: return "IF";
        case TokenType::ELIF: return "ELIF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::FOR: return "FOR";
        case TokenType::WHILE: return "WHILE";
        case TokenType::IN: return "IN";
        case TokenType::STRUCT: return "STRUCT";
        case TokenType::IMPORT: return "IMPORT";
        case TokenType::AS: return "AS";
        case TokenType::STDLIB: return "STDLIB";
        case TokenType::TRY: return "TRY";
        case TokenType::CATCH: return "CATCH";
        case TokenType::THROW: return "THROW";
        case TokenType::FINALLY: return "FINALLY";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::PERCENT: return "PERCENT";
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::EQUALEQUAL: return "EQUALEQUAL";
        case TokenType::BANG: return "BANG";
        case TokenType::BANGEQUAL: return "BANGEQUAL";
        case TokenType::LESS: return "LESS";
        case TokenType::LESSEQUAL: return "LESSEQUAL";
        case TokenType::GREATER: return "GREATER";
        case TokenType::GREATEREQUAL: return "GREATEREQUAL";
        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::NOT: return "NOT";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::COMMA: return "COMMA";
        case TokenType::DOT: return "DOT";
        case TokenType::COLON: return "COLON";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::NEWLINE: return "NEWLINE";
        case TokenType::ENDOFFILE: return "EOF";
        case TokenType::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

} // namespace nevaarize

#endif // NEVAARIZE_TOKEN_HPP
