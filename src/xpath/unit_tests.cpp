// XPath Module Unit Tests
// This file contains compiled-in unit tests for the XPath module, primarily for debugging prolog integration.

#include <parasol/main.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/xpath.h>
#include "xpath.h"
#include "api/xquery_prolog.h"

#include <iostream>
#include <sstream>

//********************************************************************************************************************
// Test helper functions

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

static void reset_test_counters() {
   test_count = 0;
   pass_count = 0;
   fail_count = 0;
}

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

static void print_test_summary() {
   std::cout << "\n=== Test Summary ===" << std::endl;
   std::cout << "Total:  " << test_count << std::endl;
   std::cout << "Passed: " << pass_count << std::endl;
   std::cout << "Failed: " << fail_count << std::endl;
   std::cout << "===================" << std::endl;
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
}

/*********************************************************************************************************************

-FUNCTION-
UnitTest: Private function for internal unit testing of the XPath module.

Private function for internal unit testing of the XPath module.

-INPUT-
ptr Meta: Optional pointer meaningful to the test functions.

-END-

*********************************************************************************************************************/

namespace xp {

void UnitTest(APTR Meta)
{
   std::cout << "\n========================================" << std::endl;
   std::cout << "XPath Module Unit Tests" << std::endl;
   std::cout << "========================================" << std::endl;

   reset_test_counters();

   // Run test suites
   test_prolog_api();
   test_prolog_in_xpath();

   // Print summary
   print_test_summary();
}

}
