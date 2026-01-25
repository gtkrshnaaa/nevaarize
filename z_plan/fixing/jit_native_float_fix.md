# Fixing Plan: JIT Native Float Garbage Values

## Status
- **Issue**: JIT compiles and runs, but printing float results yields garbage (e.g., `1.30494e+180` instead of `4.0`), even though logic checks (comparison `==`) pass.
- **Affected Component**: `JIT.cpp`, specifically `BINARY_OP` (Float arithmetic path).

## Problem Analysis
The issue lies in the x86-64 machine code generation for SSE instructions in `BINARY_OP`.

### Root Cause: Incorrect REX Prefix for Operand Loading
When loading values from General Purpose Registers (GPR) into XMM registers (for float math), the compiler emits:
```cpp
// cvtsi2sd xmm0, left.val
buf.emit8(0xF2);
buf.emit8(0x48 | (resHigh ? 0x04 : 0)); // <--- ERROR IS HERE
buf.emit8(0x0F); buf.emit8(0x2A);
buf.emit8(0xC0 | (static_cast<uint8_t>(left.valueReg) & 0x7));
```

**The Bug**:
- It uses `resHigh` (Result Register High Bit) to determine the REX prefix.
- `resHigh` controls `REX.R` (Extension of ModRM.reg field).
- `cvtsi2sd xmm0, r/m64` expects the source (GPR) encoded in ModRM.r/m.
- The GPR extension bit is `REX.B`.
- By using `resHigh` (which might be 0 or 1 independent of `left.valueReg`), we are randomly setting `REX.R`.
    - If `REX.R` is set, it addresses `XMM8` instead of `XMM0`.
    - If `left.valueReg` is > 7 (e.g., R8), we NEED `REX.B`, but we aren't setting it based on `left.valueReg`.

**Consequence**:
1. We might load the value into `XMM8` instead of `XMM0`.
2. Then we perform `addsd xmm0, xmm1`.
3. `XMM0` contains garbage (uninitialized), `XMM8` has the value.
4. Result is garbage.

## Detailed Fixing Plan

### 1. Fix Operand Loading (GPR -> XMM)
For **Left Operand** (into `XMM0`):
- **Int to Float** (`cvtsi2sd xmm0, src`):
    - Prefix: `0xF2`
    - REX: `0x48` | (`src >= 8` ? `REX.B (0x01)` : 0)
    - Opcode: `0x0F 0x2A`
    - ModRM: `0xC0` | (`src & 7`)
- **Float Copy** (`movq xmm0, src`):
    - Prefix: `0x66`
    - REX: `0x48` | (`src >= 8` ? `REX.B (0x01)` : 0)
    - Opcode: `0x0F 0x6E`
    - ModRM: `0xC0` | (`src & 7`)

*Repeat logic for Right Operand (into `XMM1`).*

### 2. Fix Arithmetic Operations (`addsd`, etc.)
- Ensure `addsd xmm0, xmm1` uses standard encoding.
- `F2 0F 58 C1` (addsd xmm0, xmm1).
- These seem correct in current code, but verify no REX prefix is accidentally modifying them implies `xmm8` ranges.

### 3. Fix Result Storage (XMM -> GPR)
- **Store Result** (`movq dst, xmm0`):
    - Prefix: `0x66`
    - REX: `0x48` | (`dst >= 8` ? `REX.B (0x01)` : 0)
        - *Wait*: `movd r/m64, xmm` (0F 7E) stores XMM into r/m64.
        - ModRM: `C0 | (xmm_reg << 3) | gpr_reg`.
        - Wait, `movq` xmm to gpr direction usually `0F 7E` ?
        - Check Intel SDM. `MOVQ r/m64, xmm1` is `66 REX.W 0F 7E /r`.
        - ModRM.reg = xmm, ModRM.rm = r/m64.
        - So `REX.R` extends XMM register (always 0 here, XMM0).
        - `REX.B` extends GPR result register.
        - **Correct Logic**: `REX = 0x48 | (resultReg >= 8 ? 0x01 : 0)`.

## Execution Steps (When Resuming)
1. Open `core/src/JIT.cpp`.
2. Locate `BINARY_OP` case.
3. Replace all REX generation logic for `cvtsi2sd` and `movq` to use the **Operand's** high-bit check (`lValHigh`/`rValHigh`), not `resHigh`.
4. Ensure `movq` result storage uses `resultReg`'s high-bit check for `REX.B`.
5. Recompile and run `examples/float_test.nva`.

## Verification Criteria
- `float_test.nva` output:
    ```
    1.5 + 2.5 = 4.0  (Not 1.3e180)
    10 + 0.5 = 10.5
    ```
