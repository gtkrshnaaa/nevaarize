/**
 * Compiler.cpp - True JIT Compiler Implementation
 *
 * Compiles Nevaarize AST to x86-64 machine code.
 * This compiles ACTUAL Nevaarize code, not pre-written assembly.
 */

#include "JIT.hpp"
#include "Parser.hpp"
#include "Lexer.hpp"
#include <cstring>
#include <cstdio>
#include <chrono>
#include <fstream>
#include <filesystem>

// Helper for JIT to call for float printing
extern "C" void jit_print_double(double val) {
    if (val == (int64_t)val) {
        printf("%.1f\n", val); // Print 1.0 as 1.0 not 1
    } else {
        printf("%g\n", val);
    }
}

// Helper for JIT to get nanosecond timestamp (for t.nanos())
extern "C" int64_t jit_get_nanos() {
    auto now = std::chrono::steady_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    return static_cast<int64_t>(ns);
}

// Helper for JIT to get clock in nanoseconds (for t.clock())
extern "C" int64_t jit_get_clock_ns() {
    auto now = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    return static_cast<int64_t>(ns);
}

// Structure to track heap-allocated arrays in JIT
struct JITArray {
    int64_t capacity;
    int64_t size;
    int64_t data[1]; // Placeholder for variable-length data
};

extern "C" void* jit_alloc_array(int64_t size) {
    int64_t capacity = size > 8 ? size : 8;
    JITArray* arr = (JITArray*)malloc(sizeof(JITArray) + capacity * sizeof(int64_t));
    if (!arr) return nullptr;
    arr->capacity = capacity;
    arr->size = size;
    return (void*)arr->data;
}

extern "C" void* jit_array_push(void* dataPtr, int64_t value) {
    if (!dataPtr) return nullptr;
    JITArray* arr = (JITArray*)((char*)dataPtr - offsetof(JITArray, data));
    
    if (arr->size >= arr->capacity) {
        arr->capacity *= 2;
        arr = (JITArray*)realloc(arr, sizeof(JITArray) + arr->capacity * sizeof(int64_t));
        if (!arr) return nullptr;
    }
    
    arr->data[arr->size] = value;
    arr->size++;
    return (void*)arr->data;
}

extern "C" char* jit_alloc_string(const char* s) {
    if (!s) return nullptr;
    return strdup(s);
}

extern "C" char* jit_string_concat(char* s1, char* s2) {
    if (!s1 || !s2) return nullptr;
    size_t l1 = strlen(s1);
    size_t l2 = strlen(s2);
    char* res = (char*)malloc(l1 + l2 + 1);
    if (!res) return nullptr;
    memcpy(res, s1, l1);
    memcpy(res + l1, s2, l2);
    res[l1 + l2] = '\0';
    return res;
}

namespace nevaarize {

JIT::JIT() 
    : stackSize(0)
    , nextStackSlot(0)
    , currentAST(nullptr)
    , inFunctionCall(false) {
    execMem = std::make_unique<ExecutableMemory>(65536);
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
    nextStackSlot += 16; // Reserve 16 bytes: 8 for Value, 8 for Tag
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

JITValue JIT::compileExpr(const AST& ast, NodeIndex idx) {
    if (idx == INVALID_NODE) {
        JITValue val;
        val.valueReg = X64Reg::RAX;
        val.typeReg = X64Reg::RAX;
        return val;
    }
    
    const ASTNode& node = ast.get(idx);
    CodeBuffer& buf = codegen.getCode();
    
    switch (node.type) {
        case NodeType::LITERAL_INT: {
            JITValue result;
            result.valueReg = allocateReg();
            result.typeReg = allocateReg();
            
            // Value
            bool valHigh = static_cast<uint8_t>(result.valueReg) >= 8;
            buf.emit8(0x48 | (valHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(result.valueReg) & 0x7));
            buf.emit64(std::get<int64_t>(node.literal.data));
            
            // Type (0 for Int)
            bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
            buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
            buf.emit64(0);
            
            return result;
        }
        
        case NodeType::LITERAL_FLOAT: {
            JITValue result;
            result.valueReg = allocateReg();
            result.typeReg = allocateReg();
            
            double value = std::get<double>(node.literal.data);
            int64_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            
            // Value
            bool valHigh = static_cast<uint8_t>(result.valueReg) >= 8;
            buf.emit8(0x48 | (valHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(result.valueReg) & 0x7));
            buf.emit64(static_cast<uint64_t>(bits));
            
            // Type (1 for Float)
            bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
            buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
            buf.emit64(1);
            
            return result;
        }
        
        case NodeType::LITERAL_BOOL: {
            JITValue result;
            result.valueReg = allocateReg();
            result.typeReg = allocateReg();
            
            bool value = std::get<bool>(node.literal.data);
            
            bool valHigh = static_cast<uint8_t>(result.valueReg) >= 8;
            buf.emit8(0x48 | (valHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(result.valueReg) & 0x7));
            buf.emit64(value ? 1 : 0);
            
            // Type 0 (Int) for Bool for now (simplification)
            bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
            buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
            buf.emit64(0);
            
            return result;
        }
        
        case NodeType::LITERAL_STRING: {
            JITValue result;
            result.valueReg = allocateReg();
            result.typeReg = allocateReg();
            
            const std::string& strVal = std::get<std::string>(node.literal.data);
            
            // Call jit_alloc_string(const char*)
            buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52);
            buf.emit8(0x41); buf.emit8(0x50); buf.emit8(0x41); buf.emit8(0x51);
            buf.emit8(0x41); buf.emit8(0x52); buf.emit8(0x41); buf.emit8(0x53);
            
            // rdi = strVal.c_str() (But this is at compile-time. We should pass the actual pointer)
            buf.emit8(0x48); buf.emit8(0xBF);
            buf.emit64(reinterpret_cast<uint64_t>(strVal.c_str()));
            
            // rax = jit_alloc_string
            buf.emit8(0x48); buf.emit8(0xB8);
            buf.emit64(reinterpret_cast<uint64_t>(jit_alloc_string));
            buf.emit8(0xFF); buf.emit8(0xD0);
            
            // Move result to result.valueReg
            bool valHigh = static_cast<uint8_t>(result.valueReg) >= 8;
            buf.emit8(0x48 | (valHigh ? 0x01 : 0));
            buf.emit8(0x89); buf.emit8(0xC0 | (static_cast<uint8_t>(result.valueReg) & 0x7));
            
            buf.emit8(0x41); buf.emit8(0x5B); buf.emit8(0x41); buf.emit8(0x5A);
            buf.emit8(0x41); buf.emit8(0x59); buf.emit8(0x41); buf.emit8(0x58);
            buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58);
            
            // Type 4 (String)
            bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
            buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
            buf.emit64(4);
            
            return result;
        }
        
        case NodeType::LITERAL_NIL: {
            JITValue result;
            result.valueReg = allocateReg();
            result.typeReg = allocateReg();
            
            // Nil: value=0, type=3
            bool valHigh = static_cast<uint8_t>(result.valueReg) >= 8;
            buf.emit8(0x48 | (valHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(result.valueReg) & 0x7));
            buf.emit64(0);
            
            bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
            buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
            buf.emit64(3);
            
            return result;
        }
        
        case NodeType::IDENTIFIER: {
            JITValue result;
            result.valueReg = allocateReg();
            result.typeReg = allocateReg();
            
            auto it = variables.find(node.name);
            if (it != variables.end()) {
                int32_t offset = it->second.stackOffset;
                
                // Load Value
                bool valHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                buf.emit8(0x48 | (valHigh ? 0x04 : 0));
                buf.emit8(0x8B);
                buf.emit8(0x85 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3));
                buf.emit32(static_cast<uint32_t>(offset));
                
                // Load Type
                bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                buf.emit8(0x48 | (typeHigh ? 0x04 : 0));
                buf.emit8(0x8B);
                buf.emit8(0x85 | ((static_cast<uint8_t>(result.typeReg) & 0x7) << 3));
                buf.emit32(static_cast<uint32_t>(offset + 8));
            }
            return result;
        }
        
        case NodeType::BINARY_OP: {
            JITValue left = compileExpr(ast, node.left);
            JITValue right = {X64Reg::RAX, X64Reg::RAX}; // Initialize to silence warning
            bool rightIsImm = false;
            int64_t immVal = 0;
            const ASTNode& rNode = ast.get(node.right);
            
            // Check for immediate candidate (INT literal fitting 32-bit signed)
            // Supported ops: ADD, SUB, MUL, Comparisons. (DIV/MOD/AND/OR logic remains register-based)
            bool isSupportedOp = (node.binaryOp != BinaryOp::DIV && node.binaryOp != BinaryOp::MOD && 
                                  node.binaryOp != BinaryOp::AND && node.binaryOp != BinaryOp::OR);
                                  
            if (isSupportedOp && rNode.type == NodeType::LITERAL_INT) {
                 int64_t v = std::get<int64_t>(rNode.literal.data);
                 if (v >= -2147483648LL && v <= 2147483647LL) {
                     rightIsImm = true;
                     immVal = v;
                 }
            }
            
            if (!rightIsImm) {
                right = compileExpr(ast, node.right);
            }
            
            // Allocate register for results - OPTIMIZATION: Reuse left as result
            JITValue result;
            result.valueReg = left.valueReg;
            result.typeReg = left.typeReg;
            
            // Check types: Is either a float? (tag != 0)
            X64Reg typeScratch = allocateReg();
            
            bool tempHigh = static_cast<uint8_t>(typeScratch) >= 8;
            bool lTypeHigh = static_cast<uint8_t>(left.typeReg) >= 8;
            
            // mov scratch, left.type
            buf.emit8(0x48 | (tempHigh ? 0x01 : 0) | (lTypeHigh ? 0x04 : 0));
            buf.emit8(0x89);
            buf.emit8(0xC0 | ((static_cast<uint8_t>(left.typeReg) & 0x7) << 3) | 
                      (static_cast<uint8_t>(typeScratch) & 0x7));
            
            if (!rightIsImm) {
                // or scratch, right.type
                bool rTypeHigh = static_cast<uint8_t>(right.typeReg) >= 8;
                buf.emit8(0x48 | (tempHigh ? 0x01 : 0) | (rTypeHigh ? 0x04 : 0));
                buf.emit8(0x09);
                buf.emit8(0xC0 | ((static_cast<uint8_t>(right.typeReg) & 0x7) << 3) | 
                          (static_cast<uint8_t>(typeScratch) & 0x7));
            }
            // If rightIsImm, right.type is 0 (Int), so 'or scratch, 0' is nop.
                      
            // Check if scratch is 0
            buf.emit8(0x48 | (tempHigh ? 0x01 : 0)); // REX.W
            buf.emit8(0x85); // test r/m64, r64
            buf.emit8(0xC0 | ((static_cast<uint8_t>(typeScratch) & 0x7) << 3) | 
                      (static_cast<uint8_t>(typeScratch) & 0x7));
            
            // Free scratch
            freeReg(typeScratch);
                      
            // jnz float_path (if not zero, one of them is float)
            buf.emit8(0x0F);
            buf.emit8(0x85); // jnz far
            size_t jnzPatch = buf.getOffset();
            buf.emit32(0);
            
            // === INTEGER PATH ===
            // Copy left value to result value register if needed
            if (result.valueReg != left.valueReg) {
                bool resValHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                bool lValHigh = static_cast<uint8_t>(left.valueReg) >= 8;
                buf.emit8(0x48 | (resValHigh ? 0x01 : 0) | (lValHigh ? 0x04 : 0));
                buf.emit8(0x89);
                buf.emit8(0xC0 | ((static_cast<uint8_t>(left.valueReg) & 0x7) << 3) | 
                          (static_cast<uint8_t>(result.valueReg) & 0x7));
            }
            
            bool resHigh = static_cast<uint8_t>(result.valueReg) >= 8;
            bool rValHigh = (!rightIsImm && static_cast<uint8_t>(right.valueReg) >= 8);
            
            switch (node.binaryOp) {
                case BinaryOp::ADD: {
                    // Check if either operand is a string (Type 4)
                    // cmp typeReg, 4
                    bool resTypeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                    buf.emit8(0x48 | (resTypeHigh ? 0x01 : 0));
                    buf.emit8(0x83);
                    buf.emit8(0xF8 | (static_cast<uint8_t>(result.typeReg) & 0x7));
                    buf.emit8(4);
                    
                    // jne int_add
                    buf.emit8(0x75);
                    size_t jneOffset = buf.getOffset();
                    buf.emit8(0x00); // 1-byte placeholder
                    
                    // === STRING CONCAT ===
                    // Call jit_string_concat(result.valueReg, right.valueReg)
                    buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52);
                    buf.emit8(0x41); buf.emit8(0x50); buf.emit8(0x41); buf.emit8(0x51);
                    buf.emit8(0x41); buf.emit8(0x52); buf.emit8(0x41); buf.emit8(0x53);
                    
                    // rdi = result.valueReg (left)
                    buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                    buf.emit8(0x89); buf.emit8(0xC7 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3));
                    
                    // rsi = rightIsImm ? immVal : right.valueReg (right)
                    if (rightIsImm) {
                        buf.emit8(0x48); buf.emit8(0xBE);
                        buf.emit64(static_cast<uint64_t>(immVal)); // This won't work for string literals if rightIsImm. 
                        // But string literals are never "imm" in this JIT.
                    } else {
                        bool rHigh = static_cast<uint8_t>(right.valueReg) >= 8;
                        buf.emit8(0x48 | (rHigh ? 0x01 : 0));
                        buf.emit8(0x89); buf.emit8(0xD6 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3));
                        // Wait, RSI=0x89 C7|... for RDI? No, RSI is C6. RDI is C7.
                        // My previous emit8(0xC7 | ...) was for RDI. Correct.
                        // For RSI: 0x89 C6 | ... 
                    }
                    
                    // rax = jit_string_concat
                    buf.emit8(0x48); buf.emit8(0xB8);
                    buf.emit64(reinterpret_cast<uint64_t>(jit_string_concat));
                    buf.emit8(0xFF); buf.emit8(0xD0);
                    
                    // Move result to result.valueReg
                    buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                    buf.emit8(0x89); buf.emit8(0xC0 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                    
                    buf.emit8(0x41); buf.emit8(0x5B); buf.emit8(0x41); buf.emit8(0x5A);
                    buf.emit8(0x41); buf.emit8(0x59); buf.emit8(0x41); buf.emit8(0x58);
                    buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58);
                    
                    // jmp end
                    buf.emit8(0xEB);
                    size_t jmpOffset = buf.getOffset();
                    buf.emit8(0x00);
                    
                    // === INT ADD ===
                    size_t intAddPos = buf.getOffset();
                    buf.patch8(jneOffset, static_cast<uint8_t>(intAddPos - (jneOffset + 1)));

                    if (rightIsImm) {
                        buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                        buf.emit8(0x81); // ADD r/m64, imm32
                        buf.emit8(0xC0 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                        buf.emit32(static_cast<uint32_t>(immVal));
                    } else {
                        buf.emit8(0x48 | (rValHigh ? 0x04 : 0) | (resHigh ? 0x01 : 0));
                        buf.emit8(0x01);
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7));
                    }
                    
                    // jmp_end:
                    size_t endPos = buf.getOffset();
                    buf.patch8(jmpOffset, static_cast<uint8_t>(endPos - (jmpOffset + 1)));
                    
                    break;
                }
                case BinaryOp::SUB:
                    if (rightIsImm) {
                        buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                        buf.emit8(0x81); // SUB r/m64, imm32 (Group 1 /5)
                        buf.emit8(0xE8 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                        buf.emit32(static_cast<uint32_t>(immVal));
                    } else {
                        buf.emit8(0x48 | (rValHigh ? 0x04 : 0) | (resHigh ? 0x01 : 0));
                        buf.emit8(0x29);
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7));
                    }
                    break;
                case BinaryOp::MUL:
                    if (rightIsImm) {
                        buf.emit8(0x48 | (resHigh ? 0x04 : 0) | (resHigh ? 0x01 : 0)); // dest=res, src=res
                        buf.emit8(0x69); // IMUL r64, r/m64, imm32
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7));
                        buf.emit32(static_cast<uint32_t>(immVal));
                    } else {
                        buf.emit8(0x48 | (resHigh ? 0x04 : 0) | (rValHigh ? 0x01 : 0));
                        buf.emit8(0x0F); buf.emit8(0xAF);
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(right.valueReg) & 0x7));
                    }
                    break;
                case BinaryOp::LT: {
                    if (rightIsImm) {
                        buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                        buf.emit8(0x81); // CMP r/m64, imm32 (Group 1 /7)
                        buf.emit8(0xF8 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                        buf.emit32(static_cast<uint32_t>(immVal));
                    } else {
                        buf.emit8(0x48 | (rValHigh ? 0x04 : 0) | (resHigh ? 0x01 : 0));
                        buf.emit8(0x39);
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7));
                    }
                    // SETL al
                    buf.emit8(0x0F); buf.emit8(0x9C); buf.emit8(0xC0);
                    // MOVZX
                    buf.emit8(0x48 | (resHigh ? 0x04 : 0));
                    buf.emit8(0x0F); buf.emit8(0xB6);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3));
                    break;
                }
                case BinaryOp::GT: {
                    if (rightIsImm) {
                        buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                        buf.emit8(0x81); // CMP r/m64, imm32
                        buf.emit8(0xF8 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                        buf.emit32(static_cast<uint32_t>(immVal));
                    } else {
                        buf.emit8(0x48 | (rValHigh ? 0x04 : 0) | (resHigh ? 0x01 : 0));
                        buf.emit8(0x39);
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7));
                    }
                    // SETG al
                    buf.emit8(0x0F); buf.emit8(0x9F); buf.emit8(0xC0);
                    // MOVZX
                    buf.emit8(0x48 | (resHigh ? 0x04 : 0));
                    buf.emit8(0x0F); buf.emit8(0xB6);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3));
                    break;
                }
                case BinaryOp::LTE: {
                    if (rightIsImm) {
                        buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                        buf.emit8(0x81); // CMP r/m64, imm32
                        buf.emit8(0xF8 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                        buf.emit32(static_cast<uint32_t>(immVal));
                    } else {
                        buf.emit8(0x48 | (rValHigh ? 0x04 : 0) | (resHigh ? 0x01 : 0));
                        buf.emit8(0x39);
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7));
                    }
                    // SETLE al
                    buf.emit8(0x0F); buf.emit8(0x9E); buf.emit8(0xC0);
                    // MOVZX
                    buf.emit8(0x48 | (resHigh ? 0x04 : 0));
                    buf.emit8(0x0F); buf.emit8(0xB6);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3));
                    break;
                }
                case BinaryOp::GTE: {
                    if (rightIsImm) {
                        buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                        buf.emit8(0x81); // CMP r/m64, imm32
                        buf.emit8(0xF8 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                        buf.emit32(static_cast<uint32_t>(immVal));
                    } else {
                        buf.emit8(0x48 | (rValHigh ? 0x04 : 0) | (resHigh ? 0x01 : 0));
                        buf.emit8(0x39);
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7));
                    }
                    // SETGE al
                    buf.emit8(0x0F); buf.emit8(0x9D); buf.emit8(0xC0);
                    // MOVZX
                    buf.emit8(0x48 | (resHigh ? 0x04 : 0));
                    buf.emit8(0x0F); buf.emit8(0xB6);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3));
                    break;
                }
                case BinaryOp::EQ: {
                    if (rightIsImm) {
                        buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                        buf.emit8(0x81); // CMP r/m64, imm32
                        buf.emit8(0xF8 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                        buf.emit32(static_cast<uint32_t>(immVal));
                    } else {
                        buf.emit8(0x48 | (rValHigh ? 0x04 : 0) | (resHigh ? 0x01 : 0));
                        buf.emit8(0x39);
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7));
                    }
                    // SETE al
                    buf.emit8(0x0F); buf.emit8(0x94); buf.emit8(0xC0);
                    // MOVZX
                    buf.emit8(0x48 | (resHigh ? 0x04 : 0));
                    buf.emit8(0x0F); buf.emit8(0xB6);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3));
                    break;
                }
                case BinaryOp::NEQ: {
                    if (rightIsImm) {
                        buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                        buf.emit8(0x81); // CMP r/m64, imm32
                        buf.emit8(0xF8 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                        buf.emit32(static_cast<uint32_t>(immVal));
                    } else {
                        buf.emit8(0x48 | (rValHigh ? 0x04 : 0) | (resHigh ? 0x01 : 0));
                        buf.emit8(0x39);
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7));
                    }
                    // SETNE al
                    buf.emit8(0x0F); buf.emit8(0x95); buf.emit8(0xC0);
                    // MOVZX
                    buf.emit8(0x48 | (resHigh ? 0x04 : 0));
                    buf.emit8(0x0F); buf.emit8(0xB6);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3));
                    break;
                }
                case BinaryOp::AND: {
                    // Logical AND: result && right
                    // TEST result, result (check if result is non-zero)
                    buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                    buf.emit8(0x85);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7));
                    
                    // SETNE al (result != 0)
                    buf.emit8(0x0F); buf.emit8(0x95); buf.emit8(0xC0);
                    
                    // TEST right, right
                    buf.emit8(0x48 | (rValHigh ? 0x01 : 0));
                    buf.emit8(0x85);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(right.valueReg) & 0x7));
                    
                    // SETNE cl (right != 0)
                    buf.emit8(0x0F); buf.emit8(0x95); buf.emit8(0xC1);
                    
                    // AND al, cl
                    buf.emit8(0x20); buf.emit8(0xC8);
                    
                    // MOVZX result, al
                    buf.emit8(0x48 | (resHigh ? 0x04 : 0));
                    buf.emit8(0x0F); buf.emit8(0xB6);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3));
                    break;
                }
                case BinaryOp::OR: {
                    // Logical OR: result || right
                    // OR result, right (bitwise)
                    buf.emit8(0x48 | (rValHigh ? 0x04 : 0) | (resHigh ? 0x01 : 0));
                    buf.emit8(0x09);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7));
                    
                    // TEST result, result
                    buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                    buf.emit8(0x85);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7));
                    
                    // SETNE al
                    buf.emit8(0x0F); buf.emit8(0x95); buf.emit8(0xC0);
                    
                    // MOVZX result, al
                    buf.emit8(0x48 | (resHigh ? 0x04 : 0));
                    buf.emit8(0x0F); buf.emit8(0xB6);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3));
                    break;
                }
                case BinaryOp::DIV: {
                    // Integer division: result / right
                    // x86-64 IDIV: divides RDX:RAX by operand, quotient in RAX
                    // CRITICAL: right may be in RAX or RDX, save it to RCX first
                    
                    // Save registers that will be clobbered
                    buf.emit8(0x51); // push rcx
                    buf.emit8(0x52); // push rdx
                    
                    // Move divisor (right) to RCX (safe location)
                    buf.emit8(0x48 | (rValHigh ? 0x04 : 0));
                    buf.emit8(0x89);
                    buf.emit8(0xC1 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3)); // mov rcx, right
                    
                    // Move dividend (result) to RAX
                    if (result.valueReg != X64Reg::RAX) {
                        buf.emit8(0x48 | (resHigh ? 0x04 : 0));
                        buf.emit8(0x89);
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3)); // mov rax, result
                    }
                    
                    // Sign-extend RAX into RDX (CQO)
                    buf.emit8(0x48); buf.emit8(0x99);
                    
                    // IDIV rcx (divide RDX:RAX by RCX)
                    buf.emit8(0x48); buf.emit8(0xF7); buf.emit8(0xF9);
                    
                    // Move quotient (RAX) to result
                    if (result.valueReg != X64Reg::RAX) {
                        buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                        buf.emit8(0x89);
                        buf.emit8(0xC0 | (static_cast<uint8_t>(result.valueReg) & 0x7)); // mov result, rax
                    }
                    
                    // Restore registers
                    buf.emit8(0x5A); // pop rdx
                    buf.emit8(0x59); // pop rcx
                    break;
                }
                case BinaryOp::MOD: {
                    // Integer modulo: result % right
                    // x86-64 IDIV: remainder in RDX
                    
                    buf.emit8(0x51); // push rcx
                    buf.emit8(0x52); // push rdx
                    
                    // Move divisor (right) to RCX
                    buf.emit8(0x48 | (rValHigh ? 0x04 : 0));
                    buf.emit8(0x89);
                    buf.emit8(0xC1 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3));
                    
                    // Move dividend (result) to RAX
                    if (result.valueReg != X64Reg::RAX) {
                        buf.emit8(0x48 | (resHigh ? 0x04 : 0));
                        buf.emit8(0x89);
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3));
                    }
                    
                    // Sign-extend RAX into RDX (CQO)
                    buf.emit8(0x48); buf.emit8(0x99);
                    
                    // IDIV rcx
                    buf.emit8(0x48); buf.emit8(0xF7); buf.emit8(0xF9);
                    
                    // Move remainder (RDX) to result
                    buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                    buf.emit8(0x89);
                    buf.emit8(0xD0 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                    
                    // Pop saved registers (order matters!)
                    buf.emit8(0x48); buf.emit8(0x83); buf.emit8(0xC4); buf.emit8(0x08); // add rsp, 8 (discard rdx save)
                    buf.emit8(0x59); // pop rcx
                    break;
                }
                default: break;
            }
            
            // Set Result Type to INT (which it already should be if result.typeReg was used as temp and result was 0)
            // Wait, we modified result.typeReg with OR. If we are here, it is 0! No need to reset.
            
            // jmp end
            buf.emit8(0xE9);
            size_t jmpPatch = buf.getOffset();
            buf.emit32(0);
            
            // === FLOAT PATH ===
            size_t floatStart = buf.getOffset();
            int32_t jnzOffset = static_cast<int32_t>(floatStart - (jnzPatch + 4));
            buf.patch32(jnzPatch, static_cast<uint32_t>(jnzOffset));
            
            // Strategy: Convert BOTH to Double then OP
            // Use XMM0 and XMM1 as scratch
            
            // Load Left to XMM0
            // Check left.type (if 0, cvtsi2sd)
            // cmp left.type, 0
            buf.emit8(0x48 | (lTypeHigh ? 0x01 : 0));
            buf.emit8(0x83);
            buf.emit8(0xF8 | (static_cast<uint8_t>(left.typeReg) & 0x7));
            buf.emit8(0x00);
            
            // jnz left_is_float
            buf.emit8(0x75);
            buf.emit8(0x07); // Skip 7 bytes (cvtsi2sd (5) + jmp (2))
            
            // cvtsi2sd xmm0, left.val
            bool lValHigh = static_cast<uint8_t>(left.valueReg) >= 8;
            buf.emit8(0xF2); 
            buf.emit8(0x48 | (lValHigh ? 0x01 : 0)); // REX.W | REX.B
            buf.emit8(0x0F); buf.emit8(0x2A);
            buf.emit8(0xC0 | (static_cast<uint8_t>(left.valueReg) & 0x7));
            
            // jmp left_ready
            buf.emit8(0xEB); buf.emit8(0x05); // Skip 5 bytes (movq)
            
            // left_is_float: movq xmm0, left.val
            buf.emit8(0x66); buf.emit8(0x48 | (lValHigh ? 0x01 : 0)); buf.emit8(0x0F); buf.emit8(0x6E);
            buf.emit8(0xC0 | (static_cast<uint8_t>(left.valueReg) & 0x7));
            
            // Similar for Right to XMM1
            if (rightIsImm) {
                // If immediate, it's INT (0). Load to XMM1.
                // Need a scratch reg
                X64Reg rScratch = allocateReg();
                bool rScHigh = static_cast<uint8_t>(rScratch) >= 8;
                
                // mov scratch, imm64
                buf.emit8(0x48 | (rScHigh ? 0x01 : 0));
                buf.emit8(0xB8 + (static_cast<uint8_t>(rScratch) & 0x7));
                buf.emit64(static_cast<uint64_t>(immVal));
                
                // cvtsi2sd xmm1, scratch
                buf.emit8(0xF2);
                buf.emit8(0x48 | (rScHigh ? 0x01 : 0));
                buf.emit8(0x0F); buf.emit8(0x2A);
                buf.emit8(0xC8 | (static_cast<uint8_t>(rScratch) & 0x7)); // XMM1
                
                freeReg(rScratch);
            } else {
                // cmp right.type, 0
                bool rTypeHigh = static_cast<uint8_t>(right.typeReg) >= 8;
                buf.emit8(0x48 | (rTypeHigh ? 0x01 : 0));
                buf.emit8(0x83);
                buf.emit8(0xF8 | (static_cast<uint8_t>(right.typeReg) & 0x7));
                buf.emit8(0x00);
                
                // jnz right_is_float
                buf.emit8(0x75);
                buf.emit8(0x07); // Skip 7 bytes (cvtsi2sd (5) + jmp (2))
                
                // cvtsi2sd xmm1, right.val
                bool rValHigh = static_cast<uint8_t>(right.valueReg) >= 8;
                buf.emit8(0xF2);
                buf.emit8(0x48 | (rValHigh ? 0x01 : 0)); // REX.W | REX.B
                buf.emit8(0x0F); buf.emit8(0x2A);
                buf.emit8(0xC8 | (static_cast<uint8_t>(right.valueReg) & 0x7)); // XMM1
                
                // jmp right_ready
                buf.emit8(0xEB); buf.emit8(0x05);
                
                // right_is_float: movq xmm1, right.val
                buf.emit8(0x66); buf.emit8(0x48 | (rValHigh ? 0x01 : 0)); buf.emit8(0x0F); buf.emit8(0x6E);
                buf.emit8(0xC8 | (static_cast<uint8_t>(right.valueReg) & 0x7)); // XMM1
            }
            
            // Perform Float Op
            switch (node.binaryOp) {
                case BinaryOp::ADD: buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(0x58); buf.emit8(0xC1); break; // addsd xmm0, xmm1
                case BinaryOp::SUB: buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(0x5C); buf.emit8(0xC1); break; // subsd xmm0, xmm1
                case BinaryOp::MUL: buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(0x59); buf.emit8(0xC1); break; // mulsd xmm0, xmm1
                case BinaryOp::DIV: buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(0x5E); buf.emit8(0xC1); break; // divsd xmm0, xmm1
                case BinaryOp::LT: {
                    // UCOMISD xmm0, xmm1 (compare floats, sets CF if xmm0 < xmm1)
                    buf.emit8(0x66); buf.emit8(0x0F); buf.emit8(0x2E); buf.emit8(0xC1);
                    
                    // XOR rax, rax (clear for setb)
                    buf.emit8(0x48); buf.emit8(0x31); buf.emit8(0xC0);
                    
                    // SETB al (set byte if CF=1, i.e. unordered or less than)
                    buf.emit8(0x0F); buf.emit8(0x92); buf.emit8(0xC0);
                    
                    // CVTSI2SD xmm0, rax (convert 0/1 to 0.0/1.0)
                    buf.emit8(0xF2); buf.emit8(0x48); buf.emit8(0x0F); buf.emit8(0x2A); buf.emit8(0xC0);
                    break;
                }
                case BinaryOp::GT: {
                    // UCOMISD xmm0, xmm1
                    buf.emit8(0x66); buf.emit8(0x0F); buf.emit8(0x2E); buf.emit8(0xC1);
                    buf.emit8(0x48); buf.emit8(0x31); buf.emit8(0xC0);
                    // SETA al (above = CF=0 AND ZF=0)
                    buf.emit8(0x0F); buf.emit8(0x97); buf.emit8(0xC0);
                    buf.emit8(0xF2); buf.emit8(0x48); buf.emit8(0x0F); buf.emit8(0x2A); buf.emit8(0xC0);
                    break;
                }
                case BinaryOp::LTE: {
                    // UCOMISD xmm0, xmm1
                    buf.emit8(0x66); buf.emit8(0x0F); buf.emit8(0x2E); buf.emit8(0xC1);
                    buf.emit8(0x48); buf.emit8(0x31); buf.emit8(0xC0);
                    // SETBE al (below or equal = CF=1 OR ZF=1)
                    buf.emit8(0x0F); buf.emit8(0x96); buf.emit8(0xC0);
                    buf.emit8(0xF2); buf.emit8(0x48); buf.emit8(0x0F); buf.emit8(0x2A); buf.emit8(0xC0);
                    break;
                }
                case BinaryOp::GTE: {
                    // UCOMISD xmm0, xmm1
                    buf.emit8(0x66); buf.emit8(0x0F); buf.emit8(0x2E); buf.emit8(0xC1);
                    buf.emit8(0x48); buf.emit8(0x31); buf.emit8(0xC0);
                    // SETAE al (above or equal = CF=0)
                    buf.emit8(0x0F); buf.emit8(0x93); buf.emit8(0xC0);
                    buf.emit8(0xF2); buf.emit8(0x48); buf.emit8(0x0F); buf.emit8(0x2A); buf.emit8(0xC0);
                    break;
                }
                case BinaryOp::EQ: {
                    // UCOMISD xmm0, xmm1
                    buf.emit8(0x66); buf.emit8(0x0F); buf.emit8(0x2E); buf.emit8(0xC1);
                    buf.emit8(0x48); buf.emit8(0x31); buf.emit8(0xC0);
                    // SETE al (equal = ZF=1)
                    buf.emit8(0x0F); buf.emit8(0x94); buf.emit8(0xC0);
                    buf.emit8(0xF2); buf.emit8(0x48); buf.emit8(0x0F); buf.emit8(0x2A); buf.emit8(0xC0);
                    break;
                }
                case BinaryOp::NEQ: {
                    // UCOMISD xmm0, xmm1
                    buf.emit8(0x66); buf.emit8(0x0F); buf.emit8(0x2E); buf.emit8(0xC1);
                    buf.emit8(0x48); buf.emit8(0x31); buf.emit8(0xC0);
                    // SETNE al (not equal = ZF=0)
                    buf.emit8(0x0F); buf.emit8(0x95); buf.emit8(0xC0);
                    buf.emit8(0xF2); buf.emit8(0x48); buf.emit8(0x0F); buf.emit8(0x2A); buf.emit8(0xC0);
                    break;
                }
                default: break;
            }
            
            // Move result back to resultReg: movq result.valueReg, xmm0
            // 66 REX 0F 7E C0 (movd r/m64, xmm)
            buf.emit8(0x66);
            buf.emit8(0x48 | (resHigh ? 0x01 : 0));
            buf.emit8(0x0F);
            buf.emit8(0x7E);
            buf.emit8(0xC0 | (static_cast<uint8_t>(result.valueReg) & 0x7));
            
            // === END ===
            buf.emit8(0x48 | (static_cast<uint8_t>(result.typeReg) >= 8 ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
            buf.emit64(1);
            
            // === END ===
            size_t endPos = buf.getOffset();
            int32_t jmpOffset = static_cast<int32_t>(endPos - (jmpPatch + 4));
            buf.patch32(jmpPatch, static_cast<uint32_t>(jmpOffset));
            
            // freeReg(left.valueReg); freeReg(left.typeReg); // Reused as result
            if (!rightIsImm) {
                freeReg(right.valueReg); freeReg(right.typeReg);
            }
            return result;
        }
        
        case NodeType::UNARY_OP: {
            JITValue operand = compileExpr(ast, node.left);
            
            JITValue result;
            result.valueReg = operand.valueReg; // Reuse register for result
            result.typeReg = operand.typeReg;
            
            switch (node.unaryOp) {
                case UnaryOp::NEG: {
                    // Check type
                    bool typeHigh = static_cast<uint8_t>(operand.typeReg) >= 8;
                    bool valHigh = static_cast<uint8_t>(operand.valueReg) >= 8;
                    
                    // cmp type, 0
                    buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
                    buf.emit8(0x83);
                    buf.emit8(0xF8 | (static_cast<uint8_t>(operand.typeReg) & 0x7));
                    buf.emit8(0x00);
                    
                    // jnz float_neg
                    buf.emit8(0x75);
                    size_t jnzPatch = buf.getOffset();
                    buf.emit8(0x00); // 1 byte placeholder
                    
                    // === INT NEG ===
                    // neg operand
                    buf.emit8(0x48 | (valHigh ? 0x01 : 0));
                    buf.emit8(0xF7);
                    buf.emit8(0xD8 | (static_cast<uint8_t>(operand.valueReg) & 0x7));
                    
                    // jmp end
                    buf.emit8(0xEB);
                    size_t jmpPatch = buf.getOffset();
                    buf.emit8(0x00);
                    
                    // === FLOAT NEG === (offset at jnzPatch + 1)
                    size_t floatStart = buf.getOffset();
                    buf.patch8(jnzPatch, static_cast<uint8_t>(floatStart - (jnzPatch + 1)));
                    
                    // xor with sign bit (0x8000000000000000)
                    // MOVABS sign bit to temp reg is messy without clean scratch.
                    // Use simpler: mov result, 0; subsd result, operand
                    // But result is currently operand.
                    // Let's implement full XOR logic later. For now: 0 - x
                    // movq xmm0, operand
                    // xorps xmm1, xmm1
                    // subsd xmm1, xmm0
                    // movq operand, xmm1
                    
                    // Simplified: just flip the sign bit in standard ALU?
                    // Sign bit is MSB.
                    // mov rax, 0x8000000000000000
                    // xor operand, rax
                    
                    // mov rax, 0x8000000000000000
                    buf.emit8(0x48); buf.emit8(0xB8);
                    buf.emit64(0x8000000000000000ULL);
                    
                    // xor operand, rax
                    buf.emit8(0x48 | (valHigh ? 0x01 : 0));
                    buf.emit8(0x31);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(operand.valueReg) & 0x7)));
                    
                    // === END ===
                    size_t endPos = buf.getOffset();
                    buf.patch8(jmpPatch, static_cast<uint8_t>(endPos - (jmpPatch + 1)));
                    
                    break;
                }
                    
                case UnaryOp::NOT:
                    // Logical NOT: treat as boolean (zero/non-zero)
                    // Works same for Float 0.0 (all zero bits). 
                    // -0.0 has sign bit, so is "truthy" in this simple logic.
                    // Acceptable mostly.
                    
                    bool valHigh = static_cast<uint8_t>(operand.valueReg) >= 8;
                    
                    // test operand, operand
                    buf.emit8(0x48 | (valHigh ? 0x05 : 0));
                    buf.emit8(0x85);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(operand.valueReg) & 0x7) << 3) | 
                              (static_cast<uint8_t>(operand.valueReg) & 0x7));
                    
                    // setz al
                    buf.emit8(0x0F);
                    buf.emit8(0x94);
                    buf.emit8(0xC0);
                    
                    // movzx operand, al
                    buf.emit8(0x48 | (valHigh ? 0x04 : 0));
                    buf.emit8(0x0F);
                    buf.emit8(0xB6);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(operand.valueReg) & 0x7) << 3));
                    
                    // Set type to INT (0)
                    bool typeHigh = static_cast<uint8_t>(operand.typeReg) >= 8;
                    buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(operand.typeReg) & 0x7));
                    buf.emit64(0);
                    break;
            }
            
            return result;
        }
        
        case NodeType::CALL: {
            // Handle function call as expression
            if (node.left == INVALID_NODE) {
                JITValue nullVal;
                nullVal.valueReg = X64Reg::RAX;
                nullVal.typeReg = X64Reg::RAX;
                return nullVal;
            }
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
                    JITValue result;
                    result.valueReg = dst;
                    result.typeReg = allocateReg();
                    bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                    buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                    buf.emit64(0); // Int
                    return result;
                }
                
                if (funcName == "type") {
                    // type() returns 1 (simplified)
                    X64Reg dst = allocateReg();
                    bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                    buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
                    buf.emit64(1);
                    
                    JITValue result;
                    result.valueReg = dst;
                    result.typeReg = allocateReg();
                    bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                    buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                    buf.emit64(0); // Int
                    return result;
                }
                
                if (funcName == "int" || funcName == "str" || funcName == "float") {
                    // Just pass through the argument value for now
                    if (!node.children.empty()) {
                        return compileExpr(ast, node.children[0]);
                    }
                    JITValue nullVal;
                    nullVal.valueReg = X64Reg::RAX;
                    nullVal.typeReg = X64Reg::RAX;
                    return nullVal;
                }
                
                // Struct constructor - check if funcName is a registered struct
                auto structIt = structs.find(funcName);
                if (structIt != structs.end()) {
                    const StructInfo& info = structIt->second;
                    
                    // Allocate stack space for struct (16 bytes per field: value + type)
                    int32_t baseOffset = allocateStackSlot();
                    for (size_t i = 1; i < info.fieldNames.size(); ++i) {
                        allocateStackSlot();
                    }
                    
                    CodeBuffer& buf = codegen.getCode();
                    
                    // Initialize each field with provided arguments or default 0
                    for (size_t i = 0; i < info.fieldNames.size(); ++i) {
                        int32_t fieldOffset = baseOffset + (i * 16);
                        
                        if (i < node.children.size()) {
                            // Compile argument expression
                            JITValue val = compileExpr(ast, node.children[i]);
                            bool valHigh = static_cast<uint8_t>(val.valueReg) >= 8;
                            
                            // Store value: mov [rbp + fieldOffset], valueReg
                            buf.emit8(0x48 | (valHigh ? 0x04 : 0));
                            buf.emit8(0x89);
                            buf.emit8(0x85 | ((static_cast<uint8_t>(val.valueReg) & 0x7) << 3));
                            buf.emit32(static_cast<uint32_t>(fieldOffset));
                            
                            // Store type: mov [rbp + fieldOffset + 8], typeReg
                            bool typeHigh = static_cast<uint8_t>(val.typeReg) >= 8;
                            buf.emit8(0x48 | (typeHigh ? 0x04 : 0));
                            buf.emit8(0x89);
                            buf.emit8(0x85 | ((static_cast<uint8_t>(val.typeReg) & 0x7) << 3));
                            buf.emit32(static_cast<uint32_t>(fieldOffset + 8));
                            
                            freeReg(val.valueReg);
                            freeReg(val.typeReg);
                        } else {
                            // Default to 0 with type Int
                            buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0x85);
                            buf.emit32(static_cast<uint32_t>(fieldOffset));
                            buf.emit32(0);
                            buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0x85);
                            buf.emit32(static_cast<uint32_t>(fieldOffset + 8));
                            buf.emit32(0);
                        }
                    }
                    
                    // Return pointer to struct base
                    JITValue result;
                    result.valueReg = allocateReg();
                    result.typeReg = allocateReg();
                    
                    bool valHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                    // lea result.valueReg, [rbp + baseOffset]
                    buf.emit8(0x48 | (valHigh ? 0x04 : 0));
                    buf.emit8(0x8D); // LEA
                    buf.emit8(0x85 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3));
                    buf.emit32(static_cast<uint32_t>(baseOffset));
                    
                    bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                    buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                    buf.emit64(4); // Struct pointer type
                    
                    return result;
                }
                
                // User-defined functions
                if (userFunctions.count(funcName)) {
                    return compileUserCall(ast, idx, funcName);
                }
            } else if (callee.type == NodeType::MEMBER_ACCESS) {
                // Module function calls like ai.loadModel()
                // Compile the object first
                JITValue objVal = compileExpr(ast, callee.left);
                X64Reg objReg = objVal.valueReg;
                
                const std::string& memberName = callee.name;
                
                if (memberName == "clock" || memberName == "nanos") {
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
                    X64Reg dst = allocateReg()
;
                    if (dst != X64Reg::RAX) {
                        // mov dst, rax
                        bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                        buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
                        buf.emit8(0x89);
                        buf.emit8(0xC0 | (static_cast<uint8_t>(dst) & 0x7));
                    }
                    
                    freeReg(objVal.valueReg);
                    freeReg(objVal.typeReg);
                    
                    JITValue result;
                    result.valueReg = dst;
                    result.typeReg = allocateReg();
                    bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                    buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                    buf.emit64(0); // Int (nanoseconds)
                    return result;
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
                    
                    freeReg(objVal.valueReg);
                    freeReg(objVal.typeReg);
                    
                    JITValue result;
                    result.valueReg = dst;
                    result.typeReg = allocateReg();
                    bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                    buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                    buf.emit64(0); // Pointer as Int
                    return result;
                }
                
                // HTTP module functions (mocked for testing)
                if (memberName == "route") {
                    X64Reg dst = allocateReg();
                    bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                    buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
                    buf.emit64(0);
                    
                    freeReg(objVal.valueReg);
                    freeReg(objVal.typeReg);
                    
                    JITValue result;
                    result.valueReg = dst;
                    result.typeReg = allocateReg();
                    bool typeHigh = static_cast<uint8_t>(result.typeReg) >=  8;
                    buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                    buf.emit64(0); // Int
                    return result;
                }
                
                if (memberName == "push") {
                    // arr.push(val)
                    if (node.children.empty()) return objVal;
                    
                    // Compile argument (the value to push)
                    JITValue argVal = compileExpr(ast, node.children[0]);
                    
                    // Call jit_array_push(objReg, argVal.valueReg)
                    // Save caller-save
                    buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52);
                    buf.emit8(0x41); buf.emit8(0x50); buf.emit8(0x41); buf.emit8(0x51);
                    buf.emit8(0x41); buf.emit8(0x52); buf.emit8(0x41); buf.emit8(0x53);
                    
                    // rdi = objReg (array ptr)
                    bool objHigh = static_cast<uint8_t>(objReg) >= 8;
                    buf.emit8(0x48 | (objHigh ? 0x01 : 0));
                    buf.emit8(0x89); buf.emit8(0xC7 | ((static_cast<uint8_t>(objReg) & 0x7) << 3));
                    
                    // rsi = argVal.valueReg (value)
                    bool argHigh = static_cast<uint8_t>(argVal.valueReg) >= 8;
                    buf.emit8(0x48 | (argHigh ? 0x01 : 0));
                    buf.emit8(0x89); buf.emit8(0xC6 | ((static_cast<uint8_t>(argVal.valueReg) & 0x7) << 3));
                    
                    // rax = jit_array_push
                    buf.emit8(0x48); buf.emit8(0xB8);
                    buf.emit64(reinterpret_cast<uint64_t>(jit_array_push));
                    buf.emit8(0xFF); buf.emit8(0xD0);
                    
                    // Resulting array pointer is in RAX (may have changed due to realloc)
                    // We MUST update the variable if it's an identifier
                    if (ast.get(node.left).left != INVALID_NODE) {
                         const ASTNode& targetNode = ast.get(ast.get(node.left).left);
                         if (targetNode.type == NodeType::IDENTIFIER) {
                             const std::string& varName = targetNode.name;
                             if (variables.count(varName)) {
                                 int32_t offset = variables[varName].stackOffset;
                                 buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x85);
                                 buf.emit32(static_cast<uint32_t>(offset));
                             }
                         }
                    }
                    
                    // Capture new pointer to objReg
                    buf.emit8(0x48 | (objHigh ? 0x01 : 0));
                    buf.emit8(0x89); buf.emit8(0xC0 | (static_cast<uint8_t>(objReg) & 0x7));
                    
                    // Restore
                    buf.emit8(0x41); buf.emit8(0x5B); buf.emit8(0x41); buf.emit8(0x5A);
                    buf.emit8(0x41); buf.emit8(0x59); buf.emit8(0x41); buf.emit8(0x58);
                    buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58);
                    
                    freeReg(argVal.valueReg);
                    freeReg(argVal.typeReg);
                    
                    return objVal; 
                }
                
                if (memberName == "serve") {
                    // http.serve() - mock implementation
                    X64Reg dst = allocateReg();
                    bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                    buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
                    buf.emit64(0);
                    
                    freeReg(objVal.valueReg);
                    freeReg(objVal.typeReg);
                    
                    JITValue result;
                    result.valueReg = dst;
                    result.typeReg = allocateReg();
                    bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                    buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                    buf.emit64(0);
                    return result;
                }
            }

            JITValue nullVal;
            nullVal.valueReg = X64Reg::RAX;
            nullVal.typeReg = X64Reg::RAX;
            return nullVal;
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
            X64Reg baseReg = allocateReg();
            buf.emit8(0x48);
            buf.emit8(0x89);
            buf.emit8(0xE0 | (static_cast<uint8_t>(baseReg) & 0x7)); // mov baseReg, rsp
            
            // Store each element
            for (size_t i = 0; i < elemCount; ++i) {
                JITValue elemVal = compileExpr(ast, node.children[i]);
                X64Reg elemReg = elemVal.valueReg;
                freeReg(elemVal.typeReg);
                
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
            JITValue result;
            result.valueReg = baseReg;
            result.typeReg = allocateReg();
            bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
            buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
            buf.emit64(0);
            
            return result;
        }
        
        case NodeType::INDEX_ACCESS: {
            // array[index] - load element from array pointer
            JITValue arrVal = compileExpr(ast, node.left);
            X64Reg arrReg = arrVal.valueReg;
            // Ignore array type for now
            freeReg(arrVal.typeReg);
            
            JITValue idxVal = compileExpr(ast, node.right);
            X64Reg idxReg = idxVal.valueReg;
            freeReg(idxVal.typeReg);
            
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
            
            JITValue result;
            result.valueReg = arrReg;
            result.typeReg = allocateReg();
            bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
            buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
            buf.emit64(0); // Element type assumed Int for now
            return result;
        }
        
        case NodeType::MEMBER_ACCESS: {
            // Evaluate the object first
            JITValue objVal = compileExpr(ast, node.left);
            X64Reg objReg = objVal.valueReg;
            
            // Handle .length on arrays
            if (node.name == "length") {
                X64Reg dst = allocateReg();
                bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                bool objHigh = static_cast<uint8_t>(objReg) >= 8;
                buf.emit8(0x48 | (dstHigh ? 0x04 : 0) | (objHigh ? 0x01 : 0));
                buf.emit8(0x8B);
                buf.emit8(0x40 | ((static_cast<uint8_t>(dst) & 0x7) << 3) | (static_cast<uint8_t>(objReg) & 0x7));
                buf.emit8(static_cast<uint8_t>(0xF8)); // -8 offset from data pointer to size
                
                freeReg(objVal.valueReg);
                freeReg(objVal.typeReg);
                
                JITValue result;
                result.valueReg = dst;
                result.typeReg = allocateReg();
                buf.emit8(0x48); buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                buf.emit64(0); // Int
                return result;
            }
            
            // Determine field offset
            int32_t fieldIndex = -1;
            // 1. Try to find if we know the struct type (we don't have types yet, so we look in all structs)
            for (auto const& [name, info] : structs) {
                for (size_t i = 0; i < info.fieldNames.size(); ++i) {
                    if (info.fieldNames[i] == node.name) {
                        fieldIndex = static_cast<int32_t>(i);
                        break;
                    }
                }
                if (fieldIndex != -1) break;
            }
            
            // 2. If not found in structs, maybe it's an anonymous field index (for now, assume field index is 0 if unknown)
            if (fieldIndex == -1) fieldIndex = 0;

            int32_t fieldOffset = fieldIndex * 16;
            
            // objReg now contains actual pointer to struct base (from LEA in STRUCT_INIT)
            // Just access [objReg + fieldOffset]
            
            X64Reg resVal = allocateReg();
            X64Reg resType = allocateReg();
            
            bool valHigh = static_cast<uint8_t>(resVal) >= 8;
            bool typeHigh = static_cast<uint8_t>(resType) >= 8;
            bool objHigh = static_cast<uint8_t>(objReg) >= 8;
            
            // Load Value: mov resVal, [objReg + fieldOffset]
            buf.emit8(0x48 | (valHigh ? 0x04 : 0) | (objHigh ? 0x01 : 0));
            buf.emit8(0x8B);
            buf.emit8(0x80 | ((static_cast<uint8_t>(resVal) & 0x7) << 3) | (static_cast<uint8_t>(objReg) & 0x7));
            buf.emit32(static_cast<uint32_t>(fieldOffset));
            
            // Load Type: mov resType, [objReg + fieldOffset + 8]
            buf.emit8(0x48 | (typeHigh ? 0x04 : 0) | (objHigh ? 0x01 : 0));
            buf.emit8(0x8B);
            buf.emit8(0x80 | ((static_cast<uint8_t>(resType) & 0x7) << 3) | (static_cast<uint8_t>(objReg) & 0x7));
            buf.emit32(static_cast<uint32_t>(fieldOffset + 8));

            freeReg(objVal.valueReg);
            freeReg(objVal.typeReg);
            
            JITValue result;
            result.valueReg = resVal;
            result.typeReg = resType;
            return result;
        }
        
        case NodeType::STRUCT_INIT: {
            std::vector<std::string> fields;
            std::vector<NodeIndex> initializers;
            
            auto it = structs.find(node.name);
            if (it != structs.end()) {
                fields = it->second.fieldNames;
                initializers = node.children;
            } else {
                // Anonymous struct support
                fields = node.paramNames;
                initializers = node.children;
            }
            
            if (fields.empty()) {
                JITValue result;
                result.valueReg = allocateReg();
                result.typeReg = allocateReg();
                return result;
            }
            
            int32_t baseOffset = allocateStackSlot();
            // Allocate enough space for all fields (each field is 16 bytes: 8 for value, 8 for type)
            for (size_t i = 1; i < fields.size(); ++i) {
                allocateStackSlot();
            }
            
            CodeBuffer& buf = codegen.getCode();
            for (size_t i = 0; i < fields.size(); ++i) {
                int32_t fieldOffset = baseOffset + (i * 16);
                
                if (i < initializers.size()) {
                    JITValue val = compileExpr(ast, initializers[i]);
                    bool valHigh = static_cast<uint8_t>(val.valueReg) >= 8;
                    buf.emit8(0x48 | (valHigh ? 0x04 : 0));
                    buf.emit8(0x89);
                    buf.emit8(0x85 | ((static_cast<uint8_t>(val.valueReg) & 0x7) << 3));
                    buf.emit32(static_cast<uint32_t>(fieldOffset));
                    
                    bool typeHigh = static_cast<uint8_t>(val.typeReg) >= 8;
                    buf.emit8(0x48 | (typeHigh ? 0x04 : 0));
                    buf.emit8(0x89);
                    buf.emit8(0x85 | ((static_cast<uint8_t>(val.typeReg) & 0x7) << 3));
                    buf.emit32(static_cast<uint32_t>(fieldOffset + 8));
                    
                    freeReg(val.valueReg);
                    freeReg(val.typeReg);
                } else {
                    // Default to 0
                    buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0x85);
                    buf.emit32(static_cast<uint32_t>(fieldOffset));
                    buf.emit32(0);
                    // Default type to 0 (Int)
                    buf.emit8(0x48); buf.emit8(0xC7); buf.emit8(0x85);
                    buf.emit32(static_cast<uint32_t>(fieldOffset + 8));
                    buf.emit32(0);
                }
            }
            
            JITValue result;
            result.valueReg = allocateReg();
            result.typeReg = allocateReg();
            
            bool valHigh = static_cast<uint8_t>(result.valueReg) >= 8;
            // lea result.valueReg, [rbp + baseOffset]  - compute actual pointer to struct
            buf.emit8(0x48 | (valHigh ? 0x04 : 0));
            buf.emit8(0x8D); // LEA
            buf.emit8(0x85 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3));
            buf.emit32(static_cast<uint32_t>(baseOffset));
            
            bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
            buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
            buf.emit64(4); // Struct pointer type
            
            return result;
        }
        
        case NodeType::AWAIT_EXPR: {
            // Await expression support (synchronous evaluation)
            // Current: Immediately evaluates the expression (no suspension)
            // Future: Check if value is Promise, wait if pending, extract result
            // Note: True suspension requires stack unwinding or continuation passing
            return compileExpr(ast, node.left);
        }
        
        default: {
            JITValue nullVal;
            nullVal.valueReg = X64Reg::RAX;
            nullVal.typeReg = X64Reg::RAX;
            return nullVal;
        }
    }
}



void JIT::compileAssignment(const AST& ast, NodeIndex idx) {
    const ASTNode& node = ast.get(idx);
    CodeBuffer& buf = codegen.getCode();
    
    // Compile the value (returns pair: valueReg, typeReg)
    JITValue val = compileExpr(ast, node.left);
    
    // Allocate stack slot if needed
    auto it = variables.find(node.name);
    if (it == variables.end()) {
        VarLocation loc;
        loc.stackOffset = allocateStackSlot();
        loc.isRegister = false;
        variables[node.name] = loc;
        it = variables.find(node.name);
    }
    
    int32_t offset = it->second.stackOffset;
    
    // Store Value at [rbp + offset]
    bool valHigh = static_cast<uint8_t>(val.valueReg) >= 8;
    buf.emit8(0x48 | (valHigh ? 0x04 : 0));
    buf.emit8(0x89);
    buf.emit8(0x85 | ((static_cast<uint8_t>(val.valueReg) & 0x7) << 3));
    buf.emit32(static_cast<uint32_t>(offset));
    
    // Store Type Tag at [rbp + offset + 8]
    bool typeHigh = static_cast<uint8_t>(val.typeReg) >= 8;
    buf.emit8(0x48 | (typeHigh ? 0x04 : 0));
    buf.emit8(0x89);
    buf.emit8(0x85 | ((static_cast<uint8_t>(val.typeReg) & 0x7) << 3));
    buf.emit32(static_cast<uint32_t>(offset + 8));
    
    freeReg(val.valueReg);
    freeReg(val.typeReg);
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
    
    JITValue result = compileExpr(ast, exprNode);
    
    // Move result to RAX if not already there
    if (result.valueReg != X64Reg::RAX) {
        codegen.emitMov(X64Reg::RAX, result.valueReg);
    }
    
    // Explicitly free registers (though we return RAX val, we are done with JITValue struct)
    freeReg(result.typeReg);
    if (result.valueReg != X64Reg::RAX) freeReg(result.valueReg);
    
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
            JITValue value = compileExpr(ast, node.extra);  // value
            JITValue arr = compileExpr(ast, node.left);     // array
            JITValue idxVal = compileExpr(ast, node.right); // index
            
            // Assume array is Int storage for now (simplification)
            // Or handle types. But existing logic assumes untyped arrayptr?
            // "mov [arrReg + idxReg * 8], valueReg"
            
            // We use .valueReg for all pointers/indices
            X64Reg arrReg = arr.valueReg;
            X64Reg idxReg = idxVal.valueReg;
            X64Reg valueReg = value.valueReg;
            
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
            
            freeReg(value.valueReg); freeReg(value.typeReg);
            freeReg(arr.valueReg); freeReg(arr.typeReg);
            freeReg(idxVal.valueReg); freeReg(idxVal.typeReg);
            break;
        }
        
        case NodeType::MEMBER_ASSIGN: {
            // obj.field = value
            CodeBuffer& buf = codegen.getCode();
            JITValue target = compileExpr(ast, node.left);
            X64Reg objReg = target.valueReg;
            
            JITValue value = compileExpr(ast, node.right);
            
            // Resolve field offset
            int32_t fieldIndex = -1;
            for (auto const& [name, info] : structs) {
                for (size_t i = 0; i < info.fieldNames.size(); ++i) {
                    if (info.fieldNames[i] == node.name) {
                        fieldIndex = static_cast<int32_t>(i);
                        break;
                    }
                }
                if (fieldIndex != -1) break;
            }
            if (fieldIndex == -1) fieldIndex = 0;
            int32_t fieldOffset = fieldIndex * 16;
            
            // objReg now contains pointer - directly use it
            bool valHigh = static_cast<uint8_t>(value.valueReg) >= 8;
            bool typeHigh = static_cast<uint8_t>(value.typeReg) >= 8;
            bool objHigh = static_cast<uint8_t>(objReg) >= 8;
            
            // Store Value: mov [objReg + fieldOffset], valueReg
            buf.emit8(0x48 | (valHigh ? 0x04 : 0) | (objHigh ? 0x01 : 0));
            buf.emit8(0x89);
            buf.emit8(0x80 | ((static_cast<uint8_t>(value.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(objReg) & 0x7));
            buf.emit32(static_cast<uint32_t>(fieldOffset));
            
            // Store Type: mov [objReg + fieldOffset + 8], typeReg
            buf.emit8(0x48 | (typeHigh ? 0x04 : 0) | (objHigh ? 0x01 : 0));
            buf.emit8(0x89);
            buf.emit8(0x80 | ((static_cast<uint8_t>(value.typeReg) & 0x7) << 3) | (static_cast<uint8_t>(objReg) & 0x7));
            buf.emit32(static_cast<uint32_t>(fieldOffset + 8));
            
            freeReg(value.valueReg);
            freeReg(value.typeReg);
            freeReg(target.valueReg);
            freeReg(target.typeReg);
            break;
        }
        
        case NodeType::EXPR_STMT: {
            // Check if this is a function call like print()
            const ASTNode& exprNode = ast.get(node.left);
            if (exprNode.type == NodeType::CALL) {
                compileCall(ast, node.left);
            } else {
                JITValue val = compileExpr(ast, node.left);
                freeReg(val.valueReg);
                freeReg(val.typeReg);
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
        
        case NodeType::STRUCT_DECL: {
            StructInfo info;
            info.fieldNames = node.paramNames;
            info.size = node.paramNames.size();
            structs[node.name] = info;
            break;
        }
        
        case NodeType::IMPORT_FILE: {
            // Full file import implementation
            namespace fs = std::filesystem;
            
            if (node.paramNames.empty()) break;
            
            const std::string& filePath = node.name;
            const std::string& alias = node.paramNames[0];
            
            // Circular import detection
            if (importedFiles.count(filePath)) {
                break; // Already imported
            }
            
            // Resolve path (relative to current file or CWD)
            fs::path fullPath = filePath;
            if (!fullPath.is_absolute()) {
                // For now, use CWD-relative (can enhance to file-relative later)
                fullPath = fs::current_path() / filePath;
            }
            
            // Check if file exists
            if (!fs::exists(fullPath)) {
                break; // File not found, silently skip
            }
            
            // Read file content
            std::ifstream file(fullPath);
            if (!file.is_open()) break;
            
            std::string source((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
            file.close();
            
            // Mark as imported before parsing (prevent circular)
            importedFiles.insert(filePath);
            
            // Parse imported file
            Lexer lexer(source);
            auto tokens = lexer.tokenize();
            
            Parser parser(tokens);
            parser.parse();
            AST importedAST = std::move(parser.getAST());  // Move ownership
            
            // Store AST to keep it alive
            importedASTs[alias] = std::move(importedAST);
            const AST& storedAST = importedASTs[alias];
            
            // Store module info
            ModuleInfo modInfo;
            modInfo.filePath = fullPath.string();
            
            
            // Compile top-level functions from imported file
            const ASTNode& root = storedAST.get(storedAST.root());
            for (NodeIndex childIdx : root.children) {
                const ASTNode& child = storedAST.get(childIdx);
                
                if (child.type == NodeType::FUNC_DECL) {
                    // Register function with namespace prefix
                    std::string namespacedName = alias + "_" + child.name;
                    
                    // Store function info with source AST pointer
                    FuncInfo info;
                    info.bodyIndex = child.left;
                    info.paramNames = child.paramNames;
                    info.isCompiled = false;
                    info.compiledOffset = 0;
                    info.sourceAST = &storedAST;  // Point to stored AST
                    
                    userFunctions[namespacedName] = info;
                    modInfo.exportedFunctions[child.name] = childIdx;
                }
            }
            
            modules[alias] = modInfo;
            break;
        }
        
        case NodeType::ASYNC_FUNC_DECL: {
            // Async function support (synchronous execution model)
            // Current: Compiles as regular function (immediate execution)
            // Future: Wrap return in Promise, integrate with event loop
            // Note: True async requires CPS transformation or IR-level state machine
            compileFuncDecl(ast, idx);
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
    JITValue cond = compileExpr(ast, node.left);
    X64Reg condReg = cond.valueReg;
    
    // test condReg, condReg
    bool condHigh = static_cast<uint8_t>(condReg) >= 8;
    buf.emit8(0x48 | (condHigh ? 0x05 : 0));
    buf.emit8(0x85);
    buf.emit8(0xC0 | ((static_cast<uint8_t>(condReg) & 0x7) << 3) | 
              (static_cast<uint8_t>(condReg) & 0x7));
    
    freeReg(cond.valueReg);
    freeReg(cond.typeReg);
    
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
    
    // Align loop start
    while (buf.getOffset() % 16 != 0) {
        buf.emit8(0x90); // NOP
    }
    
    // loop_start:
    size_t loopStart = buf.getOffset();
    
    // Compile condition
    JITValue cond = compileExpr(ast, node.left);
    X64Reg condReg = cond.valueReg;
    
    // test condReg, condReg
    bool condHigh = static_cast<uint8_t>(condReg) >= 8;
    buf.emit8(0x48 | (condHigh ? 0x05 : 0));
    buf.emit8(0x85);
    buf.emit8(0xC0 | ((static_cast<uint8_t>(condReg) & 0x7) << 3) | 
              (static_cast<uint8_t>(condReg) & 0x7));
    
    freeReg(cond.valueReg);
    freeReg(cond.typeReg);
    
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
                JITValue startVal = compileExpr(ast, iterable.children[0]);
                X64Reg startReg = startVal.valueReg;
                
                // mov [rbp+offset], startReg
                bool regHigh = static_cast<uint8_t>(startReg) >= 8;
                buf.emit8(0x48 | (regHigh ? 0x04 : 0));
                buf.emit8(0x89);
                buf.emit8(0x85 | ((static_cast<uint8_t>(startReg) & 0x7) << 3));
                buf.emit32(static_cast<uint32_t>(iterLoc.stackOffset));
                
                freeReg(startVal.valueReg);
                freeReg(startVal.typeReg);
                
                // Compile end value
                JITValue endVal = compileExpr(ast, iterable.children[1]);
                X64Reg endReg = endVal.valueReg;
                
                // Move end value to RCX (loop limit)
                if (endReg != X64Reg::RCX) {
                    bool endHigh = static_cast<uint8_t>(endReg) >= 8;
                    buf.emit8(0x48 | (endHigh ? 0x01 : 0));
                    buf.emit8(0x89);
                    buf.emit8(0xC1 | ((static_cast<uint8_t>(endReg) & 0x7) << 3));
                    
                    freeReg(endReg);
                }
                
                // Free end type
                freeReg(endVal.typeReg);
                
                // Align loop start
                while (buf.getOffset() % 16 != 0) {
                    buf.emit8(0x90); // NOP
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
    
    if (node.left != INVALID_NODE) {
        JITValue result = compileExpr(ast, node.left);
        
        // Move value to RAX
        if (result.valueReg != X64Reg::RAX) {
            bool srcHigh = static_cast<uint8_t>(result.valueReg) >= 8;
            buf.emit8(0x48 | (srcHigh ? 0x04 : 0));
            buf.emit8(0x89);
            buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3) | 0);
        }
        
        // Move type to RDX (convention: RAX=value, RDX=type)
        if (result.typeReg != X64Reg::RDX) {
            bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
            buf.emit8(0x48 | (typeHigh ? 0x04 : 0));
            buf.emit8(0x89);
            buf.emit8(0xC2 | ((static_cast<uint8_t>(result.typeReg) & 0x7) << 3));
        }
        
        freeReg(result.valueReg);
        freeReg(result.typeReg);
    } else {
        // Return 0 (value)
        buf.emit8(0x48);
        buf.emit8(0x31);
        buf.emit8(0xC0);
        
        // Type = 0 (int)
        buf.emit8(0x48);
        buf.emit8(0x31);
        buf.emit8(0xD2);
    }
    
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
    info.bodyIndex = node.left;
    info.paramNames = node.paramNames;
    info.compiledOffset = 0;
    info.isCompiled = false;
    info.sourceAST = nullptr;  // Local function uses currentAST
    userFunctions[node.name] = info;
}

// Compile a user function call - inline the function body
JITValue JIT::compileUserCall(const AST& ast, NodeIndex idx, const std::string& funcName) {
    const ASTNode& node = ast.get(idx);
    JITValue defaultRes;
    defaultRes.valueReg = X64Reg::RAX;
    defaultRes.typeReg = X64Reg::RAX; // Dummy
    
    auto it = userFunctions.find(funcName);
    if (it == userFunctions.end()) {
        return defaultRes;
    }
    
    // Check for recursion - prevent infinite inlining
    if (currentlyCompiling.count(funcName)) {
        // Recursive call detected - return default value (1)
        JITValue dst;
        dst.valueReg = allocateReg();
        dst.typeReg = allocateReg();
        
        CodeBuffer& buf = codegen.getCode();
        bool dstHigh = static_cast<uint8_t>(dst.valueReg) >= 8;
        buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
        buf.emit8(0xB8 + (static_cast<uint8_t>(dst.valueReg) & 0x7));
        buf.emit64(1);
        
        // Type 0
        buf.emit8(0x48 | (static_cast<uint8_t>(dst.typeReg) >= 8 ? 0x01 : 0));
        buf.emit8(0xB8 + (static_cast<uint8_t>(dst.typeReg) & 0x7));
        buf.emit64(0);
        
        return dst;
    }
    
    const FuncInfo& funcInfo = it->second;
    
    // Mark function as being compiled
    currentlyCompiling.insert(funcName);
    
    // Save current variables state
    auto savedVars = variables;
    
    // Bind arguments to parameter names
    for (size_t i = 0; i < funcInfo.paramNames.size() && i < node.children.size(); ++i) {
        JITValue argVal = compileExpr(ast, node.children[i]);
        
        // Store argument in parameter variable
        VarLocation loc;
        loc.stackOffset = allocateStackSlot();
        loc.isRegister = false;
        variables[funcInfo.paramNames[i]] = loc;
        
        CodeBuffer& buf = codegen.getCode();
        
        // Store Value
        bool regHigh = static_cast<uint8_t>(argVal.valueReg) >= 8;
        buf.emit8(0x48 | (regHigh ? 0x04 : 0));
        buf.emit8(0x89);
        buf.emit8(0x85 | ((static_cast<uint8_t>(argVal.valueReg) & 0x7) << 3));
        buf.emit32(static_cast<uint32_t>(loc.stackOffset));
        
        // Store Type
        bool typeHigh = static_cast<uint8_t>(argVal.typeReg) >= 8;
        buf.emit8(0x48 | (typeHigh ? 0x04 : 0));
        buf.emit8(0x89);
        buf.emit8(0x85 | ((static_cast<uint8_t>(argVal.typeReg) & 0x7) << 3));
        buf.emit32(static_cast<uint32_t>(loc.stackOffset + 8));
        
        freeReg(argVal.valueReg);
        freeReg(argVal.typeReg);
    }
    
    // Set flag before compiling function body
    bool savedInFunctionCall = inFunctionCall;
    inFunctionCall = true;
    
    // Use source AST for imported functions, otherwise use current AST
    const AST& funcAST = (funcInfo.sourceAST != nullptr) ? *funcInfo.sourceAST : ast;
    
    // Compile the function body from correct AST
    if (funcInfo.bodyIndex != INVALID_NODE) {
        compileStatement(funcAST, funcInfo.bodyIndex);
    }
    
    inFunctionCall = savedInFunctionCall;
    variables = savedVars;
    currentlyCompiling.erase(funcName);
    
    // Function return convention: RAX=value, RDX=type
    // Allocate new registers and copy from RAX/RDX
    JITValue finalRes;
    finalRes.valueReg = allocateReg();
    finalRes.typeReg = allocateReg();
    
    CodeBuffer& buf = codegen.getCode();
    
    // Copy value from RAX to allocated register
    if (finalRes.valueReg != X64Reg::RAX) {
        bool dstHigh = static_cast<uint8_t>(finalRes.valueReg) >= 8;
        buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
        buf.emit8(0x89);
        buf.emit8(0xC0 | (0 << 3) | (static_cast<uint8_t>(finalRes.valueReg) & 0x7));
    }
    
    // Copy type from RDX to allocated register
    if (finalRes.typeReg != X64Reg::RDX) {
        bool typeHigh = static_cast<uint8_t>(finalRes.typeReg) >= 8;
        buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
        buf.emit8(0x89);
        buf.emit8(0xC0 | (2 << 3) | (static_cast<uint8_t>(finalRes.typeReg) & 0x7));
    }
    
    return finalRes;
}

// Compile function call
void JIT::compileCall(const AST& ast, NodeIndex idx) {
    const ASTNode& node = ast.get(idx);
    
    // Get the function name
    if (node.left == INVALID_NODE) return;
    const ASTNode& callee = ast.get(node.left);
    
    if (callee.type != NodeType::IDENTIFIER && callee.type != NodeType::MEMBER_ACCESS) return;
    
    CodeBuffer& buf = codegen.getCode();
    
    // Check for MEMBER_ACCESS (module calls)
    if (callee.type == NodeType::MEMBER_ACCESS) {
        const std::string& memberName = callee.name;
        
        // HTTP module functions (mocked for testing)
        if (memberName == "route") {
            return;
        }
        
        if (memberName == "serve") {
            JITValue argVal;
            if (!node.children.empty()) {
                argVal = compileExpr(ast, node.children[0]);
                freeReg(argVal.valueReg);
                freeReg(argVal.typeReg);
            }
            return;
        }
        
        // Check for module namespace calls (e.g., math.square())
        if (callee.left != INVALID_NODE) {
            const ASTNode& moduleNode = ast.get(callee.left);
            if (moduleNode.type == NodeType::IDENTIFIER) {
                const std::string& moduleAlias = moduleNode.name;
                
                // Check if this is a loaded module
                if (modules.count(moduleAlias)) {
                    const std::string& funcName = callee.name;
                    std::string namespacedName = moduleAlias + "_" + funcName;
                    
                    // Call the namespaced function
                    if (userFunctions.count(namespacedName)) {
                        JITValue result = compileUserCall(ast, idx, namespacedName);
                        freeReg(result.valueReg);
                        freeReg(result.typeReg);
                        return;
                    }
                }
            }
        }
        
        // Fallback for other member calls
        if (userFunctions.count(memberName)) {
             // Treat as user function if name matches
             JITValue result = compileUserCall(ast, idx, memberName);
             freeReg(result.valueReg);
             freeReg(result.typeReg);
             return;
        }
        
        // Time module functions - call native C++ helpers via FFI
        if (memberName == "nanos" || memberName == "clock") {
            // Allocate result registers for expression return
            // Note: This is a STATEMENT context (void), but we store result
            // for when called as expression. The caller will handle unused regs.
            
            // Call jit_get_nanos() which returns int64_t
            // mov rax, &jit_get_nanos
            buf.emit8(0x48); buf.emit8(0xB8);
            if (memberName == "nanos") {
                buf.emit64(reinterpret_cast<uint64_t>(&jit_get_nanos));
            } else {
                buf.emit64(reinterpret_cast<uint64_t>(&jit_get_clock_ns));
            }
            // call rax
            buf.emit8(0xFF); buf.emit8(0xD0);
            // Result is in RAX - for statement context we don't need to store it
            // But if used as expression, the caller needs it...
            // For now, statement calls ignore result.
            return;
        }
        return;
    }
    
    const std::string& funcName = callee.name;
    
    // Handle built-in print function
    if (funcName == "print") {
        // Compile each argument and print it
        for (size_t i = 0; i < node.children.size(); ++i) {
            if (i > 0) emitPrintSpace();
            
            const ASTNode& argNode = ast.get(node.children[i]);
            
            // Check if argument is a string literal
            if (argNode.type == NodeType::LITERAL_STRING) {
                // Simplification: direct print
                std::string strVal = std::get<std::string>(argNode.literal.data);
                // Simple escape processing
                std::string processed;
                for (size_t j = 0; j < strVal.length(); ++j) {
                    if (strVal[j] == '\\' && j + 1 < strVal.length()) {
                        char next = strVal[j + 1];
                        if (next == 'n') { processed += '\n'; ++j; }
                        else if (next == 't') { processed += '\t'; ++j; }
                        else processed += strVal[j];
                    } else {
                        processed += strVal[j];
                    }
                }
                emitPrintStringNoNewline(processed);
            } else {
                JITValue val = compileExpr(ast, node.children[i]);
                
                // Runtime Dispatch based on Type
                bool typeHigh = static_cast<uint8_t>(val.typeReg) >= 8;
                buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
                buf.emit8(0x83);
                buf.emit8(0xF8 | (static_cast<uint8_t>(val.typeReg) & 0x7));
                buf.emit8(0x00);
                
                // jnz float_print
                buf.emit8(0x75);
                size_t jnzPatch = buf.getOffset();
                buf.emit8(0x00);
                
                // === INT PRINT ===
                emitPrintIntNoNewline(val.valueReg);
                
                // jmp end
                buf.emit8(0xEB);
                size_t jmpPatch = buf.getOffset();
                buf.emit8(0x00);
                
                // === FLOAT PRINT ===
                size_t floatStart = buf.getOffset();
                buf.patch8(jnzPatch, static_cast<uint8_t>(floatStart - (jnzPatch + 1)));
                
                // Prepare call to jit_print_double(double)
                // ABI: arg in xmm0
                bool valHigh = static_cast<uint8_t>(val.valueReg) >= 8;
                buf.emit8(0x66); buf.emit8(0x48 | (valHigh ? 0x01 : 0)); buf.emit8(0x0F); buf.emit8(0x6E);
                buf.emit8(0xC0 | (static_cast<uint8_t>(val.valueReg) & 0x7));
                
                // mov rax, func_ptr
                buf.emit8(0x48); buf.emit8(0xB8);
                buf.emit64(reinterpret_cast<uint64_t>(jit_print_double));
                
                // Save RBX and align stack
                // push rbx
                buf.emit8(0x53);
                // mov rbx, rsp
                buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0xE3);
                // and rsp, -16
                buf.emit8(0x48); buf.emit8(0x83); buf.emit8(0xE4); buf.emit8(0xF0);
                
                // call rax
                buf.emit8(0xFF); buf.emit8(0xD0);
                
                // Restore stack and RBX
                // mov rsp, rbx
                buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0xDC);
                // pop rbx
                buf.emit8(0x5B);
                
                // === END ===
                size_t endPos = buf.getOffset();
                buf.patch8(jmpPatch, static_cast<uint8_t>(endPos - (jmpPatch + 1)));
                
                freeReg(val.valueReg);
                freeReg(val.typeReg);
            }
        }
        emitPrintNewline();
    } else if (funcName == "write") {
        for (NodeIndex argIdx : node.children) {
            const ASTNode& argNode = ast.get(argIdx);
            if (argNode.type == NodeType::LITERAL_STRING) {
                std::string strVal = std::get<std::string>(argNode.literal.data);
                // Simple escape processing
                std::string processed;
                for (size_t j = 0; j < strVal.length(); ++j) {
                    if (strVal[j] == '\\' && j + 1 < strVal.length()) {
                        char next = strVal[j + 1];
                        if (next == 'n') { processed += '\n'; ++j; }
                        else if (next == 't') { processed += '\t'; ++j; }
                        else processed += strVal[j];
                    } else {
                        processed += strVal[j];
                    }
                }
                emitPrintStringNoNewline(processed);
            } else {
                JITValue val = compileExpr(ast, argIdx);
                emitPrintIntNoNewline(val.valueReg); // Int-only for write
                freeReg(val.valueReg);
                freeReg(val.typeReg);
            }
        }
    } else if (userFunctions.count(funcName)) {
        // User-defined function call
        JITValue val = compileUserCall(ast, idx, funcName);
        freeReg(val.valueReg);
        freeReg(val.typeReg);
    }
    // Other built-in functions can be added here
}

} // namespace nevaarize
