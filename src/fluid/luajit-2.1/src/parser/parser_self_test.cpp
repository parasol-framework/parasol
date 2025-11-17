#include "parser/parser_self_test.h"

#include <algorithm>
#include <array>
#include <optional>

#include "lj_err.h"
#include "lj_str.h"
#include "lj_state.h"
#include "lj_bc.h"
#include "parser/lj_parse.h"
#include "parser/parse_internal.h"

namespace {

struct ParserStringReader {
   const char* data = nullptr;
   size_t size = 0;
   bool consumed = false;
};

const char* parser_string_reader(lua_State* L, void* UserData, size_t* Size)
{
   UNUSED(L);
   auto* reader = reinterpret_cast<ParserStringReader*>(UserData);
   if (!reader or reader->consumed) {
      *Size = 0;
      return nullptr;
   }
   reader->consumed = true;
   *Size = reader->size;
   return reader->data;
}

ParserTraceSummary run_parser(lua_State* State, std::string_view Source, ParserConfig Config)
{
   ParserStringReader reader;
   reader.data = Source.data();
   reader.size = Source.size();
   std::string chunk_name_storage = "=(parser-self-test)";
   LexState lex(State, parser_string_reader, &reader, chunk_name_storage, std::nullopt);
   FuncState func_state;
   FuncScope block_scope;
   Config.enable_tracing = true;
   Config.max_trace_events = std::max<std::size_t>(Config.max_trace_events, 64);
   Config.max_diagnostics = std::max<std::size_t>(Config.max_diagnostics, 8);

   GCstr* chunk_name = lj_str_newz(State, chunk_name_storage.c_str());
   lex.chunkname = chunk_name;
   setstrV(State, State->top, chunk_name);
   incr_top(State);

   lex.level = 0;
   lex.fs_init(&func_state);
   ParserContext context = ParserContext::from(lex, func_state, *State, Config);
   ParserSession root_session(context, Config);
   lex.attach_context(&context);

   func_state.linedefined = 0;
   func_state.numparams = 0;
   func_state.bcbase = nullptr;
   func_state.bclim = 0;
   func_state.flags |= PROTO_VARARG;
   fscope_begin(&func_state, &block_scope, FuncScopeFlag::None);
   bcemit_AD(&func_state, BC_FUNCV, 0, 0);

   lex.next();
   lex.parse_chunk();
   if (lex.tok != TK_eof)
      lex.err_token(TK_eof);
   GCproto* proto = lex.fs_finish(lex.linenumber);
   UNUSED(proto);
   State->top--;
   lex.attach_context(nullptr);
   return summarize_trace(context.trace().entries());
}

struct DefaultSelfTests {
   std::array<ParserSelfTestCase, 3> cases;

   DefaultSelfTests()
   {
      ParserTraceSummary ast_single;
      ast_single.ast_primary_attempts = 1;
      ast_single.ast_primary_successes = 1;
      ast_single.local_statement_attempts = 1;
      ast_single.local_statement_successes = 1;
      this->cases[0] = ParserSelfTestCase{
         .name = "local_single_assignment",
         .source = "local foo = bar\n",
         .expected = ast_single,
         .pipeline_mode = ParserPipelineMode::AstPreferred,
      };

      ParserTraceSummary ast_suffix = ast_single;
      this->cases[1] = ParserSelfTestCase{
         .name = "local_suffix_chain",
         .source = "local foo = bar.baz??\n",
         .expected = ast_suffix,
         .pipeline_mode = ParserPipelineMode::AstPreferred,
      };

      ParserTraceSummary legacy_only;
      this->cases[2] = ParserSelfTestCase{
         .name = "legacy_pipeline_guard",
         .source = "local foo = (bar)\n",
         .expected = legacy_only,
         .pipeline_mode = ParserPipelineMode::LegacyOnly,
      };
   }
};

}  // namespace

ParserTraceSummary parser_trace_probe(lua_State* State, std::string_view Source, ParserConfig Config)
{
   return run_parser(State, Source, Config);
}

ParserSelfTestReport parser_run_self_tests(lua_State* State, std::span<const ParserSelfTestCase> Cases)
{
   ParserSelfTestReport report;
   report.passed = true;
   for (const auto& test_case : Cases) {
      ParserConfig config;
      config.pipeline_mode = test_case.pipeline_mode;
      ParserTraceSummary summary = parser_trace_probe(State, test_case.source, config);
      ParserSelfTestCaseResult result;
      result.name.assign(test_case.name.begin(), test_case.name.end());
      result.expected = test_case.expected;
      result.actual = summary;
      result.passed = summary.matches(test_case.expected);
      if (!result.passed)
         report.passed = false;
      report.cases.push_back(result);
   }
   return report;
}

ParserSelfTestReport parser_run_default_self_tests(lua_State* State)
{
   static DefaultSelfTests tests;
   return parser_run_self_tests(State, tests.cases);
}

