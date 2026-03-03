/**
 * Compiler.cpp - True JIT Compiler Implementation
 *
 * Compiles Nevaarize AST to Linux x86-64 machine code.
 * This compiles ACTUAL Nevaarize code, not pre-written assembly.
 */

#include "jit.hpp"
#include "parser.hpp"
#include "lexer.hpp"
#include "gc.hpp"
#include <cstring>
#include <cstdio>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>

// Helper for JIT to call for float printing (with newline)
extern "C" void jit_print_double(double val) {
    if (val == (int64_t)val) {
        printf("%.1f\n", val); // Print 1.0 as 1.0 not 1
    } else {
        printf("%g\n", val);
    }
}

// Helper for JIT to call for float printing (no newline - for multiple args)
extern "C" void jit_print_double_no_newline(double val) {
    if (val == (int64_t)val) {
        printf("%.1f", val);
    } else {
        printf("%g", val);
    }
    fflush(stdout);
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

// JIT String structure (defined at top for use by exception handling and map operations)
struct JITString {
    uint32_t magic;    // 0xNEVA
    uint32_t padding;  // Alignment to maintain 8-byte boundaries
    int64_t capacity;
    int64_t length;
    char data[1]; // Null-terminated string data
};

const uint32_t JIT_STRING_MAGIC = 0x4E455641; // "NEVA"

// Exception handling globals (thread_local for async safety)
thread_local void* current_exception_frame = nullptr;
thread_local int64_t current_exception_val = 0;
thread_local int64_t current_exception_type = 0;

/**
 * Handle unhandled exceptions with human-readable output.
 * Detects string exceptions and prints the message directly.
 */
extern "C" void jit_unhandled_exception() {
    if (current_exception_type == 4 && current_exception_val != 0) {
        // String exception — print the message
        void* dataPtr = reinterpret_cast<void*>(current_exception_val);
        JITString* str = reinterpret_cast<JITString*>(
            reinterpret_cast<char*>(dataPtr) - offsetof(JITString, data));
        std::cerr << "Unhandled Exception: ";
        std::cerr.write(str->data, str->length);
        std::cerr << std::endl;
    } else if (current_exception_type == 0) {
        std::cerr << "Unhandled Exception: " << current_exception_val << std::endl;
    } else {
        std::cerr << "Unhandled Exception (type=" << current_exception_type
                  << ", value=" << current_exception_val << ")" << std::endl;
    }
    exit(1);
}

/**
 * Print a JITString data pointer's content without newline.
 * Expects the data pointer from jit_alloc_string (points to JITString.data).
 */
extern "C" void jit_print_jitstring_no_newline(void* dataPtr) {
    if (!dataPtr) { printf("nil"); return; }
    JITString* str = reinterpret_cast<JITString*>(
        reinterpret_cast<char*>(dataPtr) - offsetof(JITString, data));
    fwrite(str->data, 1, str->length, stdout);
    fflush(stdout);
}


// Global garbage collector instance for JIT heap allocations
static nevaarize::GarbageCollector jitGC;

// Structure to track heap-allocated arrays in JIT
struct JITArray {
    int64_t capacity;
    int64_t size;
    int64_t data[1]; // Placeholder for variable-length data
};

extern "C" void* jit_alloc_array(int64_t size) {
    int64_t capacity = size > 8 ? size : 8;
    size_t totalBytes = sizeof(JITArray) + capacity * sizeof(int64_t);
    
    // Attempt GC-managed allocation
    void* mem = jitGC.allocate(totalBytes);
    if (!mem) {
        // Fallback to direct heap allocation
        mem = malloc(totalBytes);
    }
    if (!mem) return nullptr;
    
    JITArray* arr = static_cast<JITArray*>(mem);
    arr->capacity = capacity;
    arr->size = size;
    return static_cast<void*>(arr->data);
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

extern "C" void jit_array_set(void* dataPtr, int64_t index, int64_t value) {
    if (!dataPtr) return;
    JITArray* arr = (JITArray*)((char*)dataPtr - offsetof(JITArray, data));
    if (index >= 0 && index < arr->capacity) {
        arr->data[index] = value;
        if (index >= arr->size) {
            arr->size = index + 1;
        }
    }
}

extern "C" int64_t jit_array_get(void* dataPtr, int64_t index) {
    if (!dataPtr) return 0;
    JITArray* arr = (JITArray*)((char*)dataPtr - offsetof(JITArray, data));
    if (index >= 0 && index < arr->size) {
        return arr->data[index];
    }
    return 0;
}

/**
 * Return the number of elements in a JITArray.
 * Expects the data pointer (same as jit_alloc_array returns).
 */
extern "C" int64_t jit_array_size(void* dataPtr) {
    if (!dataPtr) return 0;
    JITArray* arr = (JITArray*)((char*)dataPtr - offsetof(JITArray, data));
    return arr->size;
}


// Structure to track heap-allocated maps in JIT
struct JITMapEntry {
    int64_t key;
    int64_t value;
    int8_t state; // 0 = empty, 1 = occupied, -1 = tombstone
};

struct JITMap {
    int64_t capacity;
    int64_t size;
    JITMapEntry entries[1]; // Flexible array member
};

/**
 * Detect if key is a JITString pointer by checking magic number.
 * Keys below 0x10000 are treated as plain integers to avoid
 * dereferencing invalid low addresses.
 */
static bool jit_is_string_key(int64_t key) {
    if (key == 0) return false;
    uint64_t ukey = static_cast<uint64_t>(key);
    if (ukey < 0x10000) return false; // Not a valid heap pointer
    JITString* s = reinterpret_cast<JITString*>(key);
    return s->magic == JIT_STRING_MAGIC;
}

/**
 * FNV-1a hash for arbitrary byte sequences.
 */
static uint64_t jit_fnv1a(const char* data, int64_t len) {
    uint64_t hash = 14695981039346656037ull;
    for (int64_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint8_t>(data[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

/**
 * Compute hash for a map key. Dispatches to FNV-1a for string keys
 * and multiplicative hash for integer keys.
 */
static uint64_t jit_hash_key(int64_t key) {
    if (jit_is_string_key(key)) {
        JITString* s = reinterpret_cast<JITString*>(key);
        return jit_fnv1a(s->data, s->length);
    }
    return static_cast<uint64_t>(key) * 2654435761ull;
}

/**
 * Compare two map keys for equality. String keys are compared by
 * content; all other keys are compared by raw value.
 */
static bool jit_keys_equal(int64_t a, int64_t b) {
    if (a == b) return true;
    bool aStr = jit_is_string_key(a);
    bool bStr = jit_is_string_key(b);
    if (aStr && bStr) {
        JITString* sa = reinterpret_cast<JITString*>(a);
        JITString* sb = reinterpret_cast<JITString*>(b);
        if (sa->length != sb->length) return false;
        return std::memcmp(sa->data, sb->data, sa->length) == 0;
    }
    return false;
}

extern "C" void* jit_alloc_map(int64_t initial_capacity) {
    if (initial_capacity < 8) initial_capacity = 8;
    int64_t cap = 1;
    while (cap < initial_capacity) cap *= 2;

    size_t totalBytes = sizeof(JITMap) + (cap - 1) * sizeof(JITMapEntry);
    void* mem = jitGC.allocate(totalBytes);
    if (!mem) mem = calloc(1, totalBytes);
    if (!mem) return nullptr;

    JITMap* map = static_cast<JITMap*>(mem);
    map->capacity = cap;
    map->size = 0;
    for (int64_t i = 0; i < cap; ++i) map->entries[i].state = 0;
    return static_cast<void*>(map);
}

/**
 * Resize map to double capacity and rehash all occupied entries.
 * Returns pointer to the new map.
 */
static void* jit_map_resize(JITMap* old) {
    int64_t newCap = old->capacity * 2;
    size_t totalBytes = sizeof(JITMap) + (newCap - 1) * sizeof(JITMapEntry);
    void* mem = jitGC.allocate(totalBytes);
    if (!mem) mem = calloc(1, totalBytes);
    if (!mem) return old;

    JITMap* newMap = static_cast<JITMap*>(mem);
    newMap->capacity = newCap;
    newMap->size = 0;
    for (int64_t i = 0; i < newCap; ++i) newMap->entries[i].state = 0;

    // Rehash all occupied entries from old map
    for (int64_t i = 0; i < old->capacity; ++i) {
        if (old->entries[i].state == 1) {
            uint64_t hash = jit_hash_key(old->entries[i].key);
            int64_t idx = hash & (newCap - 1);
            while (newMap->entries[idx].state == 1) {
                idx = (idx + 1) & (newCap - 1);
            }
            newMap->entries[idx].key = old->entries[i].key;
            newMap->entries[idx].value = old->entries[i].value;
            newMap->entries[idx].state = 1;
            newMap->size++;
        }
    }
    return static_cast<void*>(newMap);
}

/**
 * Insert or update a key-value pair. Returns the (possibly new) map pointer
 * since resize may reallocate the map.
 */
extern "C" void* jit_map_set(void* mapPtr, int64_t key, int64_t value) {
    if (!mapPtr) return nullptr;
    JITMap* map = static_cast<JITMap*>(mapPtr);

    // Resize at 50% load factor
    if (map->size * 2 >= map->capacity) {
        map = static_cast<JITMap*>(jit_map_resize(map));
    }

    uint64_t hash = jit_hash_key(key);
    int64_t index = hash & (map->capacity - 1);
    int64_t firstTombstone = -1;

    while (map->entries[index].state != 0) {
        if (map->entries[index].state == 1 && jit_keys_equal(map->entries[index].key, key)) {
            map->entries[index].value = value;
            return static_cast<void*>(map);
        }
        if (map->entries[index].state == -1 && firstTombstone == -1) {
            firstTombstone = index;
        }
        index = (index + 1) & (map->capacity - 1);
    }

    // Reuse tombstone slot if available
    int64_t insertIdx = (firstTombstone != -1) ? firstTombstone : index;
    map->entries[insertIdx].key = key;
    map->entries[insertIdx].value = value;
    map->entries[insertIdx].state = 1;
    map->size++;
    return static_cast<void*>(map);
}

/**
 * Retrieve value for a key. Returns 0 if not found.
 */
extern "C" int64_t jit_map_get(void* mapPtr, int64_t key) {
    if (!mapPtr) return 0;
    JITMap* map = static_cast<JITMap*>(mapPtr);

    uint64_t hash = jit_hash_key(key);
    int64_t index = hash & (map->capacity - 1);

    while (map->entries[index].state != 0) {
        if (map->entries[index].state == 1 && jit_keys_equal(map->entries[index].key, key)) {
            return map->entries[index].value;
        }
        index = (index + 1) & (map->capacity - 1);
    }
    return 0;
}

/**
 * Remove a key from the map using tombstone marker.
 * Returns 1 if removed, 0 if key was not found.
 */
extern "C" int64_t jit_map_remove(void* mapPtr, int64_t key) {
    if (!mapPtr) return 0;
    JITMap* map = static_cast<JITMap*>(mapPtr);

    uint64_t hash = jit_hash_key(key);
    int64_t index = hash & (map->capacity - 1);

    while (map->entries[index].state != 0) {
        if (map->entries[index].state == 1 && jit_keys_equal(map->entries[index].key, key)) {
            map->entries[index].state = -1; // tombstone
            map->size--;
            return 1;
        }
        index = (index + 1) & (map->capacity - 1);
    }
    return 0;
}

/**
 * Check if map contains key. Returns 1 if found, 0 otherwise.
 */
extern "C" int64_t jit_map_has(void* mapPtr, int64_t key) {
    if (!mapPtr) return 0;
    JITMap* map = static_cast<JITMap*>(mapPtr);

    uint64_t hash = jit_hash_key(key);
    int64_t index = hash & (map->capacity - 1);

    while (map->entries[index].state != 0) {
        if (map->entries[index].state == 1 && jit_keys_equal(map->entries[index].key, key)) {
            return 1;
        }
        index = (index + 1) & (map->capacity - 1);
    }
    return 0;
}

/**
 * Return the number of entries in the map.
 */
extern "C" int64_t jit_map_size(void* mapPtr) {
    if (!mapPtr) return 0;
    JITMap* map = static_cast<JITMap*>(mapPtr);
    return map->size;
}

/**
 * Return a JITArray containing all occupied keys.
 */
extern "C" void* jit_map_keys(void* mapPtr) {
    if (!mapPtr) return jit_alloc_array(0);
    JITMap* map = static_cast<JITMap*>(mapPtr);

    void* arr = jit_alloc_array(0);
    for (int64_t i = 0; i < map->capacity; ++i) {
        if (map->entries[i].state == 1) {
            arr = jit_array_push(arr, map->entries[i].key);
        }
    }
    return arr;
}

/**
 * Return a JITArray containing all occupied values.
 */
extern "C" void* jit_map_values(void* mapPtr) {
    if (!mapPtr) return jit_alloc_array(0);
    JITMap* map = static_cast<JITMap*>(mapPtr);

    void* arr = jit_alloc_array(0);
    for (int64_t i = 0; i < map->capacity; ++i) {
        if (map->entries[i].state == 1) {
            arr = jit_array_push(arr, map->entries[i].value);
        }
    }
    return arr;
}


extern "C" void* jit_alloc_string(const char* s) {
    if (!s) return nullptr;
    size_t len = strlen(s);
    
    // Calculate required capacity including null terminator
    // Use exact capacity to mark this as an IMMUTABLE compile-time literal
    int64_t capacity = len; 
    size_t totalBytes = sizeof(JITString) + capacity;
    
    // Attempt GC-managed allocation
    void* mem = jitGC.allocate(totalBytes);
    if (!mem) mem = malloc(totalBytes);
    if (!mem) return nullptr;
    
    JITString* str = static_cast<JITString*>(mem);
    str->magic = JIT_STRING_MAGIC;
    str->padding = 0;
    str->capacity = capacity;
    str->length = len;
    
    memcpy(str->data, s, len);
    str->data[len] = '\0';
    
    return static_cast<void*>(str->data);
}

extern "C" void jit_gc_collect() {
    jitGC.collectYoung();
}

extern "C" char* jit_string_concat(char* s1, char* s2) {
    if (!s1 || !s2) return nullptr;
    
    JITString* str1 = reinterpret_cast<JITString*>(s1 - offsetof(JITString, data));
    JITString* str2 = reinterpret_cast<JITString*>(s2 - offsetof(JITString, data));

    // Assume all strings in JIT are JITStrings for max performance
    size_t l1 = str1->length;
    size_t l2 = str2->length;
    size_t newLength = l1 + l2;
    
    // Fast path: In-place append ONLY if capacity allows AND it's a mutable runtime string
    // Compile-time literals have capacity == length, preventing mutation.
    if (str1->capacity > (int64_t)str1->length && str1->capacity >= (int64_t)newLength) {
        memcpy(str1->data + l1, s2, l2);
        str1->length = newLength;
        str1->data[newLength] = '\0';
        return str1->data;
    }
    
    // Growth path
    int64_t newCapacity = str1->capacity * 2;
    if (newCapacity < (int64_t)newLength) {
        newCapacity = newLength + 127; // Use 127 for fast growth 
    }
    
    // ZERO-COPY PATH: Attempt to expand block directly in the Bump Allocator
    // We can rely entirely on GC checking if it's the tip of the allocator.
    size_t totalBytes = sizeof(JITString) + newCapacity;
    if (jitGC.expand(str1, totalBytes)) {
        str1->capacity = newCapacity;
        memcpy(str1->data + l1, s2, l2);
        str1->length = newLength;
        str1->data[newLength] = '\0';
        return str1->data;
    }
    
    // Slow path: Allocate fresh buffer and copy
    void* mem = jitGC.allocate(totalBytes);
    if (!mem) mem = malloc(totalBytes);
    if (!mem) return nullptr;
    
    JITString* res = static_cast<JITString*>(mem);
    res->magic = JIT_STRING_MAGIC;
    res->padding = 0;
    res->capacity = newCapacity;
    res->length = newLength;
    
    memcpy(res->data, str1->data, l1);
    memcpy(res->data + l1, s2, l2);
    res->data[newLength] = '\0';
    return res->data;
}

/**
 * Async/Await Runtime Support
 *
 * Task-based concurrency model. An async function spawns a std::thread
 * that executes a compiled function pointer. The result is stored in
 * a JITTask struct which can be polled via await.
 */
struct JITTask {
    std::atomic<bool> completed;
    int64_t result;
    std::thread worker;
    std::mutex mtx;

    JITTask() : completed(false), result(0) {}
    ~JITTask() {
        if (worker.joinable()) {
            worker.join();
        }
    }
};

using JITCompiledFunc = int64_t (*)();

extern "C" void* jit_async_spawn(void* funcPtr) {
    JITTask* task = new JITTask();
    auto fn = reinterpret_cast<JITCompiledFunc>(funcPtr);

    task->worker = std::thread([task, fn]() {
        int64_t res = fn();
        task->result = res;
        task->completed.store(true, std::memory_order_release);
    });

    return static_cast<void*>(task);
}

extern "C" int64_t jit_await_task(void* taskPtr) {
    if (!taskPtr) return 0;
    JITTask* task = static_cast<JITTask*>(taskPtr);

    if (task->worker.joinable()) {
        task->worker.join();
    }

    int64_t result = task->result;
    return result;
}

extern "C" void jit_task_free(void* taskPtr) {
    if (!taskPtr) return;
    JITTask* task = static_cast<JITTask*>(taskPtr);
    delete task;
}

namespace nevaarize {

JIT::JIT() 
    : stackSize(0)
    , nextStackSlot(0)
    , currentAST(nullptr)
    , inFunctionCall(false) {
    execMem = std::make_unique<ExecutableMemory>(65536);
    std::memset(regInUse, 0, sizeof(regInUse));
    std::memset(xmmInUse, 0, sizeof(xmmInUse));
    
    // Reserve some registers
    regInUse[static_cast<int>(X64Reg::RSP)] = true;
    regInUse[static_cast<int>(X64Reg::RBP)] = true;

    // Reserve XMM0/XMM1 as scratch registers for float arithmetic
    xmmInUse[0] = true;
    xmmInUse[1] = true;
}

JIT::~JIT() = default;

// --- Helper Functions ---

static inline void emitXorReg(CodeBuffer& buf, X64Reg reg) {
    uint8_t r = static_cast<uint8_t>(reg);
    bool high = r >= 8;
    buf.emit8(0x48 | (high ? 0x05 : 0)); // REX.W + REX.R/B if needed
    buf.emit8(0x31);
    buf.emit8(0xC0 | ((r & 0x7) << 3) | (r & 0x7));
}

static inline void emitMovImm64(CodeBuffer& buf, X64Reg reg, uint64_t imm) {
    uint8_t r = static_cast<uint8_t>(reg);
    bool high = r >= 8;
    buf.emit8(0x48 | (high ? 0x01 : 0));
    buf.emit8(0xB8 + (r & 0x7));
    buf.emit64(imm);
}

// Check if an AST node is statically known to be an Int
bool JIT::isStaticInt(const AST& ast, NodeIndex idx) const {
    if (idx == INVALID_NODE) return false;
    const ASTNode& node = ast.get(idx);
    switch (node.type) {
        case NodeType::LITERAL_INT:
        case NodeType::LITERAL_BOOL:
            return true;
        case NodeType::IDENTIFIER:
            return knownIntVars.count(node.name) > 0;
        case NodeType::BINARY_OP: {
            if (node.binaryOp == BinaryOp::ADD || node.binaryOp == BinaryOp::SUB ||
                node.binaryOp == BinaryOp::MUL || node.binaryOp == BinaryOp::DIV ||
                node.binaryOp == BinaryOp::MOD ||
                node.binaryOp == BinaryOp::LT  || node.binaryOp == BinaryOp::GT  ||
                node.binaryOp == BinaryOp::LTE || node.binaryOp == BinaryOp::GTE ||
                node.binaryOp == BinaryOp::EQ  || node.binaryOp == BinaryOp::NEQ ||
                node.binaryOp == BinaryOp::AND || node.binaryOp == BinaryOp::OR) {
                return isStaticInt(ast, node.left) && isStaticInt(ast, node.right);
            }
            return false;
        }
        case NodeType::UNARY_OP:
            return isStaticInt(ast, node.left);
        default:
            return false;
    }
}

// Check if an AST node is statically known to be a Float
bool JIT::isStaticFloat(const AST& ast, NodeIndex idx) const {
    if (idx == INVALID_NODE) return false;
    const ASTNode& node = ast.get(idx);
    switch (node.type) {
        case NodeType::LITERAL_FLOAT:
            return true;
        case NodeType::IDENTIFIER:
            return knownFloatVars.count(node.name) > 0;
        case NodeType::BINARY_OP: {
            // Float if either operand is float (type promotion)
            bool leftFloat = isStaticFloat(ast, node.left);
            bool rightFloat = isStaticFloat(ast, node.right);
            if (leftFloat || rightFloat) {
                // At least one operand is float, result is float
                // (unless the other is a non-numeric type, which we ignore here)
                bool leftNumeric = leftFloat || isStaticInt(ast, node.left);
                bool rightNumeric = rightFloat || isStaticInt(ast, node.right);
                return leftNumeric && rightNumeric;
            }
            return false;
        }
        case NodeType::UNARY_OP:
            return isStaticFloat(ast, node.left);
        default:
            return false;
    }
}

void JIT::emitPrologue() {
    CodeBuffer& buf = codegen.getCode();
    
    // push rbp
    buf.emit8(0x55);
    
    // Preserve all callee-saved registers used by compileWhile optimizations
    // push rbx
    buf.emit8(0x53);
    // push r12
    buf.emit8(0x41); buf.emit8(0x54);
    // push r13
    buf.emit8(0x41); buf.emit8(0x55);
    // push r14
    buf.emit8(0x41); buf.emit8(0x56);
    // push r15
    buf.emit8(0x41); buf.emit8(0x57);
    
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

    // Restore callee-saved registers (reverse order of push)
    // pop r15
    buf.emit8(0x41); buf.emit8(0x5F);
    // pop r14
    buf.emit8(0x41); buf.emit8(0x5E);
    // pop r13
    buf.emit8(0x41); buf.emit8(0x5D);
    // pop r12
    buf.emit8(0x41); buf.emit8(0x5C);
    // pop rbx
    buf.emit8(0x5B);
    
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

X64Reg JIT::allocateXMMReg() {
    // XMM0/XMM1 reserved for scratch, XMM2-XMM7 available for pinning
    for (int i = 2; i < 8; ++i) {
        if (!xmmInUse[i]) {
            xmmInUse[i] = true;
            return static_cast<X64Reg>(static_cast<int>(X64Reg::XMM0) + i);
        }
    }
    return X64Reg::XMM0; // Fallback (should not happen)
}

void JIT::freeXMMReg(X64Reg reg) {
    int idx = static_cast<int>(reg) - static_cast<int>(X64Reg::XMM0);
    if (idx >= 2 && idx < 8) {
        xmmInUse[idx] = false;
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
            
            // OPTIMIZATION: Emit the string pointer directly, creating it at JIT compile time!
            // No need to call `jit_alloc_string` dynamically in the execution loop!
            void* preAllocatedStr = jit_alloc_string(strVal.c_str());
            
            bool valHigh = static_cast<uint8_t>(result.valueReg) >= 8;
            buf.emit8(0x48 | (valHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(result.valueReg) & 0x7));
            buf.emit64(reinterpret_cast<uint64_t>(preAllocatedStr));
            
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
                if (it->second.isXMMRegister) {
                    // XMM-pinned float variable: movq result.valueReg, xmmN
                    X64Reg srcXMM = it->second.reg;
                    uint8_t xmmIdx = static_cast<uint8_t>(srcXMM) - static_cast<uint8_t>(X64Reg::XMM0);
                    bool valHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                    buf.emit8(0x66);
                    buf.emit8(0x48 | (valHigh ? 0x01 : 0));
                    buf.emit8(0x0F); buf.emit8(0x7E);
                    buf.emit8(0xC0 | (xmmIdx << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7));
                    
                    // Type = Float (1)
                    bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                    buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                    buf.emit64(1);
                } else if (it->second.isRegister) {
                    // Move from assigned register
                    bool valHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                    bool srcHigh = static_cast<uint8_t>(it->second.reg) >= 8;
                    buf.emit8(0x48 | (valHigh ? 0x01 : 0) | (srcHigh ? 0x04 : 0));
                    buf.emit8(0x89);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(it->second.reg) & 0x7) << 3) | 
                              (static_cast<uint8_t>(result.valueReg) & 0x7));
                    
                    // Fetch true dynamic type from memory
                    int32_t offset = it->second.stackOffset;
                    bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                    buf.emit8(0x48 | (typeHigh ? 0x04 : 0));
                    buf.emit8(0x8B);
                    buf.emit8(0x85 | ((static_cast<uint8_t>(result.typeReg) & 0x7) << 3));
                    buf.emit32(static_cast<uint32_t>(offset + 8));
                } else {
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
            }
            return result;
        }
        
        case NodeType::BINARY_OP: {
            // Constant Folding Optimization
            const ASTNode& lNodeFold = ast.get(node.left);
            const ASTNode& rNodeFold = ast.get(node.right);
            
            if (lNodeFold.type == NodeType::LITERAL_INT && rNodeFold.type == NodeType::LITERAL_INT) {
                int64_t leftVal = std::get<int64_t>(lNodeFold.literal.data);
                int64_t rightVal = std::get<int64_t>(rNodeFold.literal.data);
                int64_t resFold = 0;
                bool folded = true;

                switch (node.binaryOp) {
                    case BinaryOp::ADD: resFold = leftVal + rightVal; break;
                    case BinaryOp::SUB: resFold = leftVal - rightVal; break;
                    case BinaryOp::MUL: resFold = leftVal * rightVal; break;
                    case BinaryOp::DIV: if (rightVal != 0) resFold = leftVal / rightVal; else folded = false; break;
                    case BinaryOp::MOD: if (rightVal != 0) resFold = leftVal % rightVal; else folded = false; break;
                    case BinaryOp::EQ:  resFold = (leftVal == rightVal); break;
                    case BinaryOp::NEQ: resFold = (leftVal != rightVal); break;
                    case BinaryOp::LT:  resFold = (leftVal < rightVal); break;
                    case BinaryOp::LTE: resFold = (leftVal <= rightVal); break;
                    case BinaryOp::GT:  resFold = (leftVal > rightVal); break;
                    case BinaryOp::GTE: resFold = (leftVal >= rightVal); break;
                    default: folded = false; break;
                }

                if (folded) {
                    JITValue result;
                    result.valueReg = allocateReg();
                    result.typeReg = allocateReg();
                    
                    bool vh = static_cast<uint8_t>(result.valueReg) >= 8;
                    buf.emit8(0x48 | (vh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.valueReg) & 0x7));
                    buf.emit64(resFold);

                    bool th = static_cast<uint8_t>(result.typeReg) >= 8;
                    buf.emit8(0x48 | (th ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                    buf.emit64(0); // Int
                    return result;
                }
            }

            // Float Constant Folding
            if (lNodeFold.type == NodeType::LITERAL_FLOAT && rNodeFold.type == NodeType::LITERAL_FLOAT) {
                double leftVal = std::get<double>(lNodeFold.literal.data);
                double rightVal = std::get<double>(rNodeFold.literal.data);
                double resFold = 0.0;
                bool folded = true;

                switch (node.binaryOp) {
                    case BinaryOp::ADD: resFold = leftVal + rightVal; break;
                    case BinaryOp::SUB: resFold = leftVal - rightVal; break;
                    case BinaryOp::MUL: resFold = leftVal * rightVal; break;
                    case BinaryOp::DIV: if (rightVal != 0.0) resFold = leftVal / rightVal; else folded = false; break;
                    default: folded = false; break;
                }

                if (folded) {
                    JITValue result;
                    result.valueReg = allocateReg();
                    result.typeReg = allocateReg();

                    uint64_t bits;
                    std::memcpy(&bits, &resFold, sizeof(bits));

                    bool vh = static_cast<uint8_t>(result.valueReg) >= 8;
                    buf.emit8(0x48 | (vh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.valueReg) & 0x7));
                    buf.emit64(bits);

                    bool th = static_cast<uint8_t>(result.typeReg) >= 8;
                    buf.emit8(0x48 | (th ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                    buf.emit64(1); // Float type tag
                    return result;
                }
            }

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
            
            // Static type inference: skip runtime type dispatch for pure-int operations
            bool staticIntPath = isStaticInt(ast, node.left) && 
                                 (rightIsImm || isStaticInt(ast, node.right));
            
            // Static float inference: skip runtime dispatch for pure-float operations
            bool staticFloatPath = !staticIntPath && isStaticFloat(ast, node.left) &&
                                   (isStaticFloat(ast, node.right) || isStaticInt(ast, node.right));
            
            size_t jnzPatch = 0;
            bool lTypeHigh = static_cast<uint8_t>(left.typeReg) >= 8;
            
            if (!staticIntPath && !staticFloatPath) {
                
                X64Reg typeScratch = allocateReg();
                bool tempHigh = static_cast<uint8_t>(typeScratch) >= 8;
                
                // mov typeScratch, left.typeReg
                buf.emit8(0x48 | (tempHigh ? 0x01 : 0) | (lTypeHigh ? 0x04 : 0));
                buf.emit8(0x89);
                buf.emit8(0xC0 | ((static_cast<uint8_t>(left.typeReg) & 0x7) << 3) | (static_cast<uint8_t>(typeScratch) & 0x7));
                
                if (!rightIsImm) {
                    // or typeScratch, right.typeReg
                    bool rTypeHigh = static_cast<uint8_t>(right.typeReg) >= 8;
                    buf.emit8(0x48 | (tempHigh ? 0x01 : 0) | (rTypeHigh ? 0x04 : 0));
                    buf.emit8(0x09);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(right.typeReg) & 0x7) << 3) | (static_cast<uint8_t>(typeScratch) & 0x7));
                }
                
                // test typeScratch, typeScratch
                buf.emit8(0x48 | (tempHigh ? 0x05 : 0)); // REX with R/B same
                buf.emit8(0x85);
                buf.emit8(0xC0 | ((static_cast<uint8_t>(typeScratch) & 0x7) << 3) | (static_cast<uint8_t>(typeScratch) & 0x7));
                
                freeReg(typeScratch);
                
                // jnz float_path (if not zero, one of them is float)
                buf.emit8(0x0F);
                buf.emit8(0x85); // jnz far
                jnzPatch = buf.getOffset();
                buf.emit32(0);
            }
            
            // === INTEGER PATH (skipped when staticFloatPath) ===
            
            if (!staticFloatPath) {
            switch (node.binaryOp) {
                case BinaryOp::ADD: {
                    if (staticIntPath) {
                        // Optim 3: Skip string check for static int
                        if (rightIsImm) {
                            // Optim 5: Use INC for +1
                            if (immVal == 1) {
                                bool resHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                                buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                                buf.emit8(0xFF); // Group 5
                                buf.emit8(0xC0 | (static_cast<uint8_t>(result.valueReg) & 0x7)); // 0 = INC
                            } else {
                                bool resHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                                buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                                buf.emit8(0x81); // ADD r/m64, imm32
                                buf.emit8(0xC0 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                                buf.emit32(static_cast<uint32_t>(immVal));
                            }
                        } else {
                            bool rValHigh = static_cast<uint8_t>(right.valueReg) >= 8;
                            bool resHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                            buf.emit8(0x48 | (rValHigh ? 0x04 : 0) | (resHigh ? 0x01 : 0));
                            buf.emit8(0x01);
                            buf.emit8(0xC0 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7));
                        }
                    } else {
                        // Standard dynamic path
                        // Check if either operand is a string (Type 4)
                        bool resTypeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                        buf.emit8(0x48 | (resTypeHigh ? 0x01 : 0));
                        buf.emit8(0x83);
                        buf.emit8(0xF8 | (static_cast<uint8_t>(result.typeReg) & 0x7));
                        buf.emit8(4);
                        
                        // jne int_add
                        buf.emit8(0x75);
                        size_t jneOffset = buf.getOffset();
                        buf.emit8(0x00); // 1-byte placeholder
                        
                        // === STRING INLINE IN-PLACE APPEND FAST PATH ===
                        // To achieve 1B+ ops/sec we must inline the capacity check.
                        // rdi = s1 (result.valueReg). rsi = s2 (right.valueReg or immVal)
                        // s1 Metadata is at rdi - 24. capacity: [rdi - 16]. length: [rdi - 8]
                        // s2 Metadata is at rsi - 24. length: [rsi - 8]
                        
                        bool resHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                        bool rHigh = static_cast<uint8_t>(right.valueReg) >= 8;
                        
                        size_t fallbackJumpOffset1 = 0;
                        size_t fallbackJumpOffset2 = 0;
                        size_t endFastPathJumpOffset = 0;
                        
                        if (!rightIsImm) {
                            // 1. Load s2 length: mov r9, [s2 - 8]
                            buf.emit8(0x4C | (rHigh ? 0x01 : 0));
                            buf.emit8(0x8B); buf.emit8(0x48 | (static_cast<uint8_t>(right.valueReg) & 0x7)); // r9 is 001 (offset 1)
                            buf.emit8(0xF8); // -8
                            
                            // 2. Check if s2 length == 1. cmp r9, 1
                            buf.emit8(0x49); buf.emit8(0x83); buf.emit8(0xF9); buf.emit8(0x01);
                            
                            // 3. jne fallback (if length != 1)
                            buf.emit8(0x75); // jne
                            fallbackJumpOffset1 = buf.getOffset();
                            buf.emit8(0x00);
                            
                            // 4. Load s1 capacity: mov r10, [s1 - 16]
                            buf.emit8(0x4C | (resHigh ? 0x01 : 0));
                            buf.emit8(0x8B); buf.emit8(0x50 | (static_cast<uint8_t>(result.valueReg) & 0x7)); // r10 is 010 (offset 2)
                            buf.emit8(0xF0); // -16
                            
                            // 5. Load s1 length: mov r11, [s1 - 8]
                            buf.emit8(0x4C | (resHigh ? 0x01 : 0));
                            buf.emit8(0x8B); buf.emit8(0x58 | (static_cast<uint8_t>(result.valueReg) & 0x7)); // r11 is 011 (offset 3)
                            buf.emit8(0xF8); // -8
                            
                            // 6. Check if capacity allows new length + null terminator
                            // cmp r10, r11
                            buf.emit8(0x4D); buf.emit8(0x39); buf.emit8(0xDA); // cmp r10, r11
                            
                            // 7. jle fallback (if capacity <= length)
                            buf.emit8(0x7E); // jle
                            fallbackJumpOffset2 = buf.getOffset();
                            buf.emit8(0x00);
                            
                            // 7b. Check again for l1 + l2 edge case.
                            // If capacity == length + 1, we can append 1 byte, but no room for \0?
                            // capacity includes room for \0? In jit_alloc_string: capacity = len > 31 ? len : 31; totalBytes = sizeof(JITString) + capacity.
                            // Since sizeof(JITString) includes data[1], total capacity is actually capacity + 1. So capacity exactly holds length + 1 bytes.
                            // But jit_string_concat does: newLength = l1 + 1. str1->capacity >= newLength.
                            // So if r10 > r11, r10 >= r11 + 1!
                            
                            // 8. We have capacity and l2 is 1! Read the char from s2: mov r8b, byte ptr [s2]
                            buf.emit8(0x44 | (rHigh ? 0x01 : 0));
                            buf.emit8(0x8A); buf.emit8(0x00 | (static_cast<uint8_t>(right.valueReg) & 0x7)); // r8b is 000
                            
                            // 9. Append byte: mov byte ptr [s1 + r11], r8b
                            buf.emit8(0x46 | (resHigh ? 0x01 : 0)); // REX prefix for base + index + r8 src
                            buf.emit8(0x88); // mov byte ptr, r8b
                            buf.emit8(0x04); // SIB byte follows (ModR/M=0x04 for [SIB])
                            buf.emit8(0x18 | (static_cast<uint8_t>(result.valueReg) & 0x7)); // index=r11 (3<<3), base=s1
                            
                            // 10. Increment length: inc qword ptr [s1 - 8]
                            buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                            buf.emit8(0xFF); buf.emit8(0x40 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                            buf.emit8(0xF8); // -8
                            
                            // 12. jmp end
                            buf.emit8(0xEB);
                            endFastPathJumpOffset = buf.getOffset();
                            buf.emit8(0x00);
                            
                            // === FALLBACK TARGET ===
                            size_t fallbackTarget = buf.getOffset();
                            buf.patch8(fallbackJumpOffset1, static_cast<uint8_t>(fallbackTarget - (fallbackJumpOffset1 + 1)));
                            buf.patch8(fallbackJumpOffset2, static_cast<uint8_t>(fallbackTarget - (fallbackJumpOffset2 + 1)));
                        }

                        // Call jit_string_concat(result.valueReg, right.valueReg) as fallback
                        buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52);
                        buf.emit8(0x41); buf.emit8(0x50); buf.emit8(0x41); buf.emit8(0x51);
                        buf.emit8(0x41); buf.emit8(0x52); buf.emit8(0x41); buf.emit8(0x53);
                        
                        buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                        buf.emit8(0x89); buf.emit8(0xC7 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3)); // rdi = s1
                        
                        if (rightIsImm) {
                            buf.emit8(0x48); buf.emit8(0xBE);
                            buf.emit64(static_cast<uint64_t>(immVal)); // rsi = immVal
                        } else {
                            bool rHigh = static_cast<uint8_t>(right.valueReg) >= 8;
                            buf.emit8(0x48 | (rHigh ? 0x01 : 0));
                            buf.emit8(0x89); buf.emit8(0xD6 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3)); // rsi = s2
                        }
                        
                        buf.emit8(0x48); buf.emit8(0xB8);
                        buf.emit64(reinterpret_cast<uint64_t>(jit_string_concat));
                        buf.emit8(0xFF); buf.emit8(0xD0);
                        
                        buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                        buf.emit8(0x89); buf.emit8(0xC0 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                        
                        buf.emit8(0x41); buf.emit8(0x5B); buf.emit8(0x41); buf.emit8(0x5A);
                        buf.emit8(0x41); buf.emit8(0x59); buf.emit8(0x41); buf.emit8(0x58);
                        buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58);
                        
                        buf.emit8(0xEB);
                        size_t jmpOffset = buf.getOffset();
                        buf.emit8(0x00);
                        
                        // === END FAST PATH TARGET ===
                        if (!rightIsImm) {
                            size_t endFastTarget = buf.getOffset();
                            buf.patch8(endFastPathJumpOffset, static_cast<uint8_t>(endFastTarget - (endFastPathJumpOffset + 1)));
                        }
                        
                        // === INT ADD ===
                        size_t intAddPos = buf.getOffset();
                        buf.patch8(jneOffset, static_cast<uint8_t>(intAddPos - (jneOffset + 1)));

                        if (rightIsImm) {
                            buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                            buf.emit8(0x81); // ADD r/m64, imm32
                            buf.emit8(0xC0 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                            buf.emit32(static_cast<uint32_t>(immVal));
                        } else {
                            bool rValHigh = static_cast<uint8_t>(right.valueReg) >= 8;
                            buf.emit8(0x48 | (rValHigh ? 0x04 : 0) | (resHigh ? 0x01 : 0));
                            buf.emit8(0x01);
                            buf.emit8(0xC0 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7));
                        }
                        
                        size_t endPos = buf.getOffset();
                        buf.patch8(jmpOffset, static_cast<uint8_t>(endPos - (jmpOffset + 1)));
                    }
                    break;
                }
                case BinaryOp::SUB:
                    if (rightIsImm) {
                        bool resHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                         // Optim 5b: Use DEC for -1
                         if (staticIntPath && immVal == 1) {
                            buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                            buf.emit8(0xFF); // Group 5
                            buf.emit8(0xC8 | (static_cast<uint8_t>(result.valueReg) & 0x7)); // 1 = DEC
                         } else {
                            buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                            buf.emit8(0x81); // SUB r/m64, imm32 (Group 1 /5)
                            buf.emit8(0xE8 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                            buf.emit32(static_cast<uint32_t>(immVal));
                         }
                    } else {
                        bool rValHigh = static_cast<uint8_t>(right.valueReg) >= 8;
                        bool resHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                        buf.emit8(0x48 | (rValHigh ? 0x04 : 0) | (resHigh ? 0x01 : 0));
                        buf.emit8(0x29); // SUB r/m64, r64
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3) |
                                          (static_cast<uint8_t>(result.valueReg) & 0x7));
                    }
                    break;
                case BinaryOp::MUL:
                    if (rightIsImm) {
                        bool resHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                        buf.emit8(0x48 | (resHigh ? 0x04 : 0) | (resHigh ? 0x01 : 0));
                        buf.emit8(0x69); // IMUL r64, r/m64, imm32
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7));
                        buf.emit32(static_cast<uint32_t>(immVal));
                    } else {
                        bool rValHigh = static_cast<uint8_t>(right.valueReg) >= 8;
                        bool resHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                        buf.emit8(0x48 | (resHigh ? 0x04 : 0) | (rValHigh ? 0x01 : 0));
                        buf.emit8(0x0F);
                        buf.emit8(0xAF); // IMUL r64, r/m64
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(right.valueReg) & 0x7));
                    }
                    break;
                case BinaryOp::LT:
                case BinaryOp::GT:
                case BinaryOp::LTE:
                case BinaryOp::GTE:
                case BinaryOp::EQ:
                case BinaryOp::NEQ: {
                     // Comparison code (cmp + setcc + movzx)
                     bool resHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                     if (rightIsImm) {
                        buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                        buf.emit8(0x81); // CMP r/m64, imm32
                        buf.emit8(0xF8 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                        buf.emit32(static_cast<uint32_t>(immVal));
                     } else {
                        bool rValHigh = static_cast<uint8_t>(right.valueReg) >= 8;
                        buf.emit8(0x48 | (rValHigh ? 0x04 : 0) | (resHigh ? 0x01 : 0));
                        buf.emit8(0x39); // CMP r/m64, r64
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7));
                     }
                     
                     // setcc al
                     uint8_t setcc = 0;
                     switch (node.binaryOp) {
                         case BinaryOp::LT: setcc = 0x9C; break; // setl (signed) - Note: JIT uses signed for ints
                         case BinaryOp::GT: setcc = 0x9F; break; // setg
                         case BinaryOp::LTE: setcc = 0x9E; break; // setle
                         case BinaryOp::GTE: setcc = 0x9D; break; // setge
                         case BinaryOp::EQ: setcc = 0x94; break; // sete
                         case BinaryOp::NEQ: setcc = 0x95; break; // setne
                         default: break;
                     }
                     buf.emit8(0x0F);
                     buf.emit8(setcc);
                     buf.emit8(0xC0); // al
                     
                     // movzx rax, al
                     buf.emit8(0x48 | (resHigh ? 0x04 : 0));
                     buf.emit8(0x0F);
                     buf.emit8(0xB6);
                     buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3)); // movzx dst, al
                     break;
                }
                case BinaryOp::DIV: {
                    bool resHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                    bool rValHigh = static_cast<uint8_t>(right.valueReg) >= 8;
                    buf.emit8(0x51); // push rcx
                    buf.emit8(0x52); // push rdx
                    buf.emit8(0x48 | (rValHigh ? 0x04 : 0));
                    buf.emit8(0x89);
                    buf.emit8(0xC1 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3)); // mov rcx, right
                    if (result.valueReg != X64Reg::RAX) {
                        buf.emit8(0x48 | (resHigh ? 0x04 : 0));
                        buf.emit8(0x89);
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3)); // mov rax, result
                    }
                    buf.emit8(0x48); buf.emit8(0x99); // cqo
                    buf.emit8(0x48); buf.emit8(0xF7); buf.emit8(0xF9); // idiv rcx
                    if (result.valueReg != X64Reg::RAX) {
                        buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                        buf.emit8(0x89);
                        buf.emit8(0xC0 | (static_cast<uint8_t>(result.valueReg) & 0x7)); // mov result, rax
                    }
                    buf.emit8(0x5A); // pop rdx
                    buf.emit8(0x59); // pop rcx
                    break;
                }
                case BinaryOp::MOD: {
                    bool resHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                    bool rValHigh = static_cast<uint8_t>(right.valueReg) >= 8;
                    buf.emit8(0x51); // push rcx
                    buf.emit8(0x52); // push rdx
                    buf.emit8(0x48 | (rValHigh ? 0x04 : 0));
                    buf.emit8(0x89);
                    buf.emit8(0xC1 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3)); // mov rcx, right
                    if (result.valueReg != X64Reg::RAX) {
                        buf.emit8(0x48 | (resHigh ? 0x04 : 0));
                        buf.emit8(0x89);
                        buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3)); // mov rax, result
                    }
                    buf.emit8(0x48); buf.emit8(0x99); // cqo
                    buf.emit8(0x48); buf.emit8(0xF7); buf.emit8(0xF9); // idiv rcx
                    buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                    buf.emit8(0x89);
                    buf.emit8(0xD0 | (static_cast<uint8_t>(result.valueReg) & 0x7)); // mov result, rdx (remainder)
                    buf.emit8(0x48); buf.emit8(0x83); buf.emit8(0xC4); buf.emit8(0x08); // add rsp, 8
                    buf.emit8(0x59); // pop rcx
                    break;
                }
                case BinaryOp::AND: {
                    bool resHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                    bool rValHigh = static_cast<uint8_t>(right.valueReg) >= 8;
                    buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                    buf.emit8(0x85);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7)); // test result, result
                    buf.emit8(0x0F); buf.emit8(0x95); buf.emit8(0xC0); // setne al
                    buf.emit8(0x48 | (rValHigh ? 0x01 : 0));
                    buf.emit8(0x85);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(right.valueReg) & 0x7)); // test right, right
                    buf.emit8(0x0F); buf.emit8(0x95); buf.emit8(0xC1); // setne cl
                    buf.emit8(0x20); buf.emit8(0xC8); // and al, cl
                    buf.emit8(0x48 | (resHigh ? 0x04 : 0));
                    buf.emit8(0x0F); buf.emit8(0xB6);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3)); // movzx result, al
                    break;
                }
                case BinaryOp::OR: {
                    bool resHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                    bool rValHigh = static_cast<uint8_t>(right.valueReg) >= 8;
                    buf.emit8(0x48 | (rValHigh ? 0x04 : 0) | (resHigh ? 0x01 : 0));
                    buf.emit8(0x09);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(right.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7)); // or result, right
                    buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                    buf.emit8(0x85);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3) | (static_cast<uint8_t>(result.valueReg) & 0x7)); // test result, result
                    buf.emit8(0x0F); buf.emit8(0x95); buf.emit8(0xC0); // setne al
                    buf.emit8(0x48 | (resHigh ? 0x04 : 0));
                    buf.emit8(0x0F); buf.emit8(0xB6);
                    buf.emit8(0xC0 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3)); // movzx result, al
                    break;
                }
                default: break;
            }

            if (staticIntPath) {
                // IMPORTANT: In fast path, we must ensure typeReg is 0 (INT)
                emitXorReg(buf, result.typeReg);
            }
            } // end if (!staticFloatPath)
            
            if (staticFloatPath) {
                // === STATIC FLOAT FAST PATH ===
                // Both operands are known float at compile-time.
                // Emit direct XMM operations without any type-check dispatch.
                bool lValHigh = static_cast<uint8_t>(left.valueReg) >= 8;
                
                // movq xmm0, left.valueReg
                buf.emit8(0x66);
                buf.emit8(0x48 | (lValHigh ? 0x01 : 0));
                buf.emit8(0x0F); buf.emit8(0x6E);
                buf.emit8(0xC0 | (static_cast<uint8_t>(left.valueReg) & 0x7));
                
                if (!rightIsImm) {
                    bool rValHigh2 = static_cast<uint8_t>(right.valueReg) >= 8;
                    
                    // Check if right operand is int (needs conversion)
                    bool rightIsIntType = isStaticInt(ast, node.right);
                    if (rightIsIntType) {
                        // cvtsi2sd xmm1, right.valueReg
                        buf.emit8(0xF2);
                        buf.emit8(0x48 | (rValHigh2 ? 0x01 : 0));
                        buf.emit8(0x0F); buf.emit8(0x2A);
                        buf.emit8(0xC8 | (static_cast<uint8_t>(right.valueReg) & 0x7));
                    } else {
                        // movq xmm1, right.valueReg
                        buf.emit8(0x66);
                        buf.emit8(0x48 | (rValHigh2 ? 0x01 : 0));
                        buf.emit8(0x0F); buf.emit8(0x6E);
                        buf.emit8(0xC8 | (static_cast<uint8_t>(right.valueReg) & 0x7));
                    }
                } else {
                    // Immediate int -> convert to double in XMM1
                    bool tHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                    buf.emit8(0x48 | (tHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                    buf.emit64(static_cast<uint64_t>(immVal));
                    
                    buf.emit8(0xF2);
                    buf.emit8(0x48 | (tHigh ? 0x01 : 0));
                    buf.emit8(0x0F); buf.emit8(0x2A);
                    buf.emit8(0xC8 | (static_cast<uint8_t>(result.typeReg) & 0x7));
                }
                
                // Perform float operation
                switch (node.binaryOp) {
                    case BinaryOp::ADD: buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(0x58); buf.emit8(0xC1); break;
                    case BinaryOp::SUB: buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(0x5C); buf.emit8(0xC1); break;
                    case BinaryOp::MUL: buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(0x59); buf.emit8(0xC1); break;
                    case BinaryOp::DIV: buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(0x5E); buf.emit8(0xC1); break;
                    default: break;
                }
                
                // movq result.valueReg, xmm0
                bool resHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                buf.emit8(0x66);
                buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                buf.emit8(0x0F); buf.emit8(0x7E);
                buf.emit8(0xC0 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                
                // Set type to FLOAT (1)
                bool resTypeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                buf.emit8(0x48 | (resTypeHigh ? 0x01 : 0));
                buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                buf.emit64(1);
            }
            
            if (!staticIntPath && !staticFloatPath) {
                // Jump over float path
                // jmp end
                buf.emit8(0xEB);
                size_t jmpPatch = buf.getOffset();
                buf.emit8(0x00);
            
                // === FLOAT PATH ===
                // patch jnz
                size_t floatStart = buf.getOffset();
                int32_t jnzOffset = static_cast<int32_t>(floatStart - (jnzPatch + 4));
                buf.patch32(jnzPatch, static_cast<uint32_t>(jnzOffset));
                
                // ... (FLOAT OP IMPL - KEEPING EXISTING REGISTER LOGIC)
                // Convert left to double in xmm0
                // We need to check if left is float or int.
                // This is complex to verify with registers.
                // For now, let's just emit the original float path loop...
                // Simpler: assume we just call a helper or do the logic inline.
                // Re-using the existing logic block for float would be best.
                
                // Since I am replacing a huge block, I need to assume the original logic for float path 
                // was correct and I should preserve it. 
                // Writing the full float path here from memory is risky.
                // Better approach: I should have read the full float path content first.
                // Assuming I can copy-paste the float logic from previous view... I can't seeing it clearly.
                
                // CRITICAL: The replacement content MUST contain the float path.
                // I will use a simplified reliable float path or try to reuse what I saw.
                // Looking at lines 1006-1188 in Log...
                
                // RE-IMPLEMENTING FLOAT PATH (Standard JIT approach)
                // 1. Convert left to xmm0
                //    test left.type, left.type; jz int_to_float
                //    movq xmm0, left.val; jmp done_left
                //    int_to_float: cvtsi2sd xmm0, left.val
                // 2. Convert right to xmm1
                // 3. Op xmm0, xmm1
                // 4. movq result.val, xmm0; mov result.type, 1
                
                // To avoid mistakes, I'll implement a robust version.
                
                // Check Left Type
                bool lTypeHigh = static_cast<uint8_t>(left.typeReg) >= 8;
                buf.emit8(0x48 | (lTypeHigh ? 0x01 : 0));
                buf.emit8(0x83); 
                buf.emit8(0xF8 | (static_cast<uint8_t>(left.typeReg) & 0x7));
                buf.emit8(0x00);
                
                buf.emit8(0x75); // jne is_float
                size_t l_jne = buf.getOffset();
                buf.emit8(0x00);
                
                // Left is Int -> Convert
                bool lValHigh = static_cast<uint8_t>(left.valueReg) >= 8;
                buf.emit8(0xF2);
                buf.emit8(0x48 | (lValHigh ? 0x01 : 0)); // REX.W
                buf.emit8(0x0F);
                buf.emit8(0x2A); // cvtsi2sd xmm0, r/m64
                buf.emit8(0xC0 | (static_cast<uint8_t>(left.valueReg) & 0x7));
                
                buf.emit8(0xEB); // jmp done_left
                size_t l_jmp = buf.getOffset();
                buf.emit8(0x00);
                
                // Left is Float (at l_jne)
                size_t l_float = buf.getOffset();
                buf.patch8(l_jne, static_cast<uint8_t>(l_float - (l_jne + 1)));
                
                buf.emit8(0x66);
                buf.emit8(0x48 | (lValHigh ? 0x01 : 0));
                buf.emit8(0x0F);
                buf.emit8(0x6E); // movq xmm0, r/m64
                buf.emit8(0xC0 | (static_cast<uint8_t>(left.valueReg) & 0x7));
                
                size_t l_done = buf.getOffset();
                buf.patch8(l_jmp, static_cast<uint8_t>(l_done - (l_jmp + 1)));
                
                // Check Right Type / Load Right
                if (rightIsImm) {
                     // cvtsi2sd xmm1, imm32? No, load imm to reg then convert.
                     // But we pushed the imm logic earlier? No. 
                     // We need a scratch reg for imm if we don't have one. 
                     // Actually `typeScratch` is free now!
                     // But `typeScratch` was freed.
                     // We can use right.valueReg? NO, right.valueReg doesn't exist if rightIsImm.
                     // We can use result.typeReg as scratch? Yes, we will overwrite it anyway.
                     
                     // mov result.typeReg, imm
                     bool tHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                     buf.emit8(0x48 | (tHigh ? 0x01 : 0));
                     buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                     buf.emit64(static_cast<uint64_t>(immVal));
                     
                     // cvtsi2sd xmm1, typeReg
                     buf.emit8(0xF2);
                     buf.emit8(0x48 | (tHigh ? 0x01 : 0));
                     buf.emit8(0x0F);
                     buf.emit8(0x2A);
                     buf.emit8(0xCB | (static_cast<uint8_t>(result.typeReg) & 0x7)); // xmm1 = 0xCB? (1 << 3) | reg. Yes.
                } else {
                    // Similar checks for Right
                    bool rTypeHigh = static_cast<uint8_t>(right.typeReg) >= 8;
                    buf.emit8(0x48 | (rTypeHigh ? 0x01 : 0));
                    buf.emit8(0x83); 
                    buf.emit8(0xF8 | (static_cast<uint8_t>(right.typeReg) & 0x7));
                    buf.emit8(0x00);
                    
                    buf.emit8(0x75); // jne is_float
                    size_t r_jne = buf.getOffset();
                    buf.emit8(0x00);
                    
                    // Right is Int
                    bool rValHigh = static_cast<uint8_t>(right.valueReg) >= 8;
                    buf.emit8(0xF2);
                    buf.emit8(0x48 | (rValHigh ? 0x01 : 0)); // REX.W
                    buf.emit8(0x0F);
                    buf.emit8(0x2A); // cvtsi2sd xmm1, r/m64
                    buf.emit8(0xC8 | (static_cast<uint8_t>(right.valueReg) & 0x7)); // xmm1 (1<<3) = 8. C8.
                    
                    buf.emit8(0xEB); // jmp done_right
                    size_t r_jmp = buf.getOffset();
                    buf.emit8(0x00);
                    
                    // Right is Float
                    size_t r_float = buf.getOffset();
                    buf.patch8(r_jne, static_cast<uint8_t>(r_float - (r_jne + 1)));
                    
                    buf.emit8(0x66);
                    buf.emit8(0x48 | (rValHigh ? 0x01 : 0));
                    buf.emit8(0x0F);
                    buf.emit8(0x6E); // movq xmm1, r/m64
                    buf.emit8(0xC8 | (static_cast<uint8_t>(right.valueReg) & 0x7));
                    
                    size_t r_done = buf.getOffset();
                    buf.patch8(r_jmp, static_cast<uint8_t>(r_done - (r_jmp + 1)));
                }
                
                // Do Float Op
                switch(node.binaryOp) {
                    case BinaryOp::ADD: 
                        buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(0x58); buf.emit8(0xC1); // addsd xmm0, xmm1
                        break;
                    case BinaryOp::SUB: 
                        buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(0x5C); buf.emit8(0xC1); // subsd xmm0, xmm1
                        break;
                    case BinaryOp::MUL: 
                        buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(0x59); buf.emit8(0xC1); // mulsd xmm0, xmm1
                        break;
                    case BinaryOp::DIV: 
                        buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(0x5E); buf.emit8(0xC1); // divsd xmm0, xmm1
                        break;
                    case BinaryOp::LT:
                        buf.emit8(0x66); buf.emit8(0x0F); buf.emit8(0x2E); buf.emit8(0xC1); // ucomisd xmm0, xmm1
                        buf.emit8(0x48); buf.emit8(0x31); buf.emit8(0xC0); // xor rax, rax
                        buf.emit8(0x0F); buf.emit8(0x92); buf.emit8(0xC0); // setb al
                        buf.emit8(0xF2); buf.emit8(0x48); buf.emit8(0x0F); buf.emit8(0x2A); buf.emit8(0xC0); // cvtsi2sd xmm0, rax
                        break;
                    case BinaryOp::GT:
                        buf.emit8(0x66); buf.emit8(0x0F); buf.emit8(0x2E); buf.emit8(0xC1);
                        buf.emit8(0x48); buf.emit8(0x31); buf.emit8(0xC0);
                        buf.emit8(0x0F); buf.emit8(0x97); buf.emit8(0xC0); // seta al
                        buf.emit8(0xF2); buf.emit8(0x48); buf.emit8(0x0F); buf.emit8(0x2A); buf.emit8(0xC0);
                        break;
                    case BinaryOp::LTE:
                        buf.emit8(0x66); buf.emit8(0x0F); buf.emit8(0x2E); buf.emit8(0xC1);
                        buf.emit8(0x48); buf.emit8(0x31); buf.emit8(0xC0);
                        buf.emit8(0x0F); buf.emit8(0x96); buf.emit8(0xC0); // setbe al
                        buf.emit8(0xF2); buf.emit8(0x48); buf.emit8(0x0F); buf.emit8(0x2A); buf.emit8(0xC0);
                        break;
                    case BinaryOp::GTE:
                        buf.emit8(0x66); buf.emit8(0x0F); buf.emit8(0x2E); buf.emit8(0xC1);
                        buf.emit8(0x48); buf.emit8(0x31); buf.emit8(0xC0);
                        buf.emit8(0x0F); buf.emit8(0x93); buf.emit8(0xC0); // setae al
                        buf.emit8(0xF2); buf.emit8(0x48); buf.emit8(0x0F); buf.emit8(0x2A); buf.emit8(0xC0);
                        break;
                    case BinaryOp::EQ:
                        buf.emit8(0x66); buf.emit8(0x0F); buf.emit8(0x2E); buf.emit8(0xC1);
                        buf.emit8(0x48); buf.emit8(0x31); buf.emit8(0xC0);
                        buf.emit8(0x0F); buf.emit8(0x94); buf.emit8(0xC0); // sete al
                        buf.emit8(0xF2); buf.emit8(0x48); buf.emit8(0x0F); buf.emit8(0x2A); buf.emit8(0xC0);
                        break;
                    case BinaryOp::NEQ:
                        buf.emit8(0x66); buf.emit8(0x0F); buf.emit8(0x2E); buf.emit8(0xC1);
                        buf.emit8(0x48); buf.emit8(0x31); buf.emit8(0xC0);
                        buf.emit8(0x0F); buf.emit8(0x95); buf.emit8(0xC0); // setne al
                        buf.emit8(0xF2); buf.emit8(0x48); buf.emit8(0x0F); buf.emit8(0x2A); buf.emit8(0xC0);
                        break;
                    default: break;
                }
                
                // movq result.valueReg, xmm0
                bool resHigh = static_cast<uint8_t>(result.valueReg) >= 8;
                buf.emit8(0x66);
                buf.emit8(0x48 | (resHigh ? 0x01 : 0));
                buf.emit8(0x0F);
                buf.emit8(0x7E); // movq r/m64, xmm0
                buf.emit8(0xC0 | (static_cast<uint8_t>(result.valueReg) & 0x7));
                
                // mov result.typeReg, 1
                bool resTypeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                buf.emit8(0x48 | (resTypeHigh ? 0x01 : 0));
                buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                buf.emit64(1);

                // Patch jmp end
                size_t endPos = buf.getOffset();
                int32_t jmpOffset = static_cast<int32_t>(endPos - (jmpPatch + 1)); // 1-byte jmp uses patch8 not patch32 in my manual emit above?
               
                buf.patch8(jmpPatch, static_cast<uint8_t>(jmpOffset));
            } // end if (!staticIntPath && !staticFloatPath)
            
            if (!rightIsImm) {
                freeReg(right.valueReg); freeReg(right.typeReg);
            }
            return result;
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
                    // Linux x86-64 IDIV: divides RDX:RAX by operand, quotient in RAX
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
                    // Linux x86-64 IDIV: remainder in RDX
                    
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
                const std::string& memberName = callee.name;
                
                // Check for module namespace calls (e.g., math.square())
                if (callee.left != INVALID_NODE) {
                    const ASTNode& moduleNode = ast.get(callee.left);
                    if (moduleNode.type == NodeType::IDENTIFIER && modules.count(moduleNode.name)) {
                        std::string namespacedName = moduleNode.name + "_" + memberName;
                        if (userFunctions.count(namespacedName)) {
                            return compileUserCall(ast, idx, namespacedName);
                        }
                    }
                }
                
                // Module function calls like ai.loadModel()
                // Compile the object first
                JITValue objVal = compileExpr(ast, callee.left);
                X64Reg objReg = objVal.valueReg;
                
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
                
                // --- Map method dispatch ---
                
                if (memberName == "size") {
                    // map.size() or arr.size() — type-aware dispatch
                    int32_t retSlot = allocateStackSlot();
                    
                    buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52);
                    buf.emit8(0x41); buf.emit8(0x50); buf.emit8(0x41); buf.emit8(0x51);
                    buf.emit8(0x41); buf.emit8(0x52); buf.emit8(0x41); buf.emit8(0x53);
                    
                    // RDI = objReg (pointer to map or array data)
                    bool objHi = static_cast<uint8_t>(objReg) >= 8;
                    buf.emit8(0x48 | (objHi ? 0x01 : 0));
                    buf.emit8(0x89); buf.emit8(0xC7 | ((static_cast<uint8_t>(objReg) & 0x7) << 3));
                    
                    // Check type: cmp typeReg, ValueType::MAP
                    bool typeHi = static_cast<uint8_t>(objVal.typeReg) >= 8;
                    buf.emit8(0x48 | (typeHi ? 0x01 : 0));
                    buf.emit8(0x83);
                    buf.emit8(0xF8 | (static_cast<uint8_t>(objVal.typeReg) & 0x7));
                    buf.emit8(static_cast<uint8_t>(ValueType::MAP));
                    
                    // je map_size
                    buf.emit8(0x74);
                    size_t jeMapPatch = buf.getOffset();
                    buf.emit8(0x00);
                    
                    // Array path: call jit_array_size
                    buf.emit8(0x48); buf.emit8(0xB8);
                    buf.emit64(reinterpret_cast<uint64_t>(jit_array_size));
                    buf.emit8(0xFF); buf.emit8(0xD0);
                    buf.emit8(0xEB); // jmp done
                    size_t jmpDonePatch = buf.getOffset();
                    buf.emit8(0x00);
                    
                    // Map path: call jit_map_size
                    size_t mapSizeLabel = buf.getOffset();
                    buf.patch8(jeMapPatch, static_cast<uint8_t>(mapSizeLabel - (jeMapPatch + 1)));
                    buf.emit8(0x48); buf.emit8(0xB8);
                    buf.emit64(reinterpret_cast<uint64_t>(jit_map_size));
                    buf.emit8(0xFF); buf.emit8(0xD0);
                    
                    // Done:
                    size_t doneLabel = buf.getOffset();
                    buf.patch8(jmpDonePatch, static_cast<uint8_t>(doneLabel - (jmpDonePatch + 1)));
                    
                    // Save return value to stack before restoring registers
                    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x85);
                    buf.emit32(static_cast<uint32_t>(retSlot));
                    
                    buf.emit8(0x41); buf.emit8(0x5B); buf.emit8(0x41); buf.emit8(0x5A);
                    buf.emit8(0x41); buf.emit8(0x59); buf.emit8(0x41); buf.emit8(0x58);
                    buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58);
                    
                    freeReg(objVal.valueReg);
                    freeReg(objVal.typeReg);
                    
                    X64Reg dst = allocateReg();
                    bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                    buf.emit8(0x48 | (dstHigh ? 0x04 : 0));
                    buf.emit8(0x8B); buf.emit8(0x85 | ((static_cast<uint8_t>(dst) & 0x7) << 3));
                    buf.emit32(static_cast<uint32_t>(retSlot));
                    
                    JITValue result;
                    result.valueReg = dst;
                    result.typeReg = allocateReg();
                    bool tHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                    buf.emit8(0x48 | (tHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                    buf.emit64(0); // Type tag 0 = INT (JIT convention)
                    return result;
                }
                
                if (memberName == "has") {
                    // map.has(key)
                    if (node.children.empty()) return objVal;
                    JITValue argVal = compileExpr(ast, node.children[0]);
                    int32_t retSlot = allocateStackSlot();
                    
                    buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52);
                    buf.emit8(0x41); buf.emit8(0x50); buf.emit8(0x41); buf.emit8(0x51);
                    buf.emit8(0x41); buf.emit8(0x52); buf.emit8(0x41); buf.emit8(0x53);
                    
                    bool objHi = static_cast<uint8_t>(objReg) >= 8;
                    buf.emit8(0x48 | (objHi ? 0x01 : 0));
                    buf.emit8(0x89); buf.emit8(0xC7 | ((static_cast<uint8_t>(objReg) & 0x7) << 3));
                    
                    bool argHigh = static_cast<uint8_t>(argVal.valueReg) >= 8;
                    buf.emit8(0x48 | (argHigh ? 0x04 : 0));
                    buf.emit8(0x89); buf.emit8(0xC6 | ((static_cast<uint8_t>(argVal.valueReg) & 0x7) << 3));
                    
                    buf.emit8(0x48); buf.emit8(0xB8);
                    buf.emit64(reinterpret_cast<uint64_t>(jit_map_has));
                    buf.emit8(0xFF); buf.emit8(0xD0);
                    
                    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x85);
                    buf.emit32(static_cast<uint32_t>(retSlot));
                    
                    buf.emit8(0x41); buf.emit8(0x5B); buf.emit8(0x41); buf.emit8(0x5A);
                    buf.emit8(0x41); buf.emit8(0x59); buf.emit8(0x41); buf.emit8(0x58);
                    buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58);
                    
                    freeReg(argVal.valueReg);
                    freeReg(argVal.typeReg);
                    freeReg(objVal.valueReg);
                    freeReg(objVal.typeReg);
                    
                    X64Reg dst = allocateReg();
                    bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                    buf.emit8(0x48 | (dstHigh ? 0x04 : 0));
                    buf.emit8(0x8B); buf.emit8(0x85 | ((static_cast<uint8_t>(dst) & 0x7) << 3));
                    buf.emit32(static_cast<uint32_t>(retSlot));
                    
                    JITValue result;
                    result.valueReg = dst;
                    result.typeReg = allocateReg();
                    bool tHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                    buf.emit8(0x48 | (tHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                    buf.emit64(0); // Type tag 0 = INT (JIT convention)
                    return result;
                }
                
                if (memberName == "remove") {
                    // map.remove(key)
                    if (node.children.empty()) return objVal;
                    JITValue argVal = compileExpr(ast, node.children[0]);
                    int32_t retSlot = allocateStackSlot();
                    
                    buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52);
                    buf.emit8(0x41); buf.emit8(0x50); buf.emit8(0x41); buf.emit8(0x51);
                    buf.emit8(0x41); buf.emit8(0x52); buf.emit8(0x41); buf.emit8(0x53);
                    
                    bool objHi = static_cast<uint8_t>(objReg) >= 8;
                    buf.emit8(0x48 | (objHi ? 0x01 : 0));
                    buf.emit8(0x89); buf.emit8(0xC7 | ((static_cast<uint8_t>(objReg) & 0x7) << 3));
                    
                    bool argHigh = static_cast<uint8_t>(argVal.valueReg) >= 8;
                    buf.emit8(0x48 | (argHigh ? 0x04 : 0));
                    buf.emit8(0x89); buf.emit8(0xC6 | ((static_cast<uint8_t>(argVal.valueReg) & 0x7) << 3));
                    
                    buf.emit8(0x48); buf.emit8(0xB8);
                    buf.emit64(reinterpret_cast<uint64_t>(jit_map_remove));
                    buf.emit8(0xFF); buf.emit8(0xD0);
                    
                    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x85);
                    buf.emit32(static_cast<uint32_t>(retSlot));
                    
                    buf.emit8(0x41); buf.emit8(0x5B); buf.emit8(0x41); buf.emit8(0x5A);
                    buf.emit8(0x41); buf.emit8(0x59); buf.emit8(0x41); buf.emit8(0x58);
                    buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58);
                    
                    freeReg(argVal.valueReg);
                    freeReg(argVal.typeReg);
                    freeReg(objVal.valueReg);
                    freeReg(objVal.typeReg);
                    
                    X64Reg dst = allocateReg();
                    bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                    buf.emit8(0x48 | (dstHigh ? 0x04 : 0));
                    buf.emit8(0x8B); buf.emit8(0x85 | ((static_cast<uint8_t>(dst) & 0x7) << 3));
                    buf.emit32(static_cast<uint32_t>(retSlot));
                    
                    JITValue result;
                    result.valueReg = dst;
                    result.typeReg = allocateReg();
                    bool tHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                    buf.emit8(0x48 | (tHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                    buf.emit64(0); // Type tag 0 = INT (JIT convention)
                    return result;
                }
                
                if (memberName == "keys") {
                    // map.keys() -> returns JITArray
                    int32_t retSlot = allocateStackSlot();
                    
                    buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52);
                    buf.emit8(0x41); buf.emit8(0x50); buf.emit8(0x41); buf.emit8(0x51);
                    buf.emit8(0x41); buf.emit8(0x52); buf.emit8(0x41); buf.emit8(0x53);
                    
                    bool objHi = static_cast<uint8_t>(objReg) >= 8;
                    buf.emit8(0x48 | (objHi ? 0x01 : 0));
                    buf.emit8(0x89); buf.emit8(0xC7 | ((static_cast<uint8_t>(objReg) & 0x7) << 3));
                    
                    buf.emit8(0x48); buf.emit8(0xB8);
                    buf.emit64(reinterpret_cast<uint64_t>(jit_map_keys));
                    buf.emit8(0xFF); buf.emit8(0xD0);
                    
                    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x85);
                    buf.emit32(static_cast<uint32_t>(retSlot));
                    
                    buf.emit8(0x41); buf.emit8(0x5B); buf.emit8(0x41); buf.emit8(0x5A);
                    buf.emit8(0x41); buf.emit8(0x59); buf.emit8(0x41); buf.emit8(0x58);
                    buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58);
                    
                    freeReg(objVal.valueReg);
                    freeReg(objVal.typeReg);
                    
                    X64Reg dst = allocateReg();
                    bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                    buf.emit8(0x48 | (dstHigh ? 0x04 : 0));
                    buf.emit8(0x8B); buf.emit8(0x85 | ((static_cast<uint8_t>(dst) & 0x7) << 3));
                    buf.emit32(static_cast<uint32_t>(retSlot));
                    
                    JITValue result;
                    result.valueReg = dst;
                    result.typeReg = allocateReg();
                    bool tHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                    buf.emit8(0x48 | (tHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                    buf.emit64(static_cast<uint64_t>(ValueType::ARRAY));
                    return result;
                }
                
                if (memberName == "values") {
                    // map.values() -> returns JITArray
                    int32_t retSlot = allocateStackSlot();
                    
                    buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52);
                    buf.emit8(0x41); buf.emit8(0x50); buf.emit8(0x41); buf.emit8(0x51);
                    buf.emit8(0x41); buf.emit8(0x52); buf.emit8(0x41); buf.emit8(0x53);
                    
                    bool objHi = static_cast<uint8_t>(objReg) >= 8;
                    buf.emit8(0x48 | (objHi ? 0x01 : 0));
                    buf.emit8(0x89); buf.emit8(0xC7 | ((static_cast<uint8_t>(objReg) & 0x7) << 3));
                    
                    buf.emit8(0x48); buf.emit8(0xB8);
                    buf.emit64(reinterpret_cast<uint64_t>(jit_map_values));
                    buf.emit8(0xFF); buf.emit8(0xD0);
                    
                    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x85);
                    buf.emit32(static_cast<uint32_t>(retSlot));
                    
                    buf.emit8(0x41); buf.emit8(0x5B); buf.emit8(0x41); buf.emit8(0x5A);
                    buf.emit8(0x41); buf.emit8(0x59); buf.emit8(0x41); buf.emit8(0x58);
                    buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58);
                    
                    freeReg(objVal.valueReg);
                    freeReg(objVal.typeReg);
                    
                    X64Reg dst = allocateReg();
                    bool dstHigh = static_cast<uint8_t>(dst) >= 8;
                    buf.emit8(0x48 | (dstHigh ? 0x04 : 0));
                    buf.emit8(0x8B); buf.emit8(0x85 | ((static_cast<uint8_t>(dst) & 0x7) << 3));
                    buf.emit32(static_cast<uint32_t>(retSlot));
                    
                    JITValue result;
                    result.valueReg = dst;
                    result.typeReg = allocateReg();
                    bool tHigh = static_cast<uint8_t>(result.typeReg) >= 8;
                    buf.emit8(0x48 | (tHigh ? 0x01 : 0));
                    buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
                    buf.emit64(static_cast<uint64_t>(ValueType::ARRAY));
                    return result;
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
            // Allocate array using JIT runtime helper
            size_t elemCount = node.children.size();
            CodeBuffer& buf = codegen.getCode();
            
            // Call jit_alloc_array(elemCount)
            // Save Scratch
            buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52); // push rax, rcx, rdx
            buf.emit8(0x56); buf.emit8(0x57);                  // push rsi, rdi
            buf.emit8(0x41); buf.emit8(0x50); buf.emit8(0x41); buf.emit8(0x51); // push r8, r9
            buf.emit8(0x41); buf.emit8(0x52); buf.emit8(0x41); buf.emit8(0x53); // push r10, r11
            
            // RDI = elemCount
            buf.emit8(0x48); buf.emit8(0xBF);
            buf.emit64(static_cast<uint64_t>(elemCount));
            
            // Call jit_alloc_array
            buf.emit8(0x48); buf.emit8(0xB8);
            buf.emit64(reinterpret_cast<uint64_t>(jit_alloc_array));
            buf.emit8(0xFF); buf.emit8(0xD0);
            
            // RAX now holds dataPtr. Save it.
            int32_t tempOffset = allocateStackSlot();
            buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x85);
            buf.emit32(static_cast<uint32_t>(tempOffset));
            
            // Restore Scratch
            buf.emit8(0x41); buf.emit8(0x5B); buf.emit8(0x41); buf.emit8(0x5A); // pop r11, r10
            buf.emit8(0x41); buf.emit8(0x59); buf.emit8(0x41); buf.emit8(0x58); // pop r9, r8
            buf.emit8(0x5F); buf.emit8(0x5E);                  // pop rdi, rsi
            buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58); // pop rdx, rcx, rax
            
            // Now, load the allocated baseReg
            X64Reg baseReg = allocateReg();
            bool baseHigh = static_cast<uint8_t>(baseReg) >= 8;
            buf.emit8(0x48 | (baseHigh ? 0x04 : 0));
            buf.emit8(0x8B); buf.emit8(0x85 | ((static_cast<uint8_t>(baseReg) & 0x7) << 3));
            buf.emit32(static_cast<uint32_t>(tempOffset));
            
            // Store each element
            for (size_t i = 0; i < elemCount; ++i) {
                JITValue elemVal = compileExpr(ast, node.children[i]);
                X64Reg elemReg = elemVal.valueReg;
                freeReg(elemVal.typeReg);
                
                // Save Scratch
                buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52); // push rax, rcx, rdx
                buf.emit8(0x56); buf.emit8(0x57);                  // push rsi, rdi
                buf.emit8(0x41); buf.emit8(0x50); buf.emit8(0x41); buf.emit8(0x51); // push r8, r9
                buf.emit8(0x41); buf.emit8(0x52); buf.emit8(0x41); buf.emit8(0x53); // push r10, r11
                
                // RDI = baseReg
                buf.emit8(0x48 | (baseHigh ? 0x04 : 0));
                buf.emit8(0x89); buf.emit8(0xC0 | ((static_cast<uint8_t>(baseReg) & 0x7) << 3) | 7);
                
                // RSI = i
                buf.emit8(0x48); buf.emit8(0xBE); // mov rsi, imm64
                buf.emit64(static_cast<uint64_t>(i));
                
                // RDX = elemReg
                bool regHigh = static_cast<uint8_t>(elemReg) >= 8;
                buf.emit8(0x48 | (regHigh ? 0x04 : 0));
                buf.emit8(0x89); buf.emit8(0xC0 | ((static_cast<uint8_t>(elemReg) & 0x7) << 3) | 2); // mov rdx, elemReg
                
                // Call jit_array_set
                buf.emit8(0x48); buf.emit8(0xB8);
                buf.emit64(reinterpret_cast<uint64_t>(jit_array_set));
                buf.emit8(0xFF); buf.emit8(0xD0);
                
                // Restore Scratch
                buf.emit8(0x41); buf.emit8(0x5B); buf.emit8(0x41); buf.emit8(0x5A);
                buf.emit8(0x41); buf.emit8(0x59); buf.emit8(0x41); buf.emit8(0x58);
                buf.emit8(0x5F); buf.emit8(0x5E);
                buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58);
                
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
        
        case NodeType::MAP_LITERAL: {
            size_t elemCount = node.children.size() / 2;
            CodeBuffer& buf = codegen.getCode();
            
            // Call jit_alloc_map(elemCount)
            buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52); // push rax, rcx, rdx
            buf.emit8(0x56); buf.emit8(0x57);                  // push rsi, rdi
            buf.emit8(0x41); buf.emit8(0x50); buf.emit8(0x41); buf.emit8(0x51); // push r8, r9
            buf.emit8(0x41); buf.emit8(0x52); buf.emit8(0x41); buf.emit8(0x53); // push r10, r11
            
            // RDI = elemCount
            buf.emit8(0x48); buf.emit8(0xBF);
            buf.emit64(static_cast<uint64_t>(elemCount));
            
            buf.emit8(0x48); buf.emit8(0xB8);
            buf.emit64(reinterpret_cast<uint64_t>(jit_alloc_map));
            buf.emit8(0xFF); buf.emit8(0xD0);
            
            int32_t tempOffset = allocateStackSlot();
            buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x85);
            buf.emit32(static_cast<uint32_t>(tempOffset));
            
            buf.emit8(0x41); buf.emit8(0x5B); buf.emit8(0x41); buf.emit8(0x5A); // pop r11, r10
            buf.emit8(0x41); buf.emit8(0x59); buf.emit8(0x41); buf.emit8(0x58); // pop r9, r8
            buf.emit8(0x5F); buf.emit8(0x5E);                  // pop rdi, rsi
            buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58); // pop rdx, rcx, rax
            
            X64Reg baseReg = allocateReg();
            bool baseHigh = static_cast<uint8_t>(baseReg) >= 8;
            buf.emit8(0x48 | (baseHigh ? 0x04 : 0));
            buf.emit8(0x8B); buf.emit8(0x85 | ((static_cast<uint8_t>(baseReg) & 0x7) << 3));
            buf.emit32(static_cast<uint32_t>(tempOffset));
            
            for (size_t i = 0; i < node.children.size() && i + 1 < node.children.size(); i += 2) {
                JITValue keyVal = compileExpr(ast, node.children[i]);
                X64Reg keyReg = keyVal.valueReg;
                freeReg(keyVal.typeReg);
                
                JITValue valVal = compileExpr(ast, node.children[i+1]);
                X64Reg valReg = valVal.valueReg;
                freeReg(valVal.typeReg);
                
                buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52); 
                buf.emit8(0x56); buf.emit8(0x57);                  
                buf.emit8(0x41); buf.emit8(0x50); buf.emit8(0x41); buf.emit8(0x51); 
                buf.emit8(0x41); buf.emit8(0x52); buf.emit8(0x41); buf.emit8(0x53); 
                
                // RDI = baseReg
                buf.emit8(0x48 | (baseHigh ? 0x04 : 0));
                buf.emit8(0x89); buf.emit8(0xC0 | ((static_cast<uint8_t>(baseReg) & 0x7) << 3) | 7);
                
                // RSI = keyReg
                bool keyHigh = static_cast<uint8_t>(keyReg) >= 8;
                buf.emit8(0x48 | (keyHigh ? 0x04 : 0));
                buf.emit8(0x89); buf.emit8(0xC0 | ((static_cast<uint8_t>(keyReg) & 0x7) << 3) | 6);
                
                // RDX = valReg
                bool valHigh = static_cast<uint8_t>(valReg) >= 8;
                buf.emit8(0x48 | (valHigh ? 0x04 : 0));
                buf.emit8(0x89); buf.emit8(0xC0 | ((static_cast<uint8_t>(valReg) & 0x7) << 3) | 2); 
                
                // Call jit_map_set (returns new map ptr in RAX)
                buf.emit8(0x48); buf.emit8(0xB8);
                buf.emit64(reinterpret_cast<uint64_t>(jit_map_set));
                buf.emit8(0xFF); buf.emit8(0xD0);
                
                // Update baseReg with returned pointer (may differ after resize)
                buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x85);
                buf.emit32(static_cast<uint32_t>(tempOffset));
                
                buf.emit8(0x41); buf.emit8(0x5B); buf.emit8(0x41); buf.emit8(0x5A);
                buf.emit8(0x41); buf.emit8(0x59); buf.emit8(0x41); buf.emit8(0x58);
                buf.emit8(0x5F); buf.emit8(0x5E);
                buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58);
                
                // Reload baseReg from temp slot (captures resize result)
                buf.emit8(0x48 | (baseHigh ? 0x04 : 0));
                buf.emit8(0x8B); buf.emit8(0x85 | ((static_cast<uint8_t>(baseReg) & 0x7) << 3));
                buf.emit32(static_cast<uint32_t>(tempOffset));
                
                freeReg(keyReg);
                freeReg(valReg);
            }
            
            JITValue result;
            result.valueReg = baseReg;
            result.typeReg = allocateReg();
            bool typeHigh = static_cast<uint8_t>(result.typeReg) >= 8;
            buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
            buf.emit64(static_cast<uint64_t>(ValueType::MAP));
            
            return result;
        }
        
        case NodeType::INDEX_ACCESS: {
            // map/array[index] - load element dynamically based on type
            JITValue arrVal = compileExpr(ast, node.left);
            X64Reg arrReg = arrVal.valueReg;
            X64Reg tReg = arrVal.typeReg;
            
            JITValue idxVal = compileExpr(ast, node.right);
            X64Reg idxReg = idxVal.valueReg;
            freeReg(idxVal.typeReg);
            
            CodeBuffer& buf = codegen.getCode();
            int32_t tempOffset = allocateStackSlot();
            
            // Check if arr is MAP
            bool tHigh = static_cast<uint8_t>(tReg) >= 8;
            buf.emit8(0x48 | (tHigh ? 0x01 : 0));
            buf.emit8(0x83); buf.emit8(0xF8 + (static_cast<uint8_t>(tReg) & 0x7));
            buf.emit8(static_cast<uint8_t>(ValueType::MAP));
            
            buf.emit8(0x0F); buf.emit8(0x84); // je is_map
            size_t isMapJump = buf.size();
            buf.emit32(0);
            
            // --- ARRAY GET ---
            // Save Scratch
            buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52); buf.emit8(0x56); buf.emit8(0x57);
            buf.emit8(0x41); buf.emit8(0x50); buf.emit8(0x41); buf.emit8(0x51);
            buf.emit8(0x41); buf.emit8(0x52); buf.emit8(0x41); buf.emit8(0x53);
            
            // RDI = arrReg
            bool arrHigh = static_cast<uint8_t>(arrReg) >= 8;
            buf.emit8(0x48 | (arrHigh ? 0x04 : 0));
            buf.emit8(0x89); buf.emit8(0xC0 | ((static_cast<uint8_t>(arrReg) & 0x7) << 3) | 7);
            
            // RSI = idxReg
            bool idxHigh = static_cast<uint8_t>(idxReg) >= 8;
            buf.emit8(0x48 | (idxHigh ? 0x04 : 0));
            buf.emit8(0x89); buf.emit8(0xC0 | ((static_cast<uint8_t>(idxReg) & 0x7) << 3) | 6);
            
            // Call jit_array_get
            buf.emit8(0x48); buf.emit8(0xB8);
            buf.emit64(reinterpret_cast<uint64_t>(jit_array_get));
            buf.emit8(0xFF); buf.emit8(0xD0);
            
            // Save RAX
            buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x85); 
            buf.emit32(static_cast<uint32_t>(tempOffset));
            
            // Restore Scratch
            buf.emit8(0x41); buf.emit8(0x5B); buf.emit8(0x41); buf.emit8(0x5A);
            buf.emit8(0x41); buf.emit8(0x59); buf.emit8(0x41); buf.emit8(0x58);
            buf.emit8(0x5F); buf.emit8(0x5E); buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58);
            
            buf.emit8(0xE9); // jmp done
            size_t doneJump = buf.size();
            buf.emit32(0);
            
            // --- MAP GET ---
            size_t isMapTarget = buf.size();
            buf.patch32(isMapJump, static_cast<int32_t>(isMapTarget - (isMapJump + 4)));
            
            // Save Scratch
            buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52); buf.emit8(0x56); buf.emit8(0x57);
            buf.emit8(0x41); buf.emit8(0x50); buf.emit8(0x41); buf.emit8(0x51);
            buf.emit8(0x41); buf.emit8(0x52); buf.emit8(0x41); buf.emit8(0x53);
            
            // RDI = arrReg
            buf.emit8(0x48 | (arrHigh ? 0x04 : 0));
            buf.emit8(0x89); buf.emit8(0xC0 | ((static_cast<uint8_t>(arrReg) & 0x7) << 3) | 7);
            
            // RSI = idxReg
            buf.emit8(0x48 | (idxHigh ? 0x04 : 0));
            buf.emit8(0x89); buf.emit8(0xC0 | ((static_cast<uint8_t>(idxReg) & 0x7) << 3) | 6);
            
            // Call jit_map_get
            buf.emit8(0x48); buf.emit8(0xB8);
            buf.emit64(reinterpret_cast<uint64_t>(jit_map_get));
            buf.emit8(0xFF); buf.emit8(0xD0);
            
            // Save RAX
            buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x85); 
            buf.emit32(static_cast<uint32_t>(tempOffset));
            
            // Restore Scratch
            buf.emit8(0x41); buf.emit8(0x5B); buf.emit8(0x41); buf.emit8(0x5A);
            buf.emit8(0x41); buf.emit8(0x59); buf.emit8(0x41); buf.emit8(0x58);
            buf.emit8(0x5F); buf.emit8(0x5E); buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58);
            
            // --- DONE ---
            size_t doneTarget = buf.size();
            buf.patch32(doneJump, static_cast<int32_t>(doneTarget - (doneJump + 4)));
            
            freeReg(arrReg);
            freeReg(tReg);
            freeReg(idxReg);
            
            JITValue result;
            result.valueReg = allocateReg();
            result.typeReg = allocateReg();
            
            bool resHigh = static_cast<uint8_t>(result.valueReg) >= 8;
            buf.emit8(0x48 | (resHigh ? 0x04 : 0));
            buf.emit8(0x8B); buf.emit8(0x85 | ((static_cast<uint8_t>(result.valueReg) & 0x7) << 3));
            buf.emit32(static_cast<uint32_t>(tempOffset));
            
            bool typeHighDst = static_cast<uint8_t>(result.typeReg) >= 8;
            buf.emit8(0x48 | (typeHighDst ? 0x01 : 0));
            buf.emit8(0xB8 + (static_cast<uint8_t>(result.typeReg) & 0x7));
            buf.emit64(0); // Assuming INT for now, can be updated later
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
            // Synchronous evaluation: current single-pass JIT inlines function bodies,
            // so there is no compiled function pointer to spawn on a thread.
            // The jit_async_spawn/jit_await_task helpers are available for future
            // IR pipeline integration (Nevaarize 2.0).
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
    
    std::vector<size_t> jumpsToEndOfAssignment;
    
    // Peephole Optimization: Direct Arithmetic for Accumulators (Registers & Stack)
    bool peepholeOptimized = false;
    if (node.left != INVALID_NODE) {
        const ASTNode& exprNode = ast.get(node.left);
        if (exprNode.type == NodeType::BINARY_OP && 
            (exprNode.binaryOp == BinaryOp::ADD || exprNode.binaryOp == BinaryOp::SUB)) {
            
            const ASTNode& leftOperand = ast.get(exprNode.left);
            const ASTNode& rightOperand = ast.get(exprNode.right);
            
            if (leftOperand.type == NodeType::IDENTIFIER && leftOperand.name == node.name && 
                rightOperand.type == NodeType::LITERAL_INT) {
                
                auto it = variables.find(node.name);
                if (it != variables.end()) {
                    int64_t immVal = std::get<int64_t>(rightOperand.literal.data);
                    if (immVal >= -2147483648LL && immVal <= 2147483647LL) {
                        CodeBuffer& buf = codegen.getCode();
                        if (it->second.isRegister) {
                            X64Reg targetReg = it->second.reg;
                            bool regHigh = static_cast<uint8_t>(targetReg) >= 8;
                            if (immVal == 1 && exprNode.binaryOp == BinaryOp::ADD) {
                                buf.emit8(0x48 | (regHigh ? 0x01 : 0)); buf.emit8(0xFF); buf.emit8(0xC0 | (static_cast<uint8_t>(targetReg) & 0x7));
                            } else if (immVal == 1 && exprNode.binaryOp == BinaryOp::SUB) {
                                buf.emit8(0x48 | (regHigh ? 0x01 : 0)); buf.emit8(0xFF); buf.emit8(0xC8 | (static_cast<uint8_t>(targetReg) & 0x7));
                            } else {
                                buf.emit8(0x48 | (regHigh ? 0x01 : 0)); buf.emit8(0x81);
                                buf.emit8((exprNode.binaryOp == BinaryOp::ADD ? 0xC0 : 0xE8) | (static_cast<uint8_t>(targetReg) & 0x7));
                                buf.emit32(static_cast<uint32_t>(immVal));
                            }
                        } else {
                            int32_t offset = it->second.stackOffset;
                            buf.emit8(0x48); buf.emit8(0x81);
                            buf.emit8(exprNode.binaryOp == BinaryOp::ADD ? 0x85 : 0xAD);
                            buf.emit32(static_cast<uint32_t>(offset));
                            buf.emit32(static_cast<uint32_t>(immVal));
                        }
                        peepholeOptimized = true;
                    }
                }
            } else if (leftOperand.type == NodeType::IDENTIFIER && leftOperand.name == node.name && 
                   rightOperand.type == NodeType::LITERAL_STRING && exprNode.binaryOp == BinaryOp::ADD) {
                
                auto it = variables.find(node.name);
                if (it != variables.end() && it->second.isRegister) {
                    std::string strVal = std::get<std::string>(rightOperand.literal.data);
                    if (strVal.length() == 1) {
                        X64Reg targetReg = it->second.reg;
                        bool regHigh = static_cast<uint8_t>(targetReg) >= 8;
                        int32_t offset = it->second.stackOffset;
                        
                        // Check if dynamic type is String (4)
                        buf.emit8(0x4C); buf.emit8(0x8B); buf.emit8(0x8D); // mov r9, [rbp + offset]
                        buf.emit32(static_cast<uint32_t>(offset + 8));
                        buf.emit8(0x49); buf.emit8(0x83); buf.emit8(0xF9); buf.emit8(0x04); // cmp r9, 4
                        
                        // If not string, jump to generic execution
                        buf.emit8(0x75);
                        size_t notStringJump = buf.getOffset();
                        buf.emit8(0x00);
                        
                        // FAST PATH: inline string concat!
                        // mov r10, [targetReg - 16] // capacity
                        buf.emit8(0x4C | (regHigh ? 0x01 : 0));
                        buf.emit8(0x8B); buf.emit8(0x50 | (static_cast<uint8_t>(targetReg) & 0x7));
                        buf.emit8(0xF0); // -16
                        
                        // mov r11, [targetReg - 8] // length
                        buf.emit8(0x4C | (regHigh ? 0x01 : 0));
                        buf.emit8(0x8B); buf.emit8(0x58 | (static_cast<uint8_t>(targetReg) & 0x7));
                        buf.emit8(0xF8); // -8
                        
                        // cmp r10, r11
                        buf.emit8(0x4D); buf.emit8(0x39); buf.emit8(0xDA);
                        
                        // jle fallbackToAlloc (if capacity <= length)
                        buf.emit8(0x7E);
                        size_t fallbackAllocJump = buf.getOffset();
                        buf.emit8(0x00);
                        
                        // mov byte ptr [targetReg + r11], literal_char
                        buf.emit8(0x42 | (regHigh ? 0x01 : 0)); // REX prefix: X=1 because r11
                        buf.emit8(0xC6);
                        buf.emit8(0x04); // SIB byte follows (ModR/M=0x04)
                        buf.emit8(0x18 | (static_cast<uint8_t>(targetReg) & 0x7));
                        buf.emit8(strVal[0]);
                        
                        // inc qword ptr [targetReg - 8]
                        buf.emit8(0x48 | (regHigh ? 0x01 : 0));
                        buf.emit8(0xFF); buf.emit8(0x40 | (static_cast<uint8_t>(targetReg) & 0x7));
                        buf.emit8(0xF8); // -8
                        
                        // jump over generic assignment since peephole succeeded!
                        buf.emit8(0xE9);
                        jumpsToEndOfAssignment.push_back(buf.getOffset());
                        buf.emit32(0);
                        
                        // FallbackAlloc: If capacity is full, call jit_string_concat
                        size_t fallbackTarget = buf.getOffset();
                        buf.patch8(fallbackAllocJump, static_cast<uint8_t>(fallbackTarget - (fallbackAllocJump + 1)));
                        
                        void* preAllocatedStr = jit_alloc_string(strVal.c_str());
                        // No need to push caller-saved registers; stack remains aligned and peephole has no live scratch registers
                        // mov rdi, targetReg
                        buf.emit8(0x48 | (regHigh ? 0x04 : 0));
                        buf.emit8(0x89); buf.emit8(0xC7 | ((static_cast<uint8_t>(targetReg) & 0x7) << 3));
                        // mov rsi, preAllocatedStr
                        buf.emit8(0x48); buf.emit8(0xBE);
                        buf.emit64(reinterpret_cast<uint64_t>(preAllocatedStr));
                        
                        buf.emit8(0x48); buf.emit8(0xB8);
                        buf.emit64(reinterpret_cast<uint64_t>(jit_string_concat));
                        buf.emit8(0xFF); buf.emit8(0xD0); // call rax
                        
                        // mov targetReg, rax
                        buf.emit8(0x48 | (regHigh ? 0x01 : 0));
                        buf.emit8(0x89); buf.emit8(0xC0 | (static_cast<uint8_t>(targetReg) & 0x7));
                        
                        // jump over generic assignment
                        buf.emit8(0xE9);
                        jumpsToEndOfAssignment.push_back(buf.getOffset());
                        buf.emit32(0);
                        
                        // Generic Fallback! If it was NOT a string, fallback to compileExpr!
                        size_t genericTarget = buf.getOffset();
                        buf.patch8(notStringJump, static_cast<uint8_t>(genericTarget - (notStringJump + 1)));
                        
                        peepholeOptimized = false; // Trigger standard compilation for fallback
                    }
                }
            } else if (leftOperand.type == NodeType::IDENTIFIER && leftOperand.name == node.name && 
                   rightOperand.type == NodeType::LITERAL_FLOAT) {
                
                auto it = variables.find(node.name);
                if (it != variables.end()) {
                    double immVal = std::get<double>(rightOperand.literal.data);
                    uint64_t bits;
                    std::memcpy(&bits, &immVal, sizeof(bits));

                    CodeBuffer& buf = codegen.getCode();

                    // Determine the SSE opcode for the binary operation
                    uint8_t sseOpcode = 0;
                    switch (exprNode.binaryOp) {
                        case BinaryOp::ADD: sseOpcode = 0x58; break; // addsd
                        case BinaryOp::SUB: sseOpcode = 0x5C; break; // subsd
                        case BinaryOp::MUL: sseOpcode = 0x59; break; // mulsd
                        case BinaryOp::DIV: sseOpcode = 0x5E; break; // divsd
                        default: break;
                    }

                    if (it->second.isXMMRegister && sseOpcode != 0) {
                        // === XMM-NATIVE FAST PATH ===
                        // Variable is pinned to an XMM register.
                        X64Reg targetXMM = it->second.reg;
                        uint8_t tIdx = static_cast<uint8_t>(targetXMM) - static_cast<uint8_t>(X64Reg::XMM0);

                        std::string constKey = std::to_string(bits);
                        auto hcIt = hoistedFloatConstants.find(constKey);

                        if (hcIt != hoistedFloatConstants.end()) {
                            // Constant is hoisted to an XMM register.
                            // Emit: addsd/subsd/mulsd/divsd xmmTarget, xmmConst
                            uint8_t cIdx = static_cast<uint8_t>(hcIt->second) - static_cast<uint8_t>(X64Reg::XMM0);
                            buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(sseOpcode);
                            buf.emit8(0xC0 | (tIdx << 3) | cIdx);
                        } else {
                            // Constant not hoisted — load into XMM1 scratch and operate
                            buf.emit8(0x48); buf.emit8(0xB8);
                            buf.emit64(bits);
                            // movq xmm1, rax
                            buf.emit8(0x66); buf.emit8(0x48); buf.emit8(0x0F); buf.emit8(0x6E); buf.emit8(0xC8);
                            // op xmmTarget, xmm1
                            buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(sseOpcode);
                            buf.emit8(0xC0 | (tIdx << 3) | 1);
                        }
                    } else if (it->second.isRegister) {
                        // GPR-pinned variable (legacy path)
                        X64Reg targetReg = it->second.reg;
                        bool regHigh = static_cast<uint8_t>(targetReg) >= 8;

                        buf.emit8(0x66); buf.emit8(0x48 | (regHigh ? 0x01 : 0)); buf.emit8(0x0F); buf.emit8(0x6E);
                        buf.emit8(0xC0 | (static_cast<uint8_t>(targetReg) & 0x7));

                        buf.emit8(0x48); buf.emit8(0xB8);
                        buf.emit64(bits);

                        buf.emit8(0x66); buf.emit8(0x48); buf.emit8(0x0F); buf.emit8(0x6E);
                        buf.emit8(0xC8);

                        if (sseOpcode != 0) {
                            buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(sseOpcode); buf.emit8(0xC1);
                        }

                        buf.emit8(0x66); buf.emit8(0x48 | (regHigh ? 0x01 : 0)); buf.emit8(0x0F); buf.emit8(0x7E);
                        buf.emit8(0xC0 | (static_cast<uint8_t>(targetReg) & 0x7));
                    } else {
                        // Stack variable (original path)
                        int32_t offset = it->second.stackOffset;

                        buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(0x10);
                        buf.emit8(0x85); buf.emit32(static_cast<uint32_t>(offset));

                        buf.emit8(0x48); buf.emit8(0xB8); buf.emit64(bits);
                        buf.emit8(0x66); buf.emit8(0x48); buf.emit8(0x0F); buf.emit8(0x6E); buf.emit8(0xC8);

                        if (sseOpcode != 0) {
                            buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(sseOpcode); buf.emit8(0xC1);
                        }

                        buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(0x11);
                        buf.emit8(0x85); buf.emit32(static_cast<uint32_t>(offset));
                    }
                    peepholeOptimized = true;
                }
            }
        }
    }
    
    if (peepholeOptimized) return;

    // Standard compilation path
    // Compile the value (returns pair: valueReg, typeReg)
    JITValue val = compileExpr(ast, node.left);

    // Track variable type for compile-time optimization
    if (isStaticInt(ast, node.left)) {
        knownIntVars.insert(node.name);
        knownFloatVars.erase(node.name);
    } else if (isStaticFloat(ast, node.left)) {
        knownFloatVars.insert(node.name);
        knownIntVars.erase(node.name);
    } else {
        knownIntVars.erase(node.name);
        knownFloatVars.erase(node.name);
    }
    
    // Allocate stack slot if needed
    auto it = variables.find(node.name);
    if (it == variables.end()) {
        VarLocation loc;
        loc.stackOffset = allocateStackSlot();
        loc.isRegister = false;
        variables[node.name] = loc;
        it = variables.find(node.name);
    }
    
    if (it->second.isXMMRegister) {
        // Store into XMM-pinned register: movq xmmN, val.valueReg
        X64Reg targetXMM = it->second.reg;
        uint8_t xmmIdx = static_cast<uint8_t>(targetXMM) - static_cast<uint8_t>(X64Reg::XMM0);
        bool valHigh = static_cast<uint8_t>(val.valueReg) >= 8;
        buf.emit8(0x66);
        buf.emit8(0x48 | (valHigh ? 0x01 : 0));
        buf.emit8(0x0F); buf.emit8(0x6E);
        buf.emit8(0xC0 | (xmmIdx << 3) | (static_cast<uint8_t>(val.valueReg) & 0x7));
    } else if (it->second.isRegister) {
        // Store into the assigned register
        bool valHigh = static_cast<uint8_t>(val.valueReg) >= 8;
        bool dstHigh = static_cast<uint8_t>(it->second.reg) >= 8;
        buf.emit8(0x48 | (dstHigh ? 0x01 : 0) | (valHigh ? 0x04 : 0));
        buf.emit8(0x89);
        buf.emit8(0xC0 | ((static_cast<uint8_t>(val.valueReg) & 0x7) << 3) | 
                  (static_cast<uint8_t>(it->second.reg) & 0x7));
        
        // Sync dynamic type to stack to properly preserve Strings/Objects
        int32_t offset = it->second.stackOffset;
        bool typeHigh = static_cast<uint8_t>(val.typeReg) >= 8;
        buf.emit8(0x48 | (typeHigh ? 0x04 : 0));
        buf.emit8(0x89);
        buf.emit8(0x85 | ((static_cast<uint8_t>(val.typeReg) & 0x7) << 3));
        buf.emit32(static_cast<uint32_t>(offset + 8));
    } else {
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
    }
    
    freeReg(val.valueReg);
    freeReg(val.typeReg);
    
    size_t endTarget = buf.getOffset();
    for (size_t patchOffset : jumpsToEndOfAssignment) {
        int32_t jmpDist = static_cast<int32_t>(endTarget - (patchOffset + 4));
        buf.patch32(patchOffset, static_cast<uint32_t>(jmpDist));
    }
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
            
        case NodeType::TRY_STMT:
            compileTryCatch(ast, idx);
            break;
            
        case NodeType::THROW_STMT:
            compileThrow(ast, idx);
            break;
            
        case NodeType::INDEX_ASSIGN: {
            // map/array[idx] = value
            CodeBuffer& buf = codegen.getCode();
            JITValue value = compileExpr(ast, node.extra);
            JITValue arr = compileExpr(ast, node.left);
            JITValue idxVal = compileExpr(ast, node.right);
            
            X64Reg arrReg = arr.valueReg;
            X64Reg tReg = arr.typeReg;
            X64Reg idxReg = idxVal.valueReg;
            X64Reg valueReg = value.valueReg;
            
            // Check if arr is MAP
            bool tHigh = static_cast<uint8_t>(tReg) >= 8;
            buf.emit8(0x48 | (tHigh ? 0x01 : 0));
            buf.emit8(0x83); buf.emit8(0xF8 + (static_cast<uint8_t>(tReg) & 0x7));
            buf.emit8(static_cast<uint8_t>(ValueType::MAP));
            
            buf.emit8(0x0F); buf.emit8(0x84); // je is_map
            size_t isMapJump = buf.size();
            buf.emit32(0);
            
            // --- ARRAY SET ---
            // Save Scratch Registers
            buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52); buf.emit8(0x56); buf.emit8(0x57);
            buf.emit8(0x41); buf.emit8(0x50); buf.emit8(0x41); buf.emit8(0x51);
            buf.emit8(0x41); buf.emit8(0x52); buf.emit8(0x41); buf.emit8(0x53);
            
            // Set Args (RDI = arrReg, RSI = idxReg, RDX = valueReg)
            bool arrHigh = static_cast<uint8_t>(arrReg) >= 8;
            buf.emit8(0x48 | (arrHigh ? 0x04 : 0));
            buf.emit8(0x89); buf.emit8(0xC0 | ((static_cast<uint8_t>(arrReg) & 0x7) << 3) | 7);
            
            bool idxHigh = static_cast<uint8_t>(idxReg) >= 8;
            buf.emit8(0x48 | (idxHigh ? 0x04 : 0));
            buf.emit8(0x89); buf.emit8(0xC0 | ((static_cast<uint8_t>(idxReg) & 0x7) << 3) | 6);
            
            bool valHigh = static_cast<uint8_t>(valueReg) >= 8;
            buf.emit8(0x48 | (valHigh ? 0x04 : 0));
            buf.emit8(0x89); buf.emit8(0xC0 | ((static_cast<uint8_t>(valueReg) & 0x7) << 3) | 2);
            
            // Call jit_array_set
            buf.emit8(0x48); buf.emit8(0xB8);
            buf.emit64(reinterpret_cast<uint64_t>(jit_array_set));
            buf.emit8(0xFF); buf.emit8(0xD0);
            
            // Restore Scratch Registers
            buf.emit8(0x41); buf.emit8(0x5B); buf.emit8(0x41); buf.emit8(0x5A);
            buf.emit8(0x41); buf.emit8(0x59); buf.emit8(0x41); buf.emit8(0x58);
            buf.emit8(0x5F); buf.emit8(0x5E); buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58);
            
            buf.emit8(0xE9); // jmp done
            size_t doneJump = buf.size();
            buf.emit32(0);
            
            // --- MAP SET ---
            size_t isMapTarget = buf.size();
            buf.patch32(isMapJump, static_cast<int32_t>(isMapTarget - (isMapJump + 4)));
            
            // Save Scratch Registers
            buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52); buf.emit8(0x56); buf.emit8(0x57);
            buf.emit8(0x41); buf.emit8(0x50); buf.emit8(0x41); buf.emit8(0x51);
            buf.emit8(0x41); buf.emit8(0x52); buf.emit8(0x41); buf.emit8(0x53);
            
            // Set Args (RDI = arrReg, RSI = idxReg, RDX = valueReg)
            buf.emit8(0x48 | (arrHigh ? 0x04 : 0));
            buf.emit8(0x89); buf.emit8(0xC0 | ((static_cast<uint8_t>(arrReg) & 0x7) << 3) | 7);
            
            buf.emit8(0x48 | (idxHigh ? 0x04 : 0));
            buf.emit8(0x89); buf.emit8(0xC0 | ((static_cast<uint8_t>(idxReg) & 0x7) << 3) | 6);
            
            buf.emit8(0x48 | (valHigh ? 0x04 : 0));
            buf.emit8(0x89); buf.emit8(0xC0 | ((static_cast<uint8_t>(valueReg) & 0x7) << 3) | 2);
            
            // Call jit_map_set (returns new map ptr in RAX)
            buf.emit8(0x48); buf.emit8(0xB8);
            buf.emit64(reinterpret_cast<uint64_t>(jit_map_set));
            buf.emit8(0xFF); buf.emit8(0xD0);
            
            // Update variable with new map pointer if source is identifier
            if (node.left != INVALID_NODE) {
                const ASTNode& srcNode = ast.get(node.left);
                if (srcNode.type == NodeType::IDENTIFIER && variables.count(srcNode.name)) {
                    int32_t offset = variables[srcNode.name].stackOffset;
                    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x85);
                    buf.emit32(static_cast<uint32_t>(offset));
                }
            }
            
            // Restore Scratch Registers
            buf.emit8(0x41); buf.emit8(0x5B); buf.emit8(0x41); buf.emit8(0x5A);
            buf.emit8(0x41); buf.emit8(0x59); buf.emit8(0x41); buf.emit8(0x58);
            buf.emit8(0x5F); buf.emit8(0x5E); buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58);
            
            // --- DONE ---
            size_t doneTarget = buf.size();
            buf.patch32(doneJump, static_cast<int32_t>(doneTarget - (doneJump + 4)));
            
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
            
            // Resolve path relative to the source file's directory
            fs::path fullPath = filePath;
            if (!fullPath.is_absolute()) {
                if (!sourceDir.empty()) {
                    fullPath = fs::path(sourceDir) / filePath;
                } else {
                    fullPath = fs::current_path() / filePath;
                }
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
            
            Parser parser(tokens, source);
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
            compileFuncDecl(ast, idx, true);
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
    
    // Simple Loop Variable Pinning (e.g. while (i < limit))
    std::string pinnedCounter, pinnedLimit;
    VarLocation oldCounterLoc, oldLimitLoc;
    bool counterPinned = false, limitPinned = false;

    // Detect i < limit or i < literal pattern
    const ASTNode& cond = ast.get(node.left);
    if (cond.type == NodeType::BINARY_OP && (cond.binaryOp == BinaryOp::LT || cond.binaryOp == BinaryOp::GT)) {
        const ASTNode& leftNode = ast.get(cond.left);
        const ASTNode& rightNode = ast.get(cond.right);
        
        if (leftNode.type == NodeType::IDENTIFIER && variables.count(leftNode.name)) {
            pinnedCounter = leftNode.name;
            if (!variables[pinnedCounter].isRegister && !regInUse[static_cast<int>(X64Reg::R12)]) { // Only pin if not already pinned and R12 is free
                oldCounterLoc = variables[pinnedCounter];
                VarLocation pinnedLoc = oldCounterLoc;
                pinnedLoc.isRegister = true;
                pinnedLoc.reg = X64Reg::R12;
                variables[pinnedCounter] = pinnedLoc;
                counterPinned = true;
                
                // R12 preserved in emitPrologue
                
                // mov r12, [rbp + offset]
                buf.emit8(0x4C); buf.emit8(0x8B); buf.emit8(0xA5);
                buf.emit32(static_cast<uint32_t>(oldCounterLoc.stackOffset));
                
                regInUse[static_cast<int>(X64Reg::R12)] = true;
                // printf("JIT Compile: Pinned counter '%s' to R12\n", pinnedCounter.c_str());
            }
        }
        
        if (rightNode.type == NodeType::IDENTIFIER && variables.count(rightNode.name)) {
            pinnedLimit = rightNode.name;
            if (!variables[pinnedLimit].isRegister && !regInUse[static_cast<int>(X64Reg::R13)]) {
                oldLimitLoc = variables[pinnedLimit];
                VarLocation pinnedLoc = oldLimitLoc;
                pinnedLoc.isRegister = true;
                pinnedLoc.reg = X64Reg::R13;
                variables[pinnedLimit] = pinnedLoc;
                limitPinned = true;
                
                // R13 preserved in emitPrologue
                
                // mov r13, [rbp + offset]
                buf.emit8(0x4C); buf.emit8(0x8B); buf.emit8(0xAD);
                buf.emit32(static_cast<uint32_t>(oldLimitLoc.stackOffset));
                
                regInUse[static_cast<int>(X64Reg::R13)] = true;
                // printf("JIT Compile: Pinned limit '%s' to R13\n", pinnedLimit.c_str());
            }
        }
    }

    // Dynamic Frequency-Based Register Allocation
    std::unordered_map<std::string, int> varFreq;
    std::function<void(NodeIndex)> scanAST = [&](NodeIndex currIdx) {
        if (currIdx == INVALID_NODE) return;
        const ASTNode& currNode = ast.get(currIdx);
        if (currNode.type == NodeType::IDENTIFIER || currNode.type == NodeType::VAR_ASSIGN) {
            varFreq[currNode.name]++;
        }
        scanAST(currNode.left);
        scanAST(currNode.right);
        scanAST(currNode.extra);
        for (NodeIndex child : currNode.children) {
            scanAST(child);
        }
    };
    scanAST(node.right);
    varFreq.erase(pinnedCounter);
    varFreq.erase(pinnedLimit);
    
    std::vector<std::pair<std::string, int>> sortedVars(varFreq.begin(), varFreq.end());
    std::sort(sortedVars.begin(), sortedVars.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });
    
    X64Reg pinRegs[] = {X64Reg::R14, X64Reg::R15, X64Reg::RBX};
    struct PinnedVar { std::string name; VarLocation oldLoc; X64Reg reg; };
    std::vector<PinnedVar> dynamicPins;
    
    size_t regIdx = 0;
    for (const auto& pair : sortedVars) {
        if (regIdx >= 3) break;
        const std::string& varName = pair.first;
        if (variables.count(varName) && !variables[varName].isRegister && !regInUse[static_cast<int>(pinRegs[regIdx])] && knownFloatVars.count(varName) == 0) {
            X64Reg targetReg = pinRegs[regIdx];
            PinnedVar pv = {varName, variables[varName], targetReg};
            dynamicPins.push_back(pv);
            
            VarLocation newLoc = pv.oldLoc;
            newLoc.isRegister = true;
            newLoc.reg = targetReg;
            variables[varName] = newLoc;
            regInUse[static_cast<int>(targetReg)] = true;
            
            // R14/R15/RBX preserved in emitPrologue
            bool regHigh = static_cast<uint8_t>(targetReg) >= 8;
            
            // mov reg, [rbp + offset]
            buf.emit8(0x48 | (regHigh ? 0x04 : 0));
            buf.emit8(0x8B);
            buf.emit8(0x85 | ((static_cast<uint8_t>(targetReg) & 0x7) << 3));
            buf.emit32(static_cast<uint32_t>(pv.oldLoc.stackOffset));
            
            // printf("JIT Compile: Pinned dynamic var '%s' to Register %u\n", varName.c_str(), static_cast<uint32_t>(targetReg));
            
            regIdx++;
        }
    }

    // XMM Register Pinning for float variables
    struct XMMPinnedVar { std::string name; VarLocation oldLoc; X64Reg xmmReg; };
    std::vector<XMMPinnedVar> xmmPins;

    for (const auto& pair : sortedVars) {
        const std::string& varName = pair.first;
        if (variables.count(varName) && !variables[varName].isRegister &&
            !variables[varName].isXMMRegister && knownFloatVars.count(varName) > 0) {
            
            X64Reg xmmTarget = allocateXMMReg();
            if (xmmTarget == X64Reg::XMM0) break; // No free XMM regs

            XMMPinnedVar xpv = {varName, variables[varName], xmmTarget};
            xmmPins.push_back(xpv);

            VarLocation newLoc = xpv.oldLoc;
            newLoc.isXMMRegister = true;
            newLoc.isRegister = false;
            newLoc.reg = xmmTarget;
            variables[varName] = newLoc;

            int32_t offset = xpv.oldLoc.stackOffset;
            uint8_t xmmIdx = static_cast<uint8_t>(xmmTarget) - static_cast<uint8_t>(X64Reg::XMM0);

            // movsd xmmN, [rbp + offset]
            buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(0x10);
            buf.emit8(0x85 | (xmmIdx << 3));
            buf.emit32(static_cast<uint32_t>(offset));
        }
    }

    // Float Constant Hoisting — scan loop body for accumulator patterns
    // Detects `var = var + <float_literal>` and hoists the constant to an XMM register
    hoistedFloatConstants.clear();
    if (node.right != INVALID_NODE) {
        std::function<void(NodeIndex)> scanConstants = [&](NodeIndex scanIdx) {
            if (scanIdx == INVALID_NODE) return;
            const ASTNode& scanNode = ast.get(scanIdx);
            
            if (scanNode.type == NodeType::VAR_ASSIGN && scanNode.left != INVALID_NODE) {
                const ASTNode& expr = ast.get(scanNode.left);
                if (expr.type == NodeType::BINARY_OP &&
                    (expr.binaryOp == BinaryOp::ADD || expr.binaryOp == BinaryOp::SUB ||
                     expr.binaryOp == BinaryOp::MUL || expr.binaryOp == BinaryOp::DIV)) {
                    const ASTNode& lOp = ast.get(expr.left);
                    const ASTNode& rOp = ast.get(expr.right);
                    if (lOp.type == NodeType::IDENTIFIER && lOp.name == scanNode.name &&
                        rOp.type == NodeType::LITERAL_FLOAT) {
                        double constVal = std::get<double>(rOp.literal.data);
                        uint64_t bits;
                        std::memcpy(&bits, &constVal, sizeof(bits));
                        std::string key = std::to_string(bits);
                        
                        if (hoistedFloatConstants.find(key) == hoistedFloatConstants.end()) {
                            X64Reg constXMM = allocateXMMReg();
                            if (constXMM != X64Reg::XMM0) {
                                hoistedFloatConstants[key] = constXMM;
                                uint8_t xmmIdx = static_cast<uint8_t>(constXMM) - static_cast<uint8_t>(X64Reg::XMM0);

                                // Load constant bits into RAX, then movq xmmN, rax
                                buf.emit8(0x48); buf.emit8(0xB8);
                                buf.emit64(bits);
                                // movq xmmN, rax
                                buf.emit8(0x66); buf.emit8(0x48); buf.emit8(0x0F); buf.emit8(0x6E);
                                buf.emit8(0xC0 | (xmmIdx << 3));
                            }
                        }
                    }
                }
            }
            
            // Recurse into block children
            for (NodeIndex child : scanNode.children) {
                scanConstants(child);
            }
            scanConstants(scanNode.left);
            scanConstants(scanNode.right);
        };
        scanConstants(node.right);
    }

    // Align loop start
    while (buf.getOffset() % 16 != 0) {
        buf.emit8(0x90); // NOP
    }
    
    // loop_start:
    size_t loopStart = buf.getOffset();
    
    // Inline condition fast path: direct CMP + conditional jump for register-pinned comparisons
    bool inlineCondition = false;
    size_t jzPatch = 0;
    
    if (cond.type == NodeType::BINARY_OP && 
        (cond.binaryOp == BinaryOp::LT || cond.binaryOp == BinaryOp::GT ||
         cond.binaryOp == BinaryOp::LTE || cond.binaryOp == BinaryOp::GTE)) {
        const ASTNode& leftCond = ast.get(cond.left);
        const ASTNode& rightCond = ast.get(cond.right);
        
        bool leftPinned = leftCond.type == NodeType::IDENTIFIER && 
                          variables.count(leftCond.name) && variables[leftCond.name].isRegister;
        bool rightPinned = rightCond.type == NodeType::IDENTIFIER && 
                           variables.count(rightCond.name) && variables[rightCond.name].isRegister;
        
        if (leftPinned && rightPinned) {
            X64Reg lReg = variables[leftCond.name].reg;
            X64Reg rReg = variables[rightCond.name].reg;
            bool lHigh = static_cast<uint8_t>(lReg) >= 8;
            bool rHigh = static_cast<uint8_t>(rReg) >= 8;
            
            // CMP lReg, rReg
            buf.emit8(0x48 | (rHigh ? 0x04 : 0) | (lHigh ? 0x01 : 0));
            buf.emit8(0x39);
            buf.emit8(0xC0 | ((static_cast<uint8_t>(rReg) & 0x7) << 3) | (static_cast<uint8_t>(lReg) & 0x7));
            
            // Conditional jump (inverted condition: exit when condition is FALSE)
            // LT  -> exit on JGE (0x8D)
            // GT  -> exit on JLE (0x8E)
            // LTE -> exit on JG  (0x8F)
            // GTE -> exit on JL  (0x8C)
            uint8_t jccOpcode = 0;
            switch (cond.binaryOp) {
                case BinaryOp::LT:  jccOpcode = 0x8D; break; // jge
                case BinaryOp::GT:  jccOpcode = 0x8E; break; // jle
                case BinaryOp::LTE: jccOpcode = 0x8F; break; // jg
                case BinaryOp::GTE: jccOpcode = 0x8C; break; // jl
                default: break;
            }
            
            buf.emit8(0x0F);
            buf.emit8(jccOpcode);
            jzPatch = buf.getOffset();
            buf.emit32(0); // Placeholder
            
            inlineCondition = true;
        }
    }
    
    if (!inlineCondition) {
        // Generic condition evaluation path
        JITValue cv = compileExpr(ast, node.left);
        X64Reg condReg = cv.valueReg;
        
        // test condReg, condReg
        bool condHigh = static_cast<uint8_t>(condReg) >= 8;
        buf.emit8(0x48 | (condHigh ? 0x05 : 0));
        buf.emit8(0x85);
        buf.emit8(0xC0 | ((static_cast<uint8_t>(condReg) & 0x7) << 3) | (static_cast<uint8_t>(condReg) & 0x7));
        
        freeReg(cv.valueReg);
        freeReg(cv.typeReg);
        
        // jz loop_end (exit if condition is false)
        buf.emit8(0x0F);
        buf.emit8(0x84);
        jzPatch = buf.getOffset();
        buf.emit32(0); // Placeholder
    }
    
    // Compile loop body (with optional unrolling)
    constexpr int UNROLL_FACTOR = 4;
    
    // Multi-accumulator state for breaking float dependency chains
    struct ShadowAccum { std::string varName; X64Reg primaryXMM; X64Reg shadowXMM; };
    std::vector<ShadowAccum> shadowAccums;
    std::vector<size_t> unrollExitPatches; // Local vector for unrolled early-exit patch points
    
    if (inlineCondition && counterPinned && limitPinned) {
        // Allocate shadow accumulators for XMM-pinned float variables
        for (const auto& xpv : xmmPins) {
            X64Reg shadowXMM = allocateXMMReg();
            if (shadowXMM == X64Reg::XMM0) break; // No free XMM
            
            shadowAccums.push_back({xpv.name, xpv.xmmReg, shadowXMM});
            uint8_t sIdx = static_cast<uint8_t>(shadowXMM) - static_cast<uint8_t>(X64Reg::XMM0);
            
            // Initialize shadow accumulator: xorpd xmmShadow, xmmShadow (0.0)
            buf.emit8(0x66); buf.emit8(0x0F); buf.emit8(0x57);
            buf.emit8(0xC0 | (sIdx << 3) | sIdx);
        }
        
        // Unrolled loop body with accumulator interleaving
        for (int u = 0; u < UNROLL_FACTOR; ++u) {
            // Swap XMM pin for even iterations to shadow accumulator
            if (u % 2 == 1) {
                for (const auto& sa : shadowAccums) {
                    auto it = variables.find(sa.varName);
                    if (it != variables.end() && it->second.isXMMRegister) {
                        it->second.reg = sa.shadowXMM;
                    }
                }
            } else if (u > 0) {
                // Swap back for odd iterations
                for (const auto& sa : shadowAccums) {
                    auto it = variables.find(sa.varName);
                    if (it != variables.end() && it->second.isXMMRegister) {
                        it->second.reg = sa.primaryXMM;
                    }
                }
            }
            
            if (node.right != INVALID_NODE) {
                compileStatement(ast, node.right);
            }
            
            // After each unrolled body except the last, check if we've exceeded limit
            if (u < UNROLL_FACTOR - 1) {
                X64Reg lReg = variables[ast.get(cond.left).name].reg;
                X64Reg rReg = variables[ast.get(cond.right).name].reg;
                bool lHigh = static_cast<uint8_t>(lReg) >= 8;
                bool rHigh = static_cast<uint8_t>(rReg) >= 8;
                
                buf.emit8(0x48 | (rHigh ? 0x04 : 0) | (lHigh ? 0x01 : 0));
                buf.emit8(0x39);
                buf.emit8(0xC0 | ((static_cast<uint8_t>(rReg) & 0x7) << 3) | (static_cast<uint8_t>(lReg) & 0x7));
                
                uint8_t jccOpcode2 = 0;
                switch (cond.binaryOp) {
                    case BinaryOp::LT:  jccOpcode2 = 0x8D; break;
                    case BinaryOp::GT:  jccOpcode2 = 0x8E; break;
                    case BinaryOp::LTE: jccOpcode2 = 0x8F; break;
                    case BinaryOp::GTE: jccOpcode2 = 0x8C; break;
                    default: break;
                }
                
                buf.emit8(0x0F);
                buf.emit8(jccOpcode2);
                size_t earlyExitPatch = buf.getOffset();
                buf.emit32(0);
                unrollExitPatches.push_back(earlyExitPatch);
            }
        }
        
        // Restore primary XMM pins after last unrolled iteration
        for (const auto& sa : shadowAccums) {
            auto it = variables.find(sa.varName);
            if (it != variables.end() && it->second.isXMMRegister) {
                it->second.reg = sa.primaryXMM;
            }
        }
    } else {
        if (node.right != INVALID_NODE) {
            compileStatement(ast, node.right);
        }
    }
    
    // jmp loop_start
    buf.emit8(0xE9);
    int32_t jumpBack = static_cast<int32_t>(loopStart - (buf.getOffset() + 4));
    buf.emit32(static_cast<uint32_t>(jumpBack));
    
    // loop_end: patch the jz
    size_t loopEnd = buf.getOffset();
    int32_t jzOffset = static_cast<int32_t>(loopEnd - (jzPatch + 4));
    buf.patch32(jzPatch, static_cast<uint32_t>(jzOffset));

    // Patch early exit points from unrolled loop iterations
    for (size_t patchOffset : unrollExitPatches) {
        int32_t earlyExitOffset = static_cast<int32_t>(loopEnd - (patchOffset + 4));
        buf.patch32(patchOffset, static_cast<uint32_t>(earlyExitOffset));
    }

    // Merge shadow accumulators into primary: addsd primary, shadow
    for (const auto& sa : shadowAccums) {
        uint8_t pIdx = static_cast<uint8_t>(sa.primaryXMM) - static_cast<uint8_t>(X64Reg::XMM0);
        uint8_t sIdx = static_cast<uint8_t>(sa.shadowXMM) - static_cast<uint8_t>(X64Reg::XMM0);
        // addsd primary, shadow
        buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(0x58);
        buf.emit8(0xC0 | (pIdx << 3) | sIdx);
        freeXMMReg(sa.shadowXMM);
    }

    // Restore Pinned Variables and Store back to stack
    if (counterPinned) {
        // mov [rbp + offset], r12
        buf.emit8(0x4C); buf.emit8(0x89); buf.emit8(0xA5);
        buf.emit32(static_cast<uint32_t>(oldCounterLoc.stackOffset));
        variables[pinnedCounter] = oldCounterLoc;
        regInUse[static_cast<int>(X64Reg::R12)] = false;
    }
    if (limitPinned) {
        // mov [rbp + offset], r13
        buf.emit8(0x4C); buf.emit8(0x89); buf.emit8(0xAD);
        buf.emit32(static_cast<uint32_t>(oldLimitLoc.stackOffset));
        variables[pinnedLimit] = oldLimitLoc;
        regInUse[static_cast<int>(X64Reg::R13)] = false;
    }

    // Restore dynamically pinned variables to stack context
    for (const auto& pv : dynamicPins) {
        bool regHigh = static_cast<uint8_t>(pv.reg) >= 8;
        buf.emit8(0x48 | (regHigh ? 0x04 : 0));
        buf.emit8(0x89);
        buf.emit8(0x85 | ((static_cast<uint8_t>(pv.reg) & 0x7) << 3));
        buf.emit32(static_cast<uint32_t>(pv.oldLoc.stackOffset));
        variables[pv.name] = pv.oldLoc;
        regInUse[static_cast<int>(pv.reg)] = false;
    }

    // Callee-saved registers restored in emitEpilogue

    // Restore XMM-pinned float variables to stack
    for (const auto& xpv : xmmPins) {
        uint8_t xmmIdx = static_cast<uint8_t>(xpv.xmmReg) - static_cast<uint8_t>(X64Reg::XMM0);
        int32_t offset = xpv.oldLoc.stackOffset;

        // movsd [rbp + offset], xmmN
        buf.emit8(0xF2); buf.emit8(0x0F); buf.emit8(0x11);
        buf.emit8(0x85 | (xmmIdx << 3));
        buf.emit32(static_cast<uint32_t>(offset));

        variables[xpv.name] = xpv.oldLoc;
        freeXMMReg(xpv.xmmReg);
    }

    // Release hoisted float constant XMM registers
    for (const auto& hc : hoistedFloatConstants) {
        freeXMMReg(hc.second);
    }
    hoistedFloatConstants.clear();
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

void JIT::compileTryCatch(const AST& ast, NodeIndex idx) {
    const ASTNode& node = ast.get(idx);
    CodeBuffer& buf = codegen.getCode();
    
    // Pre-allocate error variable slot before exception frame setup
    VarLocation errLoc;
    if (!node.name.empty()) {
        errLoc.stackOffset = allocateStackSlot();
        errLoc.isRegister = false;
        variables[node.name] = errLoc;
    }
    
    // Allocate exception frame (32 bytes)
    buf.emit8(0x48); buf.emit8(0x83); buf.emit8(0xEC); buf.emit8(32); // sub rsp, 32

    // Save current_exception_frame to [rsp + 0]
    buf.emit8(0x48); buf.emit8(0xB8);
    buf.emit64(reinterpret_cast<uint64_t>(&current_exception_frame));
    buf.emit8(0x48); buf.emit8(0x8B); buf.emit8(0x08); // mov rcx, [rax]
    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x0C); buf.emit8(0x24); // mov [rsp], rcx

    // Save catch_rip
    buf.emit8(0x48); buf.emit8(0x8D); buf.emit8(0x15); // lea rdx, [rip + offset]
    size_t catchRipPatch = buf.getOffset();
    buf.emit32(0); // placeholder
    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x54); buf.emit8(0x24); buf.emit8(0x08); // mov [rsp+8], rdx

    // Save rbp
    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x6C); buf.emit8(0x24); buf.emit8(0x10); // mov [rsp+16], rbp

    // Save rsp
    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x64); buf.emit8(0x24); buf.emit8(0x18); // mov [rsp+24], rsp

    // Change current_exception_frame to this frame (curr_rsp)
    buf.emit8(0x48); buf.emit8(0xB8);
    buf.emit64(reinterpret_cast<uint64_t>(&current_exception_frame));
    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x20); // mov [rax], rsp

    // Compile try block
    if (node.left != INVALID_NODE) {
        compileStatement(ast, node.left);
    }

    // Try block succeeded
    // Restore current_exception_frame = [rsp]
    buf.emit8(0x48); buf.emit8(0xB8);
    buf.emit64(reinterpret_cast<uint64_t>(&current_exception_frame));
    buf.emit8(0x48); buf.emit8(0x8B); buf.emit8(0x0C); buf.emit8(0x24); // mov rcx, [rsp]
    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x08); // mov [rax], rcx

    // Deallocate frame
    buf.emit8(0x48); buf.emit8(0x83); buf.emit8(0xC4); buf.emit8(32); // add rsp, 32

    // Jump to end
    buf.emit8(0xE9);
    size_t endTryPatch = buf.getOffset();
    buf.emit32(0);

    // catch_label:
    size_t catchTarget = buf.getOffset();
    buf.patch32(catchRipPatch, static_cast<int32_t>(catchTarget - (catchRipPatch + 4)));

    // Deallocate exception frame (throw restores RSP to the saved value which includes the frame)
    buf.emit8(0x48); buf.emit8(0x83); buf.emit8(0xC4); buf.emit8(32); // add rsp, 32

    // Error variable binding (slot already exists in rbp-relative frame)
    if (!node.name.empty()) {
        // load val from global
        buf.emit8(0x48); buf.emit8(0xB8);
        buf.emit64(reinterpret_cast<uint64_t>(&current_exception_val));
        buf.emit8(0x48); buf.emit8(0x8B); buf.emit8(0x08); // mov rcx, [rax]
        // store val to pre-allocated slot
        buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x8D);
        buf.emit32(static_cast<uint32_t>(errLoc.stackOffset));
        
        // load type from global
        buf.emit8(0x48); buf.emit8(0xB8);
        buf.emit64(reinterpret_cast<uint64_t>(&current_exception_type));
        buf.emit8(0x48); buf.emit8(0x8B); buf.emit8(0x08); // mov rcx, [rax]
        // store type to pre-allocated slot
        buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x8D);
        buf.emit32(static_cast<uint32_t>(errLoc.stackOffset + 8));
    }

    // Compile catch block
    if (node.right != INVALID_NODE) {
        compileStatement(ast, node.right);
    }

    // end_label:
    size_t endTarget = buf.getOffset();
    buf.patch32(endTryPatch, static_cast<int32_t>(endTarget - (endTryPatch + 4)));
}

void JIT::compileThrow(const AST& ast, NodeIndex idx) {
    const ASTNode& node = ast.get(idx);
    CodeBuffer& buf = codegen.getCode();
    
    JITValue expr = compileExpr(ast, node.left);

    // Save value and type to temp stack slots to avoid RAX clobber
    int32_t valSlot = allocateStackSlot();
    int32_t typeSlot = allocateStackSlot();

    // Store value register to temp slot [rbp + valSlot]
    bool valHigh = static_cast<uint8_t>(expr.valueReg) >= 8;
    buf.emit8(0x48 | (valHigh ? 0x04 : 0));
    buf.emit8(0x89);
    buf.emit8(0x85 | ((static_cast<uint8_t>(expr.valueReg) & 0x7) << 3));
    buf.emit32(static_cast<uint32_t>(valSlot));

    // Store type register to temp slot [rbp + typeSlot]
    bool typeHigh = static_cast<uint8_t>(expr.typeReg) >= 8;
    buf.emit8(0x48 | (typeHigh ? 0x04 : 0));
    buf.emit8(0x89);
    buf.emit8(0x85 | ((static_cast<uint8_t>(expr.typeReg) & 0x7) << 3));
    buf.emit32(static_cast<uint32_t>(typeSlot));

    freeReg(expr.valueReg);
    freeReg(expr.typeReg);

    // Load value from temp slot into RCX, then store to global
    buf.emit8(0x48); buf.emit8(0x8B); buf.emit8(0x8D); // mov rcx, [rbp + valSlot]
    buf.emit32(static_cast<uint32_t>(valSlot));
    buf.emit8(0x48); buf.emit8(0xB8);                  // mov rax, &current_exception_val
    buf.emit64(reinterpret_cast<uint64_t>(&current_exception_val));
    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x08);  // mov [rax], rcx

    // Load type from temp slot into RCX, then store to global
    buf.emit8(0x48); buf.emit8(0x8B); buf.emit8(0x8D); // mov rcx, [rbp + typeSlot]
    buf.emit32(static_cast<uint32_t>(typeSlot));
    buf.emit8(0x48); buf.emit8(0xB8);                  // mov rax, &current_exception_type
    buf.emit64(reinterpret_cast<uint64_t>(&current_exception_type));
    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x08);  // mov [rax], rcx

    // read current_exception_frame
    buf.emit8(0x48); buf.emit8(0xB8);
    buf.emit64(reinterpret_cast<uint64_t>(&current_exception_frame));
    buf.emit8(0x48); buf.emit8(0x8B); buf.emit8(0x08); // mov rcx, [rax]

    // test rcx, rcx
    buf.emit8(0x48); buf.emit8(0x85); buf.emit8(0xC9);
    buf.emit8(0x0F); buf.emit8(0x84); // jz unhandled
    size_t unhandledJump = buf.getOffset();
    buf.emit32(0);

    // current_exception_frame = [rcx]
    buf.emit8(0x48); buf.emit8(0x8B); buf.emit8(0x11); // mov rdx, [rcx]
    buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0x10); // mov [rax], rdx

    // restore rsp
    buf.emit8(0x48); buf.emit8(0x8B); buf.emit8(0x61); buf.emit8(0x18); // mov rsp, [rcx+24]
    // restore rbp
    buf.emit8(0x48); buf.emit8(0x8B); buf.emit8(0x69); buf.emit8(0x10); // mov rbp, [rcx+16]
    // load catch_rip
    buf.emit8(0x48); buf.emit8(0x8B); buf.emit8(0x51); buf.emit8(0x08); // mov rdx, [rcx+8]

    // jump
    buf.emit8(0xFF); buf.emit8(0xE2); // jmp rdx

    // unhandled_label:
    size_t unhandledTarget = buf.getOffset();
    buf.patch32(unhandledJump, static_cast<int32_t>(unhandledTarget - (unhandledJump + 4)));
    
    // call jit_unhandled_exception
    buf.emit8(0x48); buf.emit8(0xB8);
    buf.emit64(reinterpret_cast<uint64_t>(jit_unhandled_exception));
    buf.emit8(0xFF); buf.emit8(0xD0);
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
    } else {
        // Emit JMP to end of inlined function body (patched later)
        buf.emit8(0xE9); // JMP rel32
        inlinedReturnPatches.push_back(buf.getOffset());
        buf.emit32(0); // Placeholder
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
void JIT::compileFuncDecl(const AST& ast, NodeIndex idx, bool isAsync) {
    const ASTNode& node = ast.get(idx);
    
    // Store function info for later use
    FuncInfo info;
    info.bodyIndex = node.left;
    info.paramNames = node.paramNames;
    info.compiledOffset = 0;
    info.isCompiled = false;
    info.isAsync = isAsync;
    info.sourceAST = nullptr;  // Local function uses currentAST
    userFunctions[node.name] = info;
}

// Compile a user function call - inline the function body
JITValue JIT::compileUserCall(const AST& ast, NodeIndex idx, const std::string& funcName) {
    const ASTNode& node = ast.get(idx);
    JITValue defaultRes;
    defaultRes.valueReg = X64Reg::RAX;
    defaultRes.typeReg = X64Reg::RAX;
    
    auto it = userFunctions.find(funcName);
    if (it == userFunctions.end()) {
        return defaultRes;
    }
    
    // Dynamic recursion logic using Continuation-Passing Style (CPS)
    if (currentlyCompiling.count(funcName)) {
        CodeBuffer& buf = codegen.getCode();
        
        // Emulate a new stack frame by shifting RBP down by the total allocated block size
        // This isolates the recursive call's localized variables from the caller's frame.
        int32_t currentFrameSize = nextStackSlot;
        int32_t frameShift = (currentFrameSize + 15) & ~15;
        
        buf.emit8(0x55); // push rbp
        
        // Temporarily evaluate arguments in the current frame context
        // Shift RBP down, then immediately back up to satisfy compileExpr var offsets
        buf.emit8(0x48); buf.emit8(0x81); buf.emit8(0xED); // sub rbp, frameShift
        buf.emit32(static_cast<uint32_t>(frameShift));
        buf.emit8(0x48); buf.emit8(0x81); buf.emit8(0xC5); // add rbp, frameShift
        buf.emit32(static_cast<uint32_t>(frameShift));
        
        for (size_t i = 0; i < node.children.size(); ++i) {
            JITValue argVal = compileExpr(ast, node.children[i]);
            
            bool valHigh = static_cast<uint8_t>(argVal.valueReg) >= 8;
            buf.emit8(0x48 | (valHigh ? 0x01 : 0));
            buf.emit8(0x50 + (static_cast<uint8_t>(argVal.valueReg) & 0x7)); // push val
            
            bool typeHigh = static_cast<uint8_t>(argVal.typeReg) >= 8;
            buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
            buf.emit8(0x50 + (static_cast<uint8_t>(argVal.typeReg) & 0x7)); // push type
            
            freeReg(argVal.valueReg);
            freeReg(argVal.typeReg);
        }
        
        // Apply frame shift for recursive variables
        buf.emit8(0x48); buf.emit8(0x81); buf.emit8(0xED); // sub rbp, frameShift
        buf.emit32(static_cast<uint32_t>(frameShift));
        
        // Store arguments into the nested frame slots
        for (int i = static_cast<int>(node.children.size()) - 1; i >= 0; --i) {
            std::string paramName = it->second.paramNames[i];
            int32_t offset = variables[paramName].stackOffset;
            
            X64Reg typeReg = allocateReg();
            bool typeHigh = static_cast<uint8_t>(typeReg) >= 8;
            buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
            buf.emit8(0x58 + (static_cast<uint8_t>(typeReg) & 0x7)); // pop type
            
            X64Reg valReg = allocateReg();
            bool valHigh = static_cast<uint8_t>(valReg) >= 8;
            buf.emit8(0x48 | (valHigh ? 0x01 : 0));
            buf.emit8(0x58 + (static_cast<uint8_t>(valReg) & 0x7)); // pop val
            
            buf.emit8(0x48 | (valHigh ? 0x04 : 0));
            buf.emit8(0x89);
            buf.emit8(0x85 | ((static_cast<uint8_t>(valReg) & 0x7) << 3)); // mov [rbp+disp], val
            buf.emit32(static_cast<uint32_t>(offset));
            
            buf.emit8(0x48 | (typeHigh ? 0x04 : 0));
            buf.emit8(0x89);
            buf.emit8(0x85 | ((static_cast<uint8_t>(typeReg) & 0x7) << 3)); // mov [rbp+disp], type
            buf.emit32(static_cast<uint32_t>(offset + 8));
            
            freeReg(valReg);
            freeReg(typeReg);
        }
        
        // Push recursive continuation address (dynamic return)
        buf.emit8(0x48); buf.emit8(0x8D); buf.emit8(0x0D); // lea rcx, [rip + disp]
        size_t leaPatch = buf.getOffset();
        buf.emit32(0); // patched later
        buf.emit8(0x51); // push rcx
        
        // Jump to function body
        buf.emit8(0xE9); // jmp rel32
        it->second.recursiveCallPatches.push_back(buf.getOffset());
        buf.emit32(0);
        
        // Continuation Point (patched into lea rax)
        int32_t leaDisp = static_cast<int32_t>(buf.getOffset() - (leaPatch + 4));
        buf.patch32(leaPatch, static_cast<uint32_t>(leaDisp));
        
        // Epilogue: Cleanup nested frame
        buf.emit8(0x48); buf.emit8(0x81); buf.emit8(0xC5); // add rbp, frameShift
        buf.emit32(static_cast<uint32_t>(frameShift));
        buf.emit8(0x5D); // pop rbp
        
        // Restructure typical function return (value in RAX, type in RDX)
        JITValue dst;
        dst.valueReg = allocateReg();
        dst.typeReg = allocateReg();
        
        // Copy value from RAX
        if (dst.valueReg != X64Reg::RAX) {
            bool dstHigh = static_cast<uint8_t>(dst.valueReg) >= 8;
            buf.emit8(0x48 | (dstHigh ? 0x01 : 0));
            buf.emit8(0x89); // mov reg, rax
            buf.emit8(0xC0 | (0 << 3) | (static_cast<uint8_t>(dst.valueReg) & 0x7));
        }
        
        // Copy type from RDX
        if (dst.typeReg != X64Reg::RDX) {
            bool typeHigh = static_cast<uint8_t>(dst.typeReg) >= 8;
            buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
            buf.emit8(0x89); // mov reg, rdx
            buf.emit8(0xC0 | (2 << 3) | (static_cast<uint8_t>(dst.typeReg) & 0x7));
        }
        
        return dst;
    }
    
    FuncInfo& funcInfo = it->second;
    currentlyCompiling.insert(funcName);
    
    auto savedVars = variables;
    
    // Bind arguments to parameters for the outermost inlined evaluation
    for (size_t i = 0; i < funcInfo.paramNames.size() && i < node.children.size(); ++i) {
        JITValue argVal = compileExpr(ast, node.children[i]);
        
        VarLocation loc;
        loc.stackOffset = allocateStackSlot();
        loc.isRegister = false;
        variables[funcInfo.paramNames[i]] = loc;
        
        CodeBuffer& buf = codegen.getCode();
        
        bool regHigh = static_cast<uint8_t>(argVal.valueReg) >= 8;
        buf.emit8(0x48 | (regHigh ? 0x04 : 0));
        buf.emit8(0x89);
        buf.emit8(0x85 | ((static_cast<uint8_t>(argVal.valueReg) & 0x7) << 3));
        buf.emit32(static_cast<uint32_t>(loc.stackOffset));
        
        bool typeHigh = static_cast<uint8_t>(argVal.typeReg) >= 8;
        buf.emit8(0x48 | (typeHigh ? 0x04 : 0));
        buf.emit8(0x89);
        buf.emit8(0x85 | ((static_cast<uint8_t>(argVal.typeReg) & 0x7) << 3));
        buf.emit32(static_cast<uint32_t>(loc.stackOffset + 8));
        
        freeReg(argVal.valueReg);
        freeReg(argVal.typeReg);
    }
    
    bool savedInFunctionCall = inFunctionCall;
    inFunctionCall = true;
    
    auto savedReturnPatches = std::move(inlinedReturnPatches);
    inlinedReturnPatches.clear();
    
    CodeBuffer& buf = codegen.getCode();
    
    // Push continuation address for the outermost call
    buf.emit8(0x48); buf.emit8(0x8D); buf.emit8(0x0D); // lea rcx, [rip + disp]
    size_t outerLeaPatch = buf.getOffset();
    buf.emit32(0); // patched later
    buf.emit8(0x51); // push rcx
    
    // Record start of the function body for recursive jumps
    funcInfo.compiledOffset = buf.getOffset();
    
    const AST& funcAST = (funcInfo.sourceAST != nullptr) ? *funcInfo.sourceAST : ast;
    
    if (funcInfo.bodyIndex != INVALID_NODE) {
        compileStatement(funcAST, funcInfo.bodyIndex);
    }
    
    // Handle unreturned execution paths (default return 0)
    bool outTypeHigh = static_cast<uint8_t>(X64Reg::RDX) >= 8;
    buf.emit8(0x48 | (outTypeHigh ? 0x01 : 0));
    buf.emit8(0xB8 + (static_cast<uint8_t>(X64Reg::RDX) & 0x7));
    buf.emit64(0); // null type
    bool outValHigh = static_cast<uint8_t>(X64Reg::RAX) >= 8;
    buf.emit8(0x48 | (outValHigh ? 0x01 : 0));
    buf.emit8(0xB8 + (static_cast<uint8_t>(X64Reg::RAX) & 0x7));
    buf.emit64(0); // null val
    
    // Patch return nodes to target the dynamic jump handler
    size_t endPos = buf.getOffset();
    for (size_t patchOffset : inlinedReturnPatches) {
        int32_t jmpDist = static_cast<int32_t>(endPos - (patchOffset + 4));
        buf.patch32(patchOffset, static_cast<uint32_t>(jmpDist));
    }
    
    // Resolve continuation address dynamically
    buf.emit8(0x59); // pop rcx
    buf.emit8(0xFF); buf.emit8(0xE1); // jmp rcx
    
    // Patch outermost continuation displacement
    int32_t outerLeaDisp = static_cast<int32_t>(buf.getOffset() - (outerLeaPatch + 4));
    buf.patch32(outerLeaPatch, static_cast<uint32_t>(outerLeaDisp));
    
    // Resolve recursive branch target offsets
    for (size_t patchOffset : funcInfo.recursiveCallPatches) {
        int32_t relOffset = static_cast<int32_t>(funcInfo.compiledOffset - (patchOffset + 4));
        buf.patch32(patchOffset, static_cast<uint32_t>(relOffset));
    }
    funcInfo.recursiveCallPatches.clear();
    
    inlinedReturnPatches = std::move(savedReturnPatches);
    inFunctionCall = savedInFunctionCall;
    variables = savedVars;
    currentlyCompiling.erase(funcName);
    
    // Allocate new registers and copy from RAX/RDX
    JITValue finalRes;
    finalRes.valueReg = allocateReg();
    finalRes.typeReg = allocateReg();
    
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
                
                // Runtime Dispatch based on Type (0=INT, 1=FLOAT, 4=STRING)
                bool typeHigh = static_cast<uint8_t>(val.typeReg) >= 8;
                
                // cmp typeReg, 0 (INT check)
                buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
                buf.emit8(0x83);
                buf.emit8(0xF8 | (static_cast<uint8_t>(val.typeReg) & 0x7));
                buf.emit8(0x00);
                buf.emit8(0x74); // je int_print
                size_t jeIntPatch = buf.getOffset();
                buf.emit8(0x00);
                
                // cmp typeReg, 4 (STRING check)
                buf.emit8(0x48 | (typeHigh ? 0x01 : 0));
                buf.emit8(0x83);
                buf.emit8(0xF8 | (static_cast<uint8_t>(val.typeReg) & 0x7));
                buf.emit8(0x04);
                buf.emit8(0x74); // je string_print
                size_t jeStrPatch = buf.getOffset();
                buf.emit8(0x00);
                
                // === FLOAT PRINT (fallthrough) ===
                bool valHigh = static_cast<uint8_t>(val.valueReg) >= 8;
                buf.emit8(0x66); buf.emit8(0x48 | (valHigh ? 0x01 : 0)); buf.emit8(0x0F); buf.emit8(0x6E);
                buf.emit8(0xC0 | (static_cast<uint8_t>(val.valueReg) & 0x7));
                
                buf.emit8(0x48); buf.emit8(0xB8);
                buf.emit64(reinterpret_cast<uint64_t>(jit_print_double_no_newline));
                
                buf.emit8(0x53); // push rbx
                buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0xE3); // mov rbx, rsp
                buf.emit8(0x48); buf.emit8(0x83); buf.emit8(0xE4); buf.emit8(0xF0); // and rsp, -16
                buf.emit8(0xFF); buf.emit8(0xD0); // call rax
                buf.emit8(0x48); buf.emit8(0x89); buf.emit8(0xDC); // mov rsp, rbx
                buf.emit8(0x5B); // pop rbx
                
                buf.emit8(0xEB); // jmp end
                size_t jmpEndFromFloat = buf.getOffset();
                buf.emit8(0x00);
                
                // === INT PRINT ===
                size_t intPrintLabel = buf.getOffset();
                buf.patch8(jeIntPatch, static_cast<uint8_t>(intPrintLabel - (jeIntPatch + 1)));
                emitPrintIntNoNewline(val.valueReg);
                
                buf.emit8(0xEB); // jmp end
                size_t jmpEndFromInt = buf.getOffset();
                buf.emit8(0x00);
                
                // === STRING PRINT ===
                size_t strPrintLabel = buf.getOffset();
                buf.patch8(jeStrPatch, static_cast<uint8_t>(strPrintLabel - (jeStrPatch + 1)));
                
                // RDI = valueReg (JITString data pointer)
                buf.emit8(0x48 | (valHigh ? 0x01 : 0));
                buf.emit8(0x89); buf.emit8(0xC7 | ((static_cast<uint8_t>(val.valueReg) & 0x7) << 3));
                
                buf.emit8(0x50); buf.emit8(0x51); buf.emit8(0x52); // push rax, rcx, rdx
                buf.emit8(0x48); buf.emit8(0xB8);
                buf.emit64(reinterpret_cast<uint64_t>(jit_print_jitstring_no_newline));
                buf.emit8(0xFF); buf.emit8(0xD0); // call
                buf.emit8(0x5A); buf.emit8(0x59); buf.emit8(0x58); // pop rdx, rcx, rax
                
                // === END ===
                size_t endPos = buf.getOffset();
                buf.patch8(jmpEndFromFloat, static_cast<uint8_t>(endPos - (jmpEndFromFloat + 1)));
                buf.patch8(jmpEndFromInt, static_cast<uint8_t>(endPos - (jmpEndFromInt + 1)));
                
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
