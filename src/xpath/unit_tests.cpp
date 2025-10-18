// XPath Module Unit Tests
// This file contains compiled-in unit tests for the XPath module, primarily for debugging prolog integration.
// Unit tests need to be enabled in the CMakeLists.txt file and then launched from test_unit_tests.fluid

#include "xpath.h"
#include "api/xquery_prolog.h"
#include "parse/xpath_tokeniser.h"
#include "../xml/xml.h"

#include <iostream>
#include <sstream>
#include <string>

//********************************************************************************************************************
// Test helper functions

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

static void test_assert(bool Condition, const char *TestName, const char *Message) {
   test_count++;
   if (Condition) {
      pass_count++;
      std::cout << "PASS: " << TestName << std::endl;
   }
   else {
      fail_count++;
      std::cout << "FAIL: " << TestName << " - " << Message << std::endl;
   }
}

//********************************************************************************************************************
// XQueryProlog API Tests

static void test_prolog_api() {
   std::cout << "\n--- Testing XQueryProlog API ---\n" << std::endl;

   // Test 1: Create empty prolog
   {
      XQueryProlog prolog;
      test_assert(prolog.functions.empty(), "Empty prolog creation",
         "New prolog should have no functions");
   }

   // Test 2: Declare a function
   {
      XQueryProlog prolog;
      XQueryFunction func;
      func.qname = "local:test";
      func.parameter_names.push_back("x");
      prolog.declare_function(std::move(func));

      auto found = prolog.find_function("local:test", 1);
      test_assert(found not_eq nullptr, "Function declaration",
         "Declared function should be findable");
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

   // Test 5: Namespace declaration
   {
      XQueryProlog prolog;
      prolog.declare_namespace("ex", "http://example.org", nullptr);

      bool has_namespace = prolog.declared_namespaces.find("ex") not_eq prolog.declared_namespaces.end();
      test_assert(has_namespace, "Namespace declaration",
         "Declared namespace should be in prolog");
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
   std::cout << "\n--- Investigating Prolog Tokeniser Coverage ---\n" << std::endl;

   // Progress marker (2024-10-17): capturing current behaviour before adding DECLARE/FUNCTION/VARIABLE tokens.

   XPathTokeniser tokeniser;

   auto function_tokens = tokeniser.tokenize("declare function local:square($x) { $x * $x }");
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

   auto variable_tokens = tokeniser.tokenize("declare variable $value := 1");
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

   auto namespace_tokens = tokeniser.tokenize("declare namespace ex = \"http://example.org\"");
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

   auto external_tokens = tokeniser.tokenize("declare variable $flag external");
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

static void test_prolog_in_xpath() {
   std::cout << "\n--- Testing Prolog Integration ---\n" << std::endl;

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

   // Test 4: Base URI inheritance during compilation
   {
      extXML xml;
      xml.Path = const_cast<char *>("file:///sample\\doc.xml");

      XPathNode *compiled = nullptr;
      ERR error = ::xp::Compile(&xml, "1", (APTR *)&compiled);
      bool success = (error IS ERR::Okay) and (compiled not_eq nullptr);
      bool inherited = false;

      if (success)
      {
         auto prolog_ptr = compiled->get_prolog();
         if (prolog_ptr)
         {
            inherited = prolog_ptr->static_base_uri IS std::string("file:///sample/doc.xml");
         }
         FreeResource(compiled);
      }

      test_assert(success and inherited, "Prolog base URI inheritance",
         "Compiled prolog should inherit and normalise document base URI");
   }
}

//********************************************************************************************************************

static ERR run_unit_tests(APTR Meta)
{
   test_count = 0;
   pass_count = 0;
   fail_count = 0;

   test_tokeniser_prolog_keywords();
   test_prolog_api();
   test_prolog_in_xpath();

   std::cout << "\n=== Test Summary ===" << std::endl;
   std::cout << "Total:  " << test_count << std::endl;
   std::cout << "Passed: " << pass_count << std::endl;
   std::cout << "Failed: " << fail_count << std::endl;
   std::cout << "===================" << std::endl;

   return (fail_count IS 0) ? ERR::Okay : ERR::Failed;
}
