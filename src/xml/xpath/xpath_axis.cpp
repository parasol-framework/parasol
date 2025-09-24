// XPath Axis Evaluation System Implementation

#include <algorithm>

//********************************************************************************************************************
// AxisEvaluator Implementation

std::vector<XMLTag *> AxisEvaluator::evaluate_axis(AxisType Axis, XMLTag *ContextNode) {
   if (!ContextNode) return {};

   switch (Axis) {
      case AxisType::Child:
         return evaluate_child_axis(ContextNode);
      case AxisType::Descendant:
         return evaluate_descendant_axis(ContextNode);
      case AxisType::Parent:
         return evaluate_parent_axis(ContextNode);
      case AxisType::Ancestor:
         return evaluate_ancestor_axis(ContextNode);
      case AxisType::FollowingSibling:
         return evaluate_following_sibling_axis(ContextNode);
      case AxisType::PrecedingSibling:
         return evaluate_preceding_sibling_axis(ContextNode);
      case AxisType::Following:
         return evaluate_following_axis(ContextNode);
      case AxisType::Preceding:
         return evaluate_preceding_axis(ContextNode);
      case AxisType::Attribute:
         return evaluate_attribute_axis(ContextNode);
      case AxisType::Namespace:
         return evaluate_namespace_axis(ContextNode);
      case AxisType::Self:
         return evaluate_self_axis(ContextNode);
      case AxisType::DescendantOrSelf:
         return evaluate_descendant_or_self_axis(ContextNode);
      case AxisType::AncestorOrSelf:
         return evaluate_ancestor_or_self_axis(ContextNode);
      default:
         return {};
   }
}

AxisType AxisEvaluator::parse_axis_name(std::string_view AxisName) {
   if (AxisName IS "child") return AxisType::Child;
   else if (AxisName IS "descendant") return AxisType::Descendant;
   else if (AxisName IS "descendant-or-self") return AxisType::DescendantOrSelf;
   else if (AxisName IS "following") return AxisType::Following;
   else if (AxisName IS "following-sibling") return AxisType::FollowingSibling;
   else if (AxisName IS "parent") return AxisType::Parent;
   else if (AxisName IS "ancestor") return AxisType::Ancestor;
   else if (AxisName IS "ancestor-or-self") return AxisType::AncestorOrSelf;
   else if (AxisName IS "preceding") return AxisType::Preceding;
   else if (AxisName IS "preceding-sibling") return AxisType::PrecedingSibling;
   else if (AxisName IS "self") return AxisType::Self;
   else if (AxisName IS "attribute") return AxisType::Attribute;
   else if (AxisName IS "namespace") return AxisType::Namespace;
   else return AxisType::Child; // Default axis
}

std::string AxisEvaluator::axis_name_to_string(AxisType Axis) {
   switch (Axis) {
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

bool AxisEvaluator::is_reverse_axis(AxisType Axis) {
   switch (Axis) {
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

XMLTag * AxisEvaluator::find_tag_by_id(int ID) {
   if (ID IS 0) return nullptr;

   // Search through the entire document tree using the Tags vector
   for (auto& tag : xml->Tags) {
      auto found = find_tag_recursive(tag, ID);
      if (found) return found;
   }
   return nullptr;
}

XMLTag * AxisEvaluator::find_tag_recursive(XMLTag &Tag, int ID) {
   if (Tag.ID IS ID) return &Tag;

   for (auto& child : Tag.Children) {
      auto found = find_tag_recursive(child, ID);
      if (found) return found;
   }
   return nullptr;
}

std::vector<XMLTag *> AxisEvaluator::evaluate_child_axis(XMLTag *Node) {
   std::vector<XMLTag *> children;
   if (!Node) return children;

   // Only include actual tag children, not content nodes
   for (auto& child : Node->Children) {
      if (child.isTag()) { // Use helper method from XMLTag
         children.push_back(&child);
      }
   }
   return children;
}

std::vector<XMLTag *> AxisEvaluator::evaluate_descendant_axis(XMLTag *Node) {
   std::vector<XMLTag *> descendants;
   if (!Node) return descendants;

   // Only process actual tag children, not content nodes
   for (auto& child : Node->Children) {
      if (child.isTag()) {
         descendants.push_back(&child);
         auto child_descendants = evaluate_descendant_axis(&child);
         descendants.insert(descendants.end(), child_descendants.begin(), child_descendants.end());
      }
   }
   return descendants;
}

std::vector<XMLTag *> AxisEvaluator::evaluate_parent_axis(XMLTag *Node) {
   std::vector<XMLTag *> parents;
   if (Node and Node->ParentID != 0) {
      XMLTag *parent = find_tag_by_id(Node->ParentID);
      if (parent) {
         parents.push_back(parent);
      }
   }
   return parents;
}

std::vector<XMLTag *> AxisEvaluator::evaluate_ancestor_axis(XMLTag *Node) {
   std::vector<XMLTag *> ancestors;
   if (!Node) return ancestors;

   XMLTag *parent = find_tag_by_id(Node->ParentID);
   while (parent) {
      ancestors.push_back(parent);
      parent = find_tag_by_id(parent->ParentID);
   }
   return ancestors;
}

std::vector<XMLTag *> AxisEvaluator::evaluate_following_sibling_axis(XMLTag *Node) {
   std::vector<XMLTag *> siblings;
   if (!Node) return siblings;

   // Find parent and locate this node in parent's children
   XMLTag *parent = find_tag_by_id(Node->ParentID);
   if (!parent) return siblings;

   bool found_self = false;
   for (auto& child : parent->Children) {
      if (found_self and child.isTag()) {
         siblings.push_back(&child);
      } else if (&child IS Node) {
         found_self = true;
      }
   }
   return siblings;
}

std::vector<XMLTag *> AxisEvaluator::evaluate_preceding_sibling_axis(XMLTag *Node) {
   std::vector<XMLTag *> siblings;
   if (!Node) return siblings;

   // Find parent and locate this node in parent's children
   XMLTag *parent = find_tag_by_id(Node->ParentID);
   if (!parent) return siblings;

   for (auto& child : parent->Children) {
      if (&child IS Node) {
         break; // Stop when we reach the current node
      }
      if (child.isTag()) {
         siblings.push_back(&child);
      }
   }
   std::reverse(siblings.begin(), siblings.end());
   return siblings;
}

std::vector<XMLTag *> AxisEvaluator::evaluate_following_axis(XMLTag *Node) {
   std::vector<XMLTag *> following;
   if (!Node) return following;

   // Get following siblings and their descendants (document order)
   auto following_siblings = evaluate_following_sibling_axis(Node);
   for (auto *sibling : following_siblings) {
      if (sibling->isTag()) {
         following.push_back(sibling);
         auto descendants = evaluate_descendant_axis(sibling);
         following.insert(following.end(), descendants.begin(), descendants.end());
      }
   }

   // Recursively check parent's following context for complete XPath semantics
   XMLTag *parent = find_tag_by_id(Node->ParentID);
   if (parent) {
      auto parent_following = evaluate_following_axis(parent);
      following.insert(following.end(), parent_following.begin(), parent_following.end());
   }

   return following;
}

void AxisEvaluator::collect_subtree_reverse(XMLTag *Node, std::vector<XMLTag *> &Output) {
   if (!Node or !Node->isTag()) return;

   for (auto child = Node->Children.rbegin(); child != Node->Children.rend(); ++child) {
      if (!child->isTag()) continue;
      collect_subtree_reverse(&(*child), Output);
   }

   Output.push_back(Node);
}

std::vector<XMLTag *> AxisEvaluator::evaluate_preceding_axis(XMLTag *Node) {
   std::vector<XMLTag *> preceding;
   if (!Node) return preceding;

   // Get preceding siblings and their descendants (reverse document order)
   auto preceding_siblings = evaluate_preceding_sibling_axis(Node);
   for (auto *sibling : preceding_siblings) {
      collect_subtree_reverse(sibling, preceding);
   }

   // Recursively include parent's preceding context
   XMLTag *parent = find_tag_by_id(Node->ParentID);
   if (parent) {
      auto parent_preceding = evaluate_preceding_axis(parent);
      preceding.insert(preceding.end(), parent_preceding.begin(), parent_preceding.end());
   }

   return preceding;
}

std::vector<XMLTag *> AxisEvaluator::evaluate_attribute_axis(XMLTag *Node) {
   // In Parasol's XML implementation, attributes are not separate nodes
   // but are stored as properties of the tag. For XPath compatibility,
   // we return an empty set since attribute access is handled differently
   // via the @ syntax in predicates.
   return {};
}

std::vector<XMLTag *> AxisEvaluator::evaluate_namespace_axis(XMLTag *Node) {
   // Namespace nodes are not implemented as separate nodes in Parasol
   return {};
}

std::vector<XMLTag *> AxisEvaluator::evaluate_self_axis(XMLTag *Node) {
   std::vector<XMLTag *> self;
   if (Node) {
      self.push_back(Node);
   }
   return self;
}

std::vector<XMLTag *> AxisEvaluator::evaluate_descendant_or_self_axis(XMLTag *Node) {
   std::vector<XMLTag *> descendants;
   if (!Node) return descendants;

   if (Node->isTag()) {
      descendants.push_back(Node);
   }

   auto child_descendants = evaluate_descendant_axis(Node);
   descendants.insert(descendants.end(), child_descendants.begin(), child_descendants.end());

   return descendants;
}

std::vector<XMLTag *> AxisEvaluator::evaluate_ancestor_or_self_axis(XMLTag *Node) {
   std::vector<XMLTag *> ancestors;
   if (!Node) return ancestors;

   ancestors.push_back(Node);

   auto parent_ancestors = evaluate_ancestor_axis(Node);
   ancestors.insert(ancestors.end(), parent_ancestors.begin(), parent_ancestors.end());

   return ancestors;
}

//********************************************************************************************************************
// Document Order Utilities

void AxisEvaluator::sort_document_order(std::vector<XMLTag *> &Nodes) {
   std::sort(Nodes.begin(), Nodes.end(), [this](XMLTag *a, XMLTag *b) {
      return is_before_in_document_order(a, b);
   });
}

bool AxisEvaluator::is_before_in_document_order(XMLTag *Node1, XMLTag *Node2) {
   // TODO: Implement proper document order comparison
   // For now, use ID comparison as a simple heuristic
   return Node1->ID < Node2->ID;
}
