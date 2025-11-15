/*
** Lua parser (source code -> bytecode).
** Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lj_parse_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_buf.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_func.h"
#include "lj_state.h"
#include "lj_bc.h"
#if LJ_HASFFI
#include "lj_ctype.h"
#endif
#include "lj_strfmt.h"
#include "lj_lex.h"
#include "lj_parse.h"
#include "lj_vm.h"
#include "lj_vmevent.h"

// Priorities for each binary operator. ORDER OPR.

static const struct {
   uint8_t left;      // Left priority.
   uint8_t right;   // Right priority.
   const char* name;   // Name for bitlib function (if applicable).
   uint8_t name_len;   // Cached name length for bitlib lookups.
} priority[] = {
  {6,6,nullptr,0}, {6,6,nullptr,0}, {7,7,nullptr,0}, {7,7,nullptr,0}, {7,7,nullptr,0},   // ADD SUB MUL DIV MOD
  {10,9,nullptr,0}, {5,4,nullptr,0},                  // POW CONCAT (right associative)
  {3,3,nullptr,0}, {3,3,nullptr,0},                  // EQ NE
  {3,3,nullptr,0}, {3,3,nullptr,0}, {3,3,nullptr,0}, {3,3,nullptr,0},      // LT GE GT LE
  {5,4,"band",4}, {3,2,"bor",3}, {4,3,"bxor",4}, {7,5,"lshift",6}, {7,5,"rshift",6},   // BAND BOR BXOR SHL SHR (C-style precedence: XOR binds tighter than OR)
  {2,2,nullptr,0}, {1,1,nullptr,0}, {1,1,nullptr,0},         // AND OR IF_EMPTY
  {1,1,nullptr,0}                     // TERNARY
};

// Include modular parser components
#include "parser/lj_parse_types.h"
#include "parser/lj_parse_core.cpp"
#include "parser/lj_parse_constants.cpp"
#include "parser/lj_parse_scope.cpp"
#include "parser/lj_parse_regalloc.cpp"
#include "parser/lj_parse_expr.cpp"
#include "parser/lj_parse_operators.cpp"
#include "parser/lj_parse_stmt.cpp"

// -- Parse statements ----------------------------------------------------

// Parse a statement. Returns 1 if it must be the last one in a chunk.
static int parse_stmt(LexState* ls)
{
   BCLine line = ls->linenumber;
   switch (ls->tok) {
   case TK_if:
      parse_if(ls, line);
      break;
   case TK_while:
      parse_while(ls, line);
      break;
   case TK_do:
      lj_lex_next(ls);
      parse_block(ls);
      lex_match(ls, TK_end, TK_do, line);
      break;
   case TK_for:
      parse_for(ls, line);
      break;
   case TK_repeat:
      parse_repeat(ls, line);
      break;
   case TK_function:
      parse_func(ls, line);
      break;
   case TK_defer:
      parse_defer(ls);
      break;
   case TK_local:
      lj_lex_next(ls);
      parse_local(ls);
      break;
   case TK_return:
      parse_return(ls);
      return 1;  // Must be last.
   case TK_continue:
      lj_lex_next(ls);
      parse_continue(ls);
      break;  // Must be last in Lua 5.1.
   case TK_break:
      lj_lex_next(ls);
      parse_break(ls);
      return !LJ_52;  // Must be last in Lua 5.1.
#if LJ_52
   case ';':
      lj_lex_next(ls);
      break;
#endif
   default:
      parse_call_assign(ls);
      break;
   }
   return 0;
}

// A chunk is a list of statements optionally separated by semicolons.
static void parse_chunk(LexState* ls)
{
   int islast = 0;
   synlevel_begin(ls);
   while (!islast and !parse_isend(ls->tok)) {
      islast = parse_stmt(ls);
      lex_opt(ls, ';');
      lj_assertLS(ls->fs->framesize >= ls->fs->freereg &&
         ls->fs->freereg >= ls->fs->nactvar,
         "bad regalloc");
      ls->fs->freereg = ls->fs->nactvar;  // Free registers after each stmt.
   }
   synlevel_end(ls);
}

// Entry point of bytecode parser.
GCproto* lj_parse(LexState* ls)
{
   FuncState fs;
   FuncScope bl;
   GCproto* pt;
   lua_State* L = ls->L;
#ifdef LUAJIT_DISABLE_DEBUGINFO
   ls->chunkname = lj_str_newlit(L, "=");
#else
   ls->chunkname = lj_str_newz(L, ls->chunkarg);
#endif
   setstrV(L, L->top, ls->chunkname);  // Anchor chunkname string.
   incr_top(L);
   ls->level = 0;
   fs_init(ls, &fs);
   fs.linedefined = 0;
   fs.numparams = 0;
   fs.bcbase = NULL;
   fs.bclim = 0;
   fs.flags |= PROTO_VARARG;  // Main chunk is always a vararg func.
   fscope_begin(&fs, &bl, 0);
   bcemit_AD(&fs, BC_FUNCV, 0, 0);  // Placeholder.
   lj_lex_next(ls);  // Read-ahead first token.
   parse_chunk(ls);
   if (ls->tok != TK_eof)
      err_token(ls, TK_eof);
   pt = fs_finish(ls, ls->linenumber);
   L->top--;  // Drop chunkname.
   lj_assertL(fs.prev == NULL and ls->fs == NULL, "mismatched frame nesting");
   lj_assertL(pt->sizeuv == 0, "toplevel proto has upvalues");
   return pt;
}
