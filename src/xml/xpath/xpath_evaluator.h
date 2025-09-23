//********************************************************************************************************************
// XPath Evaluator - Main Evaluation Engine
//
// This file contains:
// - SimpleXPathEvaluator class (both legacy and AST methods)
// - Path evaluation logic aligned with AST_PLAN.md phases
// - Integration point for all XPath subsystems
//********************************************************************************************************************

#pragma once

//********************************************************************************************************************
// Main XPath Evaluator

class SimpleXPathEvaluator {
   public:
   struct PathInfo {
      bool flat_scan = false;
      size_t pos = 0;
      std::string_view tag_name;
      uint32_t tag_prefix = 0;
      std::string attrib_value;
      std::string_view attrib_name;
      bool wild = false;
      int subscript = 0;
   };

   private:
   extXML * xml;
   XPathFunctionLibrary function_library;
   XPathContext context;
   AxisEvaluator axis_evaluator;

   // Stack management for AST traversal (Phase 1)
   struct CursorState {
      XMLTag * tag;
      size_t child_index;
      size_t total_children;
   };
   std::vector<CursorState> cursor_stack;
   std::vector<XMLTag *> cursor_tags;

   public:
   explicit SimpleXPathEvaluator(extXML *XML) : xml(XML), axis_evaluator(XML) {}

   // Phase 1 methods (string-based legacy)
   ERR parse_path(std::string_view XPath, PathInfo &Info);
   bool match_tag(const PathInfo &Info, uint32_t CurrentPrefix);
   ERR evaluate_step(std::string_view XPath, PathInfo Info, uint32_t CurrentPrefix);

   // Phase 2+ methods (AST-based)
   ERR evaluate_ast(const XPathNode *Node, uint32_t CurrentPrefix);
   ERR evaluate_location_path(const XPathNode *PathNode, uint32_t CurrentPrefix);
   ERR evaluate_step_ast(const XPathNode *StepNode, uint32_t CurrentPrefix);
   bool match_node_test(const XPathNode *NodeTest, uint32_t CurrentPrefix);
   bool evaluate_predicate(const XPathNode *PredicateNode, uint32_t CurrentPrefix);

   // Phase 3 methods (function support)
   XPathValue evaluate_expression(const XPathNode *ExprNode, uint32_t CurrentPrefix);
   XPathValue evaluate_function_call(const XPathNode *FuncNode, uint32_t CurrentPrefix);

   // Utility method to try AST-based parsing first, fall back to string-based
   ERR find_tag_enhanced(std::string_view XPath, uint32_t CurrentPrefix);

   // Helper method to evaluate simple function expressions in string-based evaluation
   bool evaluate_function_expression(const std::string &Expression);

   // Context management for AST evaluation
   void push_context(XMLTag *Node, size_t Position = 1, size_t Size = 1);
   void pop_context();
   XMLTag * get_context_node() const { return context.context_node; }

   // Stack management for deep traversal
   void push_cursor_state(XMLTag *Tag, size_t ChildIndex, size_t TotalChildren);
   void pop_cursor_state();
   bool has_cursor_state() const { return !cursor_stack.empty(); }
};