/*********************************************************************************************************************

Test for Regex matching

*********************************************************************************************************************/

#include <parasol/startup.h>
#include <parasol/modules/regex.h>
#include <format>

JUMPTABLE_REGEX
static OBJECTPTR modRegex;

using namespace pf;

CSTRING ProgName = "RegexMatch";

//********************************************************************************************************************

static void log_success(std::string_view TestName)
{
   printf("  ✓ %.*s: PASSED\n", int(TestName.size()), TestName.data());
}

static void log_fail(std::string_view TestName, std::string_view Reason)
{
   printf("  ✗ %.*s: FAILED - %.*s\n", int(TestName.size()), TestName.data(), int(Reason.size()), Reason.data());
}

//********************************************************************************************************************

struct TestContext {
   bool match_found = false;
   int capture_count = 0;
   std::string captured;
   std::vector<std::string_view> captures;
   std::string prefix_str;
   std::string suffix_str;
};

static ERR match_callback(std::vector<std::string_view> &Captures, std::string_view Prefix, std::string_view Suffix, TestContext &Ctx)
{
   Ctx.match_found = true;
   Ctx.capture_count = Captures.size();
   Ctx.captures = Captures;
   Ctx.prefix_str = Prefix;
   Ctx.suffix_str = Suffix;
   return ERR::Okay;
}

static ERR search_callback(int Index, std::vector<std::string_view> &Captures, std::string_view Prefix, std::string_view Suffix, TestContext &Ctx)
{
   Ctx.match_found = true;
   Ctx.capture_count++;
   Ctx.captures = Captures;
   Ctx.prefix_str = Prefix;
   Ctx.suffix_str = Suffix;
   return ERR::Okay;
}

//********************************************************************************************************************

static void test_basic_single_character(int &TotalTests, int &PassedTests)
{
   TotalTests++;
   printf("Test 1: Basic single character match\n");
   Regex *regex;
   if (rx::Compile("a", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
      TestContext ctx;
      auto callback = C_FUNCTION(&match_callback, &ctx);

      if (auto error = rx::Match(regex, "apple", RMATCH::NIL, &callback); error IS ERR::Okay) {
         if (not ctx.match_found) {
            log_success("No single character match");
            PassedTests++;
         }
         else log_fail(__FUNCTION__, "Match found");
      }
      else if (error IS ERR::Search) {
         log_success("No single character match");
         PassedTests++;
      }
      else log_fail(__FUNCTION__, std::format("Match returned error {}", GetErrorMsg(error)));

      FreeResource(regex);
   }
   else log_fail(__FUNCTION__, "Could not compile regex");
}

//********************************************************************************************************************

static void test_capture_groups(int &TotalTests, int &PassedTests)
{
   TotalTests++;
   printf("\nTest 2: Capture groups\n");
   Regex *regex;
   if (rx::Compile("(\\w+)@(\\w+)\\.(\\w+)", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
      TestContext ctx;
      auto callback = C_FUNCTION(&match_callback, &ctx);

      if (rx::Match(regex, "user@example.com", RMATCH::NIL, &callback) IS ERR::Okay) {
         if (ctx.capture_count IS 4) {
            // Expecting: "user@example.com", "user", "example", "com"
            log_success("Capture groups");
            PassedTests++;
         }
         else log_fail(__FUNCTION__, std::format("Expected 4 captures, got {}", ctx.capture_count));
      }
      else log_fail(__FUNCTION__, "Match returned error");

      FreeResource(regex);
   }
   else log_fail(__FUNCTION__, "Could not compile regex");
}

//********************************************************************************************************************

static void test_case_insensitive(int &TotalTests, int &PassedTests)
{
   TotalTests++;
   printf("\nTest 3: Case insensitive match\n");
   Regex *regex;
   if (rx::Compile("hello", REGEX::ICASE, nullptr, &regex) IS ERR::Okay) {
      TestContext ctx;
      auto callback = C_FUNCTION(&search_callback, &ctx);

      if (rx::Search(regex, "HELLO WORLD", RMATCH::NIL, &callback) IS ERR::Okay) {
         if (ctx.match_found) {
            log_success("Case insensitive match");
            PassedTests++;
         }
         else log_fail(__FUNCTION__, "No match found");
      }
      else log_fail(__FUNCTION__, "Match returned error");

      FreeResource(regex);
   }
   else log_fail(__FUNCTION__, "Could not compile regex");
}

//********************************************************************************************************************

static void test_no_match_scenario(int &TotalTests, int &PassedTests)
{
   TotalTests++;
   printf("\nTest 4: No match scenario\n");
   Regex *regex;
   if (rx::Compile("xyz", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
      TestContext ctx;
      auto callback = C_FUNCTION(&match_callback, &ctx);

      if (rx::Match(regex, "abc def", RMATCH::NIL, &callback) IS ERR::Search) {
         log_success("No match scenario");
         PassedTests++;
      }
      else log_fail(__FUNCTION__, "Should have returned ERR::Search");

      FreeResource(regex);
   }
   else log_fail(__FUNCTION__, "Could not compile regex");
}

//********************************************************************************************************************

static void test_digit_matching(int &TotalTests, int &PassedTests)
{
   TotalTests++;
   printf("\nTest 5: Digit pattern matching\n");
   Regex *regex;
   if (rx::Compile("\\d+", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
      TestContext ctx;
      auto callback = C_FUNCTION(&search_callback, &ctx);

      if (rx::Search(regex, "abc123def", RMATCH::NIL, &callback) IS ERR::Okay) {
         if ((ctx.match_found) and (ctx.captures[0] IS "123")) {
            log_success("Digit pattern matching");
            PassedTests++;
         }
         else log_fail(__FUNCTION__, "Expected '123', got different result");
      }
      else log_fail(__FUNCTION__, "Match returned error");

      FreeResource(regex);
   }
   else log_fail(__FUNCTION__, "Could not compile regex");
}

//********************************************************************************************************************

static void test_word_boundaries(int &TotalTests, int &PassedTests)
{
   TotalTests++;
   printf("\nTest 6: Word boundary matching\n");
   Regex *regex;
   if (rx::Compile("\\bword\\b", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
      TestContext ctx;
      auto callback = C_FUNCTION(&search_callback, &ctx);

      if (rx::Search(regex, "a word here", RMATCH::NIL, &callback) IS ERR::Okay) {
         if (ctx.match_found) {
            log_success("Word boundary matching");
            PassedTests++;
         }
         else log_fail(__FUNCTION__, "No match found");
      }
      else log_fail(__FUNCTION__, "Match returned error");

      FreeResource(regex);
   }
   else log_fail(__FUNCTION__, "Could not compile regex");
}

//********************************************************************************************************************

static void test_prefix_suffix(int &TotalTests, int &PassedTests)
{
   TotalTests++;
   printf("\nTest 7: Prefix and suffix extraction\n");
   Regex *regex;
   if (rx::Compile("middle", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
      TestContext ctx;
      auto callback = C_FUNCTION(&search_callback, &ctx);

      if (rx::Search(regex, "start middle end", RMATCH::NIL, &callback) IS ERR::Okay) {
         if ((ctx.prefix_str IS "start ") and (ctx.suffix_str IS " end")) {
            log_success("Prefix and suffix extraction");
            PassedTests++;
         }
         else log_fail(__FUNCTION__, "Unexpected prefix or suffix values");
      }
      else log_fail(__FUNCTION__, "Match returned error");

      FreeResource(regex);
   }
   else log_fail(__FUNCTION__, "Could not compile regex");
}

//********************************************************************************************************************

static void test_empty_pattern(int &TotalTests, int &PassedTests)
{
   TotalTests++;
   printf("\nTest 8: Empty pattern\n");
   Regex *regex;
   if (rx::Compile("", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
      TestContext ctx;
      auto callback = C_FUNCTION(&match_callback, &ctx);

      if (auto error = rx::Match(regex, "anything", RMATCH::NIL, &callback); error IS ERR::Okay) {
         log_fail(__FUNCTION__, "Expected match failure");
      }
      else if (error != ERR::Search) log_fail(__FUNCTION__, std::format("Match returned error: {}", GetErrorMsg(error)));
      else {
         log_success("Empty pattern");
         PassedTests++;
      }

      FreeResource(regex);
   }
   else log_fail(__FUNCTION__, "Could not compile regex");
}

//********************************************************************************************************************

static void test_complex_email(int &TotalTests, int &PassedTests)
{
   TotalTests++;
   printf("\nTest 9: Complex email pattern\n");
   Regex *regex;
   if (rx::Compile("^([\\w._%+-]+)@([\\w.-]+)\\.([A-Za-z]{2,})$", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
      TestContext ctx;
      auto callback = C_FUNCTION(&match_callback, &ctx);

      if (rx::Match(regex, "test.user+tag@example.co.uk", RMATCH::NIL, &callback) IS ERR::Okay) {
         if ((ctx.capture_count IS 4) and (ctx.captures[1] IS "test.user+tag") and (ctx.captures[2] IS "example.co") and (ctx.captures[3] IS "uk")) {
            log_success("Complex email pattern");
            PassedTests++;
         }
         else log_fail(__FUNCTION__, "Capture validation failed");
      }
      else log_fail(__FUNCTION__, "Match returned error");

      FreeResource(regex);
   }
   else log_fail(__FUNCTION__, "Could not compile regex");
}

//********************************************************************************************************************

static void test_multiline_mode(int &TotalTests, int &PassedTests)
{
   TotalTests++;
   printf("\nTest 10: Multiline mode\n");
   Regex *regex;
   if (rx::Compile("^line", REGEX::MULTILINE, nullptr, &regex) IS ERR::Okay) {
      TestContext ctx;
      auto callback = C_FUNCTION(&search_callback, &ctx);

      if (rx::Search(regex, "first\nline two", RMATCH::NIL, &callback) IS ERR::Okay) {
         if (ctx.match_found) {
            log_success("Multiline mode");
            PassedTests++;
         }
         else log_fail(__FUNCTION__, "No match found");
      }
      else log_fail(__FUNCTION__, "Match returned error");

      FreeResource(regex);
   }
   else log_fail(__FUNCTION__, "Could not compile regex");
}

//********************************************************************************************************************

static void test_not_begin_of_line(int &TotalTests, int &PassedTests)
{
   TotalTests++;
   printf("\nTest 11: RMATCH::NOT_BEGIN_OF_LINE flag\n");
   Regex *regex;
   if (rx::Compile("^hello", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
      TestContext ctx;
      auto callback = C_FUNCTION(&match_callback, &ctx);

      if (rx::Match(regex, "hello world", RMATCH::NOT_BEGIN_OF_LINE, &callback) IS ERR::Search) {
         log_success("NOT_BEGIN_OF_LINE flag");
         PassedTests++;
      }
      else log_fail(__FUNCTION__, "Should not have matched");

      FreeResource(regex);
   }
   else log_fail(__FUNCTION__, "Could not compile regex");
}

//********************************************************************************************************************

static void test_nested_capture_groups(int &TotalTests, int &PassedTests)
{
   TotalTests++;
   printf("\nTest 12: Nested capture groups\n");
   Regex *regex;
   if (rx::Compile("((\\w+)-(\\d+))", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
      TestContext ctx;
      auto callback = C_FUNCTION(&match_callback, &ctx);

      if (rx::Match(regex, "item-42", RMATCH::NIL, &callback) IS ERR::Okay) {
         if ((ctx.capture_count IS 4) and (ctx.captures[1] IS "item-42") and (ctx.captures[2] IS "item") and (ctx.captures[3] IS "42")) {
            log_success("Nested capture groups");
            PassedTests++;
         }
         else log_fail(__FUNCTION__, "Capture validation failed");
      }
      else log_fail(__FUNCTION__, "Match returned error");

      FreeResource(regex);
   }
   else log_fail(__FUNCTION__, "Could not compile regex");
}

//********************************************************************************************************************

static void test_unicode_support(int &TotalTests, int &PassedTests)
{
   TotalTests++;
   printf("\nTest 13: Unicode text matching\n");
   Regex *regex;
   if (rx::Compile("café", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
      TestContext ctx;
      auto callback = C_FUNCTION(&search_callback, &ctx);

      if (rx::Search(regex, "I love café au lait", RMATCH::NIL, &callback) IS ERR::Okay) {
         if (ctx.match_found) {
            log_success("Unicode text matching");
            PassedTests++;
         }
         else log_fail(__FUNCTION__, "No match found");
      }
      else log_fail(__FUNCTION__, "Match returned error");

      FreeResource(regex);
   }
   else log_fail(__FUNCTION__, "Could not compile regex");
}

//********************************************************************************************************************

static void test_optional_groups(int &TotalTests, int &PassedTests)
{
   TotalTests++;
   printf("\nTest 14: Optional capture groups\n");
   Regex *regex;
   if (rx::Compile("(\\d+)(\\.\\d+)?", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
      TestContext ctx;
      auto callback = C_FUNCTION(&match_callback, &ctx);

      if (rx::Match(regex, "42", RMATCH::NIL, &callback) IS ERR::Okay) {
         if (ctx.capture_count >= 2) {
            log_success("Optional capture groups");
            PassedTests++;
         }
         else log_fail(__FUNCTION__, "Expected at least 2 captures");
      }
      else log_fail(__FUNCTION__, "Match returned error");

      FreeResource(regex);
   }
   else log_fail(__FUNCTION__, "Could not compile regex");
}

//********************************************************************************************************************

static void test_null_pointer_handling(int &TotalTests, int &PassedTests)
{
   TotalTests++;
   printf("\nTest 15: Null regex pointer handling\n");
   if (rx::Match(nullptr, "test", RMATCH::NIL, nullptr) IS ERR::NullArgs) {
      log_success("Null pointer handling");
      PassedTests++;
   }
   else log_fail(__FUNCTION__, "Should have returned ERR::NullArgs");
}

//********************************************************************************************************************

int main(int argc, CSTRING *argv)
{
   pf::Log log;

   if (auto msg = init_parasol(argc, argv)) {
      log.error("%s", msg);
      return -1;
   }

   if (objModule::load("regex", &modRegex, &RegexBase) != ERR::Okay) return -1;

   printf("=== Regex Match Test Suite ===\n\n");

   int total_tests = 0;
   int passed_tests = 0;

   test_basic_single_character(total_tests, passed_tests);
   test_capture_groups(total_tests, passed_tests);
   test_case_insensitive(total_tests, passed_tests);
   test_no_match_scenario(total_tests, passed_tests);
   test_digit_matching(total_tests, passed_tests);
   test_word_boundaries(total_tests, passed_tests);
   test_prefix_suffix(total_tests, passed_tests);
   test_empty_pattern(total_tests, passed_tests);
   test_complex_email(total_tests, passed_tests);
   test_multiline_mode(total_tests, passed_tests);
   test_not_begin_of_line(total_tests, passed_tests);
   test_nested_capture_groups(total_tests, passed_tests);
   test_unicode_support(total_tests, passed_tests);
   test_optional_groups(total_tests, passed_tests);
   test_null_pointer_handling(total_tests, passed_tests);

   printf("\n=== Test Summary ===\n");
   printf("Passed: %d/%d tests\n", passed_tests, total_tests);

   int result;
   if (passed_tests IS total_tests) {
      printf("✓ All tests PASSED!\n");
      result = 0;
   }
   else {
      printf("✗ Some tests FAILED!\n");
      result = -1;
   }

   printf("\n=== Test Complete ===\n");

   FreeResource(modRegex);
   return result;
}