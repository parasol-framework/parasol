// Bytecode instruction format.
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h

#pragma once

#include "lj_def.h"
#include "lj_arch.h"

// Bytecode instruction format, 32 bit wide, fields of 8 or 16 bit:
//
// +----+----+----+----+
// | B  | C  | A  | OP | Format ABC
// +----+----+----+----+
// |    D    | A  | OP | Format AD
// +--------------------
// MSB               LSB
//
// In-memory instructions are always stored in host byte order.

// Operand ranges and related constants.

constexpr uint8_t  BCMAX_A = 0xff;
constexpr uint8_t  BCMAX_B = 0xff;
constexpr uint8_t  BCMAX_C = 0xff;
constexpr uint16_t BCMAX_D = 0xffff;
constexpr uint16_t BCBIAS_J = 0x8000;
constexpr uint8_t  NO_REG = BCMAX_A;
#define NO_JMP      (~(BCPOS)0)

// Inline functions to get instruction fields (defined after BCOp enum).

#define bc_op(i)   ((BCOp)((i)&0xff))
#define bc_a(i)    ((BCREG)(((i)>>8)&0xff))
#define bc_b(i)    ((BCREG)((i)>>24))
#define bc_c(i)    ((BCREG)(((i)>>16)&0xff))
#define bc_d(i)    ((BCREG)((i)>>16))
#define bc_j(i)    ((ptrdiff_t)bc_d(i)-BCBIAS_J)

// Macros to set instruction fields.

#define setbc_byte(p, x, ofs) ((uint8_t *)(p))[LJ_ENDIAN_SELECT(ofs, 3-ofs)] = (uint8_t)(x)
#define setbc_op(p, x)  setbc_byte(p, (x), 0)
#define setbc_a(p, x)   setbc_byte(p, (x), 1)
#define setbc_b(p, x)   setbc_byte(p, (x), 3)
#define setbc_c(p, x)   setbc_byte(p, (x), 2)
#define setbc_d(p, x)   ((uint16_t *)(p))[LJ_ENDIAN_SELECT(1, 0)] = (uint16_t)(x)
#define setbc_j(p, x)   setbc_d(p, (BCPOS)((int32_t)(x)+BCBIAS_J))

// Macros to compose instructions.
#define BCINS_ABC(o, a, b, c) (((BCIns)(o))|((BCIns)(a)<<8)|((BCIns)(b)<<24)|((BCIns)(c)<<16))
#define BCINS_AD(o, a, d)     (((BCIns)(o))|((BCIns)(a)<<8)|((BCIns)(d)<<16))
#define BCINS_AJ(o, a, j)     BCINS_AD(o, a, (BCPOS)((int32_t)(j)+BCBIAS_J))

/* Bytecode instruction definition. Order matters, see below.
**
** (name, filler, Amode, Bmode, Cmode or Dmode, metamethod)
**
** The opcode name suffixes specify the type for RB/RC or RD:
** V = variable slot
** S = string const
** N = number const
** P = primitive type (~itype)
** B = unsigned byte literal
** M = multiple args/results
*/
#define BCDEF(_) \
  /* Comparison ops. ORDER OPR. */ \
  _(ISLT,   var,   ___,   var,   lt) \
  _(ISGE,   var,   ___,   var,   lt) \
  _(ISLE,   var,   ___,   var,   le) \
  _(ISGT,   var,   ___,   var,   le) \
  \
  _(ISEQV,   var,   ___,   var,   eq) \
  _(ISNEV,   var,   ___,   var,   eq) \
  _(ISEQS,   var,   ___,   str,   eq) \
  _(ISNES,   var,   ___,   str,   eq) \
  _(ISEQN,   var,   ___,   num,   eq) \
  _(ISNEN,   var,   ___,   num,   eq) \
  _(ISEQP,   var,   ___,   pri,   eq) \
  _(ISNEP,   var,   ___,   pri,   eq) \
  \
  /* Unary test and copy ops. */ \
  _(ISTC,   dst,   ___,   var,   ___) \
  _(ISFC,   dst,   ___,   var,   ___) \
  _(IST,   ___,   ___,   var,   ___) \
  _(ISF,   ___,   ___,   var,   ___) \
  _(ISTYPE,   var,   ___,   lit,   ___) \
  _(ISNUM,   var,   ___,   lit,   ___) \
  _(ISEMPTYARR,   var,   ___,   ___,   ___) \
  \
  /* Unary ops. */ \
  _(MOV,   dst,   ___,   var,   ___) \
  _(NOT,   dst,   ___,   var,   ___) \
  _(UNM,   dst,   ___,   var,   unm) \
  _(LEN,   dst,   ___,   var,   len) \
  \
  /* Binary ops. ORDER OPR. VV last, POW must be next. */ \
  _(ADDVN,   dst,   var,   num,   add) \
  _(SUBVN,   dst,   var,   num,   sub) \
  _(MULVN,   dst,   var,   num,   mul) \
  _(DIVVN,   dst,   var,   num,   div) \
  _(MODVN,   dst,   var,   num,   mod) \
  \
  _(ADDNV,   dst,   var,   num,   add) \
  _(SUBNV,   dst,   var,   num,   sub) \
  _(MULNV,   dst,   var,   num,   mul) \
  _(DIVNV,   dst,   var,   num,   div) \
  _(MODNV,   dst,   var,   num,   mod) \
  \
  _(ADDVV,   dst,   var,   var,   add) \
  _(SUBVV,   dst,   var,   var,   sub) \
  _(MULVV,   dst,   var,   var,   mul) \
  _(DIVVV,   dst,   var,   var,   div) \
  _(MODVV,   dst,   var,   var,   mod) \
  \
  _(POW,   dst,   var,   var,   pow) \
  _(CAT,   dst,   rbase,   rbase,   concat) \
  \
  /* Constant ops. */ \
  _(KSTR,   dst,   ___,   str,   ___) \
  _(KCDATA,   dst,   ___,   cdata,   ___) \
  _(KSHORT,   dst,   ___,   lits,   ___) \
  _(KNUM,   dst,   ___,   num,   ___) \
  _(KPRI,   dst,   ___,   pri,   ___) \
  _(KNIL,   base,   ___,   base,   ___) \
  \
  /* Upvalue and function ops. */ \
  _(UGET,   dst,   ___,   uv,   ___) \
  _(USETV,   uv,   ___,   var,   ___) \
  _(USETS,   uv,   ___,   str,   ___) \
  _(USETN,   uv,   ___,   num,   ___) \
  _(USETP,   uv,   ___,   pri,   ___) \
  _(UCLO,   rbase,   ___,   jump,   ___) \
  _(FNEW,   dst,   ___,   func,   gc) \
  \
  /* Table ops. */ \
  _(TNEW,   dst,   ___,   lit,   gc) \
  _(TDUP,   dst,   ___,   tab,   gc) \
  _(GGET,   dst,   ___,   str,   index) \
  _(GSET,   var,   ___,   str,   newindex) \
  _(TGETV,   dst,   var,   var,   index) \
  _(TGETS,   dst,   var,   str,   index) \
  _(TGETB,   dst,   var,   lit,   index) \
  _(TGETR,   dst,   var,   var,   index) \
  _(TSETV,   var,   var,   var,   newindex) \
  _(TSETS,   var,   var,   str,   newindex) \
  _(TSETB,   var,   var,   lit,   newindex) \
  _(TSETM,   base,   ___,   num,   newindex) \
  _(TSETR,   var,   var,   var,   newindex) \
  \
  /* Array ops. */ \
  _(AGETV,   dst,   var,   var,   index) \
  _(AGETB,   dst,   var,   lit,   index) \
  _(ASETV,   var,   var,   var,   newindex) \
  _(ASETB,   var,   var,   lit,   newindex) \
  _(ASGETV,  dst,   var,   var,   index) /* Safe array get (returns nil for out-of-bounds) */ \
  _(ASGETB,  dst,   var,   lit,   index) /* Safe array get with literal index */ \
  \
  /* Calls and vararg handling. T = tail call. */ \
  _(CALLM,   base,   lit,   lit,   call) \
  _(CALL,   base,   lit,   lit,   call) \
  _(CALLMT,   base,   ___,   lit,   call) \
  _(CALLT,   base,   ___,   lit,   call) \
  _(ITERC,   base,   lit,   lit,   call) \
  _(ITERN,   base,   lit,   lit,   call) \
  _(ITERA,   base,   lit,   lit,   call) \
  _(VARG,   base,   lit,   lit,   ___) \
  _(ISNEXT,   base,   ___,   jump,   ___) \
  _(ISARR,   base,   ___,   jump,   ___) \
  \
  /* Returns. */ \
  _(RETM,   base,   ___,   lit,   ___) \
  _(RET,   rbase,   ___,   lit,   ___) \
  _(RET0,   rbase,   ___,   lit,   ___) \
  _(RET1,   rbase,   ___,   lit,   ___) \
  \
  /* Type fixing. */ \
  _(TYPEFIX,   rbase,   ___,   lit,   ___) \
  \
  /* Loops and branches. I/J = interp/JIT, I/C/L = init/call/loop. */ \
  _(FORI,   base,   ___,   jump,   ___) \
  _(JFORI,   base,   ___,   jump,   ___) \
  \
  _(FORL,   base,   ___,   jump,   ___) \
  _(IFORL,   base,   ___,   jump,   ___) \
  _(JFORL,   base,   ___,   lit,   ___) \
  \
  _(ITERL,   base,   ___,   jump,   ___) \
  _(IITERL,   base,   ___,   jump,   ___) \
  _(JITERL,   base,   ___,   lit,   ___) \
  \
  _(LOOP,   rbase,   ___,   jump,   ___) \
  _(ILOOP,   rbase,   ___,   jump,   ___) \
  _(JLOOP,   rbase,   ___,   lit,   ___) \
  \
  _(JMP,   rbase,   ___,   jump,   ___) \
  \
  /* Function headers. I/J = interp/JIT, F/V/C = fixarg/vararg/C func. */ \
  _(FUNCF,   rbase,   ___,   ___,   ___) \
  _(IFUNCF,   rbase,   ___,   ___,   ___) \
  _(JFUNCF,   rbase,   ___,   lit,   ___) \
  _(FUNCV,   rbase,   ___,   ___,   ___) \
  _(IFUNCV,   rbase,   ___,   ___,   ___) \
  _(JFUNCV,   rbase,   ___,   lit,   ___) \
  _(FUNCC,   rbase,   ___,   ___,   ___) \
  _(FUNCCW,   rbase,   ___,   ___,   ___) \
  \
  /* Exception handling. */ \
  _(TRYENTER,   base,   ___,   lit,   ___) \
  _(TRYLEAVE,   base,   ___,   ___,   ___)

// Bytecode opcode numbers.
// Explicitly enumerated for debugger visibility and easy value lookup.
typedef enum {
   // Comparison ops (0-11)
   BC_ISLT    = 0,
   BC_ISGE    = 1,
   BC_ISLE    = 2,
   BC_ISGT    = 3,
   BC_ISEQV   = 4,
   BC_ISNEV   = 5,
   BC_ISEQS   = 6,
   BC_ISNES   = 7,
   BC_ISEQN   = 8,
   BC_ISNEN   = 9,
   BC_ISEQP   = 10,
   BC_ISNEP   = 11,

   // Unary test and copy ops (12-18)
   BC_ISTC    = 12,
   BC_ISFC    = 13,
   BC_IST     = 14,
   BC_ISF     = 15,
   BC_ISTYPE  = 16,
   BC_ISNUM   = 17,
   BC_ISEMPTYARR = 18,  // Check if RA is an empty array (for ?? operator)

   // Unary ops (19-22)
   BC_MOV     = 19,
   BC_NOT     = 20,
   BC_UNM     = 21,
   BC_LEN     = 22,

   // Binary ops (23-38)
   BC_ADDVN   = 23,
   BC_SUBVN   = 24,
   BC_MULVN   = 25,
   BC_DIVVN   = 26,
   BC_MODVN   = 27,
   BC_ADDNV   = 28,
   BC_SUBNV   = 29,
   BC_MULNV   = 30,
   BC_DIVNV   = 31,
   BC_MODNV   = 32,
   BC_ADDVV   = 33,
   BC_SUBVV   = 34,
   BC_MULVV   = 35,
   BC_DIVVV   = 36,
   BC_MODVV   = 37,
   BC_POW     = 38,
   BC_CAT     = 39,

   // Constant ops (40-45)
   BC_KSTR    = 40,
   BC_KCDATA  = 41,
   BC_KSHORT  = 42,
   BC_KNUM    = 43,
   BC_KPRI    = 44,
   BC_KNIL    = 45,

   // Upvalue and function ops (46-52)
   BC_UGET   = 46,
   BC_USETV  = 47,
   BC_USETS  = 48,
   BC_USETN  = 49,
   BC_USETP  = 50,
   BC_UCLO   = 51,
   BC_FNEW   = 52,

   // Table ops (53-65)
   BC_TNEW   = 53,
   BC_TDUP   = 54,
   BC_GGET   = 55,
   BC_GSET   = 56,
   BC_TGETV  = 57,
   BC_TGETS  = 58,
   BC_TGETB  = 59,
   BC_TGETR  = 60,
   BC_TSETV  = 61,
   BC_TSETS  = 62,
   BC_TSETB  = 63,
   BC_TSETM  = 64,
   BC_TSETR  = 65,

   // Array ops (66-71)
   BC_AGETV  = 66,
   BC_AGETB  = 67,
   BC_ASETV  = 68,
   BC_ASETB  = 69,
   BC_ASGETV = 70,  // Safe array get (returns nil for out-of-bounds)
   BC_ASGETB = 71,  // Safe array get with literal index

   // Calls and vararg handling (72-81)
   BC_CALLM  = 72,
   BC_CALL   = 73,
   BC_CALLMT = 74,
   BC_CALLT  = 75,
   BC_ITERC  = 76,
   BC_ITERN  = 77,
   BC_ITERA  = 78,
   BC_VARG   = 79,
   BC_ISNEXT = 80,
   BC_ISARR  = 81,

   // Returns (82-85)
   BC_RETM   = 82,
   BC_RET    = 83,
   BC_RET0   = 84,
   BC_RET1   = 85,

   // Type fixing (86)
   BC_TYPEFIX = 86,

   // Loops and branches (87-98)
   BC_FORI   = 87,
   BC_JFORI  = 88,
   BC_FORL   = 89,
   BC_IFORL  = 90,
   BC_JFORL  = 91,
   BC_ITERL  = 92,
   BC_IITERL = 93,
   BC_JITERL = 94,
   BC_LOOP   = 95,
   BC_ILOOP  = 96,
   BC_JLOOP  = 97,
   BC_JMP    = 98,

   // Function headers (99-106)
   BC_FUNCF  = 99,
   BC_IFUNCF = 100,
   BC_JFUNCF = 101,
   BC_FUNCV  = 102,
   BC_IFUNCV = 103,
   BC_JFUNCV = 104,
   BC_FUNCC  = 105,
   BC_FUNCCW = 106,

   // Exception handling (107-108)
   BC_TRYENTER = 107,
   BC_TRYLEAVE = 108,

   BC__MAX   = 109
} BCOp;

[[nodiscard]] inline constexpr bool bc_is_func_header(BCOp Op) noexcept
{
   return Op >= BC_FUNCF and Op <= BC_FUNCCW;
}

static_assert((int)BC_ISEQV + 1 == (int)BC_ISNEV);
static_assert(((int)BC_ISEQV ^ 1) == (int)BC_ISNEV);
static_assert(((int)BC_ISEQS ^ 1) == (int)BC_ISNES);
static_assert(((int)BC_ISEQN ^ 1) == (int)BC_ISNEN);
static_assert(((int)BC_ISEQP ^ 1) == (int)BC_ISNEP);
static_assert(((int)BC_ISLT ^ 1) == (int)BC_ISGE);
static_assert(((int)BC_ISLE ^ 1) == (int)BC_ISGT);
static_assert(((int)BC_ISLT ^ 3) == (int)BC_ISGT);
static_assert((int)BC_IST - (int)BC_ISTC == (int)BC_ISF - (int)BC_ISFC);
static_assert((int)BC_CALLT - (int)BC_CALL == (int)BC_CALLMT - (int)BC_CALLM);
static_assert((int)BC_CALLMT + 1 == (int)BC_CALLT);
static_assert((int)BC_RETM + 1 == (int)BC_RET);
static_assert((int)BC_FORL + 1 == (int)BC_IFORL);
static_assert((int)BC_FORL + 2 == (int)BC_JFORL);
static_assert((int)BC_ITERL + 1 == (int)BC_IITERL);
static_assert((int)BC_ITERL + 2 == (int)BC_JITERL);
static_assert((int)BC_LOOP + 1 == (int)BC_ILOOP);
static_assert((int)BC_LOOP + 2 == (int)BC_JLOOP);
static_assert((int)BC_FUNCF + 1 == (int)BC_IFUNCF);
static_assert((int)BC_FUNCF + 2 == (int)BC_JFUNCF);
static_assert((int)BC_FUNCV + 1 == (int)BC_IFUNCV);
static_assert((int)BC_FUNCV + 2 == (int)BC_JFUNCV);

// This solves a circular dependency problem, change as needed.
#define FF_next_N   4

// Stack slots used by FORI/FORL, relative to operand A.
enum {
   FORL_IDX, FORL_STOP, FORL_STEP, FORL_EXT
};

// Bytecode operand modes. ORDER BCMode
typedef enum {
   BCMnone, BCMdst, BCMbase, BCMvar, BCMrbase, BCMuv,  //  Mode A must be <= 7
   BCMlit, BCMlits, BCMpri, BCMnum, BCMstr, BCMtab, BCMfunc, BCMjump, BCMcdata,
   BCM_max
} BCMode;
#define BCM___      BCMnone

#define bcmode_a(op)   ((BCMode)(lj_bc_mode[op] & 7))
#define bcmode_b(op)   ((BCMode)((lj_bc_mode[op]>>3) & 15))
#define bcmode_c(op)   ((BCMode)((lj_bc_mode[op]>>7) & 15))
#define bcmode_d(op)   bcmode_c(op)
#define bcmode_hasd(op)   ((lj_bc_mode[op] & (15<<3)) == (BCMnone<<3))
#define bcmode_mm(op)   ((MMS)(lj_bc_mode[op]>>11))

#define BCMODE(name, ma, mb, mc, mm) \
  (BCM##ma|(BCM##mb<<3)|(BCM##mc<<7)|(MM_##mm<<11)),
#define BCMODE_FF   0

static LJ_AINLINE int bc_isret(BCOp op)
{
   return (op == BC_RETM || op == BC_RET || op == BC_RET0 || op == BC_RET1);
}

LJ_DATA const uint16_t lj_bc_mode[];
LJ_DATA const uint16_t lj_bc_ofs[];
