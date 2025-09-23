#include <cmath>
#include <limits>
#include <optional>
#include <unordered_set>

// XPath Evaluator Implementation

//********************************************************************************************************************
// Context Management

void SimpleXPathEvaluator::push_context(XMLTag *Node, size_t Position, size_t Size) {
   context_stack.push_back(context);
   context.context_node = Node;
   context.position = Position;
   context.size = Size;
}

void SimpleXPathEvaluator::pop_context() {
   if (context_stack.empty()) {
      context.context_node = nullptr;
      context.position = 1;
      context.size = 1;
      return;
   }

   context = context_stack.back();
   context_stack.pop_back();
}

void SimpleXPathEvaluator::push_cursor_state() {
   CursorState state{};
   state.tags = xml->CursorTags;

   if ((xml->CursorTags) and (xml->CursorTags->begin() != xml->CursorTags->end())) {
      state.index = size_t(xml->Cursor - xml->CursorTags->begin());
   }
   else state.index = 0;

   cursor_stack.push_back(state);
}

void SimpleXPathEvaluator::pop_cursor_state() {
   if (cursor_stack.empty()) return;

   auto state = cursor_stack.back();
   cursor_stack.pop_back();

   xml->CursorTags = state.tags;

   if (!xml->CursorTags) return;

   auto begin = xml->CursorTags->begin();
   if (state.index >= size_t(xml->CursorTags->size())) {
      xml->Cursor = xml->CursorTags->end();
   }
   else xml->Cursor = begin + state.index;
}

//********************************************************************************************************************
// Enhanced Entry Point (AST + Legacy Fallback)

ERR SimpleXPathEvaluator::find_tag_enhanced(std::string_view XPath, uint32_t CurrentPrefix) {
   pf::Log log(__FUNCTION__);
   log.msg("Enhanced XPath: %.*s", int(XPath.size()), XPath.data());

   // Ensure the document index is up to date so ParentID links are valid during AST traversal
   (void)xml->getMap();

   // Try AST-based parsing first
   XPathTokenizer tokenizer;
   auto tokens = tokenizer.tokenize(XPath);

   XPathParser parser;
   auto ast = parser.parse(tokens);

   if (ast) {
      log.msg("AST parsed successfully, evaluating...");
      auto result = evaluate_ast(ast.get(), CurrentPrefix);
      log.msg("AST evaluation result: %d", int(result));
      if (result IS ERR::Okay or result IS ERR::Search) {
         return result;
      }
      // If AST evaluation fails, fall back to string-based
      log.msg("AST evaluation failed, falling back to string-based parsing");
   } else {
      log.msg("AST parsing failed");
   }

   // Fall back to Phase 1 string-based evaluation
   PathInfo info;
   auto parse_result = parse_path(XPath, info);
   if (parse_result != ERR::Okay) return parse_result;

   if (!xml->Attrib.empty()) return ERR::Okay;

   return evaluate_step(XPath, info, CurrentPrefix);
}

//********************************************************************************************************************
// AST Evaluation Methods (Phase 1+ of AST_PLAN.md)

ERR SimpleXPathEvaluator::evaluate_ast(const XPathNode *Node, uint32_t CurrentPrefix) {
   if (!Node) return ERR::Failed;

   switch (Node->type) {
      case XPathNodeType::LocationPath:
         return evaluate_location_path(Node, CurrentPrefix);

      case XPathNodeType::Step:
         return evaluate_step_ast(Node, CurrentPrefix);

      default:
         // TODO: Implement other node types as needed for AST_PLAN.md phases
         return ERR::Failed;
   }
}

ERR SimpleXPathEvaluator::evaluate_location_path(const XPathNode *PathNode, uint32_t CurrentPrefix) {
   if ((!PathNode) or (PathNode->type != XPathNodeType::LocationPath)) return ERR::Failed;

   pf::Log log(__FUNCTION__);
   log.msg("evaluate_location_path: starting AST traversal");

   std::vector<const XPathNode *> steps;
   std::vector<std::unique_ptr<XPathNode>> synthetic_steps;

   bool has_root = false;
   bool root_descendant = false;

   for (size_t i = 0; i < PathNode->child_count(); ++i) {
      auto child = PathNode->get_child(i);
      if (!child) continue;

      if ((i IS 0) and (child->type IS XPathNodeType::Root)) {
         has_root = true;
         root_descendant = child->value IS "//";
         continue;
      }

      if (child->type IS XPathNodeType::Step) steps.push_back(child);
   }

   if (root_descendant) {
      auto descendant_step = std::make_unique<XPathNode>(XPathNodeType::Step);
      descendant_step->add_child(std::make_unique<XPathNode>(XPathNodeType::AxisSpecifier, "descendant-or-self"));
      descendant_step->add_child(std::make_unique<XPathNode>(XPathNodeType::NodeTypeTest, "node"));
      steps.insert(steps.begin(), descendant_step.get());
      synthetic_steps.push_back(std::move(descendant_step));
   }

   if (steps.empty()) {
      log.msg("evaluate_location_path: no steps to process");
      return ERR::Search;
   }

   std::vector<XMLTag *> initial_context;

   if (has_root) {
      initial_context.push_back(nullptr);
   }
   else {
      if (context.context_node) initial_context.push_back(context.context_node);
      else if ((xml->CursorTags) and (xml->Cursor != xml->CursorTags->end())) initial_context.push_back(&(*xml->Cursor));
      else initial_context.push_back(nullptr);
   }

   bool matched = false;
   auto result = evaluate_step_sequence(initial_context, steps, 0, CurrentPrefix, matched);

   if ((result != ERR::Okay) and (result != ERR::Search)) return result;

   if (xml->Callback.defined()) return ERR::Okay;
   if (matched) return ERR::Okay;
   return ERR::Search;
}

ERR SimpleXPathEvaluator::evaluate_step_ast(const XPathNode *StepNode, uint32_t CurrentPrefix) {
   if (!StepNode) return ERR::Failed;

   std::vector<const XPathNode *> steps;
   steps.push_back(StepNode);

   std::vector<XMLTag *> context_nodes;
   if (context.context_node) context_nodes.push_back(context.context_node);
   else if ((xml->CursorTags) and (xml->Cursor != xml->CursorTags->end())) context_nodes.push_back(&(*xml->Cursor));
   else context_nodes.push_back(nullptr);

   bool matched = false;
   auto result = evaluate_step_sequence(context_nodes, steps, 0, CurrentPrefix, matched);

   if ((result != ERR::Okay) and (result != ERR::Search)) return result;

   if (xml->Callback.defined()) return ERR::Okay;
   if (matched) return ERR::Okay;
   return ERR::Search;
}

ERR SimpleXPathEvaluator::evaluate_step_sequence(const std::vector<XMLTag *> &ContextNodes, const std::vector<const XPathNode *> &Steps, size_t StepIndex, uint32_t CurrentPrefix, bool &Matched) {
   if (StepIndex >= Steps.size()) return Matched ? ERR::Okay : ERR::Search;

   auto step_node = Steps[StepIndex];
   if ((!step_node) or (step_node->type != XPathNodeType::Step)) return ERR::Failed;

   const XPathNode *axis_node = nullptr;
   const XPathNode *node_test = nullptr;
   std::vector<const XPathNode *> predicate_nodes;

   for (size_t i = 0; i < step_node->child_count(); ++i) {
      auto child = step_node->get_child(i);
      if (!child) continue;

      if (child->type IS XPathNodeType::AxisSpecifier) axis_node = child;
      else if (child->type IS XPathNodeType::Predicate) predicate_nodes.push_back(child);
      else if ((!node_test) and ((child->type IS XPathNodeType::NameTest) or (child->type IS XPathNodeType::Wildcard) or (child->type IS XPathNodeType::NodeTypeTest))) {
         node_test = child;
      }
   }

   AxisType axis = AxisType::Child;
   if (axis_node) axis = AxisEvaluator::parse_axis_name(axis_node->value);

   if ((axis IS AxisType::Attribute)) {
      pf::Log log(__FUNCTION__);
      log.msg("TODO: Attribute axis deferred to later phase");
      return ERR::Failed;
   }

   if ((axis != AxisType::Child) and (axis != AxisType::DescendantOrSelf) and (axis != AxisType::Self)) {
      pf::Log log(__FUNCTION__);
      log.msg("TODO: Axis '%s' not yet supported in AST evaluator", AxisEvaluator::axis_name_to_string(axis).c_str());
      return ERR::Failed;
   }

   auto dispatch_axis = [this, axis](XMLTag *context_node) {
      std::vector<XMLTag *> results;

      if (axis IS AxisType::Child) {
         if (!context_node) {
            for (auto &tag : xml->Tags) {
               if (tag.isTag()) results.push_back(&tag);
            }
         }
         else results = axis_evaluator.evaluate_axis(AxisType::Child, context_node);
      }
      else if (axis IS AxisType::DescendantOrSelf) {
         if (!context_node) {
            results.push_back(nullptr);
            for (auto &tag : xml->Tags) {
               if (!tag.isTag()) continue;
               auto branch = axis_evaluator.evaluate_axis(AxisType::DescendantOrSelf, &tag);
               results.insert(results.end(), branch.begin(), branch.end());
            }
         }
         else results = axis_evaluator.evaluate_axis(AxisType::DescendantOrSelf, context_node);
      }
      else if (axis IS AxisType::Self) {
         results.push_back(context_node);
      }

      return results;
   };

   bool is_last_step = (StepIndex + 1 >= Steps.size());

   for (auto *context_node : ContextNodes) {
      auto axis_candidates = dispatch_axis(context_node);

      // Filter by node test first so only viable candidates remain
      std::vector<XMLTag *> filtered;
      filtered.reserve(axis_candidates.size());

      for (auto *candidate : axis_candidates) {
         if (!match_node_test(node_test, candidate, CurrentPrefix)) continue;
         filtered.push_back(candidate);
      }

      if (filtered.empty()) {
         continue;
      }

      // Apply predicates sequentially, updating the candidate list each time
      for (auto *predicate_node : predicate_nodes) {
         std::vector<XMLTag *> passed;
         passed.reserve(filtered.size());

         for (size_t index = 0; index < filtered.size(); ++index) {
            auto *candidate = filtered[index];
            push_context(candidate, index + 1, filtered.size());

            auto predicate_result = evaluate_predicate(predicate_node, CurrentPrefix);
            pop_context();

            if (predicate_result IS PredicateResult::Unsupported) {
               return ERR::Failed;
            }

            if (predicate_result IS PredicateResult::Match) {
               passed.push_back(candidate);
            }
         }

         filtered.swap(passed);

         if (filtered.empty()) break;
      }

      if (filtered.empty()) continue;

      for (size_t index = 0; index < filtered.size(); ++index) {
         auto *candidate = filtered[index];

         push_context(candidate, index + 1, filtered.size());

         if (is_last_step) {
            if (!candidate) {
               pop_context();
               continue;
            }

            auto tags = xml->getInsert(candidate, xml->Cursor);
            if (!tags) {
               pop_context();
               continue;
            }

            xml->CursorTags = tags;
            xml->Attrib.clear();

            if (!xml->Callback.defined()) {
               Matched = true;
               pop_context();
               return ERR::Okay;
            }

            push_cursor_state();

            ERR callback_error = ERR::Okay;
            if (xml->Callback.isC()) {
               auto routine = (ERR (*)(extXML *, int, CSTRING, APTR))xml->Callback.Routine;
               callback_error = routine(xml, candidate->ID, xml->Attrib.empty() ? nullptr : xml->Attrib.c_str(), xml->Callback.Meta);
            }
            else if (xml->Callback.isScript()) {
               if (sc::Call(xml->Callback, std::to_array<ScriptArg>({
                  { "XML",  xml, FD_OBJECTPTR },
                  { "Tag",  candidate->ID },
                  { "Attrib", xml->Attrib.empty() ? CSTRING(nullptr) : xml->Attrib.c_str() }
               }), callback_error) != ERR::Okay) callback_error = ERR::Terminate;
            }
            else callback_error = ERR::InvalidValue;

            pop_cursor_state();
            pop_context();

            Matched = true;

            if (callback_error IS ERR::Terminate) return ERR::Terminate;
            if (callback_error != ERR::Okay) return callback_error;

            continue;
         }

         std::vector<XMLTag *> child_context;
         child_context.push_back(candidate);

         push_cursor_state();

         if (candidate) {
            xml->CursorTags = &candidate->Children;
            xml->Cursor = xml->CursorTags->begin();
         }
         else {
            xml->CursorTags = &xml->Tags;
            xml->Cursor = xml->CursorTags->begin();
         }

         auto result = evaluate_step_sequence(child_context, Steps, StepIndex + 1, CurrentPrefix, Matched);

         if ((result IS ERR::Okay) and (!xml->Callback.defined()) and Matched) {
            auto matched_tags = xml->CursorTags;
            auto matched_cursor = xml->Cursor;

            pop_cursor_state();
            pop_context();

            xml->CursorTags = matched_tags;
            xml->Cursor = matched_cursor;
            return ERR::Okay;
         }

         pop_cursor_state();
         pop_context();

         if (result IS ERR::Terminate) return ERR::Terminate;
         if (result IS ERR::Failed) return ERR::Failed;

         if ((result IS ERR::Okay) and (!xml->Callback.defined()) and Matched) return ERR::Okay;
      }
   }

   return Matched ? ERR::Okay : ERR::Search;
}

bool SimpleXPathEvaluator::match_node_test(const XPathNode *NodeTest, XMLTag *Candidate, uint32_t CurrentPrefix) {
   if (!NodeTest) return Candidate != nullptr;

   if (NodeTest->type IS XPathNodeType::NodeTypeTest) {
      if (NodeTest->value IS "node") return true;
      if (!Candidate) return false;

      if (NodeTest->value IS "text") return Candidate->isContent();
      return false;
   }

   if (!Candidate) return false;

   if (NodeTest->type IS XPathNodeType::Wildcard) return Candidate->isTag();

   if (NodeTest->type IS XPathNodeType::NameTest) {
      std::string_view test_name = NodeTest->value;
      if (test_name.empty()) return false;

      std::string_view candidate_name = Candidate->name();

      if (test_name.find('*') != std::string::npos) return pf::wildcmp(test_name, candidate_name);

      if ((xml->Flags & XMF::NAMESPACE_AWARE) != XMF::NIL) {
         uint32_t expected_prefix = 0;
         std::string_view expected_local = test_name;

         if (auto colon = test_name.find(':'); colon != std::string::npos) {
            expected_prefix = pf::strhash(test_name.substr(0, colon));
            expected_local = test_name.substr(colon + 1);
         }

         std::string_view candidate_local = candidate_name;
         uint32_t candidate_prefix = 0;

         if (auto colon = candidate_name.find(':'); colon != std::string::npos) {
            candidate_prefix = pf::strhash(candidate_name.substr(0, colon));
            candidate_local = candidate_name.substr(colon + 1);
         }

         bool name_matches = expected_local.find('*') != std::string::npos ? pf::wildcmp(expected_local, candidate_local) : pf::iequals(expected_local, candidate_local);
         bool prefix_matches = expected_prefix ? (candidate_prefix IS expected_prefix) : true;

         return name_matches and prefix_matches;
      }

      return pf::iequals(test_name, candidate_name);
   }

   return false;
}

SimpleXPathEvaluator::PredicateResult SimpleXPathEvaluator::evaluate_predicate(const XPathNode *PredicateNode, uint32_t CurrentPrefix) {
   if ((!PredicateNode) or (PredicateNode->type != XPathNodeType::Predicate)) {
      return PredicateResult::Unsupported;
   }

   if (PredicateNode->child_count() IS 0) return PredicateResult::Unsupported;

   const XPathNode *expression = PredicateNode->get_child(0);
   if (!expression) return PredicateResult::Unsupported;

   if (expression->type IS XPathNodeType::BinaryOp) {
      auto *candidate = context.context_node;
      if (!candidate) return PredicateResult::NoMatch;

      const std::string &operation = expression->value;

      if (operation IS "attribute-exists") {
         if (expression->child_count() IS 0) return PredicateResult::Unsupported;

         const XPathNode *name_node = expression->get_child(0);
         if (!name_node) return PredicateResult::Unsupported;

         const std::string &attribute_name = name_node->value;

         if (attribute_name IS "*") {
            return (candidate->Attribs.size() > 1) ? PredicateResult::Match : PredicateResult::NoMatch;
         }

         for (int index = 1; index < std::ssize(candidate->Attribs); ++index) {
            auto &attrib = candidate->Attribs[index];
            if (pf::iequals(attrib.Name, attribute_name)) return PredicateResult::Match;
         }

         return PredicateResult::NoMatch;
      }

      if (operation IS "attribute-equals") {
         if (expression->child_count() < 2) return PredicateResult::Unsupported;

         const XPathNode *name_node = expression->get_child(0);
         const XPathNode *value_node = expression->get_child(1);
         if ((!name_node) or (!value_node)) return PredicateResult::Unsupported;

         const std::string &attribute_name = name_node->value;
         const std::string &attribute_value = value_node->value;

         bool wildcard_name = attribute_name.find('*') != std::string::npos;
         bool wildcard_value = attribute_value.find('*') != std::string::npos;

         for (int index = 1; index < std::ssize(candidate->Attribs); ++index) {
            auto &attrib = candidate->Attribs[index];

            bool name_matches;
            if (attribute_name IS "*") name_matches = true;
            else if (wildcard_name) name_matches = pf::wildcmp(attribute_name, attrib.Name);
            else name_matches = pf::iequals(attrib.Name, attribute_name);

            if (!name_matches) continue;

            bool value_matches;
            if (wildcard_value) value_matches = pf::wildcmp(attribute_value, attrib.Value);
            else value_matches = pf::iequals(attrib.Value, attribute_value);

            if (value_matches) return PredicateResult::Match;
         }

         return PredicateResult::NoMatch;
      }

      if (operation IS "content-equals") {
         if (expression->child_count() IS 0) return PredicateResult::Unsupported;

         const XPathNode *value_node = expression->get_child(0);
         if (!value_node) return PredicateResult::Unsupported;

         const std::string &expected = value_node->value;
         bool wildcard_value = expected.find('*') != std::string::npos;

         if (!candidate->Children.empty()) {
            auto &first_child = candidate->Children[0];
            if ((!first_child.Attribs.empty()) and (first_child.Attribs[0].isContent())) {
               const std::string &content = first_child.Attribs[0].Value;
               if (wildcard_value) {
                  auto match = pf::wildcmp(expected, content) ? PredicateResult::Match : PredicateResult::NoMatch;
                  return match;
               }
               else return pf::iequals(content, expected) ? PredicateResult::Match : PredicateResult::NoMatch;
            }
         }

         return PredicateResult::NoMatch;
      }
   }

   auto result_value = evaluate_expression(expression, CurrentPrefix);

   if (expression_unsupported) {
      expression_unsupported = false;
      return PredicateResult::Unsupported;
   }

   if (result_value.type IS XPathValueType::NodeSet) {
      return result_value.node_set.empty() ? PredicateResult::NoMatch : PredicateResult::Match;
   }

   if (result_value.type IS XPathValueType::Boolean) {
      return result_value.to_boolean() ? PredicateResult::Match : PredicateResult::NoMatch;
   }

   if (result_value.type IS XPathValueType::String) {
      return result_value.to_string().empty() ? PredicateResult::NoMatch : PredicateResult::Match;
   }

   if (result_value.type IS XPathValueType::Number) {
      double expected = result_value.to_number();
      if (std::isnan(expected)) return PredicateResult::NoMatch;

      double integral_part = 0.0;
      double fractional = std::modf(expected, &integral_part);
      if (fractional != 0.0) return PredicateResult::NoMatch;
      if (integral_part < 1.0) return PredicateResult::NoMatch;

      return (context.position IS size_t(integral_part)) ? PredicateResult::Match : PredicateResult::NoMatch;
   }

   return PredicateResult::Unsupported;
}

//********************************************************************************************************************
// Function and Expression Evaluation (Phase 3 of AST_PLAN.md)

namespace {

bool compare_xpath_values(const XPathValue &left_value,
                          const XPathValue &right_value)
{
   auto left_type = left_value.type;
   auto right_type = right_value.type;

   if ((left_type IS XPathValueType::Number) or (right_type IS XPathValueType::Number)) {
      double left_number = left_value.to_number();
      double right_number = right_value.to_number();
      return left_number IS right_number;
   }

   if ((left_type IS XPathValueType::Boolean) or (right_type IS XPathValueType::Boolean)) {
      bool left_boolean = left_value.to_boolean();
      bool right_boolean = right_value.to_boolean();
      return left_boolean IS right_boolean;
   }

   std::string left_string = left_value.to_string();
   std::string right_string = right_value.to_string();
   return pf::iequals(left_string, right_string);
}

} // namespace

std::vector<XMLTag *> SimpleXPathEvaluator::collect_step_results(const std::vector<XMLTag *> &ContextNodes,
                                                                 const std::vector<const XPathNode *> &Steps,
                                                                 size_t StepIndex,
                                                                 uint32_t CurrentPrefix,
                                                                 bool &Unsupported)
{
   std::vector<XMLTag *> results;

   if (Unsupported) return results;

   if (StepIndex >= Steps.size()) {
      results.insert(results.end(), ContextNodes.begin(), ContextNodes.end());
      return results;
   }

   auto step_node = Steps[StepIndex];
   if ((!step_node) or (step_node->type != XPathNodeType::Step)) {
      Unsupported = true;
      return results;
   }

   const XPathNode *axis_node = nullptr;
   const XPathNode *node_test = nullptr;
   std::vector<const XPathNode *> predicate_nodes;

   for (size_t index = 0; index < step_node->child_count(); ++index) {
      auto *child = step_node->get_child(index);
      if (!child) continue;

      if (child->type IS XPathNodeType::AxisSpecifier) axis_node = child;
      else if (child->type IS XPathNodeType::Predicate) predicate_nodes.push_back(child);
      else if ((!node_test) and ((child->type IS XPathNodeType::NameTest) or
                                 (child->type IS XPathNodeType::Wildcard) or
                                 (child->type IS XPathNodeType::NodeTypeTest))) node_test = child;
   }

   AxisType axis = AxisType::Child;
   if (axis_node) axis = AxisEvaluator::parse_axis_name(axis_node->value);

   if (axis IS AxisType::Attribute) {
      Unsupported = true;
      return results;
   }

   if ((axis != AxisType::Child) and (axis != AxisType::DescendantOrSelf) and (axis != AxisType::Self)) {
      Unsupported = true;
      return results;
   }

   auto dispatch_axis = [this, axis](XMLTag *context_node) {
      std::vector<XMLTag *> axis_results;

      if (axis IS AxisType::Child) {
         if (!context_node) {
            for (auto &tag : xml->Tags) {
               if (tag.isTag()) axis_results.push_back(&tag);
            }
         }
         else axis_results = axis_evaluator.evaluate_axis(AxisType::Child, context_node);
      }
      else if (axis IS AxisType::DescendantOrSelf) {
         if (!context_node) {
            axis_results.push_back(nullptr);
            for (auto &tag : xml->Tags) {
               if (!tag.isTag()) continue;
               auto branch = axis_evaluator.evaluate_axis(AxisType::DescendantOrSelf, &tag);
               axis_results.insert(axis_results.end(), branch.begin(), branch.end());
            }
         }
         else axis_results = axis_evaluator.evaluate_axis(AxisType::DescendantOrSelf, context_node);
      }
      else if (axis IS AxisType::Self) {
         axis_results.push_back(context_node);
      }

      return axis_results;
   };

   bool is_last_step = (StepIndex + 1 >= Steps.size());

   for (auto *context_node : ContextNodes) {
      auto axis_candidates = dispatch_axis(context_node);

      std::vector<XMLTag *> filtered;
      filtered.reserve(axis_candidates.size());

      for (auto *candidate : axis_candidates) {
         if (!match_node_test(node_test, candidate, CurrentPrefix)) continue;
         filtered.push_back(candidate);
      }

      if (filtered.empty()) continue;

      for (auto *predicate_node : predicate_nodes) {
         std::vector<XMLTag *> passed;
         passed.reserve(filtered.size());

         for (size_t index = 0; index < filtered.size(); ++index) {
            auto *candidate = filtered[index];
            push_context(candidate, index + 1, filtered.size());
            auto predicate_result = evaluate_predicate(predicate_node, CurrentPrefix);
            pop_context();

            if (predicate_result IS PredicateResult::Unsupported) {
               Unsupported = true;
               return {};
            }

            if (predicate_result IS PredicateResult::Match) passed.push_back(candidate);
         }

         filtered.swap(passed);

         if (filtered.empty()) break;
      }

      if (filtered.empty()) continue;

      if (is_last_step) {
         results.insert(results.end(), filtered.begin(), filtered.end());
         continue;
      }

      auto child_results = collect_step_results(filtered, Steps, StepIndex + 1, CurrentPrefix, Unsupported);
      if (Unsupported) return {};
      results.insert(results.end(), child_results.begin(), child_results.end());
   }

   return results;
}

XPathValue SimpleXPathEvaluator::evaluate_path_expression_value(const XPathNode *PathNode, uint32_t CurrentPrefix) {
   if (!PathNode) {
      expression_unsupported = true;
      return XPathValue();
   }

   const XPathNode *location = PathNode;
   if (PathNode->type IS XPathNodeType::Path) {
      if (PathNode->child_count() IS 0) return XPathValue();
      location = PathNode->get_child(0);
   }

   if ((!location) or (location->type != XPathNodeType::LocationPath)) {
      expression_unsupported = true;
      return XPathValue();
   }

   std::vector<const XPathNode *> steps;
   std::vector<std::unique_ptr<XPathNode>> synthetic_steps;

   bool has_root = false;
   bool root_descendant = false;

   for (size_t index = 0; index < location->child_count(); ++index) {
      auto *child = location->get_child(index);
      if (!child) continue;

      if ((index IS 0) and (child->type IS XPathNodeType::Root)) {
         has_root = true;
         root_descendant = child->value IS "//";
         continue;
      }

      if (child->type IS XPathNodeType::Step) steps.push_back(child);
   }

   if (root_descendant) {
      auto descendant_step = std::make_unique<XPathNode>(XPathNodeType::Step);
      descendant_step->add_child(std::make_unique<XPathNode>(XPathNodeType::AxisSpecifier, "descendant-or-self"));
      descendant_step->add_child(std::make_unique<XPathNode>(XPathNodeType::NodeTypeTest, "node"));
      steps.insert(steps.begin(), descendant_step.get());
      synthetic_steps.push_back(std::move(descendant_step));
   }

   std::vector<XMLTag *> initial_context;

   if (has_root) initial_context.push_back(nullptr);
   else {
      if (context.context_node) initial_context.push_back(context.context_node);
      else if ((xml->CursorTags) and (xml->Cursor != xml->CursorTags->end())) initial_context.push_back(&(*xml->Cursor));
      else initial_context.push_back(nullptr);
   }

   if (steps.empty()) return XPathValue(initial_context);

   const XPathNode *attribute_step = nullptr;
   const XPathNode *attribute_test = nullptr;

   auto last_step = steps.back();
   if (last_step) {
      const XPathNode *axis_node = nullptr;
      const XPathNode *node_test = nullptr;

      for (size_t index = 0; index < last_step->child_count(); ++index) {
         auto *child = last_step->get_child(index);
         if (!child) continue;

         if (child->type IS XPathNodeType::AxisSpecifier) axis_node = child;
         else if ((!node_test) and ((child->type IS XPathNodeType::NameTest) or
                                    (child->type IS XPathNodeType::Wildcard) or
                                    (child->type IS XPathNodeType::NodeTypeTest))) node_test = child;
      }

      AxisType axis = axis_node ? AxisEvaluator::parse_axis_name(axis_node->value) : AxisType::Child;
      if (axis IS AxisType::Attribute) {
         attribute_step = last_step;
         attribute_test = node_test;
      }
   }

   std::vector<const XPathNode *> work_steps = steps;
   if (attribute_step) work_steps.pop_back();

   bool unsupported = false;
   std::vector<XMLTag *> node_results;

   if (work_steps.empty()) {
      for (auto *candidate : initial_context) {
         if (candidate) node_results.push_back(candidate);
      }
   }
   else node_results = collect_step_results(initial_context, work_steps, 0, CurrentPrefix, unsupported);

   if (unsupported) {
      expression_unsupported = true;
      return XPathValue();
   }

   if (attribute_step) {
      std::vector<std::string> attribute_values;
      std::vector<XMLTag *> attribute_nodes;

      for (auto *candidate : node_results) {
         if (!candidate) continue;

         std::string attribute_name;
         bool wildcard_name = false;

         if (attribute_test) {
            if (attribute_test->type IS XPathNodeType::Wildcard) wildcard_name = true;
            else if (attribute_test->type IS XPathNodeType::NodeTypeTest) wildcard_name = true;
            else attribute_name = attribute_test->value;
         }
         else wildcard_name = true;

         for (size_t index = 1; index < candidate->Attribs.size(); ++index) {
            auto &attrib = candidate->Attribs[index];

            bool name_matches;
            if (wildcard_name) name_matches = true;
            else if (attribute_name.find('*') != std::string::npos) name_matches = pf::wildcmp(attribute_name, attrib.Name);
            else name_matches = pf::iequals(attrib.Name, attribute_name);

            if (!name_matches) continue;
            attribute_values.push_back(attrib.Value);
            attribute_nodes.push_back(candidate);
         }
      }

      if (attribute_nodes.empty()) return XPathValue(attribute_nodes);

      std::optional<std::string> first_value;
      if (!attribute_values.empty()) first_value = attribute_values[0];
      return XPathValue(attribute_nodes, first_value);
   }

   return XPathValue(node_results);
}

XPathValue SimpleXPathEvaluator::evaluate_expression(const XPathNode *ExprNode, uint32_t CurrentPrefix) {
   if (!ExprNode) {
      expression_unsupported = true;
      return XPathValue();
   }

   if (ExprNode->type IS XPathNodeType::Number) {
      char *end_ptr = nullptr;
      double value = std::strtod(ExprNode->value.c_str(), &end_ptr);
      if ((end_ptr) and (*end_ptr IS '\0')) return XPathValue(value);
      return XPathValue(std::numeric_limits<double>::quiet_NaN());
   }

   if ((ExprNode->type IS XPathNodeType::Literal) or (ExprNode->type IS XPathNodeType::String)) {
      return XPathValue(ExprNode->value);
   }

   if ((ExprNode->type IS XPathNodeType::Path) or (ExprNode->type IS XPathNodeType::LocationPath)) {
      return evaluate_path_expression_value(ExprNode, CurrentPrefix);
   }

   if (ExprNode->type IS XPathNodeType::FunctionCall) {
      auto value = evaluate_function_call(ExprNode, CurrentPrefix);
      if (expression_unsupported) return XPathValue();
      return value;
   }

   if (ExprNode->type IS XPathNodeType::UnaryOp) {
      if (ExprNode->child_count() IS 0) {
         expression_unsupported = true;
         return XPathValue();
      }

      auto operand = evaluate_expression(ExprNode->get_child(0), CurrentPrefix);
      if (expression_unsupported) return XPathValue();

      if (ExprNode->value IS "-") {
         return XPathValue(-operand.to_number());
      }

      if (ExprNode->value IS "not") {
         return XPathValue(!operand.to_boolean());
      }

      expression_unsupported = true;
      return XPathValue();
   }

   if (ExprNode->type IS XPathNodeType::BinaryOp) {
      if (ExprNode->child_count() < 2) {
         expression_unsupported = true;
         return XPathValue();
      }

      auto *left_node = ExprNode->get_child(0);
      auto *right_node = ExprNode->get_child(1);

      const std::string &operation = ExprNode->value;

      if (operation IS "and") {
         auto left_value = evaluate_expression(left_node, CurrentPrefix);
         if (expression_unsupported) return XPathValue();

         bool left_boolean = left_value.to_boolean();
         if (!left_boolean) return XPathValue(false);

         auto right_value = evaluate_expression(right_node, CurrentPrefix);
         if (expression_unsupported) return XPathValue();

         bool right_boolean = right_value.to_boolean();
         return XPathValue(right_boolean);
      }

      if (operation IS "or") {
         auto left_value = evaluate_expression(left_node, CurrentPrefix);
         if (expression_unsupported) return XPathValue();

         bool left_boolean = left_value.to_boolean();
         if (left_boolean) return XPathValue(true);

         auto right_value = evaluate_expression(right_node, CurrentPrefix);
         if (expression_unsupported) return XPathValue();

         bool right_boolean = right_value.to_boolean();
         return XPathValue(right_boolean);
      }

      auto left_value = evaluate_expression(left_node, CurrentPrefix);
      if (expression_unsupported) return XPathValue();
      auto right_value = evaluate_expression(right_node, CurrentPrefix);
      if (expression_unsupported) return XPathValue();

      if (operation IS "=") {
         bool equals = compare_xpath_values(left_value, right_value);
         return XPathValue(equals);
      }

      if (operation IS "!=") {
         bool equals = compare_xpath_values(left_value, right_value);
         return XPathValue(!equals);
      }

      if (operation IS "<") {
         double left_number = left_value.to_number();
         double right_number = right_value.to_number();
         if (std::isnan(left_number) or std::isnan(right_number)) return XPathValue(false);
         return XPathValue(left_number < right_number);
      }

      if (operation IS "<=") {
         double left_number = left_value.to_number();
         double right_number = right_value.to_number();
         if (std::isnan(left_number) or std::isnan(right_number)) return XPathValue(false);
         return XPathValue(left_number <= right_number);
      }

      if (operation IS ">") {
         double left_number = left_value.to_number();
         double right_number = right_value.to_number();
         if (std::isnan(left_number) or std::isnan(right_number)) return XPathValue(false);
         return XPathValue(left_number > right_number);
      }

      if (operation IS ">=") {
         double left_number = left_value.to_number();
         double right_number = right_value.to_number();
         if (std::isnan(left_number) or std::isnan(right_number)) return XPathValue(false);
         return XPathValue(left_number >= right_number);
      }

      if (operation IS "+") {
         double result = left_value.to_number() + right_value.to_number();
         return XPathValue(result);
      }

      if (operation IS "-") {
         double result = left_value.to_number() - right_value.to_number();
         return XPathValue(result);
      }

      if (operation IS "*") {
         double result = left_value.to_number() * right_value.to_number();
         return XPathValue(result);
      }

      if (operation IS "div") {
         double result = left_value.to_number() / right_value.to_number();
         return XPathValue(result);
      }

      if (operation IS "mod") {
         double left_number = left_value.to_number();
         double right_number = right_value.to_number();
         double result = std::fmod(left_number, right_number);
         return XPathValue(result);
      }

      if (operation IS "|") {
         auto left_nodes = left_value.to_node_set();
         auto right_nodes = right_value.to_node_set();

         std::unordered_set<XMLTag *> seen;
         seen.reserve(left_nodes.size() + right_nodes.size());

         std::vector<XMLTag *> combined;
         combined.reserve(left_nodes.size() + right_nodes.size());

         for (auto *node : left_nodes) {
            if (!seen.insert(node).second) continue;
            combined.push_back(node);
         }

         for (auto *node : right_nodes) {
            if (!seen.insert(node).second) continue;
            combined.push_back(node);
         }

         return XPathValue(combined);
      }

      expression_unsupported = true;
      return XPathValue();
   }

   expression_unsupported = true;
   return XPathValue();
}

XPathValue SimpleXPathEvaluator::evaluate_function_call(const XPathNode *FuncNode, uint32_t CurrentPrefix) {
   if (!FuncNode or FuncNode->type != XPathNodeType::FunctionCall) {
      return XPathValue();
   }

   std::string function_name = FuncNode->value;

   std::vector<XPathValue> args;
   args.reserve(FuncNode->child_count());

   for (size_t index = 0; index < FuncNode->child_count(); ++index) {
      auto *argument_node = FuncNode->get_child(index);
      args.push_back(evaluate_expression(argument_node, CurrentPrefix));
      if (expression_unsupported) return XPathValue();
   }

   return function_library.call_function(function_name, args, context);
}

//********************************************************************************************************************
// Legacy String-Based Methods (Phase 1 - will be removed in Phase 5)

ERR SimpleXPathEvaluator::parse_path(std::string_view XPath, PathInfo &Info) {
   pf::Log log(__FUNCTION__);

   if ((XPath.empty()) or (XPath[0] != '/')) {
      log.warning("Missing '/' prefix in '%.*s'.", int(XPath.size()), XPath.data());
      return ERR::StringFormat;
   }

   // Check for flat scan (//)
   Info.pos = [&Info, XPath]() mutable {
      if (XPath[0] != '/') return size_t(0);
      if ((XPath.size() > 1) and (XPath[1] != '/')) return size_t(1);
      Info.flat_scan = true;
      return size_t(2);
   }();

   // Check if the path is something like '/@attrib' which means we want the attribute value of the current tag
   if ((Info.pos < XPath.size()) and (XPath[Info.pos] IS '@')) {
      xml->Attrib.assign(XPath.substr(Info.pos + 1));
      return ERR::Okay;
   }

   // Parse the tag name
   auto start = Info.pos;
   auto delimiter_pos = XPath.find_first_of("/[(", Info.pos);
   Info.pos = (delimiter_pos != std::string_view::npos) ? delimiter_pos : XPath.size();
   if (Info.pos > start) Info.tag_name = XPath.substr(start, Info.pos - start);
   else Info.tag_name = "*";

   // Parse namespace prefix from current tag
   if ((xml->Flags & XMF::NAMESPACE_AWARE) != XMF::NIL) {
      if (auto colon = Info.tag_name.find(':'); colon != std::string_view::npos) {
         Info.tag_prefix = pf::strhash(Info.tag_name.substr(0, colon));
         Info.tag_name = Info.tag_name.substr(colon + 1);
      }
   }

   // Parse filter instructions
   char end_char;
   if ((Info.pos < XPath.size()) and ((XPath[Info.pos] IS '[') or (XPath[Info.pos] IS '('))) {
      if (XPath[Info.pos] IS '[') end_char = ']';
      else end_char = ')';

      Info.pos++;

      auto non_space_pos = XPath.find_first_not_of(" \t\n\r", Info.pos);
      Info.pos = (non_space_pos != std::string_view::npos) ? non_space_pos : XPath.size();

      if ((Info.pos < XPath.size()) and (XPath[Info.pos] >= '0') and (XPath[Info.pos] <= '9')) { // Parse index
         char *end;
         Info.subscript = strtol(XPath.data() + Info.pos, &end, 0);
         if (Info.subscript < 1) return ERR::Syntax; // Subscripts start from 1, not 0
         Info.pos = end - XPath.data();
      }
      else if ((Info.pos < XPath.size()) and ((XPath[Info.pos] IS '@') or (XPath[Info.pos] IS '='))) {
         if (XPath[Info.pos] IS '@') {  // Parse attribute filter such as "[@id='5']" or "[@*='v']" or "[@attr]"
            Info.pos++;

            auto len = Info.pos;
            if ((len < XPath.size()) and (XPath[len] IS '*')) {
               // Attribute wildcard
               Info.attrib_name = "*";
               len++;
            } else {
               while (len < XPath.size()) {
                  char c = XPath[len];
                  if (std::isalnum(static_cast<unsigned char>(c)) or c IS '_' or c IS '-' or c IS ':' or c IS '.') len++;
                  else break;
               }
               Info.attrib_name = XPath.substr(Info.pos, len - Info.pos);
            }
            if (Info.attrib_name.empty()) return ERR::Syntax;

            Info.pos = len;
            auto non_space_pos2 = XPath.find_first_not_of(" \t\n\r", Info.pos);
            Info.pos = (non_space_pos2 != std::string_view::npos) ? non_space_pos2 : XPath.size(); // Skip whitespace

            if ((Info.pos < XPath.size()) and (XPath[Info.pos] IS '=')) Info.pos++;
         }
         else Info.pos++; // Skip '=' (indicates matching on content)

         auto non_space_pos3 = XPath.find_first_not_of(" \t\n\r", Info.pos);
         Info.pos = (non_space_pos3 != std::string_view::npos) ? non_space_pos3 : XPath.size(); // Skip whitespace

         // Parse value (optional). If no value provided, treat as attribute-existence test [@attr]
         if ((Info.pos < XPath.size()) and ((XPath[Info.pos] IS '\'') or (XPath[Info.pos] IS '"'))) {
            const char quote = XPath[Info.pos++];
            bool esc_attrib = false;
            auto end = Info.pos;
            while ((end < XPath.size()) and (XPath[end] != quote)) {
               if (XPath[end] IS '\\') { // Escape character check
                  if ((end + 1 < XPath.size())) {
                     auto ch = XPath[end+1];
                     if ((ch IS '*') or (ch IS quote) or (ch IS '\\')) {
                        end++;
                        esc_attrib = true;
                     }
                  }
               }
               else if (XPath[end] IS '*') Info.wild = true;
               end++;
            }

            if ((end >= XPath.size()) or (XPath[end] != quote)) return ERR::Syntax; // Quote not terminated correctly

            Info.attrib_value.assign(XPath.substr(Info.pos, end - Info.pos));
            Info.pos = end + 1;

            if (esc_attrib) {
               for (int i=0; i < std::ssize(Info.attrib_value); i++) {
                  if (Info.attrib_value[i] != '\\') continue;
                  auto ch = Info.attrib_value[i+1];
                  if ((ch IS '*') or (ch IS quote) or (ch IS '\\')) {
                     Info.attrib_value.erase(i);
                     i--;
                  }
               }
            }
         }
         else if ((Info.pos < XPath.size()) and (XPath[Info.pos] != end_char)) {
            auto end_pos = XPath.find(end_char, Info.pos);
            int end = (end_pos != std::string_view::npos) ? end_pos : XPath.size();

            // Check for wildcards in the range
            if (XPath.substr(Info.pos, end - Info.pos).find('*') != std::string_view::npos) Info.wild = true;
            Info.attrib_value.assign(XPath.substr(Info.pos, end - Info.pos));
            Info.pos = end;
         }
      }
      else return ERR::Syntax;

      auto non_space_pos4 = XPath.find_first_not_of(" \t\n\r", Info.pos);
      Info.pos = (non_space_pos4 != std::string_view::npos) ? non_space_pos4 : XPath.size(); // Skip whitespace
      if ((Info.pos >= XPath.size()) or (XPath[Info.pos] != end_char)) return ERR::Syntax;
      Info.pos++;
   }

   return ERR::Okay;
}

bool SimpleXPathEvaluator::match_tag(const PathInfo &Info, uint32_t CurrentPrefix) {
   bool tag_matched = false;
   uint32_t cursor_prefix = CurrentPrefix;

   // Special handling for function calls - check if subscript == 0 and attrib_value contains function
   if (Info.subscript IS 0 and !Info.attrib_value.empty()) {
      // Check if attrib_value looks like a function call (contains "position()=", "last()", etc.)
      if (Info.attrib_value.find("position()") != std::string::npos or
          Info.attrib_value.find("last()") != std::string::npos or
          Info.attrib_value.find("count(") != std::string::npos) {

         // Try to parse and evaluate the function call
         // For now, handle simple cases like "position()=2"
         if (Info.attrib_value.find("position()=") != std::string::npos) {
            // Extract the number after "position()="
            auto pos = Info.attrib_value.find("position()=");
            if (pos != std::string::npos) {
               auto num_start = pos + 11; // Length of "position()="
               if (num_start < Info.attrib_value.length()) {
                  // Position context is handled in evaluate_step; the string-based fallback
                  // path cannot evaluate the predicate accurately, so force failure.
                  return false;
               }
            }
         }
      }
   }

   // Match both tag name and prefix, if applicable
   if ((xml->Flags & XMF::NAMESPACE_AWARE) != XMF::NIL) {
      std::string_view cursor_local_name = xml->Cursor->name();
      if (auto colon = cursor_local_name.find(':'); colon != std::string_view::npos) {
         cursor_prefix = pf::strhash(cursor_local_name.substr(0, colon));
         cursor_local_name = cursor_local_name.substr(colon + 1);
      }

      auto tag_wild = Info.tag_name.find('*') != std::string_view::npos;
      bool name_matches = tag_wild ? pf::wildcmp(Info.tag_name, cursor_local_name) : pf::iequals(Info.tag_name, cursor_local_name);
      bool prefix_matches = Info.tag_prefix ? cursor_prefix IS Info.tag_prefix : true;
      tag_matched = name_matches and prefix_matches;
   }
   else { // Traditional matching: just compare full tag names
      auto tag_wild = Info.tag_name.find('*') != std::string_view::npos;
      if (tag_wild) tag_matched = pf::wildcmp(Info.tag_name, xml->Cursor->name());
      else tag_matched = pf::iequals(Info.tag_name, xml->Cursor->name());
   }

   if (!tag_matched) return false;

   // Check attribute/content filters
   if ((!Info.attrib_name.empty()) or (!Info.attrib_value.empty())) {
      if (xml->Cursor->name()) {
         if (!Info.attrib_name.empty()) { // Match by named attribute value or existence; '*' matches any attribute name
            for (int a=1; a < std::ssize(xml->Cursor->Attribs); ++a) {
               bool name_matches = (Info.attrib_name IS "*") ? true : pf::iequals(xml->Cursor->Attribs[a].Name, Info.attrib_name);
               if (name_matches) {
                  // If no attribute value specified, treat as existence test
                  if (Info.attrib_value.empty()) return true;
                  if (Info.wild) {
                     if (pf::wildcmp(xml->Cursor->Attribs[a].Value, Info.attrib_value)) return true;
                  }
                  else if (pf::iequals(xml->Cursor->Attribs[a].Value, Info.attrib_value)) {
                     return true;
                  }
               }
            }
            return false;
         }
         else if (!Info.attrib_value.empty()) {
            if ((!xml->Cursor->Children.empty()) and (xml->Cursor->Children[0].Attribs[0].isContent())) {
               if (Info.wild) {
                  return pf::wildcmp(xml->Cursor->Children[0].Attribs[0].Value, Info.attrib_value);
               }
               else return pf::iequals(xml->Cursor->Children[0].Attribs[0].Value, Info.attrib_value);
            }
            return false;
         }
      }
      return false;
   }

   return true;
}

ERR SimpleXPathEvaluator::evaluate_step(std::string_view XPath, PathInfo Info, uint32_t CurrentPrefix) {
   pf::Log log(__FUNCTION__);

   if ((xml->Flags & XMF::LOG_ALL) != XMF::NIL) {
      log.branch("XPath: %.*s, TagName: %.*s", int(XPath.size()), XPath.data(), int(Info.tag_name.size()), Info.tag_name.data());
   }

   // Check if this predicate contains function calls that need position tracking
   bool has_function_call = false;
   std::string_view function_expression;
   if (!Info.attrib_value.empty()) {
      pf::Log func_log("Function Detection");
      func_log.msg("Checking attrib_value: '%s'", Info.attrib_value.c_str());
      if (Info.attrib_value.find("position()") != std::string::npos or
          Info.attrib_value.find("last()") != std::string::npos) {
         has_function_call = true;
         function_expression = Info.attrib_value;
         func_log.msg("Function call detected: '%.*s'", int(function_expression.size()), function_expression.data());
      }
   }

   // For function calls, we need to collect all matching nodes first to get the total count
   std::vector<XMLTag *> matching_nodes;
   if (has_function_call) {
      // First pass: collect all nodes that match the tag name (without predicate)
      PathInfo tag_only_info = Info;
      tag_only_info.attrib_value.clear(); // Remove predicate
      tag_only_info.subscript = 0; // Remove index

      for (auto cursor = xml->CursorTags->begin(); cursor != xml->CursorTags->end(); ++cursor) {
         auto saved_cursor = xml->Cursor;
         xml->Cursor = cursor;
         if (match_tag(tag_only_info, CurrentPrefix)) {
            matching_nodes.push_back(&(*cursor));
         }
         xml->Cursor = saved_cursor;
      }

      // Now evaluate the function call for each matching node
      for (size_t pos = 0; pos < matching_nodes.size(); ++pos) {
         // Set up context for function evaluation
         context.context_node = matching_nodes[pos];
         context.position = pos + 1; // XPath positions are 1-based
         context.size = matching_nodes.size();

         // Find this node in CursorTags and set xml->Cursor to it
         for (auto cursor = xml->CursorTags->begin(); cursor != xml->CursorTags->end(); ++cursor) {
            if (&(*cursor) IS matching_nodes[pos]) {
               xml->Cursor = cursor;
               break;
            }
         }

         // Evaluate the function expression
         if (evaluate_function_expression(function_expression)) {
            // This node matches the function predicate
            if (Info.pos < XPath.size() and XPath[Info.pos] IS '/' and Info.pos + 1 < XPath.size() and XPath[Info.pos+1] IS '@') {
               xml->Attrib.assign(XPath.substr(Info.pos + 2));
            } else {
               xml->Attrib.clear();
            }

            if (!xml->Callback.defined()) return ERR::Okay;

            // Call callback if defined
            auto error = ERR::Okay;
            if (xml->Callback.isC()) {
               auto routine = (ERR (*)(extXML *, int, CSTRING, APTR))xml->Callback.Routine;
               error = routine(xml, xml->Cursor->ID, xml->Attrib.empty() ? nullptr : xml->Attrib.c_str(), xml->Callback.Meta);
            }
            else if (xml->Callback.isScript()) {
               if (sc::Call(xml->Callback, std::to_array<ScriptArg>({
                  { "XML",  xml, FD_OBJECTPTR },
                  { "Tag",  xml->Cursor->ID },
                  { "Attrib", xml->Attrib.empty() ? CSTRING(nullptr) : xml->Attrib.c_str() }
               }), error) != ERR::Okay) error = ERR::Terminate;
            }

            if (error IS ERR::Terminate) return ERR::Terminate;
            if (error != ERR::Okay) return error;
         }
      }

      return xml->Callback.defined() ? ERR::Okay : ERR::Search;
   }

   // Original logic for non-function predicates
   bool stop = false;
   for (; xml->Cursor != xml->CursorTags->end() and (!stop); xml->Cursor++) {
      bool match = false;
      uint32_t cursor_prefix = CurrentPrefix;

      if (Info.flat_scan or match_tag(Info, cursor_prefix)) {
         if (Info.flat_scan and !match_tag(Info, cursor_prefix)) {
            // For flat scan, check children if current tag doesn't match
            if (!xml->Cursor->Children.empty()) {
               auto save_cursor = xml->Cursor;
               auto save_tags   = xml->CursorTags;

               xml->CursorTags = &xml->Cursor->Children;
               xml->Cursor     = xml->Cursor->Children.begin();

               ERR error = evaluate_step(XPath, Info, cursor_prefix);
               if ((error IS ERR::Okay) and (!xml->Callback.defined())) return ERR::Okay;

               xml->Cursor     = save_cursor;
               xml->CursorTags = save_tags;
            }
            continue;
         }

         match = true;
      }

      if ((not match) and (not Info.flat_scan)) continue;

      if (Info.subscript > 1) {
         Info.subscript--;
         continue;
      }
      else if (Info.subscript IS 1) {
         stop = true;
      }

      bool path_ended = Info.pos >= XPath.size() or ((XPath[Info.pos] IS '/') and (Info.pos + 1 < XPath.size()) and (XPath[Info.pos+1] IS '@'));
      if ((match) and (path_ended)) { // Matching tag found and there is nothing left in the path
         if (Info.pos < XPath.size()) xml->Attrib.assign(XPath.substr(Info.pos + 2));
         else xml->Attrib.clear();

         if (!xml->Callback.defined()) return ERR::Okay; // End of query, successfully found tag

         auto error = ERR::Okay;
         if (xml->Callback.isC()) {
            auto routine = (ERR (*)(extXML *, int, CSTRING, APTR))xml->Callback.Routine;
            error = routine(xml, xml->Cursor->ID, xml->Attrib.empty() ? nullptr : xml->Attrib.c_str(), xml->Callback.Meta);
         }
         else if (xml->Callback.isScript()) {
            if (sc::Call(xml->Callback, std::to_array<ScriptArg>({
               { "XML",  xml, FD_OBJECTPTR },
               { "Tag",  xml->Cursor->ID },
               { "Attrib", xml->Attrib.empty() ? CSTRING(nullptr) : xml->Attrib.c_str() }
            }), error) != ERR::Okay) error = ERR::Terminate;
         }
         else return ERR::InvalidValue;

         if (error IS ERR::Terminate) return ERR::Terminate;
         if (error != ERR::Okay) return error;

         // Searching will continue because the client specified a callback...
      }
      else if (!xml->Cursor->Children.empty()) { // Tag matched & XPath continues, OR flat-scan enabled.  Scan deeper into the tree
         auto save_cursor = xml->Cursor;
         auto save_tags   = xml->CursorTags;

         xml->CursorTags = &xml->Cursor->Children;
         xml->Cursor     = xml->Cursor->Children.begin();

         ERR error;
         if (Info.flat_scan) error = evaluate_step(XPath, Info, cursor_prefix); // Continue search from the beginning of the tag name
         else {
            // Parse the next step
            PathInfo next_info;
            auto parse_err = parse_path(XPath.substr(Info.pos), next_info);
            if (parse_err != ERR::Okay) return parse_err;
            error = evaluate_step(XPath.substr(Info.pos), next_info, cursor_prefix);
         }

         if ((error IS ERR::Okay) and (!xml->Callback.defined())) return ERR::Okay;
         if (error IS ERR::Terminate) return ERR::Terminate;

         xml->Cursor     = save_cursor;
         xml->CursorTags = save_tags;
      }
   }

   if (xml->Callback.defined()) return ERR::Okay;
   else return ERR::Search;
}

bool SimpleXPathEvaluator::evaluate_function_expression(std::string_view Expression) {
   // Handle simple function expressions like:
   // "position()=2"
   // "last()"
   // "position()!=1"

   constexpr std::string_view position_eq = "position()=";
   if (auto pos = Expression.find(position_eq); pos != std::string_view::npos) {
      auto num_start = pos + position_eq.size();
      if (num_start < Expression.size()) {
         auto number_view = Expression.substr(num_start);
         while (!number_view.empty() and std::isspace(static_cast<unsigned char>(number_view.front()))) {
            number_view.remove_prefix(1);
         }

         int expected_position = 0;
         auto conv = std::from_chars(number_view.data(), number_view.data() + number_view.size(), expected_position);
         if (conv.ec != std::errc()) return false;

         XPathValue pos_result = function_library.call_function("position", {}, context);
         return pos_result.to_number() IS expected_position;
      }
   }
   else if (Expression IS "last()") {
      XPathValue last_result = function_library.call_function("last", {}, context);
      return last_result.to_boolean(); // last() itself is truthy
   }
   else if (Expression.find("last()") != std::string_view::npos and Expression.find("=") != std::string_view::npos) {
      // Handle expressions like "position()=last()"
      XPathValue pos_result = function_library.call_function("position", {}, context);
      XPathValue last_result = function_library.call_function("last", {}, context);
      return pos_result.to_number() IS last_result.to_number();
   }

   return false; // Unsupported expression
}
