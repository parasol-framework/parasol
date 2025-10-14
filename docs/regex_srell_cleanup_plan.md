# Regex Module Dependence on SRELL

## Current Integration Points
- `regex_engine` derives from `srell::u8cregex` to compile patterns, access the `namedcaptures` table, and read compilation error codes via `ecode()`.【F:src/regex/regex.cpp†L52-L276】
- Runtime operations rely on `srell::regex_constants` for syntax and match flags, `srell::regex_replace` for replacements, and the `u8ccregex_iterator` / `u8ccmatch` pair for iteration over matches.【F:src/regex/regex.cpp†L120-L466】

These are the only SRELL touch points surfaced by `rg`, so any pruning must preserve this subset of functionality.

## Unused Interfaces in `srell.hpp`

### 1. Token iterator support
`regex_token_iterator` and all of its alias typedefs are defined in SRELL but never referenced elsewhere in the repository—the only hits are the declarations themselves.【F:src/regex/srell/srell.hpp†L10364-L10508】【9db3e7†L1-L35】 Removing this class would eliminate ~150 lines plus dozens of typedefs at the end of the file.【F:src/regex/srell/srell.hpp†L10587-L10708】

### 2. `sub_match` comparison helpers
Our module only inspects `matched`, `first`, and `length()` on sub-matches. The rich conversion and comparison helpers (string conversions, 40+ operator overloads, and stream insertion) are unused and purely provide STL compatibility sugar.【F:src/regex/srell/srell.hpp†L7904-L8260】 They can be dropped once we confirm no internal SRELL code depends on them (the library uses the raw iterators directly).

### 3. `match_results` convenience API
`match_results` exposes allocator plumbing, prefix/suffix accessors, `format()` helpers, and swap utilities that go unused by Parasol. The regex module currently needs only `size()`, `operator[]`, `position()`, `length()`, and the internal `clear_()` that the iterator calls.【F:src/regex/regex.cpp†L365-L448】【F:src/regex/srell/srell.hpp†L8345-L8684】 By refactoring `Regex::Replace` to build its output with `u8ccregex_iterator` directly, we can stop calling `srell::regex_replace`, allowing us to remove the `match_results::format` and `prefix()/suffix()` helpers along with the overload set of `regex_replace` that depends on them.【F:src/regex/regex.cpp†L303-L312】【F:src/regex/srell/srell.hpp†L10256-L10361】

### 4. Legacy alias typedefs
SRELL still exports the standard library style aliases (`regex`, `wregex`, `cregex`, `sregex`, etc.).【F:src/regex/srell/srell.hpp†L10573-L10670】 None of these aliases are consumed by Parasol (`rg` finds no uses), and pruning them would not affect the UTF-8/UTF-16/UTF-32 specialisations we intend to keep. We should retain the Unicode-oriented aliases (u8/u16/u32) per the future requirement, but the legacy char/wchar_t names can go.

## Proposed Cleanup Plan
1. **Delete token iteration support.** Remove `regex_token_iterator` and all `_token_iterator` typedefs, then rerun the regex unit tests to ensure no hidden dependency was overlooked.
2. **Trim `sub_match`.** Drop the unused conversion/comparison helpers, leaving only the storage, `length()`, and `set_()` members required by the engine.
3. **Rework replacement flow.** Reimplement `rx::Replace` using `u8ccregex_iterator` so it no longer calls `srell::regex_replace`. Once complete, excise the unused `regex_replace` overloads plus the `match_results::format/prefix/suffix` helpers they relied on.
4. **Cull legacy aliases.** Remove the plain `regex`/`wregex`/`cregex` family while preserving the UTF-aware aliases to satisfy future Unicode needs.

This staged approach lets us validate behaviour after each pruning step, steadily shrinking `srell.hpp` without risking regressions in the Regex module or future Unicode expansion.
