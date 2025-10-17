/*********************************************************************************************************************

XPath Module Unit Test Runner

This test calls the xp::UnitTest() function which runs compiled-in unit tests for the XPath module.

*********************************************************************************************************************/

#undef PRV_XPATH
#undef PRV_XPATH_MODULE

#include <parasol/startup.h>
#include <parasol/modules/xpath.h>

JUMPTABLE_XPATH
static OBJECTPTR modXPath;

using namespace pf;
CSTRING ProgName = "XPathUnitTest";

//********************************************************************************************************************

int main(int argc, CSTRING *argv)
{
   pf::Log log;

   if (auto msg = init_parasol(argc, argv)) {
      printf("%s (check you have installed and are running this program from the install folder)", msg);
      return -1;
   }

   if (objModule::load("xpath", &modXPath, &XPathBase) not_eq ERR::Okay) {
      printf("Failed to load XPath module\n");
      return -1;
   }

   xp::UnitTest(nullptr);

   FreeResource(modXPath);
   close_parasol();
   return 0;
}
