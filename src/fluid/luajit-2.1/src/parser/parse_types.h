// Lua parser - Type definitions and structures.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
//
// Major portions taken verbatim or adapted from the Lua interpreter.
// Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h

#pragma once

#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <concepts>
#include <type_traits>

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
   Global,     // sval = string value
   Indexed,    // info = table register, aux = index reg/byte/string const
   Jmp,        // info = instruction PC
   Relocable,  // info = instruction PC
   NonReloc,   // info = result register
   Call,       // info = instruction PC, aux = base
   Void
};

// Expression kind helper function.
[[nodiscard]] static constexpr bool vkisvar(ExpKind k) {
   return ExpKind::Local <= k and k <= ExpKind::Indexed;
}

enum class ExprFlag : uint8_t {
   None = 0x00u,
   PostfixIncStmt = 0x01u,
   HasRhsReg = 0x02u
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
   DeferArg = 0x10u
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
   BCPos t;        // True condition jump list.
   BCPos f;        // False condition jump list.

   // Constructors
   constexpr ExpDesc() : u{}, k(ExpKind::Void), flags(ExprFlag::None), t(NO_JMP), f(NO_JMP) {}

   constexpr ExpDesc(ExpKind Kind, uint32_t Info = 0) : u{}, k(Kind), flags(ExprFlag::None), t(NO_JMP), f(NO_JMP) {
      this->u.s.info = Info;
      this->u.s.aux = 0;
   }

   explicit constexpr ExpDesc(GCstr* Value) : u{}, k(ExpKind::Str), flags(ExprFlag::None), t(NO_JMP), f(NO_JMP) {
      this->u.sval = Value;
   }

   explicit constexpr ExpDesc(lua_Number Value) : u{}, k(ExpKind::Num), flags(ExprFlag::None), t(NO_JMP), f(NO_JMP) {
      setnumV(&this->u.nval, Value);
   }

   explicit constexpr ExpDesc(bool Value) : u{}, k(Value ? ExpKind::True : ExpKind::False), flags(ExprFlag::None), t(NO_JMP), f(NO_JMP) {
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

   [[nodiscard]] inline TValue* num_tv() {
      lj_assertX(this->is_num_constant(), "expr must be number constant");
      return &this->u.nval;
   }

   inline void init(ExpKind kind, uint32_t info) {
      this->k = kind;
      this->u.s.info = info;
      this->flags = ExprFlag::None;
      this->f = this->t = NO_JMP;
   }

   [[nodiscard]] inline bool is_num_zero() {
      TValue* o = this->num_tv();
      return tvisint(o) ? (intV(o) == 0) : tviszero(o);
   }
};

// Per-function linked list of scope blocks.

struct FuncScope {
   FuncScope* prev;        // Link to outer scope.
   MSize vstart;           // Start of block-local variables.
   uint8_t nactvar;        // Number of active vars outside the scope.
   FuncScopeFlag flags;    // Scope flags.
};

// Sentinel GCstr* values for special variable names (not actual string pointers).

inline GCstr * const NAME_BREAK    = (GCstr*)uintptr_t(1);
inline GCstr * const NAME_CONTINUE = (GCstr*)uintptr_t(2);
inline GCstr * const NAME_BLANK    = (GCstr*)uintptr_t(3);

// Index into variable stack.
typedef uint16_t VarIndex;
inline constexpr int LJ_MAX_VSTACK = (65536 - LJ_MAX_UPVAL);

// Variable info flags are defined in VarInfoFlag.

// Per-function state.
struct FuncState {
   GCtab *kt;          // Hash table for constants.
   LexState *ls;       // Lexer state.
   lua_State *L;       // Lua state.
   FuncScope *bl;      // Current scope.
   FuncState *prev;    // Enclosing function.
   BCPos pc;           // Next bytecode position.
   BCPos lasttarget;   // Bytecode position of last jump target.
   BCPos jpc;          // Pending jump list to next bytecode.
   BCReg freereg;      // First free register.
   BCReg nactvar;      // Number of active local variables.
   BCReg nkn, nkgc;    // Number of lua_Number/GCobj constants
   BCLine linedefined; // First line of the function definition.
   BCInsLine* bcbase;  // Base of bytecode stack.
   BCPos bclim;        // Limit of bytecode stack.
   MSize vbase;        // Base of variable stack for this function.
   uint8_t flags;      // Prototype flags.
   uint8_t numparams;  // Number of parameters.
   uint8_t framesize;  // Fixed frame size.
   uint8_t nuv;        // Number of upvalues
   std::array<VarIndex, LJ_MAX_LOCVAR> varmap;  // Map from register to variable idx.
   std::array<VarIndex, LJ_MAX_UPVAL> uvmap;   // Map from upvalue to variable idx.
   std::array<VarIndex, LJ_MAX_UPVAL> uvtmp;   // Temporary upvalue map.

   inline VarInfo & var_get(int32_t i) { return ls->vstack[varmap[i]]; }

#ifdef LUA_USE_ASSERT
   inline void assert(bool condition, const char *message, ...) { lj_assertG_(G(L), condition, message, ...); }
#else
   inline void assert(bool condition, const char *message, ...) { }
#endif
};

// Binary and unary operators. ORDER OPR
enum BinOpr : int {
   OPR_ADD, OPR_SUB, OPR_MUL, OPR_DIV, OPR_MOD, OPR_POW,  // ORDER ARITH
   OPR_CONCAT,
   OPR_NE, OPR_EQ,
   OPR_LT, OPR_GE, OPR_LE, OPR_GT,
   OPR_BAND, OPR_BOR, OPR_BXOR, OPR_SHL, OPR_SHR,
   OPR_AND, OPR_OR, OPR_IF_EMPTY,
   OPR_TERNARY,
   OPR_NOBINOPR
};

// Verify bytecode opcodes maintain correct offsets relative to their operator counterparts.
static_assert((int)BC_ISGE - (int)BC_ISLT == (int)OPR_GE - (int)OPR_LT, "BC_ISGE offset mismatch");
static_assert((int)BC_ISLE - (int)BC_ISLT == (int)OPR_LE - (int)OPR_LT, "BC_ISLE offset mismatch");
static_assert((int)BC_ISGT - (int)BC_ISLT == (int)OPR_GT - (int)OPR_LT, "BC_ISGT offset mismatch");
static_assert((int)BC_SUBVV - (int)BC_ADDVV == (int)OPR_SUB - (int)OPR_ADD, "BC_SUBVV offset mismatch");
static_assert((int)BC_MULVV - (int)BC_ADDVV == (int)OPR_MUL - (int)OPR_ADD, "BC_MULVV offset mismatch");
static_assert((int)BC_DIVVV - (int)BC_ADDVV == (int)OPR_DIV - (int)OPR_ADD, "BC_DIVVV offset mismatch");
static_assert((int)BC_MODVV - (int)BC_ADDVV == (int)OPR_MOD - (int)OPR_ADD, "BC_MODVV offset mismatch");

// Return bytecode encoding for primitive constant.

[[nodiscard]] static constexpr ExpKind const_pri(const ExpDesc* e) {
   lj_assertX(e->k <= ExpKind::True, "Bad constant primitive");
   return e->k;
}

[[nodiscard]] static inline bool tvhaskslot(const TValue* o) { return o->u32.hi == 0; }
[[nodiscard]] static inline uint32_t tvkslot(const TValue* o) { return o->u32.lo; }

// Error checking functions.

[[maybe_unused]] [[noreturn]] void err_limit(FuncState *fs, uint32_t limit, CSTRING what);

inline void checklimit(FuncState *fs, MSize v, MSize l, const char *m) { if (v >= l) err_limit(fs, l, m); }
inline void checklimitgt(FuncState *fs, MSize v, MSize l, const char *m) { if (v > l) err_limit(fs, l, m); }
inline void checkcond(LexState *ls, bool c, ErrMsg em) { if (not (c)) { ls->err_syntax(em); } }
