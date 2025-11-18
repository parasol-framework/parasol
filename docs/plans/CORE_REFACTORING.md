# Core Module C++20 Refactoring Opportunities

## Executive Summary
- Reviewed `src/core` for legacy patterns that hinder clarity, safety, or performance when targeting modern C++20.
- Identified five refactoring themes that could deliver the largest maintainability win relative to implementation cost.
- Each theme below documents the current pain point, risks, and a suggested C++20-aligned direction to guide future stories.

## Opportunities

### 1. Modernise manual memory management utilities
The central allocator in `lib_memory.cpp` still orchestrates raw `malloc`/`mmap` calls, manipulates headers via pointer arithmetic, and exposes untyped `APTR` handles. This code predates RAII and polymorphic memory resources, forcing every caller to reason about ownership manually and duplicating platform branches, barrier cookies, and ID tracking logic across the file.【F:src/core/lib_memory.cpp†L32-L214】

**Refactoring direction**
- Introduce a `MemoryBlock` RAII wrapper that owns the header bookkeeping and ensures `munmap` / `_aligned_free` is paired with the right allocation path; return it as `std::unique_ptr<MemoryBlock, MemoryDeleter>` so user code no longer needs to call `FreeResource` directly.
- Replace bespoke cookie math with strongly typed `std::byte` buffers or `std::span<std::byte>` that can encode headers safely while preserving 64-byte alignment guarantees.
- Offer polymorphic allocators (`std::pmr::memory_resource`) for MEM::MANAGED blocks so subsystems can share arenas without reimplementing the flag matrix, and collapse the duplicated UNIX/Windows branches behind small strategy objects.

### 2. Rework async action thread tracking
Async actions are tracked in a global `std::set<std::shared_ptr<std::jthread>>`, guarded by a recursive mutex, and detached immediately. Each task manually installs a lambda to erase itself, defeats the RAII guarantees of `std::jthread`, and requires polling loops and `std::this_thread::sleep_for` during shutdown.【F:src/core/lib_objects.cpp†L30-L638】

**Refactoring direction**
- Store concrete `std::jthread` instances in a `std::vector`/`std::pmr::vector` and let their destructors join automatically instead of calling `detach()` and building a manual cleanup lambda.
- Replace the bespoke stop-wait loop with `std::stop_source` fan-out: keep one `std::stop_source` per action and propagate it directly to the thread functor; shutdown can simply request stop and rely on `jthread` destruction to join without polling.
- Collapse the recursive mutex usage with `std::scoped_lock` over `glmAsyncActions` + `std::shared_mutex` or a `std::pmr::synchronized_pool_resource` if per-thread allocations remain, improving determinism under contention.

### 3. Adopt `std::filesystem` for directory enumeration
`fs_folders.cpp` builds directory handles by allocating a bespoke buffer layout (`[DirInfo][FileInfo][Driver][Name][Path]`) using `AllocMemory`, then copies C strings and tracks offsets manually. `ScanDir` subsequently manipulates char arrays and counts to synthesise volume entries. This pattern is brittle and ignores the C++17+ `std::filesystem` facilities already available in the toolchain.【F:src/core/fs_folders.cpp†L59-L200】

**Refactoring direction**
- Model `DirInfo` ownership with `std::unique_ptr` or a small struct containing `std::filesystem::directory_iterator` / `recursive_directory_iterator`, removing the need for MEM::MANAGED buffers and C-style copying.
- Represent paths via `std::filesystem::path` and `std::u8string` rather than raw `char*`, and expose file metadata through `std::filesystem::directory_entry` to eliminate duplicated `stat` logic and the manual RDF flag bookkeeping.
- Use `std::generator`/coroutines (or a simple range adaptor) to provide lazy iteration over directory entries, allowing call sites to use range-for rather than `while (!ScanDir(...))` loops.

### 4. Refresh the logging stack with type-safe formatting
`lib_log.cpp` still relies on `va_list` + `vfprintf`, `fprintf`, and an `ESC_OUTPUT` macro to render messages. The approach provides no compile-time format checking, cannot take advantage of `std::source_location`, and spreads platform-specific escape sequences throughout the function. The branching on `glLogLevel` performs manual mask checks each time, limiting optimisation.【F:src/core/lib_log.cpp†L26-L220】

**Refactoring direction**
- Introduce a `LogRecord` struct carrying `std::string_view` message templates, severity, and `std::source_location`; render using `std::format` / `std::vformat` to gain compile-time format validation and locale awareness.
- Replace the `ESC_OUTPUT` macro conditionals with a policy object (`struct TerminalSink`) that selects colourisation per platform and can be swapped for sinks like `std::ostream`, syslog, or Android’s logcat via concepts.
- Use `std::span<LogSink>` and `std::atomic<int>` log levels so adjusting `tlBaseLine` becomes a lock-free, thread-safe operation, enabling per-thread log filtering without the current recursive mutex.

### 5. Consolidate global configuration/state initialisation
`data.cpp` defines dozens of globals (`glRootPath`, `glPrivateIDCounter`, `glAsyncThreads`, etc.) as mutable free variables seeded via macros. Many of these can be `constexpr`, `constinit`, or wrapped in dedicated singletons, but today they default to `0`/empty strings and rely on runtime initialisers scattered across the module. This complicates testing and hinders deterministic startup.【F:src/core/data.cpp†L1-L97】【F:src/core/data.cpp†L98-L170】

**Refactoring direction**
- Replace `_ROOT_PATH` style macros with `constinit std::filesystem::path` defaults plus environment/registry overrides supplied through a configuration provider injected at startup.
- Group related global counters (`glPrivateIDCounter`, `glMessageIDCount`, etc.) into a `struct CoreIds` managed by `std::atomic_ref` to clarify ownership and enable unit tests to reset state via RAII.
- Transition unordered maps/sets such as `glPrivateMemory` and `glAsyncThreads` to `std::pmr` containers whose allocator lifetime matches the core; this enables deterministic teardown and eases fuzzing where re-initialisation happens frequently.

## Next Steps
1. Prioritise the allocator and async-thread refactors—they unlock RAII adoption for most dependent subsystems.
2. Tackle filesystem modernisation next; it naturally feeds into documentation and scripting examples that expect UTF-8 safe paths.
3. Finish by reworking logging/global state, as those changes ripple across the entire engine and benefit from the stability gained by earlier work.

## Status Update
- ✅ Opportunity 4 delivered: logging now routes through type-safe `std::format` helpers, a `LogRecord` pipeline, atomic log level controls, and a terminal sink policy so critical output no longer depends on raw `va_list` formatting or `ESC_OUTPUT` macros.
- ✅ Opportunity 5 delivered: global path seeds and ID counters now reset via dedicated `CorePathConfig`/`CoreIdConfig` helpers so tests or embedders can snapshot and restore deterministic startup state directly from `data.cpp`.
