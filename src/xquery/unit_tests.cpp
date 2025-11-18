// XPath Module Unit Tests
// This file contains compiled-in unit tests for the XPath module, primarily for debugging prolog integration.
// Unit tests need to be enabled in the CMakeLists.txt file and then launched from test_unit_tests.fluid

#include <parasol/modules/xquery.h>
#include "xquery.h"
#include "../xml/xml.h"

#include <iostream>
#include <sstream>
#include <string>

//********************************************************************************************************************
// Test helper functions

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

static void test_assert(bool Condition, CSTRING TestName, CSTRING Message) {
   pf::Log log("XQueryTests");
   test_count++;
   if (Condition) {
      pass_count++;
      log.msg("PASS: %s", TestName);
   }
   else {
      fail_count++;
      log.msg("FAIL: %s - %s", TestName, Message);
   }
}

//********************************************************************************************************************
// XQueryProlog API Tests

static void test_prolog_api() {
   pf::Log log("PrologTests");

   // Test 1: Create empty prolog
   {
      XQueryProlog prolog;
      test_assert(prolog.functions.empty(), "Empty prolog creation", "New prolog should have no functions");
   }

   // Test 2: Declare a function
   {
      XQueryProlog prolog;
      XQueryFunction func;
      func.qname = "local:test";
      func.parameter_names.push_back("x");
      prolog.declare_function(std::move(func));

      auto found = prolog.find_function("local:test", 1);
      test_assert(found not_eq nullptr, "Function declaration", "Declared function should be findable");
   }

   // Test 3: Function arity matching
   {
      XQueryProlog prolog;
      XQueryFunction func;
      func.qname = "local:add";
      func.parameter_names.push_back("a");
      func.parameter_names.push_back("b");
      prolog.declare_function(std::move(func));

      auto found1 = prolog.find_function("local:add", 2);
      auto found2 = prolog.find_function("local:add", 1);

      test_assert((found1 not_eq nullptr) and (found2 IS nullptr),
         "Function arity matching",
         "Function should only match correct arity");
   }

   // Test 4: Variable declaration
   {
      XQueryProlog prolog;
      XQueryVariable var;
      var.qname = "pi";
      prolog.declare_variable("pi", std::move(var));

      auto found = prolog.find_variable("pi");
      test_assert(found not_eq nullptr, "Variable declaration",
         "Declared variable should be findable");
   }

   // Test 6: Multiple functions with same name, different arity
   {
      XQueryProlog prolog;

      XQueryFunction func1;
      func1.qname = "local:format";
      prolog.declare_function(std::move(func1));

      XQueryFunction func2;
      func2.qname = "local:format";
      func2.parameter_names.push_back("fmt");
      prolog.declare_function(std::move(func2));

      XQueryFunction func3;
      func3.qname = "local:format";
      func3.parameter_names.push_back("fmt");
      func3.parameter_names.push_back("arg");
      prolog.declare_function(std::move(func3));

      auto f0 = prolog.find_function("local:format", 0);
      auto f1 = prolog.find_function("local:format", 1);
      auto f2 = prolog.find_function("local:format", 2);
      auto f3 = prolog.find_function("local:format", 3);

      bool all_found = (f0 not_eq nullptr) and (f1 not_eq nullptr) and (f2 not_eq nullptr) and (f3 IS nullptr);
      test_assert(all_found, "Function overloading by arity",
         "Should support multiple arities for same function name");
   }
}

//********************************************************************************************************************
// Prolog Integration Tests

static const char *token_type_name(XPathTokenType Type)
{
   switch (Type)
   {
      case XPathTokenType::IDENTIFIER: return "IDENTIFIER";
      case XPathTokenType::MODULE: return "MODULE";
      case XPathTokenType::IMPORT: return "IMPORT";
      case XPathTokenType::OPTION: return "OPTION";
      case XPathTokenType::ORDER: return "ORDER";
      case XPathTokenType::COLLATION: return "COLLATION";
      case XPathTokenType::ORDERING: return "ORDERING";
      case XPathTokenType::COPY_NAMESPACES: return "COPY_NAMESPACES";
      case XPathTokenType::DECIMAL_FORMAT: return "DECIMAL_FORMAT";
      case XPathTokenType::SCHEMA: return "SCHEMA";
      case XPathTokenType::DEFAULT: return "DEFAULT";
      case XPathTokenType::COLON: return "COLON";
      case XPathTokenType::ASSIGN: return "ASSIGN";
      default: return "(unclassified)";
   }
}

static void test_tokeniser_prolog_keywords()
{
   pf::Log log("TokeniserTests");

   // Progress marker (2024-10-17): capturing current behaviour before adding DECLARE/FUNCTION/VARIABLE tokens.

   XPathTokeniser tokeniser;

   auto function_block = tokeniser.tokenize("declare function local:square($x) { $x * $x }");
   const auto &function_tokens = function_block.tokens;
   test_assert(function_tokens.size() >= 6, "Function declaration token count",
      "Tokeniser should emit tokens for sample prolog function");

   if (!function_tokens.empty())
   {
      bool declare_keyword = function_tokens[0].type not_eq XPathTokenType::IDENTIFIER;
      std::string declare_message = "Tokeniser reports 'declare' as " +
         std::string(token_type_name(function_tokens[0].type));
      test_assert(declare_keyword, "Prolog keyword: declare",
         declare_message.c_str());
   }

   if (function_tokens.size() > 1)
   {
      bool function_keyword = function_tokens[1].type not_eq XPathTokenType::IDENTIFIER;
      std::string function_message = "Tokeniser reports 'function' as " +
         std::string(token_type_name(function_tokens[1].type));
      test_assert(function_keyword, "Prolog keyword: function",
         function_message.c_str());
   }

   if (function_tokens.size() > 3)
   {
      bool colon_classified = function_tokens[3].type IS XPathTokenType::COLON;
      test_assert(colon_classified, "QName prefix separator",
         "Colon between prefix and local name should be tokenised as COLON");
   }

   auto variable_block = tokeniser.tokenize("declare variable $value := 1");
   const auto &variable_tokens = variable_block.tokens;
   test_assert(variable_tokens.size() >= 5, "Variable declaration token count",
      "Tokeniser should emit tokens for sample variable declaration");

   if (!variable_tokens.empty())
   {
      bool declare_keyword = variable_tokens[0].type not_eq XPathTokenType::IDENTIFIER;
      std::string declare_message = "Tokeniser reports 'declare' as " +
         std::string(token_type_name(variable_tokens[0].type));
      test_assert(declare_keyword, "Prolog keyword reuse: declare",
         declare_message.c_str());
   }

   if (variable_tokens.size() > 1)
   {
      bool variable_keyword = variable_tokens[1].type not_eq XPathTokenType::IDENTIFIER;
      std::string variable_message = "Tokeniser reports 'variable' as " +
         std::string(token_type_name(variable_tokens[1].type));
      test_assert(variable_keyword, "Prolog keyword: variable",
         variable_message.c_str());
   }

   if (variable_tokens.size() > 4)
   {
      bool assign_token = variable_tokens[4].type IS XPathTokenType::ASSIGN;
      test_assert(assign_token, "Variable assignment operator",
         "':=' should be tokenised as ASSIGN for prolog variables");
   }

   auto namespace_block = tokeniser.tokenize("declare namespace ex = \"http://example.org\"");
   const auto &namespace_tokens = namespace_block.tokens;
   test_assert(namespace_tokens.size() >= 4, "Namespace declaration token count",
      "Tokeniser should emit tokens for namespace declaration");

   if (!namespace_tokens.empty())
   {
      bool declare_keyword = namespace_tokens[0].type not_eq XPathTokenType::IDENTIFIER;
      std::string declare_message = "Tokeniser reports 'declare' as " +
         std::string(token_type_name(namespace_tokens[0].type));
      test_assert(declare_keyword, "Prolog keyword reuse: declare (namespace)",
         declare_message.c_str());
   }

   if (namespace_tokens.size() > 1)
   {
      bool namespace_keyword = namespace_tokens[1].type not_eq XPathTokenType::IDENTIFIER;
      std::string namespace_message = "Tokeniser reports 'namespace' as " +
         std::string(token_type_name(namespace_tokens[1].type));
      test_assert(namespace_keyword, "Prolog keyword: namespace",
         namespace_message.c_str());
   }

   auto external_block = tokeniser.tokenize("declare variable $flag external");
   const auto &external_tokens = external_block.tokens;
   test_assert(external_tokens.size() >= 5, "External variable token count",
      "Tokeniser should emit tokens for external variable declaration");

   if (external_tokens.size() > 4)
   {
      bool external_keyword = external_tokens[4].type not_eq XPathTokenType::IDENTIFIER;
      std::string external_message = "Tokeniser reports 'external' as " +
         std::string(token_type_name(external_tokens[4].type));
      test_assert(external_keyword, "Prolog keyword: external",
         external_message.c_str());
   }
}

//********************************************************************************************************************
// Ensures the parser populates cached operator metadata for recognised unary and binary nodes.

static void test_parser_operator_cache_population()
{
   pf::Log log("OperatorTests");

   XPathTokeniser tokeniser;
   auto token_block = tokeniser.tokenize("1 + 2 * 3 and not(-$flag)");

   XPathParser parser;
   auto compiled = parser.parse(std::move(token_block));

   bool has_expression = compiled.expression not_eq nullptr;
   test_assert(has_expression, "Parser expression availability", "Parser should return an expression tree");
   if (not has_expression) return;

   struct CacheFlags {
      bool plus_cached = false;
      bool multiply_cached = false;
      bool logical_and_cached = false;
      bool unary_not_cached = false;
      bool unary_negate_cached = false;
   } flags;

   auto inspect = [&](auto &&self, const XPathNode *node) -> void {
      if (not node) return;

      if ((node->type IS XQueryNodeType::EXPRESSION) and (node->child_count() > 0)) {
         if (auto *child = node->get_child_safe(0)) self(self, child);
         return;
      }

      if (node->type IS XQueryNodeType::BINARY_OP) {
         auto op_text = node->get_value_view();
         if (op_text IS "+") flags.plus_cached = node->has_cached_binary_kind();
         else if (op_text IS "*") flags.multiply_cached = node->has_cached_binary_kind();
         else if (op_text IS "and") flags.logical_and_cached = node->has_cached_binary_kind();
      }
      else if (node->type IS XQueryNodeType::UNARY_OP) {
         auto op_text = node->get_value_view();
         if (op_text IS "not") flags.unary_not_cached = node->has_cached_unary_kind();
         else if (op_text IS "-") flags.unary_negate_cached = node->has_cached_unary_kind();
      }

      size_t child_total = node->child_count();
      for (size_t index = 0; index < child_total; index++) {
         if (auto *child = node->get_child_safe(index)) self(self, child);
      }
   };

   inspect(inspect, compiled.expression.get());

   test_assert(flags.plus_cached, "Binary operator '+' cache", "Parser should cache addition operator kind");
   test_assert(flags.multiply_cached, "Binary operator '*' cache", "Parser should cache multiplication operator kind");
   test_assert(flags.logical_and_cached, "Binary operator 'and' cache", "Parser should cache logical and operator kind");
   test_assert(flags.unary_not_cached, "Unary operator 'not' cache", "Parser should cache logical not operator kind");
   test_assert(flags.unary_negate_cached, "Unary operator '-' cache", "Parser should cache negation operator kind");
}

static void test_prolog_in_xpath()
{
   pf::Log log("PrologInXPath");

   // Test 1: Check if prolog structure can be accessed
   {
      XQueryProlog prolog;
      XQueryFunction func;
      func.qname = "local:square";
      func.parameter_names.push_back("x");
      prolog.declare_function(std::move(func));

      // Verify the function signature is correct
      auto found = prolog.find_function("local:square", 1);
      bool has_correct_params = false;
      if (found) {
         has_correct_params = (found->parameter_names.size() IS 1) and
                             (found->parameter_names[0] IS "x");
      }

      test_assert(has_correct_params, "Function parameter names",
         "Function should retain parameter names correctly");
   }

   // Test 2: Variable external flag
   {
      XQueryProlog prolog;
      XQueryVariable var;
      var.qname = "external_var";
      var.is_external = true;
      prolog.declare_variable("external_var", std::move(var));

      auto found = prolog.find_variable("external_var");
      test_assert(found and found->is_external, "External variable flag",
         "External variables should be marked correctly");
   }

   // Test 3: Function external flag
   {
      XQueryProlog prolog;
      XQueryFunction func;
      func.qname = "local:external_func";
      func.is_external = true;
      prolog.declare_function(std::move(func));

      auto found = prolog.find_function("local:external_func", 0);
      test_assert(found and found->is_external, "External function flag",
         "External functions should be marked correctly");
   }
}

//********************************************************************************************************************

static void run_unit_tests(int &Passed, int &Total)
{
   pf::Log log("XQueryTests");

   test_count = 0;
   pass_count = 0;
   fail_count = 0;

   test_tokeniser_prolog_keywords();
   test_parser_operator_cache_population();
   test_prolog_api();
   test_prolog_in_xpath();

   Passed = pass_count;
   Total = test_count;
   log.msg("Test Summary: %d of %d tests passed.", Passed, Total);
}
