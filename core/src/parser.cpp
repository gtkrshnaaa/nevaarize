/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * Parser.cpp - Nevaarize Recursive Descent Parser Implementation
 *
 * Full parser implementation with expression precedence handling.
 */

#include "parser.hpp"
#include <sstream>
#include <iomanip>

namespace nevaarize {

Parser::Parser(const std::vector<Token>& toks, std::string_view src)
    : tokens(toks)
    , source(src)
    , current(0) {}

const Token& Parser::peek() const {
    return tokens[current];
}

const Token& Parser::previous() const {
    return tokens[current - 1];
}

const Token& Parser::advance() {
    if (!isAtEnd()) current++;
    return previous();
}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::ENDOFFILE;
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return false;
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::matchAny(std::initializer_list<TokenType> types) {
    for (auto type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();

    std::ostringstream oss;
    oss << message << " (got '" << peek().lexeme << "')";
    error(peek(), oss.str());

    return Token(TokenType::ERROR, "", peek().line, peek().column);
}

std::string Parser::formatError(const Token& tok, const std::string& message) const {
    std::ostringstream oss;
    oss << "Error at [Line " << tok.line << ", Col " << tok.column << "]: " << message << "\n";

    if (source.empty() || tok.type == TokenType::ENDOFFILE) {
        return oss.str();
    }

    // Calculate offset of the token in the source string
    size_t offset = tok.lexeme.data() - source.data();
    if (offset >= source.size()) {
       return oss.str();
    }

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
    oss << tok.line << " | " << errorLineView << "\n";
    oss << "  | ";
    
    // Calculate pointer position taking into account leading spaces
    for (size_t i = 0; i < (size_t)(tok.column - 1); i++) {
        if (i < errorLineView.size() && errorLineView[i] == '\t') {
            oss << '\t';
        } else {
            oss << ' ';
        }
    }
    
    // Use token length for pointer if possible, otherwise just one ^
    size_t pointerLen = tok.lexeme.length();
    if (pointerLen == 0) pointerLen = 1;
    for (size_t i = 0; i < pointerLen; i++) {
        oss << "^";
    }
    oss << "\n";
    
    return oss.str();
}

void Parser::error(const Token& tok, const std::string& message) {
    errorMessages.push_back(formatError(tok, message));
}

void Parser::synchronize() {
    advance();
    while (!isAtEnd()) {
        if (previous().type == TokenType::NEWLINE) return;
        switch (peek().type) {
            case TokenType::FUNC:
            case TokenType::ASYNC:
            case TokenType::STRUCT:
            case TokenType::IF:
            case TokenType::FOR:
            case TokenType::WHILE:
            case TokenType::RETURN:
            case TokenType::IMPORT:
                return;
            default:
                break;
        }
        advance();
    }
}

void Parser::skipNewlines() {
    while (match(TokenType::NEWLINE)) {}
}

NodeIndex Parser::parse() {
    NodeIndex root = program();
    ast.setRoot(root);
    return root;
}

NodeIndex Parser::program() {
    ASTNode node(NodeType::PROGRAM, 1, 1);

    skipNewlines();
    while (!isAtEnd()) {
        size_t beforePos = current;
        NodeIndex decl = declaration();
        if (decl != INVALID_NODE) {
            node.children.push_back(decl);
        }
        
        if (current == beforePos && !isAtEnd()) {
            advance();
        }
        
        skipNewlines();
    }

    return ast.addNode(std::move(node));
}

NodeIndex Parser::declaration() {
    try {
        if (match(TokenType::ASYNC)) {
            return funcDeclaration(true);
        }
        if (match(TokenType::FUNC)) {
            return funcDeclaration(false);
        }
        if (match(TokenType::STRUCT)) {
            return structDeclaration();
        }
        if (match(TokenType::IMPORT)) {
            return importStatement();
        }
        return statement();
    } catch (...) {
        synchronize();
        return INVALID_NODE;
    }
}

NodeIndex Parser::funcDeclaration(bool isAsync) {
    if (isAsync) {
        consume(TokenType::FUNC, "Expected 'function' after 'async'");
    }

    Token nameToken = consume(TokenType::IDENTIFIER, "Expected function name");
    consume(TokenType::LPAREN, "Expected '(' after function name");

    ASTNode node(isAsync ? NodeType::ASYNC_FUNC_DECL : NodeType::FUNC_DECL,
                 nameToken.line, nameToken.column);
    node.name = std::string(nameToken.lexeme);

    if (!check(TokenType::RPAREN)) {
        do {
            Token param = consume(TokenType::IDENTIFIER, "Expected parameter name");
            node.paramNames.push_back(std::string(param.lexeme));
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RPAREN, "Expected ')' after parameters");

    skipNewlines();
    consume(TokenType::LBRACE, "Expected '{' before function body");
    node.left = block();

    return ast.addNode(std::move(node));
}

NodeIndex Parser::structDeclaration() {
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected struct name");

    ASTNode node(NodeType::STRUCT_DECL, nameToken.line, nameToken.column);
    node.name = std::string(nameToken.lexeme);

    skipNewlines();
    consume(TokenType::LBRACE, "Expected '{' before struct fields");
    skipNewlines();

    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        Token field = consume(TokenType::IDENTIFIER, "Expected field name");
        node.paramNames.push_back(std::string(field.lexeme));

        if (!check(TokenType::RBRACE)) {
            match(TokenType::COMMA);
        }
        skipNewlines();
    }

    consume(TokenType::RBRACE, "Expected '}' after struct fields");

    return ast.addNode(std::move(node));
}

NodeIndex Parser::importStatement() {
    Token first = peek();

    if (match(TokenType::STDLIB)) {
        Token libName = consume(TokenType::IDENTIFIER, "Expected library name after 'stdlib'");
        consume(TokenType::AS, "Expected 'as' in import statement");
        Token alias = consume(TokenType::IDENTIFIER, "Expected alias name");

        ASTNode node(NodeType::IMPORT_STDLIB, first.line, first.column);
        node.name = std::string(libName.lexeme);
        node.paramNames.push_back(std::string(alias.lexeme));

        return ast.addNode(std::move(node));
    } else if (match(TokenType::STRING)) {
        Token pathToken = previous();
        consume(TokenType::AS, "Expected 'as' in import statement");
        Token alias = consume(TokenType::IDENTIFIER, "Expected alias name");

        ASTNode node(NodeType::IMPORT_FILE, first.line, first.column);
        std::string_view path = pathToken.lexeme;
        if (path.size() >= 2) {
            path = path.substr(1, path.size() - 2);
        }
        node.name = std::string(path);
        node.paramNames.push_back(std::string(alias.lexeme));

        return ast.addNode(std::move(node));
    }

    error(first, "Expected 'stdlib' or file path after 'import'");
    return INVALID_NODE;
}

NodeIndex Parser::statement() {
    if (match(TokenType::IF)) return ifStatement();
    if (match(TokenType::FOR)) return forStatement();
    if (match(TokenType::WHILE)) return whileStatement();
    if (match(TokenType::RETURN)) return returnStatement();
    if (match(TokenType::TRY)) return tryStatement();
    if (match(TokenType::THROW)) return throwStatement();
    if (match(TokenType::LBRACE)) return block();

    return assignmentOrExprStmt();
}

NodeIndex Parser::ifStatement() {
    Token ifToken = previous();
    consume(TokenType::LPAREN, "Expected '(' after 'if'");
    NodeIndex condition = expression();
    consume(TokenType::RPAREN, "Expected ')' after condition");

    skipNewlines();
    consume(TokenType::LBRACE, "Expected '{' after if condition");
    NodeIndex thenBranch = block();

    ASTNode node(NodeType::IF_STMT, ifToken.line, ifToken.column);
    node.left = condition;
    node.right = thenBranch;

    skipNewlines();

    if (match(TokenType::ELIF)) {
        Token elifToken = previous();
        consume(TokenType::LPAREN, "Expected '(' after 'elif'");
        NodeIndex elifCond = expression();
        consume(TokenType::RPAREN, "Expected ')' after condition");

        skipNewlines();
        consume(TokenType::LBRACE, "Expected '{' after elif condition");
        NodeIndex elifBody = block();

        ASTNode elifNode(NodeType::IF_STMT, elifToken.line, elifToken.column);
        elifNode.left = elifCond;
        elifNode.right = elifBody;

        skipNewlines();
        if (match(TokenType::ELIF)) {
            ASTNode wrapBlock(NodeType::BLOCK, previous().line, previous().column);
            NodeIndex nestedIf = ifStatement();
            wrapBlock.children.push_back(nestedIf);
            elifNode.extra = ast.addNode(std::move(wrapBlock));
        } else if (match(TokenType::ELSE)) {
            skipNewlines();
            consume(TokenType::LBRACE, "Expected '{' after 'else'");
            elifNode.extra = block();
        }

        NodeIndex elifIdx = ast.addNode(std::move(elifNode));

        ASTNode elseBlock(NodeType::BLOCK, elifToken.line, elifToken.column);
        elseBlock.children.push_back(elifIdx);
        node.extra = ast.addNode(std::move(elseBlock));
    } else if (match(TokenType::ELSE)) {
        skipNewlines();
        consume(TokenType::LBRACE, "Expected '{' after 'else'");
        node.extra = block();
    }

    return ast.addNode(std::move(node));
}

NodeIndex Parser::forStatement() {
    Token forToken = previous();
    consume(TokenType::LPAREN, "Expected '(' after 'for'");

    Token iterVar = consume(TokenType::IDENTIFIER, "Expected iterator variable");
    consume(TokenType::IN, "Expected 'in' after iterator variable");
    NodeIndex iterable = expression();

    consume(TokenType::RPAREN, "Expected ')' after for clause");

    skipNewlines();
    consume(TokenType::LBRACE, "Expected '{' after for clause");
    NodeIndex body = block();

    ASTNode node(NodeType::FOR_STMT, forToken.line, forToken.column);
    node.name = std::string(iterVar.lexeme);
    node.left = iterable;
    node.right = body;

    return ast.addNode(std::move(node));
}

NodeIndex Parser::whileStatement() {
    Token whileToken = previous();
    consume(TokenType::LPAREN, "Expected '(' after 'while'");
    NodeIndex condition = expression();
    consume(TokenType::RPAREN, "Expected ')' after condition");

    skipNewlines();
    consume(TokenType::LBRACE, "Expected '{' after while condition");
    NodeIndex body = block();

    ASTNode node(NodeType::WHILE_STMT, whileToken.line, whileToken.column);
    node.left = condition;
    node.right = body;

    return ast.addNode(std::move(node));
}

NodeIndex Parser::tryStatement() {
    Token tryToken = previous();
    
    skipNewlines();
    consume(TokenType::LBRACE, "Expected '{' after 'try'");
    NodeIndex tryBlock = block();
    
    skipNewlines();
    consume(TokenType::CATCH, "Expected 'catch' after 'try' block");
    
    std::string errVarName = "";
    if (match(TokenType::LPAREN)) {
        Token errToken = consume(TokenType::IDENTIFIER, "Expected error variable name after '('");
        errVarName = std::string(errToken.lexeme);
        consume(TokenType::RPAREN, "Expected ')' after catch variable");
    }
    
    skipNewlines();
    consume(TokenType::LBRACE, "Expected '{' before 'catch' block");
    NodeIndex catchBlock = block();
    
    ASTNode node(NodeType::TRY_STMT, tryToken.line, tryToken.column);
    node.left = tryBlock;
    node.right = catchBlock;
    if (!errVarName.empty()) {
        node.name = errVarName;
    }
    
    // Optional finally block
    skipNewlines();
    if (match(TokenType::FINALLY)) {
        skipNewlines();
        consume(TokenType::LBRACE, "Expected '{' after 'finally'");
        NodeIndex finallyBlock = block();
        node.children.push_back(finallyBlock);
    }
    
    return ast.addNode(std::move(node));
}

NodeIndex Parser::throwStatement() {
    Token throwToken = previous();
    
    ASTNode node(NodeType::THROW_STMT, throwToken.line, throwToken.column);
    node.left = expression();
    
    return ast.addNode(std::move(node));
}

NodeIndex Parser::returnStatement() {
    Token retToken = previous();

    ASTNode node(NodeType::RETURN_STMT, retToken.line, retToken.column);

    if (!check(TokenType::NEWLINE) && !check(TokenType::RBRACE) && !isAtEnd()) {
        node.left = expression();
    }

    return ast.addNode(std::move(node));
}

NodeIndex Parser::block() {
    Token startToken = previous();
    ASTNode node(NodeType::BLOCK, startToken.line, startToken.column);

    skipNewlines();
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        NodeIndex stmt = declaration();
        if (stmt != INVALID_NODE) {
            node.children.push_back(stmt);
        }
        skipNewlines();
    }

    consume(TokenType::RBRACE, "Expected '}' after block");

    return ast.addNode(std::move(node));
}

NodeIndex Parser::expressionStatement() {
    NodeIndex expr = expression();

    if (expr == INVALID_NODE) {
        return INVALID_NODE;
    }

    ASTNode node(NodeType::EXPR_STMT, ast.get(expr).line, ast.get(expr).column);
    node.left = expr;

    return ast.addNode(std::move(node));
}

NodeIndex Parser::assignmentOrExprStmt() {
    NodeIndex expr = expression();

    if (expr == INVALID_NODE) {
        return INVALID_NODE;
    }

    if (match(TokenType::EQUAL)) {
        NodeIndex value = expression();
        if (value == INVALID_NODE) {
            return INVALID_NODE;
        }

        const ASTNode& target = ast.get(expr);

        if (target.type == NodeType::IDENTIFIER) {
            ASTNode node(NodeType::VAR_ASSIGN, target.line, target.column);
            node.name = target.name;
            node.left = value;
            return ast.addNode(std::move(node));
        } else if (target.type == NodeType::MEMBER_ACCESS) {
            ASTNode node(NodeType::MEMBER_ASSIGN, target.line, target.column);
            node.name = target.name;
            node.left = target.left;
            node.right = value;
            return ast.addNode(std::move(node));
        } else if (target.type == NodeType::INDEX_ACCESS) {
            ASTNode node(NodeType::INDEX_ASSIGN, target.line, target.column);
            node.left = target.left;
            node.right = target.right;
            node.extra = value;
            return ast.addNode(std::move(node));
        }

        error(ast.get(expr).type == NodeType::IDENTIFIER ? previous() : previous(), "Invalid assignment target");
        return INVALID_NODE;
    }

    const ASTNode& exprNode = ast.get(expr);
    ASTNode node(NodeType::EXPR_STMT, exprNode.line, exprNode.column);
    node.left = expr;
    return ast.addNode(std::move(node));
}

NodeIndex Parser::expression() {
    if (match(TokenType::AWAIT)) {
        Token awaitToken = previous();
        NodeIndex expr = expression();

        ASTNode node(NodeType::AWAIT_EXPR, awaitToken.line, awaitToken.column);
        node.left = expr;
        return ast.addNode(std::move(node));
    }

    return orExpr();
}

NodeIndex Parser::orExpr() {
    NodeIndex left = andExpr();

    while (match(TokenType::OR)) {
        Token op = previous();
        NodeIndex right = andExpr();

        ASTNode node(NodeType::BINARY_OP, op.line, op.column);
        node.binaryOp = BinaryOp::OR;
        node.left = left;
        node.right = right;
        left = ast.addNode(std::move(node));
    }

    return left;
}

NodeIndex Parser::andExpr() {
    NodeIndex left = equality();

    while (match(TokenType::AND)) {
        Token op = previous();
        NodeIndex right = equality();

        ASTNode node(NodeType::BINARY_OP, op.line, op.column);
        node.binaryOp = BinaryOp::AND;
        node.left = left;
        node.right = right;
        left = ast.addNode(std::move(node));
    }

    return left;
}

NodeIndex Parser::equality() {
    NodeIndex left = comparison();

    while (matchAny({TokenType::EQUALEQUAL, TokenType::BANGEQUAL})) {
        Token op = previous();
        NodeIndex right = comparison();

        ASTNode node(NodeType::BINARY_OP, op.line, op.column);
        node.binaryOp = (op.type == TokenType::EQUALEQUAL) ? BinaryOp::EQ : BinaryOp::NEQ;
        node.left = left;
        node.right = right;
        left = ast.addNode(std::move(node));
    }

    return left;
}

NodeIndex Parser::comparison() {
    NodeIndex left = term();

    while (matchAny({TokenType::LESS, TokenType::LESSEQUAL,
                     TokenType::GREATER, TokenType::GREATEREQUAL})) {
        Token op = previous();
        NodeIndex right = term();

        ASTNode node(NodeType::BINARY_OP, op.line, op.column);
        switch (op.type) {
            case TokenType::LESS: node.binaryOp = BinaryOp::LT; break;
            case TokenType::LESSEQUAL: node.binaryOp = BinaryOp::LTE; break;
            case TokenType::GREATER: node.binaryOp = BinaryOp::GT; break;
            case TokenType::GREATEREQUAL: node.binaryOp = BinaryOp::GTE; break;
            default: break;
        }
        node.left = left;
        node.right = right;
        left = ast.addNode(std::move(node));
    }

    return left;
}

NodeIndex Parser::term() {
    NodeIndex left = factor();

    while (matchAny({TokenType::PLUS, TokenType::MINUS})) {
        Token op = previous();
        NodeIndex right = factor();

        ASTNode node(NodeType::BINARY_OP, op.line, op.column);
        node.binaryOp = (op.type == TokenType::PLUS) ? BinaryOp::ADD : BinaryOp::SUB;
        node.left = left;
        node.right = right;
        left = ast.addNode(std::move(node));
    }

    return left;
}

NodeIndex Parser::factor() {
    NodeIndex left = unary();

    while (matchAny({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
        Token op = previous();
        NodeIndex right = unary();

        ASTNode node(NodeType::BINARY_OP, op.line, op.column);
        switch (op.type) {
            case TokenType::STAR: node.binaryOp = BinaryOp::MUL; break;
            case TokenType::SLASH: node.binaryOp = BinaryOp::DIV; break;
            case TokenType::PERCENT: node.binaryOp = BinaryOp::MOD; break;
            default: break;
        }
        node.left = left;
        node.right = right;
        left = ast.addNode(std::move(node));
    }

    return left;
}

NodeIndex Parser::unary() {
    if (matchAny({TokenType::MINUS, TokenType::BANG, TokenType::NOT})) {
        Token op = previous();
        NodeIndex right = unary();

        ASTNode node(NodeType::UNARY_OP, op.line, op.column);
        node.unaryOp = (op.type == TokenType::MINUS) ? UnaryOp::NEG : UnaryOp::NOT;
        node.left = right;
        return ast.addNode(std::move(node));
    }

    return postfix();
}

NodeIndex Parser::postfix() {
    NodeIndex expr = primary();

    while (true) {
        if (match(TokenType::LPAREN)) {
            Token callToken = previous();
            std::vector<NodeIndex> args = argumentList();
            consume(TokenType::RPAREN, "Expected ')' after arguments");

            ASTNode node(NodeType::CALL, callToken.line, callToken.column);
            node.left = expr;
            node.children = std::move(args);
            expr = ast.addNode(std::move(node));
        } else if (match(TokenType::DOT)) {
            Token memberToken = consume(TokenType::IDENTIFIER, "Expected member name after '.'");

            ASTNode node(NodeType::MEMBER_ACCESS, memberToken.line, memberToken.column);
            node.left = expr;
            node.name = std::string(memberToken.lexeme);
            expr = ast.addNode(std::move(node));
        } else if (match(TokenType::LBRACKET)) {
            Token bracketToken = previous();
            NodeIndex index = expression();
            consume(TokenType::RBRACKET, "Expected ']' after index");

            ASTNode node(NodeType::INDEX_ACCESS, bracketToken.line, bracketToken.column);
            node.left = expr;
            node.right = index;
            expr = ast.addNode(std::move(node));
        } else {
            break;
        }
    }

    return expr;
}

NodeIndex Parser::primary() {
    Token tok = peek();

    if (match(TokenType::INTEGER)) {
        ASTNode node(NodeType::LITERAL_INT, tok.line, tok.column);
        node.literal = LiteralValue(tok.intValue);
        return ast.addNode(std::move(node));
    }

    if (match(TokenType::FLOAT)) {
        ASTNode node(NodeType::LITERAL_FLOAT, tok.line, tok.column);
        node.literal = LiteralValue(tok.floatValue);
        return ast.addNode(std::move(node));
    }

    if (match(TokenType::STRING)) {
        ASTNode node(NodeType::LITERAL_STRING, tok.line, tok.column);
        std::string_view str = tok.lexeme;
        if (str.size() >= 2) {
            str = str.substr(1, str.size() - 2);
        }
        node.literal = LiteralValue(std::string(str));
        return ast.addNode(std::move(node));
    }

    if (match(TokenType::TRUE)) {
        ASTNode node(NodeType::LITERAL_BOOL, tok.line, tok.column);
        node.literal = LiteralValue(true);
        return ast.addNode(std::move(node));
    }

    if (match(TokenType::FALSE)) {
        ASTNode node(NodeType::LITERAL_BOOL, tok.line, tok.column);
        node.literal = LiteralValue(false);
        return ast.addNode(std::move(node));
    }

    if (match(TokenType::IDENTIFIER)) {
        ASTNode node(NodeType::IDENTIFIER, tok.line, tok.column);
        node.name = std::string(tok.lexeme);
        return ast.addNode(std::move(node));
    }

    if (match(TokenType::LBRACKET)) {
        ASTNode node(NodeType::ARRAY_LITERAL, tok.line, tok.column);

        if (!check(TokenType::RBRACKET)) {
            do {
                skipNewlines();
                node.children.push_back(expression());
                skipNewlines();
            } while (match(TokenType::COMMA));
        }

        consume(TokenType::RBRACKET, "Expected ']' after array elements");
        return ast.addNode(std::move(node));
    }

    if (match(TokenType::LBRACE)) {
        ASTNode node(NodeType::MAP_LITERAL, tok.line, tok.column);

        if (!check(TokenType::RBRACE)) {
            do {
                skipNewlines();
                node.children.push_back(expression());
                consume(TokenType::COLON, "Expected ':' after map key");
                skipNewlines();
                node.children.push_back(expression());
                skipNewlines();
            } while (match(TokenType::COMMA));
        }

        consume(TokenType::RBRACE, "Expected '}' after map elements");
        return ast.addNode(std::move(node));
    }

    if (match(TokenType::LPAREN)) {
        NodeIndex expr = expression();
        consume(TokenType::RPAREN, "Expected ')' after expression");
        return expr;
    }

    error(tok, "Expected expression");
    return INVALID_NODE;
}

std::vector<NodeIndex> Parser::argumentList() {
    std::vector<NodeIndex> args;

    if (!check(TokenType::RPAREN)) {
        do {
            skipNewlines();
            args.push_back(expression());
            skipNewlines();
        } while (match(TokenType::COMMA));
    }

    return args;
}

} // namespace nevaarize
