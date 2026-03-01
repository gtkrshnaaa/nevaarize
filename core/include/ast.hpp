/**
 * AST.hpp - Nevaarize Abstract Syntax Tree
 *
 * Flattened AST design for cache-friendly traversal.
 * Uses index-based references instead of pointers for memory efficiency.
 */

#ifndef NEVAARIZE_AST_HPP
#define NEVAARIZE_AST_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <variant>

namespace nevaarize {

using NodeIndex = uint32_t;
constexpr NodeIndex INVALID_NODE = UINT32_MAX;

/**
 * Node type enumeration for all AST node kinds.
 */
enum class NodeType : uint8_t {
    // Program
    PROGRAM,

    // Literals
    LITERAL_INT,
    LITERAL_FLOAT,
    LITERAL_STRING,
    LITERAL_BOOL,
    LITERAL_NIL,

    // Expressions
    IDENTIFIER,
    BINARY_OP,
    UNARY_OP,
    CALL,
    MEMBER_ACCESS,
    INDEX_ACCESS,
    ARRAY_LITERAL,
    MAP_LITERAL,
    STRUCT_INIT,
    AWAIT_EXPR,

    // Statements
    EXPR_STMT,
    VAR_ASSIGN,
    MEMBER_ASSIGN,
    INDEX_ASSIGN,
    BLOCK,
    IF_STMT,
    FOR_STMT,
    WHILE_STMT,
    RETURN_STMT,

    // Declarations
    FUNC_DECL,
    ASYNC_FUNC_DECL,
    STRUCT_DECL,
    IMPORT_STDLIB,
    IMPORT_FILE
};

/**
 * Binary operator types.
 */
enum class BinaryOp : uint8_t {
    ADD, SUB, MUL, DIV, MOD,
    EQ, NEQ, LT, LTE, GT, GTE,
    AND, OR
};

/**
 * Unary operator types.
 */
enum class UnaryOp : uint8_t {
    NEG,
    NOT
};

/**
 * Literal value storage.
 */
struct LiteralValue {
    std::variant<std::monostate, bool, int64_t, double, std::string> data;

    LiteralValue() : data(std::monostate{}) {}
    explicit LiteralValue(bool b) : data(b) {}
    explicit LiteralValue(int64_t i) : data(i) {}
    explicit LiteralValue(double d) : data(d) {}
    explicit LiteralValue(std::string s) : data(std::move(s)) {}
};

/**
 * AST Node structure.
 * Designed for cache efficiency with flattened layout.
 */
struct ASTNode {
    NodeType type;
    int32_t line;
    int32_t column;

    // Name for identifiers, functions, structs, fields
    std::string name;

    // Child node indices
    NodeIndex left = INVALID_NODE;
    NodeIndex right = INVALID_NODE;
    NodeIndex extra = INVALID_NODE;

    // For blocks, call arguments, array elements
    std::vector<NodeIndex> children;

    // For function/struct parameters
    std::vector<std::string> paramNames;

    // Literal value
    LiteralValue literal;

    // Operator type
    BinaryOp binaryOp = BinaryOp::ADD;
    UnaryOp unaryOp = UnaryOp::NEG;

    ASTNode() : type(NodeType::PROGRAM), line(0), column(0) {}
    
    ASTNode(NodeType t, int32_t ln, int32_t col) 
        : type(t), line(ln), column(col) {}
};

/**
 * Flattened AST container.
 * All nodes stored in contiguous memory for cache efficiency.
 */
class AST {
public:
    AST() = default;

    /**
     * Add a node to the AST and return its index.
     */
    NodeIndex addNode(ASTNode node) {
        NodeIndex idx = static_cast<NodeIndex>(nodes.size());
        nodes.push_back(std::move(node));
        return idx;
    }

    /**
     * Get a node by index.
     */
    ASTNode& get(NodeIndex idx) {
        return nodes[idx];
    }

    const ASTNode& get(NodeIndex idx) const {
        return nodes[idx];
    }

    /**
     * Get root node index.
     */
    NodeIndex root() const { return rootIndex; }

    /**
     * Set root node index.
     */
    void setRoot(NodeIndex idx) { rootIndex = idx; }

    /**
     * Get total node count.
     */
    size_t size() const { return nodes.size(); }

    /**
     * Reserve capacity for nodes.
     */
    void reserve(size_t count) { nodes.reserve(count); }

    /**
     * Clear all nodes.
     */
    void clear() {
        nodes.clear();
        rootIndex = INVALID_NODE;
    }

private:
    std::vector<ASTNode> nodes;
    NodeIndex rootIndex = INVALID_NODE;
};

/**
 * Convert node type to string for debugging.
 */
inline constexpr const char* nodeTypeToString(NodeType type) {
    switch (type) {
        case NodeType::PROGRAM: return "PROGRAM";
        case NodeType::LITERAL_INT: return "LITERAL_INT";
        case NodeType::LITERAL_FLOAT: return "LITERAL_FLOAT";
        case NodeType::LITERAL_STRING: return "LITERAL_STRING";
        case NodeType::LITERAL_BOOL: return "LITERAL_BOOL";
        case NodeType::LITERAL_NIL: return "LITERAL_NIL";
        case NodeType::IDENTIFIER: return "IDENTIFIER";
        case NodeType::BINARY_OP: return "BINARY_OP";
        case NodeType::UNARY_OP: return "UNARY_OP";
        case NodeType::CALL: return "CALL";
        case NodeType::MEMBER_ACCESS: return "MEMBER_ACCESS";
        case NodeType::INDEX_ACCESS: return "INDEX_ACCESS";
        case NodeType::ARRAY_LITERAL: return "ARRAY_LITERAL";
        case NodeType::MAP_LITERAL: return "MAP_LITERAL";
        case NodeType::STRUCT_INIT: return "STRUCT_INIT";
        case NodeType::AWAIT_EXPR: return "AWAIT_EXPR";
        case NodeType::EXPR_STMT: return "EXPR_STMT";
        case NodeType::VAR_ASSIGN: return "VAR_ASSIGN";
        case NodeType::MEMBER_ASSIGN: return "MEMBER_ASSIGN";
        case NodeType::INDEX_ASSIGN: return "INDEX_ASSIGN";
        case NodeType::BLOCK: return "BLOCK";
        case NodeType::IF_STMT: return "IF_STMT";
        case NodeType::FOR_STMT: return "FOR_STMT";
        case NodeType::WHILE_STMT: return "WHILE_STMT";
        case NodeType::RETURN_STMT: return "RETURN_STMT";
        case NodeType::FUNC_DECL: return "FUNC_DECL";
        case NodeType::ASYNC_FUNC_DECL: return "ASYNC_FUNC_DECL";
        case NodeType::STRUCT_DECL: return "STRUCT_DECL";
        case NodeType::IMPORT_STDLIB: return "IMPORT_STDLIB";
        case NodeType::IMPORT_FILE: return "IMPORT_FILE";
        default: return "UNKNOWN";
    }
}

} // namespace nevaarize

#endif // NEVAARIZE_AST_HPP
