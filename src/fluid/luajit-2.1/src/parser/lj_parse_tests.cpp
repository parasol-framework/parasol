// Unit tests need to be enabled in the CMakeLists.txt file and then launched from test_unit_tests.fluid

#include <parasol/main.h>

#ifdef ENABLE_UNIT_TESTS
extern void parser_unit_tests(int &Passed, int &Total)
{
   pf::Log log("LuaJITParseTests");


   //Passed += pass_count;
   //Total += test_count;
}
#endif
