// XPath Evaluator Implementation

//********************************************************************************************************************
// Context Management

void SimpleXPathEvaluator::push_context(XMLTag *Node, size_t Position, size_t Size) {
   context.context_node = Node;
   context.position = Position;
   context.size = Size;
}

void SimpleXPathEvaluator::pop_context() {
   // TODO: Implement context stack if needed
}

void SimpleXPathEvaluator::push_cursor_state(XMLTag *Tag, size_t ChildIndex, size_t TotalChildren) {
   cursor_stack.push_back({Tag, ChildIndex, TotalChildren});
   cursor_tags.push_back(Tag);
}

void SimpleXPathEvaluator::pop_cursor_state() {
   if (!cursor_stack.empty()) {
      cursor_stack.pop_back();
   }
   if (!cursor_tags.empty()) {
      cursor_tags.pop_back();
   }
}

//********************************************************************************************************************
// Enhanced Entry Point (AST + Legacy Fallback)

ERR SimpleXPathEvaluator::find_tag_enhanced(std::string_view XPath, uint32_t CurrentPrefix) {
   pf::Log log(__FUNCTION__);
   log.msg("Enhanced XPath: %.*s", int(XPath.size()), XPath.data());

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
   // TODO: Phase 1 implementation - location path traversal backbone
   // This is where the main AST traversal logic will go

   pf::Log log(__FUNCTION__);
   log.msg("evaluate_location_path: Phase 1 - AST Location Path Traversal not yet implemented");

   return ERR::Failed; // Force fallback to legacy evaluator for now
}

ERR SimpleXPathEvaluator::evaluate_step_ast(const XPathNode *StepNode, uint32_t CurrentPrefix) {
   // TODO: Phase 1 implementation - step evaluation with axis dispatcher

   pf::Log log(__FUNCTION__);
   log.msg("evaluate_step_ast: Phase 1 - AST Step Evaluation not yet implemented");

   return ERR::Failed; // Force fallback to legacy evaluator for now
}

bool SimpleXPathEvaluator::match_node_test(const XPathNode *NodeTest, uint32_t CurrentPrefix) {
   // TODO: Phase 1 implementation - node test matching in AST pipeline

   return false; // Force fallback for now
}

bool SimpleXPathEvaluator::evaluate_predicate(const XPathNode *PredicateNode, uint32_t CurrentPrefix) {
   // TODO: Phase 2 implementation - predicate evaluation for AST traversal

   return false; // Force fallback for now
}

//********************************************************************************************************************
// Function and Expression Evaluation (Phase 3 of AST_PLAN.md)

XPathValue SimpleXPathEvaluator::evaluate_expression(const XPathNode *ExprNode, uint32_t CurrentPrefix) {
   // TODO: Phase 3 implementation - expression evaluation with node-set support

   return XPathValue(); // Return empty value for now
}

XPathValue SimpleXPathEvaluator::evaluate_function_call(const XPathNode *FuncNode, uint32_t CurrentPrefix) {
   // TODO: Phase 3 implementation - function call evaluation using function_library

   if (!FuncNode or FuncNode->type != XPathNodeType::FunctionCall) {
      return XPathValue();
   }

   // Extract function name and arguments
   std::string function_name = FuncNode->value;
   std::vector<XPathValue> args;

   // TODO: Evaluate arguments from child nodes

   // Call the function library
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
                  int expected_position = std::stoi(Info.attrib_value.substr(num_start));
                  // The position context is handled in evaluate_step, but for string-based
                  // we need to manually track position. This is a limitation we'll note.
                  // We'll handle this in evaluate_step with position tracking
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
