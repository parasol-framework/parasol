
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>
#include <unordered_set>

namespace {

std::string_view trim_view(std::string_view Value)
{
   auto start = Value.find_first_not_of(" \t\r\n");
   if (start == std::string_view::npos) return std::string_view();

   auto end = Value.find_last_not_of(" \t\r\n");
   return Value.substr(start, end - start + 1);
}

std::vector<std::string_view> split_union_paths(std::string_view XPath)
{
   std::vector<std::string_view> segments;
   size_t start = 0;
   int bracket_depth = 0;
   int paren_depth = 0;
   bool in_string = false;
   char string_delim = '\0';

   for (size_t index = 0; index < XPath.size(); ++index) {
      char ch = XPath[index];

      if (in_string) {
         if ((ch IS '\\') and (index + 1 < XPath.size())) {
            index++;
            continue;
         }

         if (ch IS string_delim) in_string = false;
         continue;
      }

      if ((ch IS '\'') or (ch IS '"')) {
         in_string = true;
         string_delim = ch;
         continue;
      }

      if (ch IS '[') {
         bracket_depth++;
         continue;
      }

      if (ch IS ']') {
         if (bracket_depth > 0) bracket_depth--;
         continue;
      }

      if (ch IS '(') {
         paren_depth++;
         continue;
      }

      if (ch IS ')') {
         if (paren_depth > 0) paren_depth--;
         continue;
      }

      if ((ch IS '|') and (bracket_depth IS 0) and (paren_depth IS 0)) {
         auto segment = trim_view(XPath.substr(start, index - start));
         if (!segment.empty()) segments.push_back(segment);
         start = index + 1;
      }
   }

   auto tail = trim_view(XPath.substr(start));
   if (!tail.empty()) segments.push_back(tail);

   return segments;
}

} // namespace

//********************************************************************************************************************
// Context Management

void SimpleXPathEvaluator::push_context(XMLTag *Node, size_t Position, size_t Size, const XMLAttrib *Attribute) 
{
   context_stack.push_back(context);
   context.context_node = Node;
   context.attribute_node = Attribute;
   context.position = Position;
   context.size = Size;
}

void SimpleXPathEvaluator::pop_context() 
{
   if (context_stack.empty()) {
      context.context_node = nullptr;
      context.attribute_node = nullptr;
      context.position = 1;
      context.size = 1;
      return;
   }

   context = context_stack.back();
   context_stack.pop_back();
}

void SimpleXPathEvaluator::push_cursor_state() 
{
   CursorState state{};
   state.tags = xml->CursorTags;

   if ((xml->CursorTags) and (xml->CursorTags->begin() != xml->CursorTags->end())) {
      state.index = size_t(xml->Cursor - xml->CursorTags->begin());
   }
   else state.index = 0;

   cursor_stack.push_back(state);
}

void SimpleXPathEvaluator::pop_cursor_state() 
{
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

std::vector<SimpleXPathEvaluator::AxisMatch> SimpleXPathEvaluator::dispatch_axis(AxisType Axis, 
   XMLTag *ContextNode, const XMLAttrib *ContextAttribute) 
{
   std::vector<AxisMatch> matches;

   auto append_nodes = [&matches](const std::vector<XMLTag *> &nodes) {
      matches.reserve(matches.size() + nodes.size());
      for (auto *node : nodes) {
         matches.push_back({ node, nullptr });
      }
   };

   bool attribute_context = ContextAttribute != nullptr;

   switch (Axis) {
      case AxisType::Child: {
         if (attribute_context) break;

         if (!ContextNode) {
            for (auto &tag : xml->Tags) {
               if (!tag.isTag()) continue;
               matches.push_back({ &tag, nullptr });
            }
         }
         else append_nodes(axis_evaluator.evaluate_axis(AxisType::Child, ContextNode));
         break;
      }

      case AxisType::Descendant: {
         if (attribute_context) break;

         if (!ContextNode) {
            for (auto &tag : xml->Tags) {
               if (!tag.isTag()) continue;
               matches.push_back({ &tag, nullptr });
               append_nodes(axis_evaluator.evaluate_axis(AxisType::Descendant, &tag));
            }
         }
         else append_nodes(axis_evaluator.evaluate_axis(AxisType::Descendant, ContextNode));
         break;
      }

      case AxisType::DescendantOrSelf: {
         if (attribute_context) {
            matches.push_back({ ContextNode, ContextAttribute });
            break;
         }

         if (!ContextNode) {
            matches.push_back({ nullptr, nullptr });
            for (auto &tag : xml->Tags) {
               if (!tag.isTag()) continue;
               matches.push_back({ &tag, nullptr });
               append_nodes(axis_evaluator.evaluate_axis(AxisType::Descendant, &tag));
            }
         }
         else {
            matches.push_back({ ContextNode, nullptr });
            append_nodes(axis_evaluator.evaluate_axis(AxisType::Descendant, ContextNode));
         }
         break;
      }

      case AxisType::Self: {
         if (attribute_context) {
            matches.push_back({ ContextNode, ContextAttribute });
            matches.push_back({ ContextNode, nullptr });
         }
         else matches.push_back({ ContextNode, nullptr });
         break;
      }

      case AxisType::Parent: {
         if (attribute_context) {
            if (ContextNode) matches.push_back({ ContextNode, nullptr });
         }
         else if (ContextNode) append_nodes(axis_evaluator.evaluate_axis(AxisType::Parent, ContextNode));
         break;
      }

      case AxisType::Ancestor: {
         if (attribute_context) {
            if (ContextNode) {
               matches.push_back({ ContextNode, nullptr });
               append_nodes(axis_evaluator.evaluate_axis(AxisType::Ancestor, ContextNode));
            }
         }
         else if (ContextNode) append_nodes(axis_evaluator.evaluate_axis(AxisType::Ancestor, ContextNode));
         break;
      }

      case AxisType::AncestorOrSelf: {
         if (attribute_context) {
            matches.push_back({ ContextNode, ContextAttribute });
            if (ContextNode) {
               matches.push_back({ ContextNode, nullptr });
               append_nodes(axis_evaluator.evaluate_axis(AxisType::Ancestor, ContextNode));
            }
         }
         else if (ContextNode) {
            matches.push_back({ ContextNode, nullptr });
            append_nodes(axis_evaluator.evaluate_axis(AxisType::Ancestor, ContextNode));
         }
         else matches.push_back({ nullptr, nullptr });
         break;
      }

      case AxisType::FollowingSibling: {
         if (attribute_context) break;
         if (ContextNode) append_nodes(axis_evaluator.evaluate_axis(AxisType::FollowingSibling, ContextNode));
         break;
      }

      case AxisType::PrecedingSibling: {
         if (attribute_context) break;
         if (ContextNode) append_nodes(axis_evaluator.evaluate_axis(AxisType::PrecedingSibling, ContextNode));
         break;
      }

      case AxisType::Following: {
         if (attribute_context) break;
         if (ContextNode) append_nodes(axis_evaluator.evaluate_axis(AxisType::Following, ContextNode));
         break;
      }

      case AxisType::Preceding: {
         if (attribute_context) break;
         if (ContextNode) append_nodes(axis_evaluator.evaluate_axis(AxisType::Preceding, ContextNode));
         break;
      }

      case AxisType::Attribute: {
         if (attribute_context) break;
         if (ContextNode and ContextNode->isTag()) {
            for (size_t index = 1; index < ContextNode->Attribs.size(); ++index) {
               matches.push_back({ ContextNode, &ContextNode->Attribs[index] });
            }
         }
         break;
      }

      case AxisType::Namespace:
         break;
   }

   return matches;
}

//********************************************************************************************************************
// Enhanced Entry Point (AST Evaluation)

ERR SimpleXPathEvaluator::find_tag_enhanced(std::string_view XPath, uint32_t CurrentPrefix) {
   return find_tag_enhanced_internal(XPath, CurrentPrefix, true);
}

ERR SimpleXPathEvaluator::find_tag_enhanced_internal(std::string_view XPath, uint32_t CurrentPrefix, bool AllowUnionSplit) {
   if (AllowUnionSplit) {
      auto union_paths = split_union_paths(XPath);
      if (union_paths.size() > 1) {
         auto saved_context = context;
         auto saved_context_stack = context_stack;
         auto saved_cursor_stack = cursor_stack;
         auto saved_cursor_tags = xml->CursorTags;
         auto saved_cursor = xml->Cursor;
         auto saved_attrib = xml->Attrib;
         bool saved_expression_unsupported = expression_unsupported;

         ERR last_error = ERR::Search;

         for (auto branch : union_paths) {
            context = saved_context;
            context_stack = saved_context_stack;
            cursor_stack = saved_cursor_stack;
            xml->CursorTags = saved_cursor_tags;
            xml->Cursor = saved_cursor;
            xml->Attrib = saved_attrib;
            expression_unsupported = saved_expression_unsupported;

            auto result = find_tag_enhanced_internal(branch, CurrentPrefix, false);
            if ((result IS ERR::Okay) or (result IS ERR::Terminate)) return result;

            if (result != ERR::Search) {
               last_error = result;
               break;
            }
         }

         context = saved_context;
         context_stack = saved_context_stack;
         cursor_stack = saved_cursor_stack;
         xml->CursorTags = saved_cursor_tags;
         xml->Cursor = saved_cursor;
         xml->Attrib = saved_attrib;
         expression_unsupported = saved_expression_unsupported;

         return last_error;
      }
   }

   // Ensure the document index is up to date so ParentID links are valid during AST traversal

   (void)xml->getMap();

   XPathTokenizer tokenizer;
   auto tokens = tokenizer.tokenize(XPath);

   XPathParser parser;
  
   if (auto ast = parser.parse(tokens); ast) {
      return evaluate_ast(ast.get(), CurrentPrefix);
   }
   else return ERR::Syntax;
}

//********************************************************************************************************************
// AST Evaluation Methods

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

   bool is_last_step = (StepIndex + 1 >= Steps.size());

   for (auto *context_node : ContextNodes) {
      const XMLAttrib *context_attribute = nullptr;

      if ((context_node) and context.attribute_node and (context_node IS context.context_node)) {
         context_attribute = context.attribute_node;
      }

      auto axis_matches = dispatch_axis(axis, context_node, context_attribute);

      std::vector<AxisMatch> filtered;
      filtered.reserve(axis_matches.size());

      for (auto &match : axis_matches) {
         if (!match_node_test(node_test, axis, match.node, match.attribute, CurrentPrefix)) continue;
         filtered.push_back(match);
      }

      if (filtered.empty()) continue;

      for (auto *predicate_node : predicate_nodes) {
         std::vector<AxisMatch> passed;
         passed.reserve(filtered.size());

         for (size_t index = 0; index < filtered.size(); ++index) {
            auto &match = filtered[index];
            push_context(match.node, index + 1, filtered.size(), match.attribute);

            auto predicate_result = evaluate_predicate(predicate_node, CurrentPrefix);
            pop_context();

            if (predicate_result IS PredicateResult::Unsupported) {
               return ERR::Failed;
            }

            if (predicate_result IS PredicateResult::Match) passed.push_back(match);
         }

         filtered.swap(passed);
         if (filtered.empty()) break;
      }

      if (filtered.empty()) continue;

      if ((axis IS AxisType::Attribute) and (!is_last_step)) {
         return ERR::Failed;
      }

      for (size_t index = 0; index < filtered.size(); ++index) {
         auto &match = filtered[index];
         auto *candidate = match.node;

         push_context(candidate, index + 1, filtered.size(), match.attribute);

         if (axis IS AxisType::Attribute) {
            if (!candidate or !match.attribute) {
               pop_context();
               continue;
            }

            auto tags = xml->getInsert(candidate, xml->Cursor);
            if (!tags) {
               pop_context();
               continue;
            }

            xml->CursorTags = tags;
            xml->Attrib = match.attribute->Name;

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

bool SimpleXPathEvaluator::match_node_test(const XPathNode *NodeTest, AxisType Axis, XMLTag *Candidate, const XMLAttrib *Attribute, uint32_t CurrentPrefix) {
   bool attribute_axis = (Axis IS AxisType::Attribute) or ((Axis IS AxisType::Self) and (Attribute != nullptr));

   if (!NodeTest) {
      if (attribute_axis) return Attribute != nullptr;
      return Candidate != nullptr;
   }

   if (attribute_axis) {
      if (!Attribute) return false;

      if (NodeTest->type IS XPathNodeType::NodeTypeTest) {
         return NodeTest->value IS "node";
      }

      if (NodeTest->type IS XPathNodeType::Wildcard) return true;

      if (NodeTest->type IS XPathNodeType::NameTest) {
         std::string_view test_name = NodeTest->value;
         if (test_name.empty()) return false;

         std::string_view attribute_name = Attribute->Name;

         if (test_name.find('*') != std::string::npos) return pf::wildcmp(test_name, attribute_name);

         if ((xml->Flags & XMF::NAMESPACE_AWARE) != XMF::NIL) {
            uint32_t expected_prefix = 0;
            std::string_view expected_local = test_name;

            if (auto colon = test_name.find(':'); colon != std::string::npos) {
               expected_prefix = pf::strhash(test_name.substr(0, colon));
               expected_local = test_name.substr(colon + 1);
            }

            std::string_view candidate_local = attribute_name;
            uint32_t candidate_prefix = 0;

            if (auto colon = attribute_name.find(':'); colon != std::string::npos) {
               candidate_prefix = pf::strhash(attribute_name.substr(0, colon));
               candidate_local = attribute_name.substr(colon + 1);
            }

            bool name_matches = expected_local.find('*') != std::string::npos ? pf::wildcmp(expected_local, candidate_local) : pf::iequals(expected_local, candidate_local);
            bool prefix_matches = expected_prefix ? (candidate_prefix IS expected_prefix) : true;

            return name_matches and prefix_matches;
         }

         return pf::iequals(test_name, attribute_name);
      }

      return false;
   }

   if (NodeTest->type IS XPathNodeType::NodeTypeTest) {
      if (NodeTest->value IS "node") return true;
      if (!Candidate) return false;

      if (NodeTest->value IS "text") {
         if (!Candidate->isContent()) return false;
         return ((Candidate->Flags & (XTF::COMMENT | XTF::INSTRUCTION | XTF::NOTATION)) IS XTF::NIL);
      }

      if (NodeTest->value IS "comment") {
         bool has_comment_flag = Candidate ? (((Candidate->Flags & XTF::COMMENT) IS XTF::NIL) ? false : true) : false;
         return Candidate and has_comment_flag;
      }

      return false;
   }

   if (NodeTest->type IS XPathNodeType::ProcessingInstructionTest) {
      if (!Candidate) return false;
      if ((Candidate->Flags & XTF::INSTRUCTION) IS XTF::NIL) return false;

      if (NodeTest->value.empty()) return true;

      std::string_view candidate_name;
      if (!Candidate->Attribs.empty()) candidate_name = Candidate->Attribs[0].Name;

      if (!candidate_name.empty() and (candidate_name.front() IS '?')) candidate_name.remove_prefix(1);
      if (candidate_name.empty()) return false;

      std::string candidate_target(candidate_name);
      return pf::iequals(candidate_target, NodeTest->value);
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

std::string node_set_string_value(const XPathValue &Value, size_t Index)
{
   if (Value.node_set_string_override.has_value() and (Index IS 0)) {
      return *Value.node_set_string_override;
   }

   if (Index >= Value.node_set.size()) return std::string();

   XMLTag *tag = Value.node_set[Index];
   if (!tag) return std::string();

   if (tag->isContent()) {
      if (!tag->Attribs.empty() and !tag->Attribs[0].Value.empty()) {
         return tag->Attribs[0].Value;
      }
      return tag->getContent();
   }

   return tag->getContent();
}

double node_set_number_value(const XPathValue &Value, size_t Index)
{
   std::string str = node_set_string_value(Value, Index);
   if (str.empty()) return std::numeric_limits<double>::quiet_NaN();

   char *end_ptr = nullptr;
   double result = std::strtod(str.c_str(), &end_ptr);
   if ((end_ptr IS str.c_str()) or (*end_ptr != '\0')) {
      return std::numeric_limits<double>::quiet_NaN();
   }

   return result;
}

bool compare_xpath_values(const XPathValue &left_value,
                          const XPathValue &right_value)
{
   auto left_type = left_value.type;
   auto right_type = right_value.type;

   if ((left_type IS XPathValueType::Boolean) or (right_type IS XPathValueType::Boolean)) {
      bool left_boolean = left_value.to_boolean();
      bool right_boolean = right_value.to_boolean();
      return left_boolean IS right_boolean;
   }

   if ((left_type IS XPathValueType::Number) or (right_type IS XPathValueType::Number)) {
      if ((left_type IS XPathValueType::NodeSet) or (right_type IS XPathValueType::NodeSet)) {
         const XPathValue &node_value = (left_type IS XPathValueType::NodeSet) ? left_value : right_value;
         const XPathValue &number_value = (left_type IS XPathValueType::NodeSet) ? right_value : left_value;

         double comparison_number = number_value.to_number();
         if (std::isnan(comparison_number)) return false;

         for (size_t index = 0; index < node_value.node_set.size(); ++index) {
            double node_number = node_set_number_value(node_value, index);
            if (std::isnan(node_number)) continue;
            if (node_number IS comparison_number) return true;
         }

         return false;
      }

      double left_number = left_value.to_number();
      double right_number = right_value.to_number();
      return left_number IS right_number;
   }

   if ((left_type IS XPathValueType::NodeSet) or (right_type IS XPathValueType::NodeSet)) {
      if ((left_type IS XPathValueType::NodeSet) and (right_type IS XPathValueType::NodeSet)) {
         for (size_t left_index = 0; left_index < left_value.node_set.size(); ++left_index) {
            std::string left_string = node_set_string_value(left_value, left_index);

            for (size_t right_index = 0; right_index < right_value.node_set.size(); ++right_index) {
               std::string right_string = node_set_string_value(right_value, right_index);
               if (pf::iequals(left_string, right_string)) return true;
            }
         }

         return false;
      }

      const XPathValue &node_value = (left_type IS XPathValueType::NodeSet) ? left_value : right_value;
      const XPathValue &string_value = (left_type IS XPathValueType::NodeSet) ? right_value : left_value;

      std::string comparison_string = string_value.to_string();

      for (size_t index = 0; index < node_value.node_set.size(); ++index) {
         std::string node_string = node_set_string_value(node_value, index);
         if (pf::iequals(node_string, comparison_string)) return true;
      }

      return false;
   }

   std::string left_string = left_value.to_string();
   std::string right_string = right_value.to_string();
   return pf::iequals(left_string, right_string);
}

} // namespace

std::vector<XMLTag *> SimpleXPathEvaluator::collect_step_results(const std::vector<AxisMatch> &ContextNodes,
                                                                 const std::vector<const XPathNode *> &Steps,
                                                                 size_t StepIndex,
                                                                 uint32_t CurrentPrefix,
                                                                 bool &Unsupported)
{
   std::vector<XMLTag *> results;

   if (Unsupported) return results;

   if (StepIndex >= Steps.size()) {
      for (auto &entry : ContextNodes) results.push_back(entry.node);
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

   bool is_last_step = (StepIndex + 1 >= Steps.size());

   for (auto &context_entry : ContextNodes) {
      auto axis_matches = dispatch_axis(axis, context_entry.node, context_entry.attribute);

      std::vector<AxisMatch> filtered;
      filtered.reserve(axis_matches.size());

      for (auto &match : axis_matches) {
         if (!match_node_test(node_test, axis, match.node, match.attribute, CurrentPrefix)) continue;
         filtered.push_back(match);
      }

      if (filtered.empty()) continue;

      for (auto *predicate_node : predicate_nodes) {
         std::vector<AxisMatch> passed;
         passed.reserve(filtered.size());

         for (size_t index = 0; index < filtered.size(); ++index) {
            auto &match = filtered[index];
            push_context(match.node, index + 1, filtered.size(), match.attribute);
            auto predicate_result = evaluate_predicate(predicate_node, CurrentPrefix);
            pop_context();

            if (predicate_result IS PredicateResult::Unsupported) {
               Unsupported = true;
               return {};
            }

            if (predicate_result IS PredicateResult::Match) passed.push_back(match);
         }

         filtered.swap(passed);

         if (filtered.empty()) break;
      }

      if (filtered.empty()) continue;

      if (is_last_step) {
         for (auto &match : filtered) results.push_back(match.node);
         continue;
      }

      std::vector<AxisMatch> next_context;
      next_context.reserve(filtered.size());
      for (auto &match : filtered) next_context.push_back(match);

      auto child_results = collect_step_results(next_context, Steps, StepIndex + 1, CurrentPrefix, Unsupported);
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
   else {
      std::vector<AxisMatch> initial_matches;
      initial_matches.reserve(initial_context.size());

      for (auto *candidate : initial_context) {
         const XMLAttrib *attribute = nullptr;
         if ((candidate) and context.attribute_node and (candidate IS context.context_node)) attribute = context.attribute_node;
         initial_matches.push_back({ candidate, attribute });
      }

      node_results = collect_step_results(initial_matches, work_steps, 0, CurrentPrefix, unsupported);
   }

   if (unsupported) {
      expression_unsupported = true;
      return XPathValue();
   }

   if (context.attribute_node and (steps.size() IS 1)) {
      const XPathNode *step = steps[0];
      const XPathNode *axis_node = nullptr;
      const XPathNode *node_test = nullptr;

      for (size_t index = 0; index < step->child_count(); ++index) {
         auto *child = step->get_child(index);
         if (!child) continue;

         if (child->type IS XPathNodeType::AxisSpecifier) axis_node = child;
         else if ((!node_test) and ((child->type IS XPathNodeType::NameTest) or
                                    (child->type IS XPathNodeType::Wildcard) or
                                    (child->type IS XPathNodeType::NodeTypeTest))) node_test = child;
      }

      AxisType axis = axis_node ? AxisEvaluator::parse_axis_name(axis_node->value) : AxisType::Child;

      if ((axis IS AxisType::Self) and !node_results.empty()) {
         bool accepts_attribute = false;

         if (!node_test) accepts_attribute = true;
         else if (node_test->type IS XPathNodeType::Wildcard) accepts_attribute = true;
         else if (node_test->type IS XPathNodeType::NodeTypeTest) accepts_attribute = node_test->value IS "node";

         if (accepts_attribute) {
            return XPathValue(node_results, context.attribute_node->Value);
         }
      }
   }

   if (attribute_step) {
      std::vector<std::string> attribute_values;
      std::vector<XMLTag *> attribute_nodes;

      for (auto *candidate : node_results) {
         if (!candidate) continue;

         auto matches = dispatch_axis(AxisType::Attribute, candidate);
         for (auto &match : matches) {
            if (!match.attribute) continue;
            if (!match_node_test(attribute_test, AxisType::Attribute, match.node, match.attribute, CurrentPrefix)) continue;
            attribute_values.push_back(match.attribute->Value);
            attribute_nodes.push_back(match.node);
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

   if (ExprNode->type IS XPathNodeType::VariableReference) {
      // Look up variable in the XML object's variable storage
      auto it = xml->Variables.find(ExprNode->value);
      if (it != xml->Variables.end()) {
         return XPathValue(it->second);
      }
      else {
         // Variable not found - return empty string (XPath standard behavior)
         return XPathValue(std::string(""));
      }
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

   if (function_name IS "text") {
      std::vector<XMLTag *> text_nodes;
      std::optional<std::string> first_value;

      if (context.context_node) {
         for (auto &child : context.context_node->Children) {
            if (!child.isContent()) continue;
            text_nodes.push_back(&child);

            if ((!first_value.has_value()) and (!child.Attribs.empty())) {
               first_value = child.Attribs[0].Value;
            }
         }
      }

      return XPathValue(text_nodes, first_value);
   }

   return function_library.call_function(function_name, args, context);
}

