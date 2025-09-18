
//********************************************************************************************************************
// XPath Query
//
// [0-9]  Used for indexing
// '*'    For wild-carding of tag names
// '@'    An attribute
// '..'   Parent
// [=...] Match on encapsulated content (Not an XPath standard but we support it)
// //     Double-slash enables deep scanning of the XML tree.
//
// Round brackets may also be used as an alternative to square brackets.
//
// The use of \ as an escape character in attribute strings is supported, but keep in mind that this is not an official
// feature of the XPath standard.
//
// Examples:
//   /menu/submenu
//   /menu[2]/window
//   /menu/window/@title
//   /menu/window[@title='foo']/...
//   /menu[=contentmatch]
//   /menu//window
//   /menu/window/* (First child of the window tag)
//   /menu/*[@id='5']

ERR extXML::find_tag(std::string_view XPath)
{
   pf::Log log(__FUNCTION__);
   int i;

   if ((XPath.empty()) or (XPath[0] != '/')) {
      log.warning("Missing '/' prefix in '%.*s'.", int(XPath.size()), XPath.data());
      return ERR::StringFormat;
   }

   if ((!CursorTags) or (CursorTags->empty())) {
      log.warning("Sanity check failed; CursorTags not defined or empty.");
      return ERR::Failed;
   }

   bool flat_scan = false;
   size_t pos = [&flat_scan, XPath]() mutable {
      if (XPath[0] != '/') return 0;
      if ((XPath.size() > 1) and (XPath[1] != '/')) return 1;
      flat_scan = true;
      return 2;
   }();

   // Check if the path is something like '/@attrib' which means we want the attribute value of the current tag

   if ((pos < XPath.size()) and (XPath[pos] IS '@')) Attrib.assign(XPath.substr(pos + 1));
   
   // Parse the tag name

   auto start = pos;
   auto delimiter_pos = XPath.find_first_of("/[(", pos);
   pos = (delimiter_pos != std::string_view::npos) ? delimiter_pos : XPath.size();
   std::string_view tagname;
   if (pos > start) tagname = XPath.substr(start, pos - start);
   else tagname = "*";

   // Parse filter instructions

   std::string attrib_value;
   std::string_view attrib_name;
   bool wild = false;
   int subscript = 0;

   if ((this->Flags & XMF::LOG_ALL) != XMF::NIL) log.branch("XPath: %.*s, TagName: %.*s", int(XPath.size()), XPath.data(), int(tagname.size()), tagname.data());

   char end_char;
   if ((pos < XPath.size()) and ((XPath[pos] IS '[') or (XPath[pos] IS '('))) {
      if (XPath[pos] IS '[') end_char = ']';
      else end_char = ')';

      pos++;

      auto non_space_pos = XPath.find_first_not_of(" \t\n\r", pos);
      pos = (non_space_pos != std::string_view::npos) ? non_space_pos : XPath.size();

      if ((pos < XPath.size()) and (XPath[pos] >= '0') and (XPath[pos] <= '9')) { // Parse index
         subscript = strtol(XPath.data()+pos, nullptr, 0);
         if (subscript < 1) return ERR::Syntax; // Subscripts start from 1, not 0
         auto digit_end = XPath.find_first_not_of("0123456789", pos);
         pos = (digit_end != std::string_view::npos) ? digit_end : XPath.size();
      }
      else if ((pos < XPath.size()) and ((XPath[pos] IS '@') or (XPath[pos] IS '='))) {
         if (XPath[pos] IS '@') {  // Parse attribute filter such as "[@id='5']"
            pos++;

            auto len = pos;
            while ((len < XPath.size()) and (((XPath[len] >= 'a') and (XPath[len] <= 'z')) or
                   ((XPath[len] >= 'A') and (XPath[len] <= 'Z')) or
                   (XPath[len] IS '_'))) len++;

            attrib_name = XPath.substr(pos, len - pos);
            if (attrib_name.empty()) return ERR::Syntax;

            pos = len;
            auto non_space_pos2 = XPath.find_first_not_of(" \t\n\r", pos);
            pos = (non_space_pos2 != std::string_view::npos) ? non_space_pos2 : XPath.size(); // Skip whitespace

            if ((pos >= XPath.size()) or (XPath[pos] != '=')) return ERR::Syntax;
            pos++;
         }
         else pos++; // Skip '=' (indicates matching on content)

         auto non_space_pos3 = XPath.find_first_not_of(" \t\n\r", pos);
         pos = (non_space_pos3 != std::string_view::npos) ? non_space_pos3 : XPath.size(); // Skip whitespace

         // Parse value

         if ((pos < XPath.size()) and ((XPath[pos] IS '\'') or (XPath[pos] IS '"'))) {
            const char quote = XPath[pos++];
            bool esc_attrib = false;
            auto end = pos;
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
               else if (XPath[end] IS '*') wild = true;
               end++;
            }

            if ((end >= XPath.size()) or (XPath[end] != quote)) return ERR::Syntax; // Quote not terminated correctly

            attrib_value.assign(XPath.substr(pos, end - pos));

            pos = end + 1;

            if (esc_attrib) {
               for (i=0; i < std::ssize(attrib_value); i++) {
                  if (attrib_value[i] != '\\') continue;
                  auto ch = attrib_value[i+1];
                  if ((ch IS '*') or (ch IS quote) or (ch IS '\\')) {
                     attrib_value.erase(i);
                     i--;
                  }
               }
            }
         }
         else {
            auto end_pos = XPath.find(end_char, pos);
            int end = (end_pos != std::string_view::npos) ? end_pos : XPath.size();

            // Check for wildcards in the range
            if (XPath.substr(pos, end - pos).find('*') != std::string_view::npos) wild = true;
            attrib_value.assign(XPath.substr(pos, end - pos));
            pos = end;
         }
      }
      else return ERR::Syntax;

      auto non_space_pos4 = XPath.find_first_not_of(" \t\n\r", pos);
      pos = (non_space_pos4 != std::string_view::npos) ? non_space_pos4 : XPath.size(); // Skip whitespace
      if ((pos >= XPath.size()) or (XPath[pos] != end_char)) return ERR::Syntax;
      pos++;
   }

   auto tag_wild = tagname.find('*') != std::string_view::npos;
   
   bool stop = false;
   for (; Cursor != CursorTags->end() and (!stop); Cursor++) {
      bool match = false;
      bool tag_matched = false;

      if (tag_wild) tag_matched = pf::wildcmp(tagname, Cursor->name());
      else tag_matched = pf::iequals(tagname, Cursor->name());

      if (tag_matched) { // Desired tag name found.
         if ((!attrib_name.empty()) or (!attrib_value.empty())) {
            if (Cursor->name()) {
               if (!attrib_name.empty()) { // Match by named attribute value
                  for (int a=1; a < std::ssize(Cursor->Attribs); ++a) {
                     if (pf::iequals(Cursor->Attribs[a].Name, attrib_name)) {
                        if (wild) {
                           if ((match = pf::wildcmp(Cursor->Attribs[a].Value, attrib_value))) break;
                        }
                        else if (pf::iequals(Cursor->Attribs[a].Value, attrib_value)) {
                           match = true;
                           break;
                        }
                     }
                  }
               }
               else if (!attrib_value.empty()) {
                  if ((!Cursor->Children.empty()) and (Cursor->Children[0].Attribs[0].isContent())) {
                     if (wild) {
                        if ((match = pf::wildcmp(Cursor->Children[0].Attribs[0].Value, attrib_value))) break;
                     }
                     else match = pf::iequals(Cursor->Children[0].Attribs[0].Value, attrib_value);
                  }
               }
               else match = true;
            }

            if ((!match) and (flat_scan) and (!Cursor->Children.empty())) {
               auto save_cursor = Cursor;
               auto save_tags   = CursorTags;

               CursorTags = &Cursor->Children;
               Cursor     = Cursor->Children.begin();

               ERR error = find_tag(XPath);
               if ((error IS ERR::Okay) and (!Callback.defined())) return ERR::Okay;

               Cursor     = save_cursor;
               CursorTags = save_tags;
            }
         }
         else match = true;
      }

      if ((not match) and (not flat_scan)) continue;

      if (subscript > 1) { 
         subscript--;
         continue;
      }
      else if (subscript IS 1) {
         stop = true;
      }

      bool path_ended = pos >= XPath.size() or ((XPath[pos] IS '/') and (pos + 1 < XPath.size()) and (XPath[pos+1] IS '@'));
      if ((match) and (path_ended)) { // Matching tag found and there is nothing left in the path
         if (pos < XPath.size()) Attrib.assign(XPath.substr(pos + 2));
         else Attrib.clear();

         if (!Callback.defined()) return ERR::Okay; // End of query, successfully found tag

         auto error = ERR::Okay;
         if (Callback.isC()) {
            auto routine = (ERR (*)(extXML *, int, CSTRING, APTR))Callback.Routine;
            error = routine(this, Cursor->ID, Attrib.empty() ? nullptr : Attrib.c_str(), Callback.Meta);
         }
         else if (Callback.isScript()) {
            if (sc::Call(Callback, std::to_array<ScriptArg>({
               { "XML",  this, FD_OBJECTPTR },
               { "Tag",  Cursor->ID },
               { "Attrib", Attrib.empty() ? CSTRING(nullptr) : Attrib.c_str() }
            }), error) != ERR::Okay) error = ERR::Terminate;
         }
         else return ERR::InvalidValue;

         if (error IS ERR::Terminate) return ERR::Terminate;

         if (error != ERR::Okay) return error;

         // Searching will continue because the client specified a callback...
      }
      else if (!Cursor->Children.empty()) { // Tag matched & XPath continues, OR flat-scan enabled.  Scan deeper into the tree
         auto save_cursor = Cursor;
         auto save_tags   = CursorTags;

         CursorTags = &Cursor->Children;
         Cursor     = Cursor->Children.begin();

         ERR error;
         if (flat_scan) error = find_tag(XPath); // Continue search from the beginning of the tag name
         else error = find_tag(XPath.substr(pos));
         if ((error IS ERR::Okay) and (!Callback.defined())) return ERR::Okay;

         if (error IS ERR::Terminate) return ERR::Terminate;

         Cursor     = save_cursor;
         CursorTags = save_tags;
      }
   }

   if (Callback.defined()) return ERR::Okay;
   else return ERR::Search;
}
