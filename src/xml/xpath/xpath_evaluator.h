//********************************************************************************************************************
// XPath Evaluator - Main Evaluation Engine
//
// This file contains the final AST-only SimpleXPathEvaluator implementation and
// integrates XPath traversal, predicate, and function support.
//********************************************************************************************************************

#pragma once

//********************************************************************************************************************
// Main XPath Evaluator

struct XMLAttrib;

class SimpleXPathEvaluator {
   public:
   enum class PredicateResult {
      Match,
      NoMatch,
      Unsupported
   };

   private:
   extXML * xml;
   XPathFunctionLibrary function_library;
   XPathContext context;
   AxisEvaluator axis_evaluator;
   bool expression_unsupported = false;

   struct AxisMatch {
      XMLTag * node = nullptr;
      const XMLAttrib * attribute = nullptr;
   };

   struct CursorState {
      objXML::TAGS * tags;
      size_t index;
   };
   std::vector<CursorState> cursor_stack;
   std::vector<XPathContext> context_stack;
   std::vector<AxisMatch> dispatch_axis(AxisType Axis, XMLTag *ContextNode, const XMLAttrib *ContextAttribute = nullptr);
   std::vector<XMLTag *> collect_step_results(const std::vector<AxisMatch> &ContextNodes,
                                              const std::vector<const XPathNode *> &Steps,
                                              size_t StepIndex,
                                              uint32_t CurrentPrefix,
                                              bool &Unsupported);
   XPathValue evaluate_path_expression_value(const XPathNode *PathNode, uint32_t CurrentPrefix);
   XPathValue evaluate_path_from_nodes(const std::vector<XMLTag *> &InitialContext,
                                       const std::vector<const XMLAttrib *> &InitialAttributes,
                                       const std::vector<const XPathNode *> &Steps,
                                       const XPathNode *AttributeStep,
                                       const XPathNode *AttributeTest,
                                       uint32_t CurrentPrefix);
   ERR evaluate_top_level_expression(const XPathNode *Node, uint32_t CurrentPrefix);
   ERR process_expression_node_set(const XPathValue &Value);

   public:
   explicit SimpleXPathEvaluator(extXML *XML) : xml(XML), axis_evaluator(XML) { context.document = XML; }

   // Phase 2+ methods (AST-based)
   ERR evaluate_ast(const XPathNode *Node, uint32_t CurrentPrefix);
   ERR evaluate_location_path(const XPathNode *PathNode, uint32_t CurrentPrefix);
   ERR evaluate_step_ast(const XPathNode *StepNode, uint32_t CurrentPrefix);
   ERR evaluate_step_sequence(const std::vector<XMLTag *> &ContextNodes, const std::vector<const XPathNode *> &Steps, size_t StepIndex, uint32_t CurrentPrefix, bool &Matched);
   bool match_node_test(const XPathNode *NodeTest, AxisType Axis, XMLTag *Candidate, const XMLAttrib *Attribute, uint32_t CurrentPrefix);
   PredicateResult evaluate_predicate(const XPathNode *PredicateNode, uint32_t CurrentPrefix);

   // Phase 3 methods (function support)
   XPathValue evaluate_expression(const XPathNode *ExprNode, uint32_t CurrentPrefix);
   XPathValue evaluate_function_call(const XPathNode *FuncNode, uint32_t CurrentPrefix);

   // Entry point for AST-only evaluation (retained name for API stability)
   ERR find_tag_enhanced(std::string_view XPath, uint32_t CurrentPrefix);
   ERR find_tag_enhanced_internal(std::string_view XPath, uint32_t CurrentPrefix, bool AllowUnionSplit);

   // Context management for AST evaluation
   void push_context(XMLTag *Node, size_t Position = 1, size_t Size = 1, const XMLAttrib *Attribute = nullptr);
   void pop_context();
   XMLTag * get_context_node() const { return context.context_node; }

   // Stack management for deep traversal
   void push_cursor_state();
   void pop_cursor_state();
   bool has_cursor_state() const { return !cursor_stack.empty(); }
};
