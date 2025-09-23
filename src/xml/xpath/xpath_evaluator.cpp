// XPath Evaluator Implementation

//********************************************************************************************************************
// Context Management

void SimpleXPathEvaluator::push_context(XMLTag* node, size_t position, size_t size) {
   context.context_node = node;
   context.position = position;
   context.size = size;
}

void SimpleXPathEvaluator::pop_context() {
   // TODO: Implement context stack if needed
}

void SimpleXPathEvaluator::push_cursor_state(XMLTag* tag, size_t child_index, size_t total_children) {
   cursor_stack.push_back({tag, child_index, total_children});
   cursor_tags.push_back(tag);
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

ERR SimpleXPathEvaluator::find_tag_enhanced(std::string_view XPath, uint32_t current_prefix) {
   pf::Log log(__FUNCTION__);
   log.msg("Enhanced XPath: %.*s", int(XPath.size()), XPath.data());

   // Try AST-based parsing first
   XPathTokenizer tokenizer;
   auto tokens = tokenizer.tokenize(XPath);

   XPathParser parser;
   auto ast = parser.parse(tokens);

   if (ast) {
      log.msg("AST parsed successfully, evaluating...");
      auto result = evaluate_ast(ast.get(), current_prefix);
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

   return evaluate_step(XPath, info, current_prefix);
}

//********************************************************************************************************************
// AST Evaluation Methods (Phase 1+ of AST_PLAN.md)

ERR SimpleXPathEvaluator::evaluate_ast(const XPathNode* node, uint32_t current_prefix) {
   if (!node) return ERR::Failed;

   switch (node->type) {
      case XPathNodeType::LocationPath:
         return evaluate_location_path(node, current_prefix);

      case XPathNodeType::Step:
         return evaluate_step_ast(node, current_prefix);

      default:
         // TODO: Implement other node types as needed for AST_PLAN.md phases
         return ERR::Failed;
   }
}

ERR SimpleXPathEvaluator::evaluate_location_path(const XPathNode* path_node, uint32_t current_prefix) {
   // TODO: Phase 1 implementation - location path traversal backbone
   // This is where the main AST traversal logic will go

   pf::Log log(__FUNCTION__);
   log.msg("evaluate_location_path: Phase 1 - AST Location Path Traversal not yet implemented");

   return ERR::Failed; // Force fallback to legacy evaluator for now
}

ERR SimpleXPathEvaluator::evaluate_step_ast(const XPathNode* step_node, uint32_t current_prefix) {
   // TODO: Phase 1 implementation - step evaluation with axis dispatcher

   pf::Log log(__FUNCTION__);
   log.msg("evaluate_step_ast: Phase 1 - AST Step Evaluation not yet implemented");

   return ERR::Failed; // Force fallback to legacy evaluator for now
}

bool SimpleXPathEvaluator::match_node_test(const XPathNode* node_test, uint32_t current_prefix) {
   // TODO: Phase 1 implementation - node test matching in AST pipeline

   return false; // Force fallback for now
}

bool SimpleXPathEvaluator::evaluate_predicate(const XPathNode* predicate_node, uint32_t current_prefix) {
   // TODO: Phase 2 implementation - predicate evaluation for AST traversal

   return false; // Force fallback for now
}

//********************************************************************************************************************
// Function and Expression Evaluation (Phase 3 of AST_PLAN.md)

XPathValue SimpleXPathEvaluator::evaluate_expression(const XPathNode* expr_node, uint32_t current_prefix) {
   // TODO: Phase 3 implementation - expression evaluation with node-set support

   return XPathValue(); // Return empty value for now
}

XPathValue SimpleXPathEvaluator::evaluate_function_call(const XPathNode* func_node, uint32_t current_prefix) {
   // TODO: Phase 3 implementation - function call evaluation using function_library

   if (!func_node or func_node->type != XPathNodeType::FunctionCall) {
      return XPathValue();
   }

   // Extract function name and arguments
   std::string function_name = func_node->value;
   std::vector<XPathValue> args;

   // TODO: Evaluate arguments from child nodes

   // Call the function library
   return function_library.call_function(function_name, args, context);
}

//********************************************************************************************************************
// Legacy String-Based Methods (Phase 1 - will be removed in Phase 5)

ERR SimpleXPathEvaluator::parse_path(std::string_view XPath, PathInfo &info) {
   pf::Log log(__FUNCTION__);

   if ((XPath.empty()) or (XPath[0] != '/')) {
      log.warning("Missing '/' prefix in '%.*s'.", int(XPath.size()), XPath.data());
      return ERR::StringFormat;
   }

   // Check for flat scan (//)
   info.pos = [&info, XPath]() mutable {
      if (XPath[0] != '/') return size_t(0);
      if ((XPath.size() > 1) and (XPath[1] != '/')) return size_t(1);
      info.flat_scan = true;
      return size_t(2);
   }();

   // Check if the path is something like '/@attrib' which means we want the attribute value of the current tag
   if ((info.pos < XPath.size()) and (XPath[info.pos] IS '@')) {
      xml->Attrib.assign(XPath.substr(info.pos + 1));
      return ERR::Okay;
   }

   // Parse the tag name
   auto start = info.pos;
   auto delimiter_pos = XPath.find_first_of("/[(", info.pos);
   info.pos = (delimiter_pos != std::string_view::npos) ? delimiter_pos : XPath.size();
   if (info.pos > start) info.tag_name = XPath.substr(start, info.pos - start);
   else info.tag_name = "*";

   // Parse namespace prefix from current tag
   if ((xml->Flags & XMF::NAMESPACE_AWARE) != XMF::NIL) {
      if (auto colon = info.tag_name.find(':'); colon != std::string_view::npos) {
         info.tag_prefix = pf::strhash(info.tag_name.substr(0, colon));
         info.tag_name = info.tag_name.substr(colon + 1);
      }
   }

   // Parse filter instructions
   char end_char;
   if ((info.pos < XPath.size()) and ((XPath[info.pos] IS '[') or (XPath[info.pos] IS '('))) {
      if (XPath[info.pos] IS '[') end_char = ']';
      else end_char = ')';

      info.pos++;

      auto non_space_pos = XPath.find_first_not_of(" \t\n\r", info.pos);
      info.pos = (non_space_pos != std::string_view::npos) ? non_space_pos : XPath.size();

      if ((info.pos < XPath.size()) and (XPath[info.pos] >= '0') and (XPath[info.pos] <= '9')) { // Parse index
         char *end;
         info.subscript = strtol(XPath.data() + info.pos, &end, 0);
         if (info.subscript < 1) return ERR::Syntax; // Subscripts start from 1, not 0
         info.pos = end - XPath.data();
      }
      else if ((info.pos < XPath.size()) and ((XPath[info.pos] IS '@') or (XPath[info.pos] IS '='))) {
         if (XPath[info.pos] IS '@') {  // Parse attribute filter such as "[@id='5']" or "[@*='v']" or "[@attr]"
            info.pos++;

            auto len = info.pos;
            if ((len < XPath.size()) and (XPath[len] IS '*')) {
               // Attribute wildcard
               info.attrib_name = "*";
               len++;
            } else {
               while (len < XPath.size()) {
                  char c = XPath[len];
                  if (std::isalnum(static_cast<unsigned char>(c)) or c IS '_' or c IS '-' or c IS ':' or c IS '.') len++;
                  else break;
               }
               info.attrib_name = XPath.substr(info.pos, len - info.pos);
            }
            if (info.attrib_name.empty()) return ERR::Syntax;

            info.pos = len;
            auto non_space_pos2 = XPath.find_first_not_of(" \t\n\r", info.pos);
            info.pos = (non_space_pos2 != std::string_view::npos) ? non_space_pos2 : XPath.size(); // Skip whitespace

            if ((info.pos < XPath.size()) and (XPath[info.pos] IS '=')) info.pos++;
         }
         else info.pos++; // Skip '=' (indicates matching on content)

         auto non_space_pos3 = XPath.find_first_not_of(" \t\n\r", info.pos);
         info.pos = (non_space_pos3 != std::string_view::npos) ? non_space_pos3 : XPath.size(); // Skip whitespace

         // Parse value (optional). If no value provided, treat as attribute-existence test [@attr]
         if ((info.pos < XPath.size()) and ((XPath[info.pos] IS '\'') or (XPath[info.pos] IS '"'))) {
            const char quote = XPath[info.pos++];
            bool esc_attrib = false;
            auto end = info.pos;
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
               else if (XPath[end] IS '*') info.wild = true;
               end++;
            }

            if ((end >= XPath.size()) or (XPath[end] != quote)) return ERR::Syntax; // Quote not terminated correctly

            info.attrib_value.assign(XPath.substr(info.pos, end - info.pos));
            info.pos = end + 1;

            if (esc_attrib) {
               for (int i=0; i < std::ssize(info.attrib_value); i++) {
                  if (info.attrib_value[i] != '\\') continue;
                  auto ch = info.attrib_value[i+1];
                  if ((ch IS '*') or (ch IS quote) or (ch IS '\\')) {
                     info.attrib_value.erase(i);
                     i--;
                  }
               }
            }
         }
         else if ((info.pos < XPath.size()) and (XPath[info.pos] != end_char)) {
            auto end_pos = XPath.find(end_char, info.pos);
            int end = (end_pos != std::string_view::npos) ? end_pos : XPath.size();

            // Check for wildcards in the range
            if (XPath.substr(info.pos, end - info.pos).find('*') != std::string_view::npos) info.wild = true;
            info.attrib_value.assign(XPath.substr(info.pos, end - info.pos));
            info.pos = end;
         }
      }
      else return ERR::Syntax;

      auto non_space_pos4 = XPath.find_first_not_of(" \t\n\r", info.pos);
      info.pos = (non_space_pos4 != std::string_view::npos) ? non_space_pos4 : XPath.size(); // Skip whitespace
      if ((info.pos >= XPath.size()) or (XPath[info.pos] != end_char)) return ERR::Syntax;
      info.pos++;
   }

   return ERR::Okay;
}

bool SimpleXPathEvaluator::match_tag(const PathInfo &info, uint32_t current_prefix) {
   bool tag_matched = false;
   uint32_t cursor_prefix = current_prefix;

   // Special handling for function calls - check if subscript == 0 and attrib_value contains function
   if (info.subscript IS 0 and !info.attrib_value.empty()) {
      // Check if attrib_value looks like a function call (contains "position()=", "last()", etc.)
      if (info.attrib_value.find("position()") != std::string::npos or
          info.attrib_value.find("last()") != std::string::npos or
          info.attrib_value.find("count(") != std::string::npos) {

         // Try to parse and evaluate the function call
         // For now, handle simple cases like "position()=2"
         if (info.attrib_value.find("position()=") != std::string::npos) {
            // Extract the number after "position()="
            auto pos = info.attrib_value.find("position()=");
            if (pos != std::string::npos) {
               auto num_start = pos + 11; // Length of "position()="
               if (num_start < info.attrib_value.length()) {
                  int expected_position = std::stoi(info.attrib_value.substr(num_start));
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

      auto tag_wild = info.tag_name.find('*') != std::string_view::npos;
      bool name_matches = tag_wild ? pf::wildcmp(info.tag_name, cursor_local_name) : pf::iequals(info.tag_name, cursor_local_name);
      bool prefix_matches = info.tag_prefix ? cursor_prefix IS info.tag_prefix : true;
      tag_matched = name_matches and prefix_matches;
   }
   else { // Traditional matching: just compare full tag names
      auto tag_wild = info.tag_name.find('*') != std::string_view::npos;
      if (tag_wild) tag_matched = pf::wildcmp(info.tag_name, xml->Cursor->name());
      else tag_matched = pf::iequals(info.tag_name, xml->Cursor->name());
   }

   if (!tag_matched) return false;

   // Check attribute/content filters
   if ((!info.attrib_name.empty()) or (!info.attrib_value.empty())) {
      if (xml->Cursor->name()) {
         if (!info.attrib_name.empty()) { // Match by named attribute value or existence; '*' matches any attribute name
            for (int a=1; a < std::ssize(xml->Cursor->Attribs); ++a) {
               bool name_matches = (info.attrib_name IS "*") ? true : pf::iequals(xml->Cursor->Attribs[a].Name, info.attrib_name);
               if (name_matches) {
                  // If no attribute value specified, treat as existence test
                  if (info.attrib_value.empty()) return true;
                  if (info.wild) {
                     if (pf::wildcmp(xml->Cursor->Attribs[a].Value, info.attrib_value)) return true;
                  }
                  else if (pf::iequals(xml->Cursor->Attribs[a].Value, info.attrib_value)) {
                     return true;
                  }
               }
            }
            return false;
         }
         else if (!info.attrib_value.empty()) {
            if ((!xml->Cursor->Children.empty()) and (xml->Cursor->Children[0].Attribs[0].isContent())) {
               if (info.wild) {
                  return pf::wildcmp(xml->Cursor->Children[0].Attribs[0].Value, info.attrib_value);
               }
               else return pf::iequals(xml->Cursor->Children[0].Attribs[0].Value, info.attrib_value);
            }
            return false;
         }
      }
      return false;
   }

   return true;
}

ERR SimpleXPathEvaluator::evaluate_step(std::string_view XPath, PathInfo info, uint32_t current_prefix) {
   pf::Log log(__FUNCTION__);

   if ((xml->Flags & XMF::LOG_ALL) != XMF::NIL) {
      log.branch("XPath: %.*s, TagName: %.*s", int(XPath.size()), XPath.data(), int(info.tag_name.size()), info.tag_name.data());
   }

   // Check if this predicate contains function calls that need position tracking
   bool has_function_call = false;
   std::string function_expression;
   if (!info.attrib_value.empty()) {
      pf::Log func_log("Function Detection");
      func_log.msg("Checking attrib_value: '%s'", info.attrib_value.c_str());
      if (info.attrib_value.find("position()") != std::string::npos or
          info.attrib_value.find("last()") != std::string::npos) {
         has_function_call = true;
         function_expression = info.attrib_value;
         func_log.msg("Function call detected: '%s'", function_expression.c_str());
      }
   }

   // For function calls, we need to collect all matching nodes first to get the total count
   std::vector<XMLTag*> matching_nodes;
   if (has_function_call) {
      // First pass: collect all nodes that match the tag name (without predicate)
      PathInfo tag_only_info = info;
      tag_only_info.attrib_value.clear(); // Remove predicate
      tag_only_info.subscript = 0; // Remove index

      for (auto cursor = xml->CursorTags->begin(); cursor != xml->CursorTags->end(); ++cursor) {
         auto saved_cursor = xml->Cursor;
         xml->Cursor = cursor;
         if (match_tag(tag_only_info, current_prefix)) {
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
            if (info.pos < XPath.size() and XPath[info.pos] IS '/' and info.pos + 1 < XPath.size() and XPath[info.pos+1] IS '@') {
               xml->Attrib.assign(XPath.substr(info.pos + 2));
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
      uint32_t cursor_prefix = current_prefix;

      if (info.flat_scan or match_tag(info, cursor_prefix)) {
         if (info.flat_scan and !match_tag(info, cursor_prefix)) {
            // For flat scan, check children if current tag doesn't match
            if (!xml->Cursor->Children.empty()) {
               auto save_cursor = xml->Cursor;
               auto save_tags   = xml->CursorTags;

               xml->CursorTags = &xml->Cursor->Children;
               xml->Cursor     = xml->Cursor->Children.begin();

               ERR error = evaluate_step(XPath, info, cursor_prefix);
               if ((error IS ERR::Okay) and (!xml->Callback.defined())) return ERR::Okay;

               xml->Cursor     = save_cursor;
               xml->CursorTags = save_tags;
            }
            continue;
         }

         match = true;
      }

      if ((not match) and (not info.flat_scan)) continue;

      if (info.subscript > 1) {
         info.subscript--;
         continue;
      }
      else if (info.subscript IS 1) {
         stop = true;
      }

      bool path_ended = info.pos >= XPath.size() or ((XPath[info.pos] IS '/') and (info.pos + 1 < XPath.size()) and (XPath[info.pos+1] IS '@'));
      if ((match) and (path_ended)) { // Matching tag found and there is nothing left in the path
         if (info.pos < XPath.size()) xml->Attrib.assign(XPath.substr(info.pos + 2));
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
         if (info.flat_scan) error = evaluate_step(XPath, info, cursor_prefix); // Continue search from the beginning of the tag name
         else {
            // Parse the next step
            PathInfo next_info;
            auto parse_err = parse_path(XPath.substr(info.pos), next_info);
            if (parse_err != ERR::Okay) return parse_err;
            error = evaluate_step(XPath.substr(info.pos), next_info, cursor_prefix);
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

bool SimpleXPathEvaluator::evaluate_function_expression(const std::string& expression) {
   // Handle simple function expressions like:
   // "position()=2"
   // "last()"
   // "position()!=1"

   if (expression.find("position()=") != std::string::npos) {
      auto pos = expression.find("position()=");
      if (pos != std::string::npos) {
         auto num_start = pos + 11; // Length of "position()="
         if (num_start < expression.length()) {
            int expected_position = std::stoi(expression.substr(num_start));
            XPathValue pos_result = function_library.call_function("position", {}, context);
            return pos_result.to_number() IS expected_position;
         }
      }
   }
   else if (expression IS "last()") {
      XPathValue last_result = function_library.call_function("last", {}, context);
      return last_result.to_boolean(); // last() itself is truthy
   }
   else if (expression.find("last()") != std::string::npos and expression.find("=") != std::string::npos) {
      // Handle expressions like "position()=last()"
      XPathValue pos_result = function_library.call_function("position", {}, context);
      XPathValue last_result = function_library.call_function("last", {}, context);
      return pos_result.to_number() IS last_result.to_number();
   }

   return false; // Unsupported expression
}