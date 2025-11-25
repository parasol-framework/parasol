# Safe Navigation Operators Implementation Plan

## Overview

This plan describes the implementation of safe navigation operators for Fluid, allowing developers to safely access nested properties without explicit nil checks. The operators short-circuit evaluation when the base expression is `nil`, returning `nil` instead of raising an error.

## Status

- [x] Phase 1: Lexer Changes
- [x] Phase 2: AST Node Changes
- [x] Phase 3: AST Builder Changes
- [x] Phase 4: IR Emitter Changes
- [x] Phase 5: Testing

### Operators to Implement

| Operator | Syntax | Semantics |
|----------|--------|-----------|
| Safe field access | `a?.b` | If `a` is nil, return nil; otherwise return `a.b` |
| Safe index access | `a?[k]` | If `a` is nil, return nil; otherwise return `a[k]` |
| Safe method call | `a?:m(...)` | If `a` is nil, return nil; otherwise call `a:m(...)` |

### Design Principles

1. **Structural representation**: Safe navigation is captured in the AST, not as runtime flags
2. **Localised control flow**: Short-circuit jumps are managed within the IR emitter, not across parsing phases
3. **RAII register management**: Use `RegisterGuard` to prevent register leaks on all exit paths
4. **Chain support**: `a?.b?.c?:method()` should short-circuit the entire chain on any nil

---

## Phase 1: Lexer Changes

### New Tokens

Add three new compound tokens to `lexer.h`:

```cpp
// In TKDEF macro (around line 28)
__(safe_field, ?.) __(safe_index, ?[) __(safe_method, ?:)
```

### Lexer Recognition

In `lexer.cpp`, extend the `?` case in the main switch to recognise the new operators:

```cpp
case '?':
   lex_next(ls);
   if (ls->c == '.') {
      lex_next(ls);
      return TK_safe_field;
   }
   if (ls->c == '[') {
      lex_next(ls);
      return TK_safe_index;
   }
   if (ls->c == ':') {
      lex_next(ls);
      return TK_safe_method;
   }
   if (ls->c == '?') {
      lex_next(ls);
      return TK_if_empty;  // existing ?? operator
   }
   // Single ? for presence check - existing behaviour
   return '?';
```

### Token Types

Add corresponding `TokenKind` entries in `token_types.h`:

```cpp
SafeField,    // ?.
SafeIndex,    // ?[
SafeMethod,   // ?:
```

---

## Phase 2: AST Node Changes

### Option A: Dedicated Node Types (Recommended)

Add new expression kinds to `ast_nodes.h`:

```cpp
enum class AstNodeKind : uint16_t {
   // ... existing kinds ...
   SafeMemberExpr,
   SafeIndexExpr,
   SafeCallExpr,  // For safe method calls
};
```

Create dedicated payload structures:

```cpp
struct SafeMemberExprPayload {
   SafeMemberExprPayload() = default;
   SafeMemberExprPayload(const SafeMemberExprPayload&) = delete;
   SafeMemberExprPayload& operator=(const SafeMemberExprPayload&) = delete;
   SafeMemberExprPayload(SafeMemberExprPayload&&) noexcept = default;
   SafeMemberExprPayload& operator=(SafeMemberExprPayload&&) noexcept = default;
   ExprNodePtr table;
   Identifier member;
   ~SafeMemberExprPayload();
};

struct SafeIndexExprPayload {
   SafeIndexExprPayload() = default;
   SafeIndexExprPayload(const SafeIndexExprPayload&) = delete;
   SafeIndexExprPayload& operator=(const SafeIndexExprPayload&) = delete;
   SafeIndexExprPayload(SafeIndexExprPayload&&) noexcept = default;
   SafeIndexExprPayload& operator=(SafeIndexExprPayload&&) noexcept = default;
   ExprNodePtr table;
   ExprNodePtr index;
   ~SafeIndexExprPayload();
};

// Safe method calls use CallExprPayload with a flag or separate SafeMethodCallTarget
struct SafeMethodCallTarget {
   SafeMethodCallTarget() = default;
   SafeMethodCallTarget(const SafeMethodCallTarget&) = delete;
   SafeMethodCallTarget& operator=(const SafeMethodCallTarget&) = delete;
   SafeMethodCallTarget(SafeMethodCallTarget&&) noexcept = default;
   SafeMethodCallTarget& operator=(SafeMethodCallTarget&&) noexcept = default;
   ExprNodePtr receiver;
   Identifier method;
   ~SafeMethodCallTarget();
};

// Extend CallTarget variant
using CallTarget = std::variant<DirectCallTarget, MethodCallTarget, SafeMethodCallTarget>;
```

Update the `ExprNode::data` variant to include the new payloads.

### Builder Helpers

Add factory functions in `ast_nodes.h`:

```cpp
ExprNodePtr make_safe_member_expr(SourceSpan span, ExprNodePtr table, Identifier member);
ExprNodePtr make_safe_index_expr(SourceSpan span, ExprNodePtr table, ExprNodePtr index);
ExprNodePtr make_safe_method_call_expr(SourceSpan span, ExprNodePtr receiver, Identifier method,
   ExprNodeList arguments, bool forwards_multret);
```

---

## Phase 3: AST Builder Changes

### Extend parse_suffixed()

Modify `ast_builder.cpp` to recognise safe navigation tokens:

```cpp
ParserResult<ExprNodePtr> AstBuilder::parse_suffixed(ExprNodePtr base)
{
   while (true) {
      Token token = this->ctx.tokens().current();

      // Existing: regular field access
      if (token.kind() IS TokenKind::Dot) {
         // ... existing code ...
         continue;
      }

      // NEW: Safe field access ?.
      if (token.kind() IS TokenKind::SafeField) {
         this->ctx.tokens().advance();
         auto name_token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
         if (not name_token.ok()) return ParserResult<ExprNodePtr>::failure(name_token.error_ref());
         base = make_safe_member_expr(span_from(token, name_token.value_ref()), std::move(base),
            make_identifier(name_token.value_ref()));
         continue;
      }

      // Existing: regular index access
      if (token.kind() IS TokenKind::LeftBracket) {
         // ... existing code ...
         continue;
      }

      // NEW: Safe index access ?[
      if (token.kind() IS TokenKind::SafeIndex) {
         this->ctx.tokens().advance();
         auto index = this->parse_expression();
         if (not index.ok()) return index;
         this->ctx.consume(TokenKind::RightBracket, ParserErrorCode::ExpectedToken);
         SourceSpan span = combine_spans(base->span, index.value_ref()->span);
         base = make_safe_index_expr(span, std::move(base), std::move(index.value_ref()));
         continue;
      }

      // Existing: regular method call
      if (token.kind() IS TokenKind::Colon) {
         // ... existing code ...
         continue;
      }

      // NEW: Safe method call ?:
      if (token.kind() IS TokenKind::SafeMethod) {
         this->ctx.tokens().advance();
         auto name_token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
         if (not name_token.ok()) return ParserResult<ExprNodePtr>::failure(name_token.error_ref());
         bool forwards = false;
         auto args = this->parse_call_arguments(&forwards);
         if (not args.ok()) return ParserResult<ExprNodePtr>::failure(args.error_ref());
         SourceSpan span = combine_spans(base->span, name_token.value_ref().span());
         base = make_safe_method_call_expr(span, std::move(base),
            make_identifier(name_token.value_ref()), std::move(args.value_ref()), forwards);
         continue;
      }

      // ... rest of existing code ...
      break;
   }
   return ParserResult<ExprNodePtr>::success(std::move(base));
}
```

---

## Phase 4: IR Emitter Changes

### New Emission Methods

Add three new emission methods to `IrEmitter` in `ir_emitter.h`:

```cpp
ParserResult<ExpDesc> emit_safe_member_expr(const SafeMemberExprPayload& payload);
ParserResult<ExpDesc> emit_safe_index_expr(const SafeIndexExprPayload& payload);
ParserResult<ExpDesc> emit_safe_call_expr(const CallExprPayload& payload);  // For SafeMethodCallTarget
```

### emit_expression() Dispatch

Update the switch in `emit_expression()`:

```cpp
case AstNodeKind::SafeMemberExpr:
   return this->emit_safe_member_expr(std::get<SafeMemberExprPayload>(expr.data));
case AstNodeKind::SafeIndexExpr:
   return this->emit_safe_index_expr(std::get<SafeIndexExprPayload>(expr.data));
case AstNodeKind::SafeCallExpr:
   return this->emit_safe_call_expr(std::get<CallExprPayload>(expr.data));
```

### Implementation Pattern

All safe navigation emitters follow the same pattern (modeled on ternary expression):

```cpp
ParserResult<ExpDesc> IrEmitter::emit_safe_member_expr(const SafeMemberExprPayload& Payload)
{
   if (not Payload.table or Payload.member.symbol IS nullptr) {
      return this->unsupported_expr(AstNodeKind::SafeMemberExpr, Payload.member.span);
   }

   // 1. Emit the base table expression
   auto table_result = this->emit_expression(*Payload.table);
   if (not table_result.ok()) return table_result;

   RegisterGuard register_guard(&this->func_state);
   RegisterAllocator allocator(&this->func_state);

   // 2. Materialise base to a register
   ExpressionValue table_value(&this->func_state, table_result.value_ref());
   BCREG base_reg = table_value.discharge_to_any_reg(allocator);

   // 3. Emit nil check: if base_reg == nil, jump to short-circuit path
   ExpDesc nilv(ExpKind::Nil);
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, base_reg, const_pri(&nilv)));
   ControlFlowEdge nil_jump = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));

   // 4. Non-nil path: emit normal field access
   ExpDesc table = table_value.legacy();
   ExpDesc key(Payload.member.symbol);
   expr_index(&this->func_state, &table, &key);

   // 5. Materialise result to the base register (reuse for efficiency)
   this->materialise_to_reg(table, base_reg, "safe member access");
   allocator.collapse_freereg(base_reg);

   // 6. Skip past the nil-result path
   ControlFlowEdge skip_nil_result = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));

   // 7. Nil path: load nil into base_reg
   BCPOS nil_path_start = this->func_state.pc;
   nil_jump.patch_to(nil_path_start);
   bcemit_nil(&this->func_state, base_reg, 1);

   // 8. Merge paths
   skip_nil_result.patch_to(this->func_state.pc);

   // 9. Disarm guard to preserve result register
   register_guard.disarm();

   ExpDesc result;
   result.init(ExpKind::NonReloc, base_reg);
   return ParserResult<ExpDesc>::success(result);
}
```

### Safe Index Implementation

Similar to safe member, but evaluates the index expression only on the non-nil path:

```cpp
ParserResult<ExpDesc> IrEmitter::emit_safe_index_expr(const SafeIndexExprPayload& Payload)
{
   // ... setup as above ...

   // 3. Emit nil check
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, base_reg, const_pri(&nilv)));
   ControlFlowEdge nil_jump = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));

   // 4. Non-nil path: evaluate index expression ONLY HERE (short-circuit)
   auto key_result = this->emit_expression(*Payload.index);
   if (not key_result.ok()) return key_result;
   ExpDesc key = key_result.value_ref();
   ExpressionValue key_toval(&this->func_state, key);
   key_toval.to_val();
   key = key_toval.legacy();

   ExpDesc table = table_value.legacy();
   expr_index(&this->func_state, &table, &key);

   // ... rest as above ...
}
```

### Safe Method Call Implementation

Combines nil check with method call emission:

```cpp
ParserResult<ExpDesc> IrEmitter::emit_safe_call_expr(const CallExprPayload& Payload)
{
   const auto* safe_method = std::get_if<SafeMethodCallTarget>(&Payload.target);
   if (not safe_method or not safe_method->receiver or safe_method->method.symbol IS nullptr) {
      return this->unsupported_expr(AstNodeKind::SafeCallExpr, SourceSpan{});
   }

   // 1. Emit receiver
   auto receiver_result = this->emit_expression(*safe_method->receiver);
   if (not receiver_result.ok()) return receiver_result;

   RegisterGuard register_guard(&this->func_state);
   RegisterAllocator allocator(&this->func_state);

   // 2. Materialise receiver
   ExpressionValue receiver_value(&this->func_state, receiver_result.value_ref());
   BCREG base_reg = receiver_value.discharge_to_any_reg(allocator);

   // 3. Nil check
   ExpDesc nilv(ExpKind::Nil);
   bcemit_INS(&this->func_state, BCINS_AD(BC_ISEQP, base_reg, const_pri(&nilv)));
   ControlFlowEdge nil_jump = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));

   // 4. Non-nil path: emit method call (arguments evaluated only here)
   ExpDesc callee = receiver_value.legacy();
   ExpDesc key(ExpKind::Str);
   key.u.sval = safe_method->method.symbol;
   bcemit_method(&this->func_state, &callee, &key);
   BCREG call_base = callee.u.s.info;

   BCREG arg_count = 0;
   ExpDesc args(ExpKind::Void);
   if (not Payload.arguments.empty()) {
      auto args_result = this->emit_expression_list(Payload.arguments, arg_count);
      if (not args_result.ok()) return ParserResult<ExpDesc>::failure(args_result.error_ref());
      args = args_result.value_ref();
   }

   // ... emit CALL instruction ...

   // 5. Skip nil path
   ControlFlowEdge skip_nil = this->control_flow.make_unconditional(bcemit_jmp(&this->func_state));

   // 6. Nil path
   nil_jump.patch_to(this->func_state.pc);
   bcemit_nil(&this->func_state, base_reg, 1);

   // 7. Merge
   skip_nil.patch_to(this->func_state.pc);

   // ... cleanup and return ...
}
```

---

## Phase 5: Chain Short-Circuiting

### The Challenge

For chained expressions like `a?.b?.c`, each safe access should short-circuit the entire remaining chain, not just one level. The naive implementation would evaluate:

```
if a == nil then nil
else
   if a.b == nil then nil
   else a.b.c
```

This is correct but generates redundant nil checks.

### Solution: Implicit Short-Circuit Propagation

Since safe navigation expressions return `nil` when the base is `nil`, chained expressions naturally short-circuit:

1. `a?.b` returns `nil` if `a` is nil
2. `(a?.b)?.c` then sees a `nil` base and returns `nil`

No special handling is needed—the recursive structure handles chains automatically.

### Optimisation (Future)

A future optimisation could detect chains at the AST level and emit a single nil check for the entire chain base, sharing the short-circuit jump across all accesses. This is not required for correctness.

---

## Phase 6: Testing Strategy

### Unit Tests

The existing test file `src/fluid/tests/test_safe_nav.fluid` provides comprehensive coverage:

- Basic nil handling (`testSafeFieldOnNil`, `testSafeIndexOnNil`, `testSafeMethodOnNil`)
- Value propagation (`testSafeFieldChain`, `testSafeIndexValue`, `testSafeMethodValue`)
- Short-circuit verification (`testSafeIndexShortCircuit`, `testSafeMethodOnNil` checks argument evaluation)
- Integration with `??` operator (`testIntegrationWithPresence`)
- Edge cases: false values, zero values, empty strings, three-level chaining

### Additional Tests to Add

```lua
-- Chained safe navigation
function testChainedSafeNav()
   local a = nil
   local b = { c = nil }
   local d = { c = { d = 42 } }

   assert(a?.b?.c is nil)
   assert(b?.c?.d is nil)
   assert(d?.c?.d is 42)
end

-- Mixed operators in chain
function testMixedChain()
   local obj = { arr = { [1] = { name = "test" } } }
   local result = obj?.arr?[1]?.name
   assert(result is "test")

   local missing = nil
   assert(missing?.arr?[1]?.name is nil)
end

-- Safe method with side effects (verify short-circuit)
function testSafeMethodSideEffects()
   local log = {}
   local function track(msg)
      table.insert(log, msg)
      return msg
   end

   local obj = nil
   local result = obj?:method(track("should not appear"))
   assert(result is nil)
   assert(#log is 0, "Arguments should not be evaluated when receiver is nil")
end
```

### Integration with Flute

Register tests in CMakeLists.txt:

```cmake
flute_test(safe_nav src/fluid/tests/test_safe_nav.fluid)
```

---

## Phase 7: Error Handling

### Parser Errors

Add clear error messages for malformed safe navigation:

```cpp
// In ast_builder.cpp
if (token.kind() IS TokenKind::SafeField) {
   this->ctx.tokens().advance();
   auto name_token = this->ctx.expect_identifier(ParserErrorCode::ExpectedIdentifier);
   if (not name_token.ok()) {
      // Error: "Expected identifier after '?.'"
      return ParserResult<ExprNodePtr>::failure(name_token.error_ref());
   }
   // ...
}
```

### Runtime Behaviour

Safe navigation does **not** catch errors from the accessed member/method itself. For example:

```lua
local obj = { method = function(self) error("boom") end }
obj?:method()  -- Still raises "boom" (obj is not nil)
```

This matches the behaviour in other languages with safe navigation (TypeScript, C#, Kotlin).

---

## Implementation Order

1. **Lexer tokens** (1-2 hours)
   - Add tokens to `TKDEF` in `lexer.h`
   - Add recognition logic in `lexer.cpp`
   - Add `TokenKind` entries

2. **AST nodes** (2-3 hours)
   - Add node kinds and payload structs to `ast_nodes.h`
   - Implement destructors in `ast_nodes.cpp`
   - Add builder helpers

3. **AST builder** (1-2 hours)
   - Extend `parse_suffixed()` with new token cases

4. **IR emitter** (4-6 hours)
   - Implement `emit_safe_member_expr()`
   - Implement `emit_safe_index_expr()`
   - Implement `emit_safe_call_expr()`
   - Update `emit_expression()` dispatch

5. **Testing** (2-3 hours)
   - Verify existing tests pass
   - Add chain and edge case tests
   - Test bytecode output with `--jit-options dump-bytecode`

**Total estimated effort: 10-16 hours**

---

## Potential Pitfalls

### 1. Register Allocation in Chains

**Risk**: Nested safe accesses may orphan registers if the short-circuit path doesn't clean up properly.

**Mitigation**: Always use `RegisterGuard` with RAII cleanup. The guard's destructor ensures `freereg` is restored even on early returns.

### 2. Interaction with `??` Operator

**Risk**: `a?.b ?? "default"` might not correctly apply the fallback.

**Mitigation**: The current design naturally handles this:
- `a?.b` returns `nil` if `a` or `a.b` is `nil`
- `??` sees the `nil` and applies the fallback

No special integration is needed.

### 3. Assignment Targets

**Risk**: Allowing `obj?.field = value` (safe navigation on LHS of assignment).

**Decision**: This is explicitly **not supported** in this implementation. Safe navigation is for reading values. Assignment to a possibly-nil base is semantically ambiguous and should require explicit nil checks.

If attempted, the parser should emit a clear error: "Cannot use safe navigation as assignment target".

### 4. Method Chaining with Return Values

**Risk**: `obj?:method1()?:method2()` where method1 returns nil but isn't the base.

**Mitigation**: Each `?:` checks its immediate receiver. The chain works correctly:
- If `obj` is nil → entire expression returns nil
- If `obj:method1()` returns nil → `nil?:method2()` returns nil
- Otherwise → normal execution

---

## References

- Previous implementation analysis: `docs/plans/fluid-safe-nav-fragile.md`
- Parser architecture: `src/fluid/luajit-2.1/src/parser/AGENTS.md`
- Ternary expression (control flow pattern): `ir_emitter.cpp:emit_ternary_expr()`
- Existing test suite: `src/fluid/tests/test_safe_nav.fluid`
