// Defines the XPath axis evaluation subsystem responsible for translating abstract axis operations
// into concrete traversals of Parasol's XMLTag tree.  The header publishes the AxisType enumeration,
// the AxisEvaluator helper, and supporting data structures that keep namespace nodes, document order,
// and ancestor caches consistent across evaluations.  The accompanying implementation applies these
// declarations to realise the full set of XPath navigation semantics.

#pragma once

#include "xpath_ast.h"
#include "xpath_arena.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <parasol/modules/xml.h>
#include <parasol/strings.hpp>
#include "../xml/xml.h"

//********************************************************************************************************************
// XPath Axis Types

enum class AxisType {
   CHILD,
   DESCENDANT,
   PARENT,
   ANCESTOR,
   FOLLOWING_SIBLING,
   PRECEDING_SIBLING,
   FOLLOWING,
   PRECEDING,
   ATTRIBUTE,
   NAMESPACE,
   SELF,
   DESCENDANT_OR_SELF,
   ANCESTOR_OR_SELF
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

   struct AncestorPathView {
      std::span<XMLTag *const> Path;
      NODES * Storage = nullptr;
      bool Cached = false;
   };

   std::unordered_map<XMLTag *, NODES *> ancestor_path_cache;
   std::vector<std::unique_ptr<NODES>> ancestor_path_storage;
   std::unordered_map<uint64_t, bool> document_order_cache;

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
   void evaluate_child_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_descendant_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_parent_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_ancestor_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_following_sibling_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_preceding_sibling_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_following_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_preceding_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_namespace_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_self_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_descendant_or_self_axis(XMLTag *ContextNode, NODES &Output);
   void evaluate_ancestor_or_self_axis(XMLTag *ContextNode, NODES &Output);

   void collect_subtree_reverse(XMLTag *Node, NODES &Output);

   // Optimized namespace collection
   void collect_namespace_declarations(XMLTag *Node, std::vector<NamespaceDeclaration> &declarations);

   // Namespace node pooling helpers
   XMLTag * acquire_namespace_node();
   void recycle_namespace_nodes();

   // Document order utilities
   void sort_document_order(NODES &Nodes);
   AncestorPathView build_ancestor_path(XMLTag *Node);
   void release_ancestor_path(AncestorPathView &View);
   uint64_t make_document_order_key(XMLTag *Left, XMLTag *Right);

   // Helper methods for tag lookup
   void build_id_cache();
   extXML * find_document_for_node(XMLTag *Node);
   XMLTag * find_tag_by_id(XMLTag *ReferenceNode, int ID);

   public:
   explicit AxisEvaluator(extXML *XML, XPathArena &Arena) : xml(XML), arena(Arena) {}

   // Main evaluation method
   void evaluate_axis(AxisType Axis, XMLTag *ContextNode, NODES &Output);

   // Evaluation lifecycle helpers
   void reset_namespace_nodes();

   // Node-set utilities
   void normalise_node_set(NODES &Nodes);
   bool is_before_in_document_order(XMLTag *Node1, XMLTag *Node2);

   // Utility methods
   static AxisType parse_axis_name(std::string_view AxisName);
   static std::string_view axis_name_to_string(AxisType Axis);
   static bool is_reverse_axis(AxisType Axis);

   // Capacity estimation helper
   size_t estimate_result_size(AxisType Axis, XMLTag *ContextNode);
};
