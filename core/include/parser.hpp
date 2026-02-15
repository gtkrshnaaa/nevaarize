/**
 * Parser.hpp - Nevaarize Recursive Descent Parser
 *
 * Parses token stream into a flattened AST with expression precedence handling.
 */

#ifndef NEVAARIZE_PARSER_HPP
#define NEVAARIZE_PARSER_HPP

#include "token.hpp"
#include "ast.hpp"
#include <vector>
#include <string>
#include <initializer_list>

namespace nevaarize {

/**
 * Recursive descent parser for Nevaarize.
 * Produces a flattened AST from a token stream.
 */
class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);

    /**
     * Parse the entire token stream into an AST.
     * Returns the root node index, or INVALID_NODE on failure.
     */
    NodeIndex parse();

    /**
     * Get the constructed AST.
     */
    AST& getAST() { return ast; }
    const AST& getAST() const { return ast; }

    /**
     * Check if parsing had errors.
     */
    bool hasErrors() const { return !errorMessages.empty(); }

    /**
     * Get all error messages.
     */
    const std::vector<std::string>& errors() const { return errorMessages; }

private:
    const std::vector<Token>& tokens;
    size_t current;
    AST ast;
    std::vector<std::string> errorMessages;

    // Token navigation
    const Token& peek() const;
    const Token& previous() const;
    const Token& advance();
    bool isAtEnd() const;
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool matchAny(std::initializer_list<TokenType> types);
    Token consume(TokenType type, const std::string& message);

    // Error handling
    void error(const std::string& message);
    void synchronize();

    // Skip insignificant newlines
    void skipNewlines();

    // Grammar rules - Statements
    NodeIndex program();
    NodeIndex declaration();
    NodeIndex funcDeclaration(bool isAsync = false);
    NodeIndex structDeclaration();
    NodeIndex importStatement();
    NodeIndex statement();
    NodeIndex ifStatement();
    NodeIndex forStatement();
    NodeIndex whileStatement();
    NodeIndex returnStatement();
    NodeIndex block();
    NodeIndex expressionStatement();
    NodeIndex assignmentOrExprStmt();

    // Grammar rules - Expressions
    NodeIndex expression();
    NodeIndex orExpr();
    NodeIndex andExpr();
    NodeIndex equality();
    NodeIndex comparison();
    NodeIndex term();
    NodeIndex factor();
    NodeIndex unary();
    NodeIndex postfix();
    NodeIndex primary();

    // Helper for call arguments
    std::vector<NodeIndex> argumentList();
};

} // namespace nevaarize

#endif // NEVAARIZE_PARSER_HPP
