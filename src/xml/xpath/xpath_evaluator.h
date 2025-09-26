//********************************************************************************************************************
// XPath Evaluator - Main Evaluation Engine
//
// This file contains the final AST-only XPathEvaluator implementation and
// integrates XPath traversal, predicate, and function support.
//********************************************************************************************************************

#pragma once

#include "xpath_arena.h"

//********************************************************************************************************************
// Main XPath Evaluator

struct XMLAttrib;

class XPathEvaluator {
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
   XPathArena arena;
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
   struct CachedAstEntry {
      std::shared_ptr<XPathNode> ast;
      std::string signature;
   };
   std::unordered_map<std::string, CachedAstEntry> ast_cache;
   std::unordered_map<std::string, std::weak_ptr<XPathNode>> ast_signature_cache;
   std::deque<std::string> ast_cache_order;
   static constexpr size_t ast_cache_limit = 32;
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
   XPathValue evaluate_union_value(const std::vector<const XPathNode *> &Branches, uint32_t CurrentPrefix);
   ERR evaluate_union(const XPathNode *Node, uint32_t CurrentPrefix);
   std::shared_ptr<XPathNode> get_cached_ast(const std::string &Key);
   void store_cached_ast(const std::string &Key, const std::shared_ptr<XPathNode> &Ast, const std::string &Signature);
   void prune_ast_cache();
   std::string normalise_cache_key(std::string_view Expression) const;
   std::string build_ast_signature(const XPathNode *Node) const;

   public:
   explicit XPathEvaluator(extXML *XML) : xml(XML), axis_evaluator(XML, arena) {
      context.document = XML;
      context.expression_unsupported_flag = &expression_unsupported;
   }

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
