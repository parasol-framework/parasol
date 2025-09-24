//********************************************************************************************************************
// Final AST XPath evaluator
// The legacy string-based evaluator has been retired after completing the staged
// migration recorded in AST_PLAN.md. All queries now execute through the
// AST-driven evaluator implemented in xpath/xpath_evaluator.cpp.
//********************************************************************************************************************
// XPath Query
//
// [0-9]  Used for indexing
// '*'    For wild-carding of tag names
// '@'    An attribute
// '..'   Parent
// [=...] Match on encapsulated content (Not an XPath standard but we support it)
// //     Double-slash enables deep scanning of the XML tree.
//
// Round brackets may also be used as an alternative to square brackets.
//
// The use of \ as an escape character in attribute strings is supported, but keep in mind that this is not an official
// feature of the XPath standard.
//
// Examples:
//   /menu/submenu
//   /menu[2]/window
//   /menu/window/@title
//   /menu/window[@title='foo']/...
//   /menu[=contentmatch]
//   /menu//window
//   /menu/window/* (First child of the window tag)
//   /menu/*[@id='5']

#include <memory>

#include "xpath/xpath_ast.h"
#include "xpath/xpath_functions.h"
#include "xpath/xpath_axis.h"
#include "xpath/xpath_parser.h"
#include "xpath/xpath_evaluator.h"

#include "xpath/xpath_ast.cpp"
#include "xpath/xpath_functions.cpp"
#include "xpath/xpath_axis.cpp"
#include "xpath/xpath_parser.cpp"
#include "xpath/xpath_evaluator.cpp"

//********************************************************************************************************************
// Main XPath Entry Point

ERR extXML::find_tag(std::string_view XPath, uint32_t CurrentPrefix)
{
   pf::Log log(__FUNCTION__);

   if ((!CursorTags) or (CursorTags->empty())) {
      log.warning("Sanity check failed; CursorTags not defined or empty.");
      return ERR::Failed;
   }

   // Create evaluator and delegate to it
   SimpleXPathEvaluator evaluator(this);

   auto ast_result = evaluator.find_tag_enhanced(XPath, CurrentPrefix);
   log.msg("AST result: %d", int(ast_result));
   return ast_result;
}
