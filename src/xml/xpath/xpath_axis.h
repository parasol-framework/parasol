// XPath Axis Evaluation System
//
// This file contains:
// - Axis type definitions (Phase 4 of AST_PLAN.md)
// - Axis evaluation logic for traversing XML document structure
// - Support for all XPath 1.0 axes

#pragma once

#include "xpath_ast.h"
#include <parasol/modules/xml.h>
#include <vector>

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
   extXML* xml;

   // Helper methods for specific axes
   std::vector<XMLTag*> evaluate_child_axis(XMLTag* context_node);
   std::vector<XMLTag*> evaluate_descendant_axis(XMLTag* context_node);
   std::vector<XMLTag*> evaluate_parent_axis(XMLTag* context_node);
   std::vector<XMLTag*> evaluate_ancestor_axis(XMLTag* context_node);
   std::vector<XMLTag*> evaluate_following_sibling_axis(XMLTag* context_node);
   std::vector<XMLTag*> evaluate_preceding_sibling_axis(XMLTag* context_node);
   std::vector<XMLTag*> evaluate_following_axis(XMLTag* context_node);
   std::vector<XMLTag*> evaluate_preceding_axis(XMLTag* context_node);
   std::vector<XMLTag*> evaluate_attribute_axis(XMLTag* context_node);
   std::vector<XMLTag*> evaluate_namespace_axis(XMLTag* context_node);
   std::vector<XMLTag*> evaluate_self_axis(XMLTag* context_node);
   std::vector<XMLTag*> evaluate_descendant_or_self_axis(XMLTag* context_node);
   std::vector<XMLTag*> evaluate_ancestor_or_self_axis(XMLTag* context_node);

   // Document order utilities
   void sort_document_order(std::vector<XMLTag*>& nodes);
   bool is_before_in_document_order(XMLTag* node1, XMLTag* node2);

   // Helper methods for tag lookup
   XMLTag* find_tag_by_id(int id);
   XMLTag* find_tag_recursive(XMLTag& tag, int id);

   public:
   explicit AxisEvaluator(extXML* XML) : xml(XML) {}

   // Main evaluation method
   std::vector<XMLTag*> evaluate_axis(AxisType axis, XMLTag* context_node);

   // Utility methods
   static AxisType parse_axis_name(const std::string& axis_name);
   static std::string axis_name_to_string(AxisType axis);
   static bool is_reverse_axis(AxisType axis);
};