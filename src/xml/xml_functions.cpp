
static ERROR count_tags(extXML *Self, CSTRING Text, CSTRING *Result)
{
   pf::Log log(__FUNCTION__);

   if (*Text != '<') {
      log.warning("Malformed XML statement detected.");
      return ERR_InvalidData;
   }
   else Text++;

   // In a CDATA section, everything is skipped up to the ]]>
   // termination point.

   if (!StrCompare("![CDATA[", Text, 8, STR_MATCH_CASE)) {
      Text += 8;
      while (*Text) {
         if ((Text[0] IS ']') and (Text[1] IS ']') and (Text[2] IS '>')) {
            Text += 3;
            break;
         }
         else Text++;
      }

      Self->TagCount++; // CDATA sections are allocated as content tags

      *Result = Text;
      return ERR_Okay;
   }

   // Leveled DATA exhibits the same behaviour of CDATA but allows nesting

   if (!StrCompare("![NDATA[", Text, 8, STR_MATCH_CASE)) {
      UWORD nest = 1;
      Text += 8;
      while (*Text) {
         if ((Text[0] IS '<') and (Text[1] IS '!') and (Text[2] IS '[') and ((Text[3] IS 'N') or (Text[3] IS 'C')) and (Text[4] IS 'D') and (Text[5] IS 'A') and (Text[6] IS 'T') and (Text[7] IS 'A') and (Text[8] IS '[')) {
            nest++;
            Text += 8;
         }
         else if ((Text[0] IS ']') and (Text[1] IS ']') and (Text[2] IS '>')) {
            Text += 3;
            nest--;
            if (!nest) break;
         }
         else Text++;
      }

      Self->TagCount++; // NDATA sections are allocated as content tags

      *Result = Text;
      return ERR_Okay;
   }

   // Comment handling

   if ((Text[0] IS '!') and (Text[1] IS '-') and (Text[2] IS '-')) {
      Text += 3;
      while (*Text) {
         if ((Text[0] IS '-') and (Text[1] IS '-') and (Text[2] IS '>')) {
            Text += 3;
            if (Self->Flags & XMF_INCLUDE_COMMENTS) Self->TagCount++;
            *Result = Text;
            return ERR_Okay;
         }
         Text++;
      }

      log.warning("Unterminated comment detected.");
      return ERR_InvalidData;
   }

   // Skip past the tag's attributes

   CSTRING str = Text;
   while ((*str) and (*str != '>')) {
      if ((str[0] IS '/') and (str[1] IS '>')) break;

      skipwhitespace(str);

      if ((!*str) or (*str IS '>') or ((*str IS '/') and (str[1] IS '>')) or (*str IS '=')) break;

      while ((*str > 0x20) and (*str != '>') and (*str != '=')) { // Tag name
         if ((str[0] IS '/') and (str[1] IS '>')) break;
         str++;
      }

      skipwhitespace(str);

      if (*str IS '=') {
         str++;
         skipwhitespace(str);
         if (*str IS '"') {
            str++;
            while ((*str) and (*str != '"')) str++;
            if (*str IS '"') str++;
         }
         else if (*str IS '\'') {
            str++;
            while ((*str) and (*str != '\'')) str++;
            if (*str IS '\'') str++;
         }
         else while ((*str > 0x20) and (*str != '>')) {
            if ((str[0] IS '/') and (str[1] IS '>')) break;
            str++;
         }
      }
      else if (*str IS '"') { // Notation attributes don't have names
         str++;
         while ((*str) and (*str != '"')) str++;
         if (*str IS '"') str++;
      }
   }

   if ((*str IS '>') and (*Text != '!') and (*Text != '?')) {
      // The tag is open.  Scan the content within it and handle any child tags.

      str++;
      if (!(Self->Flags & XMF_ALL_CONTENT)) skipwhitespace(str);
      if (*str != '<') {
         while ((*str) and (*str != '<')) str++;
         if (!(Self->Flags & XMF_STRIP_CONTENT)) Self->TagCount++; // A content tag will be required
      }

      while ((str[0] IS '<') and (str[1] != '/')) {
         ERROR error;
         if (!(error = count_tags(Self, str, &str))) {
            if (!(Self->Flags & XMF_ALL_CONTENT)) skipwhitespace(str);
            if (*str != '<') {
               while ((*str) and (*str != '<')) str++;
               if (!(Self->Flags & XMF_STRIP_CONTENT)) Self->TagCount++; // An embedded content tag will be required
            }
         }
         else return error;
      }

      // There should be a closing tag - skip past it

      if ((str[0] IS '<') and (str[1] IS '/')) {
         while ((*str) and (*str != '>')) str++;
      }

      if (*str IS '>') str++;
   }
   else if ((str[0] IS '/') and (str[1] IS '>')) str += 2;

   if ((Self->Flags & XMF_STRIP_HEADERS) and ((*Text IS '?') or (*Text IS '!'))); // Ignore headers (no tag count increase)
   else Self->TagCount++;

   *Result = str;

   return ERR_Okay;
}

//********************************************************************************************************************
// Convert a text string into XML tags.

static ERROR txt_to_xml(extXML *Self, CSTRING Text)
{
   pf::Log log(__FUNCTION__);

   if ((!Self) or (!Text)) return ERR_NullArgs;

   Self->Balance = 0;
   Self->LineNo = 1;

   clear_tags(Self);  // Kill any existing tags in this XML object

   // Perform a count of the total amount of tags specified (closing tags excluded)

   CSTRING str;
   for (str=Text; (*str) and (*str != '<'); str++);
   while ((str[0] IS '<') and (str[1] != '/')) {
      if (count_tags(Self, str, &str) != ERR_Okay) {
         log.warning("Aborting XML interpretation process.");
         return ERR_InvalidData;
      }
      while ((*str) and (*str != '<')) str++;
   }

   if (Self->TagCount < 1) {
      log.warning("There are no valid tags in the XML statement.");
      return ERR_NoData;
   }

   log.trace("Detected %d raw and content based tags, options $%.8x.", Self->TagCount, Self->Flags);

   // Allocate an array to hold all of the XML tags

   XMLTag **tag;
   if (AllocMemory(sizeof(APTR) * (Self->TagCount + 1), MEM_DATA|MEM_UNTRACKED, &tag) != ERR_Okay) {
      return ERR_AllocMemory;
   }

   FreeResource(Self->Tags);
   Self->Tags = tag;

   // Extract the tag information.  This loop will extract the top-level tags.  The extract_tag() function is recursive
   // to extract the child tags.

   log.trace("Extracting tag information with extract_tag()");

   exttag ext = { .Start = Text, .TagIndex = 0, .Branch = 0 };
   XMLTag *prevtag = NULL;
   for (str=Text; (*str) and (*str != '<'); str++) if (*str IS '\n') Self->LineNo++;
   ext.Pos = str;
   while ((ext.Pos[0] IS '<') and (ext.Pos[1] != '/')) {
      LONG i = ext.TagIndex; // Remember the current tag index before extract_tag() changes it

      ERROR error = extract_tag(Self, &ext);

      if ((error != ERR_Okay) and (error != ERR_NothingDone)) {
         log.warning("Aborting XML interpretation process.");
         return ERR_InvalidData;
      }

      // Skip content/whitespace to get to the next tag
      str = ext.Pos;
      while ((*str) and (*str != '<')) { if (*str IS '\n') Self->LineNo++; str++; }
      ext.Pos = str;

      if (error IS ERR_NothingDone) continue;

      if (prevtag) prevtag->Next = Self->Tags[i];
      prevtag = Self->Tags[i];
   }

   // If the XML statement contained errors such as unclosed tags, the tag count may be greater than the actual number of tags
   // loaded.  This routine checks that the expected tag count matches what was extracted.

   for (LONG ti=0; ti < Self->TagCount; ti++) {
      if (!Self->Tags[ti]) {
         if (Self->Flags & XMF_WELL_FORMED) return log.warning(ERR_UnbalancedXML);

         log.warning("Non-fatal error - %d tags expected, loaded %d.", Self->TagCount, ti);
         if (ti > 0) {
            Self->Tags[ti-1]->Next = NULL;
            Self->Tags[ti-1]->Child = NULL;
         }
         Self->Tags[ti] = 0;
         Self->TagCount = ti;
         break;
      }
   }

   // If the WELL_FORMED flag has been used, check that the tags balance.  If they don't then return ERR_InvalidData.

   if (Self->Flags & XMF_WELL_FORMED) {
      if (Self->Balance != 0) return log.warning(ERR_UnbalancedXML);
   }

   // Set the Prev and Index fields

   for (LONG i=0; i < Self->TagCount; i++) {
      Self->Tags[i]->Index = i;
      if (Self->Tags[i]->Next) Self->Tags[i]->Next->Prev = Self->Tags[i];
   }

   // Upper/lowercase transformations

   if (Self->Flags & XMF_UPPER_CASE) {
      log.trace("Performing uppercase translations.");
      STRING str;
      for (LONG i=0; i < Self->TagCount; i++) {
         for (LONG j=0; j < Self->Tags[i]->TotalAttrib; j++) {
            if ((str = Self->Tags[i]->Attrib[j].Name)) {
               while (*str) { if ((*str >= 'a') and (*str <= 'z')) *str = *str - 'a' + 'A'; str++; }
            }
            if ((str = Self->Tags[i]->Attrib[j].Value)) {
               while (*str) { if ((*str >= 'a') and (*str <= 'z')) *str = *str - 'a' + 'A'; str++; }
            }
         }
      }
   }
   else if (Self->Flags & XMF_LOWER_CASE) {
      log.trace("Performing lowercase translations.");
      for (LONG i=0; i < Self->TagCount; i++) {
         for (LONG j=0; j < Self->Tags[i]->TotalAttrib; j++) {
            STRING str;
            if ((str = Self->Tags[i]->Attrib[j].Name)) {
               while (*str) { if ((*str >= 'A') and (*str <= 'Z')) *str = *str - 'A' + 'a'; str++; }
            }
            if ((str = Self->Tags[i]->Attrib[j].Value)) {
               while (*str) { if ((*str >= 'A') and (*str <= 'Z')) *str = *str - 'A' + 'a'; str++; }
            }
         }
      }
   }

   if (!(Self->Flags & XMF_NO_ESCAPE)) {
      log.trace("Unescaping XML.");
      for (LONG i=0; i < Self->TagCount; i++) {
         for (LONG j=0; j < Self->Tags[i]->TotalAttrib; j++) {
            if (!Self->Tags[i]->Attrib[j].Value) continue;
            if (Self->Tags[i]->CData) continue;
            xml_unescape(Self, Self->Tags[i]->Attrib[j].Value);
         }
      }
   }

   log.trace("XML parsing complete.");
   return ERR_Okay;
}

//********************************************************************************************************************
// Extracts the next tag from an XML string.  This function also recurses into itself.

static CSTRING extract_tag_attrib(extXML *Self, CSTRING Str, LONG *AttribSize, WORD *TotalAttrib)
{
   CSTRING str = Str;
   LONG size = 0;
   while ((*str) and (*str != '>')) {
      if ((str[0] IS '/') and (str[1] IS '>')) break; // Termination checks
      if ((str[0] IS '?') and (str[1] IS '>')) break;

      while ((*str) and (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
      if ((*str IS 0) or (*str IS '>') or (((*str IS '/') or (*str IS '?')) and (str[1] IS '>'))) break;

      if (*str IS '=') return NULL; // Check for invalid XML

      if (*str IS '"') { // Notation values can start with double quotes and have no name.
         str++;
         while ((*str) and (*str != '"')) { size++; str++; }
         if (*str IS '"') str++;
         size++; // String termination byte
      }
      else {
         while ((*str > 0x20) and (*str != '>') and (*str != '=')) {
            if ((str[0] IS '/') and (str[1] IS '>')) break;
            if ((str[0] IS '?') and (str[1] IS '>')) break;
            str++; size++;
         }
         size++; // String termination byte

         while ((*str) and (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }

         if (*str IS '=') {
            str++;
            while ((*str) and (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
            if (*str IS '"') {
               str++;
               while ((*str) and (*str != '"')) { if (*str IS '\n') Self->LineNo++; str++; size++; }
               if (*str IS '"') str++;
            }
            else if (*str IS '\'') {
               str++;
               while ((*str) and (*str != '\'')) { if (*str IS '\n') Self->LineNo++; str++; size++; }
               if (*str IS '\'') str++;
            }
            else while ((*str > 0x20) and (*str != '>')) {
               if ((str[0] IS '/') and (str[1] IS '>')) break;
               if ((str[0] IS '?') and (str[1] IS '>')) break;
               str++; size++;
            }

            size++; // String termination byte
         }
      }

      TotalAttrib[0]++;
   }

   *AttribSize += size;
   return str;
}

//********************************************************************************************************************
// Called by txt_to_xml() to extract the next tag from an XML string.  This function also recurses into itself.

static ERROR extract_tag(extXML *Self, exttag *Status)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Index: %d, Level: %d, %.30s", Status->TagIndex, Status->Branch, Status->Pos);

   if (Status->Pos[0] != '<') {
      log.warning("Malformed XML statement detected.");
      return ERR_InvalidData;
   }

   CSTRING str = Status->Pos+1;

   if (!(Self->Flags & XMF_INCLUDE_COMMENTS)) {
      // Comments will be stripped - check if this is a comment and skip it if this is the case.
      if (!StrCompare("!--", str, 3, STR_MATCH_CASE)) {
         LONG i;
         if ((i = StrSearch("-->", str, TRUE)) != -1) {
            Status->Pos = str + i + 3;
            return ERR_NothingDone;
         }
         else {
            log.warning("Detected malformed comment (missing --> terminator).");
            return ERR_InvalidData;
         }
      }
   }

   // Check that the tag index does not exceed the total number of calculated tags

   if (Status->TagIndex >= Self->TagCount) {
      log.warning("Ran out of array space for tag extraction (expected %d tags).", Status->TagIndex);
      return ERR_ArrayFull;
   }

   // Count the number of tag attributes

   LONG line_no = Self->LineNo;
   BYTE raw_content;

   if (!StrCompare("![CDATA[", str, 8, STR_MATCH_CASE)) { raw_content = 1; str += 8; }
   else if (!StrCompare("![NDATA[", str, 8, STR_MATCH_CASE)) { raw_content = 2; str += 8; }
   else raw_content = 0;

   if (raw_content) {
      LONG len;

      // CDATA handler

      if (raw_content IS 1) {
         for (len=0; str[len]; len++) {
            if ((str[len] IS ']') and (str[len+1] IS ']') and (str[len+2] IS '>')) {
               break;
            }
            else if (str[len] IS '\n') Self->LineNo++;
         }
      }
      else if (raw_content IS 2) {
         UWORD nest = 1;
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

      if ((Self->Flags & XMF_STRIP_CONTENT) or (!len)) {
         Status->Pos = str + len + 3;
         return ERR_NothingDone;
      }

      if (!str[len]) {
         log.warning("Malformed XML:  A CDATA section is missing its closing string.");
         return ERR_InvalidData;
      }

      // CDATA sections are assimilated into the parent tag as content

      XMLTag *tag;
      if (!AllocMemory(sizeof(XMLTag) + Self->PrivateDataSize + sizeof(XMLAttrib) + len + 1,
            MEM_UNTRACKED|MEM_NO_CLEAR, &tag, NULL)) {

         ClearMemory(tag, sizeof(XMLTag) + Self->PrivateDataSize + sizeof(XMLAttrib));

         Self->Tags[Status->TagIndex] = tag;
         tag->Private     = (BYTE *)tag + sizeof(XMLTag);
         tag->Attrib      = (XMLAttrib *)((BYTE *)tag + sizeof(XMLTag) + Self->PrivateDataSize);
         tag->TotalAttrib = 1;
         tag->ID          = glTagID++;
         tag->AttribSize  = len + 1;
         tag->CData       = TRUE;
         tag->Branch      = Status->Branch;
         tag->LineNo      = line_no;

         STRING buffer = (char *)tag->Attrib + sizeof(XMLAttrib);

         tag->Attrib->Name  = NULL;
         tag->Attrib->Value = buffer;

         CopyMemory(str, buffer, len);
         buffer[len] = 0;

         Status->TagIndex += 1;
         Status->Pos = str + len + 3;
         return ERR_Okay;
      }
      else return ERR_AllocMemory;
   }

   // Parse the element name and its attributes

   LONG attribsize = 0;
   WORD totalattrib = 0;
   str = extract_tag_attrib(Self, str, &attribsize, &totalattrib);
   if (!str) return log.warning(ERR_InvalidData);

   if ((Status->Pos[1] IS '?') or (Status->Pos[1] IS '!')) {
      if (Self->Flags & XMF_PARSE_ENTITY) {
         if (!StrCompare("!DOCTYPE", Status->Pos+1, 7, 0)) {
            parse_doctype(Self, Status->Pos+7);
         }
      }

      if ((Self->Flags & XMF_STRIP_HEADERS) ) {
         if (*str IS '>') str++;
         Status->Pos = str;
         return ERR_NothingDone;
      }
   }

   if (totalattrib <= 0) {
      log.warning("Failed to extract a tag from \"%.10s\" (offset %d), index %d, nest %d.", Status->Pos, (LONG)(Status->Pos - Status->Start), Status->TagIndex, Status->Branch);
      return ERR_InvalidData;
   }

   XMLTag *tag;
   if (AllocMemory(sizeof(XMLTag) + Self->PrivateDataSize + (sizeof(XMLAttrib) * totalattrib) + attribsize, MEM_UNTRACKED, &tag) != ERR_Okay) {
      return log.warning(ERR_AllocMemory);
   }

   tag->Private     = ((BYTE *)tag) + sizeof(XMLTag);
   tag->Attrib      = (XMLAttrib *)(((BYTE *)tag) + sizeof(XMLTag) + Self->PrivateDataSize);
   tag->TotalAttrib = totalattrib;
   tag->AttribSize  = attribsize;
   tag->ID          = glTagID++;
   tag->Branch      = Status->Branch;
   tag->LineNo      = line_no;

   Self->Tags[Status->TagIndex] = tag;
   Status->TagIndex++;
   Self->Balance++;

   // Extract all attributes within the tag

   STRING buffer = ((char *)tag->Attrib) + (sizeof(XMLAttrib) * tag->TotalAttrib);
   str    = Status->Pos+1;
   if (*str IS '?') tag->Instruction = TRUE; // Detect <?xml ?> style instruction elements.
   else if ((*str IS '!') and (str[1] >= 'A') and (str[1] <= 'Z')) tag->Notation = TRUE;

   LONG a = 0;
   while ((*str) and (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
   while ((*str) and (*str != '>')) {
      if ((str[0] IS '/') and (str[1] IS '>')) break; // Termination checks
      if ((str[0] IS '?') and (str[1] IS '>')) break;

      if (*str IS '=') return log.warning(ERR_InvalidData);

      if (a >= tag->TotalAttrib) return log.warning(ERR_BufferOverflow);

      // Extract the name of the attribute

      if (*str IS '"') { // Quoted notation attributes are parsed as values instead
         tag->Attrib[a].Name = NULL;
      }
      else {
         tag->Attrib[a].Name = buffer;
         while ((*str > 0x20) and (*str != '>') and (*str != '=')) {
            if ((str[0] IS '/') and (str[1] IS '>')) break;
            if ((str[0] IS '?') and (str[1] IS '>')) break;
            *buffer++ = *str++;
         }
         *buffer++ = 0;
      }

      // Extract the attributes value

      while ((*str) and (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }

      if (*str IS '=') {
         str++;
         while ((*str) and (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
         tag->Attrib[a].Value = buffer;
         if (*str IS '"') {
            str++;
            while ((*str) and (*str != '"')) { if (*str IS '\n') Self->LineNo++; *buffer++ = *str++; }
            if (*str IS '"') str++;
         }
         else if (*str IS '\'') {
            str++;
            while ((*str) and (*str != '\'')) { if (*str IS '\n') Self->LineNo++; *buffer++ = *str++; }
            if (*str IS '\'') str++;
         }
         else {
            while ((*str > 0x20) and (*str != '>')) {
               if ((str[0] IS '/') and (str[1] IS '>')) break;
               *buffer++ = *str++;
            }
         }

         *buffer++ = 0;
      }
      else if ((!tag->Attrib[a].Name) and (*str IS '"')) { // Detect notation value with no name
         tag->Attrib[a].Value = buffer;
         str++;
         while ((*str) and (*str != '"')) { if (*str IS '\n') Self->LineNo++; *buffer++ = *str++; }
         if (*str IS '"') str++;
         *buffer++ = 0;
      }

      a++;
      while ((*str) and (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
   }

   Status->Pos = str;

   if ((*Status->Pos IS '>') and (tag->Attrib->Name[0] != '!') and (tag->Attrib->Name[0] != '?')) {
      // We reached the end of an unclosed tag.  Extract the content within it and handle any child tags.

      LONG index = Status->TagIndex; // Remember the current tag position
      Status->Pos++;
      Status->Branch++;
      ERROR error = extract_content(Self, Status);
      Status->Branch--;

      if (!error) tag->Child = Self->Tags[index]; // Point our tag to the extract content
      else if (error != ERR_NoData) return error;

      XMLTag *child_content = tag->Child;
      while ((Status->Pos[0] IS '<') and (Status->Pos[1] != '/')) {
         index = Status->TagIndex; // Remember the current tag index before extract_tag() changes it

         Status->Branch++;
         error = extract_tag(Self, Status);
         Status->Branch--;

         if (error IS ERR_NothingDone) {
            // Extract any additional content caught in-between tags

            Status->Branch++;
            error = extract_content(Self, Status);
            Status->Branch--;

            if (!error) {
               child_content->Next = Self->Tags[index];
               child_content = Self->Tags[index];
            }
            else if (error != ERR_NoData) return error;
         }
         else if (!error) {
            if (!tag->Child) tag->Child = Self->Tags[index];
            if (child_content) child_content->Next = Self->Tags[index];
            child_content = Self->Tags[index];

            // Extract any new content caught in-between tags

            index = Status->TagIndex;
            Status->Branch++;
            error = extract_content(Self, Status);
            Status->Branch--;

            if (!error) {
               child_content->Next = Self->Tags[index];
               child_content = Self->Tags[index];
            }
            else if (error != ERR_NoData) return error;
         }
         else return ERR_Failed;
      }

      // There should be a closing tag - skip past it

      if ((Status->Pos[0] IS '<') and (Status->Pos[1] IS '/')) {
         Self->Balance--;
         while ((*Status->Pos) and (*Status->Pos != '>')) { if (*Status->Pos IS '\n') Self->LineNo++; Status->Pos++; }
      }

      if (*Status->Pos IS '>') Status->Pos++;
   }
   else {
      if ((Status->Pos[0] IS '/') and (Status->Pos[1] IS '>')) Status->Pos += 2;
      Self->Balance--;
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR extract_content(extXML *Self, exttag *Status)
{
   pf::Log log(__FUNCTION__);
   XMLTag *tag;
   STRING buffer;
   LONG i, len;

   // Skip whitespace - this will tell us if there is content or not.  If we do find some content, reset the marker to
   // the start of the content area because leading spaces may be important for content processing (e.g. for <pre> tags)

   CSTRING str = Status->Pos;
   if (!(Self->Flags & XMF_ALL_CONTENT)) {
      while ((*str) and (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
      if (*str != '<') str = Status->Pos;
   }

   // If the STRIP_CONTENT flag is set, we simply skip over the content and return a NODATA error code.

   if (Self->Flags & XMF_STRIP_CONTENT) {
      while ((*str) and (*str != '<')) { if (*str IS '\n') Self->LineNo++; str++; }
      Status->Pos = str;
      return ERR_NoData;
   }

   // Count size of the content and skip carriage returns (^M)

   for (i=0, len=0; (str[i]) and (str[i] != '<'); i++) {
      if (str[i] != '\r') len++;
   }

   if (len > 0) {
      if (!AllocMemory(sizeof(XMLTag) + Self->PrivateDataSize + sizeof(XMLAttrib) + len + 1,
            MEM_UNTRACKED|MEM_NO_CLEAR, &tag, NULL)) {

         ClearMemory(tag, sizeof(XMLTag) + Self->PrivateDataSize + sizeof(XMLAttrib));

         Self->Tags[Status->TagIndex] = tag;
         tag->Private     = (BYTE *)tag + sizeof(XMLTag);
         tag->Attrib      = (XMLAttrib *)((BYTE *)tag + sizeof(XMLTag) + Self->PrivateDataSize);
         tag->TotalAttrib = 1;
         tag->AttribSize  = len + 1;
         tag->Branch        = Status->Branch;

         buffer = (BYTE *)tag->Attrib + sizeof(XMLAttrib);

         tag->Attrib->Name  = NULL;
         tag->Attrib->Value = buffer;

         while ((*str) and (*str != '<')) {
            if (*str IS '\n') Self->LineNo++;
            if (*str != '\r') *buffer++ = *str++;
            else str++;
         }
         *buffer = 0;

         Status->TagIndex++;
         Status->Pos = str;
         return ERR_Okay;
      }
      else {
         while ((*str) and (*str != '<')) { if (*str IS '\n') Self->LineNo++; str++; } // Skip content
         Status->Pos = str;
         return ERR_AllocMemory;
      }
   }
   else {
      Status->Pos = str;
      return ERR_NoData;
   }
}

//********************************************************************************************************************
// Output an XML string with escape characters.

static LONG attrib_len(CSTRING String)
{
   LONG len = 0;
   if (String) {
      for (LONG j=0; String[j]; j++) {
         switch (String[j]) {
            case '&':  len += 5; break;
            case '<':  len += 4; break;
            case '>':  len += 4; break;
            case '"':  len += 6; break;
            //case '\'': len += 6; break;
            default:   len++; break;
         }
      }
   }
   return len;
}

static LONG output_attribvalue(CSTRING String, STRING Output)
{
   LONG i = 0;
   if ((String) and (Output)) {
      for (LONG j=0; String[j]; j++) {
         switch (String[j]) {
            case '&':  Output[i++] = '&'; Output[i++] = 'a'; Output[i++] = 'm'; Output[i++] = 'p'; Output[i++] = ';'; break;
            case '<':  Output[i++] = '&'; Output[i++] = 'l'; Output[i++] = 't'; Output[i++] = ';'; break;
            case '>':  Output[i++] = '&'; Output[i++] = 'g'; Output[i++] = 't'; Output[i++] = ';'; break;
            case '"':  Output[i++] = '&'; Output[i++] = 'q'; Output[i++] = 'u'; Output[i++] = 'o'; Output[i++] = 't'; Output[i++] = ';'; break;
            default:   Output[i++] = String[j]; break;
         }
      }
   }

   return i;
}

static LONG content_len(CSTRING String)
{
   LONG len = 0;
   if (String) {
      for (LONG j=0; String[j]; j++) {
         switch (String[j]) {
            case '&':  len += 5; break;
            case '<':  len += 4; break;
            case '>':  len += 4; break;
            default:   len++; break;
         }
      }
   }
   return len;
}

static LONG content_output(CSTRING String, STRING Output)
{
   LONG i = 0;
   if ((String) and (Output)) {
      for (LONG j=0; String[j]; j++) {
         switch (String[j]) {
            case '&':  Output[i++] = '&'; Output[i++] = 'a'; Output[i++] = 'm'; Output[i++] = 'p'; Output[i++] = ';'; break;
            case '<':  Output[i++] = '&'; Output[i++] = 'l'; Output[i++] = 't'; Output[i++] = ';'; break;
            case '>':  Output[i++] = '&'; Output[i++] = 'g'; Output[i++] = 't'; Output[i++] = ';'; break;
            default:   Output[i++] = String[j]; break;
         }
      }
   }

   return i;
}

//********************************************************************************************************************
// Converts XML data into its equivalent string.

XMLTag * build_xml_string(XMLTag *Tag, STRING Buffer, LONG Flags, LONG *Offset)
{
   pf::Log log("build_xml");
   XMLTag *xmltag;

   log.traceBranch("Index: %d, CurrentLength: %d", Tag->Index, *Offset);

   LONG offset = *Offset;

   // If the tag is a content string, copy out the data and return

   if (!Tag->Attrib->Name) {
      if (Tag->Attrib->Value) {
         if (Tag->CData) {
            if (!(Flags & XMF_STRIP_CDATA)) offset += StrCopy("<![CDATA[", Buffer+offset, COPY_ALL);
            offset += StrCopy(Tag->Attrib->Value, Buffer+offset, COPY_ALL);
            if (!(Flags & XMF_STRIP_CDATA)) offset += StrCopy("]]>", Buffer+offset, COPY_ALL);
         }
         else offset += content_output(Tag->Attrib->Value, Buffer+offset);
         Buffer[offset] = 0;
         *Offset = offset;
      }
      return Tag->Next;
   }

   // Output the Attrib assigned to this tag

   Buffer[offset++] = '<';

   LONG i;
   for (i=0; i < Tag->TotalAttrib; i++) {
      if (Tag->Attrib[i].Name) offset += output_attribvalue(Tag->Attrib[i].Name, Buffer+offset);

      if ((Tag->Attrib[i].Value) and (Tag->Attrib[i].Value[0])) {
         if (Tag->Attrib[i].Name) Buffer[offset++] = '=';
         Buffer[offset++] = '"';
         offset += output_attribvalue(Tag->Attrib[i].Value, Buffer+offset);
         Buffer[offset++] = '"';
      }
      if (i + 1 < Tag->TotalAttrib) Buffer[offset++] = ' ';
   }

   if (Tag->Instruction) {
      Buffer[offset++] = '?';
      Buffer[offset++] = '>';
      if (Flags & XMF_READABLE) Buffer[offset++] = '\n';
   }
   else if (Tag->Notation) {
      Buffer[offset++] = '>';
      if (Flags & XMF_READABLE) Buffer[offset++] = '\n';
   }
   else if ((xmltag = Tag->Child)) {
      Buffer[offset++] = '>';
      if (xmltag->Attrib->Name) Buffer[offset++] = '\n';

      *Offset = offset;
      while (xmltag) {
         xmltag = build_xml_string(xmltag, Buffer, Flags, Offset);
      }

      offset = *Offset;
      Buffer[offset++] = '<';
      Buffer[offset++] = '/';
      offset += output_attribvalue(Tag->Attrib->Name, Buffer+offset);
      Buffer[offset++] = '>';
      if (Flags & XMF_READABLE) Buffer[offset++] = '\n';
   }
   else {
      Buffer[offset++] = '/';
      Buffer[offset++] = '>';
      if (Flags & XMF_READABLE) Buffer[offset++] = '\n';
   }

   Buffer[offset] = 0;
   *Offset = offset;
   return Tag->Next;
}

//********************************************************************************************************************
// Determines the amount of bytes that would be required to write out an XML string.

XMLTag * len_xml_str(XMLTag *Tag, LONG Flags, LONG *Length)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Index: %d, CurrentLength: %d", Tag->Index, *Length);

   LONG length = *Length;

   // If the tag is a content string, copy out the data and return

   if (!Tag->Attrib->Name) {
      if (Tag->Attrib->Value) {
         if (Tag->CData) {
            length += 12; // <![CDATA[]]>
            LONG i;
            for (i=0; Tag->Attrib->Value[i]; i++) length++;
         }
         else length += content_len(Tag->Attrib->Value);
         *Length = length;
      }
      return Tag->Next;
   }

   // Output the attributes assigned to this tag

   length++; // <

   LONG i;
   for (i=0; i < Tag->TotalAttrib; i++) {
      LONG namelen = attrib_len(Tag->Attrib[i].Name);

      // Do a check just to ensure the integrity of the XML data.  Only notations can have nameless attributes
      if ((!namelen) and (!Tag->Notation)) {
         log.warning("Attribute %d in the tag at index %d is missing a defined name.", i, Tag->Index);
      }

      length += namelen;

      if ((Tag->Attrib[i].Value) and (Tag->Attrib[i].Value[0])) {
         if (namelen) length++; // =
         length++; // "
         length += attrib_len(Tag->Attrib[i].Value);
         length++; // "
      }

      if (i + 1 < Tag->TotalAttrib) length++; // Space
   }

   XMLTag *xmltag;

   if ((Tag->Attrib->Name[0] IS '?') or (Tag->Instruction)) {
      length += 2; // ?>
      if (Flags & XMF_READABLE) length++; // \n
   }
   else if (Tag->Notation) {
      length += 1; // >
      if (Flags & XMF_READABLE) length++; // \n
   }
   else if ((xmltag = Tag->Child)) {
      length++; // >
      if (xmltag->Attrib->Name) length++; // \n
      *Length = length;
      while (xmltag) xmltag = len_xml_str(xmltag, Flags, Length);
      length = *Length;
      length += 2; // </
      length += attrib_len(Tag->Attrib->Name);
      length += 1;    // >
      if (Flags & XMF_READABLE) length++; // \n
   }
   else {
      length += 2; // />
      if (Flags & XMF_READABLE) length++; // \n
   }

   *Length = length;

   return Tag->Next;
}

/*********************************************************************************************************************
** This function calls itself recursively to count all tags (including children) within a tag space.  ALL sibling tags
** are also included.
*/

static void tag_count(XMLTag *Tag, LONG *Count)
{
   while (Tag) {
      if (Tag->Child) tag_count(Tag->Child, Count);
      *Count = *Count + 1;
      Tag = Tag->Next;
   }
}

//********************************************************************************************************************

static void sift_down(ListSort **lookup, LONG Index, LONG heapsize)
{
   LONG largest = Index;
   do {
      Index = largest;
      LONG left	= (Index << 1) + 1;
      LONG right	= left + 1;

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

//********************************************************************************************************************

static void sift_up(ListSort **lookup, LONG i, LONG heapsize)
{
   LONG largest = i;
   do {
      i = largest;
      LONG left	= (i << 1) + 1;
      LONG right	= left + 1;

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

//********************************************************************************************************************
// Gets the nth sibling with the given name.

static XMLTag * next_sibling(extXML *Self, XMLTag *Tag, LONG Index, STRING Name, LONG FlatScan)
{
   //FMSG("next_sibling","Index: %d, Name: %s, Flat: %d", Index, Name, FlatScan);

   LONG flags = STR_MATCH_LEN;
   for (LONG i=0; Name[i]; i++) {
      if (Name[i] IS '*') {
         flags = STR_WILDCARD;
         break;
      }
   }

   while (Tag) {
      if ((FlatScan != -1) and (Tag->Branch < FlatScan)) return NULL;

      if ((Tag->Attrib->Name) and (!StrCompare(Name, Tag->Attrib->Name, 0, flags))) {
         if (!Index) return Tag;
         Index--;
      }

      if (FlatScan != -1) Tag = Self->Tags[Tag->Index+1];
      else Tag = Tag->Next;
   }

   return NULL;
}

/*********************************************************************************************************************
** XPath Query
**
** [0-9]  Used for indexing
** [#0-9] Presence of a plus will index against the tag array rather than the index in the tree (non-standard feature)
** '*'    For wild-carding of tag names
** '@'    An attribute
** '..'   Parent
** [=...] Match on encapsulated content (Not an XPath standard but we support it)
** //     Double-slash enables flat scanning of the XML tree.
**
** Round brackets may also be used as an alternative to square brackets.
**
** The use of \ as an escape character in attribute strings is supported, but keep in mind that this is not an official
** feature of the XPath standard.
*/

// Examples:
//   /menu/submenu
//   /menu[2]/window
//   /menu/window/@title
//   /menu/window[@title='foo']/...
//   /menu[=contentmatch]
//   /menu//window
//   /menu/window/* (First child of the window tag)
//   /menu/*[@id='5']

static ERROR find_tag2(extXML *Self, XMLTag **Tag, CSTRING XPath, CSTRING *Attrib, FUNCTION *Callback);

static XMLTag * find_tag(extXML *Self, XMLTag *Tag, CSTRING XPath, CSTRING *Attrib, FUNCTION *Callback)
{
   pf::Log log(__FUNCTION__);

   if (Attrib) *Attrib = NULL;

   // NB: If a callback is specified, the entire tree is scanned to the end.  The callback is called for each match
   // that is discovered.

   XMLTag *scan = Tag;
   ERROR error = find_tag2(Self, &scan, XPath, Attrib, Callback);

   if (Callback) return NULL;
   else if (!error) {
      if (Self->Flags & XMF_DEBUG) log.msg("Found tag %p #%d", scan, scan->Index);
      return scan;
   }
   else return NULL;
}

static ERROR find_tag2(extXML *Self, XMLTag **Tag, CSTRING XPath, CSTRING *Attrib, FUNCTION *Callback)
{
   pf::Log log("find_tag");
   char tagname[120];
   CSTRING filter_attrib_name;
   LONG pos, subscript, i, filter_attrib_name_len, filter_attrib_value_len, filter_attrib_svalue, attribwild, j;
   XMLTag *current;
   char endchar;

   if (!(current = *Tag)) return log.warning(ERR_Args);

   if ((!XPath[0]) or (XPath[0] != '/')) {
      log.warning("Missing '/' prefix in '%s'.", XPath);
      return ERR_StringFormat;
   }

   LONG flatscan = -1;
   if (XPath[0] IS '/') {
      if (XPath[1] IS '/') {
         pos = 2;
         flatscan = current->Branch;
         /*
         if (current->Next) flatscan = current->Next->Index;
         else if (current->Child) {
            flatscan = current->Child->Index;
            while (Self->Tags[flatscan].Level
         }*/
      }
      else pos = 1;
   }
   else pos = 0;

   // Parse the tag name

   if ((Attrib) and (XPath[pos] IS '@')) *Attrib = XPath + pos + 1;

   for (i=0; (((size_t)i < sizeof(tagname)-1) and (XPath[pos]) and
              (XPath[pos] != '/') and (XPath[pos] != '[') and (XPath[pos] != '(')); pos++) {
      tagname[i++] = XPath[pos];
   }
   tagname[i] = 0;

   if ((size_t)i >= sizeof(tagname)-1) {
      log.warning("Tag name in path > %d bytes: %s...", (LONG)sizeof(tagname), tagname);
      return ERR_BufferOverflow;
   }

   // Parse optional index or attribute filter

   filter_attrib_name      = NULL;
   filter_attrib_name_len  = 0;
   filter_attrib_svalue    = 0;
   filter_attrib_value_len = 0;
   attribwild              = STR_MATCH_LEN;
   LONG escattrib          = 0;

   if (Self->Flags & XMF_DEBUG) log.branch("%p, %s, XPath: %s, TagName: %s, Range: %d to %d", current, current->Attrib->Name, XPath, tagname, current->Index, flatscan);

   if ((XPath[pos] IS '[') or (XPath[pos] IS '(')) {
      if (XPath[pos] IS '[') endchar = ']';
      else endchar = ')';

      pos++;

      while ((XPath[pos]) and (XPath[pos] <= 0x20)) pos++;

      if ((XPath[pos] >= '0') and (XPath[pos] <= '9')) { // Parse index
         subscript = StrToInt(XPath+pos);
         while ((XPath[pos] >= '0') and (XPath[pos] <= '9')) pos++;
      }
      else if (XPath[pos] IS '#') { // Direct lookup into the tag array
         subscript = StrToInt(XPath+pos+1) + current->Index;
         if (subscript < Self->TagCount) {
            current = Self->Tags[subscript];
            subscript = -1;
         }
         else return log.warning(ERR_OutOfBounds);
      }
      else if ((XPath[pos] IS '@') or (XPath[pos] IS '=')) {
         subscript = -1;
         if (XPath[pos] IS '@') {
            pos++;

            // Parse filter attribute name

            filter_attrib_name = XPath + pos;
            while (((XPath[pos] >= 'a') and (XPath[pos] <= 'z')) or
                   ((XPath[pos] >= 'A') and (XPath[pos] <= 'Z')) or
                   (XPath[pos] IS '_')) pos++;

            filter_attrib_name_len = (XPath + pos) - filter_attrib_name;

            if (filter_attrib_name IS (XPath + pos)) goto parse_error; // Zero length string

            while ((XPath[pos]) and (XPath[pos] <= 0x20)) pos++; // Skip whitespace

            // Parse '='

            if (XPath[pos] != '=') goto parse_error;
            pos++;
         }
         else { // Skip '=' (indicates matching on content) The filter_attrib_name will be empty to indicate a content match is required.
            pos++;
         }

         while ((XPath[pos]) and (XPath[pos] <= 0x20)) pos++; // Skip whitespace

         // Parse value

         if ((XPath[pos] IS '\'') or (XPath[pos] IS '"')) {
            char quote = XPath[pos++];

            // Parse filter attribute value

            filter_attrib_svalue = pos;
            while ((XPath[pos]) and (XPath[pos] != quote)) {
               if (XPath[pos] IS '\\') {
                  char tchar;
                  if ((tchar = XPath[pos+1])) {
                     if ((tchar IS '*') or (tchar IS '\'')) {
                        pos++; // Escape character used - skip following character
                        escattrib++;
                     }
                  }
               }
               else if (XPath[pos] IS '*') attribwild = STR_WILDCARD;
               pos++;
            }
            filter_attrib_value_len = pos - filter_attrib_svalue;

            if (XPath[pos] != quote) goto parse_error; // Quote not terminated correctly
            pos++;
         }
         else {
            filter_attrib_svalue = pos;
            while ((XPath[pos]) and (XPath[pos] != endchar)) {
               if (XPath[pos] IS '*') attribwild = STR_WILDCARD;
               pos++;
            }
            filter_attrib_value_len = pos - filter_attrib_svalue;
         }
      }
      else goto parse_error;

      while ((XPath[pos]) and (XPath[pos] <= 0x20)) pos++; // Skip whitespace
      if (XPath[pos] != endchar) goto parse_error;
      pos++;
   }
   else subscript = -2; // No specific tag indicated, can scan all sibling tags in this section of the tree

next_sibling: // Start of loop - yes, we are using gotos for this

   if ((filter_attrib_name) or (filter_attrib_svalue)) {
      // Advance to the sibling that matches the filtered attribute or content string

      LONG tagwild = STR_MATCH_LEN;
      for (i=0; tagname[i]; i++) {
         if (tagname[i] IS '*') {
            tagwild = STR_WILDCARD;
            break;
         }
      }

      char attribvalue[filter_attrib_value_len + 1];
      CopyMemory((APTR)(XPath+filter_attrib_svalue), attribvalue, filter_attrib_value_len);
      attribvalue[filter_attrib_value_len] = 0;

      // If escape codes were present in the XPath, we need to perform a conversion process.

      if (escattrib) i = filter_attrib_svalue - escattrib + 1;
      else i = 1;

      char attrib_buffer[i];

      STRING attribval;
      if (escattrib) {
         attribval = attrib_buffer;
         for (i=0,j=0; attribvalue[i]; i++) {
            if ((attribvalue[i] IS '\\') and (attribvalue[i+1])) {
               i++;
               if (attribvalue[i] IS '*') attribval[j++] = '*';
               else if (attribvalue[i] IS '\'') attribval[j++] = '\'';
               else attribval[j++] = '\\';
               continue;
            }
            attribval[j++] = attribvalue[i];
         }
         attribval[j] = 0;
      }
      else attribval = attribvalue;

      if (filter_attrib_name) {
         while (current) {
            if ((current->Attrib->Name) and (!StrCompare(tagname, current->Attrib->Name, 0, tagwild))) {
               for (i=1; i < current->TotalAttrib; ++i) { // ignore name attribute, so start from index 1
                  if ((!StrCompare(current->Attrib[i].Name, filter_attrib_name, filter_attrib_name_len, 0)) and
                      (!StrCompare(current->Attrib[i].Value, attribval, 0, attribwild))) {
                     goto matched_attrib;
                  }
               }
            }

            if (flatscan != -1) {
               // Move to the next tag - notice that the code is a little complex because we check the integrity of the
               // tag indexes (if an index is wrong, it means we get stuck in a loop).

               LONG index = current->Index + 1;
               current = Self->Tags[index];
               if ((current) and (current->Branch < flatscan)) {
                  current = NULL;
                  break;
               }

               if ((current) and (current->Index != index)) {
                  log.warning("Corrupt tag or incorrect reference in Tags array at index %d (tag has index of %d).", index, current->Index);
                  break;
               }
            }
            else current = current->Next;
         }
      }
      else while (current) {
         if ((current->Attrib->Name) and (!StrCompare(tagname, current->Attrib->Name, 0, tagwild))) {
            // Match on content
            if ((current->Child) and (!current->Child->Attrib->Name)) {
               if (!StrCompare(current->Child->Attrib->Value, attribval, 0, attribwild)) {
                  goto matched_attrib;
               }
            }
         }

         if (flatscan != -1) {
            // Move to the next tag - notice that the code is a little complex because we check the integrity of the
            // tag indexes (if an index is wrong, it means we get stuck in a loop).

            LONG index = current->Index + 1;
            current = Self->Tags[index];

            if ((current) and (current->Branch < flatscan)) {
               current = NULL;
               break;
            }

            if ((current) and (current->Index != index)) {
               log.warning("Corrupt tag or incorrect reference in Tags array at index %d (tag has index of %d).", index, current->Index);
               break;
            }
         }
         else current = current->Next;
      }
   }
   else current = next_sibling(Self, current, (subscript >= 0) ? subscript : 0, tagname, flatscan);

matched_attrib:
   if (!current) return ERR_Search;

   XMLTag *scan;
   if (!XPath[pos]) { // Matching tag found and there is nothing left to process
      if (!Callback) {
         *Tag = current;
         return ERR_Okay; // End of query reached, successfully found tag
      }

      ERROR error = ERR_Okay;
      if (Callback->Type IS CALL_STDC) {
         auto routine = (ERROR (*)(extXML *, XMLTag *, CSTRING))Callback->StdC.Routine;
         error = routine(Self, current, NULL);
      }
      else if (Callback->Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         if ((script = Callback->Script.Script)) {
            const ScriptArg args[] = {
               { "XML",  FD_OBJECTPTR, { .Address = Self } },
               { "Tag",  FD_LONG,      { .Long = current->Index } },
               { "Attrib", FD_STRING,  { .Address = NULL } }
            };
            if (scCallback(script, Callback->Script.ProcedureID, args, ARRAYSIZE(args), &error)) error = ERR_Terminate;
         }
      }
      else error = ERR_InvalidValue;

      if (error IS ERR_Terminate) {
         *Tag = current;
         return ERR_Terminate;
      }

      if (((subscript IS -2) or (subscript IS -1)) and ((current = current->Next))) goto next_sibling;

      return error;
   }
   else if ((XPath[pos] IS '/') and (XPath[pos+1] IS '@')) {
      if (Attrib) *Attrib = XPath + pos + 2;

      if (!Callback) {
         *Tag = current;
         return ERR_Okay;
      }

      ERROR error = ERR_Okay;
      if (Callback->Type IS CALL_STDC) {
         auto routine = (ERROR (*)(extXML *, XMLTag *, CSTRING))Callback->StdC.Routine;
         error = routine(Self, current, NULL);
      }
      else if (Callback->Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         if ((script = Callback->Script.Script)) {
            const ScriptArg args[] = {
               { "XML",  FD_OBJECTPTR, { .Address = Self } },
               { "Tag",  FD_LONG,      { .Long = current->Index } },
               { "Attrib", FD_STRING,  { .Address = (STRING)(Attrib ? Attrib[0] : NULL) } }
            };
            if (scCallback(script, Callback->Script.ProcedureID, args, ARRAYSIZE(args), &error)) error = ERR_Terminate;
            ReleaseObject(script);
         }
      }
      else error = ERR_InvalidValue;

      if (error IS ERR_Terminate) {
         *Tag = current;
         return ERR_Terminate;
      }

      if (((subscript IS -2) or (subscript IS -1)) and ((current = current->Next))) goto next_sibling;

      return error;
   }
   else if ((scan = current->Child)) { // Move to next position in the XPath and scan child node
      ERROR error = find_tag2(Self, &scan, XPath+pos, Attrib, Callback);

      if (error IS ERR_Terminate) {
         *Tag = current;
         return ERR_Terminate;
      }

      if ((error) or (Callback)) {
         // Nothing matches in this subset of tags, or callbacks are in use.  Move to the next sibling if subscripts
         // are not being used.

         if (subscript < 0) {
            current = current->Next;
            goto next_sibling;
         }
      }
      else *Tag = scan;

      return error;
   }
   else return ERR_Search;

parse_error:
   log.msg("XPath unresolved: %s", XPath);
   return ERR_Search;
}

//********************************************************************************************************************

static ERROR parse_source(extXML *Self)
{
   pf::Log log(__FUNCTION__);
   CacheFile *filecache;

   log.traceBranch("");

   // Although the file will be uncached as soon it is loaded, the developer can pre-cache XML files with his own
   // call to LoadFile(), which can lead our use of LoadFile() to being quite effective.

   if (Self->Source) {
      char *buffer;
      LARGE size = 64 * 1024;
      if (!AllocMemory(size+1, MEM_STRING|MEM_NO_CLEAR, &buffer)) {
         LONG pos = 0;
         Self->ParseError = ERR_Okay;
         acSeekStart(Self->Source, 0);
         while (1) {
            LONG result;
            if (acRead(Self->Source, buffer+pos, size-pos, &result)) {
               Self->ParseError = ERR_Read;
               break;
            }
            else if (result <= 0) break;

            pos += result;
            if (pos >= size-1024) {
               if (ReallocMemory(buffer, (size * 2) + 1, &buffer, NULL)) {
                  Self->ParseError = ERR_ReallocMemory;
                  break;
               }
               size = size * 2;
            }
         }

         if (!Self->ParseError) {
            buffer[pos] = 0;
            Self->ParseError = txt_to_xml(Self, buffer);
         }

         FreeResource(buffer);
      }
      else Self->ParseError = ERR_AllocMemory;
   }
   else if (!LoadFile(Self->Path, 0, &filecache)) {
      Self->ParseError = txt_to_xml(Self, (CSTRING)filecache->Data);
      UnloadFile(filecache);
   }
   else Self->ParseError = ERR_File;

   return Self->ParseError;
}

//********************************************************************************************************************
// Extracts immediate content, does not recurse into child tags.

static ERROR get_content(extXML *Self, XMLTag *Tag, STRING Buffer, LONG Size)
{
   Buffer[0] = 0;
   if ((Tag = Tag->Child)) {
      LONG j = 0;
      while (Tag) {
         if ((!Tag->Attrib->Name) and (Tag->Attrib->Value)) {
            j += StrCopy(Tag->Attrib->Value, Buffer+j, Size-j);
            if (j >= Size) break;
         }
         Tag = Tag->Next;
      }

      if (j >= Size) {
         pf::Log log(__FUNCTION__);
         return log.warning(ERR_BufferOverflow);
      }
   }

   return ERR_Okay;
}

//********************************************************************************************************************

static void free_xml(extXML *Self)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }
   if (Self->Statement) { FreeResource(Self->Statement); Self->Statement = NULL; }
   clear_tags(Self);
   if (Self->Tags) { FreeResource(Self->Tags); Self->Tags = NULL; }
}

//**********************************************************************

static void clear_tags(extXML *XML)
{
   for (LONG i=0; i < XML->TagCount; i++) {
      if (XML->Tags[i]) FreeResource(XML->Tags[i]);
   }
   if (XML->Tags) XML->Tags[0] = NULL; // Don't free the array, just null terminate it
   XML->TagCount = 0;
}

//********************************************************************************************************************

#warning TODO: Support processing of ENTITY declarations in the doctype.

static void parse_doctype(extXML *Self, CSTRING Input)
{

}
