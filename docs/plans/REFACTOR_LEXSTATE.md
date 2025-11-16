# LexState Method Refactoring Plan

## Overview

This plan outlines the refactoring of C-style functions that take `LexState *` as their first parameter into proper C++ methods of the `LexState` class. This refactoring will improve code organisation, make the API more intuitive, and align with modern C++ practices already established in the recent lexer refactoring.

## Current State

The parser codebase at `src/fluid/luajit-2.1/src/parser/` contains approximately **141 functions** across 9 files that take `LexState *` as their first parameter. These are currently implemented as static C-style functions but would be more appropriately implemented as methods of the `LexState` class.

### File Distribution

| File | Lines of Code | LexState Functions |
|------|--------------|-------------------|
| `parse_expr.cpp` | 1,026 | ~24 |
| `parse_stmt.cpp` | 795 | ~21 |
| `parse_scope.cpp` | 714 | ~15 |
| `parse_operators.cpp` | 638 | 0 |
| `parse_regalloc.cpp` | 440 | 0 |
| `parse_constants.cpp` | 187 | 3 |
| `parse_core.cpp` | 71 | 6 |
| `lj_lex.cpp` | - | ~10 |
| `lj_parse.cpp` | - | 2 |

## Motivation

### Benefits

1. **Improved Encapsulation**: Methods naturally group functionality with the data they operate on
2. **Cleaner API**: `ls->parse_if(line)` is more intuitive than `parse_if(ls, line)`
3. **Consistency**: Aligns with recent refactoring work on the lexer (see commit history)
4. **Modern C++**: Follows C++20 best practices for object-oriented design
5. **Type Safety**: Implicit `this` pointer reduces parameter passing errors
6. **Reduced Namespace Pollution**: Methods don't need static function declarations

### Precedent

The `LexState` class already has methods:
- `next()` - advance to next token
- `lookahead_token()` - peek at next token
- `token2str()` - convert token to string
- `error()` - error reporting
- `assert_condition()` - conditional assertion

This refactoring extends this pattern to all LexState-related operations.

## Categorisation of Functions

### Category 1: Core Lexer Operations (Priority: High)

These functions are direct lexer operations and should become methods immediately:

**From `parse_core.cpp`:**
- `lex_opt(LexState *, LexToken)` → `LexState::lex_opt(LexToken)`
- `lex_check(LexState *, LexToken)` → `LexState::lex_check(LexToken)`
- `lex_match(LexState *, LexToken, LexToken, BCLine)` → `LexState::lex_match(LexToken, LexToken, BCLine)`
- `lex_str(LexState *)` → `LexState::lex_str()`

**Rationale**: These are fundamental lexer operations that directly manipulate lexer state.

### Category 2: Error Handling (Priority: High)

Error functions that should become methods:

**From `parse_core.cpp`:**
- `err_syntax(LexState *, ErrMsg)` → `LexState::err_syntax(ErrMsg)`
- `err_token(LexState *, LexToken)` → `LexState::err_token(LexToken)`

**Note**: `err_limit(FuncState *, ...)` takes `FuncState` so it should remain separate or become a `FuncState` method.

### Category 3: Variable Management (Priority: Medium)

Functions managing local variables:

**From `parse_scope.cpp`:**
- `var_new(LexState *, BCReg, GCstr *)` → `LexState::var_new(BCReg, GCstr *)`
- `var_new_lit(LexState *, BCReg, const char *, size_t)` → `LexState::var_new_lit(BCReg, const char *, size_t)`
- `var_new_fixed(LexState *, BCReg, uintptr_t)` → `LexState::var_new_fixed(BCReg, uintptr_t)`
- `var_add(LexState *, BCReg)` → `LexState::var_add(BCReg)`
- `var_remove(LexState *, BCReg)` → `LexState::var_remove(BCReg)`
- `var_lookup(LexState *, ExpDesc *)` → `LexState::var_lookup(ExpDesc *)`

**Rationale**: These functions directly manipulate `LexState::vstack` and related variable tracking.

### Category 4: Goto/Label Management (Priority: Medium)

Functions for goto and label handling:

**From `parse_scope.cpp`:**
- `gola_new(LexState *, int, uint8_t, BCPos)` → `LexState::gola_new(int, uint8_t, BCPos)`
- `gola_patch(LexState *, VarInfo *, VarInfo *)` → `LexState::gola_patch(VarInfo *, VarInfo *)`
- `gola_close(LexState *, VarInfo *)` → `LexState::gola_close(VarInfo *)`
- `gola_resolve(LexState *, FuncScope *, MSize)` → `LexState::gola_resolve(FuncScope *, MSize)`
- `gola_fixup(LexState *, FuncScope *)` → `LexState::gola_fixup(FuncScope *)`

**Rationale**: Goto/label tracking is part of lexer state management.

### Category 5: Function State Initialization (Priority: Medium)

Functions that prepare function state but require lexer context:

**From `parse_scope.cpp`:**
- `fs_prep_var(LexState *, FuncState *, size_t *)` → `LexState::fs_prep_var(FuncState *, size_t *)`
- `fs_fixup_var(LexState *, GCproto *, uint8_t *, size_t)` → `LexState::fs_fixup_var(GCproto *, uint8_t *, size_t)`
- `fs_finish(LexState *, BCLine)` → `LexState::fs_finish(BCLine)`
- `fs_init(LexState *, FuncState *)` → `LexState::fs_init(FuncState *)`

**Rationale**: These functions access both `LexState` and `FuncState` but are initiated from lexer context.

### Category 6: Expression Parsing (Priority: Low)

Expression parsing functions:

**From `parse_expr.cpp`:**
- `expr(LexState *, ExpDesc *)` → `LexState::expr(ExpDesc *)`
- `expr_str(LexState *, ExpDesc *)` → `LexState::expr_str(ExpDesc *)`
- `expr_field(LexState *, ExpDesc *)` → `LexState::expr_field(ExpDesc *)`
- `expr_bracket(LexState *, ExpDesc *)` → `LexState::expr_bracket(ExpDesc *)`
- `expr_table(LexState *, ExpDesc *)` → `LexState::expr_table(ExpDesc *)`
- `expr_primary(LexState *, ExpDesc *)` → `LexState::expr_primary(ExpDesc *)`
- `expr_simple(LexState *, ExpDesc *)` → `LexState::expr_simple(ExpDesc *)`
- `expr_unop(LexState *, ExpDesc *)` → `LexState::expr_unop(ExpDesc *)`
- `expr_binop(LexState *, ExpDesc *, uint32_t)` → `LexState::expr_binop(ExpDesc *, uint32_t)`
- `expr_shift_chain(LexState *, ExpDesc *, BinOpr)` → `LexState::expr_shift_chain(ExpDesc *, BinOpr)`
- `expr_cond(LexState *)` → `LexState::expr_cond()`
- `expr_next(LexState *)` → `LexState::expr_next()`
- `expr_list(LexState *, ExpDesc *)` → `LexState::expr_list(ExpDesc *)`

**Rationale**: Expression parsing is lexer-driven, reading tokens and building expressions.

### Category 7: Statement Parsing (Priority: Low)

Statement parsing functions:

**From `parse_stmt.cpp`:**
- `parse_assignment(LexState *, LHSVarList *, BCReg)` → `LexState::parse_assignment(LHSVarList *, BCReg)`
- `parse_call_assign(LexState *)` → `LexState::parse_call_assign()`
- `parse_local(LexState *)` → `LexState::parse_local()`
- `parse_defer(LexState *)` → `LexState::parse_defer()`
- `parse_func(LexState *, BCLine)` → `LexState::parse_func(BCLine)`
- `parse_return(LexState *)` → `LexState::parse_return()`
- `parse_continue(LexState *)` → `LexState::parse_continue()`
- `parse_break(LexState *)` → `LexState::parse_break()`
- `parse_block(LexState *)` → `LexState::parse_block()`
- `parse_while(LexState *, BCLine)` → `LexState::parse_while(BCLine)`
- `parse_repeat(LexState *, BCLine)` → `LexState::parse_repeat(BCLine)`
- `parse_for_num(LexState *, GCstr *, BCLine)` → `LexState::parse_for_num(GCstr *, BCLine)`
- `parse_for_iter(LexState *, GCstr *)` → `LexState::parse_for_iter(GCstr *)`
- `parse_for(LexState *, BCLine)` → `LexState::parse_for(BCLine)`
- `parse_if(LexState *, BCLine)` → `LexState::parse_if(BCLine)`
- `parse_stmt(LexState *)` → `LexState::parse_stmt()`
- `parse_chunk(LexState *)` → `LexState::parse_chunk()`
- `parse_then(LexState *)` → `LexState::parse_then()`
- `assign_adjust(LexState *, BCReg, BCReg, ExpDesc *)` → `LexState::assign_adjust(BCReg, BCReg, ExpDesc *)`
- `assign_hazard(LexState *, LHSVarList *, const ExpDesc *)` → `LexState::assign_hazard(LHSVarList *, const ExpDesc *)`
- `assign_compound(LexState *, LHSVarList *, LexToken)` → `LexState::assign_compound(LHSVarList *, LexToken)`

**Rationale**: Statement parsing is the main parsing loop, inherently tied to lexer state.

### Category 8: Function Body and Parameter Parsing (Priority: Low)

**From `parse_expr.cpp`:**
- `parse_params(LexState *, int)` → `LexState::parse_params(int)`
- `parse_body(LexState *, ExpDesc *, int, BCLine)` → `LexState::parse_body(ExpDesc *, int, BCLine)`
- `parse_body_defer(LexState *, ExpDesc *, BCLine)` → `LexState::parse_body_defer(ExpDesc *, BCLine)`
- `parse_args(LexState *, ExpDesc *)` → `LexState::parse_args(ExpDesc *)`
- `inc_dec_op(LexState *, BinOpr, ExpDesc *, int)` → `LexState::inc_dec_op(BinOpr, ExpDesc *, int)`

### Category 9: Utility Functions (Priority: Low)

**From `parse_expr.cpp`:**
- `synlevel_begin(LexState *)` → `LexState::synlevel_begin()`

**From `parse_stmt.cpp`:**
- `predict_next(LexState *, FuncState *, BCPos)` → Helper function (consider leaving as static or moving to FuncState)
- `snapshot_return_regs(FuncState *, BCIns *)` → FuncState function (not LexState)

### Category 10: Public API Functions (Priority: High)

**From `lj_parse.h`:**
- `lj_parse(LexState *)` → Keep as free function (entry point)
- `lj_parse_keepstr(LexState *, const char *, size_t)` → `LexState::keepstr(const char *, size_t)`
- `lj_parse_keepcdata(LexState *, TValue *, GCcdata *)` → `LexState::keepcdata(TValue *, GCcdata *)`

**Note**: `lj_parse` should remain a free function as it's the external API entry point, but it can call `ls->parse_chunk()` internally.

## Excluded Functions

Functions that should **NOT** be converted because they don't primarily operate on LexState:

**Functions taking FuncState as first parameter:**
- All functions in `parse_regalloc.cpp` (BCReg allocation, bytecode emission)
- All functions in `parse_operators.cpp` (arithmetic, comparison operations)
- Most functions in `parse_constants.cpp` (constant management)
- `err_limit()` - operates on FuncState

These functions operate on `FuncState` and only access `LexState` through `fs->ls` when needed.

## Implementation Strategy

### Phase 1: Core Lexer Operations (Week 1)

1. Convert Category 1 (lexer operations) and Category 2 (error handling)
2. Update all call sites in parser files
3. Run full test suite to verify correctness
4. Commit: "Refactor: Convert core lexer operations to LexState methods"

**Files to modify:**
- `lj_lex.h` - Add method declarations
- `parse_core.cpp` - Convert implementations
- All parser files - Update call sites

**Risk**: Low - these are simple, self-contained functions

### Phase 2: Variable and Scope Management (Week 2)

1. Convert Category 3 (variables) and Category 4 (goto/labels)
2. Update all call sites
3. Run full test suite
4. Commit: "Refactor: Convert variable/scope management to LexState methods"

**Files to modify:**
- `lj_lex.h` - Add method declarations
- `parse_scope.cpp` - Convert implementations
- All parser files - Update call sites

**Risk**: Medium - these functions interact with complex state

### Phase 3: Function State Initialization (Week 3)

1. Convert Category 5 (function state)
2. Update all call sites
3. Run full test suite
4. Commit: "Refactor: Convert function state management to LexState methods"

**Files to modify:**
- `lj_lex.h` - Add method declarations
- `parse_scope.cpp` - Convert implementations
- Parser files - Update call sites

**Risk**: Medium - interactions between LexState and FuncState

### Phase 4: Expression Parsing (Week 4)

1. Convert Category 6 (expression parsing)
2. Update all call sites
3. Run full test suite
4. Commit: "Refactor: Convert expression parsing to LexState methods"

**Files to modify:**
- `lj_lex.h` - Add method declarations
- `parse_expr.cpp` - Convert implementations
- Parser files - Update call sites

**Risk**: Medium - complex call chains, high interaction

### Phase 5: Statement Parsing (Week 5)

1. Convert Category 7 (statement parsing)
2. Update all call sites
3. Run full test suite
4. Commit: "Refactor: Convert statement parsing to LexState methods"

**Files to modify:**
- `lj_lex.h` - Add method declarations
- `parse_stmt.cpp` - Convert implementations
- Parser files - Update call sites

**Risk**: Medium-High - main parsing logic, extensive call sites

### Phase 6: Public API and Cleanup (Week 6)

1. Convert Category 10 (public API functions)
2. Clean up deprecated function wrappers
3. Update documentation
4. Final comprehensive test run
5. Commit: "Refactor: Complete LexState method conversion and cleanup"

**Files to modify:**
- `lj_parse.h` - Update API
- `lj_parse.cpp` - Update implementation
- `parse_constants.cpp` - Convert keepstr/keepcdata
- Documentation files

**Risk**: Low - final cleanup and polish

## Technical Considerations

### Header Organisation

Add method declarations to `lj_lex.h` in logical groups:

```cpp
class LexState {
public:
   // ... existing members ...

   // Core lexer operations
   bool lex_opt(LexToken tok);
   void lex_check(LexToken tok);
   void lex_match(LexToken what, LexToken who, BCLine line);
   GCstr* lex_str();

   // Error handling
   [[noreturn]] void err_syntax(ErrMsg em);
   [[noreturn]] void err_token(LexToken tok);

   // Variable management
   void var_new(BCReg n, GCstr* name);
   void var_add(BCReg nvars);
   void var_remove(BCReg tolevel);

   // Expression parsing
   void expr(ExpDesc* v);
   void expr_str(ExpDesc* e);
   // ... etc ...

   // Statement parsing
   void parse_assignment(LHSVarList* lh, BCReg nvars);
   void parse_local();
   // ... etc ...

   // Public API
   GCstr* keepstr(const char* str, size_t len);
#if LJ_HASFFI
   void keepcdata(TValue* tv, GCcdata* cd);
#endif
};
```

### Implementation Changes

For each function conversion:

**Before:**
```cpp
static void lex_check(LexState* ls, LexToken tok)
{
   if (ls->tok != tok) err_token(ls, tok);
   ls->next();
}
```

**After:**
```cpp
void LexState::lex_check(LexToken tok)
{
   if (this->tok != tok) this->err_token(tok);
   this->next();
}
```

### Call Site Updates

**Before:**
```cpp
lex_check(ls, TK_then);
```

**After:**
```cpp
ls->lex_check(TK_then);
```

### Compatibility Considerations

During transition, maintain compatibility wrappers:

```cpp
// Deprecated compatibility wrapper
static inline void lex_check(LexState* ls, LexToken tok) {
   ls->lex_check(tok);
}
```

Remove wrappers in Phase 6 cleanup.

## Testing Strategy

### Per-Phase Testing

After each phase:

1. **Unit Tests**: Run existing Flute test suite
   ```bash
   ctest --build-config Release --test-dir build/agents --output-on-failure
   ```

2. **Integration Tests**: Test all Fluid scripts in examples/
   ```bash
   build/agents-install/parasol.exe tools/flute.fluid file=examples/test_*.fluid --gfx-driver=headless
   ```

3. **Compilation Tests**: Ensure all modules compile
   ```bash
   cmake --build build/agents --config Release --parallel
   ```

### Regression Testing

Critical test cases:
- Error reporting accuracy (line numbers, messages)
- Variable scoping (local, upvalues)
- Expression parsing (precedence, ternary operators)
- Statement parsing (loops, conditionals, defer)
- Complex Fluid scripts (gui widgets, network examples)

## Risks and Mitigation

### Risk 1: Breaking Changes

**Risk**: Method conversion could introduce subtle bugs

**Mitigation**:
- Incremental phases with testing between each
- Keep compatibility wrappers until Phase 6
- Extensive regression testing

### Risk 2: Performance Impact

**Risk**: Method calls might have different performance characteristics

**Mitigation**:
- Modern compilers inline methods effectively
- Benchmark critical parsing paths
- Profile before/after comparison

### Risk 3: Scope Creep

**Risk**: Discovering related refactoring needs mid-process

**Mitigation**:
- Stick to defined scope (only LexState* first parameter functions)
- Document other issues for separate tasks
- Complete each phase before expanding

### Risk 4: Merge Conflicts

**Risk**: Ongoing development could conflict with refactoring

**Mitigation**:
- Work in dedicated feature branch
- Coordinate with team on parser changes
- Frequent rebasing from master

## Success Criteria

1. ✅ All identified LexState* functions converted to methods
2. ✅ All test suites pass without regression
3. ✅ Code compiles without warnings
4. ✅ No performance degradation (< 5% parsing time increase)
5. ✅ Improved code readability (code review consensus)
6. ✅ Documentation updated
7. ✅ Zero deprecated compatibility wrappers remaining

## Dependencies

### Prerequisites
- Existing build environment configured
- Full test suite passing on current codebase
- Familiarity with LuaJIT parser architecture

### Blockers
- None identified - this is a self-contained refactoring

## Metrics

### Quantitative
- Functions converted: 0 / ~70 (Categories 1-8, 10)
- Call sites updated: 0 / ~200 (estimated)
- Test pass rate: 100% (maintain)
- Compilation warnings: 0

### Qualitative
- Code review feedback: Positive consensus required
- Maintainability improvement: Survey of team members
- API clarity: Documentation quality assessment

## Open Questions

1. **Should `synlevel_begin()` become a method?**
   - It only increments `ls->level`
   - Could be simple `level++` at call sites
   - Decision: Convert for consistency, but consider inlining

2. **How to handle `fs_*` functions that take both LexState and FuncState?**
   - Option A: Make them LexState methods taking FuncState parameter
   - Option B: Make them FuncState methods accessing ls through fs->ls
   - Decision: Category 5 uses Option A (LexState methods)

3. **Should public API wrappers be maintained indefinitely?**
   - External code might depend on `lj_parse_keepstr`
   - Decision: Keep public API functions, implement via methods

4. **What about static inline helpers?**
   - Functions like `var_new_lit` are convenience wrappers
   - Decision: Convert to maintain consistency

## Timeline

| Phase | Duration | Completion Date |
|-------|----------|-----------------|
| Phase 1: Core Lexer | 1 week | TBD |
| Phase 2: Variables/Scope | 1 week | TBD |
| Phase 3: FuncState | 1 week | TBD |
| Phase 4: Expressions | 1 week | TBD |
| Phase 5: Statements | 1 week | TBD |
| Phase 6: Cleanup | 1 week | TBD |
| **Total** | **6 weeks** | TBD |

## References

### Related Commits
- Recent lexer refactoring: commits 1d021a5d1, f68a92ee2, ab16a3cc2, 5f71732de
- LexState class introduction: commit f68a92ee2

### Documentation
- `docs/wiki/Fluid-*.md` - Fluid language documentation
- `src/fluid/luajit-2.1/README` - LuaJIT documentation
- `CLAUDE.md` - Repository coding standards

### Similar Refactorings
- The lexer class refactoring (2025-01) provides a template for this work
- Modern C++20 patterns already established in codebase

## Notes

- This plan focuses exclusively on `LexState*` parameter functions
- Functions taking `FuncState*` as first parameter remain unchanged
- The refactoring maintains all existing functionality
- No API changes for external consumers (public functions remain)
- Each phase is independently testable and committable

## Status

- **Created**: 2025-01-16
- **Status**: Planning
- **Owner**: TBD
- **Last Updated**: 2025-01-16
