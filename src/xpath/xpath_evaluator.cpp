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
#include "../xml/schema/schema_types.h"
#include "../xml/xml.h"

#include <format>

//********************************************************************************************************************
// Constructs the evaluator with a reference to the XML document. Initialises the axis evaluator, configures
// trace settings from log depth, and prepares the evaluation context with schema registry and variable storage.

XPathEvaluator::XPathEvaluator(extXML *XML) : xml(XML), axis_evaluator(XML, arena)
{
   trace_xpath_enabled = GetResource(RES::LOG_DEPTH) >= 8;
   context.document = XML;
   context.expression_unsupported = &expression_unsupported;
   context.schema_registry = &xml::schema::registry();
   context.variables = &variable_storage;
}

//********************************************************************************************************************

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

//********************************************************************************************************************
// Records an error for the XML object & sets the expression_unsupported flag.
// Setting Force will override existing XML ErrorMsg.
// Additionally, if a Node is provided, a detailed stack trace is logged.

void XPathEvaluator::record_error(std::string_view Message, bool Force)
{
   expression_unsupported = true;
   pf::Log log("XPath");
   log.msg("%.*s", (int)Message.size(), Message.data());
   if (xml) {
      if (Force or xml->ErrorMsg.empty()) xml->ErrorMsg.assign(Message);
   }
}

void XPathEvaluator::record_error(std::string_view Message, const XPathNode *Node, bool Force)
{
   pf::Log log("XPath");

   expression_unsupported = true;
   
   // Expression signature (compact AST fingerprint)

   std::string signature = "";
   if (Node) {
      signature = build_ast_signature(Node);
   }

   log.branch("%.*s %s [Stack detail follows]", (int)Message.size(), Message.data(), signature.c_str());

   if (xml) {
      if (Force or xml->ErrorMsg.empty()) xml->ErrorMsg.assign(Message);
   }

   // Dump evaluator context stack from outermost to innermost.
   // Frames in context_stack are prior contexts; current context is appended last.

   auto emit_frame = [&](const XPathContext &frame, size_t index) {
      int node_id = -1;
      CSTRING node_name = "(null)";
      CSTRING attr_name = "∅";
      CSTRING doc_label = "unknown";

      if (frame.context_node) {
         node_id = frame.context_node->ID;
         if (!frame.context_node->Attribs.empty()) node_name = frame.context_node->Attribs[0].Name.c_str();

         // Document label: 'this' if owned by this->xml, 'foreign' if another extXML, otherwise 'unknown'
         if (is_foreign_document_node(frame.context_node)) doc_label = "foreign";
         else if (xml) doc_label = "this";
      }

      if (frame.attribute_node) attr_name = frame.attribute_node->Name.c_str();

      log.detail("[%u] node-id=%d name='%s' pos=%u/%u attr=%s doc=%s",
         unsigned(index), node_id, node_name, unsigned(frame.position), unsigned(frame.size), attr_name, doc_label);
   };

   // Emit stored frames

   for (size_t i = 0; i < context_stack.size(); ++i) emit_frame(context_stack[i], i);

   // Emit current frame as the last entry

   emit_frame(context, context_stack.size());

   // Optionally include variable bindings present in the current context

   if (!context.variables->empty()) {
      // Build a comma-separated list of variable names (best-effort, avoid allocations where possible)
      std::string names;
      names.reserve(64);
      bool first = true;
      for (const auto &entry : *context.variables) {
         if (!first) names += ", ";
         first = false;
         names += entry.first;
      }
      log.detail("Variables: count=%u names=[%s]", unsigned(context.variables->size()), names.c_str());
   }
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
