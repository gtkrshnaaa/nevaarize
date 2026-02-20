/**
 * IRGenerator.hpp - AST to IR Conversion
 *
 * Converts Nevaarize AST to intermediate representation
 * for JIT compilation.
 */

#ifndef NEVAARIZE_IR_GENERATOR_HPP
#define NEVAARIZE_IR_GENERATOR_HPP

#include "ast.hpp"
#include "ir.hpp"
#include <memory>
#include <unordered_map>

namespace nevaarize {

/**
 * Generates IR from AST for JIT compilation.
 */
class IRGenerator {
public:
    IRGenerator();

    /**
     * Generate IR for a function body (for loop, expression, etc.)
     */
    IRFunction generateLoop(const AST& ast, NodeIndex forStmt);

    /**
     * Generate IR for an expression.
     */
    IRIndex generateExpr(IRFunction& func, IRIndex blockIdx, 
                         const AST& ast, NodeIndex exprIdx);

    /**
     * Check if a node can be JIT compiled.
     */
    bool canCompile(const AST& ast, NodeIndex idx);

private:
    uint32_t tempCounter;

    IRIndex emitConst(IRFunction& func, IRIndex blockIdx, int64_t value);
    IRIndex emitBinaryOp(IRFunction& func, IRIndex blockIdx, 
                         IROpcode op, IRIndex left, IRIndex right);
    IRIndex emitLoad(IRFunction& func, IRIndex blockIdx, const std::string& var);
    void emitStore(IRFunction& func, IRIndex blockIdx, 
                   const std::string& var, IRIndex value);
};

} // namespace nevaarize

#endif // NEVAARIZE_IR_GENERATOR_HPP
