// Lua parser (source code -> bytecode).
// Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
//
// Major portions taken verbatim or adapted from the Lua interpreter.
// Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h

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
   uint8_t right;     // Right priority.
   const char* name;  // Name for bitlib function (if applicable).
   uint8_t name_len;  // Cached name length for bitlib lookups.
} priority[] = {
  {6,6,nullptr,0}, {6,6,nullptr,0}, {7,7,nullptr,0}, {7,7,nullptr,0}, {7,7,nullptr,0},   // ADD SUB MUL DIV MOD
  {10,9,nullptr,0}, {5,4,nullptr,0},                  // POW CONCAT (right associative)
  {3,3,nullptr,0}, {3,3,nullptr,0},                  // EQ NE
  {3,3,nullptr,0}, {3,3,nullptr,0}, {3,3,nullptr,0}, {3,3,nullptr,0},      // LT GE GT LE
  {5,4,"band",4}, {3,2,"bor",3}, {4,3,"bxor",4}, {7,5,"lshift",6}, {7,5,"rshift",6},   // BAND BOR BXOR SHL SHR (C-style precedence: XOR binds tighter than OR)
  {2,2,nullptr,0}, {1,1,nullptr,0}, {1,1,nullptr,0},         // AND OR IF_EMPTY
  {1,1,nullptr,0}                     // TERNARY
};

#include "parser/parse_types.h"
#include "parser/parse_internal.h"
#include "parser/parse_core.cpp"
#include "parser/parse_constants.cpp"
#include "parser/parse_scope.cpp"
#include "parser/parse_regalloc.cpp"
#include "parser/parse_expr.cpp"
#include "parser/parse_operators.cpp"
#include "parser/parse_stmt.cpp"

// Parse a statement. Returns 1 if it must be the last one in a chunk.

static int parse_stmt(LexState *State)
{
   BCLine line = State->linenumber;
   switch (State->tok) {
      case TK_if:       parse_if(State, line); break;
      case TK_while:    parse_while(State, line); break;
      case TK_do:       State->next(); parse_block(State); lex_match(State, TK_end, TK_do, line); break;
      case TK_for:      parse_for(State, line); break;
      case TK_repeat:   parse_repeat(State, line); break;
      case TK_function: parse_func(State, line); break;
      case TK_defer:    parse_defer(State); break;
      case TK_local:    State->next(); parse_local(State); break;
      case TK_return:   parse_return(State); return 1;  // Must be last.
      case TK_continue: State->next(); parse_continue(State); break;
      case TK_break:    State->next(); parse_break(State); break;
      case ';':         State->next(); break;
      default:          parse_call_assign(State); break;
   }
   return 0;
}

// A chunk is a list of statements optionally separated by semicolons.

static void parse_chunk(LexState *State)
{
   int is_last = 0;
   synlevel_begin(State);
   while (not is_last and not parse_is_end(State->tok)) {
      is_last = parse_stmt(State);
      lex_opt(State, ';');
      State->assert_condition(State->fs->framesize >= State->fs->freereg and State->fs->freereg >= State->fs->nactvar, "bad regalloc");
      State->fs->freereg = State->fs->nactvar;  // Free registers after each stmt.
   }
   synlevel_end(State);
}

// Entry point of bytecode parser.

GCproto * lj_parse(LexState *State)
{
   FuncState fs;
   FuncScope bl;
   GCproto* pt;
   lua_State* L = State->L;
#ifdef LUAJIT_DISABLE_DEBUGINFO
   State->chunkname = lj_str_newlit(L, "=");
#else
   State->chunkname = lj_str_newz(L, State->chunkarg);
#endif
   setstrV(L, L->top, State->chunkname);  // Anchor chunkname string.
   incr_top(L);
   State->level = 0;
   fs_init(State, &fs);
   fs.linedefined = 0;
   fs.numparams = 0;
   fs.bcbase = nullptr;
   fs.bclim = 0;
   fs.flags |= PROTO_VARARG;  // Main chunk is always a vararg func.
   fscope_begin(&fs, &bl, 0);
   bcemit_AD(&fs, BC_FUNCV, 0, 0);  // Placeholder.
   State->next();  // Read-ahead first token.
   parse_chunk(State);
   if (State->tok != TK_eof) err_token(State, TK_eof);
   pt = fs_finish(State, State->linenumber);
   L->top--;  // Drop chunkname.
   lj_assertL(fs.prev == nullptr and State->fs == nullptr, "mismatched frame nesting");
   lj_assertL(pt->sizeuv == 0, "toplevel proto has upvalues");
   return pt;
}
