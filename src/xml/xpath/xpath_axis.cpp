// XPath Axis Evaluation System Implementation

//********************************************************************************************************************
// AxisEvaluator Implementation

std::vector<XMLTag*> AxisEvaluator::evaluate_axis(AxisType axis, XMLTag* context_node) {
   if (!context_node) return {};

   switch (axis) {
      case AxisType::Child:
         return evaluate_child_axis(context_node);
      case AxisType::Descendant:
         return evaluate_descendant_axis(context_node);
      case AxisType::Parent:
         return evaluate_parent_axis(context_node);
      case AxisType::Ancestor:
         return evaluate_ancestor_axis(context_node);
      case AxisType::FollowingSibling:
         return evaluate_following_sibling_axis(context_node);
      case AxisType::PrecedingSibling:
         return evaluate_preceding_sibling_axis(context_node);
      case AxisType::Following:
         return evaluate_following_axis(context_node);
      case AxisType::Preceding:
         return evaluate_preceding_axis(context_node);
      case AxisType::Attribute:
         return evaluate_attribute_axis(context_node);
      case AxisType::Namespace:
         return evaluate_namespace_axis(context_node);
      case AxisType::Self:
         return evaluate_self_axis(context_node);
      case AxisType::DescendantOrSelf:
         return evaluate_descendant_or_self_axis(context_node);
      case AxisType::AncestorOrSelf:
         return evaluate_ancestor_or_self_axis(context_node);
      default:
         return {};
   }
}

AxisType AxisEvaluator::parse_axis_name(const std::string& axis_name) {
   if (axis_name IS "child") return AxisType::Child;
   else if (axis_name IS "descendant") return AxisType::Descendant;
   else if (axis_name IS "descendant-or-self") return AxisType::DescendantOrSelf;
   else if (axis_name IS "following") return AxisType::Following;
   else if (axis_name IS "following-sibling") return AxisType::FollowingSibling;
   else if (axis_name IS "parent") return AxisType::Parent;
   else if (axis_name IS "ancestor") return AxisType::Ancestor;
   else if (axis_name IS "ancestor-or-self") return AxisType::AncestorOrSelf;
   else if (axis_name IS "preceding") return AxisType::Preceding;
   else if (axis_name IS "preceding-sibling") return AxisType::PrecedingSibling;
   else if (axis_name IS "self") return AxisType::Self;
   else if (axis_name IS "attribute") return AxisType::Attribute;
   else if (axis_name IS "namespace") return AxisType::Namespace;
   else return AxisType::Child; // Default axis
}

std::string AxisEvaluator::axis_name_to_string(AxisType axis) {
   switch (axis) {
      case AxisType::Child: return "child";
      case AxisType::Descendant: return "descendant";
      case AxisType::DescendantOrSelf: return "descendant-or-self";
      case AxisType::Following: return "following";
      case AxisType::FollowingSibling: return "following-sibling";
      case AxisType::Parent: return "parent";
      case AxisType::Ancestor: return "ancestor";
      case AxisType::AncestorOrSelf: return "ancestor-or-self";
      case AxisType::Preceding: return "preceding";
      case AxisType::PrecedingSibling: return "preceding-sibling";
      case AxisType::Self: return "self";
      case AxisType::Attribute: return "attribute";
      case AxisType::Namespace: return "namespace";
      default: return "child";
   }
}

bool AxisEvaluator::is_reverse_axis(AxisType axis) {
   switch (axis) {
      case AxisType::Ancestor:
      case AxisType::AncestorOrSelf:
      case AxisType::Preceding:
      case AxisType::PrecedingSibling:
         return true;
      default:
         return false;
   }
}

//********************************************************************************************************************
// Helper Methods for Specific Axes

XMLTag* AxisEvaluator::find_tag_by_id(int id) {
   if (id IS 0) return nullptr;

   // Search through the entire document tree using the Tags vector
   for (auto& tag : xml->Tags) {
      auto found = find_tag_recursive(tag, id);
      if (found) return found;
   }
   return nullptr;
}

XMLTag* AxisEvaluator::find_tag_recursive(XMLTag& tag, int id) {
   if (tag.ID IS id) return &tag;

   for (auto& child : tag.Children) {
      auto found = find_tag_recursive(child, id);
      if (found) return found;
   }
   return nullptr;
}

std::vector<XMLTag*> AxisEvaluator::evaluate_child_axis(XMLTag* node) {
   std::vector<XMLTag*> children;
   if (!node) return children;

   // Only include actual tag children, not content nodes
   for (auto& child : node->Children) {
      if (child.isTag()) { // Use helper method from XMLTag
         children.push_back(&child);
      }
   }
   return children;
}

std::vector<XMLTag*> AxisEvaluator::evaluate_descendant_axis(XMLTag* node) {
   std::vector<XMLTag*> descendants;
   if (!node) return descendants;

   // Only process actual tag children, not content nodes
   for (auto& child : node->Children) {
      if (child.isTag()) {
         descendants.push_back(&child);
         auto child_descendants = evaluate_descendant_axis(&child);
         descendants.insert(descendants.end(), child_descendants.begin(), child_descendants.end());
      }
   }
   return descendants;
}

std::vector<XMLTag*> AxisEvaluator::evaluate_parent_axis(XMLTag* node) {
   std::vector<XMLTag*> parents;
   if (node and node->ParentID != 0) {
      XMLTag* parent = find_tag_by_id(node->ParentID);
      if (parent) {
         parents.push_back(parent);
      }
   }
   return parents;
}

std::vector<XMLTag*> AxisEvaluator::evaluate_ancestor_axis(XMLTag* node) {
   std::vector<XMLTag*> ancestors;
   if (!node) return ancestors;

   XMLTag* parent = find_tag_by_id(node->ParentID);
   while (parent) {
      ancestors.push_back(parent);
      parent = find_tag_by_id(parent->ParentID);
   }
   return ancestors;
}

std::vector<XMLTag*> AxisEvaluator::evaluate_following_sibling_axis(XMLTag* node) {
   std::vector<XMLTag*> siblings;
   if (!node) return siblings;

   // Find parent and locate this node in parent's children
   XMLTag* parent = find_tag_by_id(node->ParentID);
   if (!parent) return siblings;

   bool found_self = false;
   for (auto& child : parent->Children) {
      if (found_self and child.isTag()) {
         siblings.push_back(&child);
      } else if (&child IS node) {
         found_self = true;
      }
   }
   return siblings;
}

std::vector<XMLTag*> AxisEvaluator::evaluate_preceding_sibling_axis(XMLTag* node) {
   std::vector<XMLTag*> siblings;
   if (!node) return siblings;

   // Find parent and locate this node in parent's children
   XMLTag* parent = find_tag_by_id(node->ParentID);
   if (!parent) return siblings;

   for (auto& child : parent->Children) {
      if (&child IS node) {
         break; // Stop when we reach the current node
      }
      if (child.isTag()) {
         siblings.push_back(&child);
      }
   }
   return siblings;
}

std::vector<XMLTag*> AxisEvaluator::evaluate_following_axis(XMLTag* node) {
   std::vector<XMLTag*> following;
   if (!node) return following;

   // Get following siblings and their descendants (document order)
   auto following_siblings = evaluate_following_sibling_axis(node);
   for (auto* sibling : following_siblings) {
      if (sibling->isTag()) {
         following.push_back(sibling);
         auto descendants = evaluate_descendant_axis(sibling);
         following.insert(following.end(), descendants.begin(), descendants.end());
      }
   }

   // Recursively check parent's following context for complete XPath semantics
   XMLTag* parent = find_tag_by_id(node->ParentID);
   if (parent) {
      auto parent_following = evaluate_following_axis(parent);
      following.insert(following.end(), parent_following.begin(), parent_following.end());
   }

   return following;
}

std::vector<XMLTag*> AxisEvaluator::evaluate_preceding_axis(XMLTag* node) {
   std::vector<XMLTag*> preceding;
   if (!node) return preceding;

   // Get preceding siblings and their descendants (reverse document order)
   auto preceding_siblings = evaluate_preceding_sibling_axis(node);
   for (auto* sibling : preceding_siblings) {
      if (sibling->isTag()) {
         auto descendants = evaluate_descendant_axis(sibling);
         preceding.insert(preceding.end(), descendants.begin(), descendants.end());
         preceding.push_back(sibling); // Add sibling after its descendants
      }
   }

   // Recursively include parent's preceding context
   XMLTag* parent = find_tag_by_id(node->ParentID);
   if (parent) {
      auto parent_preceding = evaluate_preceding_axis(parent);
      preceding.insert(preceding.end(), parent_preceding.begin(), parent_preceding.end());
   }

   return preceding;
}

std::vector<XMLTag*> AxisEvaluator::evaluate_attribute_axis(XMLTag* node) {
   // In Parasol's XML implementation, attributes are not separate nodes
   // but are stored as properties of the tag. For XPath compatibility,
   // we return an empty set since attribute access is handled differently
   // via the @ syntax in predicates.
   return {};
}

std::vector<XMLTag*> AxisEvaluator::evaluate_namespace_axis(XMLTag* node) {
   // Namespace nodes are not implemented as separate nodes in Parasol
   return {};
}

std::vector<XMLTag*> AxisEvaluator::evaluate_self_axis(XMLTag* node) {
   std::vector<XMLTag*> self;
   if (node) {
      self.push_back(node);
   }
   return self;
}

std::vector<XMLTag*> AxisEvaluator::evaluate_descendant_or_self_axis(XMLTag* node) {
   std::vector<XMLTag*> descendants;
   if (!node) return descendants;

   if (node->isTag()) {
      descendants.push_back(node);
   }

   auto child_descendants = evaluate_descendant_axis(node);
   descendants.insert(descendants.end(), child_descendants.begin(), child_descendants.end());

   return descendants;
}

std::vector<XMLTag*> AxisEvaluator::evaluate_ancestor_or_self_axis(XMLTag* node) {
   std::vector<XMLTag*> ancestors;
   if (!node) return ancestors;

   ancestors.push_back(node);

   auto parent_ancestors = evaluate_ancestor_axis(node);
   ancestors.insert(ancestors.end(), parent_ancestors.begin(), parent_ancestors.end());

   return ancestors;
}

//********************************************************************************************************************
// Document Order Utilities

void AxisEvaluator::sort_document_order(std::vector<XMLTag*>& nodes) {
   std::sort(nodes.begin(), nodes.end(), [this](XMLTag* a, XMLTag* b) {
      return is_before_in_document_order(a, b);
   });
}

bool AxisEvaluator::is_before_in_document_order(XMLTag* node1, XMLTag* node2) {
   // TODO: Implement proper document order comparison
   // For now, use ID comparison as a simple heuristic
   return node1->ID < node2->ID;
}