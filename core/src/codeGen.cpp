/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * CodeGen.cpp - Nevaarize Linux x86-64 Code Generator Implementation
 *
 * Direct machine code emission for Linux x86-64.
 */

#include "codeGen.hpp"

#ifdef __linux__
#include <sys/mman.h>
#include <cstring>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace nevaarize {

void CodeGenerator::generate(const IRFunction& func) {
    // Function prologue
    emitPush(X64Reg::RBP);
    emitMov(X64Reg::RBP, X64Reg::RSP);

    // Reserve stack space for locals
    if (func.localCount > 0) {
        size_t stackSize = func.localCount * 8;
        stackSize = (stackSize + 15) & ~15;
        emitMovImm32(X64Reg::RAX, static_cast<int32_t>(stackSize));
        emitSub(X64Reg::RSP, X64Reg::RAX);
    }

    // Generate code for each basic block
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        const BasicBlock& block = func.blocks[i];
        labels[block.label] = buffer.getOffset();

        for (const IRInst& inst : block.instructions) {
            switch (inst.opcode) {
                case IROpcode::CONST_INT:
                    emitMovImm64(X64Reg::RAX, inst.intVal);
                    break;

                case IROpcode::ADD:
                    emitAdd(X64Reg::RAX, X64Reg::RCX);
                    break;

                case IROpcode::SUB:
                    emitSub(X64Reg::RAX, X64Reg::RCX);
                    break;

                case IROpcode::MUL:
                    emitMul(X64Reg::RCX);
                    break;

                case IROpcode::DIV:
                    emitDiv(X64Reg::RCX);
                    break;

                case IROpcode::NEG:
                    emitNeg(X64Reg::RAX);
                    break;

                case IROpcode::RETURN:
                    emitMov(X64Reg::RSP, X64Reg::RBP);
                    emitPop(X64Reg::RBP);
                    emitRet();
                    break;

                case IROpcode::NOP:
                    emitNop();
                    break;

                default:
                    break;
            }
        }
    }

    // Function epilogue (if no explicit return)
    emitMov(X64Reg::RSP, X64Reg::RBP);
    emitPop(X64Reg::RBP);
    emitRet();
}

void CodeGenerator::emitRex(bool w, bool r, bool x, bool b) {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08;
    if (r) rex |= 0x04;
    if (x) rex |= 0x02;
    if (b) rex |= 0x01;
    buffer.emit8(rex);
}

void CodeGenerator::emitPush(X64Reg reg) {
    uint8_t regNum = static_cast<uint8_t>(reg) & 0x7;
    if (static_cast<uint8_t>(reg) >= 8) {
        emitRex(false, false, false, true);
    }
    buffer.emit8(0x50 + regNum);
}

void CodeGenerator::emitPop(X64Reg reg) {
    uint8_t regNum = static_cast<uint8_t>(reg) & 0x7;
    if (static_cast<uint8_t>(reg) >= 8) {
        emitRex(false, false, false, true);
    }
    buffer.emit8(0x58 + regNum);
}

void CodeGenerator::emitMov(X64Reg dst, X64Reg src) {
    bool dstHigh = static_cast<uint8_t>(dst) >= 8;
    bool srcHigh = static_cast<uint8_t>(src) >= 8;
    emitRex(true, srcHigh, false, dstHigh);
    buffer.emit8(0x89);
    buffer.emit8(0xC0 | ((static_cast<uint8_t>(src) & 0x7) << 3) | (static_cast<uint8_t>(dst) & 0x7));
}

void CodeGenerator::emitMovImm64(X64Reg dst, int64_t imm) {
    bool dstHigh = static_cast<uint8_t>(dst) >= 8;
    emitRex(true, false, false, dstHigh);
    buffer.emit8(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
    buffer.emit64(static_cast<uint64_t>(imm));
}

void CodeGenerator::emitMovImm32(X64Reg dst, int32_t imm) {
    bool dstHigh = static_cast<uint8_t>(dst) >= 8;
    if (dstHigh) {
        emitRex(false, false, false, true);
    }
    buffer.emit8(0xB8 + (static_cast<uint8_t>(dst) & 0x7));
    buffer.emit32(static_cast<uint32_t>(imm));
}

void CodeGenerator::emitAdd(X64Reg dst, X64Reg src) {
    bool dstHigh = static_cast<uint8_t>(dst) >= 8;
    bool srcHigh = static_cast<uint8_t>(src) >= 8;
    emitRex(true, srcHigh, false, dstHigh);
    buffer.emit8(0x01);
    buffer.emit8(0xC0 | ((static_cast<uint8_t>(src) & 0x7) << 3) | (static_cast<uint8_t>(dst) & 0x7));
}

void CodeGenerator::emitSub(X64Reg dst, X64Reg src) {
    bool dstHigh = static_cast<uint8_t>(dst) >= 8;
    bool srcHigh = static_cast<uint8_t>(src) >= 8;
    emitRex(true, srcHigh, false, dstHigh);
    buffer.emit8(0x29);
    buffer.emit8(0xC0 | ((static_cast<uint8_t>(src) & 0x7) << 3) | (static_cast<uint8_t>(dst) & 0x7));
}

void CodeGenerator::emitMul(X64Reg src) {
    bool srcHigh = static_cast<uint8_t>(src) >= 8;
    emitRex(true, false, false, srcHigh);
    buffer.emit8(0xF7);
    buffer.emit8(0xE0 | (static_cast<uint8_t>(src) & 0x7));
}

void CodeGenerator::emitDiv(X64Reg src) {
    bool srcHigh = static_cast<uint8_t>(src) >= 8;
    emitRex(true, false, false, srcHigh);
    buffer.emit8(0xF7);
    buffer.emit8(0xF0 | (static_cast<uint8_t>(src) & 0x7));
}

void CodeGenerator::emitNeg(X64Reg reg) {
    bool regHigh = static_cast<uint8_t>(reg) >= 8;
    emitRex(true, false, false, regHigh);
    buffer.emit8(0xF7);
    buffer.emit8(0xD8 | (static_cast<uint8_t>(reg) & 0x7));
}

void CodeGenerator::emitCmp(X64Reg a, X64Reg b) {
    bool aHigh = static_cast<uint8_t>(a) >= 8;
    bool bHigh = static_cast<uint8_t>(b) >= 8;
    emitRex(true, bHigh, false, aHigh);
    buffer.emit8(0x39);
    buffer.emit8(0xC0 | ((static_cast<uint8_t>(b) & 0x7) << 3) | (static_cast<uint8_t>(a) & 0x7));
}

void CodeGenerator::emitTest(X64Reg a, X64Reg b) {
    bool aHigh = static_cast<uint8_t>(a) >= 8;
    bool bHigh = static_cast<uint8_t>(b) >= 8;
    emitRex(true, bHigh, false, aHigh);
    buffer.emit8(0x85);
    buffer.emit8(0xC0 | ((static_cast<uint8_t>(b) & 0x7) << 3) | (static_cast<uint8_t>(a) & 0x7));
}

void CodeGenerator::emitJmp(int32_t offset) {
    buffer.emit8(0xE9);
    buffer.emit32(static_cast<uint32_t>(offset));
}

void CodeGenerator::emitJe(int32_t offset) {
    buffer.emit8(0x0F);
    buffer.emit8(0x84);
    buffer.emit32(static_cast<uint32_t>(offset));
}

void CodeGenerator::emitJne(int32_t offset) {
    buffer.emit8(0x0F);
    buffer.emit8(0x85);
    buffer.emit32(static_cast<uint32_t>(offset));
}

void CodeGenerator::emitJl(int32_t offset) {
    buffer.emit8(0x0F);
    buffer.emit8(0x8C);
    buffer.emit32(static_cast<uint32_t>(offset));
}

void CodeGenerator::emitJle(int32_t offset) {
    buffer.emit8(0x0F);
    buffer.emit8(0x8E);
    buffer.emit32(static_cast<uint32_t>(offset));
}

void CodeGenerator::emitJg(int32_t offset) {
    buffer.emit8(0x0F);
    buffer.emit8(0x8F);
    buffer.emit32(static_cast<uint32_t>(offset));
}

void CodeGenerator::emitJge(int32_t offset) {
    buffer.emit8(0x0F);
    buffer.emit8(0x8D);
    buffer.emit32(static_cast<uint32_t>(offset));
}

void CodeGenerator::emitCall(X64Reg reg) {
    bool regHigh = static_cast<uint8_t>(reg) >= 8;
    if (regHigh) {
        emitRex(false, false, false, true);
    }
    buffer.emit8(0xFF);
    buffer.emit8(0xD0 | (static_cast<uint8_t>(reg) & 0x7));
}

void CodeGenerator::emitCallRel(int32_t offset) {
    buffer.emit8(0xE8);
    buffer.emit32(static_cast<uint32_t>(offset));
}

void CodeGenerator::emitRet() {
    buffer.emit8(0xC3);
}

void CodeGenerator::emitNop() {
    buffer.emit8(0x90);
}

// Executable memory implementation
ExecutableMemory::ExecutableMemory(size_t sz) : memory(nullptr), size(sz) {
#ifdef __linux__
    memory = mmap(nullptr, sz, PROT_READ | PROT_WRITE, 
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        memory = nullptr;
    }
#elif defined(_WIN32)
    memory = VirtualAlloc(nullptr, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#endif
}

ExecutableMemory::~ExecutableMemory() {
    if (memory) {
#ifdef __linux__
        munmap(memory, size);
#elif defined(_WIN32)
        VirtualFree(memory, 0, MEM_RELEASE);
#endif
    }
}

bool ExecutableMemory::write(const uint8_t* data, size_t dataSize) {
    if (!memory || dataSize > size) return false;
    std::memcpy(memory, data, dataSize);
    return true;
}

bool ExecutableMemory::makeExecutable() {
    if (!memory) return false;
#ifdef __linux__
    return mprotect(memory, size, PROT_READ | PROT_EXEC) == 0;
#elif defined(_WIN32)
    DWORD oldProtect;
    return VirtualProtect(memory, size, PAGE_EXECUTE_READ, &oldProtect) != 0;
#else
    return false;
#endif
}

} // namespace nevaarize
