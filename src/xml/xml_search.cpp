
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

ERR extXML::find_tag(CSTRING XPath)
{
   pf::Log log(__FUNCTION__);
   LONG i;

   if ((!XPath[0]) or (XPath[0] != '/')) {
      log.warning("Missing '/' prefix in '%s'.", XPath);
      return ERR::StringFormat;
   }

   if ((!CursorTags) or (CursorTags->empty())) {
      log.warning("Sanity check failed; CursorTags not defined or empty.");
      return ERR::Failed;
   }

   bool deepscan = false;
   LONG pos = [ deepscan, XPath, this ]() mutable {
      if (XPath[0] != '/') return 0;
      if (XPath[1] != '/') return 1;
      deepscan = true;
      return 2;
   }();

   // Parse the tag name

   if (XPath[pos] IS '@') Attrib.assign(XPath + pos + 1);

   auto start = pos;
   while (XPath[pos] and (XPath[pos] != '/') and (XPath[pos] != '[') and (XPath[pos] != '(')) pos++;
   std::string tagname;
   tagname.assign(XPath, start, pos - start);
   if (tagname.empty()) tagname.assign("*");

   // Parse filter instructions

   std::string attribvalue, attribname;
   bool wild = false;
   LONG subscript = 0;

   if ((this->Flags & XMF::LOG_ALL) != XMF::NIL) log.branch("XPath: %s, TagName: %s", XPath, tagname.c_str());

   char endchar;
   if ((XPath[pos] IS '[') or (XPath[pos] IS '(')) {
      if (XPath[pos] IS '[') endchar = ']';
      else endchar = ')';

      pos++;

      while ((XPath[pos]) and (XPath[pos] <= 0x20)) pos++;

      if ((XPath[pos] >= '0') and (XPath[pos] <= '9')) { // Parse index
         subscript = StrToInt(XPath+pos);
         while ((XPath[pos] >= '0') and (XPath[pos] <= '9')) pos++;
      }
      else if ((XPath[pos] IS '@') or (XPath[pos] IS '=')) {
         if (XPath[pos] IS '@') {  // Parse attribute filter such as "[@id='5']"
            pos++;

            LONG len = pos;
            while (((XPath[len] >= 'a') and (XPath[len] <= 'z')) or
                   ((XPath[len] >= 'A') and (XPath[len] <= 'Z')) or
                   (XPath[len] IS '_')) len++;

            attribname.assign(XPath, pos, len - pos);
            if (attribname.empty()) return ERR::Syntax;

            pos = len;
            while ((XPath[pos]) and (XPath[pos] <= 0x20)) pos++; // Skip whitespace

            if (XPath[pos] != '=') return ERR::Syntax;
            pos++;
         }
         else pos++; // Skip '=' (indicates matching on content)

         while ((XPath[pos]) and (XPath[pos] <= 0x20)) pos++; // Skip whitespace

         // Parse value

         if ((XPath[pos] IS '\'') or (XPath[pos] IS '"')) {
            const char quote = XPath[pos++];
            bool escattrib = false;
            LONG end = pos;
            while ((XPath[end]) and (XPath[end] != quote)) {
               if (XPath[end] IS '\\') { // Escape character check
                  auto ch = XPath[end+1];
                  if ((ch IS '*') or (ch IS quote) or (ch IS '\\')) {
                     end++;
                     escattrib = true;
                  }
               }
               else if (XPath[end] IS '*') wild = true;
               end++;
            }

            if (XPath[end] != quote) return ERR::Syntax; // Quote not terminated correctly

            attribvalue.assign(XPath, pos, end - pos);

            pos = end + 1;

            if (escattrib) {
               for (i=0; i < std::ssize(attribvalue); i++) {
                  if (attribvalue[i] != '\\') continue;
                  auto ch = attribvalue[i+1];
                  if ((ch IS '*') or (ch IS quote) or (ch IS '\\')) {
                     attribvalue.erase(i);
                     i--;
                  }
               }
            }
         }
         else {
            LONG end = pos;
            while ((XPath[end]) and (XPath[end] != endchar)) {
               if (XPath[end] IS '*') wild = true;
               end++;
            }
            attribvalue.assign(XPath + pos, end - pos);
            pos = end;
         }
      }
      else return ERR::Syntax;

      while ((XPath[pos]) and (XPath[pos] <= 0x20)) pos++; // Skip whitespace
      if (XPath[pos] != endchar) return ERR::Syntax;
      pos++;
   }

   auto tagwild = tagname.find('*') != std::string::npos;

   for (; Cursor != CursorTags->end(); Cursor++) {
      bool match = false;
      bool tag_matched = false;

      if (tagwild) tag_matched = pf::wildcmp(tagname, Cursor->name());
      else tag_matched = StrCompare(tagname, Cursor->name(), 0, STR::MATCH_LEN) IS ERR::Okay;
     
      if (tag_matched) { // Desired tag name found.
         if ((!attribname.empty()) or (!attribvalue.empty())) {
            if (Cursor->name()) {
               if (!attribname.empty()) { // Match by named attribute value
                  for (LONG a=1; a < std::ssize(Cursor->Attribs); ++a) {
                     if (StrCompare(Cursor->Attribs[a].Name, attribname, attribname.size()) IS ERR::Okay) {
                        if (wild) {
                           if ((match = pf::wildcmp(Cursor->Attribs[a].Value, attribvalue))) break;
                        }
                        else if (StrCompare(Cursor->Attribs[a].Value, attribvalue, 0, STR::MATCH_LEN) IS ERR::Okay) {
                           match = true;
                           break;
                        }
                     }
                  }
               }
               else if (!attribvalue.empty()) {
                  if ((!Cursor->Children.empty()) and (Cursor->Children[0].Attribs[0].isContent())) {
                     if (wild) {
                        if ((match = pf::wildcmp(Cursor->Children[0].Attribs[0].Value, attribvalue))) break;
                     }
                     else if (StrCompare(Cursor->Children[0].Attribs[0].Value, attribvalue, 0, STR::MATCH_LEN) IS ERR::Okay) {
                        match = true;
                     }
                  }
               }
               else match = true;
            }

            if ((!match) and (deepscan) and (!Cursor->Children.empty())) {
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

      if (!match) continue;

      if (subscript > 0) { subscript--; continue; }

      if ((!XPath[pos]) or ((XPath[pos] IS '/') and (XPath[pos+1] IS '@'))) {
         // Matching tag found and there is nothing left in the path
         if (XPath[pos]) Attrib.assign(XPath + pos + 2);
         else Attrib.clear();

         if (!Callback.defined()) return ERR::Okay; // End of query, successfully found tag

         ERR error = ERR::Okay;
         if (Callback.isC()) {
            auto routine = (ERR (*)(extXML *, LONG, CSTRING, APTR))Callback.Routine;
            error = routine(this, Cursor->ID, Attrib.empty() ? NULL : Attrib.c_str(), Callback.Meta);
         }
         else if (Callback.isScript()) {
            if (scCall(Callback, std::to_array<ScriptArg>({
               { "XML",  this, FD_OBJECTPTR },
               { "Tag",  Cursor->ID },
               { "Attrib", Attrib.empty() ? CSTRING(NULL) : Attrib.c_str() }
            }), error) != ERR::Okay) error = ERR::Terminate;
         }
         else return ERR::InvalidValue;

         if (error IS ERR::Terminate) return ERR::Terminate;

         if (error != ERR::Okay) return error;

         // Searching will continue because the client specified a callback...
      }
      else if (!Cursor->Children.empty()) { // Tag matched; XPath continues; scan deeper into the XML tree
         auto save_cursor = Cursor;
         auto save_tags   = CursorTags;

         CursorTags = &Cursor->Children;
         Cursor     = Cursor->Children.begin();

         ERR error = find_tag(XPath+pos);
         if ((error IS ERR::Okay) and (!Callback.defined())) return ERR::Okay;

         if (error IS ERR::Terminate) return ERR::Terminate;

         Cursor     = save_cursor;
         CursorTags = save_tags;
      }
   }

   if (Callback.defined()) return ERR::Okay;
   else return ERR::Search;
}
