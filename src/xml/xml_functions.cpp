
//********************************************************************************************************************
// C++20 constexpr string utilities and character classification

constexpr std::string_view view_or_empty(CSTRING Value) noexcept
{
   if (Value) return std::string_view(Value);
   return std::string_view();
}

constexpr bool is_name_char(char ch) noexcept
{
   return ((ch >= 'A') and (ch <= 'Z')) or
          ((ch >= 'a') and (ch <= 'z')) or
          ((ch >= '0') and (ch <= '9')) or
          (ch IS '.') or (ch IS '-') or (ch IS '_') or (ch IS ':');
}

constexpr bool is_name_start(char ch) noexcept
{
   return ((ch >= 'A') and (ch <= 'Z')) or
          ((ch >= 'a') and (ch <= 'z')) or
          (ch IS '_') or (ch IS ':');
}

constexpr bool is_whitespace(char ch) noexcept
{
   return ch <= 0x20;
}

constexpr char to_lower(char ch) noexcept
{
   return ((ch >= 'A') and (ch <= 'Z')) ? ch + 0x20 : ch;
}

// XML entity escape sequences
constexpr std::string_view xml_entities[] = {
   "&amp;", "&lt;", "&gt;", "&quot;"
};

constexpr char xml_chars[] = { '&', '<', '>', '"' };

static void output_attribvalue(std::string_view String, std::ostringstream &Output)
{
   for (char ch : String) {
      switch (ch) {
         case '&':  Output << "&amp;"; break;
         case '<':  Output << "&lt;"; break;
         case '>':  Output << "&gt;"; break;
         case '"':  Output << "&quot;"; break;
         default:   Output << ch; break;
      }
   }
}

inline void assign_string(STRING &Target, const std::string &Value)
{
   if (Target) { FreeResource(Target); Target = nullptr; }
   if (not Value.empty()) Target = pf::strclone(Value);
}


constexpr void skip_ws(std::string_view &view) noexcept
{
   while (not view.empty() and is_whitespace(view.front())) {
      view.remove_prefix(1);
   }
}

inline void skip_ws(const char *&ptr)
{
   if (not ptr) return;
   std::string_view view(ptr);
   skip_ws(view);
   ptr = view.data();
}

//********************************************************************************************************************

static bool ci_keyword(std::string_view &view, std::string_view keyword) noexcept
{
   if (keyword.empty() or view.size() < keyword.size()) return false;

   for (size_t i = 0; i < keyword.size(); ++i) {
      if (to_lower(view[i]) != to_lower(keyword[i])) return false;
   }

   // Check that we're not matching a partial name
   if ((view.size() > keyword.size()) and
       is_name_char(view[keyword.size()]) and
       (view[keyword.size()] != '[')) return false;

   view.remove_prefix(keyword.size());
   return true;
}

static bool ci_keyword(const char *&ptr, std::string_view keyword)
{
   if (not ptr) return false;
   std::string_view view(ptr);
   if (ci_keyword(view, keyword)) {
      ptr = view.data();
      return true;
   }
   return false;
}

static std::string_view read_name(std::string_view &view) noexcept
{
   if (view.empty() or not is_name_start(view.front())) {
      return std::string_view();
   }

   size_t length = 1;
   while ((length < view.size()) and is_name_char(view[length])) {
      length++;
   }

   auto result = view.substr(0, length);
   view.remove_prefix(length);
   return result;
}

static bool read_name(const char *&ptr, std::string &Result)
{
   if (not ptr) return false;
   std::string_view view(ptr);
   auto name = read_name(view);
   if (name.empty()) return false;

   Result.assign(name.data(), name.size());
   ptr = view.data();
   return true;
}

static ERR resolve_entity_internal(extXML *Self, const std::string &Name, std::string &Value,
   bool Parameter, std::unordered_set<std::string> &EntityStack, std::unordered_set<std::string> &ParameterStack);

static void expand_entity_references(extXML *Self, std::string &Value,
   std::unordered_set<std::string> &EntityStack, std::unordered_set<std::string> &ParameterStack)
{
   if (Value.empty()) return;

   std::string output;
   output.reserve(Value.size() * 2); // Pre-allocate larger buffer to reduce reallocations

   std::string_view view(Value);

   while (not view.empty()) {
      auto ch = view.front();

      if ((ch IS '%' or ch IS '&') and view.size() > 1) {
         bool is_parameter = (ch IS '%');
         view.remove_prefix(1); // Skip the % or &

         // Find the name end using string_view operations
         auto name_start = view;
         size_t name_length = 0;
         while (name_length < view.size() and is_name_char(view[name_length])) {
            name_length++;
         }

         if (name_length > 0 and
             name_length < view.size() and
             view[name_length] IS ';') {

            auto name_view = view.substr(0, name_length);
            std::string resolved;

            // Only create string if resolution succeeds to avoid unnecessary allocation
            if (resolve_entity_internal(Self, std::string(name_view), resolved,
                                      is_parameter, EntityStack, ParameterStack) IS ERR::Okay) {
               output += resolved;
            }
            else {
               // Reconstruct the original entity reference
               output.push_back(is_parameter ? '%' : '&');
               output.append(name_view);
               output.push_back(';');
            }

            view.remove_prefix(name_length + 1); // Skip name + ';'
            continue;
         }
         else {
            // Not a valid entity reference, backtrack
            output.push_back(is_parameter ? '%' : '&');
         }
      }
      else {
         output.push_back(ch);
         view.remove_prefix(1);
      }
   }

   Value.swap(output);
}

ERR extXML::resolveEntity(const std::string &Name, std::string &Value, bool Parameter)
{
   std::unordered_set<std::string> entity_stack;
   std::unordered_set<std::string> parameter_stack;
   return resolve_entity_internal(this, Name, Value, Parameter, entity_stack, parameter_stack);
}

static ERR resolve_entity_internal(extXML *Self, const std::string &Name, std::string &Value,
   bool Parameter, std::unordered_set<std::string> &EntityStack, std::unordered_set<std::string> &ParameterStack)
{
   pf::Log log(__FUNCTION__);

   auto &stack = Parameter ? ParameterStack : EntityStack;
   if (stack.contains(Name)) return log.warning(ERR::Loop);

   auto &table = Parameter ? Self->ParameterEntities : Self->Entities;
   auto it = table.find(Name);
   if (it IS table.end()) return ERR::Search;

   stack.insert(Name);

   Value = it->second;
   expand_entity_references(Self, Value, EntityStack, ParameterStack);

   stack.erase(Name);
   return ERR::Okay;
}

static bool read_quoted(extXML *Self, const char *&ptr, std::string &Result,
   std::unordered_set<std::string> &EntityStack, std::unordered_set<std::string> &ParameterStack)
{
   if ((not ptr) or (not *ptr)) return false;

   auto quote = *ptr;
   if ((quote != '"') and (quote != '\'')) return false;

   ptr++;
   std::string_view view(ptr);
   std::string buffer;
   buffer.reserve(view.size()); // Pre-allocate based on remaining view size

   while (not view.empty()) {
      auto ch = view.front();
      if (ch IS quote) {
         ptr = view.data() + 1;
         Result.swap(buffer);
         return true;
      }

      if ((ch IS '%' or ch IS '&') and view.size() > 1) {
         bool is_parameter = (ch IS '%');
         view.remove_prefix(1); // Skip the % or &

         // Find entity name using string_view
         size_t name_length = 0;
         while (name_length < view.size() and is_name_char(view[name_length])) {
            name_length++;
         }

         if (name_length > 0 and
             name_length < view.size() and
             view[name_length] IS ';') {

            auto name_view = view.substr(0, name_length);
            std::string resolved;

            // Only create string for entity resolution
            if (resolve_entity_internal(Self, std::string(name_view), resolved,
                                      is_parameter, EntityStack, ParameterStack) IS ERR::Okay) {
               buffer += resolved;
            }
            else {
               buffer.push_back(is_parameter ? '%' : '&');
               buffer.append(name_view);
               buffer.push_back(';');
            }

            view.remove_prefix(name_length + 1); // Skip name + ';'
            continue;
         }
         else {
            // Not a valid entity, backtrack
            buffer.push_back(is_parameter ? '%' : '&');
         }
      }
      else {
         buffer.push_back(ch);
         view.remove_prefix(1);
      }
   }

   ptr = view.data();
   return false;
}

static void parse_doctype(extXML *Self, CSTRING Input)
{
   if (not Input) return;

   const char *str = Input;

   while ((*str) and (((*str >= 'A') and (*str <= 'Z')) or ((*str >= 'a') and (*str <= 'z')))) str++;
   skip_ws(str);

   std::string document_type;
   if (not read_name(str, document_type)) return;

   assign_string(Self->DocType, document_type);
   if (Self->PublicID) { FreeResource(Self->PublicID); Self->PublicID = nullptr; }
   if (Self->SystemID) { FreeResource(Self->SystemID); Self->SystemID = nullptr; }
   Self->Entities.clear();
   Self->ParameterEntities.clear();
   Self->Notations.clear();

   skip_ws(str);

   std::unordered_set<std::string> entity_stack;
   std::unordered_set<std::string> parameter_stack;

   if (ci_keyword(str, "PUBLIC")) {
      skip_ws(str);
      std::string public_id;
      if (read_quoted(Self, str, public_id, entity_stack, parameter_stack)) assign_string(Self->PublicID, public_id);
      skip_ws(str);
      std::string system_id;
      if (read_quoted(Self, str, system_id, entity_stack, parameter_stack)) assign_string(Self->SystemID, system_id);
   }
   else if (ci_keyword(str, "SYSTEM")) {
      skip_ws(str);
      std::string system_id;
      if (read_quoted(Self, str, system_id, entity_stack, parameter_stack)) assign_string(Self->SystemID, system_id);
   }

   skip_ws(str);

   if (*str IS '[') {
      str++;

      while (*str) {
         skip_ws(str);
         if (not *str) break;
         if (*str IS ']') { str++; break; }

         if ((*str IS '<') and (str[1] IS '!')) {
            str += 2;

            if (ci_keyword(str, "ENTITY")) {
               skip_ws(str);
               bool parameter = false;
               if (*str IS '%') { parameter = true; str++; skip_ws(str); }

               std::string name;
               if (not read_name(str, name)) {
                  std::string_view remainder(str ? str : "");
                  auto close = remainder.find('>');
                  if (close IS std::string_view::npos) str += remainder.size();
                  else str += close + 1;
                  continue;
               }

               skip_ws(str);

               if (ci_keyword(str, "SYSTEM")) {
                  skip_ws(str);
                  std::string system;
                  if (read_quoted(Self, str, system, entity_stack, parameter_stack)) {
                     if (parameter) Self->ParameterEntities[name] = system;
                     else Self->Entities[name] = system;
                  }
               }
               else {
                  std::string value;
                  if ((*str IS '"') or (*str IS '\'')) {
                     if (read_quoted(Self, str, value, entity_stack, parameter_stack)) {
                        if (parameter) Self->ParameterEntities[name] = value;
                        else Self->Entities[name] = value;
                     }
                  }
               }

               std::string_view remainder(str ? str : "");
               auto close = remainder.find('>');
               if (close IS std::string_view::npos) str += remainder.size();
               else str += close + 1;
            }
            else if (ci_keyword(str, "NOTATION")) {
               skip_ws(str);
               std::string name;
               if (not read_name(str, name)) {
                  std::string_view remainder(str ? str : "");
                  auto close = remainder.find('>');
                  if (close IS std::string_view::npos) str += remainder.size();
                  else str += close + 1;
                  continue;
               }
               skip_ws(str);

               std::string notation_value;
               if (ci_keyword(str, "PUBLIC")) {
                  skip_ws(str);
                  std::string public_id;
                  std::string system_id;
                  if (read_quoted(Self, str, public_id, entity_stack, parameter_stack)) {
                     skip_ws(str);
                     if (read_quoted(Self, str, system_id, entity_stack, parameter_stack)) {
                        notation_value = public_id + " " + system_id;
                     }
                     else notation_value = public_id;
                  }
               }
               else if (ci_keyword(str, "SYSTEM")) {
                  skip_ws(str);
                  read_quoted(Self, str, notation_value, entity_stack, parameter_stack);
               }

               if (not notation_value.empty()) Self->Notations[name] = notation_value;

               std::string_view remainder(str ? str : "");
               auto close = remainder.find('>');
               if (close IS std::string_view::npos) str += remainder.size();
               else str += close + 1;
            }
            else {
               std::string_view remainder(str ? str : "");
               auto close = remainder.find('>');
               if (close IS std::string_view::npos) str += remainder.size();
               else str += close + 1;
            }
         }
         else str++;
      }
   }
}

//********************************************************************************************************************
// Extract content and add it to the end of Tags

static ERR extract_content(extXML *Self, TAGS &Tags, ParseState &State)
{
   pf::Log log(__FUNCTION__);

   // Skip whitespace - this will tell us if there is content or not.  If we do find some content, reset the marker to
   // the start of the content area because leading spaces may be important for content processing (e.g. for <pre> tags)

   CSTRING str = State.Pos;
   if ((Self->Flags & XMF::INCLUDE_WHITESPACE) IS XMF::NIL) {
      while ((*str) and (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
      if (*str != '<') str = State.Pos;
   }

   // If the STRIP_CONTENT flag is set, skip over the content and return a NODATA error code.

   if ((Self->Flags & XMF::STRIP_CONTENT) != XMF::NIL) {
      while ((*str) and (*str != '<')) { if (*str IS '\n') Self->LineNo++; str++; }
      State.Pos = str;
      return ERR::NoData;
   }

   for (int i=0; (str[i]) and (str[i] != '<'); i++) {
      if (str[i] IS '\r') continue;
      // Content detected

      std::string content;
      auto start = str;
      while ((*str) and (*str != '<')) {
         if (*str IS '\n') Self->LineNo++;
         str++;
      }

      // Copy content, skipping \r characters
      content.reserve(str - start);
      for (auto p = start; p != str; ++p) {
         if (*p != '\r') content.push_back(*p);
      }

      Tags.emplace_back(XMLTag(glTagID++, 0, { { "", std::move(content) } }));
      State.Pos = str;
      return ERR::Okay;
   }

   State.Pos = str;
   return ERR::NoData;
}

//********************************************************************************************************************
// Called by txt_to_xml() to extract the next tag from an XML string.  This function also recurses into itself.

static ERR parse_tag(extXML *Self, TAGS &Tags, ParseState &State)
{
   enum { RAW_NONE=0, RAW_CDATA, RAW_NDATA };

   pf::Log log(__FUNCTION__);

   log.traceBranch("%.30s", State.Pos);

   // Save current namespace context to restore later (for proper scoping)
   auto saved_prefix_map = State.PrefixMap;
   auto saved_default_namespace = State.DefaultNamespace;

   // Use Defer to ensure namespace context is always restored when function exits
   auto restore_namespace = pf::Defer([&]() {
      State.PrefixMap = saved_prefix_map;
      State.DefaultNamespace = saved_default_namespace;
   });

   if (State.Pos[0] != '<') {
      log.warning("Malformed XML statement detected.");
      return ERR::InvalidData;
   }

   CSTRING str = State.Pos + 1;

   auto line_no = Self->LineNo;

   if ((str[0] IS '!') and (str[1] IS '-') and (str[2] IS '-')) {
      if ((Self->Flags & XMF::INCLUDE_COMMENTS) IS XMF::NIL) {
         if (auto i = pf::strsearch("-->", str); i != -1) {
            State.Pos = str + i + 3;
            return ERR::NothingDone;
         }
         else {
            log.warning("Detected malformed comment (missing --> terminator).");
            return ERR::InvalidData;
         }
      }

      str += 3;
      auto comment_start = str;
      while ((str[0]) and !((str[0] IS '-') and (str[1] IS '-') and (str[2] IS '>'))) {
         if (str[0] IS '\n') Self->LineNo++;
         str++;
      }

      if (not str[0]) {
         log.warning("Detected malformed comment (missing --> terminator).");
         return ERR::InvalidData;
      }

      std::string comment_text(comment_start, size_t(str - comment_start));
      auto &comment_tag = Tags.emplace_back(XMLTag(glTagID++, line_no, { { "", comment_text } }));
      comment_tag.Flags |= XTF::COMMENT;
      State.Pos = str + 3;
      return ERR::Okay;
   }

   line_no = Self->LineNo;
   int8_t raw_content;

   if (not strncmp("![CDATA[", str, 8)) { raw_content = RAW_CDATA; str += 8; }
   else if (not strncmp("![NDATA[", str, 8)) { raw_content = RAW_NDATA; str += 8; }
   else raw_content = RAW_NONE;

   if (raw_content) {
      int len;

      // CDATA handler

      if (raw_content IS RAW_CDATA) {
         for (len=0; str[len]; len++) {
            if ((str[len] IS ']') and (str[len+1] IS ']') and (str[len+2] IS '>')) break;
            else if (str[len] IS '\n') Self->LineNo++;
         }
      }
      else if (raw_content IS RAW_NDATA) {
         uint16_t nest = 1;
         for (len=0; str[len]; len++) {
            if ((str[len] IS '<') and (str[len+1] IS '!') and (str[len+2] IS '[') and
                ((str[len+3] IS 'N') or (str[len+3] IS 'C')) and (str[len+4] IS 'D') and (str[len+5] IS 'A') and (str[len+6] IS 'T') and (str[len+7] IS 'A')  and (str[len+8] IS '[')) {
               nest++;
               len += 7;
            }
            else if ((str[len] IS ']') and (str[len+1] IS ']') and (str[len+2] IS '>')) {
               nest--;
               if (not nest) break;
            }
            else if (str[len] IS '\n') Self->LineNo++;
         }
      }

      // CDATA counts as content and therefore can be stripped out

      if (((Self->Flags & XMF::STRIP_CONTENT) != XMF::NIL) or (not len)) {
         State.Pos = str + len + 3;
         return ERR::NothingDone;
      }

      if (not str[len]) {
         log.warning("Malformed XML:  A CDATA section is missing its closing string.");
         return ERR::InvalidData;
      }

      // CDATA sections are assimilated into the parent tag as content

      auto &cdata_tag = Tags.emplace_back(XMLTag(glTagID++, line_no, {
         { "", std::string(str, len) }
      }));
      cdata_tag.Flags |= XTF::CDATA;
      State.Pos = str + len + 3;
      return ERR::Okay;
   }

   if ((State.Pos[1] IS '?') or (State.Pos[1] IS '!')) {
      if ((Self->Flags & XMF::PARSE_ENTITY) != XMF::NIL) {
         if (pf::startswith("!DOCTYPE", State.Pos+1)) parse_doctype(Self, State.Pos+7);
      }

      if ((Self->Flags & XMF::STRIP_HEADERS) != XMF::NIL) {
         if (*str IS '>') str++;
         State.Pos = str;
         return ERR::NothingDone;
      }
   }

   State.Balance++;

   auto &tag = Tags.emplace_back(XMLTag(glTagID++, line_no));

   // Extract all attributes within the tag

   str = State.Pos+1;
   if (*str IS '?') tag.Flags |= XTF::INSTRUCTION; // Detect <?xml ?> style instruction elements.
   else if ((*str IS '!') and (str[1] >= 'A') and (str[1] <= 'Z')) tag.Flags |= XTF::NOTATION;

   while ((*str) and (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
   while ((*str) and (*str != '>')) {
      if ((str[0] IS '/') and (str[1] IS '>')) break; // Termination checks
      if ((str[0] IS '?') and (str[1] IS '>')) break;

      if (*str IS '=') return log.warning(ERR::InvalidData);

      // Extract the name of the attribute

      std::string_view name;

      if (*str IS '"'); // Quoted notation attributes are parsed as content values
      else {
         int s = 0;
         while ((str[s] > 0x20) and (str[s] != '>') and (str[s] != '=')) {
            if ((str[s] IS '/') and (str[s+1] IS '>')) break;
            if ((str[s] IS '?') and (str[s+1] IS '>')) break;
            s++;
         }
         name = std::string_view(str, s);
         str += s;
      }

      // Extract the attributes value

      while ((*str) and (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }

      if (*str IS '=') {
         str++;
         while ((*str) and (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }

         std::string val;
         if (*str IS '"') {
            str++;
            auto start = str;
            while ((*str) and (*str != '"')) {
               if (*str IS '\n') Self->LineNo++;
               str++;
            }
            val.assign(start, str);
            if (*str IS '"') str++;
         }
         else if (*str IS '\'') {
            str++;
            auto start = str;
            while ((*str) and (*str != '\'')) {
               if (*str IS '\n') Self->LineNo++;
               str++;
            }
            val.assign(start, str);
            if (*str IS '\'') str++;
         }
         else {
            auto start = str;
            while ((*str > 0x20) and (*str != '>')) {
               if ((str[0] IS '/') and (str[1] IS '>')) break;
               str++;
            }
            val.assign(start, str);
         }

         tag.Attribs.emplace_back(std::string(name), val);

         // Detect and process namespace declarations
         if ((Self->Flags & XMF::NAMESPACE_AWARE) != XMF::NIL) {
            if (name.starts_with("xmlns")) {
               auto ns_hash = Self->registerNamespace(val);

               if (name IS "xmlns") { // Default namespace declaration
                  State.DefaultNamespace = ns_hash;
               }
               else if ((name.size() > 6) and (name[5] IS ':')) {
                  // Prefixed namespace declaration: xmlns:prefix="uri"
                  std::string prefix(name.substr(6));
                  Self->Prefixes[prefix] = ns_hash; // Permanent global record of this prefix
                  State.PrefixMap[prefix] = ns_hash;
               }
            }
         }
      }
      else if ((name.empty()) and (*str IS '"')) { // Detect notation value with no name
         str++;
         int s;
         for (s=0; (str[s]) and (str[s] != '"'); s++) {
            if (str[s] IS '\n') Self->LineNo++;
         }
         tag.Attribs.emplace_back(std::string(name), std::string(str, s));
         str += s;
         if (*str IS '"') str++;
      }
      else tag.Attribs.emplace_back(std::string(name), ""); // Either the tag name or an attribute with no value.

      while ((*str) and (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
   }

   if (tag.Attribs.empty()) {
      log.warning("No attributes parsed for tag at line %d", Self->LineNo);
      return ERR::Syntax;
   }

   // Resolve prefixed tag names to namespace IDs

   if ((Self->Flags & XMF::NAMESPACE_AWARE) != XMF::NIL) {
      if (not tag.Attribs[0].Name.empty()) {
         auto &tag_name = tag.Attribs[0].Name;
         if (auto colon = tag_name.find(':'); colon != std::string::npos) {
            std::string prefix = tag_name.substr(0, colon);
            if (auto it = State.PrefixMap.find(prefix); it != State.PrefixMap.end()) {
               tag.NamespaceID = it->second;
            }
         }
         else if (State.DefaultNamespace) { // Apply default namespace if no prefix
            tag.NamespaceID = State.DefaultNamespace;
         }
      }
   }

   State.Pos = str;

   if ((*State.Pos IS '>') and (not tag.Attribs.empty()) and
       (tag.Attribs[0].Name[0] != '!') and (tag.Attribs[0].Name[0] != '?')) {
      // We reached the end of an open tag.  Extract the content within it and handle any child tags.

      State.Pos++;
      ERR error = extract_content(Self, Tags.back().Children, State);

      if ((error != ERR::Okay) and (error != ERR::NoData)) return error;

      while ((State.Pos[0] IS '<') and (State.Pos[1] != '/')) {
         error = parse_tag(Self, Tags.back().Children, State);

         if (error IS ERR::NothingDone) {
            // Extract any additional content trapped between tags

            error = extract_content(Self, Tags.back().Children, State);

            if ((error != ERR::Okay) and (error != ERR::NoData)) return error;
         }
         else if (error IS ERR::Okay) {
            // Extract any new content caught in-between tags

            error = extract_content(Self, Tags.back().Children, State);

            if ((error != ERR::Okay) and (error != ERR::NoData)) return error;
         }
         else return ERR::Failed;
      }

      // There should be a closing tag - skip past it

      if ((State.Pos[0] IS '<') and (State.Pos[1] IS '/')) {
         State.Balance--;
         while ((*State.Pos) and (*State.Pos != '>')) { if (*State.Pos IS '\n') Self->LineNo++; State.Pos++; }
      }

      if (*State.Pos IS '>') State.Pos++;
   }
   else {
      if ((State.Pos[0] IS '/') and (State.Pos[1] IS '>')) State.Pos += 2;
      State.Balance--;
   }

   return ERR::Okay;
}

//********************************************************************************************************************
// Parse a text string into XML tags.

static ERR txt_to_xml(extXML *Self, TAGS &Tags, std::string_view Text)
{
   pf::Log log(__FUNCTION__);

   if (&Tags IS &Self->Tags) {
      if (Self->DocType)  { FreeResource(Self->DocType); Self->DocType = nullptr; }
      if (Self->PublicID) { FreeResource(Self->PublicID); Self->PublicID = nullptr; }
      if (Self->SystemID) { FreeResource(Self->SystemID); Self->SystemID = nullptr; }
      Self->Entities.clear();
      Self->ParameterEntities.clear();
      Self->Notations.clear();
   }

   // Extract the tag information.  This loop will extract the top-level tags.  The parse_tag() function is recursive
   // to extract the child tags.

   log.trace("Extracting tag information with parse_tag()");

   ParseState state;
   CSTRING str;
   for (str=Text.data(); (*str) and (*str != '<'); str++) {
      if (*str IS '\n') Self->LineNo++;
   }
   if (not str[0]) {
      Self->ParseError = log.warning(ERR::InvalidData);
      return Self->ParseError;
   }
   state.Pos = str;
   while ((state.Pos[0] IS '<') and (state.Pos[1] != '/')) {
      ERR error = parse_tag(Self, Tags, state);

      if ((error != ERR::Okay) and (error != ERR::NothingDone)) {
         log.warning("XML parsing aborted.");
         return ERR::InvalidData;
      }

      // Skip content/whitespace to get to the next tag
      str = state.Pos;
      while ((*str) and (*str != '<')) { if (*str IS '\n') Self->LineNo++; str++; }
      state.Pos = str;

      if (error IS ERR::NothingDone) continue;
   }

   // If the WELL_FORMED flag has been used, check that the tags balance.  If they don't then return ERR::InvalidData.

   if ((Self->Flags & XMF::WELL_FORMED) != XMF::NIL) {
      if (state.Balance != 0) return log.warning(ERR::UnbalancedXML);
   }

   if ((Self->Flags & XMF::NO_ESCAPE) IS XMF::NIL) {
      log.trace("Unescaping XML.");
      unescape_all(Self, Tags);
   }

   Self->modified();

   log.trace("XML parsing complete.");
   return ERR::Okay;
}

//********************************************************************************************************************
// Serialise XML data into string form.

static void serialise_xml(XMLTag &Tag, std::ostringstream &Buffer, XMF Flags)
{
   if (Tag.Attribs[0].isContent()) {
      if (not Tag.Attribs[0].Value.empty()) {
         if ((Tag.Flags & XTF::CDATA) != XTF::NIL) {
            if ((Flags & XMF::STRIP_CDATA) IS XMF::NIL) Buffer << "<![CDATA[";
            Buffer << Tag.Attribs[0].Value;
            if ((Flags & XMF::STRIP_CDATA) IS XMF::NIL) Buffer << "]]>";
         }
         else {
            auto &str = Tag.Attribs[0].Value;
            for (auto j=0; str[j]; j++) {
               switch (str[j]) {
                  case '&': Buffer << "&amp;"; break;
                  case '<': Buffer << "&lt;"; break;
                  case '>': Buffer << "&gt;"; break;
                  default:  Buffer << str[j]; break;
               }
            }
         }
      }
   }
   else if ((Flags & XMF::OMIT_TAGS) != XMF::NIL) {
      if (not Tag.Children.empty()) {
         for (auto &child : Tag.Children) {
            serialise_xml(child, Buffer, Flags);
         }
         if ((Flags & XMF::READABLE) != XMF::NIL) Buffer << '\n';
      }
   }
   else {
      Buffer << '<';

      bool insert_space = false;
      for (auto &scan : Tag.Attribs) {
         if (insert_space) Buffer << ' ';
         if (not scan.Name.empty()) output_attribvalue(scan.Name, Buffer);

         if (not scan.Value.empty()) {
            if (not scan.Name.empty()) Buffer << '=';
            Buffer << '"';
            output_attribvalue(scan.Value, Buffer);
            Buffer << '"';
         }
         insert_space = true;
      }

      if ((Tag.Flags & XTF::INSTRUCTION) != XTF::NIL) {
         Buffer << "?>";
         if ((Flags & XMF::READABLE) != XMF::NIL) Buffer << '\n';
      }
      else if ((Tag.Flags & XTF::NOTATION) != XTF::NIL) {
         Buffer << '>';
         if ((Flags & XMF::READABLE) != XMF::NIL) Buffer << '\n';
      }
      else if (not Tag.Children.empty()) {
         Buffer << '>';
         if (not Tag.Children[0].Attribs[0].isContent()) Buffer << '\n';

         for (auto &child : Tag.Children) {
            serialise_xml(child, Buffer, Flags);
         }

         Buffer << "</";
         output_attribvalue(Tag.Attribs[0].Name, Buffer);
         Buffer << '>';
         if ((Flags & XMF::READABLE) != XMF::NIL) Buffer << '\n';
      }
      else {
         Buffer << "/>";
         if ((Flags & XMF::READABLE) != XMF::NIL) Buffer << '\n';
      }
   }
}

//********************************************************************************************************************

static ERR parse_source(extXML *Self)
{
   pf::Log log(__FUNCTION__);
   CacheFile *filecache;

   log.traceBranch();

   Self->Tags.clear();
   Self->LineNo = 1;

   // Although the file will be uncached as soon it is loaded, the developer can pre-cache XML files with his own
   // call to LoadFile(), which can lead our use of LoadFile() to being quite effective.

   if (Self->Source) {
      constexpr size_t initial_size = 64 * 1024;
      constexpr size_t growth_threshold = 1024;

      std::string buffer;
      buffer.reserve(initial_size);
      Self->ParseError = ERR::Okay;
      acSeekStart(Self->Source, 0);

      while (true) {
         // Ensure we have space for the next read
         size_t current_size = buffer.size();
         if (buffer.capacity() - current_size < growth_threshold) {
            buffer.reserve(buffer.capacity() * 2);
         }

         // Read directly into the buffer
         buffer.resize(current_size + growth_threshold);
         int result;
         if (acRead(Self->Source, buffer.data() + current_size, growth_threshold, &result) != ERR::Okay) {
            Self->ParseError = ERR::Read;
            break;
         }
         else if (result <= 0) {
            buffer.resize(current_size); // Trim unused space
            break;
         }

         buffer.resize(current_size + result); // Adjust to actual read size
      }

      if (Self->ParseError IS ERR::Okay) {
         Self->ParseError = txt_to_xml(Self, Self->Tags, std::string_view(buffer));
      }
   }
   else if (LoadFile(Self->Path, LDF::NIL, &filecache) IS ERR::Okay) {
      Self->ParseError = txt_to_xml(Self, Self->Tags, std::string_view((char *)filecache->Data, filecache->Size));
      UnloadFile(filecache);
   }
   else Self->ParseError = ERR::File;

   return Self->ParseError;
}
