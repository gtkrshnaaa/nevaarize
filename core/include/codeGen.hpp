/**
 * CodeGen.hpp - Nevaarize x86-64 Code Generator
 *
 * Direct machine code generation for x86-64 architecture.
 * No external assembler, pure native code emission.
 */

#ifndef NEVAARIZE_CODEGEN_HPP
#define NEVAARIZE_CODEGEN_HPP

#include "ir.hpp"
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace nevaarize {

/**
 * Linux x86-64 register enumeration.
 */
enum class X64Reg : uint8_t {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8  = 8, R9  = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,

    XMM0 = 16, XMM1 = 17, XMM2 = 18, XMM3 = 19,
    XMM4 = 20, XMM5 = 21, XMM6 = 22, XMM7 = 23
};

/**
 * Machine code buffer for direct emission.
 */
class CodeBuffer {
public:
    CodeBuffer() {
        code.reserve(4096);
    }

    void emit8(uint8_t byte) {
        code.push_back(byte);
    }

    void emit16(uint16_t value) {
        emit8(static_cast<uint8_t>(value & 0xFF));
        emit8(static_cast<uint8_t>((value >> 8) & 0xFF));
    }

    void emit32(uint32_t value) {
        emit8(static_cast<uint8_t>(value & 0xFF));
        emit8(static_cast<uint8_t>((value >> 8) & 0xFF));
        emit8(static_cast<uint8_t>((value >> 16) & 0xFF));
        emit8(static_cast<uint8_t>((value >> 24) & 0xFF));
    }

    void emit64(uint64_t value) {
        emit32(static_cast<uint32_t>(value & 0xFFFFFFFF));
        emit32(static_cast<uint32_t>((value >> 32) & 0xFFFFFFFF));
    }

    size_t size() const { return code.size(); }
    const uint8_t* data() const { return code.data(); }
    uint8_t* data() { return code.data(); }

    void patch32(size_t offset, uint32_t value) {
        code[offset] = static_cast<uint8_t>(value & 0xFF);
        code[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        code[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
        code[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
    }

    void patch8(size_t offset, uint8_t value) {
        if (offset < code.size()) {
            code[offset] = value;
        }
    }

    size_t getOffset() const { return code.size(); }

private:
    std::vector<uint8_t> code;
};

/**
 * Linux x86-64 native code generator.
 */
class CodeGenerator {
public:
    CodeGenerator() = default;

    /**
     * Generate code for an IR function.
     */
    void generate(const IRFunction& func);

    /**
     * Get generated code buffer.
     */
    CodeBuffer& getCode() { return buffer; }
    const CodeBuffer& getCode() const { return buffer; }

    // Linux x86-64 instruction emission helpers
    void emitPush(X64Reg reg);
    void emitPop(X64Reg reg);
    void emitMov(X64Reg dst, X64Reg src);
    void emitMovImm64(X64Reg dst, int64_t imm);
    void emitMovImm32(X64Reg dst, int32_t imm);
    void emitAdd(X64Reg dst, X64Reg src);
    void emitSub(X64Reg dst, X64Reg src);
    void emitMul(X64Reg src);
    void emitDiv(X64Reg src);
    void emitNeg(X64Reg reg);
    void emitCmp(X64Reg a, X64Reg b);
    void emitTest(X64Reg a, X64Reg b);
    void emitJmp(int32_t offset);
    void emitJe(int32_t offset);
    void emitJne(int32_t offset);
    void emitJl(int32_t offset);
    void emitJle(int32_t offset);
    void emitJg(int32_t offset);
    void emitJge(int32_t offset);
    void emitCall(X64Reg reg);
    void emitCallRel(int32_t offset);
    void emitRet();
    void emitNop();

    // REX prefix helpers
    void emitRex(bool w, bool r, bool x, bool b);

private:
    CodeBuffer buffer;
    std::unordered_map<std::string, size_t> labels;
    std::vector<std::pair<size_t, std::string>> patchPoints;
};

/**
 * Executable memory manager for JIT code.
 */
class ExecutableMemory {
public:
    ExecutableMemory(size_t size);
    ~ExecutableMemory();

    ExecutableMemory(const ExecutableMemory&) = delete;
    ExecutableMemory& operator=(const ExecutableMemory&) = delete;

    void* getBase() const { return memory; }
    size_t getSize() const { return size; }

    bool write(const uint8_t* data, size_t dataSize);
    bool makeExecutable();

    template<typename FuncPtr>
    FuncPtr getFunction(size_t offset = 0) const {
        return reinterpret_cast<FuncPtr>(static_cast<uint8_t*>(memory) + offset);
    }

private:
    void* memory;
    size_t size;
};

} // namespace nevaarize

#endif // NEVAARIZE_CODEGEN_HPP
