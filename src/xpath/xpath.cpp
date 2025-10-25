/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-MODULE-
XPath: Provides XPath 2.0 and XQuery 1.0+ support for the XML module.

The XPath module provides comprehensive support for XPath 2.0 and XQuery languages, enabling powerful querying and
navigation of XML documents.  It provides the @XQuery class as its primary interface, and operates in conjunction 
with the @XML class to provide a standards-compliant query engine with extensive functionality.

-END-

*********************************************************************************************************************/

#include <parasol/modules/regex.h>
#include "../link/unicode.h"
#include "../xml/uri_utils.h"
#include "functions/accessor_support.h"

JUMPTABLE_CORE
JUMPTABLE_REGEX

#include "xpath_def.c"

static OBJECTPTR glContext = nullptr;
static OBJECTPTR modRegex = nullptr;
static OBJECTPTR clXQuery = nullptr;

static ERR add_xquery_class(void);

//*********************************************************************************************************************
// Dynamic loader for the Regex functionality.  We only load it as needed due to the size of the module.

extern "C" ERR load_regex(void)
{
#ifndef PARASOL_STATIC
   if (not modRegex) {
      pf::SwitchContext ctx(glContext);
      if (objModule::load("regex", &modRegex, &RegexBase) != ERR::Okay) return ERR::InitModule;
   }
#endif
   return ERR::Okay;
}

//********************************************************************************************************************

#ifdef ENABLE_UNIT_TESTS
#include "unit_tests.cpp"
#endif

//********************************************************************************************************************

static ERR MODInit(OBJECTPTR pModule, struct CoreBase *pCore)
{
   CoreBase = pCore;
   glContext = CurrentContext();
   return add_xquery_class();
}

static ERR MODOpen(OBJECTPTR Module)
{
   Module->set(FID_FunctionList, glFunctions);
   return ERR::Okay;
}

static ERR MODExpunge(void)
{
   if (clXQuery) { FreeResource(clXQuery); clXQuery = nullptr; }
   if (modRegex) { FreeResource(modRegex); modRegex = nullptr; }
   return ERR::Okay;
}

namespace xp {

/*********************************************************************************************************************

-FUNCTION-
UnitTest: Private function for internal unit testing of the XPath module.

Private function for internal unit testing of the XPath module.

-INPUT-
ptr Meta: Optional pointer meaningful to the test functions.

-ERRORS-
Okay: All tests passed.
Failed: One or more tests failed.

-END-

*********************************************************************************************************************/

ERR UnitTest(APTR Meta)
{
#ifdef ENABLE_UNIT_TESTS
   return run_unit_tests(Meta);
#else
   return ERR::Okay;
#endif
}

} // namespace xp

#include "xquery_class.cpp"

//********************************************************************************************************************

static STRUCTS glStructures = {
};

//********************************************************************************************************************

PARASOL_MOD(MODInit, nullptr, MODOpen, MODExpunge, MOD_IDL, &glStructures)
extern "C" struct ModHeader * register_xpath_module() { return &ModHeader; }
