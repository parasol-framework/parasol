// Parser self-test helpers using the tracing infrastructure.

#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "parser/parser_context.h"
#include "parser/parser_trace.h"

struct ParserSelfTestCase {
   std::string_view name;
   std::string_view source;
   ParserTraceSummary expected;
   ParserPipelineMode pipeline_mode = ParserPipelineMode::AstPreferred;
};

struct ParserSelfTestCaseResult {
   std::string name;
   ParserTraceSummary expected;
   ParserTraceSummary actual;
   bool passed = false;
};

struct ParserSelfTestReport {
   bool passed = false;
   std::vector<ParserSelfTestCaseResult> cases;
};

ParserTraceSummary parser_trace_probe(lua_State* State, std::string_view Source, ParserConfig Config);
ParserSelfTestReport parser_run_self_tests(lua_State* State, std::span<const ParserSelfTestCase> Cases);
ParserSelfTestReport parser_run_default_self_tests(lua_State* State);

