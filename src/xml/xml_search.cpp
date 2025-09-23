//********************************************************************************************************************
// TODO: Rewrite in-progress - AST evaluator under construction
// The legacy string-based evaluator is being replaced in stages:
// 1) Full AST traversal (location paths, descendant handling, callbacks)
// 2) Predicate parity (indices, attribute filters, Parasol extensions)
// 3) Function/expression node-set support
// 4) Axis + node test completion (self/parent/ancestor/etc., namespace-aware)
// 5) Remove legacy fallback once all behaviours restored
// Temporary regressions expected; tests referencing unimplemented features may be disabled or adjusted.
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

#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <system_error>

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

   // Use the enhanced evaluator which tries AST first, then falls back to legacy
   if ((Flags & XMF::PARSE_HTML) IS XMF::NIL) {
      auto enhanced_result = evaluator.find_tag_enhanced(XPath, CurrentPrefix);
      log.msg("Enhanced result: %d", int(enhanced_result));
      if (enhanced_result IS ERR::Okay or enhanced_result IS ERR::Search) {
         return enhanced_result;
      }
   }

   // If enhanced evaluation failed or HTML mode, use legacy path directly
   SimpleXPathEvaluator::PathInfo info;

   // Parse the path
   auto parse_result = evaluator.parse_path(XPath, info);
   if (parse_result != ERR::Okay) return parse_result;

   // Handle simple attribute-only paths like '/@attrib' (already handled in parse_path)
   if (!Attrib.empty()) return ERR::Okay;

   // Evaluate the XPath step
   return evaluator.evaluate_step(XPath, info, CurrentPrefix);
}
