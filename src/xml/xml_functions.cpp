
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

static void parse_doctype(extXML *Self, CSTRING Input)
{

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

static ERR extract_tag(extXML *Self, TAGS &Tags, ParseState &State)
{
   enum { RAW_NONE=0, RAW_CDATA, RAW_NDATA };

   pf::Log log(__FUNCTION__);

   log.traceBranch("%.30s", State.Pos);

   if (State.Pos[0] != '<') {
      log.warning("Malformed XML statement detected.");
      return ERR::InvalidData;
   }

   CSTRING str = State.Pos + 1;

   if ((Self->Flags & XMF::INCLUDE_COMMENTS) IS XMF::NIL) {
      // Comments will be stripped - check if this is a comment and skip it if this is the case.
      if ((str[0] IS '!') and (str[1] IS '-') and (str[2] IS '-')) {
         int i;
         if ((i = pf::strsearch("-->", str)) != -1) {
            State.Pos = str + i + 3;
            return ERR::NothingDone;
         }
         else {
            log.warning("Detected malformed comment (missing --> terminator).");
            return ERR::InvalidData;
         }
      }
   }

   auto line_no = Self->LineNo;
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

      std::string name;

      if (*str IS '"'); // Quoted notation attributes are parsed as content values
      else {
         int s = 0;
         while ((str[s] > 0x20) and (str[s] != '>') and (str[s] != '=')) {
            if ((str[s] IS '/') and (str[s+1] IS '>')) break;
            if ((str[s] IS '?') and (str[s+1] IS '>')) break;
            s++;
         }
         name.assign(str, s);
         str += s;
      }

      // Extract the attributes value

      while ((*str) and (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }

      if (*str IS '=') {
         str++;
         while ((*str) and (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
         std::ostringstream buffer;
         if (*str IS '"') {
            str++;
            while ((*str) and (*str != '"')) { if (*str IS '\n') Self->LineNo++; buffer << *str++; }
            if (*str IS '"') str++;
         }
         else if (*str IS '\'') {
            str++;
            while ((*str) and (*str != '\'')) { if (*str IS '\n') Self->LineNo++; buffer << *str++; }
            if (*str IS '\'') str++;
         }
         else {
            while ((*str > 0x20) and (*str != '>')) {
               if ((str[0] IS '/') and (str[1] IS '>')) break;
               buffer << *str++;
            }
         }

         tag.Attribs.emplace_back(name, buffer.str());
      }
      else if ((name.empty()) and (*str IS '"')) { // Detect notation value with no name
         str++;
         int s;
         for (s=0; (str[s]) and (str[s] != '"'); s++) {
            if (str[s] IS '\n') Self->LineNo++;
         }
         tag.Attribs.emplace_back(name, std::string(str, s));
         str += s;
         if (*str IS '"') str++;
      }
      else tag.Attribs.emplace_back(name, ""); // Either the tag name or an attribute with no value.

      while ((*str) and (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
   }

   if (tag.Attribs.empty()) {
      log.warning("No attributes parsed for tag at line %d", Self->LineNo);
      return ERR::Syntax;
   }

   State.Pos = str;

   if ((*State.Pos IS '>') and (!tag.Attribs.empty()) and
       (tag.Attribs[0].Name[0] != '!') and (tag.Attribs[0].Name[0] != '?')) {
      // We reached the end of an open tag.  Extract the content within it and handle any child tags.

      State.Pos++;
      ERR error = extract_content(Self, Tags.back().Children, State);

      if ((error != ERR::Okay) and (error != ERR::NoData)) return error;

      while ((State.Pos[0] IS '<') and (State.Pos[1] != '/')) {
         error = extract_tag(Self, Tags.back().Children, State);

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

   // Extract the tag information.  This loop will extract the top-level tags.  The extract_tag() function is recursive
   // to extract the child tags.

   log.trace("Extracting tag information with extract_tag()");

   ParseState state;
   CSTRING str;
   for (str=Text; (*str) and (*str != '<'); str++) if (*str IS '\n') Self->LineNo++;
   state.Pos = str;
   while ((state.Pos[0] IS '<') and (state.Pos[1] != '/')) {
      ERR error = extract_tag(Self, Tags, state);

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
#if 0
static void sift_down(ListSort **lookup, int Index, int heapsize)
{
   int largest = Index;
   do {
      Index = largest;
      int left	= (Index << 1) + 1;
      int right	= left + 1;

      if (left < heapsize){
         if (str_sort(lookup[largest]->String, lookup[left]->String) > 0) largest = left;

         if (right < heapsize) {
            if (str_sort(lookup[largest]->String, lookup[right]->String) > 0) largest = right;
         }
      }

      if (largest != Index) {
         ListSort *temp = lookup[Index];
         lookup[Index] = lookup[largest];
         lookup[largest] = temp;
      }
   } while (largest != Index);
}
#endif
//********************************************************************************************************************
#if 0
static void sift_up(ListSort **lookup, int i, int heapsize)
{
   int largest = i;
   do {
      i = largest;
      int left	= (i << 1) + 1;
      int right	= left + 1;

      if (left < heapsize){
         if (str_sort(lookup[largest]->String, lookup[left]->String) < 0) largest = left;

         if (right < heapsize) {
            if (str_sort(lookup[largest]->String, lookup[right]->String) < 0) largest = right;
         }
      }

      if (largest != i) {
         ListSort *temp = lookup[i];
         lookup[i] = lookup[largest];
         lookup[largest] = temp;
      }
   } while (largest != i);
}
#endif

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
      char *buffer;
      int64_t size = 64 * 1024;
      if (AllocMemory(size+1, MEM::STRING|MEM::NO_CLEAR, &buffer) IS ERR::Okay) {
         int pos = 0;
         Self->ParseError = ERR::Okay;
         acSeekStart(Self->Source, 0);
         while (1) {
            int result;
            if (acRead(Self->Source, buffer+pos, size-pos, &result) != ERR::Okay) {
               Self->ParseError = ERR::Read;
               break;
            }
            else if (result <= 0) break;

            pos += result;
            if (pos >= size-1024) {
               if (ReallocMemory(buffer, (size * 2) + 1, (APTR *)&buffer, nullptr) != ERR::Okay) {
                  Self->ParseError = ERR::ReallocMemory;
                  break;
               }
               size = size * 2;
            }
         }

         if (Self->ParseError IS ERR::Okay) {
            buffer[pos] = 0;
            Self->ParseError = txt_to_xml(Self, Self->Tags, buffer);
         }

         FreeResource(buffer);
      }
      else Self->ParseError = ERR::AllocMemory;
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
