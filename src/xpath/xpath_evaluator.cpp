//********************************************************************************************************************
// XPath Evaluation Engine
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
#include "../xml/schema/schema_types.h"
#include "../xml/xml.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

//********************************************************************************************************************

namespace
{
std::optional<std::string> normalise_env_value(const char *Value)
{
   if (!Value) return std::nullopt;

   while ((*Value IS ' ') or (*Value IS '\t') or (*Value IS '\r') or (*Value IS '\n')) ++Value;
   if (*Value IS '\0') return std::nullopt;

   std::string normalised(Value);
   while (!normalised.empty() and ((normalised.back() IS ' ') or (normalised.back() IS '\t') or (normalised.back() IS '\r') or (normalised.back() IS '\n')))
   {
      normalised.pop_back();
   }

   std::transform(normalised.begin(), normalised.end(), normalised.begin(),
      [](unsigned char Ch) -> char { return (char)std::tolower((unsigned char)Ch); });

   return normalised;
}

bool env_flag_enabled(const char *Value)
{
   auto normalised = normalise_env_value(Value);
   if (!normalised.has_value()) return false;

   if ((*normalised IS "0") or (*normalised IS "false") or (*normalised IS "off") or (*normalised IS "no") or (*normalised IS "disable") or (*normalised IS "disabled"))
   {
      return false;
   }

   return true;
}

struct TraceLevelConfig
{
   VLF detail = VLF::API;
   VLF verbose = VLF::DETAIL;
};

TraceLevelConfig parse_trace_levels(const char *Value)
{
   TraceLevelConfig config;

   auto normalised = normalise_env_value(Value);
   if (!normalised.has_value()) return config;

   const std::string &level = *normalised;

   if ((level IS "warning") or (level IS "warn"))
   {
      config.detail = VLF::WARNING;
      config.verbose = VLF::API;
      return config;
   }

   if (level IS "info")
   {
      config.detail = VLF::INFO;
      config.verbose = VLF::WARNING;
      return config;
   }

   if ((level IS "detail") or (level IS "detailed"))
   {
      config.detail = VLF::DETAIL;
      config.verbose = VLF::TRACE;
      return config;
   }

   if ((level IS "trace") or (level IS "verbose"))
   {
      config.detail = VLF::TRACE;
      config.verbose = VLF::TRACE;
      return config;
   }

   if ((level IS "warning+api") or (level IS "warn+api"))
   {
      config.detail = VLF::WARNING;
      config.verbose = VLF::API;
      return config;
   }

   return config;
}
} // namespace

XPathEvaluator::XPathEvaluator(extXML *XML) : xml(XML), axis_evaluator(XML, arena)
{
   trace_xpath_enabled = env_flag_enabled(std::getenv("PARASOL_TRACE_XPATH"));
   auto trace_levels = parse_trace_levels(std::getenv("PARASOL_TRACE_XPATH_LEVEL"));
   trace_detail_level = trace_levels.detail;
   trace_verbose_level = trace_levels.verbose;
   context.document = XML;
   context.expression_unsupported = &expression_unsupported;
   context.schema_registry = &xml::schema::registry();
}

std::string XPathEvaluator::build_ast_signature(const XPathNode *Node) const
{
   if (!Node) return std::string("#");

   std::string children_sig;
   for (size_t index = 0; index < Node->child_count(); ++index) {
      auto child = Node->get_child(index);
      children_sig += build_ast_signature(child);
      children_sig += ',';
   }

   return std::format("({}|{}:{})", int(Node->type), Node->value, children_sig);
}

void XPathEvaluator::record_error(std::string_view Message, bool Force)
{
   expression_unsupported = true;
   if (is_trace_enabled(TraceCategory::XPath))
   {
      pf::Log log("XPath");
      log.msg(trace_detail_level, "record_error: %s", std::string(Message).c_str());
   }
   if (!xml) return;
   if (Force or xml->ErrorMsg.empty()) xml->ErrorMsg.assign(Message);
}

//********************************************************************************************************************
// Public method for AST Evaluation

ERR XPathEvaluator::find_tag(const XPathNode &XPath, uint32_t CurrentPrefix)
{
   // Reset the evaluator state
   axis_evaluator.reset_namespace_nodes();
   arena.reset();

   return evaluate_ast(&XPath, CurrentPrefix);
}

void XPathEvaluator::set_trace_enabled(TraceCategory Category, bool Enabled)
{
   if (Category IS TraceCategory::XPath) trace_xpath_enabled = Enabled;
}

bool XPathEvaluator::is_trace_enabled(TraceCategory Category) const
{
   if (Category IS TraceCategory::XPath) return trace_xpath_enabled;
   return false;
}

//********************************************************************************************************************
// Public method to evaluate complete XPath expressions and return computed values

ERR XPathEvaluator::evaluate_xpath_expression(const XPathNode &XPath, XPathVal *Result, uint32_t CurrentPrefix)
{
   (void)xml->getMap(); // Ensure the tag ID and ParentID values are defined

   // Set context to document root if not already set

   if (!context.context_node) push_context(&xml->Tags[0], 1, 1);

   // Evaluate the compiled AST and return the XPathVal directly

   expression_unsupported = false;
   constructed_nodes.clear();
   next_constructed_node_id = -1;

   const XPathNode *node = &XPath;
   if (node->type IS XPathNodeType::EXPRESSION) {
      if (node->child_count() > 0) node = node->get_child(0);
      else node = nullptr;
   }

   *Result = std::move(evaluate_expression(node, CurrentPrefix));

   if (expression_unsupported) {
      if (xml and xml->ErrorMsg.empty()) xml->ErrorMsg = "Unsupported XPath expression.";
      return ERR::Syntax;
   }
   else return ERR::Okay;
}
