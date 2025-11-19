// Lua parser
//
// Copyright (C) 2025 Paul Manias

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
#include <parasol/main.h>

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
#include "parser/token_types.cpp"
#include "parser/token_stream.cpp"
#include "parser/parser_diagnostics.cpp"
#include "parser/parser_profiler.h"
#include "parser/parser_context.cpp"
#include "parser/ast_nodes.cpp"
#include "parser/ast_builder.cpp"
#include "parser/ir_emitter.cpp"
#include "parser/parse_types.h"
#include "parser/parse_internal.h"
#include "parser/parse_core.cpp"
#include "parser/parse_constants.cpp"
#include "parser/parse_scope.cpp"
#include "parser/parse_regalloc.cpp"
#include "parser/parse_expr.cpp"
#include "parser/parse_operators.cpp"
#include "parser/parse_stmt.cpp"
#include "../../../defs.h"

namespace {

static constexpr size_t kMaxLoggedStatements = 12;

static void report_pipeline_error(ParserContext& context, const ParserError& error)
{
   context.emit_error(error.code, error.token, error.message);
}

static void flush_non_fatal_errors(ParserContext& context)
{
   if (context.config().abort_on_error) return;
   if (context.diagnostics().has_errors()) raise_accumulated_diagnostics(context);
}

static void trace_ast_boundary(ParserContext& context, const BlockStmt& chunk, const char* stage)
{
   if (not context.config().trace_ast_boundaries) return;

   pf::Log log("Fluid-Parser");
   StatementListView statements = chunk.view();
   SourceSpan span = chunk.span;
   log.detail("ast-boundary[%s]: statements=%" PRId64 " span=%d:%d offset=%" PRId64,
      stage, statements.size(), int(span.line), int(span.column), span.offset);

   size_t index = 0;
   for (const StmtNode& stmt : statements) {
      if (index >= kMaxLoggedStatements) {
         log.detail("   ... truncated after %" PRId64 " statements ...", index);
         break;
      }

      size_t children = ast_statement_child_count(stmt);
      SourceSpan stmt_span = stmt.span;
      log.detail("   stmt[%" PRId64 "] kind=%d children=%" PRId64 " span=%d:%d offset=%" PRId64, index,
         int(stmt.kind), children, int(stmt_span.line), int(stmt_span.column), stmt_span.offset);

      if (stmt.kind IS AstNodeKind::ExpressionStmt) {
         const auto* payload = std::get_if<ExpressionStmtPayload>(&stmt.data);
         if (payload and payload->expression) {
            const ExprNode& expr = *payload->expression;
            size_t expr_children = ast_expression_child_count(expr);
            SourceSpan expr_span = expr.span;
            log.detail("      expr kind=%d children=%" PRId64 " span=%d:%d offset=%" PRId64,
               int(expr.kind), expr_children, int(expr_span.line), int(expr_span.column), expr_span.offset);
         }
      }

      ++index;
   }
}

static void trace_bytecode_snapshot(ParserContext& context, const char* label)
{
   pf::Log log("Fluid-Parser");

   if (not context.config().dump_ast_bytecode) return;

   FuncState& fs = context.func();
   log.detail("bytecode-%s: count=%u", label, (unsigned)fs.pc);
   for (BCPos pc = 0; pc < fs.pc; ++pc) {
      const BCInsLine& line = fs.bcbase[pc];
      log.detail("   [%04d] op=%03d A=%03d B=%03d C=%03d D=%05d line=%d",
         (int)pc, (int)bc_op(line.ins), (int)bc_a(line.ins), (int)bc_b(line.ins),
         (int)bc_c(line.ins), (int)bc_d(line.ins), (int)line.line);
   }
}

static void run_ast_pipeline(ParserContext& context, ParserProfiler& profiler)
{
   ParserProfiler::StageTimer parse_timer = profiler.stage("parse");
   AstBuilder builder(context);
   auto chunk_result = builder.parse_chunk();
   if (not chunk_result.ok()) {
      report_pipeline_error(context, chunk_result.error_ref());
      flush_non_fatal_errors(context);
      return;
   }

   std::unique_ptr<BlockStmt> chunk = std::move(chunk_result.value_ref());
   parse_timer.stop();
   trace_ast_boundary(context, *chunk, "parse");

   ParserProfiler::StageTimer emit_timer = profiler.stage("emit");
   IrEmitter emitter(context);
   auto emit_result = emitter.emit_chunk(*chunk);
   if (not emit_result.ok()) {
      report_pipeline_error(context, emit_result.error_ref());
      flush_non_fatal_errors(context);
      return;
   }

   emit_timer.stop();

   trace_bytecode_snapshot(context, "ast");
   flush_non_fatal_errors(context);
}

}  // namespace

static ParserConfig make_parser_config(lua_State &State)
{
   pf::Log log("FluidParser");
   ParserConfig config;

   if (State.jit_pipeline) config.enable_ast_pipeline = true;

   if (State.jit_trace_boundary) config.trace_ast_boundaries = true;

   if (State.jit_trace_bytecode) config.dump_ast_bytecode = true;

   if (State.jit_profile) {
      log.msg("JIT parser profiling enabled.");
      config.profile_stages = true;
   }

   if (State.jit_diagnose) {
      log.msg("JIT diagnostic mode enabled.");
      config.abort_on_error = false;
      config.max_diagnostics = 32;
   }

   if (State.jit_trace) {
      log.msg("JIT trace mode enabled.");
      config.trace_tokens = true;
      config.trace_expectations = true;
   }

   return config;
}

// Entry point of bytecode parser.

extern GCproto * lj_parse(LexState *State)
{
   FuncState fs;
   FuncScope bl;
   GCproto *pt;
   lua_State *L = State->L;

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
   ParserProfiler  profiler(root_context.config().profile_stages, &root_context.profiling_result());

   State->next(); // Read-ahead first token.

   if (session_config.enable_ast_pipeline) {
      run_ast_pipeline(root_context, profiler);
   }
   else {
      pf::Log().warning("Using legacy Lua parser; AST pipeline is disabled.");
      ParserProfiler::StageTimer legacy_timer = profiler.stage("legacy-chunk");
      State->parse_chunk(root_context);
      legacy_timer.stop();
   }

   if (profiler.enabled()) {
      pf::Log profile_log("Fluid-Parser");
      profiler.log_results(profile_log);
   }

   if (State->tok != TK_eof) State->err_token(TK_eof);
   pt = State->fs_finish(State->linenumber);
   L->top--;  // Drop chunkname.
   lj_assertL(fs.prev == nullptr and State->fs == nullptr, "mismatched frame nesting");
   lj_assertL(pt->sizeuv == 0, "toplevel proto has upvalues");
   return pt;
}
