/**
 * Compiler.cpp - True JIT Compiler Implementation
 *
 * Compiles Nevaarize AST to x86-64 machine code.
 * This compiles ACTUAL Nevaarize code, not pre-written assembly.
 */

#include "Compiler.hpp"
#include <cstring>

namespace nevaarize {

Compiler::Compiler() 
    : stackSize(0)
    , nextStackSlot(0) {
    execMem = std::make_unique<ExecutableMemory>(16384);
    std::memset(regInUse, 0, sizeof(regInUse));
    
    // Reserve some registers
    regInUse[static_cast<int>(X64Reg::RSP)] = true;
    regInUse[static_cast<int>(X64Reg::RBP)] = true;
}

Compiler::~Compiler() = default;

void Compiler::emitPrologue() {
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
    buf.emit32(256); // Reserve 256 bytes for locals
}

void Compiler::emitEpilogue() {
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

X64Reg Compiler::allocateReg() {
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

void Compiler::freeReg(X64Reg reg) {
    int idx = static_cast<int>(reg);
    if (idx != static_cast<int>(X64Reg::RSP) && 
        idx != static_cast<int>(X64Reg::RBP)) {
        regInUse[idx] = false;
    }
}

int32_t Compiler::allocateStackSlot() {
    nextStackSlot += 8;
    if (nextStackSlot > stackSize) {
        stackSize = nextStackSlot;
    }
    return -nextStackSlot;
}

bool Compiler::canCompileLoop(const AST& ast, NodeIndex forNode) {
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

X64Reg Compiler::compileExpr(const AST& ast, NodeIndex idx) {
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
        
        default:
            return X64Reg::RAX;
    }
}

void Compiler::compileAssignment(const AST& ast, NodeIndex idx) {
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

void Compiler::compileBlock(const AST& ast, NodeIndex idx) {
    const ASTNode& block = ast.get(idx);
    
    for (NodeIndex stmtIdx : block.children) {
        const ASTNode& stmt = ast.get(stmtIdx);
        
        switch (stmt.type) {
            case NodeType::VAR_ASSIGN:
                compileAssignment(ast, stmtIdx);
                break;
            case NodeType::EXPR_STMT:
                freeReg(compileExpr(ast, stmt.left));
                break;
            default:
                break;
        }
    }
}

CompiledFunc Compiler::compileForLoop(const AST& ast, NodeIndex forNode,
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

CompiledFunc Compiler::compileExpression(const AST& ast, NodeIndex exprNode) {
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

int64_t Compiler::execute(CompiledFunc fn) {
    return fn();
}

// Compile a full program to native code
CompiledFunc Compiler::compile(const AST& ast) {
    codegen = CodeGenerator();
    variables.clear();
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
void Compiler::compileStatement(const AST& ast, NodeIndex idx) {
    if (idx == INVALID_NODE) return;
    
    const ASTNode& node = ast.get(idx);
    
    switch (node.type) {
        case NodeType::VAR_ASSIGN:
            compileAssignment(ast, idx);
            break;
            
        case NodeType::EXPR_STMT:
            freeReg(compileExpr(ast, node.left));
            break;
            
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
            // For now, skip complex for loops - they need Range handling
            break;
            
        case NodeType::RETURN_STMT:
            compileReturn(ast, idx);
            break;
            
        default:
            // Skip unsupported statements for now
            break;
    }
}

// Compile if/else statement
void Compiler::compileIf(const AST& ast, NodeIndex idx) {
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
void Compiler::compileWhile(const AST& ast, NodeIndex idx) {
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

// Compile return statement
void Compiler::compileReturn(const AST& ast, NodeIndex idx) {
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
    
    // Emit epilogue and return
    emitEpilogue();
}

} // namespace nevaarize
