# Nevaarize Formal Language Specification: Type System & Memory Model

## 1. Value Representation

Nevaarize utilizes a 64-bit tagged dual-word memory representation across the JIT engine. Every variable slot reserves 16 contiguous bytes on the stack:
- **Bytes 0 to 7**: 64-bit value bits (primitive integer, IEEE 754 double, or 64-bit heap pointer).
- **Bytes 8 to 15**: 64-bit integer type tag.

### Type Tag Table

| Tag | Type Name | In-Memory Representation | Pass Semantics |
|:---|:---|:---|:---|
| 0 | `INT` | 64-bit two's complement signed integer | Value |
| 1 | `FLOAT` | 64-bit IEEE 754 double precision float | Value |
| 2 | `BOOL` | 64-bit integer (0 = false, 1 = true) | Value |
| 3 | `NIL` | 64-bit integer (0) | Value |
| 4 | `STRING` | 64-bit pointer to `JITString` header | Reference |
| 5 | `ARRAY` | 64-bit pointer to `JITArray` header | Reference |
| 6 | `MAP` | 64-bit pointer to `JITMap` header | Reference |
| 7 | `STRUCT` | 64-bit pointer to `JITStruct` header | Reference |
| 8 | `ASYNC_HANDLE` | 64-bit pointer to worker `TaskHandle` | Reference |

## 2. Heap Layout & Structures

### 2.1 Strings (`JITString`)
Strings are immutable UTF-8 byte sequences allocated on the managed heap:

```
+------------------+------------------+------------------+------------------+--------------------+
| Magic: 0x4E455641| Padding: 4 bytes | Capacity: 8 bytes| Length: 8 bytes  | Data: char[...] \0 |
+------------------+------------------+------------------+------------------+--------------------+
```

The pointer handed to JIT registers points directly to the `data` payload offset. Header metadata (magic, capacity, length) resides at negative byte offsets (`ptr - 24`, `ptr - 16`, `ptr - 8`).

### 2.2 Arrays (`JITArray`)
Arrays are contiguous dynamic arrays supporting homogeneous numeric optimization and heterogeneous elements:

```
+------------------+------------------+------------------+------------------+--------------------+
| Magic: 0x41525259| ElemType: 4 bytes| Capacity: 8 bytes| Size: 8 bytes    | Data: int64_t[...] |
+------------------+------------------+------------------+------------------+--------------------+
```

When all elements are floats (`ElemType = 1`), arrays leverage AVX2 (256-bit) and AVX-512 (512-bit) vector instructions for reductions and arithmetic.

### 2.3 Maps (`JITMap`)
Hash maps utilize open-addressed quadratic probing with tombstone markers for efficient key-value lookups.

### 2.4 Structs (`JITStruct`)
User-defined structures contain named fields accessed via compiled field offset tables:

```nva
struct Point {
    x,
    y
}

p = Point(10, 20)
print(p.x, p.y)
```

## 3. Register & Stack Allocation

- The JIT allocates general-purpose registers (RAX, RCX, RDX, RBX, RSI, RDI, R8-R15) using a linear-scan register allocator.
- Hot float loop counters and accumulators are pinned directly to XMM registers (XMM0 through XMM7) to eliminate memory-to-register latency.
- Unpinned variables reside at negative offsets relative to the base pointer (`[rbp - offset]`).
