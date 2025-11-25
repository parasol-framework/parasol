// Lua parser
//
// Copyright (C) 2025 Paul Manias

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
#include "lexer.h"
#include "parser.h"
#include "lj_vm.h"
#include "lj_vmevent.h"

#include <parasol/main.h>

// Priorities for each binary operator. ORDER OPR.

static const struct {
   uint8_t left;      // Left priority.
   uint8_t right;     // Right priority.
   CSTRING name;      // Name for bitlib function (if applicable).
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

#include "dump_bytecode.h"
#include "token_types.h"
#include "parse_types.h"
#include "parse_internal.h"
#include "parser_profiler.h"
#include "value_categories.h"
#include "../../../defs.h"

#include "token_types.cpp"
#include "token_stream.cpp"
#include "parser_diagnostics.cpp"
#include "parser_context.cpp"
#include "ast_nodes.cpp"
#include "ast_builder.cpp"
#include "parse_control_flow.cpp"
#include "ir_emitter.cpp"
#include "parse_core.cpp"
#include "parse_constants.cpp"
#include "parse_scope.cpp"
#include "parse_regalloc.cpp"
#include "parse_expr.cpp"
#include "parse_stmt.cpp"
#include "operator_emitter.cpp"
#include "value_categories.cpp"

static constexpr size_t kMaxLoggedStatements = 12;

static void raise_accumulated_diagnostics(ParserContext &Context)
{
   auto entries = Context.diagnostics().entries();
   if (entries.empty()) return;

   auto summary = std::format("parser reported {} {}:\n", entries.size(), entries.size() == 1 ? "error" : "errors");

   for (const auto& diagnostic : entries) {
      SourceSpan span = diagnostic.token.span();
      std::string_view message = diagnostic.message.empty() ? "unexpected token" : diagnostic.message;
      summary += std::format("   line {}:{} - {}\n", span.line, span.column, message);
   }

   lua_State *L = &Context.lua();

   // Store diagnostic information in lua_State before throwing
   L->parser_diagnostics = new ParserDiagnostics(Context.diagnostics());

   GCstr *message = lj_str_new(L, summary.data(), summary.size());
   setstrV(L, L->top++, message);
   lj_err_throw(L, LUA_ERRSYNTAX);
}

//********************************************************************************************************************

static void report_pipeline_error(ParserContext &Context, const ParserError &Error)
{
   Context.emit_error(Error.code, Error.token, Error.message);
}

//********************************************************************************************************************

static void flush_non_fatal_errors(ParserContext &Context)
{
   if (Context.config().abort_on_error) return;
   if (Context.diagnostics().has_errors()) raise_accumulated_diagnostics(Context);
}

//********************************************************************************************************************

static void trace_ast_boundary(ParserContext &Context, const BlockStmt &Chunk, CSTRING Stage)
{
   pf::Log log("AST-Boundary");

   auto prv = (prvFluid *)Context.lua().Script->ChildPrivate;
   if ((prv->JitOptions & JOF::TRACE_BOUNDARY) IS JOF::NIL) return;

   StatementListView statements = Chunk.view();
   SourceSpan span = Chunk.span;
   log.branch("[%s]: statements=%" PRId64 " span=%d:%d offset=%" PRId64,
      Stage, statements.size(), int(span.line), int(span.column), span.offset);

   size_t index = 0;
   for (const StmtNode& stmt : statements) {
      if (index >= kMaxLoggedStatements) {
         log.msg("... truncated after %" PRId64 " statements ...", index);
         break;
      }

      size_t children = ast_statement_child_count(stmt);
      SourceSpan stmt_span = stmt.span;
      log.msg("stmt[%" PRId64 "] kind=%d children=%" PRId64 " span=%d:%d offset=%" PRId64, index,
         int(stmt.kind), children, int(stmt_span.line), int(stmt_span.column), stmt_span.offset);

      if (stmt.kind IS AstNodeKind::ExpressionStmt) {
         const auto * payload = std::get_if<ExpressionStmtPayload>(&stmt.data);
         if (payload and payload->expression) {
            const ExprNode& expr = *payload->expression;
            size_t expr_children = ast_expression_child_count(expr);
            SourceSpan expr_span = expr.span;
            log.msg("      expr kind=%d children=%" PRId64 " span=%d:%d offset=%" PRId64,
               int(expr.kind), expr_children, int(expr_span.line), int(expr_span.column), expr_span.offset);
         }
      }

      ++index;
   }
}

//********************************************************************************************************************
// Run the AST-based parsing pipeline.

static void run_ast_pipeline(ParserContext &Context, ParserProfiler &Profiler)
{
   ParserProfiler::StageTimer parse_timer = Profiler.stage("parse");
   AstBuilder builder(Context);

   auto chunk_result = builder.parse_chunk();

   if (not chunk_result.ok()) {
      report_pipeline_error(Context, chunk_result.error_ref());
      flush_non_fatal_errors(Context);
      return;
   }

   std::unique_ptr<BlockStmt> chunk = std::move(chunk_result.value_ref());
   parse_timer.stop();
   trace_ast_boundary(Context, *chunk, "parse");

   ParserProfiler::StageTimer emit_timer = Profiler.stage("emit");
   IrEmitter emitter(Context);
   auto emit_result = emitter.emit_chunk(*chunk);
   if (not emit_result.ok()) {
      report_pipeline_error(Context, emit_result.error_ref());
      flush_non_fatal_errors(Context);
      return;
   }

   emit_timer.stop();
}

//********************************************************************************************************************

static ParserConfig make_parser_config(lua_State &State)
{
   ParserConfig config;

   auto prv = (prvFluid *)State.Script->ChildPrivate;

   if ((prv->JitOptions & JOF::DIAGNOSE) != JOF::NIL) {
      // Cancel aborting on error and enable deeper log tracing.
      config.abort_on_error = false;
      config.max_diagnostics = 32;
   }

   return config;
}

//********************************************************************************************************************
// Entry point of bytecode parser.

extern GCproto * lj_parse(LexState *State)
{
   pf::Log log("Parser");
   FuncState fs;
   FuncScope bl;
   GCproto *pt;
   lua_State *L = State->L;

   auto prv = (prvFluid *)L->Script->ChildPrivate;

#ifdef LUAJIT_DISABLE_DEBUGINFO
   State->chunkname = lj_str_newlit(L, "=");
#else
   State->chunkname = lj_str_newz(L, State->chunkarg);
#endif

   setstrV(L, L->top, State->chunkname);  // Anchor chunkname string.
   incr_top(L);
   State->level = 0;
   State->fs_init(&fs);
   fs.linedefined = 0;
   fs.numparams = 0;
   fs.bcbase = nullptr;
   fs.bclim = 0;
   fs.flags |= PROTO_VARARG;  // Main chunk is always a vararg func.
   fscope_begin(&fs, &bl, FuncScopeFlag::None);
   bcemit_AD(&fs, BC_FUNCV, 0, 0);  // Placeholder.

   ParserAllocator allocator      = ParserAllocator::from(L);
   ParserContext   root_context   = ParserContext::from(*State, fs, allocator);
   ParserConfig    session_config = make_parser_config(*L);

   ParserSession   root_session(root_context, session_config);
   ParserProfiler  profiler((prv->JitOptions & JOF::PROFILE) != JOF::NIL, &root_context.profiling_result());

   State->next(); // Read-ahead first token.

   run_ast_pipeline(root_context, profiler);

   if ((prv->JitOptions & JOF::DUMP_BYTECODE) != JOF::NIL) dump_bytecode(root_context);

   flush_non_fatal_errors(root_context);

   if (profiler.enabled()) {
      profiler.log_results(log);
   }

   if (State->tok != TK_eof) State->err_token(TK_eof);
   pt = State->fs_finish(State->linenumber);
   L->top--;  // Drop chunkname.
   lj_assertL(fs.prev == nullptr and State->fs == nullptr, "mismatched frame nesting");
   lj_assertL(pt->sizeuv == 0, "toplevel proto has upvalues");
   return pt;
}
