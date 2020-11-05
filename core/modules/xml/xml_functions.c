
static ERROR count_tags(objXML *Self, CSTRING Text, CSTRING *Result)
{
   if (*Text != '<') {
      LogF("@CountTags:","Malformed XML statement detected.");
      return ERR_InvalidData;
   }
   else Text++;

   // In a CDATA section, everything is skipped up to the ]]>
   // termination point.

   if (!StrCompare("![CDATA[", Text, 8, STR_MATCH_CASE)) {
      Text += 8;
      while (*Text) {
         if ((Text[0] IS ']') AND (Text[1] IS ']') AND (Text[2] IS '>')) {
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
         if ((Text[0] IS '<') AND (Text[1] IS '!') AND (Text[2] IS '[') AND ((Text[3] IS 'N') OR (Text[3] IS 'C')) AND (Text[4] IS 'D') AND (Text[5] IS 'A') AND (Text[6] IS 'T') AND (Text[7] IS 'A') AND (Text[8] IS '[')) {
            nest++;
            Text += 8;
         }
         else if ((Text[0] IS ']') AND (Text[1] IS ']') AND (Text[2] IS '>')) {
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

   if ((Text[0] IS '!') AND (Text[1] IS '-') AND (Text[2] IS '-')) {
      Text += 3;
      while (*Text) {
         if ((Text[0] IS '-') AND (Text[1] IS '-') AND (Text[2] IS '>')) {
            Text += 3;
            if (Self->Flags & XMF_INCLUDE_COMMENTS) Self->TagCount++;
            *Result = Text;
            return ERR_Okay;
         }
         Text++;
      }

      LogF("@count_tags","Unterminated comment detected.");
      return ERR_InvalidData;
   }

   // Skip past the tag's attributes

   CSTRING str = Text;
   while ((*str) AND (*str != '>')) {
      if ((str[0] IS '/') AND (str[1] IS '>')) break;

      skipwhitespace(str);

      if ((!*str) OR (*str IS '>') OR ((*str IS '/') AND (str[1] IS '>')) OR (*str IS '=')) break;

      while ((*str > 0x20) AND (*str != '>') AND (*str != '=')) { // Tag name
         if ((str[0] IS '/') AND (str[1] IS '>')) break;
         str++;
      }

      skipwhitespace(str);

      if (*str IS '=') {
         str++;
         skipwhitespace(str);
         if (*str IS '"') {
            str++;
            while ((*str) AND (*str != '"')) str++;
            if (*str IS '"') str++;
         }
         else if (*str IS '\'') {
            str++;
            while ((*str) AND (*str != '\'')) str++;
            if (*str IS '\'') str++;
         }
         else while ((*str > 0x20) AND (*str != '>')) {
            if ((str[0] IS '/') AND (str[1] IS '>')) break;
            str++;
         }
      }
      else if (*str IS '"') { // Notation attributes don't have names
         str++;
         while ((*str) AND (*str != '"')) str++;
         if (*str IS '"') str++;
      }
   }

   if ((*str IS '>') AND (*Text != '!') AND (*Text != '?')) {
      // The tag is open.  Scan the content within it and handle any child tags.

      str++;
      if (!(Self->Flags & XMF_ALL_CONTENT)) skipwhitespace(str);
      if (*str != '<') {
         while ((*str) AND (*str != '<')) str++;
         if (!(Self->Flags & XMF_STRIP_CONTENT)) Self->TagCount++; // A content tag will be required
      }

      while ((str[0] IS '<') AND (str[1] != '/')) {
         ERROR error;
         if (!(error = count_tags(Self, str, &str))) {
            if (!(Self->Flags & XMF_ALL_CONTENT)) skipwhitespace(str);
            if (*str != '<') {
               while ((*str) AND (*str != '<')) str++;
               if (!(Self->Flags & XMF_STRIP_CONTENT)) Self->TagCount++; // An embedded content tag will be required
            }
         }
         else return error;
      }

      // There should be a closing tag - skip past it

      if ((str[0] IS '<') AND (str[1] IS '/')) {
         while ((*str) AND (*str != '>')) str++;
      }

      if (*str IS '>') str++;
   }
   else if ((str[0] IS '/') AND (str[1] IS '>')) str += 2;

   if ((Self->Flags & XMF_STRIP_HEADERS) AND ((*Text IS '?') OR (*Text IS '!'))); // Ignore headers (no tag count increase)
   else Self->TagCount++;

   *Result = str;

   return ERR_Okay;
}

//****************************************************************************
// Convert a text string into XML tags.

static ERROR txt_to_xml(objXML *Self, CSTRING Text)
{
   if ((!Self) OR (!Text)) return ERR_NullArgs;

   Self->Balance = 0;
   Self->LineNo = 1;

   clear_tags(Self);  // Kill any existing tags in this XML object

   // Perform a count of the total amount of tags specified (closing tags excluded)

   CSTRING str;
   for (str=Text; (*str) AND (*str != '<'); str++);
   while ((str[0] IS '<') AND (str[1] != '/')) {
      if (count_tags(Self, str, &str) != ERR_Okay) {
         LogErrorMsg("Aborting XML interpretation process.");
         return ERR_InvalidData;
      }
      while ((*str) AND (*str != '<')) str++;
   }

   if (Self->TagCount < 1) {
      LogErrorMsg("There are no valid tags in the XML statement.");
      return ERR_NoData;
   }

   MSG("Detected %d raw and content based tags, options $%.8x.", Self->TagCount, Self->Flags);

   // Allocate an array to hold all of the XML tags

   struct XMLTag **tag;
   if (AllocMemory(sizeof(APTR) * (Self->TagCount + 1), MEM_DATA|MEM_UNTRACKED, &tag, NULL) != ERR_Okay) {
      return ERR_AllocMemory;
   }

   FreeResource(Self->Tags);
   Self->Tags = tag;

   // Extract the tag information.  This loop will extract the top-level tags.  The extract_tag() function is recursive
   // to extract the child tags.

   MSG("Extracting tag information with extract_tag()");

   struct exttag ext = { .Start = Text, .TagIndex = 0, .Branch = 0 };
   struct XMLTag *prevtag = NULL;
   for (str=Text; (*str) AND (*str != '<'); str++) if (*str IS '\n') Self->LineNo++;
   ext.Pos = str;
   while ((ext.Pos[0] IS '<') AND (ext.Pos[1] != '/')) {
      LONG i = ext.TagIndex; // Remember the current tag index before extract_tag() changes it

      ERROR error = extract_tag(Self, &ext);

      if ((error != ERR_Okay) AND (error != ERR_NothingDone)) {
         LogErrorMsg("Aborting XML interpretation process.");
         return ERR_InvalidData;
      }

      // Skip content/whitespace to get to the next tag
      str = ext.Pos;
      while ((*str) AND (*str != '<')) { if (*str IS '\n') Self->LineNo++; str++; }
      ext.Pos = str;

      if (error IS ERR_NothingDone) continue;

      if (prevtag) prevtag->Next = Self->Tags[i];
      prevtag = Self->Tags[i];
   }

   // If the XML statement contained errors such as unclosed tags, the tag count may be greater than the actual number of tags
   // loaded.  This routine checks that the expected tag count matches what was extracted.

   LONG tagindex;
   for (tagindex=0; tagindex < Self->TagCount; tagindex++) {
      if (!Self->Tags[tagindex]) {
         if (Self->Flags & XMF_WELL_FORMED) return PostError(ERR_UnbalancedXML);

         LogErrorMsg("Non-fatal error - %d tags expected, loaded %d.", Self->TagCount, tagindex);
         if (tagindex > 0) {
            Self->Tags[tagindex-1]->Next = NULL;
            Self->Tags[tagindex-1]->Child = NULL;
         }
         Self->Tags[tagindex] = 0;
         Self->TagCount = tagindex;
         break;
      }
   }

   // If the WELL_FORMED flag has been used, check that the tags balance.  If they don't then return ERR_InvalidData.

   if (Self->Flags & XMF_WELL_FORMED) {
      if (Self->Balance != 0) {
         return PostError(ERR_UnbalancedXML);
      }
   }

   // Set the Prev and Index fields

   LONG i;
   for (i=0; i < Self->TagCount; i++) {
      Self->Tags[i]->Index = i;
      if (Self->Tags[i]->Next) Self->Tags[i]->Next->Prev = Self->Tags[i];
   }

   // Upper/lowercase transformations

   if (Self->Flags & XMF_UPPER_CASE) {
      MSG("Performing uppercase translations.");
      STRING str;
      for (i=0; i < Self->TagCount; i++) {
         LONG j;
         for (j=0; j < Self->Tags[i]->TotalAttrib; j++) {
            if ((str = Self->Tags[i]->Attrib[j].Name)) {
               while (*str) { if ((*str >= 'a') AND (*str <= 'z')) *str = *str - 'a' + 'A'; str++; }
            }
            if ((str = Self->Tags[i]->Attrib[j].Value)) {
               while (*str) { if ((*str >= 'a') AND (*str <= 'z')) *str = *str - 'a' + 'A'; str++; }
            }
         }
      }
   }
   else if (Self->Flags & XMF_LOWER_CASE) {
      MSG("Performing lowercase translations.");
      for (i=0; i < Self->TagCount; i++) {
         LONG j;
         for (j=0; j < Self->Tags[i]->TotalAttrib; j++) {
            STRING str;
            if ((str = Self->Tags[i]->Attrib[j].Name)) {
               while (*str) { if ((*str >= 'A') AND (*str <= 'Z')) *str = *str - 'A' + 'a'; str++; }
            }
            if ((str = Self->Tags[i]->Attrib[j].Value)) {
               while (*str) { if ((*str >= 'A') AND (*str <= 'Z')) *str = *str - 'A' + 'a'; str++; }
            }
         }
      }
   }

   if (!(Self->Flags & XMF_NO_ESCAPE)) {
      MSG("Unescaping XML.");
      LONG i, j;
      for (i=0; i < Self->TagCount; i++) {
         for (j=0; j < Self->Tags[i]->TotalAttrib; j++) {
            if (!Self->Tags[i]->Attrib[j].Value) continue;
            if (Self->Tags[i]->CData) continue;
            xml_unescape(Self, Self->Tags[i]->Attrib[j].Value);
         }
      }
   }

   MSG("XML parsing complete.");
   return ERR_Okay;
}

//****************************************************************************
// Extracts the next tag from an XML string.  This function also recurses into itself.

static CSTRING extract_tag_attrib(objXML *Self, CSTRING Str, LONG *AttribSize, WORD *TotalAttrib)
{
   CSTRING str = Str;
   LONG size = 0;
   while ((*str) AND (*str != '>')) {
      if ((str[0] IS '/') AND (str[1] IS '>')) break; // Termination checks
      if ((str[0] IS '?') AND (str[1] IS '>')) break;

      while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
      if ((*str IS 0) OR (*str IS '>') OR (((*str IS '/') OR (*str IS '?')) AND (str[1] IS '>'))) break;

      if (*str IS '=') return NULL; // Check for invalid XML

      if (*str IS '"') { // Notation values can start with double quotes and have no name.
         str++;
         while ((*str) AND (*str != '"')) { size++; str++; }
         if (*str IS '"') str++;
         size++; // String termination byte
      }
      else {
         while ((*str > 0x20) AND (*str != '>') AND (*str != '=')) {
            if ((str[0] IS '/') AND (str[1] IS '>')) break;
            if ((str[0] IS '?') AND (str[1] IS '>')) break;
            str++; size++;
         }
         size++; // String termination byte

         while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }

         if (*str IS '=') {
            str++;
            while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
            if (*str IS '"') {
               str++;
               while ((*str) AND (*str != '"')) { if (*str IS '\n') Self->LineNo++; str++; size++; }
               if (*str IS '"') str++;
            }
            else if (*str IS '\'') {
               str++;
               while ((*str) AND (*str != '\'')) { if (*str IS '\n') Self->LineNo++; str++; size++; }
               if (*str IS '\'') str++;
            }
            else while ((*str > 0x20) AND (*str != '>')) {
               if ((str[0] IS '/') AND (str[1] IS '>')) break;
               if ((str[0] IS '?') AND (str[1] IS '>')) break;
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

//****************************************************************************
// Called by txt_to_xml() to extract the next tag from an XML string.  This function also recurses into itself.

static ERROR extract_tag(objXML *Self, struct exttag *Status)
{
   FMSG("extract_tag()","Index: %d, Level: %d, %.30s", Status->TagIndex, Status->Branch, Status->Pos);

   if (Status->Pos[0] != '<') {
      LogErrorMsg("Malformed XML statement detected.");
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
            LogErrorMsg("Detected malformed comment (missing --> terminator).");
            return ERR_InvalidData;
         }
      }
   }

   // Check that the tag index does not exceed the total number of calculated tags

   if (Status->TagIndex >= Self->TagCount) {
      LogErrorMsg("Ran out of array space for tag extraction (expected %d tags).", Status->TagIndex);
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
            if ((str[len] IS ']') AND (str[len+1] IS ']') AND (str[len+2] IS '>')) {
               break;
            }
            else if (str[len] IS '\n') Self->LineNo++;
         }
      }
      else if (raw_content IS 2) {
         UWORD nest = 1;
         for (len=0; str[len]; len++) {
            if ((str[len] IS '<') AND (str[len+1] IS '!') AND (str[len+2] IS '[') AND
                ((str[len+3] IS 'N') OR (str[len+3] IS 'C')) AND (str[len+4] IS 'D') AND (str[len+5] IS 'A') AND (str[len+6] IS 'T') AND (str[len+7] IS 'A')  AND (str[len+8] IS '[')) {
               nest++;
               len += 7;
            }
            else if ((str[len] IS ']') AND (str[len+1] IS ']') AND (str[len+2] IS '>')) {
               nest--;
               if (!nest) break;
            }
            else if (str[len] IS '\n') Self->LineNo++;
         }
      }

      // CDATA counts as content and therefore can be stripped out

      if ((Self->Flags & XMF_STRIP_CONTENT) OR (!len)) {
         Status->Pos = str + len + 3;
         return ERR_NothingDone;
      }

      if (!str[len]) {
         LogErrorMsg("Malformed XML:  A CDATA section is missing its closing string.");
         return ERR_InvalidData;
      }

      // CDATA sections are assimilated into the parent tag as content

      struct XMLTag *tag;
      if (!AllocMemory(sizeof(struct XMLTag) + Self->PrivateDataSize + sizeof(struct XMLAttrib) + len + 1,
            MEM_UNTRACKED|MEM_NO_CLEAR, &tag, NULL)) {

         ClearMemory(tag, sizeof(struct XMLTag) + Self->PrivateDataSize + sizeof(struct XMLAttrib));

         Self->Tags[Status->TagIndex] = tag;
         tag->Private     = ((APTR)tag) + sizeof(struct XMLTag);
         tag->Attrib      = ((APTR)tag) + sizeof(struct XMLTag) + Self->PrivateDataSize;
         tag->TotalAttrib = 1;
         tag->ID          = glTagID++;
         tag->AttribSize  = len + 1;
         tag->CData       = TRUE;
         tag->Branch      = Status->Branch;
         tag->LineNo      = line_no;

         STRING buffer = ((APTR)tag->Attrib) + sizeof(struct XMLAttrib);

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
   if (!str) return PostError(ERR_InvalidData);

   if ((Status->Pos[1] IS '?') OR (Status->Pos[1] IS '!')) {
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
      LogErrorMsg("Failed to extract a tag from \"%.10s\" (offset %d), index %d, nest %d.", Status->Pos, (LONG)(Status->Pos - Status->Start), Status->TagIndex, Status->Branch);
      return ERR_InvalidData;
   }

   struct XMLTag *tag;
   if (AllocMemory(sizeof(struct XMLTag) + Self->PrivateDataSize + (sizeof(struct XMLAttrib) * totalattrib) + attribsize, MEM_UNTRACKED, &tag, NULL) != ERR_Okay) {
      return PostError(ERR_AllocMemory);
   }

   tag->Private     = ((APTR)tag) + sizeof(struct XMLTag);
   tag->Attrib      = ((APTR)tag) + sizeof(struct XMLTag) + Self->PrivateDataSize;
   tag->TotalAttrib = totalattrib;
   tag->AttribSize  = attribsize;
   tag->ID          = glTagID++;
   tag->Branch      = Status->Branch;
   tag->LineNo      = line_no;

   Self->Tags[Status->TagIndex] = tag;
   Status->TagIndex++;
   Self->Balance++;

   // Extract all attributes within the tag

   STRING buffer = ((APTR)tag->Attrib) + (sizeof(struct XMLAttrib) * tag->TotalAttrib);
   str    = Status->Pos+1;
   if (*str IS '?') tag->Instruction = TRUE; // Detect <?xml ?> style instruction elements.
   else if ((*str IS '!') AND (str[1] >= 'A') AND (str[1] <= 'Z')) tag->Notation = TRUE;

   LONG a = 0;
   while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
   while ((*str) AND (*str != '>')) {
      if ((str[0] IS '/') AND (str[1] IS '>')) break; // Termination checks
      if ((str[0] IS '?') AND (str[1] IS '>')) break;

      if (*str IS '=') return PostError(ERR_InvalidData);

      if (a >= tag->TotalAttrib) return PostError(ERR_BufferOverflow);

      // Extract the name of the attribute

      if (*str IS '"') { // Quoted notation attributes are parsed as values instead
         tag->Attrib[a].Name = NULL;
      }
      else {
         tag->Attrib[a].Name = buffer;
         while ((*str > 0x20) AND (*str != '>') AND (*str != '=')) {
            if ((str[0] IS '/') AND (str[1] IS '>')) break;
            if ((str[0] IS '?') AND (str[1] IS '>')) break;
            *buffer++ = *str++;
         }
         *buffer++ = 0;
      }

      // Extract the attributes value

      while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }

      if (*str IS '=') {
         str++;
         while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
         tag->Attrib[a].Value = buffer;
         if (*str IS '"') {
            str++;
            while ((*str) AND (*str != '"')) { if (*str IS '\n') Self->LineNo++; *buffer++ = *str++; }
            if (*str IS '"') str++;
         }
         else if (*str IS '\'') {
            str++;
            while ((*str) AND (*str != '\'')) { if (*str IS '\n') Self->LineNo++; *buffer++ = *str++; }
            if (*str IS '\'') str++;
         }
         else {
            while ((*str > 0x20) AND (*str != '>')) {
               if ((str[0] IS '/') AND (str[1] IS '>')) break;
               *buffer++ = *str++;
            }
         }

         *buffer++ = 0;
      }
      else if ((!tag->Attrib[a].Name) AND (*str IS '"')) { // Detect notation value with no name
         tag->Attrib[a].Value = buffer;
         str++;
         while ((*str) AND (*str != '"')) { if (*str IS '\n') Self->LineNo++; *buffer++ = *str++; }
         if (*str IS '"') str++;
         *buffer++ = 0;
      }

      a++;
      while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
   }

   Status->Pos = str;

   if ((*Status->Pos IS '>') AND (tag->Attrib->Name[0] != '!') AND (tag->Attrib->Name[0] != '?')) {
      // We reached the end of an unclosed tag.  Extract the content within it and handle any child tags.

      LONG index = Status->TagIndex; // Remember the current tag position
      Status->Pos++;
      Status->Branch++;
      ERROR error = extract_content(Self, Status);
      Status->Branch--;

      if (!error) tag->Child = Self->Tags[index]; // Point our tag to the extract content
      else if (error != ERR_NoData) return error;

      struct XMLTag *child_content = tag->Child;
      while ((Status->Pos[0] IS '<') AND (Status->Pos[1] != '/')) {
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

      if ((Status->Pos[0] IS '<') AND (Status->Pos[1] IS '/')) {
         Self->Balance--;
         while ((*Status->Pos) AND (*Status->Pos != '>')) { if (*Status->Pos IS '\n') Self->LineNo++; Status->Pos++; }
      }

      if (*Status->Pos IS '>') Status->Pos++;
   }
   else {
      if ((Status->Pos[0] IS '/') AND (Status->Pos[1] IS '>')) Status->Pos += 2;
      Self->Balance--;
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR extract_content(objXML *Self, struct exttag *Status)
{
   struct XMLTag *tag;
   STRING buffer;
   LONG i, len;

   // Skip whitespace - this will tell us if there is content or not.  If we do find some content, reset the marker to
   // the start of the content area because leading spaces may be important for content processing (e.g. for <pre> tags)

   CSTRING str = Status->Pos;
   if (!(Self->Flags & XMF_ALL_CONTENT)) {
      while ((*str) AND (*str <= 0x20)) { if (*str IS '\n') Self->LineNo++; str++; }
      if (*str != '<') str = Status->Pos;
   }

   // If the STRIP_CONTENT flag is set, we simply skip over the content and return a NODATA error code.

   if (Self->Flags & XMF_STRIP_CONTENT) {
      while ((*str) AND (*str != '<')) { if (*str IS '\n') Self->LineNo++; str++; }
      Status->Pos = str;
      return ERR_NoData;
   }

   // Count size of the content and skip carriage returns (^M)

   for (i=0, len=0; (str[i]) AND (str[i] != '<'); i++) {
      if (str[i] != '\r') len++;
   }

   if (len > 0) {
      if (!AllocMemory(sizeof(struct XMLTag) + Self->PrivateDataSize + sizeof(struct XMLAttrib) + len + 1,
            MEM_UNTRACKED|MEM_NO_CLEAR, &tag, NULL)) {

         ClearMemory(tag, sizeof(struct XMLTag) + Self->PrivateDataSize + sizeof(struct XMLAttrib));

         Self->Tags[Status->TagIndex] = tag;
         tag->Private     = ((APTR)tag) + sizeof(struct XMLTag);
         tag->Attrib      = ((APTR)tag) + sizeof(struct XMLTag) + Self->PrivateDataSize;
         tag->TotalAttrib = 1;
         tag->AttribSize  = len + 1;
         tag->Branch        = Status->Branch;

         buffer = ((APTR)tag->Attrib) + sizeof(struct XMLAttrib);

         tag->Attrib->Name  = NULL;
         tag->Attrib->Value = buffer;

         while ((*str) AND (*str != '<')) {
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
         while ((*str) AND (*str != '<')) { if (*str IS '\n') Self->LineNo++; str++; } // Skip content
         Status->Pos = str;
         return ERR_AllocMemory;
      }
   }
   else {
      Status->Pos = str;
      return ERR_NoData;
   }
}

//****************************************************************************
// Output an XML string with escape characters.

static LONG attrib_len(CSTRING String)
{
   LONG len = 0;
   if (String) {
      LONG j;
      for (j=0; String[j]; j++) {
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
   if ((String) AND (Output)) {
      LONG j;
      for (j=0; String[j]; j++) {
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
      LONG j;
      for (j=0; String[j]; j++) {
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
   if ((String) AND (Output)) {
      LONG j;
      for (j=0; String[j]; j++) {
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

//****************************************************************************
// Converts XML data into its equivalent string.

struct XMLTag * build_xml_string(struct XMLTag *Tag, STRING Buffer, LONG Flags, LONG *Offset)
{
   struct XMLTag *xmltag;

   FMSG("~build_xml()","Index: %d, CurrentLength: %d", Tag->Index, *Offset);

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
      STEP();
      return Tag->Next;
   }

   // Output the Attrib assigned to this tag

   Buffer[offset++] = '<';

   LONG i;
   for (i=0; i < Tag->TotalAttrib; i++) {
      if (Tag->Attrib[i].Name) offset += output_attribvalue(Tag->Attrib[i].Name, Buffer+offset);

      if ((Tag->Attrib[i].Value) AND (Tag->Attrib[i].Value[0])) {
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
   STEP();
   return Tag->Next;
}

//****************************************************************************
// Determines the amount of bytes that would be required to write out an XML string.

struct XMLTag * len_xml_str(struct XMLTag *Tag, LONG Flags, LONG *Length)
{
   FMSG("~len_xml_str()","Index: %d, CurrentLength: %d", Tag->Index, *Length);

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
      STEP();
      return Tag->Next;
   }

   // Output the attributes assigned to this tag

   length++; // <

   LONG i;
   for (i=0; i < Tag->TotalAttrib; i++) {
      LONG namelen = attrib_len(Tag->Attrib[i].Name);

      // Do a check just to ensure the integrity of the XML data.  Only notations can have nameless attributes
      if ((!namelen) AND (!Tag->Notation)) {
         LogErrorMsg("Attribute %d in the tag at index %d is missing a defined name.", i, Tag->Index);
      }

      length += namelen;

      if ((Tag->Attrib[i].Value) AND (Tag->Attrib[i].Value[0])) {
         if (namelen) length++; // =
         length++; // "
         length += attrib_len(Tag->Attrib[i].Value);
         length++; // "
      }

      if (i + 1 < Tag->TotalAttrib) length++; // Space
   }

   struct XMLTag *xmltag;

   if ((Tag->Attrib->Name[0] IS '?') OR (Tag->Instruction)) {
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

   STEP();
   return Tag->Next;
}

/*****************************************************************************
** This function calls itself recursively to count all tags (including children) within a tag space.  ALL sibling tags
** are also included.
*/

static void tag_count(struct XMLTag *Tag, LONG *Count)
{
   while (Tag) {
      if (Tag->Child) tag_count(Tag->Child, Count);
      *Count = *Count + 1;
      Tag = Tag->Next;
   }
}

//****************************************************************************

static void sift_down(struct ListSort **lookup, LONG Index, LONG heapsize)
{
   LONG largest = Index;
   do {
      Index = largest;
      LONG left	= (Index << 1) + 1;
      LONG right	= left + 1;

      if (left < heapsize){
         if (StrSortCompare(lookup[largest]->String, lookup[left]->String) > 0) largest = left;

         if (right < heapsize) {
            if (StrSortCompare(lookup[largest]->String, lookup[right]->String) > 0) largest = right;
         }
      }

      if (largest != Index) {
         struct ListSort *temp = lookup[Index];
         lookup[Index] = lookup[largest];
         lookup[largest] = temp;
      }
   } while (largest != Index);
}

//****************************************************************************

static void sift_up(struct ListSort **lookup, LONG i, LONG heapsize)
{
   LONG largest = i;
   do {
      i = largest;
      LONG left	= (i << 1) + 1;
      LONG right	= left + 1;

      if (left < heapsize){
         if (StrSortCompare(lookup[largest]->String, lookup[left]->String) < 0) largest = left;

         if (right < heapsize) {
            if (StrSortCompare(lookup[largest]->String, lookup[right]->String) < 0) largest = right;
         }
      }

      if (largest != i) {
         struct ListSort *temp = lookup[i];
         lookup[i] = lookup[largest];
         lookup[largest] = temp;
      }
   } while (largest != i);
}

//****************************************************************************
// Gets the nth sibling with the given name.

static struct XMLTag * next_sibling(objXML *Self, struct XMLTag *Tag, LONG Index, STRING Name, LONG FlatScan)
{
   //FMSG("next_sibling","Index: %d, Name: %s, Flat: %d", Index, Name, FlatScan);

   LONG i;
   LONG flags = STR_MATCH_LEN;
   for (i=0; Name[i]; i++) {
      if (Name[i] IS '*') {
         flags = STR_WILDCARD;
         break;
      }
   }

   while (Tag) {
      if ((FlatScan != -1) AND (Tag->Branch < FlatScan)) return NULL;

      if ((Tag->Attrib->Name) AND (!StrCompare(Name, Tag->Attrib->Name, 0, flags))) {
         if (!Index) return Tag;
         Index--;
      }

      if (FlatScan != -1) Tag = Self->Tags[Tag->Index+1];
      else Tag = Tag->Next;
   }

   return NULL;
}

/*****************************************************************************
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

static ERROR find_tag2(objXML *Self, struct XMLTag **Tag, CSTRING XPath, CSTRING *Attrib, FUNCTION *Callback);

static struct XMLTag * find_tag(objXML *Self, struct XMLTag *Tag, CSTRING XPath, CSTRING *Attrib, FUNCTION *Callback)
{
   if (Attrib) *Attrib = NULL;

   // NB: If a callback is specified, the entire tree is scanned to the end.  The callback is called for each match
   // that is discovered.

   struct XMLTag *scan = Tag;
   ERROR error = find_tag2(Self, &scan, XPath, Attrib, Callback);

   if (Callback) return NULL;
   else if (!error) {
      if (Self->Flags & XMF_DEBUG) LogF("find_tag:","Found tag %p #%d", scan, scan->Index);
      return scan;
   }
   else return NULL;
}

static ERROR find_tag2(objXML *Self, struct XMLTag **Tag, CSTRING XPath, CSTRING *Attrib, FUNCTION *Callback)
{
   char tagname[120];
   CSTRING filter_attrib_name;
   LONG pos, subscript, i, filter_attrib_name_len, filter_attrib_value_len, filter_attrib_svalue, attribwild, j;
   struct XMLTag *current;
   ERROR error;
   UBYTE endchar;

   if (!(current = *Tag)) return PostError(ERR_Args);

   if ((!XPath[0]) OR (XPath[0] != '/')) {
      LogErrorMsg("Missing '/' prefix in '%s'.", XPath);
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

   if ((Attrib) AND (XPath[pos] IS '@')) *Attrib = XPath + pos + 1;

   for (i=0; ((i < sizeof(tagname)-1) AND (XPath[pos]) AND
              (XPath[pos] != '/') AND (XPath[pos] != '[') AND (XPath[pos] != '(')); pos++) {
      tagname[i++] = XPath[pos];
   }
   tagname[i] = 0;

   if (i >= sizeof(tagname)-1) {
      LogErrorMsg("Tag name in path > %d bytes: %s...", (LONG)sizeof(tagname), tagname);
      return ERR_BufferOverflow;
   }

   // Parse optional index or attribute filter

   filter_attrib_name      = NULL;
   filter_attrib_name_len  = 0;
   filter_attrib_svalue    = 0;
   filter_attrib_value_len = 0;
   attribwild              = STR_MATCH_LEN;
   LONG escattrib          = 0;

   if (Self->Flags & XMF_DEBUG) LogF("~find_tag()","%p, %s, XPath: %s, TagName: %s, Range: %d to %d", current, current->Attrib->Name, XPath, tagname, current->Index, flatscan);

   if ((XPath[pos] IS '[') OR (XPath[pos] IS '(')) {
      if (XPath[pos] IS '[') endchar = ']';
      else endchar = ')';

      pos++;

      while ((XPath[pos]) AND (XPath[pos] <= 0x20)) pos++;

      if ((XPath[pos] >= '0') AND (XPath[pos] <= '9')) {
         // Parse index

         subscript = StrToInt(XPath+pos);

         while ((XPath[pos] >= '0') AND (XPath[pos] <= '9')) pos++;
      }
      else if (XPath[pos] IS '#') {
         // Direct lookup into the tag array

         subscript = StrToInt(XPath+pos+1) + current->Index;
         if (subscript < Self->TagCount) {
            current = Self->Tags[subscript];
            subscript = -1;
         }
         else return PostError(ERR_OutOfBounds);
      }
      else if ((XPath[pos] IS '@') OR (XPath[pos] IS '=')) {
         subscript = -1;
         if (XPath[pos] IS '@') {
            pos++;

            // Parse filter attribute name

            filter_attrib_name = XPath + pos;
            while (((XPath[pos] >= 'a') AND (XPath[pos] <= 'z')) OR
                   ((XPath[pos] >= 'A') AND (XPath[pos] <= 'Z')) OR
                   (XPath[pos] IS '_')) pos++;

            filter_attrib_name_len = (XPath + pos) - filter_attrib_name;

            if (filter_attrib_name IS (XPath + pos)) goto parse_error; // Zero length string

            while ((XPath[pos]) AND (XPath[pos] <= 0x20)) pos++; // Skip whitespace

            // Parse '='

            if (XPath[pos] != '=') goto parse_error;
            pos++;
         }
         else {
            // Skip '=' (indicates matching on content) The filter_attrib_name will be empty to indicate a content match is required.

            pos++;
         }

         while ((XPath[pos]) AND (XPath[pos] <= 0x20)) pos++; // Skip whitespace

         // Parse value

         if ((XPath[pos] IS '\'') OR (XPath[pos] IS '"')) {
            UBYTE quote, tchar;

            quote = XPath[pos++];

            // Parse filter attribute value

            filter_attrib_svalue = pos;
            while ((XPath[pos]) AND (XPath[pos] != quote)) {
               if (XPath[pos] IS '\\') {
                  if ((tchar = XPath[pos+1])) {
                     if ((tchar IS '*') OR (tchar IS '\'')) {
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
            while ((XPath[pos]) AND (XPath[pos] != endchar)) {
               if (XPath[pos] IS '*') attribwild = STR_WILDCARD;
               pos++;
            }
            filter_attrib_value_len = pos - filter_attrib_svalue;
         }
      }
      else goto parse_error;

      while ((XPath[pos]) AND (XPath[pos] <= 0x20)) pos++; // Skip whitespace
      if (XPath[pos] != endchar) goto parse_error;
      pos++;
   }
   else subscript = -2; // No specific tag indicated, can scan all sibling tags in this section of the tree

next_sibling: // Start of loop - yes, we are using gotos for this

   if ((filter_attrib_name) OR (filter_attrib_svalue)) {
      // Advance to the sibling that matches the filtered attribute or content string

      LONG tagwild = STR_MATCH_LEN;
      for (i=0; tagname[i]; i++) {
         if (tagname[i] IS '*') {
            tagwild = STR_WILDCARD;
            break;
         }
      }

      UBYTE attribvalue[filter_attrib_value_len + 1];
      CopyMemory((APTR)(XPath+filter_attrib_svalue), attribvalue, filter_attrib_value_len);
      attribvalue[filter_attrib_value_len] = 0;

      // If escape codes were present in the XPath, we need to perform a conversion process.

      if (escattrib) i = filter_attrib_svalue - escattrib + 1;
      else i = 1;

      UBYTE attrib_buffer[i];

      STRING attribval;
      if (escattrib) {
         attribval = attrib_buffer;
         for (i=0,j=0; attribvalue[i]; i++) {
            if ((attribvalue[i] IS '\\') AND (attribvalue[i+1])) {
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
            if ((current->Attrib->Name) AND (!StrCompare(tagname, current->Attrib->Name, 0, tagwild))) {
               for (i=1; i < current->TotalAttrib; ++i) { // ignore name attribute, so start from index 1
                  if ((!StrCompare(current->Attrib[i].Name, filter_attrib_name, filter_attrib_name_len, 0)) AND
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
               if ((current) AND (current->Branch < flatscan)) {
                  current = NULL;
                  break;
               }

               if ((current) AND (current->Index != index)) {
                  LogErrorMsg("Corrupt tag or incorrect reference in Tags array at index %d (tag has index of %d).", index, current->Index);
                  break;
               }
            }
            else current = current->Next;
         }
      }
      else while (current) {
         if ((current->Attrib->Name) AND (!StrCompare(tagname, current->Attrib->Name, 0, tagwild))) {
            // Match on content
            if ((current->Child) AND (!current->Child->Attrib->Name)) {
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

            if ((current) AND (current->Branch < flatscan)) {
               current = NULL;
               break;
            }

            if ((current) AND (current->Index != index)) {
               LogErrorMsg("Corrupt tag or incorrect reference in Tags array at index %d (tag has index of %d).", index, current->Index);
               break;
            }
         }
         else current = current->Next;
      }
   }
   else current = next_sibling(Self, current, (subscript >= 0) ? subscript : 0, tagname, flatscan);

matched_attrib:

   if (current) {
      struct XMLTag *scan;

      if (!XPath[pos]) {
         // Matching tag found and there is nothing left to process

         if (Callback) {
            if (Callback->Type IS CALL_STDC) {
               ERROR (*routine)(objXML *, struct XMLTag *, CSTRING);
               routine = Callback->StdC.Routine;
               error = routine(Self, current, NULL);
            }
            else if (Callback->Type IS CALL_SCRIPT) {
               OBJECTPTR script;
               if ((script = Callback->Script.Script)) {
                  const struct ScriptArg args[] = {
                     { "XML",  FD_OBJECTPTR, { .Address = Self } },
                     { "Tag",  FD_LONG,      { .Long = current->Index } },
                     { "Attrib", FD_STRING,  { .Address = NULL } }
                  };
                  if (!scCallback(script, Callback->Script.ProcedureID, args, ARRAYSIZE(args))) {
                     GetLong(script, FID_Error, &error);
                  }
                  else error = ERR_Terminate; // Fatal error in attempting to execute the procedure
               }
            }

            if (error IS ERR_Terminate) {
               *Tag = current;
               if (Self->Flags & XMF_DEBUG) LogReturn();
               return ERR_Terminate;
            }
            if (((subscript IS -2) OR (subscript IS -1)) AND ((current = current->Next))) goto next_sibling;
         }
         else {
            *Tag = current;
            if (Self->Flags & XMF_DEBUG) LogReturn();
            return ERR_Okay; // End of query reached, successfully found tag
         }
      }
      else if ((XPath[pos] IS '/') AND (XPath[pos+1] IS '@')) {
         if (Attrib) *Attrib = XPath + pos + 2;

         if (Callback) {
            if (Callback->Type IS CALL_STDC) {
               ERROR (*routine)(objXML *, struct XMLTag *, CSTRING);
               routine = Callback->StdC.Routine;
               error = routine(Self, current, NULL);
            }
            else if (Callback->Type IS CALL_SCRIPT) {
               OBJECTPTR script;
               if ((script = Callback->Script.Script)) {
                  const struct ScriptArg args[] = {
                     { "XML",  FD_OBJECTPTR, { .Address = Self } },
                     { "Tag",  FD_LONG,      { .Long = current->Index } },
                     { "Attrib", FD_STRING,  { .Address = (STRING)(Attrib ? Attrib[0] : NULL) } }
                  };
                  if (!scCallback(script, Callback->Script.ProcedureID, args, ARRAYSIZE(args))) {
                     GetLong(script, FID_Error, &error);
                  }
                  else error = ERR_Terminate; // Fatal error in attempting to execute the procedure
                  ReleaseObject(script);
               }
            }

            if (error IS ERR_Terminate) {
               *Tag = current;
               if (Self->Flags & XMF_DEBUG) LogReturn();
               return ERR_Terminate;
            }
            if (((subscript IS -2) OR (subscript IS -1)) AND ((current = current->Next))) goto next_sibling;
         }
         else {
            *Tag = current;
            if (Self->Flags & XMF_DEBUG) LogReturn();
            return ERR_Okay;
         }
      }
      else if ((scan = current->Child)) {
         // Move to next position in the XPath and scan child node

         error = find_tag2(Self, &scan, XPath+pos, Attrib, Callback);

         if (error IS ERR_Terminate) {
            *Tag = current;
            if (Self->Flags & XMF_DEBUG) LogReturn();
            return ERR_Terminate;
         }
         else if ((error) OR (Callback)) {
            // Nothing matches in this subset of tags, or callbacks are in use.  Move to the next sibling if subscripts
            // are not being used.

            if (subscript < 0) {
               current = current->Next;
               goto next_sibling;
            }
         }
         else *Tag = scan;
      }
      else error = ERR_Search;
   }
   else {
      error = ERR_Search;
   }

   if (Self->Flags & XMF_DEBUG) LogReturn();
   return error;

parse_error:
   LogMsg("XPath unresolved: %s", XPath);
   if (Self->Flags & XMF_DEBUG) LogReturn();
   return ERR_Search;
}

//****************************************************************************

static ERROR parse_source(objXML *Self)
{
   struct CacheFile *filecache;

   FMSG("~parse_source()","");

   // Although the file will be uncached as soon it is loaded, the developer can pre-cache XML files with his own
   // call to LoadFile(), which can lead our use of LoadFile() to being quite effective.

   if (Self->Source) {
      UBYTE *buffer;
      LARGE size = 64 * 1024;
      if (!AllocMemory(size+1, MEM_STRING|MEM_NO_CLEAR, &buffer, NULL)) {
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
      Self->ParseError = txt_to_xml(Self, filecache->Data);
      UnloadFile(filecache);
   }
   else Self->ParseError = ERR_File;

   STEP();
   return Self->ParseError;
}

//****************************************************************************
// Extracts immediate content, does not recurse into child tags.

static ERROR get_content(objXML *Self, struct XMLTag *Tag, STRING Buffer, LONG Size)
{
   Buffer[0] = 0;
   if ((Tag = Tag->Child)) {
      LONG j = 0;
      while (Tag) {
         if ((!Tag->Attrib->Name) AND (Tag->Attrib->Value)) {
            j += StrCopy(Tag->Attrib->Value, Buffer+j, Size-j);
            if (j >= Size) break;
         }
         Tag = Tag->Next;
      }

      if (j >= Size) return PostError(ERR_BufferOverflow);
   }

   return ERR_Okay;
}

//****************************************************************************

static void free_xml(objXML *Self)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }
   if (Self->Statement) { FreeResource(Self->Statement); Self->Statement = NULL; }
   clear_tags(Self);
   if (Self->Tags) { FreeResource(Self->Tags); Self->Tags = NULL; }
}

//**********************************************************************

static void clear_tags(objXML *XML)
{
   LONG i;
   for (i=0; i < XML->TagCount; i++) {
      if (XML->Tags[i]) FreeResource(XML->Tags[i]);
   }
   if (XML->Tags) XML->Tags[0] = NULL; // Don't free the array, just null terminate it
   XML->TagCount = 0;
}

//****************************************************************************

#warning TODO: Support processing of ENTITY declarations in the doctype.

static void parse_doctype(objXML *Self, CSTRING Input)
{

}
