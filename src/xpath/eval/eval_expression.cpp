
#include "eval.h"
#include "eval_detail.h"
#include "../api/xpath_functions.h"
#include "../../xml/schema/schema_types.h"
#include "../../xml/xml.h"
#include <parasol/strings.hpp>
#include <format>
#include <utility>

struct SequenceEntry {
   XMLTag * node = nullptr;
   const XMLAttrib * attribute = nullptr;
   std::string string_value;
};

struct ForBindingDefinition {
   std::string name;
   const XPathNode * sequence = nullptr;
};

struct QuantifiedBindingDefinition {
   std::string name;
   const XPathNode * sequence = nullptr;
};

//********************************************************************************************************************
// File-local helpers extracted from large embedded lambdas to reduce function length and improve readability.

static std::string canonicalise_variable_qname(std::string_view Candidate,
   const XQueryProlog &SourceProlog, const extXML *Document)
{
   if ((Candidate.size() > 2) and (Candidate[0] IS 'Q') and (Candidate[1] IS '{')) {
      return std::string(Candidate);
   }

   size_t colon_position = Candidate.find(':');
   if (colon_position != std::string_view::npos) {
      std::string prefix(Candidate.substr(0, colon_position));
      std::string_view local_name_view = Candidate.substr(colon_position + 1);

      auto uri_entry = SourceProlog.declared_namespace_uris.find(prefix);
      if (uri_entry != SourceProlog.declared_namespace_uris.end()) {
         return std::format("Q{{{}}}{}", uri_entry->second, local_name_view);
      }

      if (Document) {
         auto prefix_it = Document->Prefixes.find(prefix);
         if (prefix_it != Document->Prefixes.end()) {
            auto ns_it = Document->NSRegistry.find(prefix_it->second);
            if (ns_it != Document->NSRegistry.end()) {
               return std::format("Q{{{}}}{}", ns_it->second, local_name_view);
            }
         }
      }
   }

   return std::string(Candidate);
}

//********************************************************************************************************************

static void append_value_to_sequence(const XPathVal &Value, std::vector<SequenceEntry> &Entries,
   int &NextConstructedNodeId, std::vector<std::unique_ptr<XMLTag>> &ConstructedNodes)
{
   if (Value.Type IS XPVT::NodeSet) {
      bool use_override = Value.node_set_string_override.has_value() and Value.node_set_string_values.empty();
      for (size_t index = 0; index < Value.node_set.size(); ++index) {
         XMLTag *node = Value.node_set[index];
         if (not node) continue;

         const XMLAttrib *attribute = nullptr;
         if (index < Value.node_set_attributes.size()) attribute = Value.node_set_attributes[index];

         std::string item_string;
         if (index < Value.node_set_string_values.size()) item_string = Value.node_set_string_values[index];
         else if (use_override) item_string = *Value.node_set_string_override;
         else if (attribute) item_string = attribute->Value;
         else item_string = XPathVal::node_string_value(node);

         Entries.push_back({ node, attribute, std::move(item_string) });
      }
      return;
   }

   std::string text = Value.to_string();
   pf::vector<XMLAttrib> text_attribs;
   text_attribs.emplace_back("", text);

   XMLTag text_node(NextConstructedNodeId--, 0, text_attribs);
   text_node.ParentID = 0;

   auto stored = std::make_unique<XMLTag>(std::move(text_node));
   XMLTag *root = stored.get();
   ConstructedNodes.push_back(std::move(stored));

   Entries.push_back({ root, nullptr, std::move(text) });
}

//********************************************************************************************************************

enum class BinaryOperationKind {
   AND,
   OR,
   UNION,
   INTERSECT,
   EXCEPT,
   COMMA,
   EQ,
   NE,
   EQ_WORD,
   NE_WORD,
   LT,
   LE,
   GT,
   GE,
   ADD,
   SUB,
   MUL,
   DIV,
   MOD,
   UNKNOWN
};

static BinaryOperationKind map_binary_operation(std::string_view Op)
{
   if (Op IS "and") return BinaryOperationKind::AND;
   if (Op IS "or") return BinaryOperationKind::OR;
   if (Op IS "|") return BinaryOperationKind::UNION;
   if (Op IS "intersect") return BinaryOperationKind::INTERSECT;
   if (Op IS "except") return BinaryOperationKind::EXCEPT;
   if (Op IS ",") return BinaryOperationKind::COMMA;
   if (Op IS "=") return BinaryOperationKind::EQ;
   if (Op IS "!=") return BinaryOperationKind::NE;
   if (Op IS "eq") return BinaryOperationKind::EQ_WORD;
   if (Op IS "ne") return BinaryOperationKind::NE_WORD;
   if ((Op IS "<") or (Op IS "lt")) return BinaryOperationKind::LT;
   if ((Op IS "<=") or (Op IS "le")) return BinaryOperationKind::LE;
   if ((Op IS ">") or (Op IS "gt")) return BinaryOperationKind::GT;
   if ((Op IS ">=") or (Op IS "ge")) return BinaryOperationKind::GE;
   if (Op IS "+") return BinaryOperationKind::ADD;
   if (Op IS "-") return BinaryOperationKind::SUB;
   if (Op IS "*") return BinaryOperationKind::MUL;
   if (Op IS "div") return BinaryOperationKind::DIV;
   if (Op IS "mod") return BinaryOperationKind::MOD;
   return BinaryOperationKind::UNKNOWN;
}

//********************************************************************************************************************

static bool append_iteration_value_helper(const XPathVal &IterationValue, NODES &CombinedNodes,
   std::vector<const XMLAttrib *> &CombinedAttributes, std::vector<std::string> &CombinedStrings,
   std::optional<std::string> &CombinedOverride)
{
   if (IterationValue.Type IS XPVT::NodeSet) {
      size_t length = IterationValue.node_set.size();
      if (length < IterationValue.node_set_attributes.size()) length = IterationValue.node_set_attributes.size();
      if (length < IterationValue.node_set_string_values.size()) length = IterationValue.node_set_string_values.size();
      if ((length IS 0) and IterationValue.node_set_string_override.has_value()) length = 1;

      for (size_t node_index = 0; node_index < length; ++node_index) {
         XMLTag *node = node_index < IterationValue.node_set.size() ? IterationValue.node_set[node_index] : nullptr;
         CombinedNodes.push_back(node);

         const XMLAttrib *attribute = nullptr;
         if (node_index < IterationValue.node_set_attributes.size()) attribute = IterationValue.node_set_attributes[node_index];
         CombinedAttributes.push_back(attribute);

         std::string node_string;
         bool use_override = IterationValue.node_set_string_override.has_value() and IterationValue.node_set_string_values.empty() and (node_index IS 0);
         if (node_index < IterationValue.node_set_string_values.size()) node_string = IterationValue.node_set_string_values[node_index];
         else if (use_override) node_string = *IterationValue.node_set_string_override;
         else if (attribute) node_string = attribute->Value;
         else if (node) node_string = XPathVal::node_string_value(node);

         CombinedStrings.push_back(node_string);
         if (not CombinedOverride.has_value()) CombinedOverride = node_string;
      }

      if (IterationValue.node_set_string_override.has_value() and IterationValue.node_set_string_values.empty()) {
         if (not CombinedOverride.has_value()) CombinedOverride = IterationValue.node_set_string_override;
      }

      return true;
   }

   if (IterationValue.is_empty()) return true;

   std::string atomic_string = IterationValue.to_string();
   CombinedNodes.push_back(nullptr);
   CombinedAttributes.push_back(nullptr);
   CombinedStrings.push_back(atomic_string);
   if (not CombinedOverride.has_value()) CombinedOverride = atomic_string;
   return true;
}

//********************************************************************************************************************

static bool evaluate_for_bindings_recursive(XPathEvaluator &Self, XPathContext &Context,
   const std::vector<ForBindingDefinition> &Bindings, size_t BindingIndex,
   const XPathNode *ReturnNode, uint32_t CurrentPrefix, NODES &CombinedNodes,
   std::vector<const XMLAttrib *> &CombinedAttributes, std::vector<std::string> &CombinedStrings,
   std::optional<std::string> &CombinedOverride, bool &ExpressionUnsupported)
{
   if (BindingIndex >= Bindings.size()) {
      auto iteration_value = Self.evaluate_expression(ReturnNode, CurrentPrefix);
      if (ExpressionUnsupported) return false;
      if (not append_iteration_value_helper(iteration_value, CombinedNodes, CombinedAttributes, CombinedStrings, CombinedOverride)) return false;
      return true;
   }

   const auto &binding = Bindings[BindingIndex];
   if (not binding.sequence) {
      ExpressionUnsupported = true;
      return false;
   }

   const std::string variable_name = binding.name;

   auto sequence_value = Self.evaluate_expression(binding.sequence, CurrentPrefix);
   if (ExpressionUnsupported) return false;

   if (sequence_value.Type != XPVT::NodeSet) {
      ExpressionUnsupported = true;
      return false;
   }

   size_t sequence_size = sequence_value.node_set.size();

   if (sequence_size IS 0) return true;

   for (size_t index = 0; index < sequence_size; ++index) {
      XMLTag *item_node = sequence_value.node_set[index];
      const XMLAttrib *item_attribute = nullptr;
      if (index < sequence_value.node_set_attributes.size()) {
         item_attribute = sequence_value.node_set_attributes[index];
      }

      XPathVal bound_value;
      bound_value.Type = XPVT::NodeSet;
      bound_value.preserve_node_order = false;
      bound_value.node_set.push_back(item_node);
      bound_value.node_set_attributes.push_back(item_attribute);

      std::string item_string;
      bool use_override = sequence_value.node_set_string_override.has_value() and (index IS 0) and sequence_value.node_set_string_values.empty();
      if (index < sequence_value.node_set_string_values.size()) {
         item_string = sequence_value.node_set_string_values[index];
      }
      else if (use_override) item_string = *sequence_value.node_set_string_override;
      else if (item_node) item_string = XPathVal::node_string_value(item_node);

      bound_value.node_set_string_values.push_back(item_string);
      bound_value.node_set_string_override = item_string;

      VariableBindingGuard iteration_guard(Context, variable_name, std::move(bound_value));

      Self.push_context(item_node, index + 1, sequence_size, item_attribute);
      bool iteration_ok = evaluate_for_bindings_recursive(Self, Context, Bindings, BindingIndex + 1,
         ReturnNode, CurrentPrefix, CombinedNodes, CombinedAttributes, CombinedStrings, CombinedOverride, ExpressionUnsupported);
      Self.pop_context();

      if (not iteration_ok) return false;
      if (ExpressionUnsupported) return false;
   }

   return true;
}

//********************************************************************************************************************

static bool evaluate_quantified_binding_recursive(XPathEvaluator &Self, XPathContext &Context,
   const std::vector<QuantifiedBindingDefinition> &Bindings, size_t BindingIndex,
   bool IsSome, bool IsEvery, const XPathNode *ConditionNode, uint32_t CurrentPrefix,
   bool &ExpressionUnsupported)
{
   if (BindingIndex >= Bindings.size()) {
      auto condition_value = Self.evaluate_expression(ConditionNode, CurrentPrefix);
      if (ExpressionUnsupported) return false;
      return condition_value.to_boolean();
   }

   const auto &binding = Bindings[BindingIndex];
   if (not binding.sequence) {
      ExpressionUnsupported = true;
      return false;
   }

   const std::string variable_name = binding.name;

   auto sequence_value = Self.evaluate_expression(binding.sequence, CurrentPrefix);
   if (ExpressionUnsupported) return false;

   if (sequence_value.Type != XPVT::NodeSet) {
      ExpressionUnsupported = true;
      return false;
   }

   size_t sequence_size = sequence_value.node_set.size();

   if (sequence_size IS 0) return IsEvery;

   for (size_t index = 0; index < sequence_size; ++index) {
      XMLTag *item_node = sequence_value.node_set[index];
      const XMLAttrib *item_attribute = nullptr;
      if (index < sequence_value.node_set_attributes.size()) {
         item_attribute = sequence_value.node_set_attributes[index];
      }

      XPathVal bound_value;
      bound_value.Type = XPVT::NodeSet;
      bound_value.preserve_node_order = false;
      bound_value.node_set.push_back(item_node);
      bound_value.node_set_attributes.push_back(item_attribute);

      std::string item_string;
      bool use_override = sequence_value.node_set_string_override.has_value() and (index IS 0) and sequence_value.node_set_string_values.empty();
      if (index < sequence_value.node_set_string_values.size()) {
         item_string = sequence_value.node_set_string_values[index];
      }
      else if (use_override) item_string = *sequence_value.node_set_string_override;
      else if (item_node) item_string = XPathVal::node_string_value(item_node);

      bound_value.node_set_string_values.push_back(item_string);
      bound_value.node_set_string_override = item_string;

      VariableBindingGuard iteration_guard(Context, variable_name, std::move(bound_value));

      Self.push_context(item_node, index + 1, sequence_size, item_attribute);
      bool branch_result = evaluate_quantified_binding_recursive(Self, Context, Bindings, BindingIndex + 1,
         IsSome, IsEvery, ConditionNode, CurrentPrefix, ExpressionUnsupported);
      Self.pop_context();

      if (ExpressionUnsupported) return false;

      if (branch_result) {
         if (IsSome) return true;
      }
      else if (IsEvery) return false;
   }

   return IsEvery;
}

//********************************************************************************************************************
// Resolves a variable reference by consulting the dynamic context, document bindings, and finally the prolog.

bool XPathEvaluator::resolve_variable_value(std::string_view QName, uint32_t CurrentPrefix,
   XPathVal &OutValue, const XPathNode *ReferenceNode)
{
   std::string name(QName);
   std::string normalised_name(name);

   if (context.variables) {
      auto local_variable = context.variables->find(name);
      if (local_variable != context.variables->end()) {
         OutValue = local_variable->second;
         return true;
      }
   }

   if (xml) {
      auto xml_variable = xml->Variables.find(name);
      if (xml_variable != xml->Variables.end()) {
         OutValue = XPathVal(xml_variable->second);
         return true;
      }
   }

   auto prolog = context.prolog;
   if (not prolog) return false;

   const XQueryVariable *variable = prolog->find_variable(QName);
   std::shared_ptr<XQueryProlog> owner_prolog = prolog;
   auto active_module_cache = context.module_cache;
   std::string module_uri;
   std::string imported_local_name;
   std::string canonical_lookup;

   if (not variable) {
      uint32_t namespace_hash = 0;

      if ((name.size() > 2) and (name[0] IS 'Q') and (name[1] IS '{')) {
         size_t closing = name.find('}');
         if (closing != std::string::npos) {
            module_uri = name.substr(2, closing - 2);
            imported_local_name = name.substr(closing + 1);
            if (not module_uri.empty()) namespace_hash = pf::strhash(module_uri);
         }
      }

      if (not namespace_hash) {
         auto separator = name.find(':');
         if (separator != std::string::npos) {
            std::string prefix = name.substr(0, separator);
            imported_local_name = name.substr(separator + 1);
            namespace_hash = prolog->resolve_prefix(prefix, context.document);
            if (namespace_hash != 0) {
               auto uri_entry = prolog->declared_namespace_uris.find(prefix);
               if (uri_entry != prolog->declared_namespace_uris.end()) module_uri = uri_entry->second;
               else if (context.document) {
                  auto prefix_it = context.document->Prefixes.find(prefix);
                  if (prefix_it != context.document->Prefixes.end()) {
                     auto ns_it = context.document->NSRegistry.find(prefix_it->second);
                     if (ns_it != context.document->NSRegistry.end()) module_uri = ns_it->second;
                  }
               }
            }
         }
      }

      const XQueryModuleImport *matched_import = nullptr;
      if (namespace_hash != 0) {
         for (const auto &import : prolog->module_imports) {
            if (pf::strhash(import.target_namespace) IS namespace_hash) {
               matched_import = &import;
               if (module_uri.empty()) module_uri = import.target_namespace;
               break;
            }
         }
      }

      if (matched_import) {
         if (module_uri.empty()) {
            std::string message = "Module variable '" + name + "' has an unresolved namespace.";
            record_error(message, ReferenceNode, true);
            return false;
         }

         auto module_cache = context.module_cache;
         if (not module_cache) {
            std::string message = "Module variable '" + name + "' requires a module cache.";
            record_error(message, ReferenceNode, true);
            return false;
         }

         (void)module_cache->fetch_or_load(module_uri, *prolog, *this);

         auto module_info = module_cache->find_module(module_uri);
         if (not module_info) {
            std::string message = "Module '" + module_uri + "' could not be loaded for variable '" + name + "'.";
            record_error(message, ReferenceNode, true);
            return false;
         }

         if (not module_info) {
            std::string message = "Module '" + module_uri + "' does not expose a prolog.";
            record_error(message, ReferenceNode, true);
            return false;
         }

         const XQueryVariable *module_variable = module_info->prolog->find_variable(name);

         if (not module_uri.empty() and not imported_local_name.empty()) {
            canonical_lookup = std::format("Q{{{}}}{}", module_uri, imported_local_name);
         }

         if (not module_variable and not canonical_lookup.empty()) {
            module_variable = module_info->prolog->find_variable(canonical_lookup);
         }

         if (not module_variable) {
            for (const auto &entry : module_info->prolog->variables) {
               const auto &candidate = entry.second;
               if (candidate.qname IS name) {
                  module_variable = &candidate;
                  break;
               }

               if (not canonical_lookup.empty() and (candidate.qname IS canonical_lookup)) {
                  module_variable = &candidate;
                  break;
               }

               size_t colon_pos = candidate.qname.find(':');
               if ((colon_pos != std::string::npos) and !imported_local_name.empty()) {
                  std::string candidate_prefix = candidate.qname.substr(0, colon_pos);
                  std::string candidate_local = candidate.qname.substr(colon_pos + 1);
                  if (candidate_local IS imported_local_name) {
                     uint32_t candidate_hash = module_info->prolog->resolve_prefix(candidate_prefix, nullptr);
                     if (candidate_hash IS namespace_hash) {
                        module_variable = &candidate;
                        break;
                     }
                  }
               }
            }
         }

         if (not module_variable) {
            std::string message = "Module variable '" + name + "' is not declared by namespace '" + module_uri + "'.";
            record_error(message, ReferenceNode, true);
            return false;
         }

         variable = module_variable;
         owner_prolog = module_info->prolog;
         active_module_cache = module_cache;
      }

      if (not variable) return false;
   }

   if (owner_prolog) {
      if (not canonical_lookup.empty()) normalised_name = canonical_lookup;
      else {
         normalised_name = canonicalise_variable_qname(name, *owner_prolog, context.document);
         if (normalised_name IS name) {
            normalised_name = canonicalise_variable_qname(variable->qname, *owner_prolog, context.document);
         }
      }
   }

   auto cached_value = prolog_variable_cache.find(normalised_name);
   if (cached_value != prolog_variable_cache.end()) {
      OutValue = cached_value->second;
      return true;
   }

   if (normalised_name != name) {
      auto alias_value = prolog_variable_cache.find(name);
      if (alias_value != prolog_variable_cache.end()) {
         prolog_variable_cache.insert_or_assign(normalised_name, alias_value->second);
         OutValue = alias_value->second;
         return true;
      }
   }

   if (variable->qname != normalised_name) {
      auto declared_value = prolog_variable_cache.find(variable->qname);
      if (declared_value != prolog_variable_cache.end()) {
         prolog_variable_cache.insert_or_assign(normalised_name, declared_value->second);
         if (normalised_name != name) {
            prolog_variable_cache.insert_or_assign(name, declared_value->second);
         }
         OutValue = declared_value->second;
         return true;
      }
   }

   if (variable->is_external) {
      std::string message = "External variable '" + name + "' is not supported.";
      record_error(message, ReferenceNode, true);
      return false;
   }

   if (not variable->initializer) {
      std::string message = "Variable '" + name + "' is missing an initialiser.";
      record_error(message, ReferenceNode, true);
      return false;
   }

   if (variables_in_evaluation.find(normalised_name) != variables_in_evaluation.end()) {
      std::string message = "Variable '" + name + "' has a circular dependency.";
      record_error(message, ReferenceNode, true);
      return false;
   }

   auto previous_prolog = context.prolog;
   auto previous_cache = context.module_cache;

   bool switched_context = false;
   if (owner_prolog and (owner_prolog.get() != previous_prolog.get())) {
      context.prolog = owner_prolog;
      if (active_module_cache) context.module_cache = active_module_cache;
      switched_context = true;
   }

   variables_in_evaluation.insert(normalised_name);
   XPathVal computed_value = evaluate_expression(variable->initializer.get(), CurrentPrefix);
   variables_in_evaluation.erase(normalised_name);

   if (switched_context) {
      context.prolog = previous_prolog;
      context.module_cache = previous_cache;
   }

   if (expression_unsupported) {
      std::string message = "Failed to evaluate initialiser for variable '" + name + "'.";
      record_error(message, ReferenceNode);
      return false;
   }

   auto inserted = prolog_variable_cache.insert_or_assign(normalised_name, computed_value);
   OutValue = inserted.first->second;

   if (normalised_name != name) {
      prolog_variable_cache.insert_or_assign(name, inserted.first->second);
   }

   if (variable->qname != normalised_name and variable->qname != name) {
      prolog_variable_cache.insert_or_assign(variable->qname, inserted.first->second);
   }

   return true;
}

//********************************************************************************************************************
// Evaluates an XPath/XQuery expression node and returns its computed value.  Responsibilities:
//
// - Dispatches on node kind (numbers, literals, constructors, paths, predicates, and control flow).
// - Preserves XPath semantics such as document order, short-circuiting (and/or), and context-sensitive
//   evaluation for filters, paths, and quantified/for expressions.
// - Integrates XQuery Prolog settings (ordering, construction, namespaces) and consults the module cache
//   when user-defined functions or variables require module resolution.
// - Uses push_context/pop_context to manage the evaluation context for node-set operations and predicates.
// - Signals unsupported constructs via 'expression_unsupported' and reports diagnostics with record_error().
// - Produces results as XPathVal, including node-set values with associated attribute/string metadata.
//
// Notes:
// - Function is side-effect free for input XML; constructed text nodes are owned by 'constructed_nodes'.
// - Returns empty values on failure paths; callers must check 'expression_unsupported' when necessary.

XPathVal XPathEvaluator::evaluate_expression(const XPathNode *ExprNode, uint32_t CurrentPrefix)
{
   pf::Log log("XPath");

   if (not ExprNode) {
      record_error("Unsupported XPath expression: empty node", (const XPathNode *)nullptr, true);
      return XPathVal();
   }

   // Use a switch for common node kinds for clarity and consistent early-returns
   switch (ExprNode->type) {
      case XPathNodeType::EMPTY_SEQUENCE: {
         // Return an empty node-set to represent the empty sequence
         return XPathVal(pf::vector<XMLTag *>{});
      }
      case XPathNodeType::NUMBER: {
         char *end_ptr = nullptr;
         double value = std::strtod(ExprNode->value.c_str(), &end_ptr);
         if ((end_ptr) and (*end_ptr IS '\0')) return XPathVal(value);
         return XPathVal(std::numeric_limits<double>::quiet_NaN());
      }
      case XPathNodeType::LITERAL:
      case XPathNodeType::STRING: {
         return XPathVal(ExprNode->value);
      }
      case XPathNodeType::DIRECT_ELEMENT_CONSTRUCTOR: {
         return evaluate_direct_element_constructor(ExprNode, CurrentPrefix);
      }
      case XPathNodeType::COMPUTED_ELEMENT_CONSTRUCTOR: {
         return evaluate_computed_element_constructor(ExprNode, CurrentPrefix);
      }
      case XPathNodeType::COMPUTED_ATTRIBUTE_CONSTRUCTOR: {
         return evaluate_computed_attribute_constructor(ExprNode, CurrentPrefix);
      }
      case XPathNodeType::TEXT_CONSTRUCTOR: {
         return evaluate_text_constructor(ExprNode, CurrentPrefix);
      }
      case XPathNodeType::COMMENT_CONSTRUCTOR: {
         return evaluate_comment_constructor(ExprNode, CurrentPrefix);
      }
      case XPathNodeType::PI_CONSTRUCTOR: {
         return evaluate_pi_constructor(ExprNode, CurrentPrefix);
      }
      case XPathNodeType::DOCUMENT_CONSTRUCTOR: {
         return evaluate_document_constructor(ExprNode, CurrentPrefix);
      }
      case XPathNodeType::LOCATION_PATH: {
         return evaluate_path_expression_value(ExprNode, CurrentPrefix);
      }
      case XPathNodeType::UNION: {
         std::vector<const XPathNode *> branches;
         branches.reserve(ExprNode->child_count());
         for (size_t index = 0; index < ExprNode->child_count(); ++index) {
            auto *branch = ExprNode->get_child(index);
            if (branch) branches.push_back(branch);
         }
         return evaluate_union_value(branches, CurrentPrefix);
      }
      case XPathNodeType::CONDITIONAL: {
         if (ExprNode->child_count() < 3) {
            expression_unsupported = true;
            return XPathVal();
         }
         auto *condition_node = ExprNode->get_child(0);
         auto *then_node = ExprNode->get_child(1);
         auto *else_node = ExprNode->get_child(2);
         if ((not condition_node) or (not then_node) or (not else_node)) {
            expression_unsupported = true;
            return XPathVal();
         }
         auto condition_value = evaluate_expression(condition_node, CurrentPrefix);
         if (expression_unsupported) return XPathVal();
         bool condition_boolean = condition_value.to_boolean();
         auto *selected_node = condition_boolean ? then_node : else_node;
         auto branch_value = evaluate_expression(selected_node, CurrentPrefix);
         return branch_value;
      }
      default: break; // handled below
   }

   // LET expressions share the same diagnostic surface as the parser.  Whenever a binding fails we populate
   // extXML::ErrorMsg so Fluid callers receive precise feedback rather than generic failure codes.

   if (ExprNode->type IS XPathNodeType::LET_EXPRESSION) {
      if (ExprNode->child_count() < 2) {
         record_error("LET expression requires at least one binding and a return clause.", ExprNode, true);
         return XPathVal();
      }

      const XPathNode *return_node = ExprNode->get_child(ExprNode->child_count() - 1);
      if (not return_node) {
         record_error("LET expression is missing its return clause.", ExprNode, true);
         return XPathVal();
      }

      std::vector<VariableBindingGuard> binding_guards;
      binding_guards.reserve(ExprNode->child_count() - 1);

      for (size_t index = 0; index + 1 < ExprNode->child_count(); ++index) {
         const XPathNode *binding_node = ExprNode->get_child(index);
         if ((not binding_node) or !(binding_node->type IS XPathNodeType::LET_BINDING)) {
            record_error("LET expression contains an invalid binding clause.", binding_node ? binding_node : ExprNode, true);
            return XPathVal();
         }

         if ((binding_node->value.empty()) or (binding_node->child_count() IS 0)) {
            record_error("Let binding requires a variable name and expression.", binding_node, true);
            return XPathVal();
         }

         const XPathNode *binding_expr = binding_node->get_child(0);
         if (not binding_expr) {
            record_error("Let binding requires an expression node.", binding_node, true);
            return XPathVal();
         }

         XPathVal bound_value = evaluate_expression(binding_expr, CurrentPrefix);
         if (expression_unsupported) {
            record_error("Let binding expression could not be evaluated.", binding_expr);
            return XPathVal();
         }

         binding_guards.emplace_back(context, binding_node->value, std::move(bound_value));
      }

      auto result_value = evaluate_expression(return_node, CurrentPrefix);
      if (expression_unsupported) {
         record_error("Let return expression could not be evaluated.", return_node);
         return XPathVal();
      }
      return result_value;
   }

   // FLWOR evaluation mirrors that approach, capturing structural and runtime issues so test_xpath_flwor.fluid can assert
   // on human-readable error text while we continue to guard performance-sensitive paths.

   if (ExprNode->type IS XPathNodeType::FLWOR_EXPRESSION) {
      return evaluate_flwor_pipeline(ExprNode, CurrentPrefix);
   }

   if (ExprNode->type IS XPathNodeType::FOR_EXPRESSION) {
      if (ExprNode->child_count() < 2) {
         expression_unsupported = true;
         return XPathVal();
      }

      const XPathNode *return_node = ExprNode->get_child(ExprNode->child_count() - 1);
      if (not return_node) {
         expression_unsupported = true;
         return XPathVal();
      }

      std::vector<ForBindingDefinition> bindings;
      bindings.reserve(ExprNode->child_count());

      // Support both the current and historical AST layouts for simple for-expressions.
      // Older parsers encoded a single binding by placing the variable name in
      // ExprNode->value and the iteration sequence at child(0), with the return
      // expression as the last child. Newer trees emit one or more explicit
      // FOR_BINDING children followed by the return expression. The flag below
      // allows the evaluator to accept either form for backwards compatibility.

      bool legacy_layout = false;

      for (size_t index = 0; index + 1 < ExprNode->child_count(); ++index) {
         const XPathNode *binding_node = ExprNode->get_child(index);
         if (binding_node and (binding_node->type IS XPathNodeType::FOR_BINDING)) {
            if ((binding_node->value.empty()) or (binding_node->child_count() IS 0)) {
               expression_unsupported = true;
               return XPathVal();
            }

            bindings.push_back({ binding_node->value, binding_node->get_child(0) });
            continue;
         }

         // Encountered a child that is not a FOR_BINDING: treat the node
         // as using the legacy single-binding layout described above.

         legacy_layout = true;
         break;
      }

      if (legacy_layout) {
         if (ExprNode->child_count() < 2) {
            expression_unsupported = true;
            return XPathVal();
         }

         const XPathNode *sequence_node = ExprNode->get_child(0);
         if ((not sequence_node) or (not return_node) or ExprNode->value.empty()) {
            expression_unsupported = true;
            return XPathVal();
         }

         bindings.clear();
         bindings.push_back({ ExprNode->value, sequence_node });
      }

      if (bindings.empty()) {
         expression_unsupported = true;
         return XPathVal();
      }

      NODES combined_nodes;
      std::vector<std::string> combined_strings;
      std::vector<const XMLAttrib *> combined_attributes;
      std::optional<std::string> combined_override;

      bool evaluation_ok = evaluate_for_bindings_recursive(*this, context, bindings, 0, return_node, CurrentPrefix,
         combined_nodes, combined_attributes, combined_strings, combined_override, expression_unsupported);
      if (not evaluation_ok) return XPathVal();
      if (expression_unsupported) return XPathVal();

      XPathVal result;
      result.Type = XPVT::NodeSet;
      result.preserve_node_order = false;
      result.node_set = std::move(combined_nodes);
      result.node_set_string_values = std::move(combined_strings);
      result.node_set_attributes = std::move(combined_attributes);
      if (combined_override.has_value()) result.node_set_string_override = combined_override;
      else result.node_set_string_override.reset();
      return result;
   }

   if (ExprNode->type IS XPathNodeType::QUANTIFIED_EXPRESSION) {
      if (ExprNode->child_count() < 2) {
         expression_unsupported = true;
         return XPathVal();
      }

      bool is_some = ExprNode->value IS "some";
      bool is_every = ExprNode->value IS "every";

      if ((not is_some) and (not is_every)) {
         expression_unsupported = true;
         return XPathVal();
      }

      const XPathNode *condition_node = ExprNode->get_child(ExprNode->child_count() - 1);
      if (not condition_node) {
         expression_unsupported = true;
         return XPathVal();
      }

      std::vector<QuantifiedBindingDefinition> bindings;
      bindings.reserve(ExprNode->child_count());

      for (size_t index = 0; index + 1 < ExprNode->child_count(); ++index) {
         const XPathNode *binding_node = ExprNode->get_child(index);
         if ((not binding_node) or (binding_node->type != XPathNodeType::QUANTIFIED_BINDING)) {
            expression_unsupported = true;
            return XPathVal();
         }

         if ((binding_node->value.empty()) or (binding_node->child_count() IS 0)) {
            expression_unsupported = true;
            return XPathVal();
         }

         bindings.push_back({ binding_node->value, binding_node->get_child(0) });
      }

      if (bindings.empty()) {
         expression_unsupported = true;
         return XPathVal();
      }

      bool quant_result = evaluate_quantified_binding_recursive(*this, context, bindings, 0,
         is_some, is_every, condition_node, CurrentPrefix, expression_unsupported);
      if (expression_unsupported) return XPathVal();

      return XPathVal(quant_result);
   }

   if (ExprNode->type IS XPathNodeType::FILTER) {
      if (ExprNode->child_count() IS 0) {
         expression_unsupported = true;
         return XPathVal();
      }

      auto base_value = evaluate_expression(ExprNode->get_child(0), CurrentPrefix);
      if (expression_unsupported) return XPathVal();

      if (base_value.Type != XPVT::NodeSet) {
         expression_unsupported = true;
         return XPathVal();
      }

      std::vector<size_t> working_indices(base_value.node_set.size());
      for (size_t index = 0; index < working_indices.size(); ++index) {
         working_indices[index] = index;
      }

      for (size_t predicate_index = 1; predicate_index < ExprNode->child_count(); ++predicate_index) {
         auto *predicate_node = ExprNode->get_child(predicate_index);
         if (not predicate_node) continue;

         std::vector<size_t> passed;
         passed.reserve(working_indices.size());

         for (size_t position = 0; position < working_indices.size(); ++position) {
            size_t base_index = working_indices[position];
            XMLTag *candidate = base_value.node_set[base_index];
            const XMLAttrib *attribute = nullptr;
            if (base_index < base_value.node_set_attributes.size()) {
               attribute = base_value.node_set_attributes[base_index];
            }

            push_context(candidate, position + 1, working_indices.size(), attribute);
            auto predicate_result = evaluate_predicate(predicate_node, CurrentPrefix);
            pop_context();

            if (predicate_result IS PredicateResult::UNSUPPORTED) {
               expression_unsupported = true;
               return XPathVal();
            }

            if (predicate_result IS PredicateResult::MATCH) passed.push_back(base_index);
         }

         working_indices.swap(passed);
         if (working_indices.empty()) break;
      }

      NODES filtered_nodes;
      filtered_nodes.reserve(working_indices.size());

      std::vector<std::string> filtered_strings;
      filtered_strings.reserve(working_indices.size());

      std::vector<const XMLAttrib *> filtered_attributes;
      filtered_attributes.reserve(working_indices.size());

      for (size_t index : working_indices) {
         filtered_nodes.push_back(base_value.node_set[index]);
         if (index < base_value.node_set_string_values.size()) {
            filtered_strings.push_back(base_value.node_set_string_values[index]);
         }
         const XMLAttrib *attribute = nullptr;
         if (index < base_value.node_set_attributes.size()) {
            attribute = base_value.node_set_attributes[index];
         }
         filtered_attributes.push_back(attribute);
      }

      std::optional<std::string> first_value;
      if (not working_indices.empty()) {
         size_t first_index = working_indices[0];
         if (base_value.node_set_string_override.has_value() and (first_index IS 0)) {
            first_value = base_value.node_set_string_override;
         }
         else if (first_index < base_value.node_set_string_values.size()) {
            first_value = base_value.node_set_string_values[first_index];
         }
      }

      return XPathVal(filtered_nodes, first_value, std::move(filtered_strings), std::move(filtered_attributes));
   }

   if (ExprNode->type IS XPathNodeType::PATH) {
      if (ExprNode->child_count() IS 0) {
         expression_unsupported = true;
         return XPathVal();
      }

      auto *first_child = ExprNode->get_child(0);
      if (first_child and (first_child->type IS XPathNodeType::LOCATION_PATH)) {
         return evaluate_path_expression_value(ExprNode, CurrentPrefix);
      }

      auto base_value = evaluate_expression(first_child, CurrentPrefix);
      if (expression_unsupported) return XPathVal();

      if (base_value.Type != XPVT::NodeSet) {
         return XPathVal(base_value.to_node_set());
      }

      std::vector<const XPathNode *> steps;
      for (size_t index = 1; index < ExprNode->child_count(); ++index) {
         auto *child = ExprNode->get_child(index);
         if (child and (child->type IS XPathNodeType::STEP)) steps.push_back(child);
      }

      if (steps.empty()) return base_value;

      const XPathNode *attribute_step = nullptr;
      const XPathNode *attribute_test = nullptr;

      if (not steps.empty()) {
         auto *last_step = steps.back();
         const XPathNode *axis_node = nullptr;
         const XPathNode *node_test = nullptr;

         for (size_t index = 0; index < last_step->child_count(); ++index) {
            auto *child = last_step->get_child(index);
            if (not child) continue;

            if (child->type IS XPathNodeType::AXIS_SPECIFIER) axis_node = child;
            else if ((not node_test) and ((child->type IS XPathNodeType::NAME_TEST) or
                                       (child->type IS XPathNodeType::WILDCARD) or
                                       (child->type IS XPathNodeType::NODE_TYPE_TEST))) node_test = child;
         }

         AxisType axis = axis_node ? AxisEvaluator::parse_axis_name(axis_node->value) : AxisType::CHILD;
         if (axis IS AxisType::ATTRIBUTE) {
            attribute_step = last_step;
            attribute_test = node_test;
         }
      }

      return evaluate_path_from_nodes(base_value.node_set, base_value.node_set_attributes, steps, attribute_step,
         attribute_test, CurrentPrefix);
   }

   if (ExprNode->type IS XPathNodeType::FUNCTION_CALL) {
      auto value = evaluate_function_call(ExprNode, CurrentPrefix);
      if (expression_unsupported) return XPathVal();
      return value;
   }

   if (ExprNode->type IS XPathNodeType::UNARY_OP) {
      if (ExprNode->child_count() IS 0) {
         expression_unsupported = true;
         return XPathVal();
      }

      auto operand = evaluate_expression(ExprNode->get_child(0), CurrentPrefix);
      if (expression_unsupported) return XPathVal();

      if (ExprNode->value IS "-") return XPathVal(-operand.to_number());
      if (ExprNode->value IS "not") return XPathVal(not operand.to_boolean());

      expression_unsupported = true;
      return XPathVal();
   }

   if (ExprNode->type IS XPathNodeType::BINARY_OP) {
      if (ExprNode->child_count() < 2) {
         expression_unsupported = true;
         return XPathVal();
      }

      auto *left_node = ExprNode->get_child(0);
      auto *right_node = ExprNode->get_child(1);

      const std::string &operation = ExprNode->value;

      // Switch-based dispatch for common logical/set operations
      switch (map_binary_operation(operation)) {
         case BinaryOperationKind::AND: {
            auto left_value = evaluate_expression(left_node, CurrentPrefix);
            if (expression_unsupported) return XPathVal();
            bool left_boolean = left_value.to_boolean();
            if (not left_boolean) return XPathVal(false);
            auto right_value = evaluate_expression(right_node, CurrentPrefix);
            if (expression_unsupported) return XPathVal();
            bool right_boolean = right_value.to_boolean();
            return XPathVal(right_boolean);
         }
         case BinaryOperationKind::OR: {
            auto left_value = evaluate_expression(left_node, CurrentPrefix);
            if (expression_unsupported) return XPathVal();
            bool left_boolean = left_value.to_boolean();
            if (left_boolean) return XPathVal(true);
            auto right_value = evaluate_expression(right_node, CurrentPrefix);
            if (expression_unsupported) return XPathVal();
            bool right_boolean = right_value.to_boolean();
            return XPathVal(right_boolean);
         }
         case BinaryOperationKind::UNION: {
            std::vector<const XPathNode *> branches;
            branches.reserve(2);
            if (left_node) branches.push_back(left_node);
            if (right_node) branches.push_back(right_node);
            return evaluate_union_value(branches, CurrentPrefix);
         }
         case BinaryOperationKind::INTERSECT: {
            return evaluate_intersect_value(left_node, right_node, CurrentPrefix);
         }
         case BinaryOperationKind::EXCEPT: {
            return evaluate_except_value(left_node, right_node, CurrentPrefix);
         }
         default: {
            // Other operations handled below
            break;
         }
      }

      // and/or/union/intersect/except handled above via switch

      BinaryOperationKind op_kind = map_binary_operation(operation);

      if (op_kind IS BinaryOperationKind::COMMA) {
         auto left_value = evaluate_expression(left_node, CurrentPrefix);
         if (expression_unsupported) return XPathVal();
         auto right_value = evaluate_expression(right_node, CurrentPrefix);
         if (expression_unsupported) return XPathVal();

         std::vector<SequenceEntry> entries;
         entries.reserve(left_value.node_set.size() + right_value.node_set.size());

         append_value_to_sequence(left_value, entries, next_constructed_node_id, constructed_nodes);
         append_value_to_sequence(right_value, entries, next_constructed_node_id, constructed_nodes);

         if (entries.empty()) {
            NODES empty_nodes;
            return XPathVal(empty_nodes);
         }

         NODES combined_nodes;
         combined_nodes.reserve(entries.size());
         std::vector<const XMLAttrib *> combined_attributes;
         combined_attributes.reserve(entries.size());
         std::vector<std::string> combined_strings;
         combined_strings.reserve(entries.size());

         for (auto &entry : entries) {
            combined_nodes.push_back(entry.node);
            combined_attributes.push_back(entry.attribute);
            combined_strings.push_back(std::move(entry.string_value));
         }

         XPathVal result(combined_nodes, std::nullopt, std::move(combined_strings), std::move(combined_attributes));
         if (not prolog_ordering_is_ordered()) result.preserve_node_order = true;
         return result;
      }

      auto left_value = evaluate_expression(left_node, CurrentPrefix);
      if (expression_unsupported) return XPathVal();
      auto right_value = evaluate_expression(right_node, CurrentPrefix);
      if (expression_unsupported) return XPathVal();

      switch (op_kind) {
         case BinaryOperationKind::EQ: {
            bool equals = compare_xpath_values(left_value, right_value);
            return XPathVal(equals);
         }
         case BinaryOperationKind::NE: {
            bool equals = compare_xpath_values(left_value, right_value);
            return XPathVal(not equals);
         }
         case BinaryOperationKind::EQ_WORD: {
            auto left_scalar = promote_value_comparison_operand(left_value);
            auto right_scalar = promote_value_comparison_operand(right_value);
            if (not left_scalar.has_value() or !right_scalar.has_value()) return XPathVal(false);
            bool equals = compare_xpath_values(*left_scalar, *right_scalar);
            return XPathVal(equals);
         }
         case BinaryOperationKind::NE_WORD: {
            auto left_scalar = promote_value_comparison_operand(left_value);
            auto right_scalar = promote_value_comparison_operand(right_value);
            if (not left_scalar.has_value() or (not right_scalar.has_value())) return XPathVal(false);
            bool equals = compare_xpath_values(*left_scalar, *right_scalar);
            return XPathVal(not equals);
         }
         case BinaryOperationKind::LT: {
            // Handles both symbol '<' and textual 'lt'
            if (operation IS "lt") {
               auto left_scalar = promote_value_comparison_operand(left_value);
               auto right_scalar = promote_value_comparison_operand(right_value);
               if (not left_scalar.has_value() or (not right_scalar.has_value())) return XPathVal(false);
               bool result = compare_xpath_relational(*left_scalar, *right_scalar, RelationalOperator::LESS);
               return XPathVal(result);
            }
            bool result = compare_xpath_relational(left_value, right_value, RelationalOperator::LESS);
            return XPathVal(result);
         }
         case BinaryOperationKind::LE: {
            if (operation IS "le") {
               auto left_scalar = promote_value_comparison_operand(left_value);
               auto right_scalar = promote_value_comparison_operand(right_value);
               if (not left_scalar.has_value() or not right_scalar.has_value()) return XPathVal(false);
               bool result = compare_xpath_relational(*left_scalar, *right_scalar, RelationalOperator::LESS_OR_EQUAL);
               return XPathVal(result);
            }
            bool result = compare_xpath_relational(left_value, right_value, RelationalOperator::LESS_OR_EQUAL);
            return XPathVal(result);
         }
         case BinaryOperationKind::GT: {
            if (operation IS "gt") {
               auto left_scalar = promote_value_comparison_operand(left_value);
               auto right_scalar = promote_value_comparison_operand(right_value);
               if (not left_scalar.has_value() or not right_scalar.has_value()) return XPathVal(false);
               bool result = compare_xpath_relational(*left_scalar, *right_scalar, RelationalOperator::GREATER);
               return XPathVal(result);
            }
            bool result = compare_xpath_relational(left_value, right_value, RelationalOperator::GREATER);
            return XPathVal(result);
         }
         case BinaryOperationKind::GE: {
            if (operation IS "ge") {
               auto left_scalar = promote_value_comparison_operand(left_value);
               auto right_scalar = promote_value_comparison_operand(right_value);
               if (not left_scalar.has_value() or not right_scalar.has_value()) return XPathVal(false);
               bool result = compare_xpath_relational(*left_scalar, *right_scalar, RelationalOperator::GREATER_OR_EQUAL);
               return XPathVal(result);
            }
            bool result = compare_xpath_relational(left_value, right_value, RelationalOperator::GREATER_OR_EQUAL);
            return XPathVal(result);
         }
         case BinaryOperationKind::ADD: {
            double result = left_value.to_number() + right_value.to_number();
            return XPathVal(result);
         }
         case BinaryOperationKind::SUB: {
            double result = left_value.to_number() - right_value.to_number();
            return XPathVal(result);
         }
         case BinaryOperationKind::MUL: {
            double result = left_value.to_number() * right_value.to_number();
            return XPathVal(result);
         }
         case BinaryOperationKind::DIV: {
            double result = left_value.to_number() / right_value.to_number();
            return XPathVal(result);
         }
         case BinaryOperationKind::MOD: {
            double left_number = left_value.to_number();
            double right_number = right_value.to_number();
            double result = std::fmod(left_number, right_number);
            return XPathVal(result);
         }
         default: {
            expression_unsupported = true;
            return XPathVal();
         }
      }
   }

   // EXPRESSION nodes are wrappers - unwrap to the child node
   if (ExprNode->type IS XPathNodeType::EXPRESSION) {
      if (ExprNode->child_count() > 0) {
         return evaluate_expression(ExprNode->get_child(0), CurrentPrefix);
      }
      expression_unsupported = true;
      return XPathVal();
   }

   if (ExprNode->type IS XPathNodeType::VARIABLE_REFERENCE) {
      XPathVal resolved_value;
      if (resolve_variable_value(ExprNode->value, CurrentPrefix, resolved_value, ExprNode)) {
         return resolved_value;
      }

      if (is_trace_enabled()) {
         log.msg(VLF::TRACE, "Variable lookup failed for '%s'", ExprNode->value.c_str());
         if (context.variables and context.variables->empty() IS false) {
            std::string binding_list;
            auto it = context.variables->begin();
            if (it != context.variables->end()) {
               binding_list = it->first;
               ++it;
               for (; it != context.variables->end(); ++it) {
                  binding_list.append(", ");
                  binding_list.append(it->first);
               }
            }
            log.msg(VLF::TRACE, "Context bindings available: [%s]", binding_list.c_str());
         }
      }

      expression_unsupported = true;
      return XPathVal();
   }

   expression_unsupported = true;
   return XPathVal();
}
