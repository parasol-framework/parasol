# Plan: Expand LuaJIT BCIns from 32-bit to 64-bit

**Status:** Phases 1-8 completed ✓ (x64 foundation, VM assembly, serialization, parser, JIT recorder, debug, ARM64, PPC64)

## Platform Scope

- **x64 (Windows/Linux):** Primary target - **COMPLETED** (Phases 1-6)
- **ARM64:** Phase 7 - **COMPLETED**
- **PPC (big-endian):** Phase 8 - **COMPLETED**

## Objective

Change the bytecode instruction type (`BCIns`) from 32-bit to 64-bit to enable native 64-bit pointer storage directly in instructions. This is particularly valuable for inline caching where cached pointers can be embedded in the instruction itself.

## New Instruction Formats

```
Current 32-bit (DEPRECATED):
+----+----+----+----+
| B  | C  | A  | OP |  Format ABC (bits 0-31)
+----+----+----+----+

New 64-bit formats:

Format ABCP (extended operand):
+----+----+----+----+----+----+----+----+
|        P (32-bit) | B  | C  | A  | OP |
+----+----+----+----+----+----+----+----+
bits 63          32  31              0

Format ADP (extended with D operand):
+----+----+----+----+----+----+----+----+
|        P (32-bit) |    D    | A  | OP |
+----+----+----+----+----+----+----+----+

Format AP (pointer with register):
+----+----+----+----+----+----+----+----+
|           PTR (48-bit)      | A  | OP |
+----+----+----+----+----+----+----+----+
bits 63                 16 15  8  7  0
```

**Key design decisions**:
1. Opcode remains in bits 0-7 for all formats (dispatch unchanged)
2. Standard opcodes use lower 32 bits; upper 32 bits available for extended data
3. **AP format**: 48-bit pointer + 8-bit A operand (for inline caching with register)

**Pointer storage**: User-space pointers on x64 fit in 47 bits. The AP format's 48 bits is sufficient for all user-space addresses. Runtime asserts verify pointers are in the low-half canonical range.

---

## Implementation Phases

### Phase 1: Foundation - Type and Macro Changes ✓ COMPLETED

**Files:**
- `src/fluid/luajit-2.1/src/runtime/lj_obj.h` (line 256)
- `src/fluid/luajit-2.1/src/bytecode/lj_bc.h`

**Changes completed:**

1. ✓ Changed BCIns type:
   ```cpp
   using BCIns = uint64_t;  // Was uint32_t
   ```

2. ✓ Updated setbc_* macros to use bitmask/shift (endian-independent):
   ```cpp
   #define setbc_op(p, x)  do { BCIns *_p = (BCIns*)(p); \
      *_p = (*_p & ~0xFFULL) | ((BCIns)(uint8_t)(x)); } while(0)
   #define setbc_a(p, x)   do { BCIns *_p = (BCIns*)(p); \
      *_p = (*_p & ~0xFF00ULL) | ((BCIns)(uint8_t)(x) << 8); } while(0)
   // ... similar for setbc_b, setbc_c, setbc_d
   ```

3. ✓ Added new macros for pointer formats:
   ```cpp
   #define BC_PTR_USERSPACE_MAX  0x00007FFFFFFFFFFFULL
   #define bc_p32(i)    ((uint32_t)((i) >> 32))
   #define setbc_p32(p, v)  ...
   #define BCINS_ABCP(o,a,b,c,p32) ...
   #define BCINS_ADP(o,a,d,p32) ...
   #define bc_ptr(i)  ((void*)((i) >> 16))
   #define setbc_ptr(p, ptr)  ...
   #define BCINS_AP(o, a, ptr) ...
   ```

4. ✓ Removed old extension word macros (bc_ex, bc_dx, bc_e, bc_f, BCINS_EXT, etc.)

---

### Phase 2: VM Assembly - x64 Instruction Fetch/Dispatch ✓ COMPLETED

**Files:**
- `src/fluid/luajit-2.1/src/jit/vm_x64.dasc` (~50 locations modified)

**Changes completed:**

1. ✓ Updated `ins_NEXT` macro:
   ```asm
   .macro ins_NEXT
     mov RC, [PC]           // Fetch full 64-bit instruction
     movzx RAd, RCH         // Extract A (bits 8-15)
     movzx OP, RCL          // Extract opcode (bits 0-7)
     add PC, 8              // Advance by 8 bytes (was 4)
     shr RC, 16             // Shift for D extraction (preserves upper bits)
     jmp aword [DISPATCH+OP*8]
   .endmacro
   ```

2. ✓ Updated `ins_callt` macro similarly (add PC, 8; 64-bit load)

3. ✓ Updated all PC arithmetic: `add PC, 4` → `add PC, 8`, `sub PC, 4` → `sub PC, 8`

4. ✓ Updated PC-relative offsets:
   ```asm
   .define PC_OP, byte [PC-8]    // Was PC-4
   .define PC_RA, byte [PC-7]    // Was PC-3
   .define PC_RB, byte [PC-5]    // Was PC-1
   .define PC_RC, byte [PC-6]    // Was PC-2
   .define PC_RD, word [PC-6]    // Was PC-2
   ```

5. ✓ Added pointer extraction macros:
   ```asm
   .macro ins_getp32, dst
     mov dst, dword [PC-4]  // Upper 32 bits of instruction
   .endmacro

   .macro ins_getptr, dst
     mov dst, qword [PC-8]  // Full 64-bit load
     shr dst, 16            // Extract 48-bit pointer
   .endmacro
   ```

6. ✓ Updated branchPC macro: `lea PC, [PC+reg*8-BCBIAS_J*8]`

7. ✓ **Bug fix**: In `vm_exit_interp` label 4, fixed `[RC-3]` → `[RC-7]` for correct A field extraction from 64-bit call instruction

8. ✓ **Forward compatibility**: Changed `shr RCd, 16` → `shr RC, 16` in multiple locations to preserve upper 32 bits for future 64-bit opcodes:
   - `ins_NEXT` macro (line 213)
   - `vm_exit_interp` (line 2708)
   - `lj_meta_for` retry path (line 1284)

9. ✓ Removed `ins_NEXT_EXT` macro (no longer needed)

---

### Phase 3: Bytecode Serialization ✓ COMPLETED

**Files:**
- `src/fluid/luajit-2.1/src/bytecode/lj_bcwrite.cpp`
- `src/fluid/luajit-2.1/src/bytecode/lj_bcread.cpp`
- `src/fluid/luajit-2.1/src/bytecode/lj_bcdump.h`

**Changes completed:**

1. ✓ **bcread_bytecode()**: Updated to use `lj_bswap64` for endianness swap:
   ```cpp
   if (bcread_swap(State)) {
      for (i = 1; i < sizebc; i++) bc[i] = lj_bswap64(bc[i]);
   }
   ```

2. ✓ **bcwrite_bytecode()**: Updated JIT unpatch loop to use macros:
   ```cpp
   BCIns* q = (BCIns*)p;
   for (i = 0; i < nbc; i++, q++) {
      BCOp op = bc_op(*q);
      // ... opcode fixup logic ...
      setbc_op(q, (uint8_t)(op - BC_IFORL + BC_FORL));
      // For JLOOP etc: copy full 64-bit instruction
      *q = traceref(J, rd)->startins;
   }
   ```

3. ✓ Removed `bcwrite_has_ext()` function (extension word detection no longer needed)

4. ✓ Removed `BCDUMP_F_EXT` flag from header

5. ✓ Removed extension word validation in bcread (all instructions are now single 64-bit words)

---

### Phase 4: Parser/Compiler ✓ COMPLETED

**Files:**
- `src/fluid/luajit-2.1/src/parser/parse_regalloc.cpp`
- `src/fluid/luajit-2.1/src/parser/parse_internal.h`

**Changes completed:**

1. ✓ `bcemit_INS()` handles 64-bit instructions automatically (BCIns type change)

2. ✓ Added helpers for setting pointer operands:
   ```cpp
   static inline void bcemit_set_p32(FuncState *fs, BCPOS pc, uint32_t val) {
      setbc_p32(&fs->bcbase[pc].ins, val);
   }

   static inline void bcemit_set_ptr(FuncState *fs, BCPOS pc, void *ptr) {
      setbc_ptr(&fs->bcbase[pc].ins, ptr);
   }
   ```

3. ✓ Removed `bcemit_INS_EXT()` function (two-word emission no longer needed)

4. ✓ Removed `bcemit_ABC_EXT` and `bcemit_AD_EXT16` templates

5. ✓ Fixed bitmasking for negated string constant indices: `(~rc) & 0xFFu` to ensure 8-bit field

---

### Phase 5: JIT Recorder ✓ COMPLETED

**Files:**
- `src/fluid/luajit-2.1/src/lj_record.cpp`

**Changes completed:**

1. ✓ Removed extension word fields from `RecordOps` structure:
   ```cpp
   // Removed:
   // BCIns ext_word;
   // uint8_t ex, dx, e, f;
   ```

2. ✓ Removed extension word decoding in `rec_decode_operands()`:
   ```cpp
   // Removed bcmode_ext() check and extension word extraction
   ```

3. ✓ PC arithmetic works correctly via `sizeof(BCIns)` (now 8 bytes)

---

### Phase 6: Debug and Auxiliary ✓ COMPLETED

**Files:**
- `src/fluid/luajit-2.1/src/debug/dump_bytecode.cpp`

**Changes completed:**

1. ✓ Simplified `BytecodeInfo` structure (removed extension word fields):
   ```cpp
   // Removed:
   // bool is_extended;
   // BCIns ext_word;
   // uint8_t ext_ex, ext_dx, ext_e, ext_f;
   // uint16_t ext_d16, ext_e16;
   ```

2. ✓ Simplified `extract_instruction_info()` (no extension word handling)

3. ✓ Simplified `format_bc_line()` (no extension suffix output)

4. ✓ Simplified `trace_proto_bytecode()` (no extension word skipping)

---

### Phase 7: ARM64 VM Assembly ✓ COMPLETED

**Files:**
- `src/fluid/luajit-2.1/src/jit/vm_arm64.dasc`

**Changes completed:**

1. ✓ Updated `ins_NEXT` macro:
   ```asm
   .macro ins_NEXT
     ldr INS, [PC], #8      // Fetch full 64-bit instruction, advance PC by 8
     add TMP1, GL, INS, uxtb #3
     decode_RA RA, INS
     ldr TMP0, [TMP1, #GG_G2DISP]
     decode_RD RC, INS
     br TMP0
   .endmacro
   ```

2. ✓ Updated `ins_callt` macro similarly (64-bit load, PC += 8)

3. ✓ Added pointer extraction macros:
   ```asm
   .macro ins_getp32, dst
     ldr dst, [PC, #-4]     // Upper 32 bits of instruction
   .endmacro

   .macro ins_getptr, dst
     ldr dst, [PC, #-8]     // Load full 64-bit instruction
     lsr dst, dst, #16      // Extract 48-bit pointer
   .endmacro
   ```

4. ✓ Updated all PC arithmetic: `#4` → `#8`, `#-4` → `#-8`

5. ✓ Updated all branch calculations: `lsl #2` → `lsl #3`, `#0x20000` → `#0x40000`

6. ✓ Updated PC-relative field accesses:
   - `[PC, #-4]` → `[PC, #-8]` for loading instructions
   - `[PC, #-4+OFS_*]` → `[PC, #-8+OFS_*]` for field access
   - `[PC, #-4+PC2PROTO(*)]` → `[PC, #-8+PC2PROTO(*)]` for proto access

7. ✓ Updated startins handling for 64-bit loads/stores:
   - `ldr TMP2w, TRACE:RA->startins` → `ldr TMP2, TRACE:RA->startins`
   - `str TMP2w, [RC]` → `str TMP2, [RC]`
   - `bfxil TMP2w, TMP1w, #0, #8` → `bfxil TMP2, TMP1, #0, #8`

8. ✓ Fixed vm_exit_interp KBASE setup: `[CARG1, #-4]` → `[CARG1, #-8]`

---

### Phase 8: PPC VM Assembly - Big-Endian ✓ COMPLETED

**Files:**
- `src/fluid/luajit-2.1/src/jit/vm_ppc.dasc`

**Changes completed:**

1. ✓ Updated `ins_NEXT1` macro for 64-bit instructions:
   ```asm
   .macro ins_NEXT1
     ld INS, 0(PC)             // Fetch full 64-bit instruction
     addi PC, PC, 8            // Advance to next instruction (64-bit = 8 bytes)
   .endmacro
   ```

2. ✓ Updated `ins_callt` macro similarly (64-bit load, PC += 8)

3. ✓ Updated `branch_RD` macro for 64-bit PC scaling:
   ```asm
   .macro branch_RD
     addis PC, PC, -(BCBIAS_J*8 >> 16)
     add PC, PC, RD            // RD already D*8 from decode_RD8
   .endmacro
   ```

4. ✓ Added pointer extraction macros:
   ```asm
   .macro ins_getp32, dst
     lwz dst, -4(PC)           // Upper 32 bits of 64-bit instruction
   .endmacro

   .macro ins_getptr, dst
     ld dst, -8(PC)            // Load full 64-bit instruction
     srdi dst, dst, 16         // Extract 48-bit pointer
   .endmacro
   ```

5. ✓ Updated all PC arithmetic: `addi PC, PC, 4` → `addi PC, PC, 8`, `subi PC, PC, 4` → `subi PC, PC, 8`

6. ✓ Updated all instruction loads: `lwz INS, -4(PC)` → `ld INS, -8(PC)`, `lwz INS, 0(PC)` → `ld INS, 0(PC)`

7. ✓ Updated all branch bias calculations: `BCBIAS_J*4` → `BCBIAS_J*8`

8. ✓ Updated all branch offset decoding: `decode_RD4` → `decode_RD8` for branch calculations

9. ✓ Updated PC2PROTO offsets: `-4+PC2PROTO(...)` → `-8+PC2PROTO(...)`

10. ✓ Updated vm_exit_interp instruction fetch for 64-bit, including BC_FUNC* opcode comparisons (×4 → ×8)

11. ✓ Removed unnecessary `srwi RD, 1` in branch calculations (RD*8 is now the correct byte offset)

**Note:** The existing decode_* macros (decode_OP8, decode_RA8, decode_RB8, decode_RC8, decode_RD8) operate on the low 32 bits of a 64-bit register using `rlwinm`, which correctly extracts fields from 64-bit BCIns since the opcode/A/B/C/D fields remain in bits 0-31.

---

## Risk Mitigation

| Risk | Mitigation | Status |
|------|------------|--------|
| Memory doubling | Acceptable trade-off for native pointer storage | ✓ Accepted |
| I-cache impact | Benchmark before/after; modern CPUs prefetch well | ✓ Tested |
| Missing PC+4->PC+8 | Grep all PC increments; add debug assertions | ✓ Fixed |
| Endianness (PPC) | Use bitmask/shift for setbc_* macros | ✓ Done |
| Non-user-space pointers | Runtime asserts verify ptr <= BC_PTR_USERSPACE_MAX | ✓ Added |
| JIT unpatch corruption | Use setbc_op() macro, not byte offsets | ✓ Fixed |
| Byte-swap inconsistency | Use lj_bswap64 in both reader/writer | ✓ Done |
| vm_exit_interp A field | Fixed [RC-3] → [RC-7] for 64-bit bytecode | ✓ Fixed |
| Forward compatibility | Changed shr RCd → shr RC to preserve upper bits | ✓ Done |

---

## Verification

1. **Build**: `cmake --build build/agents --config Debug --parallel`

2. **Run all tests**:
   ```bash
   ctest --build-config Debug --test-dir build/agents --output-on-failure
   ```

3. **Specific bytecode tests**:
   ```bash
   build/agents-install/parasol tools/flute.fluid file=src/fluid/tests/test_jit.fluid --log-warning
   ```

4. **Nested try-except test** (validates PC handling):
   ```bash
   build/agents-install/parasol test.fluid --log-warning
   ```

5. **Verify disassembly** shows correct 64-bit format

---

## Files Summary

| File | Phase | Changes | Status |
|------|-------|---------|--------|
| `src/fluid/luajit-2.1/src/runtime/lj_obj.h` | 1 | BCIns: uint32_t → uint64_t | ✓ |
| `src/fluid/luajit-2.1/src/bytecode/lj_bc.h` | 1 | setbc_* macros, pointer macros | ✓ |
| `src/fluid/luajit-2.1/src/jit/vm_x64.dasc` | 2 | PC increments, ins_NEXT, ~50 locations | ✓ |
| `src/fluid/luajit-2.1/src/bytecode/lj_bcwrite.cpp` | 3 | 64-bit writes, JIT unpatch fix | ✓ |
| `src/fluid/luajit-2.1/src/bytecode/lj_bcread.cpp` | 3 | 64-bit reads, lj_bswap64 | ✓ |
| `src/fluid/luajit-2.1/src/parser/parse_regalloc.cpp` | 4 | Removed bcemit_INS_EXT | ✓ |
| `src/fluid/luajit-2.1/src/parser/parse_internal.h` | 4 | Pointer emission helpers | ✓ |
| `src/fluid/luajit-2.1/src/lj_record.cpp` | 5 | Removed RecordOps extension fields | ✓ |
| `src/fluid/luajit-2.1/src/debug/dump_bytecode.cpp` | 6 | Simplified (no extension words) | ✓ |
| `src/fluid/luajit-2.1/src/jit/vm_arm64.dasc` | 7 | 64-bit fetch, PC arithmetic, branch calc | ✓ |
| `src/fluid/luajit-2.1/src/jit/vm_ppc.dasc` | 8 | 64-bit fetch, PC arithmetic, branch calc, pointer extraction | ✓ |
