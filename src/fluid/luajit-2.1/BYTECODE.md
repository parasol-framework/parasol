# Bytecode Semantics Reference (Draft Layout)

> **Note to writer:** This file is a *scaffold* only. Do not remove the instructional text until the document is fully written and reviewed. Replace each “Instructions for this section” block with real content when authoring the final version.

---

## 1. Introduction and Scope

**Instructions for this section:**
- Explain *why* this document exists:
  - To capture the semantics of LuaJIT bytecode as used by the Fluid parser and AST pipeline.
  - To prevent control-flow misunderstandings (e.g. around `BC_ISEQP` “skip-next” behaviour) when modifying operators or control structures.
- Clarify that this document reflects the **Parasol‑integrated LuaJIT 2.1** variant, not arbitrary upstream forks.
- State the primary audiences:
  - Parser / IR emitter maintainers (`IrEmitter`, `OperatorEmitter`).
  - People adding / debugging operators (`??`, `?`, ternary, logical ops, bitwise ops).
- Briefly list the main kinds of questions this doc should answer (e.g. “What does `BC_ISEQP` do when values are equal?”, “How do I wire short‑circuiting for `lhs ?? rhs`?”).

---

## 2. Notation, Conventions, and Versioning

**Instructions for this section:**
- Define the notational conventions used in the rest of the document:
  - How registers are denoted (`R0`, `R1`, `base`, `A/B/C/D` fields, etc.).
  - How conditions are written (e.g. “*equal → skip next instruction; not equal → execute next instruction*”).
  - How “next instruction” is interpreted in the presence of `JMP`.
- State which **LuaJIT version and configuration** this description matches (e.g. LuaJIT 2.1, Parasol’s integrated tree, `LJ_FR2` assumptions where relevant).
- Include a note on how to keep this file in sync:
  - “Whenever bytecode emission patterns change in `parse_operators.cpp`, `operator_emitter.cpp`, or `ir_emitter.cpp`, review and update this document.”

---

## 3. Bytecode Overview

### 3.1 High-Level Structure

**Instructions for this subsection:**
- Provide a brief overview of how LuaJIT bytecode is structured:
  - Instruction words (`BCIns`), line table, prototype (`GCproto`), etc.
  - The meaning of A/B/C/D fields at a high level.
- Emphasise that this document focuses on **control‑flow and conditional instructions**, not the full opcode set.

### 3.2 Resources and Cross-References

**Instructions for this subsection:**
- List key source files that define or manipulate bytecode:
  - Where opcodes are defined (e.g. `lj_bc.h`, `lj_bcdef.h`).
  - Where they are emitted from the parser paths (`parse_operators.cpp`, `operator_emitter.cpp`, `ir_emitter.cpp`).
  - Any relevant AGENTS or design docs (e.g. parser AGENTS file).
- For each referenced file, briefly state *what kind of information* it contains (definitions, patterns, tests).

---

## 4. Conditional and Comparison Bytecodes

### 4.1 Summary Table: Conditional Bytecode Matrix

**Instructions for this subsection:**
- Provide a **compact table** (matrix) summarising:
  - Opcode (e.g. `BC_ISEQP`, `BC_ISEQN`, `BC_ISEQS`, `BC_ISEQV`, `BC_ISNEP`, `BC_ISNEV`, `BC_ISLT`, `BC_ISGE`, etc.).
  - Logical condition on operands (e.g. “A == const”, “A != const”, “A < B”).
  - Behaviour for a **true** condition: does the VM **execute or skip** the next instruction?
  - Behaviour for a **false** condition: does the VM **execute or skip** the next instruction?
  - Typical immediate follower instruction in the parser (`JMP`, `KPRI`, etc.).
- The goal is that a maintainer can answer “if this comparison is true, does the subsequent `JMP` run or get skipped?” in one glance.

### 4.2 Equality-with-Constant Opcodes (`BC_ISEQP`, `BC_ISEQN`, `BC_ISEQS`, `BC_ISNEP`, `BC_ISNEV`)

**Instructions for this subsection:**
- For each of these opcodes:
  - Restate the condition in plain language (e.g. “`BC_ISEQP A, const` tests equality with a primitive (‘pri’) constant”).
  - Explain **exactly** when the VM skips the next instruction vs executes it.
  - Show the canonical **“compare + `JMP`” pattern** for “branch when *not* equal” vs “branch when equal”.
- Include at least one concrete, annotated example that matches patterns used in the parser (e.g. the presence operator’s comparison chain).

### 4.3 Generic Comparison Opcodes (`BC_ISLT`, `BC_ISGE`, `BC_ISLE`, `BC_ISGT`)

**Instructions for this subsection:**
- Describe how these opcodes compare two registers (or register vs constant) and how they interact with `JMP`.
- Document:
  - The condition they test.
  - Whether they follow the same “skip next instruction when condition is true” idiom, or something different.
- Include a simple pattern example used by the parser (e.g. numeric `for` loop bounds, or comparison operators in expressions).

### 4.4 Interaction with `BC_JMP` and Jump Lists

**Instructions for this subsection:**
- Describe the pattern “comparison followed by `BC_JMP`” used by the parser:
  - What it means for a jump to be **taken** vs **skipped** in this encoding.
  - How `ControlFlowEdge` / `jmp_patch` manage these targets in the C++ code.
- Clarify the difference between:
  - A single `JMP` being patched.
  - A **list** of jumps (e.g. for chained conditions) all patched to a shared target.
- Include guidance on **reading** these patterns in the emitted bytecode listing (e.g. from `mtDebugLog('disasm')`).

---

## 5. Control-Flow and Short-Circuit Patterns

### 5.1 Logical Operators (`and`, `or`)

**Instructions for this subsection:**
- Describe, at a bytecode level, how short‑circuiting is achieved for `and` / `or`:
  - How the first operand is evaluated and normalised.
  - How conditional branches are wired so that the RHS is evaluated only when needed.
- Provide one or two annotated bytecode examples of `a and b` and `a or b` with truthy and falsey first operands.

### 5.2 Ternary Operator (`cond ? true_val :> false_val`)

**Instructions for this subsection:**
- Explain the ternary operator’s contract:
  - Only one of the branches is evaluated.
  - The final result lives in a **single, predictable register** (e.g. the condition’s register).
- Map out the bytecode pattern used by `IrEmitter::emit_ternary_expr`:
  - How extended falsey semantics are implemented (if applicable).
  - How jumps from the condition drive control into the true vs false branch.
  - How register collapse (`freereg`) is handled after each branch.
- Include an example or pseudo‑bytecode trace for a simple ternary expression.

### 5.3 Presence Operator (`x?`) – Extended Falsey Check

**Instructions for this subsection:**
- Describe the semantics of the presence operator:
  - Which values are considered falsey (nil, false, 0, empty string).
  - How it differs from Lua’s built‑in truthiness.
- Document the **comparison + jump chain** pattern used to implement these checks:
  - How each `BC_ISEQP` / `BC_ISEQN` / `BC_ISEQS` is followed by a `JMP`.
  - How the jumps are patched to the “false” or “true” branch.
- Emphasise that this section should make it obvious why mis‑wiring these patches changes the semantics.

### 5.4 If-Empty Operator (`lhs ?? rhs`) – Short-Circuiting with Extended Falsey Semantics

**Instructions for this subsection:**
- State the high‑level semantics:
  - Evaluate `lhs`; if it is extended‑falsey (nil/false/0/""), evaluate and return `rhs`.
  - Otherwise, return `lhs` and **do not** evaluate `rhs`.
- Describe AST pipeline path (`IrEmitter::emit_if_empty_expr` and any `OperatorEmitter` support).
- Spell out the comparison+JMP chain for extended falsey values.
- Explain exactly how jumps are patched to “evaluate RHS” vs “skip RHS”.
- Note how the result is kept in a single register and how `freereg` is collapsed to avoid leaks / extra arguments.
- Explicitly warn about the common misinterpretation (e.g. “all checks chain to RHS, therefore RHS always runs”) and show why it’s incorrect for the current implementation.

---

## 6. Register Semantics and Multi-Value Behaviour

### 6.1 Register Lifetimes, `freereg`, and `nactvar`

**Instructions for this subsection:**
- Summarise how LuaJIT manages:
  - Active variables (`nactvar`).
  - Free registers (`freereg`) above the active window.
- Explain how the parser and register allocator are expected to:
  - Allocate temporaries.
  - Collapse `freereg` when a temporary is no longer needed (e.g. via helpers like `ir_collapse_freereg`, `RegisterGuard`, `expr_free`).
- Highlight why getting this wrong can manifest as:
  - Extra values leaking into vararg / function call contexts.
  - Spurious arguments or return values.

### 6.2 `ExpKind::Call` and Multi-Return Semantics

**Instructions for this subsection:**
- Describe how a `BC_CALL` result initially appears as an `ExpKind::Call` with a base register and auxiliary info.
- Clarify:
  - When a call is allowed to propagate multiple returns (e.g. `BC_CALLM` for argument lists).
  - When operators or expressions are required to *collapse* it to a single value.
- Show patterns where:
  - Calls are used as operands to binary operators (must restrict to single result).
  - Calls are used in contexts like `lhs ?? rhs`, presence check, or ternary.

### 6.3 Preventing Register Leaks in Chained Operations

**Instructions for this subsection:**
- Summarise known pitfalls documented elsewhere (e.g. in AGENTS docs):
  - Orphaned registers when chaining across precedence levels.
  - Leaked slots when reusing borrowed registers incorrectly.
- Provide concrete rules of thumb:
  - When to reuse the top‑of‑stack register from LHS.
  - When to free or collapse temporary registers after an operation finish.

---

## 7. Common Emission Patterns and Anti-Patterns

### 7.1 Canonical Patterns (Do This)

**Instructions for this subsection:**
- Provide a small catalogue of **“blessed” patterns**:
  - Conditional compare + `JMP` for “branch on not‑equal”.
  - Presence / if‑empty extended falsey check chains.
  - Logical operator short‑circuiting layouts.
  - Ternary implementation layout.
- For each, instruct the writer to:
  - Show the high‑level Fluid / Lua source.
  - Show the expected bytecode snippet.
  - Cross‑reference the exact helper functions that emit it.

### 7.2 Typical Mistakes (Do NOT Do This)

**Instructions for this subsection:**
- List recurring errors seen historically:
  - Misunderstanding “skip next instruction when equal” and wiring jumps to the wrong branch.
  - Forgetting to collapse `freereg` after emitting RHS values, leading to leaked arguments.
  - Allocating a new register without `expr_free`, causing multi‑value leaks.
- For each anti‑pattern, instruct the writer to:
  - Provide a short “buggy bytecode sketch”.
  - Explain the runtime symptom (e.g. RHS runs when it should not, extra arguments appear).
  - Point to one or more regression tests that would catch the issue.

---

## 8. Testing, Debugging, and Tooling

### 8.1 Using Flute and Fluid Tests

**Instructions for this subsection:**
- Explain how to use the existing Fluid test suite to validate control‑flow changes:
  - Which tests cover `??`, `?`, logical operators, ternary, etc. (e.g. `test_if_empty.fluid`).
- Recommend strategies for designing new tests:
  - Use side‑effects (counters, errors) on the RHS to detect unwanted evaluation.
  - Use vararg capturing (`...`) to spot leaked arguments.

### 8.2 Disassembly and Bytecode Inspection

**Instructions for this subsection:**
- Describe how to obtain disassemblies:
  - Via `mtDebugLog('disasm')` on `fluid` objects.
  - Any command‑line flags (`--jit-options dump-bytecode,diagnose`, etc.).
- Show how to map disassembled output back to:
  - Source expressions.
  - IR emitter / operator emitter code paths.
- Encourage using disassembly as the **source of truth** when reasoning about control‑flow.

---

## 9. Maintenance Guidelines

**Instructions for this section:**
- Define a checklist for contributors who touch:
  - Conditional bytecode emission.
  - Short‑circuiting operators.
  - Register allocation in operator paths.
- Include items like:
  - “Update the opcode matrix if you introduce or repurpose a conditional bytecode.”
  - “Add or update regression tests in `src/fluid/tests/` for new control‑flow behaviours.”
  - “Regenerate and examine disassembly for representative examples.”
- Clarify the process for reviewers:
  - What aspects they should double‑check against this document.
  - How to spot mismatches between code and documented semantics.

---

## 10. Glossary and Quick Reference

**Instructions for this section:**
- Provide short, precise definitions of key terms:
  - `BCIns`, `BCOp`, `freereg`, `nactvar`, `ExpKind`, “extended falsey”, “short‑circuit”, etc.
- Include a very concise **“cheat sheet”**:
  - A miniature version of the conditional‑bytecode matrix.
  - A one‑line summary for each major operator’s control‑flow pattern (`and`, `or`, `?`, `??`, ternary).
- Ensure this section is optimised for “I have 30 seconds to remind myself what `BC_ISEQP` does” scenarios.
