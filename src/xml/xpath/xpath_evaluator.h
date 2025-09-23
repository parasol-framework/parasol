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
   extXML *xml;
   XPathFunctionLibrary function_library;
   XPathContext context;
   AxisEvaluator axis_evaluator;

   // Stack management for AST traversal (Phase 1)
   struct CursorState {
      XMLTag* tag;
      size_t child_index;
      size_t total_children;
   };
   std::vector<CursorState> cursor_stack;
   std::vector<XMLTag*> cursor_tags;

   public:
   explicit SimpleXPathEvaluator(extXML *XML) : xml(XML), axis_evaluator(XML) {}

   // Phase 1 methods (string-based legacy)
   ERR parse_path(std::string_view XPath, PathInfo &info);
   bool match_tag(const PathInfo &info, uint32_t current_prefix);
   ERR evaluate_step(std::string_view XPath, PathInfo info, uint32_t current_prefix);

   // Phase 2+ methods (AST-based)
   ERR evaluate_ast(const XPathNode* node, uint32_t current_prefix);
   ERR evaluate_location_path(const XPathNode* path_node, uint32_t current_prefix);
   ERR evaluate_step_ast(const XPathNode* step_node, uint32_t current_prefix);
   bool match_node_test(const XPathNode* node_test, uint32_t current_prefix);
   bool evaluate_predicate(const XPathNode* predicate_node, uint32_t current_prefix);

   // Phase 3 methods (function support)
   XPathValue evaluate_expression(const XPathNode* expr_node, uint32_t current_prefix);
   XPathValue evaluate_function_call(const XPathNode* func_node, uint32_t current_prefix);

   // Utility method to try AST-based parsing first, fall back to string-based
   ERR find_tag_enhanced(std::string_view XPath, uint32_t current_prefix);

   // Helper method to evaluate simple function expressions in string-based evaluation
   bool evaluate_function_expression(const std::string& expression);

   // Context management for AST evaluation
   void push_context(XMLTag* node, size_t position = 1, size_t size = 1);
   void pop_context();
   XMLTag* get_context_node() const { return context.context_node; }

   // Stack management for deep traversal
   void push_cursor_state(XMLTag* tag, size_t child_index, size_t total_children);
   void pop_cursor_state();
   bool has_cursor_state() const { return !cursor_stack.empty(); }
};