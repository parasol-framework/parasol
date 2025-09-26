// XPath Axis Evaluation System
//
// This file contains:
// - Axis type definitions (Phase 4 of AST_PLAN.md)
// - Axis evaluation logic for traversing XML document structure
// - Support for all XPath 1.0 axes

#pragma once

#include "xpath_ast.h"
#include "xpath_arena.h"

//********************************************************************************************************************
// XPath Axis Types

enum class AxisType {
   Child,
   Descendant,
   Parent,
   Ancestor,
   FollowingSibling,
   PrecedingSibling,
   Following,
   Preceding,
   Attribute,
   Namespace,
   Self,
   DescendantOrSelf,
   AncestorOrSelf
};

//********************************************************************************************************************
// Axis Evaluation Engine

class AxisEvaluator {
   private:
   extXML * xml;
   XPathArena & arena;
   std::vector<std::unique_ptr<XMLTag>> namespace_node_storage;
   std::unordered_map<int, XMLTag *> id_lookup;
   bool id_cache_built = false;

   // Optimized namespace handling data structures
   struct NamespaceDeclaration {
      std::string prefix;
      std::string uri;

      bool operator<(const NamespaceDeclaration &other) const {
         return prefix < other.prefix;
      }
   };

   // Reusable storage for namespace processing to avoid allocations
   std::vector<NamespaceDeclaration> namespace_declarations;
   std::vector<int> visited_node_ids;

   // Namespace node pool for reuse to reduce allocations
   std::vector<std::unique_ptr<XMLTag>> namespace_node_pool;

   // Helper methods for specific axes
   void evaluate_child_axis(XMLTag *ContextNode, std::vector<XMLTag *> &Output);
   void evaluate_descendant_axis(XMLTag *ContextNode, std::vector<XMLTag *> &Output);
   void evaluate_parent_axis(XMLTag *ContextNode, std::vector<XMLTag *> &Output);
   void evaluate_ancestor_axis(XMLTag *ContextNode, std::vector<XMLTag *> &Output);
   void evaluate_following_sibling_axis(XMLTag *ContextNode, std::vector<XMLTag *> &Output);
   void evaluate_preceding_sibling_axis(XMLTag *ContextNode, std::vector<XMLTag *> &Output);
   void evaluate_following_axis(XMLTag *ContextNode, std::vector<XMLTag *> &Output);
   void evaluate_preceding_axis(XMLTag *ContextNode, std::vector<XMLTag *> &Output);
   void evaluate_attribute_axis(XMLTag *ContextNode, std::vector<XMLTag *> &Output);
   void evaluate_namespace_axis(XMLTag *ContextNode, std::vector<XMLTag *> &Output);
   void evaluate_self_axis(XMLTag *ContextNode, std::vector<XMLTag *> &Output);
   void evaluate_descendant_or_self_axis(XMLTag *ContextNode, std::vector<XMLTag *> &Output);
   void evaluate_ancestor_or_self_axis(XMLTag *ContextNode, std::vector<XMLTag *> &Output);

   void collect_subtree_reverse(XMLTag *Node, std::vector<XMLTag *> &Output);

   // Optimized namespace collection
   void collect_namespace_declarations(XMLTag *Node, std::vector<NamespaceDeclaration> &declarations);

   // Namespace node pooling helpers
   XMLTag * acquire_namespace_node();
   void recycle_namespace_nodes();

   // Document order utilities
   void sort_document_order(std::vector<XMLTag *> &Nodes);
   std::vector<XMLTag *> build_ancestor_path(XMLTag *Node);

   // Helper methods for tag lookup
   void build_id_cache();
   XMLTag * find_tag_by_id(int ID);

   public:
   explicit AxisEvaluator(extXML *XML, XPathArena &Arena) : xml(XML), arena(Arena) {}

   // Main evaluation method
   void evaluate_axis(AxisType Axis, XMLTag *ContextNode, std::vector<XMLTag *> &Output);

   // Evaluation lifecycle helpers
   void reset_namespace_nodes();

   // Node-set utilities
   void normalise_node_set(std::vector<XMLTag *> &Nodes);
   bool is_before_in_document_order(XMLTag *Node1, XMLTag *Node2);

   // Utility methods
   static AxisType parse_axis_name(std::string_view AxisName);
   static std::string_view axis_name_to_string(AxisType Axis);
   static bool is_reverse_axis(AxisType Axis);

   // Capacity estimation helper
   size_t estimate_result_size(AxisType Axis, XMLTag *ContextNode);
};
