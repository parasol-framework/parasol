# LuaJIT 2.1 Integration Notes

This file captures practices and gotchas observed while maintaining the LuaJIT 2.1 sources that ship inside Parasol. Use it as a quick orientation before diving into changes.

**Note:** For parser-specific implementation details, register allocation, operator implementation, and debugging strategies, see [`src/parser/AGENTS.md`](src/parser/AGENTS.md).

**Note:** For library system details including LJLIB_* macros, buildvm code generation, fast function implementation, and JIT recording, see [`src/lib/AGENTS.md`](src/lib/AGENTS.md).

## Repository Layout Highlights
- `src/`: Upstream LuaJIT sources (parser, VM, JIT engine) with Parasol-specific modernisations
- `src/parser/`: C++20 refactored parser (see [`src/parser/AGENTS.md`](src/parser/AGENTS.md) for structure details)
- `src/lib/`: Library implementations with LJLIB_* macros (see [`src/lib/AGENTS.md`](src/lib/AGENTS.md) for buildvm and fast function details)
- `src/fluid/tests/`: Fluid regression tests exercise the embedded LuaJIT runtime
- [`BYTECODE.md`](BYTECODE.md): Complete bytecode instruction reference with control-flow semantics for parser/emitter work
- CMake writes generated headers, the VM object/assembly, and host helpers under `build/agents/src/fluid/luajit-generated/`. The final static library ends up in `build/agents/luajit-2.1/lib/`.


## Integration & Build Tips
- Always rebuild via CMake (e.g. `cmake --build build/agents --config <BuildType>`) after touching LuaJIT or Fluid sources so the static library target is regenerated and relinked into the Fluid module.
- CMake drives three build strategies, matching the logic in `src/fluid/CMakeLists.txt`:
  - **MSVC**: `msvcbuild_codegen.bat` produces generated headers and `lj_vm.obj`, and CMake links `lua51.lib` next to the upstream sources.
  - **Unix-like toolchains**: CMake builds the host tools (`minilua` and `buildvm`), generates assembly with DynASM, then archives `lj_vm.o` + `ljamalg.o` into `libluajit-5.1.a`.
- Install (`cmake --install build/agents --config <BuildType>`) before running tests so the freshly built `parasol` binary (or `parasol.exe` on Windows) and scripts land in `build/agents-install/`.

## Error Handling Configuration
- **Windows (MSVC)**: Must NOT define `LUAJIT_NO_UNWIND`. MSVC always uses Structured Exception Handling (SEH) via `RaiseException()` and `lj_err_unwind_win()`.  There is no "internal unwinding" implementation for MSVC - SEH is the only viable mechanism. Setting `LJ_NO_UNWIND` for MSVC breaks exception handling and causes catch() tests to fail with "attempt to call a nil value" errors.
- The `LJ_NO_UNWIND` flag results in broken code that corrupts memory if used in GCC builds.

## Testing
- Use `ctest --build-config <BuildType> --test-dir build/agents -R <label>` to run subsets, or omit `-R` for the full suite. Fluid regression tests are under `src/fluid/tests/` and catch most parser/VM regressions.
- For quick manual checks, launch `parasol` (or `parasol.exe` on Windows) from `build/agents-install/bin/` with `--no-crash-handler --log-warning` so failures bubble out as exit codes.
- **Critical**: After touching LuaJIT C sources, rebuild both the Fluid module and `parasol_cmd`, then reinstall:
  ```bash
  cmake --build build/agents --config <BuildType> --parallel
  cmake --install build/agents --config <BuildType>
  ```
- When debugging parser issues, create minimal Fluid scripts to isolate the behaviour before running the full test suite.
- Unit tests are managed by `MODTests()` in `src/fluid/fluid.cpp`.
- To run the compiled-in unit tests, run `src/fluid/tests/test_unit_tests.fluid` with the `--log-api` option to view the output from stderr.
- Run `parasol` with `--jit-options` to pass JIT engine flags as a CSV list, e.g. `--jit-options dump-bytecode,trace-boundary`.  Available options are:
  - `trace-tokens` Trace tokenisation
  - `trace-expect` Trace expectations
  - `trace-boundary` Trace boundary crossings between interpreted and JIT code
  - `dump-bytecode` Dump disassembled bytecode at the end of parsing
  - `diagnose` Disables abort-on-error so that the full script is parsed.  Use in conjunction with other options for deeper log messages
  - `profile` Use timers to profile JIT parsing, also required for run-time profiling.
- More precise debugging with the JIT engine is possible by setting the `jitOptions` field on `fluid` objects.  Example:

```lua
  local script = obj.new('fluid', { statement = [[
  function test()
     return 42
  end
  ]],
  jitOptions = 'dump-bytecode,diagnose'
  })
  script.acActivate()
```

## Miscellaneous Gotchas
- Check that any new compile-time constants or flags (e.g. `#define`s) do
  not collide with upstream naming; we will eventually rebase to newer
  LuaJIT drops.
- Generated build outputs under `build/agents/` can be removed safely; do not
  store investigation artefacts there long-term.
- Keep an eye on Fluid tests after modifying LuaJIT semantics—failures often
  surface as subtle script regressions rather than outright crashes.

## VM Assembly and buildvm Dependencies

**Critical Build Dependency**: The `lj_obj.h` file contains the `MMDEF` macro which defines the metamethod table. When modifying `MMDEF` (e.g., adding new metamethods like `__close`), both `buildvm.exe` and `lj_vm.obj` must be regenerated.

**Why This Matters:**
- `buildvm` generates `lj_vm.obj` with hardcoded metamethod offsets derived from `MMDEF`
- If `lj_obj.h` changes but `lj_vm.obj` is not regenerated, the VM assembly will have stale offsets
- This causes cryptic runtime failures like `PANIC: unprotected error in call to Lua API ()` affecting *all* Fluid scripts

**Adding New Metamethods:**

When adding entries to `MMDEF` in `lj_obj.h`:
1. **Position matters**: The `MMDEF` macro generates an enum (`MM_*`) with sequential values. The first 6-8 metamethods are "fast" (negative cached). New metamethods should typically be added at the end, after `_(tostring)`, to avoid shifting existing metamethod indices.
2. **Force rebuild**: After modifying `MMDEF`, delete the generated VM files to force regeneration:
   ```bash
   rm build/agents/src/fluid/luajit-generated/buildvm.exe
   rm build/agents/src/fluid/luajit-generated/lj_vm.obj
   ```
3. **Full rebuild**: Run `cmake --build build/agents --config <BuildType> --parallel` to regenerate and rebuild.

**CMake Dependency**: The CMakeLists.txt includes `lj_obj.h` in the `DEPENDS` clause for both buildvm compilation and VM generation commands. This ensures automatic regeneration when `lj_obj.h` changes.

---

## Quick Reference

**File Locations:**
- Parser source: `src/fluid/luajit-2.1/src/parser/` (see [`src/parser/AGENTS.md`](src/parser/AGENTS.md) for parser-specific details)
- Bytecode reference: `src/fluid/luajit-2.1/BYTECODE.md` (instruction matrix, control-flow semantics)
- Fluid tests: `src/fluid/tests/`
