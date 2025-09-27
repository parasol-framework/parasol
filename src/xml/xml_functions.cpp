
//********************************************************************************************************************
// Output an XML string with escape characters.

template <class T> void output_attribvalue(T &&String, std::ostringstream &Output)
{
   CSTRING str = to_cstring(String);
   for (auto j=0; str[j]; j++) {
      switch (str[j]) {
         case '&':  Output << "&amp;"; break;
         case '<':  Output << "&lt;"; break;
         case '>':  Output << "&gt;"; break;
         case '"':  Output << "&quot;"; break;
         default:   Output << str[j]; break;
      }
   }
}

//********************************************************************************************************************
// TODO: Support processing of ENTITY declarations in the doctype.
//ENTITY       <!ENTITY % textgizmo "fontgizmo">
//INSTRUCTION  <?XML version="1.0" standalone="yes" ?>
//NOTATION     <!NOTATION gif SYSTEM "viewer.exe">

static bool is_name_char(char ch)
{
   if ((ch >= 'A') and (ch <= 'Z')) return true;
   if ((ch >= 'a') and (ch <= 'z')) return true;
   if ((ch >= '0') and (ch <= '9')) return true;
   if ((ch IS '.') or (ch IS '-') or (ch IS '_') or (ch IS ':')) return true;
   return false;
}

static void assign_string(STRING &Target, const std::string &Value)
{
   if (Target) { FreeResource(Target); Target = nullptr; }
   if (!Value.empty()) Target = pf::strclone(Value);
}

static void clear_string(STRING &Target)
{
   if (Target) { FreeResource(Target); Target = nullptr; }
}

static bool is_name_start(char ch)
{
   if ((ch >= 'A') and (ch <= 'Z')) return true;
   if ((ch >= 'a') and (ch <= 'z')) return true;
   if ((ch IS '_') or (ch IS ':')) return true;
   return false;
}

static void skip_ws(const char *&ptr)
{
   while ((ptr) and (*ptr) and (*ptr <= 0x20)) ptr++;
}

static bool ci_keyword(const char *&ptr, CSTRING Keyword)
{
   auto p = ptr;
   auto k = Keyword;

   while ((*p) and (*k)) {
      auto ch = *p;
      if ((ch >= 'A') and (ch <= 'Z')) ch += 0x20;
      auto cmp = *k;
      if ((cmp >= 'A') and (cmp <= 'Z')) cmp += 0x20;
      if (ch != cmp) return false;
      p++;
      k++;
   }

   if (*k) return false;
   if ((is_name_char(*p)) and (*p != '[')) return false;

   ptr = p;
   return true;
}

static bool read_name(const char *&ptr, std::string &Result)
{
   if (!is_name_start(*ptr)) return false;

   auto start = ptr;
   ptr++;
   while (is_name_char(*ptr)) ptr++;

   Result.assign(start, ptr - start);
   return true;
}

static ERR resolve_entity_internal(extXML *Self, const std::string &Name, std::string &Value,
   bool Parameter, std::unordered_set<std::string> &EntityStack, std::unordered_set<std::string> &ParameterStack);

static void expand_entity_references(extXML *Self, std::string &Value,
   std::unordered_set<std::string> &EntityStack, std::unordered_set<std::string> &ParameterStack)
{
   if (Value.empty()) return;

   std::string output;
   output.reserve(Value.size());

   for (size_t i = 0; i < Value.size(); ) {
      auto ch = Value[i];
      if ((ch IS '%') and (i + 1 < Value.size())) {
         auto start = i + 1;
         auto end = start;
         while ((end < Value.size()) and is_name_char(Value[end])) end++;
         if ((end < Value.size()) and (Value[end] IS ';')) {
            std::string name(Value.begin() + start, Value.begin() + end);
            std::string resolved;
            if (resolve_entity_internal(Self, name, resolved, true, EntityStack, ParameterStack) IS ERR::Okay) {
               output += resolved;
            }
            else {
               output.append("%");
               output += name;
               output.append(";");
            }
            i = end + 1;
            continue;
         }
      }
      else if ((ch IS '&') and (i + 1 < Value.size())) {
         auto start = i + 1;
         auto end = start;
         while ((end < Value.size()) and is_name_char(Value[end])) end++;
         if ((end < Value.size()) and (Value[end] IS ';')) {
            std::string name(Value.begin() + start, Value.begin() + end);
            std::string resolved;
            if (resolve_entity_internal(Self, name, resolved, false, EntityStack, ParameterStack) IS ERR::Okay) {
               output += resolved;
            }
            else {
               output.append("&");
               output += name;
               output.append(";");
            }
            i = end + 1;
            continue;
         }
      }

      output.push_back(ch);
      i++;
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
   if ((!ptr) or (!*ptr)) return false;

   auto quote = *ptr;
   if ((quote != '"') and (quote != '\'')) return false;

   ptr++;
   std::string buffer;

   while ((*ptr) and (*ptr != quote)) {
      if (*ptr IS '%') {
         ptr++;
         const char *start = ptr;
         while (*ptr and is_name_char(*ptr)) ptr++;
         if (*ptr IS ';') {
            std::string name(start, ptr - start);
            std::string resolved;
            if (resolve_entity_internal(Self, name, resolved, true, EntityStack, ParameterStack) IS ERR::Okay) {
               buffer += resolved;
            }
            else {
               buffer.append("%");
               buffer += name;
               buffer.append(";");
            }
            ptr++;
            continue;
         }
      }
      else if (*ptr IS '&') {
         ptr++;
         const char *start = ptr;
         while (*ptr and is_name_char(*ptr)) ptr++;
         if (*ptr IS ';') {
            std::string name(start, ptr - start);
            std::string resolved;
            if (resolve_entity_internal(Self, name, resolved, false, EntityStack, ParameterStack) IS ERR::Okay) {
               buffer += resolved;
            }
            else {
               buffer.append("&");
               buffer += name;
               buffer.append(";");
            }
            ptr++;
            continue;
         }
      }

      buffer.push_back(*ptr);
      ptr++;
   }

   if (*ptr != quote) return false;
   ptr++;
   Result.swap(buffer);
   return true;
}

static void parse_doctype(extXML *Self, CSTRING Input)
{
   if (!Input) return;

   const char *str = Input;

   while ((*str) and (((*str >= 'A') and (*str <= 'Z')) or ((*str >= 'a') and (*str <= 'z')))) str++;
   skip_ws(str);

   std::string document_type;
   if (!read_name(str, document_type)) return;

   assign_string(Self->DocumentType, document_type);
   clear_string(Self->PublicID);
   clear_string(Self->SystemID);
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
      else clear_string(Self->PublicID);
      skip_ws(str);
      std::string system_id;
      if (read_quoted(Self, str, system_id, entity_stack, parameter_stack)) assign_string(Self->SystemID, system_id);
      else clear_string(Self->SystemID);
   }
   else if (ci_keyword(str, "SYSTEM")) {
      skip_ws(str);
      std::string system_id;
      if (read_quoted(Self, str, system_id, entity_stack, parameter_stack)) assign_string(Self->SystemID, system_id);
      else clear_string(Self->SystemID);
   }

   skip_ws(str);

   if (*str IS '[') {
      str++;

      while (*str) {
         skip_ws(str);
         if (!*str) break;
         if (*str IS ']') { str++; break; }

         if ((*str IS '<') and (str[1] IS '!')) {
            str += 2;

            if (ci_keyword(str, "ENTITY")) {
               skip_ws(str);
               bool parameter = false;
               if (*str IS '%') { parameter = true; str++; skip_ws(str); }

               std::string name;
               if (!read_name(str, name)) {
                  while ((*str) and (*str != '>')) str++;
                  if (*str IS '>') str++;
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

               while ((*str) and (*str != '>')) str++;
               if (*str IS '>') str++;
            }
            else if (ci_keyword(str, "NOTATION")) {
               skip_ws(str);
               std::string name;
               if (!read_name(str, name)) {
                  while ((*str) and (*str != '>')) str++;
                  if (*str IS '>') str++;
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

               if (!notation_value.empty()) Self->Notations[name] = notation_value;

               while ((*str) and (*str != '>')) str++;
               if (*str IS '>') str++;
            }
            else {
               while ((*str) and (*str != '>')) str++;
               if (*str IS '>') str++;
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

      std::ostringstream buffer;
      while ((*str) and (*str != '<')) {
         if (*str IS '\n') Self->LineNo++;
         if (*str != '\r') buffer << *str++;
         else str++;
      }
      Tags.emplace_back(XMLTag(glTagID++, 0, { { "", buffer.str() } }));
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

      if (!str[0]) {
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

   if (!strncmp("![CDATA[", str, 8)) { raw_content = RAW_CDATA; str += 8; }
   else if (!strncmp("![NDATA[", str, 8)) { raw_content = RAW_NDATA; str += 8; }
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
               if (!nest) break;
            }
            else if (str[len] IS '\n') Self->LineNo++;
         }
      }

      // CDATA counts as content and therefore can be stripped out

      if (((Self->Flags & XMF::STRIP_CONTENT) != XMF::NIL) or (!len)) {
         State.Pos = str + len + 3;
         return ERR::NothingDone;
      }

      if (!str[len]) {
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
         std::ostringstream attrib_value;
         if (*str IS '"') {
            str++;
            while ((*str) and (*str != '"')) { if (*str IS '\n') Self->LineNo++; attrib_value << *str++; }
            if (*str IS '"') str++;
         }
         else if (*str IS '\'') {
            str++;
            while ((*str) and (*str != '\'')) { if (*str IS '\n') Self->LineNo++; attrib_value << *str++; }
            if (*str IS '\'') str++;
         }
         else {
            while ((*str > 0x20) and (*str != '>')) {
               if ((str[0] IS '/') and (str[1] IS '>')) break;
               attrib_value << *str++;
            }
         }

         auto val = attrib_value.str();

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
      if (!tag.Attribs[0].Name.empty()) {
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

   if ((*State.Pos IS '>') and (!tag.Attribs.empty()) and
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
// Convert a text string into XML tags.

static ERR txt_to_xml(extXML *Self, TAGS &Tags, CSTRING Text)
{
   pf::Log log(__FUNCTION__);

   if (&Tags IS &Self->Tags) {
      clear_string(Self->DocumentType);
      clear_string(Self->PublicID);
      clear_string(Self->SystemID);
      Self->Entities.clear();
      Self->ParameterEntities.clear();
      Self->Notations.clear();
   }

   // Extract the tag information.  This loop will extract the top-level tags.  The parse_tag() function is recursive
   // to extract the child tags.

   log.trace("Extracting tag information with parse_tag()");

   ParseState state;
   CSTRING str;
   for (str=Text; (*str) and (*str != '<'); str++) if (*str IS '\n') Self->LineNo++;
   if (!str[0]) {
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
      if (!Tag.Attribs[0].Value.empty()) {
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
      if (!Tag.Children.empty()) {
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
         if (!scan.Name.empty()) output_attribvalue(scan.Name, Buffer);

         if (!scan.Value.empty()) {
            if (!scan.Name.empty()) Buffer << '=';
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
      else if (!Tag.Children.empty()) {
         Buffer << '>';
         if (!Tag.Children[0].Attribs[0].isContent()) Buffer << '\n';

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
      std::string buffer;
      buffer.resize(64 * 1024);
      Self->ParseError = ERR::Okay;
      acSeekStart(Self->Source, 0);
      int pos = 0;
      while (true) {
         int result;
         if (acRead(Self->Source, buffer.data() + pos, std::ssize(buffer) - pos, &result) != ERR::Okay) {
            Self->ParseError = ERR::Read;
            break;
         }
         else if (result <= 0) break;

         pos += result;
         if (pos >= std::ssize(buffer) - 1024) buffer.resize(std::ssize(buffer) * 2);
      }

      if (Self->ParseError IS ERR::Okay) {
         Self->ParseError = txt_to_xml(Self, Self->Tags, buffer.c_str());
      }
   }
   else if (LoadFile(Self->Path, LDF::NIL, &filecache) IS ERR::Okay) {
      Self->ParseError = txt_to_xml(Self, Self->Tags, (CSTRING)filecache->Data);
      UnloadFile(filecache);
   }
   else Self->ParseError = ERR::File;

   return Self->ParseError;
}

//********************************************************************************************************************
// Extracts immediate content, does not recurse into child tags.

static ERR get_content(extXML *Self, XMLTag &Tag, STRING Buffer, int Size)
{
   Buffer[0] = 0;
   if (!Tag.Children.empty()) {
      int j = 0;
      for (auto &scan : Tag.Children) {
         if (scan.Attribs.empty()) continue; // Sanity check (there should always be at least 1 attribute)

         if (scan.Attribs[0].isContent()) {
            j += pf::strcopy(scan.Attribs[0].Value, Buffer+j, Size-j);
            if (j >= Size) return ERR::BufferOverflow;
         }
      }
   }

   return ERR::Okay;
}

static ERR get_all_content(extXML *Self, XMLTag &Tag, STRING Buffer, int Size, int &Output)
{
   Buffer[0] = 0;
   Output = 0;
   if (Tag.Children.empty()) return ERR::Okay;

   int j = 0;
   for (auto &scan : Tag.Children) {
      if (not scan.Attribs.empty()) { // Sanity check (there should always be at least 1 attribute)
         if (scan.Attribs[0].isContent()) {
            j += pf::strcopy(scan.Attribs[0].Value, Buffer+j, Size-j);
            if (j >= Size) return ERR::BufferOverflow;
         }
      }

      if (not scan.Children.empty()) {
         int out;
         ERR error = get_all_content(Self, scan, Buffer+j, Size-j, out);
         j += out;
         if ((error IS ERR::BufferOverflow) or (j >= Size)) return ERR::BufferOverflow;
      }
   }

   Output = j;
   return ERR::Okay;
}
