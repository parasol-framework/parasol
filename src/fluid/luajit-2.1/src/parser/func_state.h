#pragma once

#include "parse_types.h"
#include "lexer_types.h"

//********************************************************************************************************************
// FuncState tracks all parser state for a single function being compiled:
//
// - Register allocation (freereg, nactvar, framesize)
// - Bytecode emission (pc, bcbase, bclim)
// - Jump management (jpc, lasttarget)
// - Scoping and upvalues (bl, uvmap)
// - Constants (kt, nkn, nkgc)
//
// Design Notes:
// - Raw members are retained for backward compatibility during gradual migration
// - Type-safe accessors (current_pc(), free_reg(), etc.) provide BCPos/BCReg returns
// - Helper methods encapsulate common patterns (reset_freereg(), is_temp_register())
// - std::span views provide bounds-checked access to arrays
// - Debug assertions validate invariants in development builds

struct LexState;
struct FuncScope;
struct BCInsLine;

struct FuncState {
   GCtab *kt;          // Hash table for constants.
   LexState *ls;       // Lexer state.
   lua_State *L;       // Lua state.
   FuncScope *bl;      // Current scope.
   BCPOS pc;           // Next bytecode position.
   BCPOS lasttarget;   // Bytecode position of last jump target.
   BCPOS jpc;          // Pending jump list to next bytecode.
   BCREG freereg;      // First free register.
   BCREG nactvar;      // Number of active local variables.
   BCREG nkn, nkgc;    // Number of lua_Number/GCobj constants
   BCLine linedefined; // First line of the function definition.
   BCInsLine* bcbase;  // Base of bytecode stack.
   BCPOS bclim;        // Limit of bytecode stack.
   MSize vbase;        // Base of variable stack for this function.
   uint8_t flags;      // Prototype flags.
   uint8_t numparams;  // Number of parameters.
   uint8_t framesize;  // Fixed frame size.
   uint8_t nuv;        // Number of upvalues
   std::array<VarIndex, LJ_MAX_LOCVAR> varmap;  // Map from register to variable idx.
   std::array<VarIndex, LJ_MAX_UPVAL> uvmap;   // Map from upvalue to variable idx.
   std::array<VarIndex, LJ_MAX_UPVAL> uvtmp;   // Temporary upvalue map.

   // Track explicitly declared global names.  This prevents new unscoped variables from being interpreted as locals
   // and thus shadowing global variables.
   ankerl::unordered_dense::set<GCstr*> declared_globals;

   // Track global names declared with <const> attribute for compile-time reassignment checks.
   ankerl::unordered_dense::set<GCstr*> const_globals;

   // Function name for named function declarations (used for tostring() output).
   // Set before fs_finish() is called. nullptr for anonymous functions.
   GCstr* funcname = nullptr;

   // Return types for runtime type checking.  Set during function emission if explicit return types are declared.
   // FluidType::Unknown (default) means no type constraint is applied for that position.
   std::array<FluidType, MAX_RETURN_TYPES> return_types{};

   // Try-except metadata for bytecode-level exception handling.
   // These are populated during emit_try_except_stmt and copied to GCproto during fs_finish.
   std::vector<TryBlockDesc>   try_blocks;    // Try block descriptors
   std::vector<TryHandlerDesc> try_handlers;  // Handler descriptors
   uint8_t try_depth = 0;  // Current try nesting depth for break/continue cleanup
   bool is_root = false;   // True if this is the top-level (root) function

   // Return strong types for bytecode positions and registers

   [[nodiscard]] constexpr BCPos current_pc() const noexcept { return BCPos(pc); }
   [[nodiscard]] constexpr BCPos last_target() const noexcept { return BCPos(lasttarget); }
   [[nodiscard]] constexpr BCPos pending_jmp() const noexcept { return BCPos(jpc); }
   [[nodiscard]] constexpr BCPos bytecode_limit() const noexcept { return BCPos(bclim); }

   [[nodiscard]] constexpr BCReg free_reg() const noexcept { return BCReg(freereg); }
   [[nodiscard]] constexpr BCReg active_var_count() const noexcept { return BCReg(nactvar); }
   [[nodiscard]] constexpr BCReg frame_size() const noexcept { return BCReg(framesize); }

   // Reset free register to the first register after local variables.
   // Common pattern: fs->freereg = fs->nactvar
   constexpr void reset_freereg() noexcept { freereg = nactvar; }

   // Ensure freereg is at least at nactvar level.
   // Common pattern: if (fs->freereg < fs->nactvar) fs->freereg = fs->nactvar;
   constexpr void ensure_freereg_at_locals() noexcept { if (freereg < nactvar) freereg = nactvar; }

   // Check if a register is a temporary (above local variables).
   [[nodiscard]] constexpr bool is_temp_register(BCReg Reg) const noexcept { return Reg.raw() >= nactvar; }

   // Check if a register is a local variable slot.
   [[nodiscard]] constexpr bool is_local_register(BCReg Reg) const noexcept { return Reg.raw() < nactvar; }

   // Check if a register is at the top of the stack (can be freed).
   [[nodiscard]] constexpr bool is_stack_top(BCReg Reg) const noexcept { return Reg.raw() + 1 IS freereg; }

   // Get the next available register without allocating it.
   [[nodiscard]] constexpr BCReg next_free() const noexcept { return BCReg(freereg); }

   // Check if there are pending jumps to patch.
   [[nodiscard]] constexpr bool has_pending_jumps() const noexcept { return jpc != NO_JMP; }

   // Clear pending jump list.
   constexpr void clear_pending_jumps() noexcept { jpc = NO_JMP; }

   // --- Bytecode Access ---

   // Get bytecode instruction at a position (bounds-checked in debug).
   [[nodiscard]] inline BCInsLine& bytecode_at(BCPos Pos) noexcept {
      lj_assertX(Pos.raw() < pc, "bytecode position out of range");
      return bcbase[Pos.raw()];
   }

   [[nodiscard]] inline const BCInsLine& bytecode_at(BCPos Pos) const noexcept {
      lj_assertX(Pos.raw() < pc, "bytecode position out of range");
      return bcbase[Pos.raw()];
   }

   // Get the last emitted instruction (common pattern).
   [[nodiscard]] inline BCInsLine& last_instruction() noexcept {
      lj_assertX(pc > 0, "no instructions emitted");
      return bcbase[pc - 1];
   }

   [[nodiscard]] inline const BCInsLine& last_instruction() const noexcept {
      lj_assertX(pc > 0, "no instructions emitted");
      return bcbase[pc - 1];
   }

   // Get a span view of the bytecode up to current pc.

   [[nodiscard]] inline std::span<BCInsLine> bytecode_span() noexcept { return std::span<BCInsLine>(bcbase, pc); }
   [[nodiscard]] inline std::span<const BCInsLine> bytecode_span() const noexcept { return std::span<const BCInsLine>(bcbase, pc); }

   // Get a span view of active upvalue mappings.

   [[nodiscard]] inline std::span<VarIndex> upvalue_span() noexcept { return std::span<VarIndex>(uvmap.data(), nuv); }
   [[nodiscard]] inline std::span<const VarIndex> upvalue_span() const noexcept { return std::span<const VarIndex>(uvmap.data(), nuv); }

   // Get a span view of active variable mappings.

   [[nodiscard]] inline std::span<VarIndex> varmap_span() noexcept { return std::span<VarIndex>(varmap.data(), nactvar); }
   [[nodiscard]] inline std::span<const VarIndex> varmap_span() const noexcept { return std::span<const VarIndex>(varmap.data(), nactvar); }

   // Get variable info for a local variable slot.

   [[nodiscard]] VarInfo & var_get(int32_t Slot);

   [[nodiscard]] const VarInfo& var_get(int32_t Slot) const;

   // Get variable info using typed register index.
   [[nodiscard]] inline VarInfo& var_at(BCReg Reg) {
      return var_get(int32_t(Reg.raw()));
   }

   [[nodiscard]] inline const VarInfo& var_at(BCReg Reg) const {
      return var_get(int32_t(Reg.raw()));
   }

   // --- Constant Counts ---

   [[nodiscard]] constexpr BCReg num_constants() const noexcept { return BCReg(nkn); }
   [[nodiscard]] constexpr BCReg gc_constants() const noexcept { return BCReg(nkgc); }

   // Check if we're at the top-level function (no enclosing function).
   [[nodiscard]] constexpr bool is_top_level() const noexcept { return is_root; }

   // Check if we have an active scope block.
   [[nodiscard]] constexpr bool has_active_scope() const noexcept { return bl != nullptr; }

   // --- Debug Assertions ---
   // Assertions are implemented as methods that call the macro so __FILE__/__LINE__ work correctly.
   // The variadic format arguments are forwarded to allow printf-style messages.

#ifdef LUA_USE_ASSERT
   // Validate register allocation invariant: freereg >= nactvar
   inline void assert_regalloc() const {
      lj_assertG_(G(L), freereg >= nactvar, "bad register allocation: freereg < nactvar");
   }

   // Validate that freereg equals nactvar (common check after scope cleanup)
   inline void assert_freereg_at_locals() const {
      lj_assertG_(G(L), freereg IS nactvar, "bad register state: freereg != nactvar");
   }
#else
   constexpr void assert_regalloc() const noexcept { }
   constexpr void assert_freereg_at_locals() const noexcept { }
#endif
};


[[maybe_unused]] void err_limit(FuncState *fs, uint32_t limit, CSTRING what);

inline void checklimit(FuncState *fs, MSize v, MSize l, const char *m) { if (v >= l) err_limit(fs, l, m); }
inline void checklimitgt(FuncState *fs, MSize v, MSize l, const char *m) { if (v > l) err_limit(fs, l, m); }
