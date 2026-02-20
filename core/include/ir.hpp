/**
 * IR.hpp - Nevaarize Intermediate Representation
 *
 * SSA-based IR for optimization and code generation.
 * Platform-independent representation for JIT compilation.
 */

#ifndef NEVAARIZE_IR_HPP
#define NEVAARIZE_IR_HPP

#include "value.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace nevaarize {

using IRIndex = uint32_t;
constexpr IRIndex IR_INVALID = UINT32_MAX;

/**
 * IR opcode enumeration.
 */
enum class IROpcode : uint8_t {
    // Constants
    CONST_INT,
    CONST_FLOAT,
    CONST_BOOL,
    CONST_STRING,
    CONST_NIL,

    // Variables
    LOAD_VAR,
    STORE_VAR,
    LOAD_GLOBAL,
    STORE_GLOBAL,

    // Arithmetic
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    NEG,

    // Comparison
    EQ,
    NEQ,
    LT,
    LTE,
    GT,
    GTE,

    // Logical
    AND,
    OR,
    NOT,

    // Control flow
    JUMP,
    JUMP_IF,
    JUMP_IF_NOT,
    CALL,
    RETURN,

    // Objects
    NEW_ARRAY,
    ARRAY_GET,
    ARRAY_SET,
    ARRAY_PUSH,
    NEW_STRUCT,
    FIELD_GET,
    FIELD_SET,

    // Conversions
    TO_STRING,
    TO_INT,
    TO_FLOAT,

    // Special
    PRINT,
    NOP
};

/**
 * IR instruction.
 */
struct IRInst {
    IROpcode opcode;
    IRIndex dest;
    IRIndex src1;
    IRIndex src2;

    int64_t intVal;
    double floatVal;
    std::string stringVal;

    IRInst() 
        : opcode(IROpcode::NOP)
        , dest(IR_INVALID)
        , src1(IR_INVALID)
        , src2(IR_INVALID)
        , intVal(0)
        , floatVal(0.0) {}

    explicit IRInst(IROpcode op)
        : opcode(op)
        , dest(IR_INVALID)
        , src1(IR_INVALID)
        , src2(IR_INVALID)
        , intVal(0)
        , floatVal(0.0) {}
};

/**
 * Basic block for control flow.
 */
struct BasicBlock {
    std::string label;
    std::vector<IRInst> instructions;
    IRIndex successor1 = IR_INVALID;
    IRIndex successor2 = IR_INVALID;

    explicit BasicBlock(std::string lbl = "")
        : label(std::move(lbl)) {}
};

/**
 * IR function representation.
 */
struct IRFunction {
    std::string name;
    std::vector<std::string> params;
    std::vector<BasicBlock> blocks;
    uint32_t localCount = 0;
    bool isAsync = false;

    explicit IRFunction(std::string n = "")
        : name(std::move(n)) {}

    IRIndex addBlock(const std::string& label = "") {
        IRIndex idx = static_cast<IRIndex>(blocks.size());
        blocks.emplace_back(label.empty() ? "bb" + std::to_string(idx) : label);
        return idx;
    }

    IRIndex addInst(IRIndex blockIdx, IRInst inst) {
        IRIndex idx = static_cast<IRIndex>(blocks[blockIdx].instructions.size());
        blocks[blockIdx].instructions.push_back(std::move(inst));
        return idx;
    }

    IRIndex newLocal() {
        return localCount++;
    }
};

/**
 * IR module container.
 */
class IRModule {
public:
    IRModule() = default;

    IRIndex addFunction(IRFunction func) {
        IRIndex idx = static_cast<IRIndex>(functions.size());
        functions.push_back(std::move(func));
        return idx;
    }

    IRFunction& getFunction(IRIndex idx) {
        return functions[idx];
    }

    const IRFunction& getFunction(IRIndex idx) const {
        return functions[idx];
    }

    size_t functionCount() const {
        return functions.size();
    }

    std::vector<IRFunction>& getFunctions() {
        return functions;
    }

private:
    std::vector<IRFunction> functions;
};

/**
 * Convert opcode to string for debugging.
 */
inline constexpr const char* opcodeToString(IROpcode op) {
    switch (op) {
        case IROpcode::CONST_INT: return "CONST_INT";
        case IROpcode::CONST_FLOAT: return "CONST_FLOAT";
        case IROpcode::CONST_BOOL: return "CONST_BOOL";
        case IROpcode::CONST_STRING: return "CONST_STRING";
        case IROpcode::CONST_NIL: return "CONST_NIL";
        case IROpcode::LOAD_VAR: return "LOAD_VAR";
        case IROpcode::STORE_VAR: return "STORE_VAR";
        case IROpcode::LOAD_GLOBAL: return "LOAD_GLOBAL";
        case IROpcode::STORE_GLOBAL: return "STORE_GLOBAL";
        case IROpcode::ADD: return "ADD";
        case IROpcode::SUB: return "SUB";
        case IROpcode::MUL: return "MUL";
        case IROpcode::DIV: return "DIV";
        case IROpcode::MOD: return "MOD";
        case IROpcode::NEG: return "NEG";
        case IROpcode::EQ: return "EQ";
        case IROpcode::NEQ: return "NEQ";
        case IROpcode::LT: return "LT";
        case IROpcode::LTE: return "LTE";
        case IROpcode::GT: return "GT";
        case IROpcode::GTE: return "GTE";
        case IROpcode::AND: return "AND";
        case IROpcode::OR: return "OR";
        case IROpcode::NOT: return "NOT";
        case IROpcode::JUMP: return "JUMP";
        case IROpcode::JUMP_IF: return "JUMP_IF";
        case IROpcode::JUMP_IF_NOT: return "JUMP_IF_NOT";
        case IROpcode::CALL: return "CALL";
        case IROpcode::RETURN: return "RETURN";
        case IROpcode::NEW_ARRAY: return "NEW_ARRAY";
        case IROpcode::ARRAY_GET: return "ARRAY_GET";
        case IROpcode::ARRAY_SET: return "ARRAY_SET";
        case IROpcode::ARRAY_PUSH: return "ARRAY_PUSH";
        case IROpcode::NEW_STRUCT: return "NEW_STRUCT";
        case IROpcode::FIELD_GET: return "FIELD_GET";
        case IROpcode::FIELD_SET: return "FIELD_SET";
        case IROpcode::TO_STRING: return "TO_STRING";
        case IROpcode::TO_INT: return "TO_INT";
        case IROpcode::TO_FLOAT: return "TO_FLOAT";
        case IROpcode::PRINT: return "PRINT";
        case IROpcode::NOP: return "NOP";
        default: return "UNKNOWN";
    }
}

} // namespace nevaarize

#endif // NEVAARIZE_IR_HPP
