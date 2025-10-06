//********************************************************************************************************************
// XPath Evaluator - Main Evaluation Engine

#pragma once

#include <string_view>
#include <unordered_map>

class extXML;

#include "xpath_axis.h"
#include "xpath_functions.h"

struct XMLAttrib;
class CompiledXPath;

class XPathEvaluator {
   public:
   enum class PredicateResult {
      MATCH,
      NO_MATCH,
      UNSUPPORTED
   };

   private:
   extXML * xml;
   XPathContext context;
   XPathArena arena;
   AxisEvaluator axis_evaluator;
   bool expression_unsupported = false;

   struct AxisMatch {
      XMLTag * node = nullptr;
      const XMLAttrib * attribute = nullptr;
   };

   using PredicateHandler = PredicateResult (XPathEvaluator::*)(const XPathNode *, uint32_t);

   struct CursorState {
      pf::vector<XMLTag> * tags;
      size_t index;
   };

   std::vector<CursorState> cursor_stack;
   std::vector<XPathContext> context_stack;
   
   std::vector<AxisMatch> dispatch_axis(AxisType Axis, XMLTag *ContextNode, const XMLAttrib *ContextAttribute = nullptr);
   extXML * resolve_document_for_node(XMLTag *Node) const;
   bool is_foreign_document_node(XMLTag *Node) const;
   std::vector<XMLTag *> collect_step_results(const std::vector<AxisMatch> &,
      const std::vector<const XPathNode *> &, size_t, uint32_t, bool &);
   XPathVal evaluate_path_expression_value(const XPathNode *PathNode, uint32_t CurrentPrefix);
   XPathVal evaluate_path_from_nodes(const std::vector<XMLTag *> &,
      const std::vector<const XMLAttrib *> &, const std::vector<const XPathNode *> &, const XPathNode *, 
      const XPathNode *, uint32_t);
   ERR evaluate_top_level_expression(const XPathNode *Node, uint32_t CurrentPrefix);
   ERR process_expression_node_set(const XPathVal &Value);
   XPathVal evaluate_union_value(const std::vector<const XPathNode *> &Branches, uint32_t CurrentPrefix);
   XPathVal evaluate_intersect_value(const XPathNode *Left, const XPathNode *Right, uint32_t CurrentPrefix);
   XPathVal evaluate_except_value(const XPathNode *Left, const XPathNode *Right, uint32_t CurrentPrefix);
   ERR evaluate_union(const XPathNode *Node, uint32_t CurrentPrefix);

   void expand_axis_candidates(const AxisMatch &ContextEntry, AxisType Axis,
      const XPathNode *NodeTest, uint32_t CurrentPrefix, std::vector<AxisMatch> &FilteredMatches);
   ERR apply_predicates_to_candidates(const std::vector<const XPathNode *> &PredicateNodes,
      uint32_t CurrentPrefix, std::vector<AxisMatch> &Candidates, std::vector<AxisMatch> &ScratchBuffer);
   ERR process_step_matches(const std::vector<AxisMatch> &Matches, AxisType Axis, bool IsLastStep,
      bool &Matched, std::vector<AxisMatch> &NextContext, bool &ShouldTerminate);
   ERR invoke_callback(XMLTag *Node, const XMLAttrib *Attribute, bool &Matched, bool &ShouldTerminate);

   PredicateResult dispatch_predicate_operation(std::string_view OperationName, const XPathNode *Expression,
      uint32_t CurrentPrefix);
   const std::unordered_map<std::string_view, PredicateHandler> &predicate_handler_map() const;
   PredicateResult handle_attribute_exists_predicate(const XPathNode *Expression, uint32_t CurrentPrefix);
   PredicateResult handle_attribute_equals_predicate(const XPathNode *Expression, uint32_t CurrentPrefix);
   PredicateResult handle_content_equals_predicate(const XPathNode *Expression, uint32_t CurrentPrefix);

   std::string build_ast_signature(const XPathNode *Node) const;

   void record_error(std::string_view Message, bool Force = false);

   public:
   explicit XPathEvaluator(extXML *XML) : xml(XML), axis_evaluator(XML, arena) {
      context.document = XML;
      context.expression_unsupported = &expression_unsupported;
      context.schema_registry = &xml::schema::registry();
   }

   ERR evaluate_ast(const XPathNode *Node, uint32_t CurrentPrefix);
   ERR evaluate_location_path(const XPathNode *PathNode, uint32_t CurrentPrefix);
   ERR evaluate_step_ast(const XPathNode *StepNode, uint32_t CurrentPrefix);
   ERR evaluate_step_sequence(const std::vector<XMLTag *> &ContextNodes, const std::vector<const XPathNode *> &Steps, size_t StepIndex, uint32_t CurrentPrefix, bool &Matched);
   bool match_node_test(const XPathNode *NodeTest, AxisType Axis, XMLTag *Candidate, const XMLAttrib *Attribute, uint32_t CurrentPrefix);
   PredicateResult evaluate_predicate(const XPathNode *PredicateNode, uint32_t CurrentPrefix);

   XPathVal evaluate_expression(const XPathNode *ExprNode, uint32_t CurrentPrefix);
   XPathVal evaluate_function_call(const XPathNode *FuncNode, uint32_t CurrentPrefix);

   // Entry point for compiled XPath evaluation
   ERR find_tag(const XPathNode &, uint32_t);

   // Full XPath expression evaluation returning computed values.  Will update the provided XPathValue
   ERR evaluate_xpath_expression(const XPathNode &, XPathVal *, uint32_t CurrentPrefix = 0);

   // Context management for AST evaluation
   void push_context(XMLTag *Node, size_t Position = 1, size_t Size = 1, const XMLAttrib *Attribute = nullptr);
   void pop_context();
   XMLTag * get_context_node() const { return context.context_node; }

   // Stack management for deep traversal
   void push_cursor_state();
   void pop_cursor_state();
   bool has_cursor_state() const { return !cursor_stack.empty(); }
};
