/*
** Lua parser - Type definitions and structures.
** Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#ifndef _LJ_PARSE_TYPES_H
#define _LJ_PARSE_TYPES_H

#define vkisvar(k)   (VLOCAL <= (k) && (k) <= VINDEXED)

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

#define SAFE_NAV_CHAIN_FLAG      0x01u
// Flag carried in ExpDesc.flags to signal that a postfix increment formed a statement.
#define POSTFIX_INC_STMT_FLAG    0x02u
// Internal flag indicating that ExpDesc.aux stores a RHS register for OPR_IF_EMPTY.
#define EXP_HAS_RHS_REG_FLAG     0x04u

/*
** Expression helpers that previously relied on flag bits within ExpDesc.aux now
** store their metadata in ExpDesc.flags. The aux field can therefore be used
** directly for temporary payloads (e.g., register numbers) without additional
** masking.
*/

// Macros for expressions.
#define expr_hasjump(e)      ((e)->t != (e)->f)

#define expr_isk(e)      ((e)->k <= VKLAST)
#define expr_isk_nojump(e)   (expr_isk(e) && !expr_hasjump(e))
#define expr_isnumk(e)      ((e)->k == VKNUM)
#define expr_isnumk_nojump(e)   (expr_isnumk(e) && !expr_hasjump(e))
#define expr_isstrk(e)      ((e)->k == VKSTR)

#define expr_numtv(e)      check_exp(expr_isnumk((e)), &(e)->u.nval)
#define expr_numberV(e)      numberVnum(expr_numtv((e)))

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

#define NAME_BREAK      ((GCstr *)(uintptr_t)1)
#define NAME_CONTINUE   ((GCstr *)(uintptr_t)2)
#define NAME_BLANK      ((GCstr *)(uintptr_t)3)

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
   VarIndex varmap[LJ_MAX_LOCVAR];  // Map from register to variable idx.
   VarIndex uvmap[LJ_MAX_UPVAL];   // Map from upvalue to variable idx.
   VarIndex uvtmp[LJ_MAX_UPVAL];   // Temporary upvalue map.
} FuncState;

// Variable access macro.
#define var_get(ls, fs, i)   ((ls)->vstack[(fs)->varmap[(i)]])

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

// -- Constant and utility macros -----------------------------------------

// Return bytecode encoding for primitive constant.
#define const_pri(e)      check_exp((e)->k <= VKTRUE, (e)->k)

#define tvhaskslot(o)   ((o)->u32.hi == 0)
#define tvkslot(o)   ((o)->u32.lo)

// Error checking macros.
#define checklimit(fs, v, l, m)      if ((v) >= (l)) err_limit(fs, l, m)
#define checklimitgt(fs, v, l, m)   if ((v) > (l)) err_limit(fs, l, m)
#define checkcond(ls, c, em)      { if (!(c)) err_syntax(ls, em); }

#endif
