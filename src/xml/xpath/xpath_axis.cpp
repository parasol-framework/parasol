//********************************************************************************************************************
// XPath Axis Evaluation System Implementation
//********************************************************************************************************************
//
// The axis evaluator encapsulates the traversal rules needed to support XPath's location steps in
// Parasol's XML engine.  XPath exposes a wide variety of axes—child, ancestor, following, namespace,
// and so forth—that each describe a different relationship between nodes.  Translating those abstract
// relationships into concrete navigation across the engine's tree representation requires a fair
// amount of bookkeeping: we need to preserve document order, honour namespace scoping, emulate axes
// that are not represented explicitly in the DOM (such as attributes or namespaces), and provide
// deterministic handling for synthetic nodes used by the evaluator.
//
// This file implements the traversal logic in a stand-alone helper so that the tokenizer, parser, and
// evaluator can remain focused on syntactic and semantic concerns.  The AxisEvaluator is responsible
// for translating an AxisType into a set of XMLTag pointers, normalising the resulting node sets, and
// providing utility operations that encode XPath's ordering semantics.  Splitting these operations out
// keeps the evaluator readable and makes it easier to extend axis support in the future (for example,
// by adding document order caches or debugging hooks).

#include <algorithm>
#include <array>
#include <map>
#include <ranges>
#include <unordered_set>

namespace {

struct AxisNameMapping {
   AxisType Type;
   std::string_view Name;
};

constexpr std::array axis_mappings{
   AxisNameMapping{ AxisType::Child, "child" },
   AxisNameMapping{ AxisType::Descendant, "descendant" },
   AxisNameMapping{ AxisType::DescendantOrSelf, "descendant-or-self" },
   AxisNameMapping{ AxisType::Following, "following" },
   AxisNameMapping{ AxisType::FollowingSibling, "following-sibling" },
   AxisNameMapping{ AxisType::Parent, "parent" },
   AxisNameMapping{ AxisType::Ancestor, "ancestor" },
   AxisNameMapping{ AxisType::AncestorOrSelf, "ancestor-or-self" },
   AxisNameMapping{ AxisType::Preceding, "preceding" },
   AxisNameMapping{ AxisType::PrecedingSibling, "preceding-sibling" },
   AxisNameMapping{ AxisType::Self, "self" },
   AxisNameMapping{ AxisType::Attribute, "attribute" },
   AxisNameMapping{ AxisType::Namespace, "namespace" }
};

constexpr std::array reverse_axes{
   AxisType::Ancestor,
   AxisType::AncestorOrSelf,
   AxisType::Preceding,
   AxisType::PrecedingSibling
};

}

//********************************************************************************************************************
// AxisEvaluator Implementation

// Dispatch helper that selects the concrete traversal routine for a requested axis.
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

// Clear any synthetic namespace nodes created by namespace axis evaluation.
void AxisEvaluator::reset_namespace_nodes() {
   namespace_node_storage.clear();
}

AxisType AxisEvaluator::parse_axis_name(std::string_view AxisName) {
   auto match = std::ranges::find_if(axis_mappings, [AxisName](const AxisNameMapping &Entry) {
      return AxisName IS Entry.Name;
   });

   if (match != axis_mappings.end()) return match->Type;
   return AxisType::Child;
}

std::string_view AxisEvaluator::axis_name_to_string(AxisType Axis) {
   auto match = std::ranges::find_if(axis_mappings, [Axis](const AxisNameMapping &Entry) {
      return Axis IS Entry.Type;
   });

   if (match != axis_mappings.end()) return match->Name;
   return "child";
}

bool AxisEvaluator::is_reverse_axis(AxisType Axis) {
   return std::ranges::any_of(reverse_axes, [Axis](AxisType Candidate) {
      return Candidate IS Axis;
   });
}

//********************************************************************************************************************
// Helper Methods for Specific Axes

// Build or refresh a cache that maps XML node IDs to their corresponding tags.
void AxisEvaluator::build_id_cache() {
   id_lookup.clear();
   if (!xml) {
      id_cache_built = true;
      return;
   }

   std::vector<XMLTag *> stack;
   stack.reserve(xml->Tags.size());

   for (auto &root_tag : xml->Tags) {
      stack.push_back(&root_tag);

      while (!stack.empty()) {
         XMLTag *current = stack.back();
         stack.pop_back();

         id_lookup[current->ID] = current;

         if (current->Children.empty()) continue;

         for (auto child = current->Children.rbegin(); child != current->Children.rend(); ++child) {
            stack.push_back(&(*child));
         }
      }
   }

   id_cache_built = true;
}

// Perform an ID-based lookup with caching to avoid repeated depth-first scans.
XMLTag * AxisEvaluator::find_tag_by_id(int ID) {
   if ((ID IS 0) or (!xml)) return nullptr;

   if (!id_cache_built) build_id_cache();

   auto iter = id_lookup.find(ID);
   if (iter != id_lookup.end()) return iter->second;

   build_id_cache();
   iter = id_lookup.find(ID);
   if (iter != id_lookup.end()) return iter->second;

   return nullptr;
}

// Standard child-axis traversal: collect direct children in document order.
std::vector<XMLTag *> AxisEvaluator::evaluate_child_axis(XMLTag *Node) {
   std::vector<XMLTag *> children;
   if (!Node) return children;

   for (auto& child : Node->Children) {
      children.push_back(&child);
   }
   return children;
}

// Depth-first walk that flattens all descendant tags beneath the context node.
std::vector<XMLTag *> AxisEvaluator::evaluate_descendant_axis(XMLTag *Node) {
   std::vector<XMLTag *> descendants;
   if (!Node) return descendants;

   std::vector<XMLTag *> stack;
   stack.reserve(Node->Children.size());

   for (auto &child : Node->Children) {
      auto *child_ptr = &child;
      descendants.push_back(child_ptr);
      if (child.isTag()) stack.push_back(child_ptr);
   }

   while (!stack.empty()) {
      XMLTag *current = stack.back();
      stack.pop_back();

      for (auto &grandchild : current->Children) {
         auto *grandchild_ptr = &grandchild;
         descendants.push_back(grandchild_ptr);
         if (grandchild.isTag()) stack.push_back(grandchild_ptr);
      }
   }

   return descendants;
}

// Parent axis resolves a single parent node by ID reference.
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

// Ascend towards the root, collecting each ancestor encountered along the way.
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
      if (found_self) {
         siblings.push_back(&child);
      }

      if (&child IS Node) {
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

      siblings.push_back(&child);
   }
   std::reverse(siblings.begin(), siblings.end());
   return siblings;
}

// Following axis enumerates nodes that appear after the context node in document order.
std::vector<XMLTag *> AxisEvaluator::evaluate_following_axis(XMLTag *Node) {
   std::vector<XMLTag *> following;
   if (!Node) return following;

   // Get following siblings and their descendants (document order)
   auto following_siblings = evaluate_following_sibling_axis(Node);
   for (auto *sibling : following_siblings) {
      following.push_back(sibling);

      if (sibling->isTag()) {
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

// Helper that traverses a subtree in reverse document order, used by the preceding axis.
void AxisEvaluator::collect_subtree_reverse(XMLTag *Node, std::vector<XMLTag *> &Output) {
   if (!Node) return;

   for (auto child = Node->Children.rbegin(); child != Node->Children.rend(); ++child) {
      collect_subtree_reverse(&(*child), Output);
   }

   Output.push_back(Node);
}

// Preceding axis mirrors the following axis but in reverse.
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

// Namespace axis is modelled with transient nodes that expose in-scope prefix mappings.
std::vector<XMLTag *> AxisEvaluator::evaluate_namespace_axis(XMLTag *Node) {
   std::vector<XMLTag *> namespaces;

   if (!Node) return namespaces;

   std::map<std::string, std::string, std::less<>> in_scope;

   auto add_namespace = [&](const std::string &Prefix, const std::string &URI) {
      if (in_scope.find(Prefix) != in_scope.end()) return;
      in_scope.insert({ Prefix, URI });
   };

   add_namespace("xml", "http://www.w3.org/XML/1998/namespace");

   std::unordered_set<int> visited_ids;
   XMLTag *current = Node;

   while (current) {
      if (visited_ids.insert(current->ID).second) {
         for (size_t index = 1; index < current->Attribs.size(); ++index) {
            const auto &attrib = current->Attribs[index];

            if (attrib.Name.rfind("xmlns", 0) != 0) continue;

            std::string prefix;
            if (attrib.Name.length() IS 5) prefix.clear();
            else if ((attrib.Name.length() > 6) and (attrib.Name[5] IS ':')) {
               prefix = attrib.Name.substr(6);
            }
            else continue;

            add_namespace(prefix, attrib.Value);
         }
      }

      if (!current->ParentID) break;
      current = find_tag_by_id(current->ParentID);
   }

   auto emit_namespace = [&](const std::string &Prefix, const std::string &URI) {
      auto node = std::make_unique<XMLTag>(0);
      node->Attribs.clear();
      node->Children.clear();
      node->Attribs.emplace_back(Prefix, std::string());

      XMLTag content_node(0);
      content_node.Attribs.clear();
      content_node.Children.clear();
      content_node.Attribs.emplace_back(std::string(), URI);
      node->Children.push_back(content_node);

      node->NamespaceID = xml ? xml->registerNamespace(URI) : 0;

      namespaces.push_back(node.get());
      namespace_node_storage.push_back(std::move(node));
   };

   for (const auto &entry : in_scope) {
      emit_namespace(entry.first, entry.second);
   }

   return namespaces;
}

std::vector<XMLTag *> AxisEvaluator::evaluate_self_axis(XMLTag *Node) {
   std::vector<XMLTag *> self;
   if (Node) {
      self.push_back(Node);
   }
   return self;
}

// Combine self and descendant traversal for the descendant-or-self axis.
std::vector<XMLTag *> AxisEvaluator::evaluate_descendant_or_self_axis(XMLTag *Node) {
   std::vector<XMLTag *> descendants;
   if (!Node) return descendants;

   descendants.push_back(Node);

   auto child_descendants = evaluate_descendant_axis(Node);
   descendants.insert(descendants.end(), child_descendants.begin(), child_descendants.end());

   return descendants;
}

// Combine self and ancestor traversal for the ancestor-or-self axis.
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

// Stable ordering is critical for XPath equality semantics; this method enforces document order.
void AxisEvaluator::sort_document_order(std::vector<XMLTag *> &Nodes) {
   if (Nodes.size() < 2) return;

   std::sort(Nodes.begin(), Nodes.end(), [this](XMLTag *Left, XMLTag *Right) {
      if (Left IS Right) return false;
      if (!Left) return false;
      if (!Right) return true;
      return is_before_in_document_order(Left, Right);
   });
}

// Construct the chain of ancestors from the root to the specified node.  The resulting path enables
// relative ordering checks for arbitrarily distant nodes.
std::vector<XMLTag *> AxisEvaluator::build_ancestor_path(XMLTag *Node)
{
   std::vector<XMLTag *> path;
   XMLTag *current = Node;

   while (current) {
      path.push_back(current);
      if (!current->ParentID) break;
      current = find_tag_by_id(current->ParentID);
   }

   std::reverse(path.begin(), path.end());
   return path;
}

// Evaluate whether Node1 precedes Node2 in document order, handling synthetic nodes gracefully.
bool AxisEvaluator::is_before_in_document_order(XMLTag *Node1, XMLTag *Node2) {
   if ((!Node1) or (!Node2) or (Node1 IS Node2)) return false;

   if ((Node1->ID IS 0) or (Node2->ID IS 0)) {
      if (Node1->ID IS Node2->ID) return Node1 < Node2;
      return Node1->ID < Node2->ID;
   }

   auto path1 = build_ancestor_path(Node1);
   auto path2 = build_ancestor_path(Node2);

   if (path1.empty() or path2.empty()) return Node1 < Node2;

   size_t max_common = std::min(path1.size(), path2.size());
   size_t index = 0;

   while ((index < max_common) and (path1[index] IS path2[index])) index++;

   if (index IS max_common) {
      return path1.size() < path2.size();
   }

   if (index IS 0) {
      return path1[index]->ID < path2[index]->ID;
   }

   XMLTag *parent = path1[index - 1];
   XMLTag *branch1 = path1[index];
   XMLTag *branch2 = path2[index];

   for (auto &child : parent->Children) {
      if (&child IS branch1) return true;
      if (&child IS branch2) return false;
   }

   return branch1->ID < branch2->ID;
}

// Remove null entries, enforce document order, and deduplicate the node-set to satisfy XPath rules.
void AxisEvaluator::normalise_node_set(std::vector<XMLTag *> &Nodes)
{
   std::erase_if(Nodes, [](XMLTag *Node) { return !Node; });
   if (Nodes.size() < 2) return;

   sort_document_order(Nodes);

   auto new_end = std::unique(Nodes.begin(), Nodes.end(), [](XMLTag *Left, XMLTag *Right) {
      return Left IS Right;
   });

   Nodes.erase(new_end, Nodes.end());
}
