// Lua parser - Type definitions and structures.
//
// Copyright (C) 2025 Paul Manias
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
// Major portions taken verbatim or adapted from the Lua interpreter.
// Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h

#pragma once

#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <concepts>
#include <type_traits>
#include <ankerl/unordered_dense.h>
#include <variant>
#include <ranges>

// Expression kinds.

enum class ExpKind : uint8_t {
   // Constant expressions must be first and in this order:
   Nil,
   False,
   True,
   Str,        // sval = string value
   Num,        // nval = number value
   Last = Num,
   CData,      // nval = cdata value, not treated as a constant expression
   // Non-constant expressions follow:
   Local,      // info = local register, aux = vstack index
   Upval,      // info = upvalue index, aux = vstack index
   Global,     // sval = string value (explicit global or known global reference)
   Unscoped,   // sval = string value (undeclared variable - scope determined by context)
   Indexed,    // info = table register, aux = index reg/byte/string const
   IndexedArray, // info = array register, aux = index reg/byte (array indexing)
   SafeIndexedArray, // info = array register, aux = index reg/byte (safe array indexing - nil for out-of-bounds)
   Jmp,        // info = instruction PC
   Relocable,  // info = instruction PC
   NonReloc,   // info = result register
   Call,       // info = instruction PC, aux = base
   Void
};

// Expression kind helper function - returns true for variable-like expressions.
// Note: Unscoped is between Global and Indexed, so this range check covers it.
// IndexedArray and SafeIndexedArray are also considered variable-like expressions for assignment purposes.
[[nodiscard]] static constexpr bool vkisvar(ExpKind k) {
   return ExpKind::Local <= k and k <= ExpKind::SafeIndexedArray;
}

enum class ExprFlag : uint8_t {
   None = 0x00u,
   PostfixIncStmt = 0x01u,
   HasRhsReg = 0x02u,
   BitwiseBase = 0x04u  // aux contains base register for bitwise call frame
};

enum class FuncScopeFlag : uint8_t {
   None = 0x00u,
   Loop = 0x01u,
   Break = 0x02u,
   Upvalue = 0x08u,
   NoClose = 0x10u,
   Continue = 0x20u
};

enum class VarInfoFlag : uint8_t {
   None = 0x00u,
   VarReadWrite = 0x01u,
   Jump = 0x02u,
   JumpTarget = 0x04u,
   Defer = 0x08u,
   DeferArg = 0x10u,
   Close = 0x20u
};

// Concept for flag types that support bitwise operations
template<typename Flag>
concept FlagType = std::same_as<Flag, ExprFlag> or
                   std::same_as<Flag, FuncScopeFlag> or
                   std::same_as<Flag, VarInfoFlag>;

template<FlagType Flag> [[nodiscard]] static constexpr Flag operator|(Flag Left, Flag Right) {
   using Underlying = std::underlying_type_t<Flag>;
   return Flag(Underlying(Left) | Underlying(Right));
}

template<FlagType Flag> [[nodiscard]] static constexpr Flag operator&(Flag Left, Flag Right) {
   using Underlying = std::underlying_type_t<Flag>;
   return Flag(Underlying(Left) & Underlying(Right));
}

template<FlagType Flag> [[nodiscard]] static constexpr Flag operator~(Flag Value) {
   using Underlying = std::underlying_type_t<Flag>;
   return Flag(~Underlying(Value));
}

template<FlagType Flag> static constexpr Flag & operator|=(Flag &Left, Flag Right) { return Left = Left | Right; }
template<FlagType Flag> static constexpr Flag & operator&=(Flag &Left, Flag Right) { return Left = Left & Right; }
template<FlagType Flag> [[nodiscard]] static constexpr bool has_flag(Flag Flags, Flag Mask) { return (Flags & Mask) != Flag::None; }

// Enhanced flag utilities for clearer flag handling
template<FlagType Flag> [[nodiscard]] static constexpr bool has_any(Flag Flags, Flag Mask) {
   return (Flags & Mask) != Flag::None;
}

template<FlagType Flag> [[nodiscard]] static constexpr bool has_all(Flag Flags, Flag Mask) {
   return (Flags & Mask) IS Mask;
}

template<FlagType Flag> static constexpr void clear_flag(Flag &Flags, Flag Mask) {
   Flags = Flags & ~Mask;
}

// Strong index types for type-safe register, position, and variable indices.
// Uses C++20 three-way comparison for automatic generation of all six comparison operators.

template<typename Tag, typename T>
struct StrongIndex {
   T value;

   constexpr StrongIndex() = default;
   constexpr explicit StrongIndex(T v) : value(v) {}
   constexpr T raw() const { return value; }

   // Implicit conversion to underlying type for ergonomic usage
   // This allows: int(bcpos), printf("%d", bcreg), bcpos >= 1, etc.
   // while still preventing implicit construction from raw types
   constexpr operator T() const { return value; }

   auto operator<=>(const StrongIndex&) const = default;
   bool operator==(const StrongIndex&) const = default;
};

// Arithmetic operators for StrongIndex types

template<typename Tag, typename T>
constexpr StrongIndex<Tag, T> operator+(StrongIndex<Tag, T> a, T offset) {
   return StrongIndex<Tag, T>(a.value + offset);
}

template<typename Tag, typename T>
constexpr StrongIndex<Tag, T> operator+(StrongIndex<Tag, T> a, StrongIndex<Tag, T> offset) {
   return StrongIndex<Tag, T>(a.value + offset.value);
}

template<typename Tag, typename T>
constexpr StrongIndex<Tag, T> operator-(StrongIndex<Tag, T> a, T offset) {
   return StrongIndex<Tag, T>(a.value - offset);
}

template<typename Tag, typename T>
constexpr T operator-(StrongIndex<Tag, T> a, StrongIndex<Tag, T> b) {
   return a.value - b.value;
}

template<typename Tag, typename T>
constexpr StrongIndex<Tag, T>& operator++(StrongIndex<Tag, T>& a) {
   ++a.value;
   return a;
}

template<typename Tag, typename T>
constexpr StrongIndex<Tag, T> operator++(StrongIndex<Tag, T>& a, int) {
   auto old = a;
   ++a.value;
   return old;
}

template<typename Tag, typename T>
constexpr StrongIndex<Tag, T>& operator--(StrongIndex<Tag, T>& a) {
   --a.value;
   return a;
}

template<typename Tag, typename T>
constexpr StrongIndex<Tag, T> operator--(StrongIndex<Tag, T>& a, int) {
   auto old = a;
   --a.value;
   return old;
}

template<typename Tag, typename T>
constexpr StrongIndex<Tag, T>& operator+=(StrongIndex<Tag, T>& a, T offset) {
   a.value += offset;
   return a;
}

template<typename Tag, typename T>
constexpr StrongIndex<Tag, T>& operator-=(StrongIndex<Tag, T>& a, T offset) {
   a.value -= offset;
   return a;
}

// Strong type aliases using distinct tag types

using BCPos = StrongIndex<struct BCPosTag, BCPOS>;
using BCReg = StrongIndex<struct BCRegTag, BCREG>;

// Expression descriptor.

struct ExpDesc {
   union {
      struct { // For non-constant expressions like Local, Upval, Global, Indexed, Jmp, Relocable, NonReloc, Call, Void
         uint32_t info;  // Primary info.
         uint32_t aux;   // Secondary info.
      } s;
      TValue nval;   // ExpKind::Num number value.
      GCstr* sval;   // ExpKind::Str string value.
   } u;
   ExpKind k;      // Expression kind.
   ExprFlag flags; // Expression flags.
   FluidType result_type = FluidType::Unknown;  // Known result type (for Call: callee's first return type)
   BCPOS t;        // True condition jump list.
   BCPOS f;        // False condition jump list.

   // Constructors
   constexpr ExpDesc() : u{}, k(ExpKind::Void), flags(ExprFlag::None), result_type(FluidType::Unknown), t(NO_JMP), f(NO_JMP) {}

   constexpr ExpDesc(ExpKind Kind, uint32_t Info = 0) : u{}, k(Kind), flags(ExprFlag::None), result_type(FluidType::Unknown), t(NO_JMP), f(NO_JMP) {
      this->u.s.info = Info;
      this->u.s.aux = 0;
   }

   explicit constexpr ExpDesc(GCstr* Value) : u{}, k(ExpKind::Str), flags(ExprFlag::None), result_type(FluidType::Str), t(NO_JMP), f(NO_JMP) {
      this->u.sval = Value;
   }

   explicit constexpr ExpDesc(lua_Number Value) : u{}, k(ExpKind::Num), flags(ExprFlag::None), result_type(FluidType::Num), t(NO_JMP), f(NO_JMP) {
      setnumV(&this->u.nval, Value);
   }

   explicit constexpr ExpDesc(bool Value) : u{}, k(Value ? ExpKind::True : ExpKind::False), flags(ExprFlag::None), result_type(FluidType::Bool), t(NO_JMP), f(NO_JMP) {
      this->u.s.info = 0;
      this->u.s.aux = 0;
   }

   // Member methods for expression queries and manipulation
   [[nodiscard]] inline bool has_jump() const { return this->t != this->f; }
   [[nodiscard]] inline bool is_constant() const { return this->k <= ExpKind::Last; }
   [[nodiscard]] inline bool is_constant_nojump() const { return this->is_constant() and not this->has_jump(); }
   [[nodiscard]] inline bool is_num_constant() const { return this->k == ExpKind::Num; }
   [[nodiscard]] inline bool is_num_constant_nojump() const { return this->is_num_constant() and not this->has_jump(); }
   [[nodiscard]] inline bool is_str_constant() const { return this->k == ExpKind::Str; }
   [[nodiscard]] inline lua_Number number_value() { return numberVnum(this->num_tv()); }
   [[nodiscard]] inline bool is_nil() const { return this->k IS ExpKind::Nil; }
   [[nodiscard]] inline bool is_false() const { return this->k IS ExpKind::False; }
   [[nodiscard]] inline bool is_true() const { return this->k IS ExpKind::True; }
   [[nodiscard]] inline bool is_string() const { return this->k IS ExpKind::Str; }
   [[nodiscard]] inline bool is_number() const { return this->k IS ExpKind::Num; }
   [[nodiscard]] inline bool is_local() const { return this->k IS ExpKind::Local; }
   [[nodiscard]] inline bool is_upvalue() const { return this->k IS ExpKind::Upval; }
   [[nodiscard]] inline bool is_global() const { return this->k IS ExpKind::Global; }
   [[nodiscard]] inline bool is_indexed() const { return this->k IS ExpKind::Indexed; }
   [[nodiscard]] inline bool is_indexed_array() const { return this->k IS ExpKind::IndexedArray; }
   [[nodiscard]] inline bool is_safe_indexed_array() const { return this->k IS ExpKind::SafeIndexedArray; }
   [[nodiscard]] inline bool is_any_indexed() const { return this->k IS ExpKind::Indexed or this->k IS ExpKind::IndexedArray or this->k IS ExpKind::SafeIndexedArray; }
   [[nodiscard]] inline bool is_register() const { return this->k IS ExpKind::Local or this->k IS ExpKind::NonReloc; }

   // Extended falsey check (nil, false, 0, "")
   // Supports Fluid's extended falsey semantics for ?? operator
   [[nodiscard]] bool is_falsey() const;

   [[nodiscard]] inline TValue* num_tv() {
      lj_assertX(this->is_num_constant(), "expr must be number constant");
      return &this->u.nval;
   }

   inline void init(ExpKind kind, uint32_t info) {
      this->k = kind;
      this->u.s.info = info;
      this->flags = ExprFlag::None;
      this->result_type = FluidType::Unknown;
      this->f = this->t = NO_JMP;
   }

   [[nodiscard]] inline bool is_num_zero() {
      TValue* o = this->num_tv();
      return tvisint(o) ? (intV(o) == 0) : tviszero(o);
   }
};

// Per-function linked list of scope blocks.
// Design: FuncScope is always stack-allocated at call sites, so parent scopes naturally outlive
// child scopes via C++ stack semantics. The raw `prev` pointer is intentional for zero-overhead
// traversal without ownership concerns. Lifecycle is managed by ScopeGuard RAII wrapper.

struct FuncScope {
   FuncScope* prev;        // Link to outer scope (non-owning, stack guarantees validity).
   MSize vstart;           // Start of block-local variables.
   uint8_t nactvar;        // Number of active vars outside the scope.
   FuncScopeFlag flags;    // Scope flags.
};

// Type-safe special variable names to replace legacy sentinel pointers.

enum class SpecialName : uint8_t { None, Break, Continue, Blank };

struct VarName {
   std::variant<SpecialName, GCstr*> value;

   constexpr VarName() : value(SpecialName::None) {}
   constexpr VarName(SpecialName special) : value(special) {}
   constexpr VarName(GCstr* str) : value(str) {}

   [[nodiscard]] constexpr bool is_special() const { return std::holds_alternative<SpecialName>(value); }
   [[nodiscard]] constexpr bool is_break() const { return is_special() and std::get<SpecialName>(value) IS SpecialName::Break; }
   [[nodiscard]] constexpr bool is_continue() const { return is_special() and std::get<SpecialName>(value) IS SpecialName::Continue; }
   [[nodiscard]] constexpr bool is_blank() const { return is_special() and std::get<SpecialName>(value) IS SpecialName::Blank; }
   [[nodiscard]] constexpr GCstr* as_string() const { return std::get<GCstr*>(value); }

   [[nodiscard]] constexpr bool operator==(const VarName& other) const = default;
   [[nodiscard]] constexpr bool operator==(GCstr* str) const {
      return not is_special() and as_string() IS str;
   }
};

// Legacy sentinel pointers for backward compatibility during transition.
inline GCstr * const NAME_BREAK    = (GCstr*)uintptr_t(1);
inline GCstr * const NAME_CONTINUE = (GCstr*)uintptr_t(2);
inline GCstr * const NAME_BLANK    = (GCstr*)uintptr_t(3);

// Index into variable stack.
typedef uint16_t VarIndex;
inline constexpr int LJ_MAX_VSTACK = (65536 - LJ_MAX_UPVAL);

// Strong type for variable slot indices (defined after VarIndex)
using VarSlot = StrongIndex<struct VarSlotTag, VarIndex>;

// Variable info flags are defined in VarInfoFlag.

//********************************************************************************************************************
// FuncState tracks all parser state for a single function being compiled:
//
// - Register allocation (freereg, nactvar, framesize)
// - Bytecode emission (pc, bcbase, bclim)
// - Jump management (jpc, lasttarget)
// - Scoping and upvalues (bl, prev, uvmap)
// - Constants (kt, nkn, nkgc)
//
// Design Notes:
// - Raw members are retained for backward compatibility during gradual migration
// - Type-safe accessors (current_pc(), free_reg(), etc.) provide BCPos/BCReg returns
// - Helper methods encapsulate common patterns (reset_freereg(), is_temp_register())
// - std::span views provide bounds-checked access to arrays
// - Debug assertions validate invariants in development builds

struct FuncState {
   GCtab *kt;          // Hash table for constants.
   LexState *ls;       // Lexer state.
   lua_State *L;       // Lua state.
   FuncScope *bl;      // Current scope.
   FuncState *prev;    // Enclosing function.
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

   // Function name for named function declarations (used for tostring() output).
   // Set before fs_finish() is called. nullptr for anonymous functions.
   GCstr* funcname = nullptr;

   // Return types for runtime type checking.  Set during function emission if explicit return types are declared.
   // FluidType::Unknown (default) means no type constraint is applied for that position.
   std::array<FluidType, MAX_RETURN_TYPES> return_types{};

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

   [[nodiscard]] inline VarInfo& var_get(int32_t Slot) {
      lj_assertX(Slot >= 0 and Slot < int32_t(varmap.size()), "variable slot out of range");
      return ls->vstack[varmap[Slot]];
   }

   [[nodiscard]] inline const VarInfo& var_get(int32_t Slot) const {
      lj_assertX(Slot >= 0 and Slot < int32_t(varmap.size()), "variable slot out of range");
      return ls->vstack[varmap[Slot]];
   }

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
   [[nodiscard]] constexpr bool is_top_level() const noexcept { return prev IS nullptr; }

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

// Parser assertion macro - expands __FILE__/__LINE__ at call site for accurate error locations.
// Usage: fs_check_assert(fs, condition, "format string", args...);
// Note: Cannot be a method because __FILE__/__LINE__ would resolve inside the method, not at call site.
#ifdef LUA_USE_ASSERT
   #define fs_check_assert(fs, c, ...) lj_assertG_(G((fs)->L), (c), __VA_ARGS__)
#else
   #define fs_check_assert(fs, c, ...) ((void)(fs))
#endif

// Binary and unary operators. ORDER OPR

enum class BinOpr : int8_t {
   Add, Sub, Mul, Div, Mod, Pow,  // ORDER ARITH
   Concat,
   NotEqual, Equal,
   LessThan, GreaterEqual, LessEqual, GreaterThan,
   BitAnd, BitOr, BitXor, ShiftLeft, ShiftRight,
   LogicalAnd, LogicalOr, IfEmpty,
   Ternary,
   None
};

// Arithmetic offset helper for bytecode generation
[[nodiscard]] constexpr int to_arith_offset(BinOpr op) {
   return int(op) - int(BinOpr::Add);
}

// Operator classification helpers (non-template versions for parse_types.h)
// Template versions with concept constraints are in parse_internal.h

[[nodiscard]] constexpr bool is_arithmetic_op(BinOpr op) {
   return op >= BinOpr::Add and op <= BinOpr::Pow;
}

[[nodiscard]] constexpr bool is_comparison_op(BinOpr op) {
   return op >= BinOpr::NotEqual and op <= BinOpr::GreaterThan;
}

[[nodiscard]] constexpr bool is_bitwise_op(BinOpr op) {
   return op >= BinOpr::BitAnd and op <= BinOpr::ShiftRight;
}

[[nodiscard]] constexpr bool is_logical_op(BinOpr op) {
   return op IS BinOpr::LogicalAnd or op IS BinOpr::LogicalOr or op IS BinOpr::IfEmpty;
}

// Verify bytecode opcodes maintain correct offsets relative to their operator counterparts.

static_assert((int)BC_ISGE - (int)BC_ISLT == int(BinOpr::GreaterEqual) - int(BinOpr::LessThan), "BC_ISGE offset mismatch");
static_assert((int)BC_ISLE - (int)BC_ISLT == int(BinOpr::LessEqual) - int(BinOpr::LessThan), "BC_ISLE offset mismatch");
static_assert((int)BC_ISGT - (int)BC_ISLT == int(BinOpr::GreaterThan) - int(BinOpr::LessThan), "BC_ISGT offset mismatch");
static_assert((int)BC_SUBVV - (int)BC_ADDVV == int(BinOpr::Sub) - int(BinOpr::Add), "BC_SUBVV offset mismatch");
static_assert((int)BC_MULVV - (int)BC_ADDVV == int(BinOpr::Mul) - int(BinOpr::Add), "BC_MULVV offset mismatch");
static_assert((int)BC_DIVVV - (int)BC_ADDVV == int(BinOpr::Div) - int(BinOpr::Add), "BC_DIVVV offset mismatch");
static_assert((int)BC_MODVV - (int)BC_ADDVV == int(BinOpr::Mod) - int(BinOpr::Add), "BC_MODVV offset mismatch");

// Return bytecode encoding for primitive constant.

[[nodiscard]] static constexpr ExpKind const_pri(const ExpDesc* e) {
   lj_assertX(e->k <= ExpKind::True, "Bad constant primitive");
   return e->k;
}

// Register manipulation helpers (non-template versions for parse_types.h)
// Template versions with concept constraints are in parse_internal.h

[[nodiscard]] constexpr bool is_valid_register(BCREG reg) {
   return reg < NO_REG;
}

[[nodiscard]] constexpr bool is_valid_jump(BCPOS pos) {
   return pos != NO_JMP;
}

[[nodiscard]] constexpr BCREG next_register(BCREG reg) {
   return reg + 1;
}

[[nodiscard]] static inline bool tvhaskslot(const TValue* o) { return o->u32.hi == 0; }
[[nodiscard]] static inline uint32_t tvkslot(const TValue* o) { return o->u32.lo; }

// Error checking functions.

[[maybe_unused]] void err_limit(FuncState *fs, uint32_t limit, CSTRING what);

inline void checklimit(FuncState *fs, MSize v, MSize l, const char *m) { if (v >= l) err_limit(fs, l, m); }
inline void checklimitgt(FuncState *fs, MSize v, MSize l, const char *m) { if (v > l) err_limit(fs, l, m); }
inline void checkcond(LexState *ls, bool c, ErrMsg em) { if (not (c)) { ls->err_syntax(em); } }
