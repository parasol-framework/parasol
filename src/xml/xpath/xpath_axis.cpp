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
   recycle_namespace_nodes();
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

   // Reserve cache space based on estimated node count (conservative estimate)
   size_t estimated_nodes = xml->Tags.size() * 8; // Assume average 8 nodes per root tag
   id_lookup.reserve(estimated_nodes);

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

// Estimate the likely result size for an axis to enable optimal vector pre-sizing.
size_t AxisEvaluator::estimate_result_size(AxisType Axis, XMLTag *ContextNode) {
   if (!ContextNode) return 0;

   switch (Axis) {
      case AxisType::Child:
         return ContextNode->Children.size();

      case AxisType::Descendant:
      case AxisType::DescendantOrSelf:
         // Conservative estimate: depth * average children per node
         return ContextNode->Children.size() * 4;

      case AxisType::Parent:
      case AxisType::Self:
         return 1;

      case AxisType::Ancestor:
      case AxisType::AncestorOrSelf:
         // Typical XML depth estimate
         return 10;

      case AxisType::FollowingSibling:
      case AxisType::PrecedingSibling: {
         XMLTag *parent = find_tag_by_id(ContextNode->ParentID);
         return parent ? parent->Children.size() : 0;
      }

      case AxisType::Following:
      case AxisType::Preceding:
         // Conservative estimate for document-order traversal
         return 20;

      case AxisType::Attribute:
         return ContextNode->Attribs.size();

      case AxisType::Namespace:
         // Typical namespace count estimate
         return 5;

      default:
         return 4; // Conservative fallback
   }
}

// Standard child-axis traversal: collect direct children in document order.
std::vector<XMLTag *> AxisEvaluator::evaluate_child_axis(XMLTag *Node) {
   std::vector<XMLTag *> children;
   if (!Node) return children;

   // Pre-size vector to avoid reallocations during child collection
   children.reserve(Node->Children.size());

   for (auto& child : Node->Children) {
      children.push_back(&child);
   }
   return children;
}

// Depth-first walk that flattens all descendant tags beneath the context node.
std::vector<XMLTag *> AxisEvaluator::evaluate_descendant_axis(XMLTag *Node) {
   std::vector<XMLTag *> descendants;
   if (!Node) return descendants;

   // Heuristic-based reserve: estimate descendants based on depth * average children
   // Use a conservative estimate assuming average of 3 children per level and depth of 4
   size_t estimated_size = Node->Children.size() * 4;
   descendants.reserve(estimated_size);

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

   // Reserve based on estimated tree depth (typical XML depth is 8-12 levels)
   ancestors.reserve(10);

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

   // Reserve based on sibling count (worst case is all siblings follow this node)
   siblings.reserve(parent->Children.size());

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

   // Reserve based on sibling count (worst case is all siblings precede this node)
   siblings.reserve(parent->Children.size());

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

// Collect namespace declarations using optimized flat vector approach
void AxisEvaluator::collect_namespace_declarations(XMLTag *Node, std::vector<NamespaceDeclaration> &declarations) {
   if (!Node) return;

   // Reuse storage vectors to avoid allocations
   visited_node_ids.clear();
   visited_node_ids.reserve(10); // Typical tree depth

   declarations.clear();
   declarations.reserve(8); // Typical namespace count

   // Add default xml namespace
   declarations.push_back({"xml", "http://www.w3.org/XML/1998/namespace"});

   XMLTag *current = Node;
   while (current) {
      // Check if we've already processed this node to avoid cycles
      bool already_visited = false;
      for (int visited_id : visited_node_ids) {
         if (visited_id IS current->ID) {
            already_visited = true;
            break;
         }
      }

      if (!already_visited) {
         visited_node_ids.push_back(current->ID);

         // Scan attributes for namespace declarations
         for (size_t index = 1; index < current->Attribs.size(); ++index) {
            const auto &attrib = current->Attribs[index];

            if (attrib.Name.rfind("xmlns", 0) != 0) continue;

            std::string prefix;
            if (attrib.Name.length() IS 5) {
               prefix.clear(); // Default namespace
            }
            else if ((attrib.Name.length() > 6) and (attrib.Name[5] IS ':')) {
               prefix = attrib.Name.substr(6);
            }
            else continue;

            // Check if prefix already exists (inner scopes override outer)
            bool found_existing = false;
            for (const auto &existing : declarations) {
               if (existing.prefix IS prefix) {
                  found_existing = true;
                  break;
               }
            }

            if (!found_existing) {
               declarations.push_back({std::move(prefix), attrib.Value});
            }
         }
      }

      if (!current->ParentID) break;
      current = find_tag_by_id(current->ParentID);
   }

   // Sort declarations by prefix for consistent ordering and deduplication
   std::sort(declarations.begin(), declarations.end());

   // Remove any duplicates (shouldn't happen but ensures correctness)
   auto new_end = std::unique(declarations.begin(), declarations.end(),
      [](const NamespaceDeclaration &a, const NamespaceDeclaration &b) {
         return a.prefix IS b.prefix;
      });
   declarations.erase(new_end, declarations.end());
}

// Acquire a namespace node from the pool or create a new one
XMLTag * AxisEvaluator::acquire_namespace_node() {
   if (!namespace_node_pool.empty()) {
      auto node = std::move(namespace_node_pool.back());
      namespace_node_pool.pop_back();

      // Reset the node for reuse
      node->Attribs.clear();
      node->Children.clear();

      XMLTag *raw_ptr = node.get();
      namespace_node_storage.push_back(std::move(node));
      return raw_ptr;
   }

   // Create new node if pool is empty
   auto node = std::make_unique<XMLTag>(0);
   XMLTag *raw_ptr = node.get();
   namespace_node_storage.push_back(std::move(node));
   return raw_ptr;
}

// Recycle namespace nodes back to the pool for reuse
void AxisEvaluator::recycle_namespace_nodes() {
   // Move nodes from storage back to pool for reuse
   for (auto &node : namespace_node_storage) {
      if (node) {
         node->Attribs.clear();
         node->Children.clear();
         namespace_node_pool.push_back(std::move(node));
      }
   }
   namespace_node_storage.clear();
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
// Optimized version using flat vector approach and node pooling.
std::vector<XMLTag *> AxisEvaluator::evaluate_namespace_axis(XMLTag *Node) {
   std::vector<XMLTag *> namespaces;

   if (!Node) return namespaces;

   // Use optimized flat vector approach for namespace collection
   collect_namespace_declarations(Node, namespace_declarations);

   // Pre-size result vector
   namespaces.reserve(namespace_declarations.size());

   // Create namespace nodes using pooling
   auto emit_namespace = [&](const std::string &Prefix, const std::string &URI) {
      XMLTag *node = acquire_namespace_node();

      node->Attribs.emplace_back(Prefix, std::string());

      XMLTag content_node(0);
      content_node.Attribs.emplace_back(std::string(), URI);
      node->Children.push_back(std::move(content_node));

      node->NamespaceID = xml ? xml->registerNamespace(URI) : 0;

      namespaces.push_back(node);
   };

   for (const auto &declaration : namespace_declarations) {
      emit_namespace(declaration.prefix, declaration.uri);
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
