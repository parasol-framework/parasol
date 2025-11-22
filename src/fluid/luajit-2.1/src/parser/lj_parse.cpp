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

#include "parser/token_types.h"
#include "parser/token_types.cpp"
#include "parser/token_stream.cpp"
#include "parser/parser_diagnostics.cpp"
#include "parser/parser_profiler.h"
#include "parser/parser_context.cpp"
#include "parser/ast_nodes.cpp"
#include "parser/ast_builder.cpp"
#include "parser/parse_control_flow.cpp"
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

static constexpr size_t kMaxLoggedStatements = 12;

//********************************************************************************************************************

static std::string format_string_constant(std::string_view Data)
{
   static constexpr size_t max_length = 40;
   static constexpr std::string_view hex_digits = "0123456789ABCDEF";

   std::string text;
   const auto limit = std::min(Data.size(), max_length);
   const bool truncated = Data.size() > max_length;

   text.reserve(limit * 2 + (truncated ? 6 : 2));

   for (size_t i = 0; i < limit; i++) {
      const unsigned char ch = Data[i];

      switch (ch) {
         case '\n': text += "\\n"; break;
         case '\r': text += "\\r"; break;
         case '\t': text += "\\t"; break;
         case '\\': text += "\\\\"; break;
         case '"':  text += "\\\""; break;
         default:
            if (ch < 32) {
               text += "\\x";
               text += hex_digits[(ch >> 4) & 15];
               text += hex_digits[ch & 15];
            }
            else text += char(ch);
            break;
      }
   }

   if (truncated) text += "...";

   return std::format("\"{}\"", text);
}

//********************************************************************************************************************

static std::string describe_num_constant(const TValue *Value)
{
   if (tvisint(Value)) return std::format("{}", intV(Value));
   if (tvisnum(Value)) return std::format("{}", numV(Value));
   return "<number>";
}

//********************************************************************************************************************

static std::string describe_gc_constant(GCproto *Proto, ptrdiff_t Index)
{
   GCobj *gc_obj = proto_kgc(Proto, Index);

   if (gc_obj->gch.gct IS (uint8_t)~LJ_TSTR) {
      GCstr *str_obj = gco2str(gc_obj);
      return std::format("K{}", format_string_constant({strdata(str_obj), str_obj->len}));
   }

   if (gc_obj->gch.gct IS (uint8_t)~LJ_TPROTO) {
      GCproto *child = gco2pt(gc_obj);
      return std::format("K<func {}-{}>", child->firstline, child->firstline + child->numline);
   }

   if (gc_obj->gch.gct IS (uint8_t)~LJ_TTAB) return "K<table>";

#if LJ_HASFFI
   if (gc_obj->gch.gct IS (uint8_t)~LJ_TCDATA) return "K<cdata>";
#endif

   return "K<gc>";
}

//********************************************************************************************************************

static std::string describe_primitive(int Value)
{
   switch (Value) {
      case 0: return "nil";
      case 1: return "false";
      case 2: return "true";
      default: return std::format("pri({})", Value);
   }
}

//********************************************************************************************************************

static std::string_view get_proto_uvname(GCproto *Proto, uint32_t Index)
{
   const uint8_t *info = proto_uvinfo(Proto);
   if (not info or Index >= Proto->sizeuv) return {};

   const uint8_t *ptr = info;

   for (uint32_t i = 0; i < Index; ++i) {
      while (*ptr) ++ptr;
      ++ptr;
   }

   if (not *ptr) return {};
   return std::string_view(reinterpret_cast<const char *>(ptr));
}

//********************************************************************************************************************

static std::string describe_operand_value(GCproto *Proto, BCMode Mode, int Value, BCPos Pc)
{
   switch (Mode) {
      case BCMdst:
      case BCMbase:
      case BCMvar:
      case BCMrbase:
         return std::format("R{}", Value);

      case BCMuv: {
         auto name = get_proto_uvname(Proto, (uint32_t)Value);
         return name.empty() ? std::format("U{}", Value) : std::format("U{}({})", Value, name);
      }

      case BCMlit:
         return std::format("#{}", Value);

      case BCMlits:
         return std::format("#{}", (int16_t)Value);

      case BCMpri:
         return describe_primitive(Value);

      case BCMnum:
         return std::format("#{}", describe_num_constant(proto_knumtv(Proto, Value)));

      case BCMstr:
      case BCMfunc:
      case BCMtab:
      case BCMcdata:
         return describe_gc_constant(Proto, -(ptrdiff_t)Value - 1);

      case BCMjump: {
         if ((BCPos)Value IS NO_JMP) return "->(no)";

         const ptrdiff_t offset = (ptrdiff_t)Value - BCBIAS_J;
         const ptrdiff_t dest = (ptrdiff_t)Pc + 1 + offset;

         if (dest < 0) return "->(neg)";
         if (dest >= (ptrdiff_t)Proto->sizebc) return "->(out)";

         return std::format("->{}{}", dest, offset >= 0 ? std::format("(+{})", offset) : std::format("({})", offset));
      }

      default:
         return std::format("?{}", Value);
   }
}

//********************************************************************************************************************

static void append_operand(std::string &Operands, std::string_view Label, std::string_view Value)
{
   if (not Operands.empty()) Operands += ' ';
   Operands += std::format("{}={}", Label, Value);
}

//********************************************************************************************************************
// Describe operand value during parsing (from FuncState context)

static std::string describe_operand_from_fs(FuncState *fs, BCMode Mode, int Value, BCPos Pc)
{
   switch (Mode) {
      case BCMdst:
      case BCMbase:
      case BCMvar:
      case BCMrbase:
         return std::format("R{}", Value);

      case BCMuv:
         return std::format("U{}", Value);

      case BCMlit:
         return std::format("#{}", Value);

      case BCMlits:
         return std::format("#{}", (int16_t)Value);

      case BCMpri:
         return describe_primitive(Value);

      case BCMnum: {
         // Look up number constant in the constant table
         GCtab *kt = fs->kt;
         Node *node = noderef(kt->node);

         for (uint32_t i = 0; i <= kt->hmask; ++i) {
            TValue *val = &node[i].val;
            if (tvhaskslot(val) and tvkslot(val) IS (uint32_t)Value) {
               TValue *key_tv = &node[i].key;
               if (tvisnum(key_tv) or tvisint(key_tv)) {
                  return std::format("#{}", describe_num_constant(key_tv));
               }
               break;
            }
         }
         return std::format("#<num{}>", Value);
      }

      case BCMstr:
      case BCMfunc:
      case BCMtab:
      case BCMcdata: {
         // Look up GC constant in the constant table
         GCtab *kt = fs->kt;
         Node *node = noderef(kt->node);

         for (uint32_t i = 0; i <= kt->hmask; ++i) {
            TValue *val = &node[i].val;
            if (tvhaskslot(val) and tvkslot(val) IS (uint32_t)Value) {
               TValue *key_tv = &node[i].key;

               if (tvisstr(key_tv)) {
                  GCstr *str_obj = strV(key_tv);
                  return std::format("K{}", format_string_constant({strdata(str_obj), str_obj->len}));
               }

               if (tvisproto(key_tv)) {
                  GCproto *child = protoV(key_tv);
                  return std::format("K<func {}-{}>", child->firstline, child->firstline + child->numline);
               }

               if (tvistab(key_tv)) return "K<table>";

#if LJ_HASFFI
               if (tviscdata(key_tv)) return "K<cdata>";
#endif
               break;
            }
         }
         return std::format("K<gc{}>", Value);
      }

      case BCMjump: {
         if ((BCPos)Value IS NO_JMP) return "->(no)";

         const ptrdiff_t offset = (ptrdiff_t)Value - BCBIAS_J;
         const ptrdiff_t dest = (ptrdiff_t)Pc + 1 + offset;

         if (dest < 0) return "->(neg)";
         if (dest >= (ptrdiff_t)fs->pc) return "->(out)";

         return std::format("->{}{}", dest, offset >= 0 ? std::format("(+{})", offset) : std::format("({})", offset));
      }

      default:
         return std::format("?{}", Value);
   }
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
         const auto* payload = std::get_if<ExpressionStmtPayload>(&stmt.data);
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
// Extract bytecode info and build operands string - common helper

struct BytecodeInfo {
   BCOp op;
   CSTRING op_name;
   BCMode mode_a, mode_b, mode_c, mode_d;
   int value_a, value_b, value_c, value_d;
};

static BytecodeInfo extract_instruction_info(BCIns Ins)
{
   BytecodeInfo info;
   info.op = bc_op(Ins);
   info.op_name = (info.op < BC__MAX) ? glBytecodeNames[info.op] : "???";
   info.mode_a  = bcmode_a(info.op);
   info.mode_b  = bcmode_b(info.op);
   info.mode_c  = bcmode_c(info.op);
   info.mode_d  = bcmode_d(info.op);
   info.value_a = bc_a(Ins);
   info.value_b = bc_b(Ins);
   info.value_c = bc_c(Ins);
   info.value_d = bc_d(Ins);
   return info;
}

//********************************************************************************************************************
// Recursively print bytecode for a finalized prototype.

static void trace_proto_bytecode(GCproto *Proto, int Indent = 0)
{
   pf::Log log("ByteCode");
   if (not Proto) return;

   const BCIns *bc_stream = proto_bc(Proto);
   std::string indent_str(Indent * 2, ' ');

   if (Indent > 0) {
      log.branch("%s--- Nested function: lines %d-%d, %d bytecodes ---",
         indent_str.c_str(), int(Proto->firstline),
         int(Proto->firstline + Proto->numline), int(Proto->sizebc));
   }

   for (BCPos pc = 0; pc < Proto->sizebc; ++pc) {
      BCIns instruction = bc_stream[pc];
      auto info = extract_instruction_info(instruction);

      std::string operands;

      if (info.mode_a != BCMnone) append_operand(operands, "A", describe_operand_value(Proto, info.mode_a, info.value_a, pc));

      if (bcmode_hasd(info.op)) {
         if (info.mode_d != BCMnone) append_operand(operands, "D", describe_operand_value(Proto, info.mode_d, info.value_d, pc));
      }
      else {
         if (info.mode_b != BCMnone) append_operand(operands, "B", describe_operand_value(Proto, info.mode_b, info.value_b, pc));
         if (info.mode_c != BCMnone) append_operand(operands, "C", describe_operand_value(Proto, info.mode_c, info.value_c, pc));
      }

      log.msg("%s[%04d] %-10s %s", indent_str.c_str(), (int)pc, info.op_name, operands.c_str());

      // If this is a FNEW instruction, recursively disassemble the child prototype
      if (info.op IS BC_FNEW) {
         const ptrdiff_t index = -(ptrdiff_t)info.value_d - 1;
         const bool valid = ((uintptr_t)(intptr_t)index >= (uintptr_t)-(intptr_t)Proto->sizekgc);

         if (valid) {
            GCobj *gc_obj = proto_kgc(Proto, index);
            if (gc_obj->gch.gct IS (uint8_t)~LJ_TPROTO) {
               GCproto *child = gco2pt(gc_obj);
               trace_proto_bytecode(child, Indent + 1);
            }
         }
      }
   }
}

//********************************************************************************************************************
// Print a complete disassembly of bytecode instructions.

static void dump_bytecode(ParserContext &Context)
{
   pf::Log log("ByteCode");

   auto prv = (prvFluid *)Context.lua().Script->ChildPrivate;
   if ((prv->JitOptions & JOF::DUMP_BYTECODE) IS JOF::NIL) return;

   FuncState &fs = Context.func();
   log.branch("Instruction Count: %u", (unsigned)fs.pc);
   for (BCPos pc = 0; pc < fs.pc; ++pc) {
      const BCInsLine& line = fs.bcbase[pc];
      auto info = extract_instruction_info(line.ins);

      std::string operands;

      if (info.mode_a != BCMnone) append_operand(operands, "A", describe_operand_from_fs(&fs, info.mode_a, info.value_a, pc));

      if (bcmode_hasd(info.op)) {
         if (info.mode_d != BCMnone) append_operand(operands, "D", describe_operand_from_fs(&fs, info.mode_d, info.value_d, pc));
      }
      else {
         if (info.mode_b != BCMnone) append_operand(operands, "B", describe_operand_from_fs(&fs, info.mode_b, info.value_b, pc));
         if (info.mode_c != BCMnone) append_operand(operands, "C", describe_operand_from_fs(&fs, info.mode_c, info.value_c, pc));
      }

      log.msg("[%04d] %-10s %s", (int)pc, info.op_name, operands.c_str());

      // If this is a FNEW instruction, look up and print the child prototype
      if (info.op IS BC_FNEW) {
         // FNEW uses D operand which stores the constant slot index
         // Search the hash table to find the proto with this slot number
         GCtab *kt = fs.kt;
         Node *node = noderef(kt->node);

         for (uint32_t i = 0; i <= kt->hmask; ++i) {
            TValue *val = &node[i].val;
            if (tvhaskslot(val) and tvkslot(val) IS (uint32_t)info.value_d) {
               TValue *key_tv = &node[i].key;
               if (tvisproto(key_tv)) {
                  GCproto *child = protoV(key_tv);
                  trace_proto_bytecode(child, 1);
               }
               break;
            }
         }
      }
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

   // Print a complete disassembly of bytecode instructions after AST emission.
   dump_bytecode(Context);

   flush_non_fatal_errors(Context);
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

   if ((prv->JitOptions & JOF::LEGACY) IS JOF::NIL) {
      run_ast_pipeline(root_context, profiler);
   }
   else {
      log.msg("Using legacy Lua parser.");
      ParserProfiler::StageTimer legacy_timer = profiler.stage("legacy-chunk");
      State->parse_chunk(root_context);
      legacy_timer.stop();
   }

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
