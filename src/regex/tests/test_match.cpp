/*********************************************************************************************************************

Test for Regex matching

*********************************************************************************************************************/

#include <parasol/startup.h>
#include <parasol/modules/regex.h>

JUMPTABLE_REGEX
static OBJECTPTR modRegex;

using namespace pf;

CSTRING ProgName = "RegexMatch";

//********************************************************************************************************************

static void log_success(CSTRING TestName)
{
   printf("  ✓ %s: PASSED\n", TestName);
}

static void log_fail(CSTRING TestName, CSTRING Reason)
{
   printf("  ✗ %s: FAILED - %s\n", TestName, Reason);
}

//********************************************************************************************************************

struct TestContext {
   bool match_found = false;
   int capture_count = 0;
   std::string captured;
   std::vector<std::string> captures;
   std::string prefix_str;
   std::string suffix_str;
};

static ERR test_callback(int Index, std::string_view Capture, std::string_view Prefix, std::string_view Suffix, TestContext &Ctx)
{
   Ctx.match_found = true;
   Ctx.capture_count++;
   if (Index IS 0) {
      Ctx.captured = std::string(Capture);
      Ctx.prefix_str = std::string(Prefix);
      Ctx.suffix_str = std::string(Suffix);
   }
   Ctx.captures.push_back(std::string(Capture));
   return ERR::Okay;
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

   // Test 1: Basic match - single character
   {
      total_tests++;
      printf("Test 1: Basic single character match\n");
      Regex *regex;
      if (rx::Compile("a", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
         TestContext ctx;
         auto callback = C_FUNCTION(&test_callback, &ctx);

         if (rx::Match(regex, "apple", RMATCH::NIL, &callback) IS ERR::Okay) {
            if (ctx.match_found) {
               log_success("Single character match");
               passed_tests++;
            }
            else log_fail("Single character match", "No match found");
         }
         else log_fail("Single character match", "Match returned error");

         FreeResource(regex);
      }
      else log_fail("Single character match", "Could not compile regex");
   }

   // Test 2: Match with capture groups
   {
      total_tests++;
      printf("\nTest 2: Capture groups\n");
      Regex *regex;
      if (rx::Compile("(\\w+)@(\\w+)\\.(\\w+)", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
         TestContext ctx;
         auto callback = C_FUNCTION(&test_callback, &ctx);

         if (rx::Match(regex, "user@example.com", RMATCH::NIL, &callback) IS ERR::Okay) {
            if (ctx.capture_count IS 4) {
               log_success("Capture groups");
               passed_tests++;
            }
            else log_fail("Capture groups", "Expected 4 captures, got different count");
         }
         else log_fail("Capture groups", "Match returned error");

         FreeResource(regex);
      }
      else log_fail("Capture groups", "Could not compile regex");
   }

   // Test 3: Case insensitive matching
   {
      total_tests++;
      printf("\nTest 3: Case insensitive match\n");
      Regex *regex;
      if (rx::Compile("hello", REGEX::ICASE, nullptr, &regex) IS ERR::Okay) {
         TestContext ctx;
         auto callback = C_FUNCTION(&test_callback, &ctx);

         if (rx::Match(regex, "HELLO WORLD", RMATCH::NIL, &callback) IS ERR::Okay) {
            if (ctx.match_found) {
               log_success("Case insensitive match");
               passed_tests++;
            }
            else log_fail("Case insensitive match", "No match found");
         }
         else log_fail("Case insensitive match", "Match returned error");

         FreeResource(regex);
      }
      else log_fail("Case insensitive match", "Could not compile regex");
   }

   // Test 4: No match scenario
   {
      total_tests++;
      printf("\nTest 4: No match scenario\n");
      Regex *regex;
      if (rx::Compile("xyz", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
         TestContext ctx;
         auto callback = C_FUNCTION(&test_callback, &ctx);

         if (rx::Match(regex, "abc def", RMATCH::NIL, &callback) IS ERR::Search) {
            log_success("No match scenario");
            passed_tests++;
         }
         else log_fail("No match scenario", "Should have returned ERR::Search");

         FreeResource(regex);
      }
      else log_fail("No match scenario", "Could not compile regex");
   }

   // Test 5: Digit matching
   {
      total_tests++;
      printf("\nTest 5: Digit pattern matching\n");
      Regex *regex;
      if (rx::Compile("\\d+", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
         TestContext ctx;
         auto callback = C_FUNCTION(&test_callback, &ctx);

         if (rx::Match(regex, "abc123def", RMATCH::NIL, &callback) IS ERR::Okay) {
            if ((ctx.match_found) and (ctx.captured IS "123")) {
               log_success("Digit pattern matching");
               passed_tests++;
            }
            else log_fail("Digit pattern matching", "Expected '123', got different result");
         }
         else log_fail("Digit pattern matching", "Match returned error");

         FreeResource(regex);
      }
      else log_fail("Digit pattern matching", "Could not compile regex");
   }

   // Test 6: Word boundaries
   {
      total_tests++;
      printf("\nTest 6: Word boundary matching\n");
      Regex *regex;
      if (rx::Compile("\\bword\\b", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
         TestContext ctx;
         auto callback = C_FUNCTION(&test_callback, &ctx);

         if (rx::Match(regex, "a word here", RMATCH::NIL, &callback) IS ERR::Okay) {
            if (ctx.match_found) {
               log_success("Word boundary matching");
               passed_tests++;
            }
            else log_fail("Word boundary matching", "No match found");
         }
         else log_fail("Word boundary matching", "Match returned error");

         FreeResource(regex);
      }
      else log_fail("Word boundary matching", "Could not compile regex");
   }

   // Test 7: Prefix and suffix extraction
   {
      total_tests++;
      printf("\nTest 7: Prefix and suffix extraction\n");
      Regex *regex;
      if (rx::Compile("middle", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
         TestContext ctx;
         auto callback = C_FUNCTION(&test_callback, &ctx);

         if (rx::Match(regex, "start middle end", RMATCH::NIL, &callback) IS ERR::Okay) {
            if ((ctx.prefix_str IS "start ") and (ctx.suffix_str IS " end")) {
               log_success("Prefix and suffix extraction");
               passed_tests++;
            }
            else log_fail("Prefix and suffix extraction", "Unexpected prefix or suffix values");
         }
         else log_fail("Prefix and suffix extraction", "Match returned error");

         FreeResource(regex);
      }
      else log_fail("Prefix and suffix extraction", "Could not compile regex");
   }

   // Test 8: Empty pattern
   {
      total_tests++;
      printf("\nTest 8: Empty pattern\n");
      Regex *regex;
      if (rx::Compile("", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
         TestContext ctx;
         auto callback = C_FUNCTION(&test_callback, &ctx);

         if (rx::Match(regex, "anything", RMATCH::NIL, &callback) IS ERR::Okay) {
            if (ctx.match_found) {
               log_success("Empty pattern");
               passed_tests++;
            }
            else log_fail("Empty pattern", "No match found");
         }
         else log_fail("Empty pattern", "Match returned error");

         FreeResource(regex);
      }
      else log_fail("Empty pattern", "Could not compile regex");
   }

   // Test 9: Complex email pattern
   {
      total_tests++;
      printf("\nTest 9: Complex email pattern\n");
      Regex *regex;
      if (rx::Compile("^([\\w._%+-]+)@([\\w.-]+)\\.([A-Za-z]{2,})$", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
         TestContext ctx;
         auto callback = C_FUNCTION(&test_callback, &ctx);

         if (rx::Match(regex, "test.user+tag@example.co.uk", RMATCH::NIL, &callback) IS ERR::Okay) {
            if ((ctx.capture_count IS 4) and (ctx.captures[1] IS "test.user+tag") and (ctx.captures[2] IS "example.co") and (ctx.captures[3] IS "uk")) {
               log_success("Complex email pattern");
               passed_tests++;
            }
            else log_fail("Complex email pattern", "Capture validation failed");
         }
         else log_fail("Complex email pattern", "Match returned error");

         FreeResource(regex);
      }
      else log_fail("Complex email pattern", "Could not compile regex");
   }

   // Test 10: Multiline mode
   {
      total_tests++;
      printf("\nTest 10: Multiline mode\n");
      Regex *regex;
      if (rx::Compile("^line", REGEX::MULTILINE, nullptr, &regex) IS ERR::Okay) {
         TestContext ctx;
         auto callback = C_FUNCTION(&test_callback, &ctx);

         if (rx::Match(regex, "first\nline two", RMATCH::NIL, &callback) IS ERR::Okay) {
            if (ctx.match_found) {
               log_success("Multiline mode");
               passed_tests++;
            }
            else log_fail("Multiline mode", "No match found");
         }
         else log_fail("Multiline mode", "Match returned error");

         FreeResource(regex);
      }
      else log_fail("Multiline mode", "Could not compile regex");
   }

   // Test 11: Match flags - NOT_BEGIN_OF_LINE
   {
      total_tests++;
      printf("\nTest 11: RMATCH::NOT_BEGIN_OF_LINE flag\n");
      Regex *regex;
      if (rx::Compile("^hello", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
         TestContext ctx;
         auto callback = C_FUNCTION(&test_callback, &ctx);

         if (rx::Match(regex, "hello world", RMATCH::NOT_BEGIN_OF_LINE, &callback) IS ERR::Search) {
            log_success("NOT_BEGIN_OF_LINE flag");
            passed_tests++;
         }
         else log_fail("NOT_BEGIN_OF_LINE flag", "Should not have matched");

         FreeResource(regex);
      }
      else log_fail("NOT_BEGIN_OF_LINE flag", "Could not compile regex");
   }

   // Test 12: Multiple nested capture groups
   {
      total_tests++;
      printf("\nTest 12: Nested capture groups\n");
      Regex *regex;
      if (rx::Compile("((\\w+)-(\\d+))", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
         TestContext ctx;
         auto callback = C_FUNCTION(&test_callback, &ctx);

         if (rx::Match(regex, "item-42", RMATCH::NIL, &callback) IS ERR::Okay) {
            if ((ctx.capture_count IS 4) and (ctx.captures[1] IS "item-42") and (ctx.captures[2] IS "item") and (ctx.captures[3] IS "42")) {
               log_success("Nested capture groups");
               passed_tests++;
            }
            else log_fail("Nested capture groups", "Capture validation failed");
         }
         else log_fail("Nested capture groups", "Match returned error");

         FreeResource(regex);
      }
      else log_fail("Nested capture groups", "Could not compile regex");
   }

   // Test 13: Unicode support
   {
      total_tests++;
      printf("\nTest 13: Unicode text matching\n");
      Regex *regex;
      if (rx::Compile("café", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
         TestContext ctx;
         auto callback = C_FUNCTION(&test_callback, &ctx);

         if (rx::Match(regex, "I love café au lait", RMATCH::NIL, &callback) IS ERR::Okay) {
            if (ctx.match_found) {
               log_success("Unicode text matching");
               passed_tests++;
            }
            else log_fail("Unicode text matching", "No match found");
         }
         else log_fail("Unicode text matching", "Match returned error");

         FreeResource(regex);
      }
      else log_fail("Unicode text matching", "Could not compile regex");
   }

   // Test 14: Optional groups
   {
      total_tests++;
      printf("\nTest 14: Optional capture groups\n");
      Regex *regex;
      if (rx::Compile("(\\d+)(\\.\\d+)?", REGEX::NIL, nullptr, &regex) IS ERR::Okay) {
         TestContext ctx;
         auto callback = C_FUNCTION(&test_callback, &ctx);

         if (rx::Match(regex, "42", RMATCH::NIL, &callback) IS ERR::Okay) {
            if (ctx.capture_count >= 2) {
               log_success("Optional capture groups");
               passed_tests++;
            }
            else log_fail("Optional capture groups", "Expected at least 2 captures");
         }
         else log_fail("Optional capture groups", "Match returned error");

         FreeResource(regex);
      }
      else log_fail("Optional capture groups", "Could not compile regex");
   }

   // Test 15: Null pointer handling
   {
      total_tests++;
      printf("\nTest 15: Null regex pointer handling\n");
      if (rx::Match(nullptr, "test", RMATCH::NIL, nullptr) IS ERR::NullArgs) {
         log_success("Null pointer handling");
         passed_tests++;
      }
      else log_fail("Null pointer handling", "Should have returned ERR::NullArgs");
   }

   printf("\n=== Test Summary ===\n");
   printf("Passed: %d/%d tests\n", passed_tests, total_tests);
   if (passed_tests IS total_tests) {
      printf("✓ All tests PASSED!\n");
   }
   else {
      printf("✗ Some tests FAILED!\n");
   }

   printf("\n=== Test Complete ===\n");

   FreeResource(modRegex);
   return 0;
}