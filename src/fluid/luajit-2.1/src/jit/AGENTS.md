The functions in the *.dasc files are typically backed by fallback functions in C++, e.g. assert in vm_x64.dasc has associated code in `src/fluid/luajit-2.1/src/lib/lib_base.cpp` and `src/fluid/luajit-2.1/src/lj_ffrecord.cpp`

## Windows x64 Register Aliasing

**Critical**: On Windows x64, LuaJIT registers alias with the calling convention registers differently than on POSIX:

|Register|Windows x64|POSIX x64|
|-|-|-|
|CARG1|rcx|rdi|
|CARG2|rdx|rsi|
|CARG3|r8|rdx|
|CARG4|r9|rcx|
|BASE|rdx|rdx|

**The dangerous aliasing**: On Windows x64, `BASE == rdx == CARG2`. On POSIX, `BASE == rdx == CARG3`.

All aliases can be read from lines 16 - 101 of vm_x64.dasc.  At the time of writing, the following aliases are defined:

```
BASE = rdx
if X64WIN
   KBASE     = rdi		// Must be C callee-save.
   PC        = rsi		// Must be C callee-save.
   DISPATCH  = rbx		// Must be C callee-save.
   KBASEd    = edi
   PCd       = esi
   DISPATCHd = ebx
else
   KBASE     = r15		// Must be C callee-save.
   PC        = rbx		// Must be C callee-save.
   DISPATCH  = r14		// Must be C callee-save.
   KBASEd    = r15d
   PCd       = ebx
   DISPATCHd = r14d
endif

RA     = rcx
RAd    = ecx
RAH    = ch
RAL    = cl
RB     = rbp		// Must be rbp (C callee-save).
RBd    = ebp
RC     = rax		// Must be rax.
RCd    = eax
RCW    = ax
RCH    = ah
RCL    = al
OP     = RBd
RD     = RC
RDd    = RCd
RDW    = RCW
RDL    = RCL
TMPR   = r10
TMPRd  = r10d
ITYPE  = r11
ITYPEd = r11d

if X64WIN
   CARG1  =	rcx // x64/WIN64 C call arguments.
   CARG2  =	rdx
   CARG3  =	r8
   CARG4  =	r9
   CARG1d =	ecx
   CARG2d =	edx
   CARG3d =	r8d
   CARG4d =	r9d
else
   CARG1  = rdi // x64/POSIX C call arguments.
   CARG2  = rsi
   CARG3  = rdx
   CARG4  = rcx
   CARG5  = r8
   CARG6  = r9
   CARG1d = edi
   CARG2d = esi
   CARG3d = edx
   CARG4d = ecx
   CARG5d = r8d
   CARG6d = r9d
endif

// Type definitions
type L      = lua_State
type GL     = global_State
type TVALUE = TValue
type GCOBJ  = GCobj
type STR    = GCstr
type TAB    = GCtab
type LFUNC  = GCfuncL
type CFUNC  = GCfuncC
type PROTO  = GCproto
type UPVAL  = GCupval
type NODE   = Node
type NARGS  = int
type TRACE  = GCtrace
type SBUF   = SBuf
type ARRAY  = GCarray
```

### Common Bug Pattern

When calling C functions that take 4 arguments (e.g., `lj_arr_getidx`), setting `CARG2` on Windows will clobber `BASE`:

```asm
// WRONG - Windows x64 bug:
|  mov CARG2, ARRAY:RB        // Sets rdx, CLOBBERS BASE!
|  lea CARG4, [BASE-8]        // BASE is now garbage!

// CORRECT - Use .if X64WIN to reorder:
|.if X64WIN
|  lea CARG4, [BASE-8]        // Compute CARG4 while BASE is valid
|  mov CARG2, ARRAY:RB        // Now safe to clobber BASE
|.else
|  mov CARG2, ARRAY:RB        // On POSIX, CARG2 != BASE
|  lea CARG4, [BASE-8]        // CARG3 == BASE on POSIX, but computed before clobber
|.endif
```

### Double Aliasing Hazard

When a value is stored in a register that also needs to be saved:

```asm
// WRONG - RB contains array, but we try to save BASE to RB:
|  mov RB, BASE               // Saves BASE, but DESTROYS array!
|  mov CARG2, ARRAY:RB        // Now CARG2 gets wrong value!

// CORRECT - Save array first via temporary:
|.if X64WIN
|  mov CARG3, ARRAY:RB        // Save array to r8 temporarily
|  lea CARG4, [BASE-8]        // Compute result address
|  mov RB, BASE               // Now safe to save BASE to rbp
|  mov CARG2, CARG3           // Move array from temp to CARG2
|  mov CARG3d, TMPRd          // Set index
|.endif
```

### Callee-Saved Registers

`RB` (rbp) is callee-saved, making it safe for preserving values across C calls. Use it to save `BASE` before calls that clobber it, then restore after:

```asm
|  mov RB, BASE               // Save before call
|  call extern some_c_function
|  mov BASE, RB               // Restore after call
```

### Debugging Tips

1. Use `jit.off()` in Fluid scripts to isolate interpreter issues from JIT issues
2. Check all call sites for `lj_arr_getidx` and similar 4-argument C functions
3. Trace register usage backwards from the call to find clobber points
4. Look for patterns where `CARG2` is set before other args that need `BASE`

## Extended 64-bit Instructions

Extended bytecode instructions consume two 32-bit words: the primary instruction word and an extension word. These are identified by `bcmode_ext(op)` returning true (bit 15 of `lj_bc_mode[]`).

### Extension Word Layout

```
Word 0 (standard):  [B:8][C:8][A:8][OP:8]   - Primary operands
Word 1 (extension): [F:8][E:8][DX:8][EX:8]  - Extended operands
                    or [E16:16][D16:16]     - Mixed 16-bit format
```

### VM Handler Pattern

When implementing a VM handler for an extended opcode, use `ins_next_ext` instead of `ins_next`:

```asm
case BC_EXTENDED_EXAMPLE:
  |  ins_ABC                    // RA, RB, RC from primary word
  |  mov TMPRd, [PC]            // Read entire extension word (PC still points here)
  |  // Extract fields as needed using TMPR
  |  // ... use extended operands ...
  |  ins_next_ext               // Skips extension word AND fetches next instruction
```

The `ins_next_ext` macro (defined in all vm_*.dasc files) combines skipping the extension word with fetching the next instruction, saving one `add` instruction compared to manual `add PC, 4` followed by `ins_next`.

**Field extraction options** (choose based on which fields you need):

```asm
  // If you need just one field, read the byte directly:
  |  movzx REGd, byte [PC]      // EX field (bits 0-7)
  |  movzx REGd, byte [PC+1]    // DX field (bits 8-15)
  |  movzx REGd, byte [PC+2]    // E field (bits 16-23)
  |  movzx REGd, byte [PC+3]    // F field (bits 24-31)

  // For 16-bit fields:
  |  movzx REGd, word [PC]      // D16 field (bits 0-15)
  |  movzx REGd, word [PC+2]    // E16 field (bits 16-31)

  // If you need multiple fields from a register:
  |  mov TMPRd, [PC]            // Load once
  |  movzx EXd, TMPR_L          // EX (low byte)
  |  mov REGd, TMPRd
  |  shr REGd, 24               // F (high byte, no mask needed)
```

Read the full 32-bit word when you need multiple fields or must preserve it. Read individual bytes when you only need one or two specific fields.

### Key Points

1. **Use `ins_next_ext`**: Always use `ins_next_ext` instead of `ins_next` for extended instruction handlers
2. **Read extension word from `[PC]`**: PC still points to the extension word when the handler starts
3. **Field extraction**: Use shifts and masks to extract 8-bit fields, or read bytes directly from `[PC+offset]`
4. **16-bit fields**: For mixed format, use `movzx REGd, word [PC]` for D16, `movzx REGd, word [PC+2]` for E16

### Related Files

- `src/fluid/luajit-2.1/src/jit/vm_x64.dasc` - x64 VM with `ins_NEXT_EXT` macro
- `src/fluid/luajit-2.1/src/jit/vm_arm64.dasc` - ARM64 VM with `ins_NEXT_EXT` macro
- `src/fluid/luajit-2.1/src/jit/vm_ppc.dasc` - PPC VM with `ins_NEXT_EXT` and `ins_NEXT1_EXT` macros
- `src/fluid/luajit-2.1/src/bytecode/lj_bc.h` - `bcmode_ext()`, `bc_ex()`, `bc_dx()`, `bc_e()`, `bc_f()`, `BCINS_EXT()`
- `src/fluid/luajit-2.1/src/lj_record.cpp` - `RecordOps` structure with `ext_word`, `ex`, `dx`, `e`, `f` fields
- `src/fluid/luajit-2.1/BYTECODE.md` - Full documentation of extended instruction format
