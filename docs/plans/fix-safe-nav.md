# Fix Safe Navigation Short‑Circuiting (Fluid/LuaJIT)

## Overview
Safe navigation for Fluid (`?.`, `?:`, and `?[`) is implemented (lexer + parser), but tests show that runtime short‑circuiting is not occurring. Access and calls still execute when the base is `nil`, causing "attempt to index ... a nil value" failures.

Goal: Make the safe field/index/method helpers generate correct bytecode so that:
- `obj?.field` returns `nil` if `obj` is `nil` (no table access attempted)
- `obj?[key]` returns `nil` if `obj` is `nil` (no index/key evaluation)
- `obj?:method(args...)` returns `nil` if `obj` is `nil` (no method lookup, no arg evaluation)
- Chaining and multi‑return semantics work as intended

Primary file: `src/fluid/luajit-2.1/src/lj_parse.c`

Reference tests: `src/fluid/tests/test_safe_nav.fluid`

Context links:
- Chat log: `codex-chat-2025-11-07.md`
- Plan that introduced feature: `docs/plans/safe-navigation.md`

## Symptoms (from user’s test log)
Typical failures observed:
- attempt to index local 'user' (a nil value)
- attempt to index field 'profile' (a nil value)
- attempt to index a nil value

Interpretation: The nil‑check branch is not actually skipping the field/index/method work at runtime.

## Root Cause Hypothesis
- The helpers emit ISEQP + BC_JMP sequences, but use `jmp_patch()` where `jmp_patchins()` is required for patching a single, direct `BC_JMP` instruction by PC.
- In `expr_safe_method`, on the nil path `obj` is not initialised before the final `*v = obj`, causing undefined behavior (the resulting expression descriptor may be uninitialised or wrong).

Both issues align with the prior session’s observations and the PR review notes.

## What’s Already Implemented (keep intact)
- Tokens: `TK_safe_field`, `TK_safe_method` in `src/fluid/luajit-2.1/src/lj_lex.h`
- Lexer recognition: `?.` and `?:` in `src/fluid/luajit-2.1/src/lj_lex.c`
- Parser helpers and suffix integration:
  - `expr_safe_field`, `expr_safe_index`, `expr_safe_method` in `src/fluid/luajit-2.1/src/lj_parse.c`
  - Suffix handling added to `expr_primary` / simple paths

## Changes To Make
1) Patch single‑JMPs with `jmp_patchins` instead of `jmp_patch` in all three helpers
- Affects jump that enters the non‑nil path and the `skip_end` merge jump.

2) Initialise `obj` on the nil path in `expr_safe_method`
- After emitting `bcemit_AD(..., base_reg, VKNIL)` add `expr_init(&obj, VNONRELOC, base_reg);`
- Ensures `*v = obj;` at the end yields a valid expression on both paths; nil path returns a single `nil` value.

3) Do NOT free the object register right after `expr_toanyreg`
- The PR suggestion to add `expr_free(fs, v)` immediately after `expr_toanyreg` (in field/index/method) is rejected.
- Rationale: The base object register is needed for the index/method emission that follows. Freeing it risks aliasing with `result_reg` and clobbering the base, causing wrong codegen.

4) Preserve intended semantics
- Field/Index: Both branches must write to the same `result_reg`. Nil branch loads `nil` there; non‑nil branch computes into that register.
- Method call: Only evaluate arguments on the non‑nil path. Preserve `VCALL` when base is non‑nil to keep multi‑return semantics (`local a,b = obj?.f()` yields one `nil` on nil path, full results on non‑nil path).

## Exact Edit Points
File: `src/fluid/luajit-2.1/src/lj_parse.c`

- Safe field: around 2108–2122
  - Replace:
    - `jmp_patch(fs, jump_call, fs->pc);` with `jmp_patchins(fs, jump_call, fs->pc);`
    - `jmp_patch(fs, skip_end, fs->pc);` with `jmp_patchins(fs, skip_end, fs->pc);`

- Safe index: around 2141–2156
  - Replace:
    - `jmp_patch(fs, jump_call, fs->pc);` with `jmp_patchins(fs, jump_call, fs->pc);`
    - `jmp_patch(fs, skip_end, fs->pc);` with `jmp_patchins(fs, skip_end, fs->pc);`

- Safe method: around 2176–2191
  - Replace:
    - `jmp_patch(fs, jump_call, fs->pc);` with `jmp_patchins(fs, jump_call, fs->pc);`
    - `jmp_patch(fs, skip_end, fs->pc);` with `jmp_patchins(fs, skip_end, fs->pc);`
  - Add after `bcemit_AD(fs, BC_KPRI, base_reg, VKNIL);`:
    - `expr_init(&obj, VNONRELOC, base_reg);`

Note: Line numbers are based on the current tree; adjust if the file shifts.

## Build & Test (per AGENTS.md)
- Configure (if needed):
  - Debug: `cmake -S . -B build/agents -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=build/agents-install -DRUN_ANYWHERE=TRUE -DPARASOL_STATIC=ON -DPARASOL_VLOG=TRUE`
- Build module(s) and `parasol_cmd` then install:
  - `cmake --build build/agents --config Debug --target fluid parasol_cmd --parallel`
  - `cmake --install build/agents --config Debug`
- Run tests (always post‑install):
  - `ctest --test-dir build/agents -C Debug -L fluid_safe_nav --output-on-failure`

If a manual run is desired (Linux):
- `cd src/fluid/tests`
- `../../..//build/agents-install/parasol ../../../tools/flute.fluid file=$(pwd)/test_safe_nav.fluid --gfx-driver=headless --log-warning`

## Verification Checklist
- Field
  - [ ] `nil?.field` returns `nil`
  - [ ] `obj?.field` returns value
- Index
  - [ ] `nil?[key]` returns `nil` and does not evaluate `key` expression
  - [ ] `obj?[key]` returns value
- Method
  - [ ] `nil?:f(args...)` returns `nil` and does not evaluate `args`
  - [ ] `obj?:f(args...)` calls method
  - [ ] `local a,b = obj?.f()` preserves multi‑return on non‑nil base; nil base yields a single `nil`
- Chaining
  - [ ] `obj?.a?.b` short‑circuits properly
- Falsey values
  - [ ] `{flag=false}?.flag` is `false`, `{count=0}?.count` is `0`, `{text=""}?.text` is ""
- Presence and if‑empty integration unchanged

## Notes on Code Style & Scope
- These edits are in LuaJIT C sources; project’s “CRITICAL C++ REQUIREMENTS” don’t apply here. Follow existing LuaJIT style and local macros.
- Avoid adding `expr_free(fs, v)` after `expr_toanyreg` in these helpers; it’s not a leak in this context and can be harmful due to upcoming use of the base register.

## Troubleshooting
- If failures persist, dump bytecode for a minimal case (dev only):
  - Set `LUAJIT_DUMPBC=1` and run a tiny script exercising `nil?.field` to confirm the ISEQP/JMP/KPRI/… layout around the skip points.
- Confirm the helpers are being compiled into the installed binary (do a targeted rebuild of `fluid` and `parasol_cmd`, then re‑install).

## Acceptance Criteria
- All tests in `src/fluid/tests/test_safe_nav.fluid` pass via `ctest -L fluid_safe_nav`.
- No regressions in existing Fluid parsing or `?`/`??` operator semantics.
- Safe navigation yields nil‑only short‑circuiting and does not evaluate RHS work when base is nil.

## Appendix: PR Review Items
- Initialising `obj` in the nil path of `expr_safe_method`: ACCEPTED (prevents UB and enforces single‑nil result)
- Adding `expr_free(fs, v)` after `expr_toanyreg` in helpers: REJECTED (base reg still needed; freeing may alias/clobber)

