/*
** Lua parser - Internal function declarations.
** Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#ifndef _LJ_PARSE_INTERNAL_H
#define _LJ_PARSE_INTERNAL_H

// -- Error handling (lj_parse_core.c) -------------------------------------

LJ_NORET LJ_NOINLINE static void err_syntax(LexState* ls, ErrMsg em);
LJ_NORET LJ_NOINLINE static void err_token(LexState* ls, LexToken tok);
LJ_NORET static void err_limit(FuncState* fs, uint32_t limit, const char* what);

// -- Lexer helpers (lj_parse_core.c) ---------------------------------------

static int lex_opt(LexState* ls, LexToken tok);
static void lex_check(LexState* ls, LexToken tok);
static void lex_match(LexState* ls, LexToken what, LexToken who, BCLine line);
static GCstr* lex_str(LexState* ls);

// -- Constants (lj_parse_constants.c) --------------------------------------

static BCReg const_num(FuncState* fs, ExpDesc* e);
static BCReg const_gc(FuncState* fs, GCobj* gc, uint32_t itype);
static BCReg const_str(FuncState* fs, ExpDesc* e);

// -- Jump list handling (lj_parse_constants.c) ----------------------------

static BCPos jmp_next(FuncState* fs, BCPos pc);
static int jmp_novalue(FuncState* fs, BCPos list);
static int jmp_patchtestreg(FuncState* fs, BCPos pc, BCReg reg);
static void jmp_dropval(FuncState* fs, BCPos list);
static void jmp_patchins(FuncState* fs, BCPos pc, BCPos dest);
static void jmp_append(FuncState* fs, BCPos* l1, BCPos l2);
static void jmp_patchval(FuncState* fs, BCPos list, BCPos vtarget,
   BCReg reg, BCPos dtarget);
static void jmp_tohere(FuncState* fs, BCPos list);
static void jmp_patch(FuncState* fs, BCPos list, BCPos target);

// -- Expression flag lifecycle management ------------------------------------

// Helper functions for managing expression flags with explicit lifecycle semantics.
// These make flag ownership and consumption more explicit and easier to audit.

// Consume a flag from an expression, clearing it and returning whether it was set.
// Use this when an operator takes ownership of a flagged value.
[[nodiscard]] static inline bool expr_consume_flag(ExpDesc* e, uint8_t flag) {
   if (e->flags & flag) {
      e->flags &= ~flag;
      return true;
   }
   return false;
}

// Check if an expression has a flag without consuming it.
[[nodiscard]] static inline bool expr_has_flag(const ExpDesc* e, uint8_t flag) {
   return (e->flags & flag) != 0;
}

// Set a flag on an expression.
static inline void expr_set_flag(ExpDesc* e, uint8_t flag) {
   e->flags |= flag;
}

// Clear a flag on an expression.
static inline void expr_clear_flag(ExpDesc* e, uint8_t flag) {
   e->flags &= ~flag;
}

// -- Register allocation (lj_parse_regalloc.c) ----------------------------

static void bcreg_bump(FuncState* fs, BCReg n);
static void bcreg_reserve(FuncState* fs, BCReg n);
static void bcreg_free(FuncState* fs, BCReg reg);
static void expr_free(FuncState* fs, ExpDesc* e);

// -- Bytecode emission (lj_parse_regalloc.c) ------------------------------

static BCPos bcemit_INS(FuncState* fs, BCIns ins);
static void expr_discharge(FuncState* fs, ExpDesc* e);
static void bcemit_nil(FuncState* fs, BCReg from, BCReg n);
static void expr_toreg_nobranch(FuncState* fs, ExpDesc* e, BCReg reg);
static void expr_toreg(FuncState* fs, ExpDesc* e, BCReg reg);
static void expr_tonextreg(FuncState* fs, ExpDesc* e);
static BCReg expr_toanyreg(FuncState* fs, ExpDesc* e);
static void expr_toval(FuncState* fs, ExpDesc* e);
static void bcemit_store(FuncState* fs, ExpDesc* var, ExpDesc* e);
static void bcemit_method(FuncState* fs, ExpDesc* e, ExpDesc* key);
static BCPos bcemit_jmp(FuncState* fs);
static void invertcond(FuncState* fs, ExpDesc* e);
static BCPos bcemit_branch(FuncState* fs, ExpDesc* e, int cond);
static void bcemit_branch_t(FuncState* fs, ExpDesc* e);
static void bcemit_branch_f(FuncState* fs, ExpDesc* e);

// -- Operators (lj_parse_operators.c) -------------------------------------

static int foldarith(BinOpr opr, ExpDesc* e1, ExpDesc* e2);
static void bcemit_arith(FuncState* fs, BinOpr opr, ExpDesc* e1, ExpDesc* e2);
static void bcemit_comp(FuncState* fs, BinOpr opr, ExpDesc* e1, ExpDesc* e2);
static void bcemit_binop_left(FuncState* fs, BinOpr op, ExpDesc* e);
static void bcemit_shift_call_at_base(FuncState* fs, const char* fname, MSize fname_len, ExpDesc* lhs, ExpDesc* rhs, BCReg base);
static void bcemit_bit_call(FuncState* fs, const char* fname, MSize fname_len, ExpDesc* lhs, ExpDesc* rhs);
static void bcemit_unary_bit_call(FuncState* fs, const char* fname, MSize fname_len, ExpDesc* arg);
static void bcemit_presence_check(FuncState* fs, ExpDesc* e);
static void bcemit_binop(FuncState* fs, BinOpr op, ExpDesc* e1, ExpDesc* e2);
static void bcemit_unop(FuncState* fs, BCOp op, ExpDesc* e);

// -- Variables and scope (lj_parse_scope.c) -------------------------------

static int is_blank_identifier(GCstr* name);
static void var_new(LexState* ls, BCReg n, GCstr* name);
static void var_add(LexState* ls, BCReg nvars);
static void var_remove(LexState* ls, BCReg tolevel);
static BCReg var_lookup_local(FuncState* fs, GCstr* n);
static MSize var_lookup_uv(FuncState* fs, MSize vidx, ExpDesc* e);
static MSize var_lookup_(FuncState* fs, GCstr* name, ExpDesc* e, int first);

// -- Goto and labels (lj_parse_scope.c) -----------------------------------

static MSize gola_new(LexState* ls, int jump_type, uint8_t info, BCPos pc);
static void gola_patch(LexState* ls, VarInfo* vg, VarInfo* vl);
static void gola_close(LexState* ls, VarInfo* vg);
static void gola_resolve(LexState* ls, FuncScope* bl, MSize idx);
static void gola_fixup(LexState* ls, FuncScope* bl);

// -- Function scope (lj_parse_scope.c) ------------------------------------

static void fscope_begin(FuncState* fs, FuncScope* bl, int flags);
static void fscope_loop_continue(FuncState* fs, BCPos pos);
static void execute_defers(FuncState* fs, BCReg limit);
static void fscope_end(FuncState* fs);
static void fscope_uvmark(FuncState* fs, BCReg level);

// -- RAII Helper Classes --------------------------------------------------

#include "lj_parse_raii.h"

// -- C++20 Concepts -------------------------------------------------------

#include "lj_parse_concepts.h"

// -- Function state (lj_parse_scope.c) ------------------------------------

static void fs_fixup_bc(FuncState* fs, GCproto* pt, BCIns* bc, MSize n);
static void fs_fixup_uv2(FuncState* fs, GCproto* pt);
static void fs_fixup_k(FuncState* fs, GCproto* pt, void* kptr);
static void fs_fixup_uv1(FuncState* fs, GCproto* pt, uint16_t* uv);
static size_t fs_prep_line(FuncState* fs, BCLine numline);
static void fs_fixup_line(FuncState* fs, GCproto* pt,
   void* lineinfo, BCLine numline);
static size_t fs_prep_var(LexState* ls, FuncState* fs, size_t* ofsvar);
static void fs_fixup_var(LexState* ls, GCproto* pt, uint8_t* p, size_t ofsvar);
static int bcopisret(BCOp op);
static void fs_fixup_ret(FuncState* fs);
static GCproto* fs_finish(LexState* ls, BCLine line);
static void fs_init(LexState* ls, FuncState* fs);

// -- Expressions (lj_parse_expr.c) ----------------------------------------

static void expr(LexState* ls, ExpDesc* v);
static void expr_str(LexState* ls, ExpDesc* e);
static void expr_index(FuncState* fs, ExpDesc* t, ExpDesc* e);
static void expr_field(LexState* ls, ExpDesc* v);
static void expr_bracket(LexState* ls, ExpDesc* v);
static void expr_kvalue(FuncState* fs, TValue* v, ExpDesc* e);
static void expr_table(LexState* ls, ExpDesc* e);
static BCReg parse_params(LexState* ls, int needself);
static void parse_body_impl(LexState* ls, ExpDesc* e, int needself,
   BCLine line);
static void parse_body(LexState* ls, ExpDesc* e, int needself, BCLine line);
static void parse_body_defer(LexState* ls, ExpDesc* e, BCLine line);
static BCReg expr_list(LexState* ls, ExpDesc* v);
static void parse_args(LexState* ls, ExpDesc* e);
static void inc_dec_op(LexState* ls, BinOpr op, ExpDesc* v, int isPost);
static void expr_primary(LexState* ls, ExpDesc* v);
static void expr_simple(LexState* ls, ExpDesc* v);
static void synlevel_begin(LexState* ls);
static BinOpr token2binop(LexToken tok);
static BinOpr expr_binop(LexState* ls, ExpDesc* v, uint32_t limit);
static BinOpr expr_shift_chain(LexState* ls, ExpDesc* lhs, BinOpr op);
static void expr_unop(LexState* ls, ExpDesc* v);
static void expr_next(LexState* ls);
static BCPos expr_cond(LexState* ls);

// -- Statements (lj_parse_stmt.c) -----------------------------------------

static void assign_hazard(LexState* ls, LHSVarList* lh, const ExpDesc* v);
static void assign_adjust(LexState* ls, BCReg nvars, BCReg nexps, ExpDesc* e);
static int assign_compound(LexState* ls, LHSVarList* lh, LexToken opType);
static void parse_assignment(LexState* ls, LHSVarList* lh, BCReg nvars);
static void parse_call_assign(LexState* ls);
static void parse_local(LexState* ls);
static void snapshot_return_regs(FuncState* fs, BCIns* ins);
static void parse_defer(LexState* ls);
static void parse_func(LexState* ls, BCLine line);
static int parse_isend(LexToken tok);
static void parse_return(LexState* ls);
static void parse_continue(LexState* ls);
static void parse_break(LexState* ls);
static void parse_block(LexState* ls);
static void parse_while(LexState* ls, BCLine line);
static void parse_repeat(LexState* ls, BCLine line);
static void parse_for_num(LexState* ls, GCstr* varname, BCLine line);
static int predict_next(LexState* ls, FuncState* fs, BCPos pc);
static void parse_for_iter(LexState* ls, GCstr* indexname);
static void parse_for(LexState* ls, BCLine line);
static BCPos parse_then(LexState* ls);
static void parse_if(LexState* ls, BCLine line);
static int parse_stmt(LexState* ls);
static void parse_chunk(LexState* ls);

#endif
