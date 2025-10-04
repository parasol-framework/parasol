//********************************************************************************************************************
// XPath Evaluation Engine
//********************************************************************************************************************
//
// The evaluator coordinates the complete XPath execution pipeline for Parasol's XML subsystem.  It
// receives token sequences from the tokenizer, constructs an AST via the parser, and then walks that
// AST to resolve node-sets, scalar values, and boolean predicates against the in-memory document
// model.  Beyond expression evaluation, the class maintains the implicit evaluation context defined by
// the XPath specification (context node, size, position, and active attribute), marshals axis
// traversal through AxisEvaluator, and carefully mirrors document order semantics so that results
// match the behaviour expected by downstream engines.
//
// This translation unit focuses on execution concerns: stack management for nested contexts, helper
// routines for managing evaluation state, AST caching, dispatching axes, and interpretation of AST nodes.  A
// large portion of the logic is defensive—preserving cursor state for integration with the legacy
// cursor-based API, falling back gracefully when unsupported expressions are encountered, and
// honouring namespace prefix resolution rules.  By keeping the evaluator self-contained, the parser
// and tokenizer remain ignorant of runtime data structures, and testing of the evaluator can be done
// independently of XML parsing.

#include "xpath_evaluator.h"
#include "xpath_evaluator_detail.h"
#include "xpath_functions.h"
#include "xpath_axis.h"
#include "../schema/schema_types.h"
#include "../xml.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace {

// Lightweight view-based trim used for cache key normalisation.
[[maybe_unused]] std::string_view trim_view(std::string_view Value)
{
   auto start = Value.find_first_not_of(" \t\r\n");
   if (start IS std::string_view::npos) return std::string_view();

   auto end = Value.find_last_not_of(" \t\r\n");
   return Value.substr(start, end - start + 1);
}

} // namespace

std::string XPathEvaluator::build_ast_signature(const XPathNode *Node) const
{
   if (!Node) return std::string("#");

   std::string signature;
   signature.reserve(16);

   signature += '(';
   signature += std::to_string(int(Node->type));
   signature += '|';
   signature += Node->value;
   signature += ':';

   for (size_t index = 0; index < Node->child_count(); ++index) {
      auto child = Node->get_child(index);
      signature += build_ast_signature(child);
      signature += ',';
   }

   signature += ')';

   return signature;
}

void XPathEvaluator::record_error(std::string_view Message, bool Force)
{
   expression_unsupported = true;
   if (!xml) return;
   if (Force or xml->ErrorMsg.empty()) xml->ErrorMsg.assign(Message);
}

//********************************************************************************************************************
// Public method for AST Evaluation

ERR XPathEvaluator::find_tag(const CompiledXPath &CompiledPath, uint32_t CurrentPrefix)
{
   if (!CompiledPath.isValid()) return ERR::Syntax;

   // Reset the evaluator state
   axis_evaluator.reset_namespace_nodes();
   arena.reset();

   // Ensure the tag ID and ParentID values are defined

   (void)xml->getMap();

   return evaluate_ast(CompiledPath.getAST(), CurrentPrefix);
}

//********************************************************************************************************************
// Public method to evaluate complete XPath expressions and return computed values

ERR XPathEvaluator::evaluate_xpath_expression(const CompiledXPath &CompiledPath, XPathValue &Result, uint32_t CurrentPrefix)
{
   if (!CompiledPath.isValid()) return ERR::Syntax;

   // Ensure the tag ID and ParentID values are defined

   (void)xml->getMap();

   // Set context to document root if not already set

   if (!context.context_node) push_context(&xml->Tags[0], 1, 1);

   // Evaluate the compiled AST and return the XPathValue directly

   expression_unsupported = false;

   const XPathNode *expression_node = CompiledPath.getAST();
   if (expression_node and (expression_node->type IS XPathNodeType::EXPRESSION)) {
      if (expression_node->child_count() > 0) expression_node = expression_node->get_child(0);
      else expression_node = nullptr;
   }

   Result = evaluate_expression(expression_node, CurrentPrefix);

   if (expression_unsupported) {
      if (xml and xml->ErrorMsg.empty()) xml->ErrorMsg = "Unsupported XPath expression.";
      return ERR::Syntax;
   }
   else return ERR::Okay;
}
