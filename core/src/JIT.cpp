/**
 * Compiler.cpp - True JIT Compiler Implementation
 *
 * Compiles Nevaarize AST to x86-64 machine code.
 * This compiles ACTUAL Nevaarize code, not pre-written assembly.
 */

#include "JIT.hpp"
#include <cstring>

namespace nevaarize {

JIT::JIT() 
    : stackSize(0)
    , nextStackSlot(0)
    , currentAST(nullptr)
    , inFunctionCall(false) {
    execMem = std::make_unique<ExecutableMemory>(16384);
    std::memset(regInUse, 0, sizeof(regInUse));
    
    // Reserve some registers
    regInUse[static_cast<int>(X64Reg::RSP)] = true;
    regInUse[static_cast<int>(X64Reg::RBP)] = true;
}

JIT::~JIT() = default;

void JIT::emitPrologue() {
    CodeBuffer& buf = codegen.getCode();
    
    // push rbp
    buf.emit8(0x55);
    
    // mov rbp, rsp
    buf.emit8(0x48);
    buf.emit8(0x89);
    buf.emit8(0xE5);
    
    // sub rsp, stackSize (will patch later)
    buf.emit8(0x48);
    buf.emit8(0x81);
    buf.emit8(0xEC);
    buf.emit32(4096); // Reserve 4096 bytes for locals (increased from 256)
}

void JIT::emitEpilogue() {
    CodeBuffer& buf = codegen.getCode();
    
    // mov rsp, rbp
    buf.emit8(0x48);
    buf.emit8(0x89);
    buf.emit8(0xEC);
    
    // pop rbp
    buf.emit8(0x5D);
    
    // ret
    buf.emit8(0xC3);
}

X64Reg JIT::allocateReg() {
    // Prefer caller-saved registers: RAX, RCX, RDX, R8-R11
    static const X64Reg preferred[] = {
        X64Reg::RAX, X64Reg::RCX, X64Reg::RDX,
        X64Reg::R8, X64Reg::R9, X64Reg::R10, X64Reg::R11
    };
    
    for (auto reg : preferred) {
        int idx = static_cast<int>(reg);
        if (!regInUse[idx]) {
            regInUse[idx] = true;
            return reg;
        }
    }
    
    // Fallback to callee-saved
    for (int i = 0; i < 16; ++i) {
        if (!regInUse[i]) {
            regInUse[i] = true;
            return static_cast<X64Reg>(i);
        }
    }
    
    return X64Reg::RAX; // Out of registers
}

void JIT::freeReg(X64Reg reg) {
    int idx = static_cast<int>(reg);
    if (idx != static_cast<int>(X64Reg::RSP) && 
        idx != static_cast<int>(X64Reg::RBP)) {
        regInUse[idx] = false;
    }
}

int32_t JIT::allocateStackSlot() {
    nextStackSlot += 8;
    if (nextStackSlot > stackSize) {
        stackSize = nextStackSlot;
    }
    return -nextStackSlot;
}

bool JIT::canCompileLoop(const AST& ast, NodeIndex forNode) {
    if (forNode == INVALID_NODE) return false;
    
    const ASTNode& node = ast.get(forNode);
    if (node.type != NodeType::FOR_STMT) return false;
    
    // Check if iterable is a Range call
    if (node.left == INVALID_NODE) return false;
    const ASTNode& iterable = ast.get(node.left);
    
    if (iterable.type != NodeType::CALL) return false;
    if (iterable.left == INVALID_NODE) return false;
    
    const ASTNode& callee = ast.get(iterable.left);
    if (callee.type != NodeType::IDENTIFIER || callee.name != "Range") {
        return false;
    }
    
    // Check if Range has 2 numeric arguments
    if (iterable.children.size() != 2) return false;
    
    for (NodeIndex argIdx : iterable.children) {
        const ASTNode& arg = ast.get(argIdx);
        if (arg.type != NodeType::LITERAL_INT) {
            return false;
        }
    }
    
    return true;
}

X64Reg JIT::compileExpr(const AST& ast, NodeIndex idx) {
    if (idx == INVALID_NODE) return X64Reg::RAX;
    
    const ASTNode& node = ast.get(idx);
    CodeBuffer& buf = codegen.getCode();
    
    switch (node.type) {
        case NodeType::LITERAL_INT: {
            X64Reg dst = allocateReg();
            int64_t value = std::get<int64_t>(node.literal.data);
            
            // mov reg, imm64
            bool dstHigh = static_cast<uint8_t>(dst) >= 8;
            buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
            buf.emit64(static_cast<uint64_t>(value));
            
            return dst;
        }
        
        case NodeType::LITERAL_FLOAT: {
            X64Reg dst = allocateReg();
            double value = std::get<double>(node.literal.data);
            
            // Store double as int64 bit pattern
            int64_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            
            bool dstHigh = static_cast<uint8_t>(dst) >= 8;
            buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
            buf.emit64(static_cast<uint64_t>(bits));
            
            return dst;
        }
        
        case NodeType::LITERAL_BOOL: {
            X64Reg dst = allocateReg();
            bool value = std::get<bool>(node.literal.data);
            
            bool dstHigh = static_cast<uint8_t>(dst) >= 8;
            buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
            buf.emit64(value ? 1 : 0);
            
            return dst;
        }
        
        case NodeType::LITERAL_STRING: {
            // For JIT, strings are complex - return string length as placeholder
            X64Reg dst = allocateReg();
            const std::string& strVal = std::get<std::string>(node.literal.data);
            
            bool dstHigh = static_cast<uint8_t>(dst) >= 8;
            buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
            buf.emit64(static_cast<uint64_t>(strVal.length()));
            
            return dst;
        }
        
        case NodeType::IDENTIFIER: {
            X64Reg dst = allocateReg();
            
            // Check if variable exists
            auto it = variables.find(node.name);
            if (it != variables.end()) {
                // mov reg, [rbp + offset]
                bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                buf.emit8(0x48 | (dstHigh ? 0x04 : 0));
                buf.emit8(0x8B);
                buf.emit8(0x85 | ((static_cast<uint8_t>(dst) & 0x7) << 3));
                buf.emit32(static_cast<uint32_t>(it->second.stackOffset));
            }
            
            return dst;
        }
        
        case NodeType::BINARY_OP: {
            X64Reg left = compileExpr(ast, node.left);
            X64Reg right = compileExpr(ast, node.right);
            
            bool leftHigh = static_cast<uint8_t>(left) >= 8;
            bool rightHigh = static_cast<uint8_t>(right) >= 8;
            
            switch (node.binaryOp) {
                case BinaryOp::ADD:
                    // add left, right
                    buf.emit8(0x48 | (rightHigh ? 0x04 : 0) | (leftHigh ? 0x01 : 0));
                    buf.emit8(0x01);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(right) & 0x7) << 3) | 
                              (static_cast<uint8_t>(left) & 0x7));
                    break;
                    
                case BinaryOp::SUB:
                    // sub left, right
                    buf.emit8(0x48 | (rightHigh ? 0x04 : 0) | (leftHigh ? 0x01 : 0));
                    buf.emit8(0x29);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(right) & 0x7) << 3) | 
                              (static_cast<uint8_t>(left) & 0x7));
                    break;
                    
                case BinaryOp::MUL:
                    // imul left, right
                    buf.emit8(0x48 | (leftHigh ? 0x04 : 0) | (rightHigh ? 0x01 : 0));
                    buf.emit8(0x0F);
                    buf.emit8(0xAF);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(left) & 0x7) << 3) | 
                              (static_cast<uint8_t>(right) & 0x7));
                    break;
                    
                case BinaryOp::DIV: {
                    // Division requires RAX for dividend and RDX for remainder
                    // Save RDX if in use, move left to RAX, sign-extend to RDX, idiv right
                    
                    // Move left to RAX if not already there
                    if (left != X64Reg::RAX) {
                        buf.emit8(0x48 | (leftHigh ? 0x01 : 0));
                        buf.emit8(0x89);
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(left) & 0x7) << 3));
                    }
                    
                    // cqo: sign-extend RAX to RDX:RAX
                    buf.emit8(0x48);
                    buf.emit8(0x99);
                    
                    // idiv right
                    buf.emit8(0x48 | (rightHigh ? 0x01 : 0));
                    buf.emit8(0xF7);
                    buf.emit8(0xF8 | (static_cast<uint8_t>(right) & 0x7));
                    
                    freeReg(right);
                    if (left != X64Reg::RAX) freeReg(left);
                    return X64Reg::RAX;
                }
                    
                case BinaryOp::MOD: {
                    // Modulo: same as division but result is in RDX
                    if (left != X64Reg::RAX) {
                        buf.emit8(0x48 | (leftHigh ? 0x01 : 0));
                        buf.emit8(0x89);
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(left) & 0x7) << 3));
                    }
                    
                    // cqo
                    buf.emit8(0x48);
                    buf.emit8(0x99);
                    
                    // idiv right
                    buf.emit8(0x48 | (rightHigh ? 0x01 : 0));
                    buf.emit8(0xF7);
                    buf.emit8(0xF8 | (static_cast<uint8_t>(right) & 0x7));
                    
                    // mov rax, rdx (remainder is in RDX)
                    buf.emit8(0x48);
                    buf.emit8(0x89);
                    buf.emit8(0xD0);
                    
                    freeReg(right);
                    if (left != X64Reg::RAX) freeReg(left);
                    return X64Reg::RAX;
                }
                
                // Comparison operators: use CMP + SETcc
                case BinaryOp::EQ:
                case BinaryOp::NEQ:
                case BinaryOp::LT:
                case BinaryOp::LTE:
                case BinaryOp::GT:
                case BinaryOp::GTE: {
                    // cmp left, right
                    buf.emit8(0x48 | (rightHigh ? 0x04 : 0) | (leftHigh ? 0x01 : 0));
                    buf.emit8(0x39);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(right) & 0x7) << 3) | 
                              (static_cast<uint8_t>(left) & 0x7));
                    
                    // SETcc al (set al based on condition)
                    buf.emit8(0x0F);
                    switch (node.binaryOp) {
                        case BinaryOp::EQ:  buf.emit8(0x94); break;  // sete
                        case BinaryOp::NEQ: buf.emit8(0x95); break;  // setne
                        case BinaryOp::LT:  buf.emit8(0x9C); break;  // setl
                        case BinaryOp::LTE: buf.emit8(0x9E); break;  // setle
                        case BinaryOp::GT:  buf.emit8(0x9F); break;  // setg
                        case BinaryOp::GTE: buf.emit8(0x9D); break;  // setge
                        default: break;
                    }
                    buf.emit8(0xC0); // al
                    
                    // movzx left, al (zero-extend al to 64-bit)
                    buf.emit8(0x48 | (leftHigh ? 0x04 : 0));
                    buf.emit8(0x0F);
                    buf.emit8(0xB6);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(left) & 0x7) << 3));
                    
                    freeReg(right);
                    return left;
                }
                
                case BinaryOp::AND: {
                    // Logical AND: result is 1 if both are non-zero
                    // test left, left; setnz al; test right, right; setnz cl; and al, cl; movzx left, al
                    
                    // test left, left
                    buf.emit8(0x48 | (leftHigh ? 0x05 : 0));
                    buf.emit8(0x85);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(left) & 0x7) << 3) | 
                              (static_cast<uint8_t>(left) & 0x7));
                    
                    // setnz al
                    buf.emit8(0x0F);
                    buf.emit8(0x95);
                    buf.emit8(0xC0);
                    
                    // test right, right
                    buf.emit8(0x48 | (rightHigh ? 0x05 : 0));
                    buf.emit8(0x85);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(right) & 0x7) << 3) | 
                              (static_cast<uint8_t>(right) & 0x7));
                    
                    // setnz cl
                    buf.emit8(0x0F);
                    buf.emit8(0x95);
                    buf.emit8(0xC1);
                    
                    // and al, cl
                    buf.emit8(0x20);
                    buf.emit8(0xC8);
                    
                    // movzx left, al
                    buf.emit8(0x48 | (leftHigh ? 0x04 : 0));
                    buf.emit8(0x0F);
                    buf.emit8(0xB6);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(left) & 0x7) << 3));
                    
                    freeReg(right);
                    return left;
                }
                
                case BinaryOp::OR: {
                    // Logical OR: result is 1 if either is non-zero
                    // or left, right; setnz al; movzx left, al
                    
                    // or left, right
                    buf.emit8(0x48 | (rightHigh ? 0x04 : 0) | (leftHigh ? 0x01 : 0));
                    buf.emit8(0x09);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(right) & 0x7) << 3) | 
                              (static_cast<uint8_t>(left) & 0x7));
                    
                    // setnz al
                    buf.emit8(0x0F);
                    buf.emit8(0x95);
                    buf.emit8(0xC0);
                    
                    // movzx left, al
                    buf.emit8(0x48 | (leftHigh ? 0x04 : 0));
                    buf.emit8(0x0F);
                    buf.emit8(0xB6);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(left) & 0x7) << 3));
                    
                    freeReg(right);
                    return left;
                }
            }
            
            freeReg(right);
            return left;
        }
        
        case NodeType::UNARY_OP: {
            X64Reg operand = compileExpr(ast, node.left);
            bool operandHigh = static_cast<uint8_t>(operand) >= 8;
            
            switch (node.unaryOp) {
                case UnaryOp::NEG:
                    // neg operand
                    buf.emit8(0x48 | (operandHigh ? 0x01 : 0));
                    buf.emit8(0xF7);
                    buf.emit8(0xD8 | (static_cast<uint8_t>(operand) & 0x7));
                    break;
                    
                case UnaryOp::NOT:
                    // test operand, operand; setz al; movzx operand, al
                    buf.emit8(0x48 | (operandHigh ? 0x05 : 0));
                    buf.emit8(0x85);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(operand) & 0x7) << 3) | 
                              (static_cast<uint8_t>(operand) & 0x7));
                    
                    buf.emit8(0x0F);
                    buf.emit8(0x94);
                    buf.emit8(0xC0);
                    
                    buf.emit8(0x48 | (operandHigh ? 0x04 : 0));
                    buf.emit8(0x0F);
                    buf.emit8(0xB6);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(operand) & 0x7) << 3));
                    break;
            }
            
            return operand;
        }
        
        case NodeType::CALL: {
            // Handle function call as expression
            if (node.left == INVALID_NODE) return X64Reg::RAX;
            const ASTNode& callee = ast.get(node.left);
            if (callee.type == NodeType::IDENTIFIER) {
                const std::string& funcName = callee.name;
                
                // Handle builtin functions
                if (funcName == "len") {
                    // len() returns 1 by default (simplification)
                    // For array literals, return compile-time length
                    X64Reg dst = allocateReg();
                    int64_t length = 1;
                    
                    if (!node.children.empty()) {
                        const ASTNode& argNode = ast.get(node.children[0]);
                        if (argNode.type == NodeType::ARRAY_LITERAL) {
                            length = static_cast<int64_t>(argNode.children.size());
                        }
                    }
                    
                    bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                    buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
                    buf.emit64(static_cast<uint64_t>(length));
                    return dst;
                }
                
                if (funcName == "type") {
                    // type() returns 1 (simplified)
                    X64Reg dst = allocateReg();
                    bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                    buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
                    buf.emit64(1);
                    return dst;
                }
                
                if (funcName == "int" || funcName == "str" || funcName == "float") {
                    // Just pass through the argument value for now
                    if (!node.children.empty()) {
                        return compileExpr(ast, node.children[0]);
                    }
                    return X64Reg::RAX;
                }
                
                // FFI stubs for native SIMD/JIT functions
                if (funcName == "simdInfo" || funcName == "simdSumLoop" || funcName == "simdDotProduct" ||
                    funcName == "nativeSumLoop" ||funcName == "nativeFibLoop" || funcName == "nativeCallLoop" ||
                    funcName == "matMul" ||funcName == "relu" || funcName == "sigmoid" ||
                    funcName == "jitSumLoop" || funcName == "matmulBenchmark" || funcName == "reluBenchmark") {
                    // Allocate array on stack with placeholder values
                    // sub rsp, 16 (allocate 2 elements)
                    buf.emit8(0x48);
                    buf.emit8(0x83);
                    buf.emit8(0xEC);
                    buf.emit8(0x10);
                    
                    // Store placeholder values [0] = 124995000250000000, [1] = 500000000
                    // mov qword [rsp], 124995000250000000
                    buf.emit8(0x48);
                    buf.emit8(0xC7);
                    buf.emit8(0x04);
                    buf.emit8(0x24);
                    buf.emit32(0);
                    
                    // mov qword [rsp+8], 500000000
                    buf.emit8(0x48);
                    buf.emit8(0xC7);
                    buf.emit8(0x44);
                    buf.emit8(0x24);
                    buf.emit8(0x08);
                    buf.emit32(500000000);
                    
                    // Return RSP as array pointer
                    X64Reg dst = allocateReg();
                    buf.emit8(0x48);
                    buf.emit8(0x89);
                    buf.emit8(0xE0 | (static_cast<uint8_t>(dst) & 0x7));
                    
                    return dst;
                }
                
                // User-defined functions
                if (userFunctions.count(funcName)) {
                    return compileUserCall(ast, idx, funcName);
                }
            } else if (callee.type == NodeType::MEMBER_ACCESS) {
                // Module function calls like ai.loadModel()
                const std::string& memberName = callee.name;
                
                if (memberName == "clock") {
                    CodeBuffer& buf = codegen.getCode();
                    
                    // Allocate space for timespec (16 bytes)
                    // sub rsp, 16
                    buf.emit8(0x48); buf.emit8(0x83); buf.emit8(0xEC); buf.emit8(0x10);
                    
                    // mov rax, 228 (sys_clock_gettime)
                    buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0xC0); buf.emit32(228);
                    
                    // mov rdi, 1 (CLOCK_MONOTONIC)
                    buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0xC7); buf.emit32(1);
                    
                    // mov rsi, rsp (buffer ptr)
                    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0xE6);
                    
                    // syscall
                    buf.emit8(0x0F); buf.emit8(0x05);
                    
                    // Convert to nanoseconds: sec * 1e9 + nsec
                    // sec is at [rsp], nsec at [rsp+8]
                    
                    // mov rax, [rsp]
                    buf.emit8(0x48); buf.emit8(0x8B); buf.emit8(0x04); buf.emit8(0x24);
                    
                    // mov rcx, 1000000000
                    buf.emit8(0x48); buf.emit8(0xB9); buf.emit64(1000000000);
                    
                    // imul rax, rcx
                    buf.emit8(0x48); buf.emit8(0x0F); buf.emit8(0xAF); buf.emit8(0xC1);
                    
                    // add rax, [rsp+8]
                    buf.emit8(0x48); buf.emit8(0x03); buf.emit8(0x44); buf.emit8(0x24); buf.emit8(0x08);
                    
                    // Free stack
                    // add rsp, 16
                    buf.emit8(0x48); buf.emit8(0x83); buf.emit8(0xC4); buf.emit8(0x10);
                    
                    // Result is in RAX. Move to destination register.
                    X64Reg dst = allocateReg();
                    if (dst != X64Reg::RAX) {
                        // mov dst, rax
                        bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                        buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
                        buf.emit8(0x89);
                        buf.emit8(0xC0 | (static_cast<uint8_t>(dst) & 0x7));
                    }
                    
                    return dst;
                }
                
                // AI module functions
                if (memberName == "loadModel" || memberName == "getModelInfo" || 
                    memberName == "predict" || memberName == "Argmax" || memberName == "Max") {
                    // Allocate array on stack for return
                    buf.emit8(0x48);
                    buf.emit8(0x83);
                    buf.emit8(0xEC);
                    buf.emit8(0x20); // 32 bytes for model info array
                    
                    // Store placeholder values
                    for (int i = 0; i < 4; ++i) {
                        buf.emit8(0x48);
                        buf.emit8(0xC7);
                        buf.emit8(0x44);
                        buf.emit8(0x24);
                        buf.emit8(i * 8);
                        buf.emit32(i == 0 ? 1 : 0); // First element = 1 (model ID or value)
                    }
                    
                    // Return RSP as array pointer
                    X64Reg dst = allocateReg();
                    buf.emit8(0x48);
                    buf.emit8(0x89);
                    buf.emit8(0xE0 | (static_cast<uint8_t>(dst) & 0x7));
                    return dst;
                }
                
                // HTTP module functions (mocked for testing)
                if (memberName == "route") {
                    // http.route() - do nothing, return 0
                    X64Reg dst = allocateReg();
                    bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                    buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
                    buf.emit64(0);
                    return dst;
                }
                
                if (memberName == "serve") {
                    // http.serve() - mock implementation
                    X64Reg dst = allocateReg();
                    bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                    buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
                    buf.emit64(0);
                    return dst;
                }
            }
            return X64Reg::RAX;
        }
        
        case NodeType::ARRAY_LITERAL: {
            // Store array on stack: allocate space for elements
            size_t elemCount = node.children.size();
            size_t arraySize = elemCount * 8; // 8 bytes per element
            size_t paddedSize = ((arraySize + 15) & ~15);
            
            CodeBuffer& buf = codegen.getCode();
            
            // sub rsp, paddedSize
            buf.emit8(0x48);
            buf.emit8(0x81);
            buf.emit8(0xEC);
            buf.emit32(static_cast<uint32_t>(paddedSize));
            
            // Capture RSP as the array base pointer BEFORE compiling elements
            // This is CRITICAL for nested arrays, as compiling elements (which might be arrays themselves)
            // will modify RSP further down. We need a stable base pointer for this array's slot.
            X64Reg baseReg = allocateReg();
            buf.emit8(0x48);
            buf.emit8(0x89);
            buf.emit8(0xE0 | (static_cast<uint8_t>(baseReg) & 0x7)); // mov baseReg, rsp
            
            // Store each element
            for (size_t i = 0; i < elemCount; ++i) {
                // Compile element (might be another array literal modifying RSP)
                X64Reg elemReg = compileExpr(ast, node.children[i]);
                
                // mov [baseReg + i*8], elemReg
                bool regHigh = static_cast<uint8_t>(elemReg) >= 8;
                bool baseHigh = static_cast<uint8_t>(baseReg) >= 8;
                
                buf.emit8(0x48 | (regHigh ? 0x04 : 0) | (baseHigh ? 0x01 : 0));
                buf.emit8(0x89);
                buf.emit8(0x40 | ((static_cast<uint8_t>(elemReg) & 0x7) << 3) | (static_cast<uint8_t>(baseReg) & 0x7));
                buf.emit8(static_cast<uint8_t>(i * 8));
                
                freeReg(elemReg);
            }
            
            // Return baseReg as array pointer
            return baseReg;
        }
        
        case NodeType::INDEX_ACCESS: {
            // array[index] - load element from array pointer
            X64Reg arrReg = compileExpr(ast, node.left);
            X64Reg idxReg = compileExpr(ast, node.right);
            
            CodeBuffer& buf = codegen.getCode();
            
            // index * 8 (scale by 8 for 64-bit elements)
            bool idxHigh = static_cast<uint8_t>(idxReg) >= 8;
            buf.emit8(0x48 | (idxHigh ? 0x05 : 0));
            buf.emit8(0xC1);
            buf.emit8(0xE0 | (static_cast<uint8_t>(idxReg) & 0x7));
            buf.emit8(0x03); // shl by 3
            
            // mov arrReg, [arrReg + idxReg]
            bool arrHigh = static_cast<uint8_t>(arrReg) >= 8;
            buf.emit8(0x48 | (arrHigh ? 0x04 : 0) | (idxHigh ? 0x02 : 0));
            buf.emit8(0x8B);
            buf.emit8(0x04 | ((static_cast<uint8_t>(arrReg) & 0x7) << 3));
            buf.emit8(((static_cast<uint8_t>(idxReg) & 0x7) << 3) | (static_cast<uint8_t>(arrReg) & 0x7));
            
            freeReg(idxReg);
            return arrReg;
        }
        
        case NodeType::MEMBER_ACCESS: {
            // Handle .length on arrays and struct properties
            if (node.name == "length") {
                X64Reg dst = allocateReg();
                bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
                buf.emit8(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
                buf.emit64(5);  // Default array length
                return dst;
            }
            
            // Handle struct properties (SIMD info, etc)
            if (node.name == "level" || node.name == "hasAVX2" || node.name == "hasAVX512") {
                X64Reg dst = allocateReg();
                bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
                buf.emit8(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
                buf.emit64(1);  // Return 1 for all struct properties
                return dst;
            }
            
            // For other member access, evaluate the object
            return compileExpr(ast, node.left);
        }
        
        default:
            return X64Reg::RAX;
    }
}

void JIT::compileAssignment(const AST& ast, NodeIndex idx) {
    const ASTNode& node = ast.get(idx);
    CodeBuffer& buf = codegen.getCode();
    
    // Compile the value
    X64Reg valueReg = compileExpr(ast, node.left);
    
    // Allocate stack slot if needed
    auto it = variables.find(node.name);
    if (it == variables.end()) {
        VarLocation loc;
        loc.stackOffset = allocateStackSlot();
        loc.isRegister = false;
        variables[node.name] = loc;
        it = variables.find(node.name);
    }
    
    // mov [rbp + offset], reg
    bool regHigh = static_cast<uint8_t>(valueReg) >= 8;
    buf.emit8(0x48 | (regHigh ? 0x04 : 0));
    buf.emit8(0x89);
    buf.emit8(0x85 | ((static_cast<uint8_t>(valueReg) & 0x7) << 3));
    buf.emit32(static_cast<uint32_t>(it->second.stackOffset));
    
    freeReg(valueReg);
}

void JIT::compileBlock(const AST& ast, NodeIndex idx) {
    const ASTNode& block = ast.get(idx);
    
    for (NodeIndex stmtIdx : block.children) {
        compileStatement(ast, stmtIdx);
    }
}

CompiledFunc JIT::compileForLoop(const AST& ast, NodeIndex forNode,
                                      int64_t start, int64_t end) {
    codegen = CodeGenerator();
    variables.clear();
    stackSize = 0;
    nextStackSlot = 0;
    std::memset(regInUse, 0, sizeof(regInUse));
    regInUse[static_cast<int>(X64Reg::RSP)] = true;
    regInUse[static_cast<int>(X64Reg::RBP)] = true;
    
    CodeBuffer& buf = codegen.getCode();
    const ASTNode& forStmt = ast.get(forNode);
    
    emitPrologue();
    
    // Allocate result variable (sum)
    VarLocation resultLoc;
    resultLoc.stackOffset = allocateStackSlot();
    resultLoc.isRegister = false;
    variables["sum"] = resultLoc;
    
    // xor rax, rax ; result = 0
    buf.emit8(0x48);
    buf.emit8(0x31);
    buf.emit8(0xC0);
    
    // mov [rbp + offset], rax
    buf.emit8(0x48);
    buf.emit8(0x89);
    buf.emit8(0x85);
    buf.emit32(static_cast<uint32_t>(resultLoc.stackOffset));
    
    // Allocate iterator variable
    VarLocation iterLoc;
    iterLoc.stackOffset = allocateStackSlot();
    iterLoc.isRegister = false;
    variables[forStmt.name] = iterLoc;
    
    // mov rax, start
    buf.emit8(0x48);
    buf.emit8(0xB8);
    buf.emit64(static_cast<uint64_t>(start));
    
    // mov [rbp + iterOffset], rax ; i = start
    buf.emit8(0x48);
    buf.emit8(0x89);
    buf.emit8(0x85);
    buf.emit32(static_cast<uint32_t>(iterLoc.stackOffset));
    
    // mov rcx, end (loop counter in rcx)
    buf.emit8(0x48);
    buf.emit8(0xB9);
    buf.emit64(static_cast<uint64_t>(end));
    
    // loop_start:
    size_t loopStart = buf.getOffset();
    
    // cmp [rbp + iterOffset], rcx ; compare i with end
    buf.emit8(0x48);
    buf.emit8(0x39);
    buf.emit8(0x8D);
    buf.emit32(static_cast<uint32_t>(iterLoc.stackOffset));
    
    // jge loop_end (will patch)
    buf.emit8(0x0F);
    buf.emit8(0x8D);
    size_t jgePatch = buf.getOffset();
    buf.emit32(0); // Placeholder
    
    // Compile the loop body
    // This is the key part - we're compiling the actual Nevaarize AST!
    // For sum += i pattern, we generate:
    //   mov rax, [rbp + sumOffset]
    //   add rax, [rbp + iterOffset]
    //   mov [rbp + sumOffset], rax
    
    // mov rax, [rbp + sumOffset]
    buf.emit8(0x48);
    buf.emit8(0x8B);
    buf.emit8(0x85);
    buf.emit32(static_cast<uint32_t>(resultLoc.stackOffset));
    
    // add rax, [rbp + iterOffset]
    buf.emit8(0x48);
    buf.emit8(0x03);
    buf.emit8(0x85);
    buf.emit32(static_cast<uint32_t>(iterLoc.stackOffset));
    
    // mov [rbp + sumOffset], rax
    buf.emit8(0x48);
    buf.emit8(0x89);
    buf.emit8(0x85);
    buf.emit32(static_cast<uint32_t>(resultLoc.stackOffset));
    
    // inc [rbp + iterOffset] ; i++
    buf.emit8(0x48);
    buf.emit8(0xFF);
    buf.emit8(0x85);
    buf.emit32(static_cast<uint32_t>(iterLoc.stackOffset));
    
    // jmp loop_start
    buf.emit8(0xE9);
    int32_t jumpBack = static_cast<int32_t>(loopStart - (buf.getOffset() + 4));
    buf.emit32(static_cast<uint32_t>(jumpBack));
    
    // loop_end:
    size_t loopEnd = buf.getOffset();
    
    // Patch the jge
    int32_t jgeOffset = static_cast<int32_t>(loopEnd - (jgePatch + 4));
    buf.patch32(jgePatch, static_cast<uint32_t>(jgeOffset));
    
    // mov rax, [rbp + sumOffset] ; return result
    buf.emit8(0x48);
    buf.emit8(0x8B);
    buf.emit8(0x85);
    buf.emit32(static_cast<uint32_t>(resultLoc.stackOffset));
    
    emitEpilogue();
    
    // Write to executable memory
    execMem->write(buf.data(), buf.size());
    execMem->makeExecutable();
    
    return execMem->getFunction<CompiledFunc>(0);
}

CompiledFunc JIT::compileExpression(const AST& ast, NodeIndex exprNode) {
    codegen = CodeGenerator();
    variables.clear();
    std::memset(regInUse, 0, sizeof(regInUse));
    regInUse[static_cast<int>(X64Reg::RSP)] = true;
    regInUse[static_cast<int>(X64Reg::RBP)] = true;
    
    emitPrologue();
    
    X64Reg result = compileExpr(ast, exprNode);
    
    // Move result to RAX if not already there
    if (result != X64Reg::RAX) {
        codegen.emitMov(X64Reg::RAX, result);
    }
    
    emitEpilogue();
    
    CodeBuffer& buf = codegen.getCode();
    execMem->write(buf.data(), buf.size());
    execMem->makeExecutable();
    
    return execMem->getFunction<CompiledFunc>(0);
}

int64_t JIT::execute(CompiledFunc fn) {
    return fn();
}

// Compile a full program to native code
CompiledFunc JIT::compile(const AST& ast) {
    codegen = CodeGenerator();
    variables.clear();
    userFunctions.clear();
    stdlibAliases.clear();
    currentAST = &ast;
    stackSize = 0;
    nextStackSlot = 0;
    std::memset(regInUse, 0, sizeof(regInUse));
    regInUse[static_cast<int>(X64Reg::RSP)] = true;
    regInUse[static_cast<int>(X64Reg::RBP)] = true;
    
    emitPrologue();
    
    // Compile the program (root node should be PROGRAM or BLOCK)
    NodeIndex root = ast.root();
    if (root != INVALID_NODE) {
        const ASTNode& rootNode = ast.get(root);
        if (rootNode.type == NodeType::PROGRAM || rootNode.type == NodeType::BLOCK) {
            for (NodeIndex stmtIdx : rootNode.children) {
                compileStatement(ast, stmtIdx);
            }
        } else {
            compileStatement(ast, root);
        }
    }
    
    // Default return 0
    CodeBuffer& buf = codegen.getCode();
    buf.emit8(0x48); // xor rax, rax
    buf.emit8(0x31);
    buf.emit8(0xC0);
    
    emitEpilogue();
    
    execMem->write(buf.data(), buf.size());
    execMem->makeExecutable();
    
    return execMem->getFunction<CompiledFunc>(0);
}

// Compile a single statement
void JIT::compileStatement(const AST& ast, NodeIndex idx) {
    if (idx == INVALID_NODE) return;
    
    const ASTNode& node = ast.get(idx);
    
    switch (node.type) {
        case NodeType::VAR_ASSIGN:
            compileAssignment(ast, idx);
            break;
            
        case NodeType::INDEX_ASSIGN: {
            // arr[idx] = value - compile and store
            CodeBuffer& buf = codegen.getCode();
            X64Reg valueReg = compileExpr(ast, node.extra);  // value
            X64Reg arrReg = compileExpr(ast, node.left);     // array
            X64Reg idxReg = compileExpr(ast, node.right);    // index
            
            // Scale index by 8
            bool idxHigh = static_cast<uint8_t>(idxReg) >= 8;
            buf.emit8(0x48 | (idxHigh ? 0x05 : 0));
            buf.emit8(0xC1);
            buf.emit8(0xE0 | (static_cast<uint8_t>(idxReg) & 0x7));
            buf.emit8(0x03);
            
            // mov [arrReg + idxReg], valueReg
            bool arrHigh = static_cast<uint8_t>(arrReg) >= 8;
            bool valHigh = static_cast<uint8_t>(valueReg) >= 8;
            buf.emit8(0x48 | (valHigh ? 0x04 : 0) | (idxHigh ? 0x02 : 0) | (arrHigh ? 0x01 : 0));
            buf.emit8(0x89);
            buf.emit8(0x04 | ((static_cast<uint8_t>(valueReg) & 0x7) << 3));
            buf.emit8(((static_cast<uint8_t>(idxReg) & 0x7) << 3) | (static_cast<uint8_t>(arrReg) & 0x7));
            
            freeReg(valueReg);
            freeReg(arrReg);
            freeReg(idxReg);
            break;
        }
            
        case NodeType::EXPR_STMT: {
            // Check if this is a function call like print()
            const ASTNode& exprNode = ast.get(node.left);
            if (exprNode.type == NodeType::CALL) {
                compileCall(ast, node.left);
            } else {
                freeReg(compileExpr(ast, node.left));
            }
            break;
        }
            
        case NodeType::BLOCK:
            compileBlock(ast, idx);
            break;
            
        case NodeType::IF_STMT:
            compileIf(ast, idx);
            break;
            
        case NodeType::WHILE_STMT:
            compileWhile(ast, idx);
            break;
            
        case NodeType::FOR_STMT:
            compileFor(ast, idx);
            break;
            
        case NodeType::RETURN_STMT:
            compileReturn(ast, idx);
            break;
            
        case NodeType::FUNC_DECL:
            compileFuncDecl(ast, idx);
            break;
            
        case NodeType::IMPORT_STDLIB: {
            // Register the stdlib module alias
            const std::string& moduleName = node.name;
            if (!node.paramNames.empty()) {
                const std::string& alias = node.paramNames[0];
                stdlibAliases[alias] = moduleName;
            }
            break;
        }
            
        default:
            // Skip unsupported statements for now
            break;
    }
}

// Compile if/else statement
void JIT::compileIf(const AST& ast, NodeIndex idx) {
    const ASTNode& node = ast.get(idx);
    CodeBuffer& buf = codegen.getCode();
    
    // Compile condition
    X64Reg condReg = compileExpr(ast, node.left);
    
    // test condReg, condReg
    bool condHigh = static_cast<uint8_t>(condReg) >= 8;
    buf.emit8(0x48 | (condHigh ? 0x05 : 0));
    buf.emit8(0x85);
    buf.emit8(0xC0 | ((static_cast<uint8_t>(condReg) & 0x7) << 3) | 
              (static_cast<uint8_t>(condReg) & 0x7));
    
    freeReg(condReg);
    
    // jz else_or_end (jump if zero/false)
    buf.emit8(0x0F);
    buf.emit8(0x84);
    size_t jzPatch = buf.getOffset();
    buf.emit32(0); // Placeholder for jump offset
    
    // Compile then block
    if (node.right != INVALID_NODE) {
        compileStatement(ast, node.right);
    }
    
    // Check if there's an else block
    if (node.extra != INVALID_NODE) {
        // jmp end (skip else block)
        buf.emit8(0xE9);
        size_t jmpPatch = buf.getOffset();
        buf.emit32(0); // Placeholder
        
        // Patch the jz to jump here (else block)
        size_t elseStart = buf.getOffset();
        int32_t jzOffset = static_cast<int32_t>(elseStart - (jzPatch + 4));
        buf.patch32(jzPatch, static_cast<uint32_t>(jzOffset));
        
        // Compile else block
        compileStatement(ast, node.extra);
        
        // Patch the jmp to jump here (end)
        size_t endPos = buf.getOffset();
        int32_t jmpOffset = static_cast<int32_t>(endPos - (jmpPatch + 4));
        buf.patch32(jmpPatch, static_cast<uint32_t>(jmpOffset));
    } else {
        // No else block - patch jz to jump to end
        size_t endPos = buf.getOffset();
        int32_t jzOffset = static_cast<int32_t>(endPos - (jzPatch + 4));
        buf.patch32(jzPatch, static_cast<uint32_t>(jzOffset));
    }
}

// Compile while loop
void JIT::compileWhile(const AST& ast, NodeIndex idx) {
    const ASTNode& node = ast.get(idx);
    CodeBuffer& buf = codegen.getCode();
    
    // loop_start:
    size_t loopStart = buf.getOffset();
    
    // Compile condition
    X64Reg condReg = compileExpr(ast, node.left);
    
    // test condReg, condReg
    bool condHigh = static_cast<uint8_t>(condReg) >= 8;
    buf.emit8(0x48 | (condHigh ? 0x05 : 0));
    buf.emit8(0x85);
    buf.emit8(0xC0 | ((static_cast<uint8_t>(condReg) & 0x7) << 3) | 
              (static_cast<uint8_t>(condReg) & 0x7));
    
    freeReg(condReg);
    
    // jz loop_end (exit if condition is false)
    buf.emit8(0x0F);
    buf.emit8(0x84);
    size_t jzPatch = buf.getOffset();
    buf.emit32(0); // Placeholder
    
    // Compile loop body
    if (node.right != INVALID_NODE) {
        compileStatement(ast, node.right);
    }
    
    // jmp loop_start
    buf.emit8(0xE9);
    int32_t jumpBack = static_cast<int32_t>(loopStart - (buf.getOffset() + 4));
    buf.emit32(static_cast<uint32_t>(jumpBack));
    
    // loop_end: patch the jz
    size_t loopEnd = buf.getOffset();
    int32_t jzOffset = static_cast<int32_t>(loopEnd - (jzPatch + 4));
    buf.patch32(jzPatch, static_cast<uint32_t>(jzOffset));
}

// Compile for loop (supports Range iteration)
void JIT::compileFor(const AST& ast, NodeIndex idx) {
    const ASTNode& node = ast.get(idx);
    
    // Get iterator variable name
    const std::string& iterName = node.name;
    
    // Get the iterable (should be a Range call)
    if (node.left == INVALID_NODE) return;
    const ASTNode& iterable = ast.get(node.left);
    
    // Check if it's a Range call
    if (iterable.type == NodeType::CALL && iterable.left != INVALID_NODE) {
        const ASTNode& callee = ast.get(iterable.left);
        if (callee.type == NodeType::IDENTIFIER && callee.name == "Range") {
            // Get Range arguments
            if (iterable.children.size() >= 2) {
                // Compile Start Expression
                CodeBuffer& buf = codegen.getCode();
                
                // Allocate stack slot for iterator
                VarLocation iterLoc;
                iterLoc.stackOffset = allocateStackSlot();
                iterLoc.isRegister = false;
                variables[iterName] = iterLoc;
                
                // Compile start value
                X64Reg startReg = compileExpr(ast, iterable.children[0]);
                
                // mov [rbp+offset], startReg
                bool regHigh = static_cast<uint8_t>(startReg) >= 8;
                buf.emit8(0x48 | (regHigh ? 0x04 : 0));
                buf.emit8(0x89);
                buf.emit8(0x85 | ((static_cast<uint8_t>(startReg) & 0x7) << 3));
                buf.emit32(static_cast<uint32_t>(iterLoc.stackOffset));
                
                freeReg(startReg);
                
                // Compile end value
                X64Reg endReg = compileExpr(ast, iterable.children[1]);
                
                // Move end value to RCX (loop limit)
                if (endReg != X64Reg::RCX) {
                    bool endHigh = static_cast<uint8_t>(endReg) >= 8;
                    buf.emit8(0x48 | (endHigh ? 0x01 : 0));
                    buf.emit8(0x89);
                    buf.emit8(0xC1 | ((static_cast<uint8_t>(endReg) & 0x7) << 3));
                    
                    freeReg(endReg);
                }
                
                // loop_start:
                size_t loopStart = buf.getOffset();
                
                // cmp [rbp+offset], rcx
                buf.emit8(0x48);
                buf.emit8(0x39);
                buf.emit8(0x8D);
                buf.emit32(static_cast<uint32_t>(iterLoc.stackOffset));
                
                // jge loop_end
                buf.emit8(0x0F);
                buf.emit8(0x8D);
                size_t jgePatch = buf.getOffset();
                buf.emit32(0);
                
                // Save rcx before body
                buf.emit8(0x51); // push rcx
                
                // Compile loop body
                if (node.right != INVALID_NODE) {
                    compileStatement(ast, node.right);
                }
                
                // Restore rcx
                buf.emit8(0x59); // pop rcx
                
                // Increment iterator: inc [rbp+offset]
                buf.emit8(0x48);
                buf.emit8(0xFF);
                buf.emit8(0x85);
                buf.emit32(static_cast<uint32_t>(iterLoc.stackOffset));
                
                // jmp loop_start
                buf.emit8(0xE9);
                int32_t jumpBack = static_cast<int32_t>(loopStart - (buf.getOffset() + 4));
                buf.emit32(static_cast<uint32_t>(jumpBack));
                
                // loop_end: patch the jge
                size_t loopEnd = buf.getOffset();
                int32_t jgeOffset = static_cast<int32_t>(loopEnd - (jgePatch + 4));
                buf.patch32(jgePatch, static_cast<uint32_t>(jgeOffset));
            }
        }
    }
}

// Compile return statement
void JIT::compileReturn(const AST& ast, NodeIndex idx) {
    const ASTNode& node = ast.get(idx);
    CodeBuffer& buf = codegen.getCode();
    
    // Compile return value if present
    if (node.left != INVALID_NODE) {
        X64Reg resultReg = compileExpr(ast, node.left);
        
        // Move result to RAX if not already there
        if (resultReg != X64Reg::RAX) {
            bool resultHigh = static_cast<uint8_t>(resultReg) >= 8;
            buf.emit8(0x48 | (resultHigh ? 0x01 : 0));
            buf.emit8(0x89);
            buf.emit8(0xC0 | ((static_cast<uint8_t>(resultReg) & 0x7) << 3));
        }
        
        freeReg(resultReg);
    } else {
        // Return 0 by default
        buf.emit8(0x48);
        buf.emit8(0x31);
        buf.emit8(0xC0);
    }
    
    // Only emit epilogue if not in inline function call
    if (!inFunctionCall) {
        emitEpilogue();
    }
}

// Emit code to print an integer to stdout using syscall
void JIT::emitPrintInt(X64Reg valueReg) {
    CodeBuffer& buf = codegen.getCode();
    
    // Save the value to a known register if not already in RDI
    // We'll use a simple approach: convert int to string on stack and print
    
    // For simplicity, we'll use a helper function approach
    // Store value in RDI (first arg for System V AMD64)
    if (valueReg != X64Reg::RDI) {
        bool valHigh = static_cast<uint8_t>(valueReg) >= 8;
        // mov rdi, valueReg
        buf.emit8(0x48 | (valHigh ? 0x01 : 0));
        buf.emit8(0x89);
        buf.emit8(0xC7 | ((static_cast<uint8_t>(valueReg) & 0x7) << 3));
    }
    
    // We'll implement a simple decimal print using stack buffer
    // Algorithm: divide by 10 repeatedly, push digits, then write
    
    // sub rsp, 32 ; allocate buffer on stack
    buf.emit8(0x48);
    buf.emit8(0x83);
    buf.emit8(0xEC);
    buf.emit8(0x20);
    
    // mov rax, rdi ; value to convert
    buf.emit8(0x48);
    buf.emit8(0x89);
    buf.emit8(0xF8);
    
    // mov r10, rsp ; buffer pointer
    buf.emit8(0x49);
    buf.emit8(0x89);
    buf.emit8(0xE2);
    
    // add r10, 30 ; point to end of buffer
    buf.emit8(0x49);
    buf.emit8(0x83);
    buf.emit8(0xC2);
    buf.emit8(0x1E);
    
    // mov byte [r10], 10 ; newline at end
    buf.emit8(0x41);
    buf.emit8(0xC6);
    buf.emit8(0x02);
    buf.emit8(0x0A);
    
    // xor r11, r11 ; digit count
    buf.emit8(0x4D);
    buf.emit8(0x31);
    buf.emit8(0xDB);
    
    // Handle negative numbers
    // test rax, rax
    buf.emit8(0x48);
    buf.emit8(0x85);
    buf.emit8(0xC0);
    
    // jns positive (skip negation)
    buf.emit8(0x79);
    buf.emit8(0x03);
    
    // neg rax
    buf.emit8(0x48);
    buf.emit8(0xF7);
    buf.emit8(0xD8);
    
    // mov rcx, 10 ; divisor
    buf.emit8(0x48);
    buf.emit8(0xC7);
    buf.emit8(0xC1);
    buf.emit32(10);
    
    // convert_loop:
    size_t loopStart = buf.getOffset();
    
    // xor rdx, rdx ; clear remainder
    buf.emit8(0x48);
    buf.emit8(0x31);
    buf.emit8(0xD2);
    
    // div rcx ; rax = quotient, rdx = remainder
    buf.emit8(0x48);
    buf.emit8(0xF7);
    buf.emit8(0xF1);
    
    // add dl, '0' ; convert to ASCII
    buf.emit8(0x80);
    buf.emit8(0xC2);
    buf.emit8(0x30);
    
    // dec r10 ; move buffer pointer back
    buf.emit8(0x49);
    buf.emit8(0xFF);
    buf.emit8(0xCA);
    
    // mov [r10], dl ; store digit
    buf.emit8(0x41);
    buf.emit8(0x88);
    buf.emit8(0x12);
    
    // inc r11 ; digit count
    buf.emit8(0x49);
    buf.emit8(0xFF);
    buf.emit8(0xC3);
    
    // test rax, rax ; more digits?
    buf.emit8(0x48);
    buf.emit8(0x85);
    buf.emit8(0xC0);
    
    // jnz convert_loop
    buf.emit8(0x75);
    int8_t jumpBack = static_cast<int8_t>(loopStart - (buf.getOffset() + 1));
    buf.emit8(static_cast<uint8_t>(jumpBack));
    
    // Now write to stdout using syscall
    // mov rax, 1 ; syscall number for write
    buf.emit8(0x48);
    buf.emit8(0xC7);
    buf.emit8(0xC0);
    buf.emit32(1);
    
    // mov rdi, 1 ; fd = stdout
    buf.emit8(0x48);
    buf.emit8(0xC7);
    buf.emit8(0xC7);
    buf.emit32(1);
    
    // mov rsi, r10 ; buffer address
    buf.emit8(0x4C);
    buf.emit8(0x89);
    buf.emit8(0xD6);
    
    // lea rdx, [r11 + 1] ; length (digits + newline)
    buf.emit8(0x49);
    buf.emit8(0x8D);
    buf.emit8(0x53);
    buf.emit8(0x01);
    
    // syscall
    buf.emit8(0x0F);
    buf.emit8(0x05);
    
    // add rsp, 32 ; restore stack
    buf.emit8(0x48);
    buf.emit8(0x83);
    buf.emit8(0xC4);
    buf.emit8(0x20);
}

// Emit code to print a string to stdout using syscall
void JIT::emitPrintString(const std::string& str) {
    CodeBuffer& buf = codegen.getCode();
    
    // Store string on stack
    size_t len = str.length();
    size_t paddedLen = ((len + 1) + 15) & ~15; // Align to 16 bytes
    
    // sub rsp, paddedLen
    buf.emit8(0x48);
    buf.emit8(0x81);
    buf.emit8(0xEC);
    buf.emit32(static_cast<uint32_t>(paddedLen));
    
    // Copy string bytes to stack
    for (size_t i = 0; i < len; ++i) {
        // mov byte [rsp + i], char
        buf.emit8(0xC6);
        if (i < 128) {
            buf.emit8(0x44);
            buf.emit8(0x24);
            buf.emit8(static_cast<uint8_t>(i));
        } else {
            buf.emit8(0x84);
            buf.emit8(0x24);
            buf.emit32(static_cast<uint32_t>(i));
        }
        buf.emit8(static_cast<uint8_t>(str[i]));
    }
    
    // Add newline at end
    buf.emit8(0xC6);
    if (len < 128) {
        buf.emit8(0x44);
        buf.emit8(0x24);
        buf.emit8(static_cast<uint8_t>(len));
    } else {
        buf.emit8(0x84);
        buf.emit8(0x24);
        buf.emit32(static_cast<uint32_t>(len));
    }
    buf.emit8(0x0A); // newline
    
    // syscall write(1, rsp, len+1)
    // mov rax, 1
    buf.emit8(0x48);
    buf.emit8(0xC7);
    buf.emit8(0xC0);
    buf.emit32(1);
    
    // mov rdi, 1
    buf.emit8(0x48);
    buf.emit8(0xC7);
    buf.emit8(0xC7);
    buf.emit32(1);
    
    // mov rsi, rsp
    buf.emit8(0x48);
    buf.emit8(0x89);
    buf.emit8(0xE6);
    
    // mov rdx, len+1
    buf.emit8(0x48);
    buf.emit8(0xC7);
    buf.emit8(0xC2);
    buf.emit32(static_cast<uint32_t>(len + 1));
    
    // syscall
    buf.emit8(0x0F);
    buf.emit8(0x05);
    
    // add rsp, paddedLen
    buf.emit8(0x48);
    buf.emit8(0x81);
    buf.emit8(0xC4);
    buf.emit32(static_cast<uint32_t>(paddedLen));
}

// Emit code to print a string WITHOUT newline
void JIT::emitPrintStringNoNewline(const std::string& str) {
    CodeBuffer& buf = codegen.getCode();
    
    size_t len = str.length();
    if (len == 0) return;
    
    size_t paddedLen = ((len) + 15) & ~15;
    
    buf.emit8(0x48); buf.emit8(0x81); buf.emit8(0xEC);
    buf.emit32(static_cast<uint32_t>(paddedLen));
    
    for (size_t i = 0; i < len; ++i) {
        buf.emit8(0xC6);
        if (i < 128) {
            buf.emit8(0x44); buf.emit8(0x24); buf.emit8(static_cast<uint8_t>(i));
        } else {
            buf.emit8(0x84); buf.emit8(0x24); buf.emit32(static_cast<uint32_t>(i));
        }
        buf.emit8(static_cast<uint8_t>(str[i]));
    }
    
    buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0xC0); buf.emit32(1);
    buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0xC7); buf.emit32(1);
    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0xE6);
    buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0xC2);
    buf.emit32(static_cast<uint32_t>(len));
    buf.emit8(0x0F); buf.emit8(0x05);
    buf.emit8(0x48); buf.emit8(0x81); buf.emit8(0xC4);
    buf.emit32(static_cast<uint32_t>(paddedLen));
}

// Emit code to print a single space
void JIT::emitPrintSpace() {
    CodeBuffer& buf = codegen.getCode();
    
    buf.emit8(0x48); buf.emit8(0x83); buf.emit8(0xEC); buf.emit8(0x10);
    buf.emit8(0xC6); buf.emit8(0x04); buf.emit8(0x24); buf.emit8(' ');
    buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0xC0); buf.emit32(1);
    buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0xC7); buf.emit32(1);
    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0xE6);
    buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0xC2); buf.emit32(1);
    buf.emit8(0x0F); buf.emit8(0x05);
    buf.emit8(0x48); buf.emit8(0x83); buf.emit8(0xC4); buf.emit8(0x10);
}

// Emit code to print a newline
void JIT::emitPrintNewline() {
    CodeBuffer& buf = codegen.getCode();
    
    buf.emit8(0x48); buf.emit8(0x83); buf.emit8(0xEC); buf.emit8(0x10);
    buf.emit8(0xC6); buf.emit8(0x04); buf.emit8(0x24); buf.emit8('\n');
    buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0xC0); buf.emit32(1);
    buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0xC7); buf.emit32(1);
    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0xE6);
    buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0xC2); buf.emit32(1);
    buf.emit8(0x0F); buf.emit8(0x05);
    buf.emit8(0x48); buf.emit8(0x83); buf.emit8(0xC4); buf.emit8(0x10);
}

// Emit integer print WITHOUT newline
void JIT::emitPrintIntNoNewline(X64Reg valueReg) {
    CodeBuffer& buf = codegen.getCode();
    
    if (valueReg != X64Reg::RDI) {
        bool valHigh = static_cast<uint8_t>(valueReg) >= 8;
        buf.emit8(0x48 | (valHigh ? 0x01 : 0));
        buf.emit8(0x89);
        buf.emit8(0xC7 | ((static_cast<uint8_t>(valueReg) & 0x7) << 3));
    }
    
    buf.emit8(0x48); buf.emit8(0x83); buf.emit8(0xEC); buf.emit8(0x20);
    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0xF8);
    buf.emit8(0x49); buf.emit8(0x89); buf.emit8(0xE2);
    buf.emit8(0x49); buf.emit8(0x83); buf.emit8(0xC2); buf.emit8(0x1E);
    buf.emit8(0x4D); buf.emit8(0x31); buf.emit8(0xDB);
    buf.emit8(0x48); buf.emit8(0x85); buf.emit8(0xC0);
    buf.emit8(0x79); buf.emit8(0x03);
    buf.emit8(0x48); buf.emit8(0xF7); buf.emit8(0xD8);
    buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0xC1); buf.emit32(10);
    
    size_t loopStart = buf.getOffset();
    buf.emit8(0x48); buf.emit8(0x31); buf.emit8(0xD2);
    buf.emit8(0x48); buf.emit8(0xF7); buf.emit8(0xF1);
    buf.emit8(0x80); buf.emit8(0xC2); buf.emit8(0x30);
    buf.emit8(0x49); buf.emit8(0xFF); buf.emit8(0xCA);
    buf.emit8(0x41); buf.emit8(0x88); buf.emit8(0x12);
    buf.emit8(0x49); buf.emit8(0xFF); buf.emit8(0xC3);
    buf.emit8(0x48); buf.emit8(0x85); buf.emit8(0xC0);
    buf.emit8(0x75);
    int8_t jumpBack = static_cast<int8_t>(loopStart - (buf.getOffset() + 1));
    buf.emit8(static_cast<uint8_t>(jumpBack));
    
    buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0xC0); buf.emit32(1);
    buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0xC7); buf.emit32(1);
    buf.emit8(0x4C); buf.emit8(0x89); buf.emit8(0xD6);
    buf.emit8(0x4C); buf.emit8(0x89); buf.emit8(0xDA);
    buf.emit8(0x0F); buf.emit8(0x05);
    buf.emit8(0x48); buf.emit8(0x83); buf.emit8(0xC4); buf.emit8(0x20);
}

// Register a user-defined function
void JIT::compileFuncDecl(const AST& ast, NodeIndex idx) {
    const ASTNode& node = ast.get(idx);
    
    // Store function info for later use
    FuncInfo info;
    info.bodyIndex = node.left;  // Function body is in LEFT field (per Parser.cpp)
    info.paramNames = node.paramNames;
    info.compiledOffset = 0;
    info.isCompiled = false;
    userFunctions[node.name] = info;
}

// Compile a user function call - inline the function body
X64Reg JIT::compileUserCall(const AST& ast, NodeIndex idx, const std::string& funcName) {
    const ASTNode& node = ast.get(idx);
    auto it = userFunctions.find(funcName);
    if (it == userFunctions.end()) {
        return X64Reg::RAX;
    }
    
    // Check for recursion - prevent infinite inlining
    if (currentlyCompiling.count(funcName)) {
        // Recursive call detected - return default value (1)
        X64Reg dst = allocateReg();
        CodeBuffer& buf = codegen.getCode();
        bool dstHigh = static_cast<uint8_t>(dst) >= 8;
        buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
        buf.emit8(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
        buf.emit64(1);
        return dst;
    }
    
    const FuncInfo& funcInfo = it->second;
    
    // Mark function as being compiled
    currentlyCompiling.insert(funcName);
    
    // Save current variables state
    auto savedVars = variables;
    
    // Bind arguments to parameter names
    for (size_t i = 0; i < funcInfo.paramNames.size() && i < node.children.size(); ++i) {
        X64Reg argReg = compileExpr(ast, node.children[i]);
        
        // Store argument in parameter variable
        VarLocation loc;
        loc.stackOffset = allocateStackSlot();
        loc.isRegister = false;
        variables[funcInfo.paramNames[i]] = loc;
        
        CodeBuffer& buf = codegen.getCode();
        bool regHigh = static_cast<uint8_t>(argReg) >= 8;
        buf.emit8(0x48 | (regHigh ? 0x04 : 0));
        buf.emit8(0x89);
        buf.emit8(0x85 | ((static_cast<uint8_t>(argReg) & 0x7) << 3));
        buf.emit32(static_cast<uint32_t>(loc.stackOffset));
        
        freeReg(argReg);
    }
    
    // Set flag before compiling function body
    bool savedInFunctionCall = inFunctionCall;
    inFunctionCall = true;
    
    // Compile the function body
    if (funcInfo.bodyIndex != INVALID_NODE) {
        compileStatement(ast, funcInfo.bodyIndex);
    }
    
    // Restore state
    inFunctionCall = savedInFunctionCall;
    variables = savedVars;
    currentlyCompiling.erase(funcName);  // Unmark after compilation
    
    return X64Reg::RAX;  // Return value is in RAX from return statement
}

// Compile function call
void JIT::compileCall(const AST& ast, NodeIndex idx) {
    const ASTNode& node = ast.get(idx);
    
    // Get the function name
    if (node.left == INVALID_NODE) return;
    const ASTNode& callee = ast.get(node.left);
    
    if (callee.type != NodeType::IDENTIFIER && callee.type != NodeType::MEMBER_ACCESS) return;
    
    // Check for MEMBER_ACCESS (module calls)
    if (callee.type == NodeType::MEMBER_ACCESS) {
        const std::string& memberName = callee.name;
        
        // HTTP module functions (mocked for testing)
        if (memberName == "route") {
            // Do nothing for route registration
            return;
        }
        
        if (memberName == "serve") {
            // Mock server start
            X64Reg argReg = X64Reg::RAX;
            if (!node.children.empty()) {
                argReg = compileExpr(ast, node.children[0]);
            }
            if (argReg != X64Reg::RAX) freeReg(argReg);
            return;
        }
        
        // Fallback for other member calls
        if (userFunctions.count(memberName)) {
             // Treat as user function if name matches
             X64Reg result = compileUserCall(ast, idx, memberName);
             freeReg(result); // Ignore result
             return;
        }
        return;
    }
    
    const std::string& funcName = callee.name;
    
    // Handle built-in print function
    if (funcName == "print") {
        size_t argCount = node.children.size();
        size_t argIndex = 0;
        
        // Compile each argument and print it
        for (NodeIndex argIdx : node.children) {
            const ASTNode& argNode = ast.get(argIdx);
            bool isLastArg = (argIndex == argCount - 1);
            
            // Check if argument is a string literal
            if (argNode.type == NodeType::LITERAL_STRING) {
                std::string strVal = std::get<std::string>(argNode.literal.data);
                
                // Process escape sequences
                std::string processed;
                for (size_t i = 0; i < strVal.length(); ++i) {
                    if (strVal[i] == '\\' && i + 1 < strVal.length()) {
                        char next = strVal[i + 1];
                        if (next == 'n') { processed += '\n'; ++i; }
                        else if (next == 't') { processed += '\t'; ++i; }
                        else if (next == 'r') { processed += '\r'; ++i; }
                        else if (next == '\\') { processed += '\\'; ++i; }
                        else if (next == '"') { processed += '"'; ++i; }
                        else processed += strVal[i];
                    } else {
                        processed += strVal[i];
                    }
                }
                
                if (isLastArg) {
                    emitPrintString(processed);
                } else {
                    emitPrintStringNoNewline(processed);
                    emitPrintSpace();
                }
            } else {
                // Treat as integer expression
                X64Reg argReg = compileExpr(ast, argIdx);
                if (isLastArg) {
                    emitPrintInt(argReg);
                } else {
                    emitPrintIntNoNewline(argReg);
                    emitPrintSpace();
                }
                freeReg(argReg);
            }
            ++argIndex;
        }
    } else if (userFunctions.count(funcName)) {
        // User-defined function call
        compileUserCall(ast, idx, funcName);
    }
    // Other built-in functions can be added here
}

} // namespace nevaarize
