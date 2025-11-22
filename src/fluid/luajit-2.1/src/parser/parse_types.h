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

template<typename Flag>
struct FlagOpsEnabled : std::false_type {};

template<>
struct FlagOpsEnabled<ExprFlag> : std::true_type {};

template<>
struct FlagOpsEnabled<FuncScopeFlag> : std::true_type {};

template<>
struct FlagOpsEnabled<VarInfoFlag> : std::true_type {};

template<typename Flag>
requires FlagOpsEnabled<Flag>::value
[[nodiscard]] static constexpr Flag operator|(Flag Left, Flag Right)
{
   return Flag((uint8_t)Left | (uint8_t)Right);
}

template<typename Flag>
requires FlagOpsEnabled<Flag>::value
[[nodiscard]] static constexpr Flag operator&(Flag Left, Flag Right)
{
   return Flag((uint8_t)Left & (uint8_t)Right);
}

template<typename Flag>
requires FlagOpsEnabled<Flag>::value
[[nodiscard]] static constexpr Flag operator~(Flag Value)
{
   return Flag(~(uint8_t)Value);
}

template<typename Flag>
requires FlagOpsEnabled<Flag>::value
static inline Flag& operator|=(Flag& Left, Flag Right)
{
   Left = Left | Right;
   return Left;
}

template<typename Flag>
requires FlagOpsEnabled<Flag>::value
static inline Flag& operator&=(Flag& Left, Flag Right)
{
   Left = Left & Right;
   return Left;
}

template<typename Flag>
requires FlagOpsEnabled<Flag>::value
[[nodiscard]] static inline bool has_flag(Flag Flags, Flag Mask)
{
   return (Flags & Mask) != Flag::None;
}

// Expression descriptor.
typedef struct ExpDesc {
   union {
      struct {
         uint32_t info;   // Primary info.
         uint32_t aux;   // Secondary info.
      } s;
      TValue nval;   // Number value.
      GCstr* sval;   // String value.
   } u;
   ExpKind k;
   ExprFlag flags;      // Expression flags.
   BCPos t;      // True condition jump list.
   BCPos f;      // False condition jump list.
} ExpDesc;

// Expression helpers that previously relied on flag bits within ExpDesc.aux now
// store their metadata in ExpDesc.flags. The aux field can therefore be used
// directly for temporary payloads (e.g., register numbers) without additional
// masking.

// TOOD: Expression query functions.
// DEPRECATED: Use ExpressionValue::has_jump() when possible. This function is retained
// for legitimate raw ExpDesc* usage where ExpressionValue wrapper is not available.

[[nodiscard]] static inline bool expr_hasjump(const ExpDesc* e) {
   return e->t != e->f;
}

[[nodiscard]] static inline bool expr_isk(const ExpDesc* e) {
   return e->k <= ExpKind::Last;
}

[[nodiscard]] static inline bool expr_isk_nojump(const ExpDesc* e) {
   return expr_isk(e) and not expr_hasjump(e);
}

[[nodiscard]] static inline bool expr_isnumk(const ExpDesc* e) {
   return e->k == ExpKind::Num;
}

[[nodiscard]] static inline bool expr_isnumk_nojump(const ExpDesc* e) {
   return expr_isnumk(e) and not expr_hasjump(e);
}

[[nodiscard]] static inline bool expr_isstrk(const ExpDesc* e) {
   return e->k == ExpKind::Str;
}

[[nodiscard]] static inline TValue* expr_numtv(ExpDesc* e) {
   lj_assertX(expr_isnumk(e), "expr must be number constant");
   return &e->u.nval;
}

[[nodiscard]] static inline lua_Number expr_numberV(ExpDesc* e) {
   return numberVnum(expr_numtv(e));
}

// Initialize expression.
static LJ_AINLINE void expr_init(ExpDesc* e, ExpKind k, uint32_t info)
{
   e->k = k;
   e->u.s.info = info;
   e->flags = ExprFlag::None;
   e->f = e->t = NO_JMP;
}

[[nodiscard]] static constexpr ExpDesc make_const_expr(ExpKind Kind, uint32_t Info = 0)
{
   ExpDesc expression{};
   expression.u.s.info = Info;
   expression.u.s.aux = 0;
   expression.k = Kind;
   expression.flags = ExprFlag::None;
   expression.t = NO_JMP;
   expression.f = NO_JMP;
   return expression;
}

[[nodiscard]] static constexpr ExpDesc make_nil_expr()
{
   return make_const_expr(ExpKind::Nil);
}

[[nodiscard]] static constexpr ExpDesc make_bool_expr(bool Value)
{
   return make_const_expr(Value ? ExpKind::True : ExpKind::False);
}

[[nodiscard]] static inline ExpDesc make_num_expr(lua_Number Value)
{
   ExpDesc expression = make_const_expr(ExpKind::Num);
   setnumV(&expression.u.nval, Value);
   return expression;
}

[[nodiscard]] static inline ExpDesc make_interned_string_expr(GCstr* Value)
{
   ExpDesc expression = make_const_expr(ExpKind::Str);
   expression.u.sval = Value;
   return expression;
}

// Check number constant for +-0.
static LJ_AINLINE int expr_numiszero(ExpDesc* e)
{
   TValue* o = expr_numtv(e);
   return tvisint(o) ? (intV(o) == 0) : tviszero(o);
}

// Per-function linked list of scope blocks.

typedef struct FuncScope {
   struct FuncScope* prev; // Link to outer scope.
   MSize vstart;           // Start of block-local variables.
   uint8_t nactvar;        // Number of active vars outside the scope.
   FuncScopeFlag flags;    // Scope flags.
} FuncScope;

#define NAME_BREAK      ((GCstr*)uintptr_t(1))
#define NAME_CONTINUE   ((GCstr*)uintptr_t(2))
#define NAME_BLANK      ((GCstr*)uintptr_t(3))

// Index into variable stack.
typedef uint16_t VarIndex;
inline constexpr int LJ_MAX_VSTACK = (65536 - LJ_MAX_UPVAL);

// Variable info flags are defined in VarInfoFlag.

// Per-function state.
typedef struct FuncState {
   GCtab* kt;          // Hash table for constants.
   LexState* ls;       // Lexer state.
   lua_State* L;       // Lua state.
   FuncScope* bl;      // Current scope.
   struct FuncState* prev; // Enclosing function.
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
} FuncState;

// Variable access macro.
[[nodiscard]] static inline VarInfo& var_get(LexState* ls, FuncState* fs, int32_t i) {
   return ls->vstack[fs->varmap[i]];
}

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

LJ_STATIC_ASSERT((int)BC_ISGE - (int)BC_ISLT == (int)OPR_GE - (int)OPR_LT);
LJ_STATIC_ASSERT((int)BC_ISLE - (int)BC_ISLT == (int)OPR_LE - (int)OPR_LT);
LJ_STATIC_ASSERT((int)BC_ISGT - (int)BC_ISLT == (int)OPR_GT - (int)OPR_LT);
LJ_STATIC_ASSERT((int)BC_SUBVV - (int)BC_ADDVV == (int)OPR_SUB - (int)OPR_ADD);
LJ_STATIC_ASSERT((int)BC_MULVV - (int)BC_ADDVV == (int)OPR_MUL - (int)OPR_ADD);
LJ_STATIC_ASSERT((int)BC_DIVVV - (int)BC_ADDVV == (int)OPR_DIV - (int)OPR_ADD);
LJ_STATIC_ASSERT((int)BC_MODVV - (int)BC_ADDVV == (int)OPR_MOD - (int)OPR_ADD);

#ifdef LUA_USE_ASSERT
#define lj_assertFS(c, ...)   (lj_assertG_(G(fs->L), (c), __VA_ARGS__))
#else
#define lj_assertFS(c, ...)   ((void)fs)
#endif

// Return bytecode encoding for primitive constant.

[[nodiscard]] static constexpr ExpKind const_pri(const ExpDesc* e) {
   lj_assertX(e->k <= ExpKind::True, "bad constant primitive");
   return e->k;
}

[[nodiscard]] static inline bool tvhaskslot(const TValue* o) {
   return o->u32.hi == 0;
}

[[nodiscard]] static inline uint32_t tvkslot(const TValue* o) {
   return o->u32.lo;
}

// Error checking macros.
#define checklimit(fs, v, l, m)      if ((v) >= (l)) err_limit(fs, l, m)
#define checklimitgt(fs, v, l, m)   if ((v) > (l)) err_limit(fs, l, m)
#define checkcond(ls, c, em)      { if (not (c)) (ls)->err_syntax(em); }
