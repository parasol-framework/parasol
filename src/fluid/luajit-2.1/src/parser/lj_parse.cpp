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

#include "parser/token_types.h"
#include "parser/token_stream.h"
#include "parser/parser_context.h"
#include "parser/parser_ast.h"
#include "parser/ir_emitter.h"
#include "parser/parse_types.h"
#include "parser/register_allocator.h"
#include "parser/expression_value.h"
#include "parser/control_flow_graph.h"
#include "parser/parse_internal.h"
#include "parser/token_types.cpp"
#include "parser/token_stream.cpp"
#include "parser/register_allocator.cpp"
#include "parser/expression_value.cpp"
#include "parser/control_flow_graph.cpp"
#include "parser/parser_context.cpp"
#include "parser/parser_ast.cpp"
#include "parser/ir_emitter.cpp"
#include "parser/parse_core.cpp"
#include "parser/parse_constants.cpp"
#include "parser/parse_scope.cpp"
#include "parser/parse_regalloc.cpp"
#include "parser/parse_expr.cpp"
#include "parser/parse_operators.cpp"
#include "parser/parse_stmt.cpp"

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
   State->fs_init(&fs);
   ParserConfig parser_config{};
   ParserContext parser_context = ParserContext::from(*State, fs, *L, parser_config);
   ParserSession root_session(parser_context, parser_config);
   State->attach_context(&parser_context);
   fs.linedefined = 0;
   fs.numparams = 0;
   fs.bcbase = nullptr;
   fs.bclim = 0;
   fs.flags |= PROTO_VARARG;  // Main chunk is always a vararg func.
   fscope_begin(&fs, &bl, FuncScopeFlag::None);
   bcemit_AD(&fs, BC_FUNCV, 0, 0);  // Placeholder.
   State->next();  // Read-ahead first token.
   State->parse_chunk();
   if (State->tok != TK_eof) State->err_token(TK_eof);
   pt = State->fs_finish(State->linenumber);
   L->top--;  // Drop chunkname.
   lj_assertL(fs.prev == nullptr and State->fs == nullptr, "mismatched frame nesting");
   lj_assertL(pt->sizeuv == 0, "toplevel proto has upvalues");
   State->attach_context(nullptr);
   return pt;
}
