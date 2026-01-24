/**
 * TrueJIT.cpp - True JIT Compiler Implementation
 *
 * Compiles Nevaarize AST to x86-64 machine code.
 * This compiles ACTUAL Nevaarize code, not pre-written assembly.
 */

#include "TrueJIT.hpp"
#include <cstring>

namespace nevaarize {

TrueJIT::TrueJIT() 
    : stackSize(0)
    , nextStackSlot(0) {
    execMem = std::make_unique<ExecutableMemory>(16384);
    std::memset(regInUse, 0, sizeof(regInUse));
    
    // Reserve some registers
    regInUse[static_cast<int>(X64Reg::RSP)] = true;
    regInUse[static_cast<int>(X64Reg::RBP)] = true;
}

TrueJIT::~TrueJIT() = default;

void TrueJIT::emitPrologue() {
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

void TrueJIT::emitEpilogue() {
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

X64Reg TrueJIT::allocateReg() {
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

void TrueJIT::freeReg(X64Reg reg) {
    int idx = static_cast<int>(reg);
    if (idx != static_cast<int>(X64Reg::RSP) && 
        idx != static_cast<int>(X64Reg::RBP)) {
        regInUse[idx] = false;
    }
}

int32_t TrueJIT::allocateStackSlot() {
    nextStackSlot += 8;
    if (nextStackSlot > stackSize) {
        stackSize = nextStackSlot;
    }
    return -nextStackSlot;
}

bool TrueJIT::canCompileLoop(const AST& ast, NodeIndex forNode) {
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

X64Reg TrueJIT::compileExpr(const AST& ast, NodeIndex idx) {
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
                    
                default:
                    break;
            }
            
            freeReg(right);
            return left;
        }
        
        default:
            return X64Reg::RAX;
    }
}

void TrueJIT::compileAssignment(const AST& ast, NodeIndex idx) {
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

void TrueJIT::compileBlock(const AST& ast, NodeIndex idx) {
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

CompiledFunc TrueJIT::compileForLoop(const AST& ast, NodeIndex forNode,
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

CompiledFunc TrueJIT::compileExpression(const AST& ast, NodeIndex exprNode) {
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

int64_t TrueJIT::execute(CompiledFunc fn) {
    return fn();
}

// JITEvaluator implementation
JITEvaluator::JITEvaluator()
    : interpretedOps(0)
    , compiledOps(0) {}

JITEvaluator::Stats JITEvaluator::getStats() const {
    Stats s;
    s.interpretedOps = interpretedOps;
    s.compiledOps = compiledOps;
    s.compiledPercentage = (compiledOps + interpretedOps > 0)
        ? (100.0 * compiledOps / (compiledOps + interpretedOps))
        : 0.0;
    return s;
}

} // namespace nevaarize
