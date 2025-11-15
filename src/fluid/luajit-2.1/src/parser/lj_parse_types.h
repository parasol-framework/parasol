/*
** Lua parser - Type definitions and structures.
** Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#ifndef _LJ_PARSE_TYPES_H
#define _LJ_PARSE_TYPES_H

#include <array>
#include <span>
#include <string_view>
#include <concepts>

// -- Parser structures and definitions -----------------------------------

// Expression kinds.
typedef enum {
   // Constant expressions must be first and in this order:
   VKNIL,
   VKFALSE,
   VKTRUE,
   VKSTR,   // sval = string value
   VKNUM,   // nval = number value
   VKLAST = VKNUM,
   VKCDATA,   // nval = cdata value, not treated as a constant expression
   // Non-constant expressions follow:
   VLOCAL,   // info = local register, aux = vstack index
   VUPVAL,   // info = upvalue index, aux = vstack index
   VGLOBAL,   // sval = string value
   VINDEXED,   // info = table register, aux = index reg/byte/string const
   VJMP,      // info = instruction PC
   VRELOCABLE,   // info = instruction PC
   VNONRELOC,   // info = result register
   VCALL,   // info = instruction PC, aux = base
   VVOID
} ExpKind;

// Expression kind helper function.
[[nodiscard]] static constexpr bool vkisvar(ExpKind k) {
   return VLOCAL <= k and k <= VINDEXED;
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
   uint8_t flags;      // Expression flags.
   BCPos t;      // True condition jump list.
   BCPos f;      // False condition jump list.
} ExpDesc;

// Flag carried in ExpDesc.flags to signal that a postfix increment formed a statement.
#define POSTFIX_INC_STMT_FLAG    0x01u

// Internal flag indicating that ExpDesc.aux stores a RHS register for OPR_IF_EMPTY.
#define EXP_HAS_RHS_REG_FLAG     0x02u

/*
** Expression helpers that previously relied on flag bits within ExpDesc.aux now
** store their metadata in ExpDesc.flags. The aux field can therefore be used
** directly for temporary payloads (e.g., register numbers) without additional
** masking.
*/

// -- C++20 Concepts for Expression Type Safety -------------------------------

/*
** ConstExpressionDescriptor: Concept for const expression descriptor types
**
** Ensures a type has the required members for expression handling.
** Provides compile-time validation and clearer error messages.
*/
template<typename T>
concept ConstExpressionDescriptor = requires(const T* e) {
   { e->k } -> std::convertible_to<ExpKind>;
   { e->t } -> std::convertible_to<BCPos>;
   { e->f } -> std::convertible_to<BCPos>;
   requires sizeof(e->u) > 0;  // Has union member
};

// Expression query functions with C++20 concept constraints.
template<ConstExpressionDescriptor E>
[[nodiscard]] static constexpr bool expr_hasjump(const E* e) {
   return e->t != e->f;
}

template<ConstExpressionDescriptor E>
[[nodiscard]] static constexpr bool expr_isk(const E* e) {
   return e->k <= VKLAST;
}

template<ConstExpressionDescriptor E>
[[nodiscard]] static constexpr bool expr_isk_nojump(const E* e) {
   return expr_isk(e) and not expr_hasjump(e);
}

template<ConstExpressionDescriptor E>
[[nodiscard]] static constexpr bool expr_isnumk(const E* e) {
   return e->k == VKNUM;
}

template<ConstExpressionDescriptor E>
[[nodiscard]] static constexpr bool expr_isnumk_nojump(const E* e) {
   return expr_isnumk(e) and not expr_hasjump(e);
}

template<ConstExpressionDescriptor E>
[[nodiscard]] static constexpr bool expr_isstrk(const E* e) {
   return e->k == VKSTR;
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
   e->flags = 0;
   e->f = e->t = NO_JMP;
}

// Check number constant for +-0.
static LJ_AINLINE int expr_numiszero(ExpDesc* e)
{
   TValue* o = expr_numtv(e);
   return tvisint(o) ? (intV(o) == 0) : tviszero(o);
}

// Per-function linked list of scope blocks.
typedef struct FuncScope {
   struct FuncScope* prev;   // Link to outer scope.
   MSize vstart;         // Start of block-local variables.
   uint8_t nactvar;      // Number of active vars outside the scope.
   uint8_t flags;      // Scope flags.
} FuncScope;

#define FSCOPE_LOOP      0x01   // Scope is a (breakable) loop.
#define FSCOPE_BREAK      0x02   // Break used in scope.
#define FSCOPE_UPVAL      0x08   // Upvalue in scope.
#define FSCOPE_NOCLOSE      0x10   // Do not close upvalues.
#define FSCOPE_CONTINUE   0x20   // Continue used in scope.

#define NAME_BREAK      ((GCstr*)uintptr_t(1))
#define NAME_CONTINUE   ((GCstr*)uintptr_t(2))
#define NAME_BLANK      ((GCstr*)uintptr_t(3))

// Index into variable stack.
typedef uint16_t VarIndex;
#define LJ_MAX_VSTACK      (65536 - LJ_MAX_UPVAL)

// Variable info.
#define VSTACK_VAR_RW      0x01   // R/W variable.
#define VSTACK_JUMP      0x02   // Pending goto (used by break/continue).
#define VSTACK_JUMP_TARGET 0x04   // Jump to (used by break/continue).
#define VSTACK_DEFER      0x08   // Deferred handler.
#define VSTACK_DEFERARG   0x10   // Deferred handler argument.

// Per-function state.
typedef struct FuncState {
   GCtab* kt;         // Hash table for constants.
   LexState* ls;         // Lexer state.
   lua_State* L;         // Lua state.
   FuncScope* bl;      // Current scope.
   struct FuncState* prev;   // Enclosing function.
   BCPos pc;         // Next bytecode position.
   BCPos lasttarget;      // Bytecode position of last jump target.
   BCPos jpc;         // Pending jump list to next bytecode.
   BCReg freereg;      // First free register.
   BCReg nactvar;      // Number of active local variables.
   BCReg nkn, nkgc;      // Number of lua_Number/GCobj constants
   BCLine linedefined;      // First line of the function definition.
   BCInsLine* bcbase;      // Base of bytecode stack.
   BCPos bclim;         // Limit of bytecode stack.
   MSize vbase;         // Base of variable stack for this function.
   uint8_t flags;      // Prototype flags.
   uint8_t numparams;      // Number of parameters.
   uint8_t framesize;      // Fixed frame size.
   uint8_t nuv;         // Number of upvalues
   std::array<VarIndex, LJ_MAX_LOCVAR> varmap;  // Map from register to variable idx.
   std::array<VarIndex, LJ_MAX_UPVAL> uvmap;   // Map from upvalue to variable idx.
   std::array<VarIndex, LJ_MAX_UPVAL> uvtmp;   // Temporary upvalue map.
} FuncState;

// Variable access macro.
[[nodiscard]] static inline VarInfo& var_get(LexState* ls, FuncState* fs, int32_t i) {
   return ls->vstack[fs->varmap[i]];
}

// Binary and unary operators. ORDER OPR
typedef enum BinOpr {
   OPR_ADD, OPR_SUB, OPR_MUL, OPR_DIV, OPR_MOD, OPR_POW,  // ORDER ARITH
   OPR_CONCAT,
   OPR_NE, OPR_EQ,
   OPR_LT, OPR_GE, OPR_LE, OPR_GT,
   OPR_BAND, OPR_BOR, OPR_BXOR, OPR_SHL, OPR_SHR,
   OPR_AND, OPR_OR, OPR_IF_EMPTY,
   OPR_TERNARY,
   OPR_NOBINOPR
} BinOpr;

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

// -- Constant and utility functions --------------------------------------

// Return bytecode encoding for primitive constant.
[[nodiscard]] static constexpr ExpKind const_pri(const ExpDesc* e) {
   lj_assertX(e->k <= VKTRUE, "bad constant primitive");
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
#define checkcond(ls, c, em)      { if (!(c)) err_syntax(ls, em); }

#endif
