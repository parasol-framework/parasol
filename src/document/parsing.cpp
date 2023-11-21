
static void check_para_attrib(extDocument *, const std::string &, const std::string &, bcParagraph *);

//********************************************************************************************************************
// Check for a pending font and/or style change and respond appropriately.

static void style_check(extDocument *Self, StreamChar &Cursor)
{
   if (Self->Style.FaceChange) {
      // Create a new font object for the current style

      auto style_name = get_font_style(Self->Style.FontStyle.Options);
      Self->Style.FontStyle.FontIndex = create_font(Self->Style.Face, style_name, Self->Style.Point);
      //log.trace("Changed font to index %d, face %s, style %s, size %d.", Self->Style.FontStyle.Index, Self->Style.Face, style_name, Self->Style.Point);
      Self->Style.FaceChange  = false;
      Self->Style.StyleChange = true;
   }

   if (Self->Style.StyleChange) { // Insert a font change into the text stream
      // NB: Assigning a new UID is suboptimal in cases where we are reverting to a previously registered state
      // (i.e. anywhere where saved_style_check() has been used).  We could allow insertCode() to lookup formerly
      // allocated UID's and save some memory usage if we improved the management of saved styles.
      Self->Style.FontStyle.UID = glByteCodeID++;
      Self->insertCode(Cursor, Self->Style.FontStyle);
      Self->Style.StyleChange = false;
   }
}

//********************************************************************************************************************

static bool eval_condition(const std::string &String)
{
   pf::Log log(__FUNCTION__);

   static const FieldDef table[] = {
      { "<>", COND_NOT_EQUAL },
      { "!=", COND_NOT_EQUAL },
      { "=",  COND_EQUAL },
      { "==", COND_EQUAL },
      { "<",  COND_LESS_THAN },
      { "<=", COND_LESS_EQUAL },
      { ">",  COND_GREATER_THAN },
      { ">=", COND_GREATER_EQUAL },
      { NULL, 0 }
   };

   LONG start = 0;
   while ((start < LONG(String.size())) and (String[start] <= 0x20)) start++;

   bool reverse = false;

   // Find the condition statement

   LONG i;
   for (i=start; i < LONG(String.size()); i++) {
      if ((String[i] IS '!') and (String[i+1] IS '=')) break;
      if (String[i] IS '>') break;
      if (String[i] IS '<') break;
      if (String[i] IS '=') break;
   }

   // If there is no condition statement, evaluate the statement as an integer

   if (i >= LONG(String.size())) {
      if (StrToInt(String)) return true;
      else return false;
   }

   LONG cpos = i;

   // Extract Test value

   while ((i > 0) and (String[i-1] IS ' ')) i--;
   std::string test(String, i);

   // Condition value

   LONG condition = 0;
   {
      std::string cond;
      cond.reserve(3);
      char c;
      for (i=cpos,c=0; (c < 2) and ((String[i] IS '!') or (String[i] IS '=') or (String[i] IS '>') or (String[i] IS '<')); i++) {
         cond[c++] = String[i];
      }

      for (unsigned j=0; table[j].Name; j++) {
         if (!StrMatch(cond, table[j].Name)) {
            condition = table[j].Value;
            break;
         }
      }
   }

   while ((String[i]) and (String[i] <= 0x20)) i++; // skip white-space

   bool truth = false;
   if (!test.empty()) {
      if (condition) {
         // Convert the If->Compare to its specified type

         auto cmp_type  = StrDatatype(String.c_str()+i);
         auto test_type = StrDatatype(test.c_str());

         if (((test_type IS STT::NUMBER) or (test_type IS STT::FLOAT)) and ((cmp_type IS STT::NUMBER) or (cmp_type IS STT::FLOAT))) {
            auto cmp_float  = StrToFloat(String.c_str()+i);
            auto test_float = StrToFloat(test);
            switch (condition) {
               case COND_NOT_EQUAL:     if (test_float != cmp_float) truth = true; break;
               case COND_EQUAL:         if (test_float IS cmp_float) truth = true; break;
               case COND_LESS_THAN:     if (test_float <  cmp_float) truth = true; break;
               case COND_LESS_EQUAL:    if (test_float <= cmp_float) truth = true; break;
               case COND_GREATER_THAN:  if (test_float >  cmp_float) truth = true; break;
               case COND_GREATER_EQUAL: if (test_float >= cmp_float) truth = true; break;
               default: log.warning("Unsupported condition type %d.", condition);
            }
         }
         else if (condition IS COND_EQUAL) {
            if (!StrMatch(test, String.c_str()+i)) truth = true;
         }
         else if (condition IS COND_NOT_EQUAL) {
            if (StrMatch(test, String.c_str()+i) != ERR_Okay) truth = true;
         }
         else log.warning("String comparison for condition %d not possible.", condition);
      }
      else log.warning("No test condition in \"%s\".", String.c_str());
   }
   else log.warning("No test value in \"%s\".", String.c_str());

   if (reverse) return truth ^ 1;
   else return truth;
}

//********************************************************************************************************************
// Used by if, elseif, while statements to check the satisfaction of conditions.

static bool check_tag_conditions(extDocument *Self, XMLTag &Tag)
{
   pf::Log log("eval");

   bool satisfied = false;
   bool reverse = false;
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("statement", Tag.Attribs[i].Name)) {
         satisfied = eval_condition(Tag.Attribs[i].Value);
         log.trace("Statement: %s", Tag.Attribs[i].Value);
         break;
      }
      else if (!StrMatch("exists", Tag.Attribs[i].Name)) {
         OBJECTID object_id;
         if (!FindObject(Tag.Attribs[i].Value.c_str(), 0, FOF::SMART_NAMES, &object_id)) {
            if (valid_objectid(Self, object_id)) {
               satisfied = true;
            }
         }
         break;
      }
      else if (!StrMatch("notnull", Tag.Attribs[i].Name)) {
         log.trace("NotNull: %s", Tag.Attribs[i].Value);
         if (Tag.Attribs[i].Value.empty()) satisfied = false;
         else if (Tag.Attribs[i].Value == "0") satisfied = false;
         else satisfied = true;
      }
      else if ((!StrMatch("isnull", Tag.Attribs[i].Name)) or (!StrMatch("null", Tag.Attribs[i].Name))) {
         log.trace("IsNull: %s", Tag.Attribs[i].Value);
            if (Tag.Attribs[i].Value.empty()) satisfied = true;
            else if (Tag.Attribs[i].Value == "0") satisfied = true;
            else satisfied = false;
      }
      else if (!StrMatch("not", Tag.Attribs[i].Name)) {
         reverse = true;
      }
   }

   // Check for a not condition and invert the satisfied value if found

   if (reverse) satisfied = satisfied ^ 1;

   return satisfied;
}

//********************************************************************************************************************
// This is the principal function for the parsing of XML tags.  Insertion into the stream will occur at Index, which
// is updated on completion.
//
// Supported Flags:
//   IPF::NO_CONTENT:
//   IPF::STRIP_FEEDS:

static TRF parse_tag(extDocument *Self, objXML *XML, XMLTag &Tag, StreamChar &Index, IPF &Flags)
{
   pf::Log log(__FUNCTION__);

   if (Self->Error) {
      log.traceWarning("Error field is set, returning immediately.");
      return TRF::NIL;
   }

   IPF filter = Flags & IPF::FILTER_ALL;
   XMLTag *object_template = NULL;

   auto saved_attribs = Tag.Attribs;
   translate_attrib_args(Self, Tag.Attribs);

   auto tagname = Tag.Attribs[0].Name;
   if (tagname.starts_with('$')) tagname.erase(0, 1);
   object_template = NULL;

   TRF result = TRF::NIL;
   if (Tag.isContent()) {
      if ((Flags & IPF::NO_CONTENT) IS IPF::NIL) {
         if ((Flags & IPF::STRIP_FEEDS) != IPF::NIL) {
            if (Self->CurrentObject) {
               // Objects do not normally accept document content (user should use <xml>)
               // An exception is made for content that is injected within an object tag.

               if (XML IS Self->InjectXML) {
                  unsigned i = 0;
                  while ((Tag.Attribs[0].Value[i] IS '\n') or (Tag.Attribs[0].Value[i] IS '\r')) i++;
                  acDataContent(Self->CurrentObject, Tag.Attribs[0].Value.c_str() + i);
               }
            }
            else if (Self->ParagraphDepth) { // We must be in a paragraph to accept content as text
               unsigned i = 0;
               while ((Tag.Attribs[0].Value[i] IS '\n') or (Tag.Attribs[0].Value[i] IS '\r')) i++;
               if (i > 0) {
                  auto content = Tag.Attribs[0].Value.substr(i);
                  insert_text(Self, Index, content, ((Self->Style.FontStyle.Options & FSO::PREFORMAT) != FSO::NIL));
               }
               else insert_text(Self, Index, Tag.Attribs[0].Value, ((Self->Style.FontStyle.Options & FSO::PREFORMAT) != FSO::NIL));
            }
            Flags &= ~IPF::STRIP_FEEDS;
         }
         else if (Self->CurrentObject) {
            if (XML IS Self->InjectXML) acDataContent(Self->CurrentObject, Tag.Attribs[0].Value.c_str());
         }
         else if (Self->ParagraphDepth) { // We must be in a paragraph to accept content as text
            insert_text(Self, Index, Tag.Attribs[0].Value, ((Self->Style.FontStyle.Options & FSO::PREFORMAT) != FSO::NIL));
         }
      }
      Tag.Attribs = saved_attribs;
      return result;
   }

   if (Self->Templates) { // Check for templates first, as they can be used to override the default RPL tag names.
      bool template_match = false;
      for (auto &scan : Self->Templates->Tags) {
         for (unsigned i=0; i < scan.Attribs.size(); i++) {
            if ((!StrMatch("name", scan.Attribs[i].Name)) and (!StrMatch(tagname, scan.Attribs[i].Value))) {
               template_match = true;
            }
         }

         if (template_match) {
            // Process the template by jumping into it.  Arguments in the tag are added to a sequential
            // list that will be processed in reverse by translate_attrib_args().

            pf::Log log(__FUNCTION__);

            initTemplate block(Self, Tag.Children, XML); // Required for the <inject/> feature to work inside the template

            log.traceBranch("Executing template '%s'.", tagname.c_str());

            Self->TemplateArgs.push_back(&Tag);
            parse_tags(Self, Self->Templates, scan.Children, Index, Flags);
            Self->TemplateArgs.pop_back();

            Tag.Attribs = saved_attribs;
            return result;
         }
      }
   }

   if (auto tag = glTags.find(tagname); tag != glTags.end()) {
      auto &tr = tag->second;
      if (((tr.Flags & TAG::FILTER_ALL) != TAG::NIL) and ((tr.Flags & TAG(filter)) IS TAG::NIL)) {
         // A filter applies to this tag and the filter flags do not match
         log.warning("Invalid use of tag '%s' - Not applied to the correct tag parent.", tagname.c_str());
         Self->Error = ERR_InvalidData;
      }
      else if (tr.Routine) {
         //log.traceBranch("%s", tagname);

         if ((Self->CurrentObject) and ((tr.Flags & (TAG::OBJECTOK|TAG::CONDITIONAL)) IS TAG::NIL)) {
            log.warning("Illegal use of tag %s within object of class '%s'.", tagname.c_str(), Self->CurrentObject->Class->ClassName);
            result = TRF::BREAK;
         }
         else {

            if (((Flags & IPF::NO_CONTENT) != IPF::NIL) and ((tr.Flags & TAG::CONTENT) != TAG::NIL)) {
               // Do nothing when content is not allowed
               log.trace("Content disabled on '%s', tag not processed.", tagname.c_str());
            }
            else if ((tr.Flags & TAG::CHILDREN) != TAG::NIL) {
               // Child content is compulsory or tag has no effect
               if (!Tag.Children.empty()) tr.Routine(Self, XML, Tag, Tag.Children, Index, Flags);
               else log.trace("No content found in tag '%s'", tagname.c_str());
            }
            else tr.Routine(Self, XML, Tag, Tag.Children, Index, Flags);

         }
      }
   }
   else if (!StrMatch("break", tagname)) {
      // Breaking stops executing all tags (within this section) beyond the breakpoint.  If in a loop, the loop
      // will stop executing.

      result = TRF::BREAK;
   }
   else if (!StrMatch("continue", tagname)) {
      // Continuing - does the same thing as a break but the loop continues.
      // If used when not in a loop, then all sibling tags are skipped.

      result = TRF::CONTINUE;
   }
   else if (!StrMatch("if", tagname)) {
      if (check_tag_conditions(Self, Tag)) { // Statement is true
         Flags &= ~IPF::CHECK_ELSE;
         result = parse_tags(Self, XML, Tag.Children, Index, Flags);
      }
      else Flags |= IPF::CHECK_ELSE;
   }
   else if (!StrMatch("elseif", tagname)) {
      if ((Flags & IPF::CHECK_ELSE) != IPF::NIL) {
         if (check_tag_conditions(Self, Tag)) { // Statement is true
            Flags &= ~IPF::CHECK_ELSE;
            result = parse_tags(Self, XML, Tag.Children, Index, Flags);
         }
      }
   }
   else if (!StrMatch("else", tagname)) {
      if ((Flags & IPF::CHECK_ELSE) != IPF::NIL) {
         Flags &= ~IPF::CHECK_ELSE;
         result = parse_tags(Self, XML, Tag.Children, Index, Flags);
      }
   }
   else if (!StrMatch("while", tagname)) {
      if ((!Tag.Children.empty()) and (check_tag_conditions(Self, Tag))) {
         // Save/restore the statement string on each cycle to fully evaluate the condition each time.

         auto saveindex = Self->LoopIndex;
         Self->LoopIndex = 0;

         bool state = true;
         while (state) {
            state = check_tag_conditions(Self, Tag);
            Tag.Attribs = saved_attribs;
            translate_attrib_args(Self, Tag.Attribs);

            if ((state) and ((parse_tags(Self, XML, Tag.Children, Index, Flags) & TRF::BREAK) != TRF::NIL)) break;

            Self->LoopIndex++;
         }

         Self->LoopIndex = saveindex;
      }
   }
   else if ((Flags & IPF::NO_CONTENT) IS IPF::NIL) {
      log.warning("Tag '%s' unsupported as an instruction or template.", tagname.c_str());
   }
   else log.warning("Unrecognised tag '%s' used in a content-restricted area.", tagname.c_str());

   Tag.Attribs = saved_attribs;
   return result;
}

static TRF parse_tags(extDocument *Self, objXML *XML, objXML::TAGS &Tags, StreamChar &Index, IPF Flags)
{
   TRF result = TRF::NIL;

   for (auto &tag : Tags) {
      // Note that Flags will carry state between multiple calls to parse_tag().  This allows if/else to work correctly.
      result = parse_tag(Self, XML, tag, Index, Flags);
      if ((Self->Error) or ((result & (TRF::CONTINUE|TRF::BREAK)) != TRF::NIL)) break;
   }

   return result;
}

//********************************************************************************************************************

static void check_para_attrib(extDocument *Self, const std::string &Attrib, const std::string &Value, bcParagraph *esc)
{
   switch (StrHash(Attrib)) {
      case HASH_anchor:
         Self->Style.StyleChange = true;
         Self->Style.FontStyle.Options |= FSO::IN_LINE;
         break;

      case HASH_leading:
         if (esc) {
            esc->LeadingRatio = StrToFloat(Value);
            if (esc->LeadingRatio < MIN_LEADING) esc->LeadingRatio = MIN_LEADING;
            else if (esc->LeadingRatio > MAX_LEADING) esc->LeadingRatio = MAX_LEADING;
         }
         break;

      case HASH_nowrap:
         Self->Style.StyleChange = true;
         Self->Style.FontStyle.Options |= FSO::NO_WRAP;
         break;

      case HASH_valign: {
         // Vertical alignment defines the vertical position for text in cases where the line height is greater than
         // the text itself (e.g. if an image is anchored in the line).
         ALIGN align = ALIGN::NIL;
         if (!StrMatch("top", Value)) align = ALIGN::TOP;
         else if (!StrMatch("center", Value)) align = ALIGN::VERTICAL;
         else if (!StrMatch("middle", Value)) align = ALIGN::VERTICAL;
         else if (!StrMatch("bottom", Value)) align = ALIGN::BOTTOM;
         if (align != ALIGN::NIL) {
            Self->Style.StyleChange = true;
            Self->Style.FontStyle.VAlign = (Self->Style.FontStyle.VAlign & (ALIGN::TOP|ALIGN::VERTICAL|ALIGN::BOTTOM)) | align;
         }
         break;
      }

      case HASH_kerning:  // REQUIRES CODE and DOCUMENTATION
         break;

      case HASH_lineheight: // REQUIRES CODE and DOCUMENTATION
         // Line height is expressed as a ratio - 1.0 is standard, 1.5 would be an extra half, 0.5 would squash the text by half.

         //Self->Style.LineHeightRatio = StrToFloat(Tag.Attribs[i].Value);
         //if (Self->Style.LineHeightRatio < MIN_LINEHEIGHT) Self->Style.LineHeightRatio = MIN_LINEHEIGHT;
         break;

      case HASH_trim:
         esc->Trim = true;
         break;

      case HASH_vspacing:
         // Vertical spacing between embedded paragraphs.  Ratio is expressed as a measure of the *default* lineheight (not the height of the
         // last line of the paragraph).  E.g. 1.5 is one and a half times the standard lineheight.  The default is 1.0.

         if (esc) {
            esc->VSpacing = StrToFloat(Value);
            if (esc->VSpacing < MIN_VSPACING) esc->VSpacing = MIN_VSPACING;
            else if (esc->VSpacing > MAX_VSPACING) esc->VSpacing = MAX_VSPACING;
         }
         break;
   }
}

//********************************************************************************************************************

static void trim_preformat(extDocument *Self, StreamChar &Index)
{
   auto i = Index.Index - 1;
   for (; i > 0; i--) {
      if (Self->Stream[i].Code IS ESC::TEXT) {
         auto &text = escape_data<bcText>(Self, i);

         static const std::string ws(" \t\f\v\n\r");
         auto found = text.Text.find_last_not_of(ws);
         if (found != std::string::npos) {
            text.Text.erase(found + 1);
            break;
         }
         else text.Text.clear();
      }
      else break;
   }
}

/*********************************************************************************************************************
** This function is used to manage hierarchical styling:
**
** + Save Font Style
**   + Execute child tags
** + Restore Font Style
**
** If the last style that comes out of parse_tag() does not match the style stored in SaveStatus, we need to record a
** style change.
*/

static void saved_style_check(extDocument *Self, style_status &SavedStatus)
{
   auto face_change = Self->Style.FaceChange;
   auto style = Self->Style.StyleChange;

   if (SavedStatus.FontStyle.FontIndex != Self->Style.FontStyle.FontIndex) face_change = true;

   if ((SavedStatus.FontStyle.Options != Self->Style.FontStyle.Options) or
       (SavedStatus.FontStyle.Fill != Self->Style.FontStyle.Fill) or
       (SavedStatus.FontStyle.VAlign != Self->Style.FontStyle.VAlign)) {
      style = true;
   }

   if ((face_change) or (style)) {
      Self->Style = SavedStatus; // Restore the style that we had before processing the children

      // Reapply the fontstate and stylestate information

      Self->Style.FaceChange  = face_change;
      Self->Style.StyleChange = style;
   }
}

//********************************************************************************************************************
// Advances the cursor.  It is only possible to advance positively on either axis.

static void tag_advance(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   auto &advance = Self->reserveCode<bcAdvance>(Index);

   for (unsigned i = 1; i < Tag.Attribs.size(); i++) {
      switch (StrHash(Tag.Attribs[i].Name)) {
         case HASH_x: advance.X = StrToInt(Tag.Attribs[i].Value); break;
         case HASH_y: advance.Y = StrToInt(Tag.Attribs[i].Value); break;
      }
   }

   if (advance.X < 0) advance.X = 0;
   else if (advance.X > 4000) advance.X = 4000;

   if (advance.Y < 0) advance.Y = 0;
   else if (advance.Y > 4000) advance.Y = 4000;
}

//********************************************************************************************************************
// NB: If a <body> tag contains any children, it is treated as a template and must contain an <inject/> tag so that
// the XML insertion point is known.

static void tag_body(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   static const LONG MAX_BODY_MARGIN = 500;

   // Body tag needs to be placed before any content

   for (unsigned i = 1; i < Tag.Attribs.size(); i++) {
      switch (StrHash(Tag.Attribs[i].Name)) {
         case HASH_link:
            if (Self->LinkFill) FreeResource(Self->LinkFill);
            Self->LinkFill = StrClone(Tag.Attribs[i].Value.c_str());
            break;

         case HASH_vlink:
            if (Self->VLinkFill) FreeResource(Self->VLinkFill);
            Self->VLinkFill = StrClone(Tag.Attribs[i].Value.c_str());
            break;

         case HASH_selectcolour: // Colour to use when a link is selected (using the tab key to get to a link will select it)
            if (Self->LinkSelectFill) FreeResource(Self->LinkSelectFill);
            Self->LinkSelectFill = StrClone(Tag.Attribs[i].Value.c_str());
            break;

         case HASH_leftmargin:
            Self->LeftMargin = StrToInt(Tag.Attribs[i].Value);
            if (Self->LeftMargin < 0) Self->LeftMargin = 0;
            else if (Self->LeftMargin > MAX_BODY_MARGIN) Self->LeftMargin = MAX_BODY_MARGIN;
            break;

         case HASH_rightmargin:
            Self->RightMargin = StrToInt(Tag.Attribs[i].Value);
            if (Self->RightMargin < 0) Self->RightMargin = 0;
            else if (Self->RightMargin > MAX_BODY_MARGIN) Self->RightMargin = MAX_BODY_MARGIN;
            break;

         case HASH_topmargin:
            Self->TopMargin = StrToInt(Tag.Attribs[i].Value);
            if (Self->TopMargin < 0) Self->TopMargin = 0;
            else if (Self->TopMargin > MAX_BODY_MARGIN) Self->TopMargin = MAX_BODY_MARGIN;
            break;

         case HASH_bottommargin:
            Self->BottomMargin = StrToInt(Tag.Attribs[i].Value);
            if (Self->BottomMargin < 0) Self->BottomMargin = 0;
            else if (Self->BottomMargin > MAX_BODY_MARGIN) Self->BottomMargin = MAX_BODY_MARGIN;
            break;

         case HASH_margins:
            Self->LeftMargin = StrToInt(Tag.Attribs[i].Value);
            if (Self->LeftMargin < 0) Self->LeftMargin = 0;
            else if (Self->LeftMargin > MAX_BODY_MARGIN) Self->LeftMargin = MAX_BODY_MARGIN;
            Self->RightMargin  = Self->LeftMargin;
            Self->TopMargin    = Self->LeftMargin;
            Self->BottomMargin = Self->LeftMargin;
            break;

         case HASH_lineheight:
            Self->LineHeight = StrToInt(Tag.Attribs[i].Value);
            if (Self->LineHeight < 4) Self->LineHeight = 4;
            else if (Self->LineHeight > 100) Self->LineHeight = 100;
            break;

         case HASH_pagewidth:
         case HASH_width:
            Self->PageWidth = StrToFloat(Tag.Attribs[i].Value);
            if (Self->PageWidth < 1) Self->PageWidth = 1;
            else if (Self->PageWidth > 6000) Self->PageWidth = 6000;

            if (Tag.Attribs[i].Value.find('%') != std::string::npos) Self->RelPageWidth = true;
            else Self->RelPageWidth = false;
            log.msg("Page width forced to %.0f%s.", Self->PageWidth, Self->RelPageWidth ? "%%" : "");
            break;

         case HASH_colour: // Background colour
            if (Self->Background) FreeResource(Self->Background);
            Self->Background = StrClone(Tag.Attribs[i].Value.c_str());
            break;

         case HASH_face:
         case HASH_fontface:
            SetField(Self, FID_FontFace, Tag.Attribs[i].Value);
            break;

         case HASH_fontsize: // Default font point size
            Self->FontSize = StrToInt(Tag.Attribs[i].Value);
            break;

         case HASH_fontcolour: // Default font colour
            if (Self->FontFill) FreeResource(Self->FontFill);
            Self->FontFill = StrClone(Tag.Attribs[i].Value.c_str());
            break;

         default:
            log.warning("Style attribute %s=%s not supported.", Tag.Attribs[i].Name.c_str(), Tag.Attribs[i].Value.c_str());
            break;
      }
   }

   Self->Style.FontStyle.FontIndex = create_font(Self->FontFace, "Regular", Self->FontSize);
   Self->Style.FontStyle.Options   = FSO::NIL;
   Self->Style.FontStyle.Fill      = Self->FontFill;

   Self->Style.Face        = Self->FontFace;
   Self->Style.Point       = Self->FontSize;
   Self->Style.FaceChange  = true;
   Self->Style.StyleChange = true;

   if (!Children.empty()) Self->BodyTag = &Children;
}

//********************************************************************************************************************
// In background mode, all objects are targeted to the View viewport rather than the Page viewport.

static void tag_background(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   Self->BkgdGfx++;
   parse_tags(Self, XML, Children, Index);
   Self->BkgdGfx--;
}

//********************************************************************************************************************

static void tag_bold(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   if ((Self->Style.FontStyle.Options & FSO::BOLD) IS FSO::NIL) {
      auto savestatus = Self->Style; // Save the current style
      Self->Style.FaceChange = true; // Bold fonts are typically a different typeset
      Self->Style.FontStyle.Options |= FSO::BOLD;
      parse_tags(Self, XML, Children, Index);
      saved_style_check(Self, savestatus);
   }
   else parse_tags(Self, XML, Children, Index, Flags & ~IPF::FILTER_ALL);
}

//********************************************************************************************************************

static void tag_br(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   insert_text(Self, Index, "\n", true);
   Self->NoWhitespace = true;
}

//********************************************************************************************************************
// Use caching to create objects that will persist between document refreshes and page changes (so long as said page
// resides within the same document source).  The following code illustrates how to create a persistent XML object:
//
// <if not exists="[xml192]">
//   <cache>
//     <xml name="xml192"/>
//   </cache>
// </if>
//
// The object is removed when the document object is destroyed, or the document source is changed.
//
// NOTE: Another valid method of caching an object is to use a persistent script.

static void tag_cache(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   Self->ObjectCache++;
   parse_tags(Self, XML, Children, Index);
   Self->ObjectCache--;
}

//********************************************************************************************************************
// Use this instruction to call a function during the parsing of the document.
//
// The only argument required by this tag is 'function'.  All following attributes are treated as arguments that are
// passed to the called procedure (note that arguments are passed in the order in which they appear).
//
// Global arguments can be set against the script object itself if the argument is prefixed with an underscore.
//
// To call a function that isn't in the default script, simply specify the name of the target script before the
// function name, split with a dot, e.g. "script.function".
//
// <call function="[script].function" arg1="" arg2="" _global=""/>

static void tag_call(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);
   OBJECTPTR script = Self->DefaultScript;

   std::string function;
   if (Tag.Attribs.size() > 1) {
      if (!StrMatch("function", Tag.Attribs[1].Name)) {
         if (auto i = Tag.Attribs[1].Value.find('.');  i != std::string::npos) {
            auto script_name = Tag.Attribs[1].Value.substr(0, i);

            OBJECTID id;
            if (!FindObject(script_name.c_str(), 0, FOF::NIL, &id)) script = GetObjectPtr(id);

            function.assign(Tag.Attribs[1].Value, i + 1);
         }
         else function = Tag.Attribs[1].Value;
      }
   }

   if (function.empty()) {
      log.warning("The first attribute to <call/> must be a function reference.");
      Self->Error = ERR_Syntax;
      return;
   }

   if (!script) {
      log.warning("No script in this document for a requested <call/>.");
      Self->Error = ERR_Failed;
      return;
   }

   {
      pf::Log log(__FUNCTION__);
      log.traceBranch("Calling script #%d function '%s'", script->UID, function.c_str());

      if (Tag.Attribs.size() > 2) {
         std::vector<ScriptArg> args;

         unsigned index = 0;
         for (unsigned i=2; i < Tag.Attribs.size(); i++) {
            if (Tag.Attribs[i].Name[0] IS '_') { // Global variable setting
               acSetVar(script, Tag.Attribs[i].Name.c_str()+1, Tag.Attribs[i].Value.c_str());
            }
            else if (args[index].Name[0] IS '@') {
               args.emplace_back(Tag.Attribs[i].Name.c_str() + 1, Tag.Attribs[i].Value);
            }
            else args.emplace_back(Tag.Attribs[i].Name.c_str(), Tag.Attribs[i].Value);
         }

         scExec(script, function.c_str(), args.data(), args.size());
      }
      else scExec(script, function.c_str(), NULL, 0);
   }

   // Check for a result and print it

   CSTRING *results;
   LONG size;
   if ((!GetFieldArray(script, FID_Results, &results, &size)) and (size > 0)) {
      auto xmlinc = objXML::create::global(fl::Statement(results[0]), fl::Flags(XMF::PARSE_HTML|XMF::STRIP_HEADERS));
      if (xmlinc) {
         parse_tags(Self, xmlinc, xmlinc->Tags, Index, Flags);

         // Add the created XML object to the document rather than destroying it

         Self->Resources.emplace_back(xmlinc->UID, RTD::OBJECT_TEMP);
      }
      FreeResource(results);
   }
}

//********************************************************************************************************************

static void tag_caps(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   if ((Self->Style.FontStyle.Options & FSO::CAPS) IS FSO::NIL) {
      auto savestatus = Self->Style;
      Self->Style.StyleChange = true;
      Self->Style.FontStyle.Options |= FSO::CAPS;
      parse_tags(Self, XML, Tag.Children, Index);
      saved_style_check(Self, savestatus);
   }
   else parse_tags(Self, XML, Tag.Children, Index, Flags);
}

//********************************************************************************************************************

static void tag_debug(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log("DocMsg");
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("msg", Tag.Attribs[i].Name)) log.warning("%s", Tag.Attribs[i].Value.c_str());
   }
}

//********************************************************************************************************************
// Use div to structure the document in a similar way to paragraphs.  The main difference is that it avoids the
// declaration of paragraph start and end points and won't cause line breaks.

static void tag_div(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   auto savestatus = Self->Style;
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("align", Tag.Attribs[i].Name)) {
         if ((!StrMatch(Tag.Attribs[i].Value, "center")) or (!StrMatch(Tag.Attribs[i].Value, "horizontal"))) {
            Self->Style.StyleChange = true;
            Self->Style.FontStyle.Options |= FSO::ALIGN_CENTER;
         }
         else if (!StrMatch(Tag.Attribs[i].Value, "right")) {
            Self->Style.StyleChange = true;
            Self->Style.FontStyle.Options |= FSO::ALIGN_RIGHT;
         }
         else log.warning("Alignment type '%s' not supported.", Tag.Attribs[i].Value.c_str());
      }
      else check_para_attrib(Self, Tag.Attribs[i].Name, Tag.Attribs[i].Value, 0);
   }

   parse_tags(Self, XML, Children, Index);
   saved_style_check(Self, savestatus);
}

//********************************************************************************************************************
// Creates a new edit definition.  These are stored in a linked list.  Edit definitions are used by referring to them
// by name in table cells.

static void tag_editdef(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   DocEdit edit;
   std::string name;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      switch (StrHash(Tag.Attribs[i].Name)) {
         case HASH_maxchars:
            edit.MaxChars = StrToInt(Tag.Attribs[i].Value);
            if (edit.MaxChars < 0) edit.MaxChars = -1;
            break;

         case HASH_name: name = Tag.Attribs[i].Value; break;

         case HASH_selectcolour: break;

         case HASH_linebreaks: edit.LineBreaks = StrToInt(Tag.Attribs[i].Value); break;

         case HASH_editfonts:
         case HASH_editimages:
         case HASH_edittables:
         case HASH_editall:
            break;

         case HASH_onchange:
            if (!Tag.Attribs[i].Value.empty()) edit.OnChange = Tag.Attribs[i].Value;
            break;

         case HASH_onexit:
            if (!Tag.Attribs[i].Value.empty()) edit.OnExit = Tag.Attribs[i].Value;
            break;

         case HASH_onenter:
            if (!Tag.Attribs[i].Value.empty()) edit.OnEnter = Tag.Attribs[i].Value;
            break;

         default:
            if (Tag.Attribs[i].Name[0] IS '@') {
               edit.Args.emplace_back(make_pair(Tag.Attribs[i].Name, Tag.Attribs[i].Value));
            }
            else if (Tag.Attribs[i].Name[0] IS '_') {
               edit.Args.emplace_back(make_pair(Tag.Attribs[i].Name, Tag.Attribs[i].Value));
            }
      }
   }

   if (!name.empty()) Self->EditDefs[name] = std::move(edit);
}

//********************************************************************************************************************
// This very simple tag tells the parser that the object or link that immediately follows the focus element should
// have the initial focus when the user interacts with the document.  Commonly used for things such as input boxes.
//
// If the focus tag encapsulates any content, it will be processed in the same way as if it were to immediately follow
// the closing tag.
//
// Note that for hyperlinks, the 'select' attribute can also be used as a convenient means to assign focus.

static void tag_focus(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   Self->FocusIndex = Self->Tabs.size();
}

//********************************************************************************************************************

static void tag_footer(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   Self->FooterTag = &Children;
}

//********************************************************************************************************************

static void tag_header(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   Self->HeaderTag = &Children;
}

/*********************************************************************************************************************
** Indent document block.  The extent of the indentation can be customised in the Units value.
*/

static void tag_indent(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   bcParagraph esc;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch(Tag.Attribs[i].Name, "units")) {
         esc.Indent = StrToInt(Tag.Attribs[i].Name);
         if (esc.Indent < 0) esc.Indent = 0;
         for (LONG j=0; Tag.Attribs[i].Name[j]; j++) {
            if (Tag.Attribs[i].Name[j] IS '%') { esc.Relative = true; break; }
         }
      }
      else check_para_attrib(Self, Tag.Attribs[i].Name, Tag.Attribs[i].Value, &esc);
   }

   Self->insertCode(Index, esc);

      parse_tags(Self, XML, Children, Index);

   Self->reserveCode<bcParagraphEnd>(Index);
   Self->NoWhitespace = true;
}

//********************************************************************************************************************
// Use of <meta> for custom information is allowed and is ignored by the parser.

static void tag_head(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   // The head contains information about the document

   for (auto &scan : Tag.Children) {
      // Anything allocated here needs to be freed in unload_doc()
      if (!StrMatch("title", scan.name())) {
         if (scan.hasContent()) {
            if (Self->Title) FreeResource(Self->Title);
            Self->Title = StrClone(scan.Children[0].Attribs[0].Value.c_str());
         }
      }
      else if (!StrMatch("author", scan.name())) {
         if (scan.hasContent()) {
            if (Self->Author) FreeResource(Self->Author);
            Self->Author = StrClone(scan.Children[0].Attribs[0].Value.c_str());
         }
      }
      else if (!StrMatch("copyright", scan.name())) {
         if (scan.hasContent()) {
            if (Self->Copyright) FreeResource(Self->Copyright);
            Self->Copyright = StrClone(scan.Children[0].Attribs[0].Value.c_str());
         }
      }
      else if (!StrMatch("keywords", scan.name())) {
         if (scan.hasContent()) {
            if (Self->Keywords) FreeResource(Self->Keywords);
            Self->Keywords = StrClone(scan.Children[0].Attribs[0].Value.c_str());
         }
      }
      else if (!StrMatch("description", scan.name())) {
         if (scan.hasContent()) {
            if (Self->Description) FreeResource(Self->Description);
            Self->Description = StrClone(scan.Children[0].Attribs[0].Value.c_str());
         }
      }
   }
}

//********************************************************************************************************************
// Include XML from another RIPPLE file.

static void tag_include(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("src", Tag.Attribs[i].Name)) {
         if (auto xmlinc = objXML::create::integral(fl::Path(Tag.Attribs[i].Value), fl::Flags(XMF::PARSE_HTML|XMF::STRIP_HEADERS))) {
            parse_tags(Self, xmlinc, xmlinc->Tags, Index, Flags);
            Self->Resources.emplace_back(xmlinc->UID, RTD::OBJECT_TEMP);
         }
         else log.warning("Failed to include '%s'", Tag.Attribs[i].Value.c_str());
         return;
      }
   }

   log.warning("<include> directive missing required 'src' element.");
}

//********************************************************************************************************************

static void tag_parse(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   // The value attribute will contain XML.  We will parse the XML as if it were part of the document source.  This feature
   // is typically used when pulling XML information out of an object field.

   if (Tag.Attribs.size() > 1) {
      if ((!StrMatch("value", Tag.Attribs[1].Name)) or (!StrMatch("$value", Tag.Attribs[1].Name))) {
         log.traceBranch("Parsing string value as XML...");

         if (auto xmlinc = objXML::create::integral(fl::Statement(Tag.Attribs[1].Value), fl::Flags(XMF::PARSE_HTML|XMF::STRIP_HEADERS))) {
            parse_tags(Self, xmlinc, xmlinc->Tags, Index, Flags);

            // Add the created XML object to the document rather than destroying it

            Self->Resources.emplace_back(xmlinc->UID, RTD::OBJECT_TEMP);
         }
      }
   }
}

//********************************************************************************************************************
// Bitmap images are supported as vector rectangles with a texture map.  If the image is SVG then it is loaded into a
// viewport, so effectively the same, but scalable.
//
// Images are always inline by default, that is to say that they do not prevent content from appearing on either side.
// If content should be blocked on either side then the client can place the image within <p> tags.
//
// TODO: SVG images should be rendered to a cached bitmap texture so that they do not need to be redrawn unless
// changed to a higher resolution or otherwise modified.

static void tag_image(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   bcImage img;
   std::string src, icon;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      auto hash = StrHash(Tag.Attribs[i].Name);
      auto &value = Tag.Attribs[i].Value;
      if (hash IS HASH_src) {
         src = value;
      }
      else if (hash IS HASH_icon) {
         icon = value; // e.g. "items/checkmark"
      }
      else if (hash IS HASH_unicode) {
         // TODO: The unicode option reads the first glyph in the provided string and treats it in the same fashion
         // as the 'icon' attribute.
      }
      else if ((hash IS HASH_float) or (hash IS HASH_align)) {
         // Setting the horizontal alignment of an image will cause it to float above the text.
         // If the image is declared inside a paragraph, it will be completely de-anchored as a result.
         auto vh = StrHash(value);
         if (vh IS HASH_left)        img.align = ALIGN::LEFT;
         else if (vh IS HASH_right)  img.align = ALIGN::RIGHT;
         else if (vh IS HASH_center) img.align = ALIGN::CENTER;
         else if (vh IS HASH_middle) img.align = ALIGN::CENTER;
         else log.warning("Invalid alignment value '%s'", value.c_str());
      }
      else if (hash IS HASH_valign) {
         // If the image is anchored and the line is taller than the image, the image can be vertically aligned.
         auto vh = StrHash(value);
         if (vh IS HASH_top) img.align = ALIGN::TOP;
         else if (vh IS HASH_center) img.align = ALIGN::VERTICAL;
         else if (vh IS HASH_middle) img.align = ALIGN::VERTICAL;
         else if (vh IS HASH_bottom) img.align = ALIGN::BOTTOM;
         else log.warning("Invalid valign value '%s'", value.c_str());
      }
      else if (hash IS HASH_padding) {
         // Set padding values in clockwise order.  For percentages, the final value is calculated from the area of
         // the image itself (area being taken as the diagonal length).

         auto str = value.c_str();
         str = read_unit(str, img.pad.left, img.pad.left_pct);
         str = read_unit(str, img.pad.top, img.pad.top_pct);
         str = read_unit(str, img.pad.right, img.pad.right_pct);
         str = read_unit(str, img.pad.bottom, img.pad.bottom_pct);
         img.padding = true;
      }
      else if (hash IS HASH_width) {
         read_unit(value.c_str(), img.width, img.width_pct);
      }
      else if (hash IS HASH_height) {
         read_unit(value.c_str(), img.height, img.height_pct);
      }
      else log.warning("<image> unsupported attribute '%s'", Tag.Attribs[i].Name.c_str());
   }

   if (!icon.empty()) { // Load a vector image via a named SVG icon.
      if (img.width < 0) img.width = 0; // Zero is equivalent to 'auto', meaning on-the-fly computation
      if (img.height < 0) img.height = 0;

      if (auto pattern = objVectorPattern::create::global({
            fl::Owner(Self->Scene->UID),
            fl::PageWidth(8),
            fl::PageHeight(8),
            fl::SpreadMethod(VSPREAD::PAD)
         })) {

         objVectorViewport *pat_viewport;
         pattern->getPtr(FID_Viewport, &pat_viewport);

         // Load the SVG source file and target the VectorPattern

         std::string icon_path = "icons:" + icon + ".svg";
         if (auto svg = objSVG::create::integral({
               fl::Name("svg_image"),
               fl::Path(icon_path),
               fl::Target(pat_viewport)
            })) {

            FreeResource(svg);

            std::string name = "icon" + std::to_string(Self->UniqueID++);
            scAddDef(Self->Scene, name.c_str(), pattern);

            if (auto rect = objVectorRectangle::create::global({
                  fl::Name("rect_image"),
                  fl::Owner(Self->Page->UID),
                  fl::X(0), fl::Y(0), fl::Width(8), fl::Height(8), // Will be corrected during layout
                  fl::Fill("url(#" + name + ")")
               })) {

               Self->Resources.emplace_back(rect->UID, RTD::OBJECT_UNLOAD_DELAY);

               img.rect = rect;
               if (!img.floating()) Self->NoWhitespace = false; // Images count as characters when inline.
               Self->insertCode(Index, img);
               return;
            }
         }
         else log.warning("Failed to load '%s'", icon_path.c_str());

         FreeResource(pattern);
      }
   }
   else if (!src.empty()) { // Load a static bitmap image.
      if (auto pic = objPicture::create::integral({
            fl::Path(src),
            fl::BitsPerPixel(32),
            fl::Flags(PCF::FORCE_ALPHA_32)
         })) {

         std::string name = "img" + std::to_string(Self->UniqueID++);
         if (auto img_cache = objVectorImage::create::global({
               fl::Name(name),
               fl::Owner(Self->Scene->UID),
               fl::Picture(pic),
               fl::Units(VUNIT::BOUNDING_BOX),
               fl::SpreadMethod(VSPREAD::PAD)
            })) {
            scAddDef(Self->Scene, name.c_str(), img_cache);

            if (auto rect = objVectorRectangle::create::global({
                  fl::Owner(Self->Page->UID),
                  fl::X(0), fl::Y(0),
                  fl::Width(1), fl::Height(1),
                  fl::Fill("url(#" + name + ")")
               })) {

               Self->Resources.emplace_back(img_cache->UID, RTD::OBJECT_UNLOAD_DELAY);
               Self->Resources.emplace_back(pic->UID, RTD::OBJECT_UNLOAD_DELAY);
               Self->Resources.emplace_back(rect->UID, RTD::OBJECT_UNLOAD_DELAY);

               if (!img.floating()) Self->NoWhitespace = false; // Images count as characters when inline.
               img.rect = rect;
               Self->insertCode(Index, img);
               return;
            }

            FreeResource(img_cache);
         }
         FreeResource(pic);
      }
      else log.warning("Failed to load '%s'", src.c_str());
   }
   else {
      log.warning("No src or icon defined for <image> tag.");
      return;
   }
}

//********************************************************************************************************************
// Indexes set bookmarks that can be used for quick-scrolling to document sections.  They can also be used to mark
// sections of content that may require run-time modification.
//
// <index name="News">
//   <p>Something in here.</p>
// </index>
//
// If the name attribute is not specified, an attempt will be made to derive the name from the first immediate string
// of the index' content, e.g:
//
//   <index>News</>
//
// The developer can use indexes to bookmark areas of code that are of interest.  The FindIndex() method is used for
// this purpose.

static void tag_index(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   ULONG name = 0;
   bool visible = true;
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("name", Tag.Attribs[i].Name)) {
         name = StrHash(Tag.Attribs[i].Value);
      }
      else if (!StrMatch("hide", Tag.Attribs[i].Name)) {
         visible = false;
      }
      else log.warning("<index> unsupported attribute '%s'", Tag.Attribs[i].Name.c_str());
   }

   if ((!name) and (!Children.empty())) {
      if (Children[0].isContent()) name = StrHash(Children[0].Attribs[0].Value);
   }

   // This style check ensures that the font style is up to date before the start of the index.
   // This is important if the developer wants to insert content at the start of the index,
   // as that content should have the attributes of the current font style.

   style_check(Self, Index);

   if (name) {
      bcIndex esc(name, Self->UniqueID++, 0, visible, Self->Invisible ? false : true);

      Self->insertCode(Index, esc);

      if (!Children.empty()) {
         if (!visible) Self->Invisible++;
         parse_tags(Self, XML, Children, Index);
         if (!visible) Self->Invisible--;
      }

      bcIndexEnd end(esc.ID);
      Self->insertCode(Index, end);
   }
   else if (!Children.empty()) parse_tags(Self, XML, Children, Index);
}

//********************************************************************************************************************
// If calling a function with 'onclick', all arguments must be identified with the @ prefix.  Parameters will be
// passed to the function in the order in which they are given.  Global values can be set against the document
// object itself, if a parameter is prefixed with an underscore.
//
// Script objects can be specifically referenced when calling a function, e.g. "myscript.function".  If no script
// object is referenced, then it is assumed that the default script contains the function.
//
// <a href="http://" onclick="function" colour="rgb" @arg1="" @arg2="" _global=""/>
//
// Dummy links that specify neither an href or onclick value can be useful in embedded documents if the
// EventCallback feature is used.

static void tag_link(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   bcLink link;
   bool select = false;
   std::string colour, hint, pointermotion;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      switch (StrHash(Tag.Attribs[i].Name)) {
         case HASH_href:
            if (link.Type IS LINK::NIL) {
               link.Ref = Tag.Attribs[i].Value;
               link.Type = LINK::HREF;
            }
            break;

         case HASH_onclick:
            if (link.Type IS LINK::NIL) { // Function to execute on click
               link.Ref = Tag.Attribs[i].Value;
               link.Type = LINK::FUNCTION;
            }
            break;

         case HASH_hint:
         case HASH_title: // 'title' is the http equivalent of our 'hint'
            log.msg("No support for <a> hints yet.");
            hint = Tag.Attribs[i].Value;
            break;

         case HASH_colour:
            colour = Tag.Attribs[i].Value;
            break;

         case HASH_pointermotion: // Function to execute on pointer motion
            pointermotion = Tag.Attribs[i].Value;
            break;

         case HASH_select: select = true; break;

         default:
            if (Tag.Attribs[i].Name.starts_with('@')) link.Args.push_back(make_pair(Tag.Attribs[i].Name, Tag.Attribs[i].Value));
            else if (Tag.Attribs[i].Name.starts_with('_')) link.Args.push_back(make_pair(Tag.Attribs[i].Name, Tag.Attribs[i].Value));
            else log.warning("<a|link> unsupported attribute '%s'", Tag.Attribs[i].Name.c_str());
      }
   }

   std::ostringstream buffer;

   if ((link.Type != LINK::NIL) or (!Tag.Children.empty())) {
      link.ID    = ++Self->LinkID;
      link.Align = Self->Style.FontStyle.Options;

      auto pos = sizeof(link);
      if (link.Type IS LINK::FUNCTION) buffer << link.Ref << '\0';
      else buffer << link.Ref << '\0';

      if (!pointermotion.empty()) {
         link.PointerMotion = pos;
         buffer << pointermotion << '\0';
      }

      Self->insertCode(Index, link);

      auto savestatus = Self->Style;

      Self->Style.StyleChange        = true;
      Self->Style.FontStyle.Options |= FSO::UNDERLINE;

      if (!colour.empty()) Self->Style.FontStyle.Fill = colour;
      else Self->Style.FontStyle.Fill = Self->LinkFill;

      parse_tags(Self, XML, Tag.Children, Index);

      saved_style_check(Self, savestatus);

      Self->reserveCode<bcLinkEnd>(Index);

      // This style check will forcibly revert the font back to whatever it was rather than waiting for new content
      // to result in a change.  The reason why we want to do this is to make it easier to manage run-time insertion
      // of new content.  For instance if the user enters text on a new line following an <h1> heading, the user's
      // expectation would be for the new text to be in the format of the body's font and not the <h1> font.

      style_check(Self, Index);

      // Links are added to the list of tab-able points

      auto i = add_tabfocus(Self, TT_LINK, link.ID);
      if (select) Self->FocusIndex = i;
   }
   else parse_tags(Self, XML, Tag.Children, Index, Flags & (~IPF::FILTER_ALL));
}

//********************************************************************************************************************

static void tag_list(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);
   bcList esc, *savelist;

   esc.Fill    = Self->Style.FontStyle.Fill; // Default fill matches the current font colour
   esc.ItemNum = esc.Start;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("fill", Tag.Attribs[i].Name)) {
         esc.Fill = Tag.Attribs[i].Value;
      }
      else if (!StrMatch("indent", Tag.Attribs[i].Name)) {
         // Affects the indenting to apply to child items.
         esc.BlockIndent = StrToInt(Tag.Attribs[i].Value);
      }
      else if (!StrMatch("vspacing", Tag.Attribs[i].Name)) {
         esc.VSpacing = StrToFloat(Tag.Attribs[i].Value);
         if (esc.VSpacing < 0) esc.VSpacing = 0;
      }
      else if (!StrMatch("type", Tag.Attribs[i].Name)) {
         if (!StrMatch("bullet", Tag.Attribs[i].Value)) {
            esc.Type = bcList::BULLET;
         }
         else if (!StrMatch("ordered", Tag.Attribs[i].Value)) {
            esc.Type = bcList::ORDERED;
            esc.ItemIndent = 0;
         }
         else if (!StrMatch("custom", Tag.Attribs[i].Value)) {
            esc.Type = bcList::CUSTOM;
            esc.ItemIndent = 0;
         }
      }
      else log.msg("Unknown list attribute '%s'", Tag.Attribs[i].Name.c_str());
   }

   style_check(Self, Index); // Font changes must take place prior to the list for correct bullet point alignment

   // Note: Paragraphs are not inserted because <li> does this

   Self->insertCode(Index, esc);

   savelist = Self->Style.List;
   Self->Style.List = &esc;

      if (!Children.empty()) parse_tags(Self, XML, Children, Index);

   Self->Style.List = savelist;

   Self->reserveCode<bcListEnd>(Index);

   Self->NoWhitespace = true;
}

//********************************************************************************************************************
// Also see check_para_attrib() for paragraph attributes.

static void tag_paragraph(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   Self->ParagraphDepth++;

   bcParagraph esc;
   esc.LeadingRatio = 0;

   auto savestatus = Self->Style;
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("align", Tag.Attribs[i].Name)) {
         if ((!StrMatch(Tag.Attribs[i].Value, "center")) or (!StrMatch(Tag.Attribs[i].Value, "horizontal"))) {
            Self->Style.StyleChange = true;
            Self->Style.FontStyle.Options |= FSO::ALIGN_CENTER;
         }
         else if (!StrMatch(Tag.Attribs[i].Value, "right")) {
            Self->Style.StyleChange = true;
            Self->Style.FontStyle.Options |= FSO::ALIGN_RIGHT;
         }
         else log.warning("Alignment type '%s' not supported.", Tag.Attribs[i].Value.c_str());
      }
      else check_para_attrib(Self, Tag.Attribs[i].Name, Tag.Attribs[i].Value, &esc);
   }

   Self->insertCode(Index, esc);

   Self->NoWhitespace = esc.Trim;

   parse_tags(Self, XML, Children, Index);
   saved_style_check(Self, savestatus);

   bcParagraphEnd end;
   Self->insertCode(Index, end);
   Self->NoWhitespace = true;

   // This style check will forcibly revert the font back to whatever it was rather than waiting for new content to
   // result in a change.  The reason why we want to do this is to make it easier to manage run-time insertion of new
   // content.  For instance if the user enters text on a new line following an <h1> heading, the user's
   // expectation would be for the new text to be in the format of the body's font and not the <h1> font.

   style_check(Self, Index);

   Self->ParagraphDepth--;
}

//********************************************************************************************************************

static void tag_print(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   // Copy the content from the value attribute into the document stream.  If used inside an object, the data is sent
   // to that object as XML.

   if (Tag.Attribs.size() > 1) {
      auto tagname = Tag.Attribs[1].Name.c_str();
      if (*tagname IS '$') tagname++;

      if (!StrMatch("value", tagname)) {
         if (Self->CurrentObject) {
            acDataText(Self->CurrentObject, Tag.Attribs[1].Value.c_str());
         }
         else {
            insert_text(Self, Index, Tag.Attribs[1].Value, (Self->Style.FontStyle.Options & FSO::PREFORMAT) != FSO::NIL);
         }
      }
      else if (!StrMatch("src", Tag.Attribs[1].Name)) {
         // This option is only supported in unrestricted mode
         if ((Self->Flags & DCF::UNRESTRICTED) != DCF::NIL) {
            CacheFile *cache;
            if (!LoadFile(Tag.Attribs[1].Value.c_str(), LDF::NIL, &cache)) {
               insert_text(Self, Index, std::string((CSTRING)cache->Data), (Self->Style.FontStyle.Options & FSO::PREFORMAT) != FSO::NIL);
               UnloadFile(cache);
            }
         }
         else log.warning("Cannot <print src.../> unless in unrestricted mode.");
      }
   }
}

//********************************************************************************************************************
// Sets the attributes of an object.  NOTE: For security reasons, this feature is limited to objects that are children
// of the document object.
//
//   <set object="" fields .../>
//
//   <set arg=value .../>
//
// Note: XML validity could be improved restricting the set tag so that args were set as <set arg="argname"
// value="value"/>, however apart from being more convoluted, this would also result in more syntactic cruft as each
// arg setting would require its own set element.

static void tag_set(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   if (Tag.Attribs.size() > 1) {
      if (!StrMatch("object", Tag.Attribs[1].Name)) {
         OBJECTID objectid;
         if (!FindObject(Tag.Attribs[1].Value.c_str(), 0, FOF::SMART_NAMES, &objectid)) {
            if (valid_objectid(Self, objectid)) {
               pf::ScopedObjectLock object(objectid, 3000);
               if (object.granted()) {
                  for (unsigned i=2; i < Tag.Attribs.size(); i++) {
                     log.trace("tag_set:","#%d %s = '%s'", objectid, Tag.Attribs[i].Name, Tag.Attribs[i].Value);

                     auto fid = StrHash(Tag.Attribs[i].Name[0] IS '@' ? Tag.Attribs[i].Name.c_str()+1 : Tag.Attribs[i].Name.c_str());
                     object->set(fid, Tag.Attribs[i].Value);
                  }
               }
            }
         }
      }
      else {
         // Set document arguments
         for (unsigned i=1; i < Tag.Attribs.size(); i++) {
            if (Tag.Attribs[i].Name[0] IS '@') {
               acSetVar(Self, Tag.Attribs[i].Name.c_str()+1, Tag.Attribs[i].Value.c_str());
            }
            else acSetVar(Self, Tag.Attribs[i].Name.c_str(), Tag.Attribs[i].Value.c_str());
         }
      }
   }
}

//********************************************************************************************************************

static void tag_template(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   // Templates can be used to create custom tags.
   //
   // <template name="customimage">
   //   <image src="" background="#f0f0f0"/>
   // </template>

   if (!Self->InTemplate) add_template(Self, XML, Tag);
}

//********************************************************************************************************************
// Used to send XML data to an embedded object.
//
// NOTE: If no child tags or content is inside the XML string, or if attributes are attached to the XML tag, then the
// user is trying to create a new XML object (under the Data category), not the XML reserved word.

static void tag_xml(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   tag_xml_content(Self, XML, Tag, PXF::ARGS);
}

static void tag_xmlraw(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   tag_xml_content(Self, XML, Tag, PXF::NIL);
}

static void tag_xmltranslate(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   tag_xml_content(Self, XML, Tag, PXF::TRANSLATE|PXF::ARGS);
}

//********************************************************************************************************************
// For use the by tag_xml*() range of functions only.  Forwards <xml> data sections to a target object via XML data
// channels.  Content will be translated only if requested by the caller.

static void tag_xml_content(extDocument *Self, objXML *XML, XMLTag &Tag, PXF Flags)
{
   pf::Log log(__FUNCTION__);

   if (Tag.Children.empty()) return;

   OBJECTPTR target = Self->CurrentObject;
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("object", Tag.Attribs[i].Name)) {
         OBJECTID id;
         if (!FindObject(Tag.Attribs[i].Value.c_str(), 0, FOF::NIL, &id)) {
            target = GetObjectPtr(id);
            if (!valid_object(Self, target)) return;
         }
         else return;
      }
   }

   DLAYOUT("XML: %d, Tag: %d, Target: %d", XML->UID, Tag.ID, target->UID);

   if (!target) {
      log.warning("<xml> used without a valid object reference to receive the XML.");
      return;
   }

   STRING xmlstr;
   if (!xmlGetString(XML, Tag.ID, XMF::INCLUDE_SIBLINGS, &xmlstr)) {
      if ((Flags & (PXF::ARGS|PXF::TRANSLATE)) != PXF::NIL) {
         std::string str(xmlstr);
         translate_args(Self, str, str);

         if ((Flags & PXF::TRANSLATE) != PXF::NIL) tag_xml_content_eval(Self, str);

         acDataXML(target, str.c_str());
      }
      else acDataXML(target, xmlstr);

      FreeResource(xmlstr);
   }
}

//********************************************************************************************************************

static ERROR calc(const std::string &String, DOUBLE *Result, std::string &Output)
{
   enum SIGN { PLUS=1, MINUS, MULTIPLY, DIVIDE, MODULO };

   if (Result) *Result = 0;

   Output.clear();

   // Search for brackets and translate them first

   std::string in(String);
   while (true) {
      // Find the last bracketed reference

      LONG last_bracket = 0;
      for (unsigned i=0; i < in.size(); i++) {
         if (in[i] IS '\'') { // Skip anything in quotes
            i++;
            while (in[i]) {
               if (in[i] IS '\\') {
                  i++; // Skip backslashes and the following character
                  if (!in[i]) break;
               }
               else if (in[i] IS '\'') break;
               i++;
            }
            if (in[i] IS '\'') i++;
         }
         else if (in[i] IS '(') last_bracket = i;
      }

      if (last_bracket > 0) { // Bracket found, translate its contents
         LONG end;
         for (end=last_bracket+1; (in[end]) and (in[end-1] != ')'); end++);
         std::string buf(in, last_bracket, end - last_bracket);

         DOUBLE calc_float;
         std::string out;
         calc(buf.c_str()+1, &calc_float, out);
         in.replace(last_bracket, end - last_bracket, out);
      }
      else break;
   }

   // Perform the calculation

   STRING end;
   WORD precision = 9;
   DOUBLE total   = 0;
   DOUBLE overall = 0;
   LONG index     = 0;
   SIGN sign      = PLUS;
   bool number    = false;
   for (unsigned s=0; in[s];) {
      if (in[s] <= 0x20); // Do nothing with whitespace
      else if (in[s] IS '\'') {
         if (number) { // Write the current floating point number to the buffer before the next calculation
            Output  += write_calc(total, precision);
            overall += total; // Reset the number
            total   = 0;
            number  = false;
         }

         s++;
         while (index < LONG(Output.size())-1) {
            if (in[s] IS '\\') s++; // Skip the \ character and continue so that we can copy the character immediately after it
            else if (in[s] IS '\'') break;

            Output += in[s++];
         }
      }
      else if (in[s] IS 'f') { // Fixed floating point precision adjustment
         s++;
         precision = -strtol(in.c_str() + s, &end, 10);
         s += end - in.c_str();
         continue;
      }
      else if (in[s] IS 'p') { // Floating point precision adjustment
         s++;
         precision = strtol(in.c_str() + s, &end, 10);
         s += end - in.c_str();
         continue;
      }
      else if ((in[s] >= '0') and (in[s] <= '9')) {
         number = true;
         DOUBLE fvalue = strtod(in.c_str() + s, &end);
         s += end - in.c_str();

         if (sign IS MINUS)         total = total - fvalue;
         else if (sign IS MULTIPLY) total = total * fvalue;
         else if (sign IS MODULO)   total = F2I(total) % F2I(fvalue);
         else if (sign IS DIVIDE) {
            if (fvalue) total = total / fvalue; // NB: Avoid division by zero errors
         }
         else total += fvalue;

         sign = PLUS; // The mathematical sign is reset whenever a number is encountered
         continue;
      }
      else if (in[s] IS '-') {
         if (sign IS MINUS) sign = PLUS; // Handle double-negatives
         else sign = MINUS;
      }
      else if (in[s] IS '+') sign = PLUS;
      else if (in[s] IS '*') sign = MULTIPLY;
      else if (in[s] IS '/') sign = DIVIDE;
      else if (in[s] IS '%') sign = MODULO;

      for (++s; (in[s] & 0xc0) IS 0x80; s++);
   }

   if (number) Output += write_calc(total, precision);
   if (Result) *Result = overall + total;
   return ERR_Okay;
}

/*********************************************************************************************************************

This function is used to translate strings that make object and field references using the standard referencing format.
References are made to objects by enclosing statements within square brackets.  As a result of calling this function,
all references within the Buffer will be translated to their relevant format.  The Buffer needs to be large enough to
accommodate these adjustments as it will be expanded during the translation.  It is recommended that the Buffer is at
least two times the actual length of the string that you are translating.

Valid references can be made to an object by name, ID or relative parameters.  Here are some examples illustrating the
different variations:

<types type="Reference">
<type name="[surface]">Name reference.</>
<type name="[#49302]">ID reference.</>
<type name="[self]">Relative reference to the object that has the current context, or the document.</>
</table>

Field references are a slightly different matter and will be converted to the value of the field that they are
referencing.  A field reference is defined using the object referencing format, but they contain a `.fieldname`
extension.  Here are some examples:

<pre>
[surface.width]
[file.location]
</pre>

A string such as `[mywindow.height] + [mywindow.width]` could be translated to `255 + 120` for instance.  References to
string based fields can expand the Buffer very quickly, which is why large buffer spaces are recommended for all-purpose
translations.

Simple calculations are possible by enclosing a statement within a `[=...]` section.  For example the aforementioned
string can be expanded to `[=[mywindow.height] + [mywindow.width]]`, which would give a result of 375.

The escape character for string translation is `$` and should be used as `[$...]`, which prevents everything within the
square brackets from being translated.  The `[$]` characters will be removed as part of this process unless the
KEEP_ESCAPE flag is used.  To escape a single right or left bracket, use `[rb]` or `[lb]` respectively.

*********************************************************************************************************************/

// Evaluate object references and calculations

static ERROR tag_xml_content_eval(extDocument *Self, std::string &Buffer)
{
   pf::Log log(__FUNCTION__);
   LONG i;

   // Quick check for translation symbols

   if (Buffer.find('[') IS std::string::npos) return ERR_EmptyString;

   log.traceBranch("%.80s", Buffer.c_str());

   ERROR error = ERR_Okay;
   ERROR majorerror = ERR_Okay;

   // Skip to the end of the buffer (translation occurs 'backwards')

   auto pos = LONG(Buffer.size() - 1);
   while (pos >= 0) {
      if ((Buffer[pos] IS '[') and ((Buffer[pos+1] IS '@') or (Buffer[pos+1] IS '%'))) {
         // Ignore arguments, e.g. [@id] or [%id].  It's also useful for ignoring [@attrib] in xpath.
         pos--;
      }
      else if (Buffer[pos] IS '[') {
         // Make sure that there is a balanced closing bracket

         LONG end;
         LONG balance = 0;
         for (end=pos; Buffer[end]; end++) {
            if (Buffer[end] IS '[') balance++;
            else if (Buffer[end] IS ']') {
               balance--;
               if (!balance) break;
            }
         }

         if (Buffer[end] != ']') {
            log.warning("Unbalanced string: %.90s ...", Buffer.c_str());
            return ERR_InvalidData;
         }

         if (Buffer[pos+1] IS '=') { // Perform a calculation
            std::string num;
            num.assign(Buffer, pos+2, end-(pos+2));

            std::string calcbuffer;
            DOUBLE value;
            calc(num, &value, calcbuffer);
            Buffer.insert(end-pos+1, calcbuffer);
         }
         else if (Buffer[pos+1] IS '$') { // Escape sequence - e.g. translates [$ABC] to ABC.  Note: Use [rb] and [lb] instead for brackets.
            Buffer.erase(end, 1); // ']'
            Buffer.erase(pos, 2); // '[$'
            pos--;
            continue;
         }
         else {
            std::string name;
            name.reserve(64);

            for (i=pos+1; (Buffer[i] != '.') and (i < end); i++) {
               name += std::tolower(Buffer[i]);
            }

            // Check for [lb] and [rb] escape codes

            char code = 0;
            if (name == "rb") code = ']';
            else if (name == "lb") code = '[';

            if (code) {
               Buffer[pos] = code;
               Buffer.erase(pos+1, 3);
               pos--;
               continue;
            }
            else {
               OBJECTID objectid = 0;
               if (!StrMatch(name, "self")) objectid = CurrentContext()->UID;
               else FindObject(name.c_str(), 0, FOF::SMART_NAMES, &objectid);

               if (objectid) {
                  OBJECTPTR object = NULL;
                  if (Buffer[i] IS '.') {
                     // Get the field from the object
                     i++;

                     std::string field(Buffer, i, end);
                     if (!AccessObject(objectid, 2000, &object)) {
                        OBJECTPTR target;
                        const Field *classfield;
                        if (((classfield = find_field(object, field.c_str(), &target))) and (classfield->Flags & FD_STRING)) {
                           CSTRING str;
                           if (!GetField(object, (FIELD)classfield->FieldID|TSTR, &str)) {
                              Buffer.insert(end-pos+1, str);
                           }
                        }
                        else { // Get field as an unlisted type and manage any buffer overflow
                           std::string tbuffer;
                           tbuffer.reserve(4096);
repeat:
                           tbuffer[tbuffer.capacity()-1] = 0;
                           if (!GetFieldVariable(object, field.c_str(), tbuffer.data(), tbuffer.capacity())) {
                              if (tbuffer[tbuffer.capacity()-1]) {
                                 tbuffer.reserve(tbuffer.capacity() * 2);
                                 goto repeat;
                              }
                              Buffer.insert(end-pos+1, tbuffer);
                           }
                        }
                        // NB: For fields, error code is always Okay so that the reference evaluates to NULL

                        ReleaseObject(object);
                     }
                     else error = ERR_AccessObject;
                  }
                  else { // Convert the object reference to an ID
                     Buffer.insert(end-pos+1, std::string("#") + std::to_string(objectid));
                  }
               }
               else {
                  error = ERR_NoMatchingObject;
                  log.traceWarning("Failed to find object '%s'", name.c_str());
               }
            }
         }

         if (error != ERR_Okay) {
            pos--;
            majorerror = error;
            error = ERR_Okay;
         }
      }
      else pos--;
   }

   log.trace("Result: %s", Buffer.c_str());

   return majorerror;
}

//********************************************************************************************************************

static void tag_font(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   auto savestatus = Self->Style;
   bool preformat = false;
   auto flags = IPF::NIL;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("colour", Tag.Attribs[i].Name)) {
         Self->Style.StyleChange = true;
         Self->Style.FontStyle.Fill = Tag.Attribs[i].Value;
      }
      else if (!StrMatch("face", Tag.Attribs[i].Name)) {
         Self->Style.FaceChange = true;

         auto j = Tag.Attribs[i].Value.find(':');
         if (j != std::string::npos) { // Point size follows
            auto str = Tag.Attribs[i].Value.c_str();
            j++;
            Self->Style.Point = StrToInt(str+j);
            j = Tag.Attribs[i].Value.find(':', j);
            if (j != std::string::npos) { // Style follows
               j++;
               if (!StrMatch("bold", str+j)) {
                  Self->Style.FaceChange = true;
                  Self->Style.FontStyle.Options |= FSO::BOLD;
               }
               else if (!StrMatch("italic", str+j)) {
                  Self->Style.FaceChange = true;
                  Self->Style.FontStyle.Options |= FSO::ITALIC;
               }
               else if (!StrMatch("bold italic", str+j)) {
                  Self->Style.FaceChange = true;
                  Self->Style.FontStyle.Options |= FSO::BOLD|FSO::ITALIC;
               }
            }
         }

         Self->Style.Face = Tag.Attribs[i].Value.substr(0, j);
      }
      else if (!StrMatch("size", Tag.Attribs[i].Name)) {
         Self->Style.FaceChange = true;
         Self->Style.Point = StrToFloat(Tag.Attribs[i].Value);
      }
      else if (!StrMatch("style", Tag.Attribs[i].Name)) {
         if (!StrMatch("bold", Tag.Attribs[i].Value)) {
            Self->Style.FaceChange = true;
            Self->Style.FontStyle.Options |= FSO::BOLD;
         }
         else if (!StrMatch("italic", Tag.Attribs[i].Value)) {
            Self->Style.FaceChange = true;
            Self->Style.FontStyle.Options |= FSO::ITALIC;
         }
         else if (!StrMatch("bold italic", Tag.Attribs[i].Value)) {
            Self->Style.FaceChange = true;
            Self->Style.FontStyle.Options |= FSO::BOLD|FSO::ITALIC;
         }
      }
      else if (!StrMatch("preformat", Tag.Attribs[i].Name)) {
         Self->Style.StyleChange = true;
         Self->Style.FontStyle.Options |= FSO::PREFORMAT;
         preformat = true;
         flags |= IPF::STRIP_FEEDS;
      }
   }

   parse_tags(Self, XML, Children, Index, flags);

   saved_style_check(Self, savestatus);

   if (preformat) trim_preformat(Self, Index);
}

//********************************************************************************************************************

static void tag_vector(extDocument *Self, const std::string &pagetarget, CLASSID class_id, XMLTag *Template,
   objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   // NF::INTEGRAL is only set when the object is owned by the document

   OBJECTPTR object;
   if (NewObject(class_id, (Self->CurrentObject) ? NF::NIL : NF::INTEGRAL, &object)) {
      log.warning("Failed to create object of class #%d.", class_id);
      return;
   }

   log.branch("Processing %s object from document tag, owner #%d.", object->Class->ClassName, Self->CurrentObject ? Self->CurrentObject->UID : -1);

   // Setup the callback interception so that we can control the order in which objects draw their graphics to the surface.

   if (Self->CurrentObject) {
      SetOwner(object, Self->CurrentObject);
   }
   else if (!pagetarget.empty()) {
      auto field_id = StrHash(pagetarget);
      if (Self->BkgdGfx) object->set(field_id, Self->View);
      else object->set(field_id, Self->Page);
   }

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      auto argname = Tag.Attribs[i].Name.c_str();
      while (*argname IS '$') argname++;
      if (Tag.Attribs[i].Value.empty()) object->set(StrHash(argname), "1");
      else object->set(StrHash(argname), Tag.Attribs[i].Value);
   }

   // Check for the 'data' tag which can be used to send data feed information prior to initialisation.
   //
   // <data type="text">Content</data>
   // <data type="xml" template="TemplateName"/>
   // <data type="xml" object="[xmlobj]"/>
   // <data type="xml">Content</data>

   bool customised = false;
   /*
   if (!Tag.Children.empty()) {
      STRING src;

      for (auto &scan : Tag.Children) {
         if (StrMatch("data", scan.Attribs[0].Name)) continue;

         if (*e_revert > *s_revert) {
            while (*e_revert > *s_revert) {
               *e_revert -= 1;
               Self->RestoreAttrib[*e_revert].Attrib[0] = Self->RestoreAttrib[*e_revert].String;
            }
         }
         Self->RestoreAttrib.resize(*s_revert);

         *s_revert = Self->RestoreAttrib.size();
         *e_revert = 0;
         translate_attrib_args(Self, scan.Attribs);
         *e_revert = Self->RestoreAttrib.size();

         const std::string *type = scan.attrib("type");

         if ((!type) or (!StrMatch("text", type))) {
            if (scan.Children.empty()) continue;
            std::string buffer;
            xmlGetContent(scan, buffer);
            if (!buffer.empty()) acDataText(object, buffer.c_str());
         }
         else if (!StrMatch("xml", type)) {
            customised = true;

            if (auto t = scan.attrib("template")) {
               for (auto &tmp : Self->Templates->Tags) {
                  for (unsigned i=1; i < tmp.Attribs.size(); i++) {
                     if ((!StrMatch("Name", tmp.Attribs[i].Name)) and (!StrMatch(t, tmp.Attribs[i].Value))) {
                        if (!xmlGetString(Self->Templates, tmp.Children[0].ID, XMF::INCLUDE_SIBLINGS|XMF::STRIP_CDATA, &content)) {
                           acDataXML(object, content.c_str());
                        }

                        break;
                     }
                  }
               }
            }
            else if (auto src = scan.attrib("object")) {
               OBJECTID objectid;
               if (!FindObject(src.c_str(), 0, FOF::SMART_NAMES, &objectid)) {
                  if ((objectid) and (valid_objectid(Self, objectid))) {
                     objXML *objxml;
                     if (!AccessObject(objectid, 3000, &objxml)) {
                        if (objxml->Class->ClassID IS ID_XML) {
                           if (!xmlGetString(objxml, 0, XMF::INCLUDE_SIBLINGS|XMF::STRIP_CDATA, &content)) {
                              acDataXML(object, content.c_str());
                           }
                        }
                        else log.warning("Cannot extract XML data from a non-XML object.");
                        ReleaseObject(objxml);
                     }
                  }
                  else log.warning("Invalid object reference '%s'", src);
               }
               else log.warning("Unable to find object '%s'", src);
            }
            else {
               if (scan.Children.empty()) continue;
               if (!xmlGetString(XML, scan.Children.ID, XMF::INCLUDE_SIBLINGS|XMF::STRIP_CDATA, &content)) {
                  acDataXML(object, content.c_str());
               }
            }
         }
         else log.warning("Unsupported data type '%s'", type);
      }
   }
   */
   // Feeds are applied to invoked objects, whereby the object's class name matches a feed.

   if ((!customised) and (Template)) {
      STRING content;
      if (!xmlGetString(Self->Templates, Template->Children[0].ID, XMF::INCLUDE_SIBLINGS|XMF::STRIP_CDATA, &content)) {
         acDataXML(object, content);
         FreeResource(content);
      }
   }

   if (!InitObject(object)) {
      bcVector escobj;

      if (Self->Invisible) acHide(object); // Hide the object if it's in an invisible section

      // Child tags are processed as normal, but are applied with respect to the object.  Any tags that reflect
      // document content are passed to the object as XML.

      if (!Tag.Children.empty()) {
         pf::Log log(__FUNCTION__);
         log.traceBranch("Processing child tags for object #%d.", object->UID);
         auto prevobject = Self->CurrentObject;
         Self->CurrentObject = object;
         parse_tags(Self, XML, Tag.Children, Index, Flags & (~IPF::FILTER_ALL));
         Self->CurrentObject = prevobject;
      }

      if (&Children != &Tag.Children) {
         pf::Log log(__FUNCTION__);
         log.traceBranch("Processing further child tags for object #%d.", object->UID);
         auto prevobject = Self->CurrentObject;
         Self->CurrentObject = object;
         parse_tags(Self, XML, Children, Index, Flags & (~IPF::FILTER_ALL));
         Self->CurrentObject = prevobject;
      }

      // The object can self-destruct in ClosingTag(), so check that it still exists before inserting it into the text stream.

      if (!CheckObjectExists(object->UID)) {
         if (Self->BkgdGfx) {
            auto &resource = Self->Resources.emplace_back(object->UID, RTD::OBJECT_UNLOAD);
            resource.ClassID = class_id;
         }
         else {
            escobj.ObjectID = object->UID;
            escobj.ClassID  = object->Class->ClassID;
            escobj.Inline = false;
            if (Self->CurrentObject) escobj.Owned = true;

            // By default objects are assumed to be in the background (thus not embedded as part of the text stream).
            // This section is intended to confirm the graphical state of the object.

            if (object->Class->ClassID IS ID_VECTOR) {
               //if (layout->Layout & (LAYOUT_BACKGROUND|LAYOUT_FOREGROUND));
               //else if (layout->Layout & LAYOUT_EMBEDDED) escobj.Inline = true;
            }
            else escobj.Inline = true; // If the layout object is not present, the object is managing its own graphics and likely is embedded (button, combobox, checkbox etc are like this)

            style_check(Self, Index);
            Self->insertCode(Index, escobj);

            if (Self->ObjectCache) {
               switch (object->Class->ClassID) {
                  // The following class types can be cached
                  case ID_XML:
                  case ID_FILE:
                  case ID_CONFIG:
                  case ID_COMPRESSION:
                  case ID_SCRIPT: {
                     Self->Resources.emplace_back(object->UID, RTD::PERSISTENT_OBJECT);
                     break;
                  }

                  // The following class types use their own internal caching system

                  default:
                     log.warning("Cannot cache object of class type '%s'", object->Class->ClassName);
                  //case ID_IMAGE:
                  //   auto &res = Self->Resources.emplace_back(object->UID, RTD::OBJECT_UNLOAD);
                     break;
               }
            }
            else {
               auto &res = Self->Resources.emplace_back(object->UID, RTD::OBJECT_UNLOAD);
               res.ClassID = class_id;
            }

            // If the object is inline, we will allow whitespace to immediately follow the object.

            if (escobj.Inline) Self->NoWhitespace = false;

            // Add the object to the tab-list if it is in our list of classes that support keyboard input.

            static const CLASSID classes[] = { ID_VECTOR };

            for (unsigned i=0; i < ARRAYSIZE(classes); i++) {
               if (classes[i] IS class_id) {
                  add_tabfocus(Self, TT_OBJECT, object->UID);
                  break;
               }
            }
         }
      }
      else log.trace("Object %d self-destructed.", object->UID);
   }
   else {
      FreeResource(object);
      log.warning("Failed to initialise object of class $%.8x", class_id);
   }
}

//********************************************************************************************************************

static void tag_pre(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
//   bcParagraph para;
//   Self->insertCode(Index, para);

   if ((Self->Style.FontStyle.Options & FSO::PREFORMAT) IS FSO::NIL) {
      auto savestatus = Self->Style;
      Self->Style.StyleChange = true;
      Self->Style.FontStyle.Options |= FSO::PREFORMAT;
      parse_tags(Self, XML, Children, Index, IPF::STRIP_FEEDS);
      saved_style_check(Self, savestatus);
   }
   else parse_tags(Self, XML, Children, Index, IPF::STRIP_FEEDS);

   trim_preformat(Self, Index);

//   Self->reserveCode<bcParagraphEnd>(Index);
//   Self->NoWhitespace = true;
}

//********************************************************************************************************************
// By default, a script will be activated when the parser encounters it in the document.  If the script returns a
// result string, that result is assumed to be valid XML and is processed by the parser as such.
//
// If the script contains functions, those functions can be called at any time, either during the parsing process or
// when the document is displayed.
//
// The first script encountered by the parser will serve as the default source for all function calls.  If you need to
// call functions in other scripts then you need to access them by name - e.g. 'myscript.function()'.
//
// Only the first section of content enclosed within the <script> tag (CDATA) is accepted by the script parser.

static void tag_script(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);
   objScript *script;
   ERROR error;

   std::string type = "fluid";
   std::string src, cachefile, name;
   bool defaultscript = false;
   bool persistent = false;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      auto tagname = Tag.Attribs[i].Name.c_str();
      if (*tagname IS '$') tagname++;
      if (*tagname IS '@') continue; // Variables are set later

      if (!StrMatch("type", tagname)) {
         type = Tag.Attribs[i].Value;
      }
      else if (!StrMatch("persistent", tagname)) {
         // A script that is marked as persistent will survive refreshes
         persistent = true;
      }
      else if (!StrMatch("src", tagname)) {
         if (safe_file_path(Self, Tag.Attribs[i].Value)) {
            src = Tag.Attribs[i].Value;
         }
         else {
            log.warning("Security violation - cannot set script src to: %s", Tag.Attribs[i].Value.c_str());
            return;
         }
      }
      else if (!StrMatch("cachefile", tagname)) {
         // Currently the security risk of specifying a cache file is that you could overwrite files on the user's PC,
         // so for the time being this requires unrestricted mode.

         if ((Self->Flags & DCF::UNRESTRICTED) != DCF::NIL) {
            cachefile = Tag.Attribs[i].Value;
         }
         else {
            log.warning("Security violation - cannot set script cachefile to: %s", Tag.Attribs[i].Value.c_str());
            return;
         }
      }
      else if (!StrMatch("name", tagname)) {
         name = Tag.Attribs[i].Value;
      }
      else if (!StrMatch("postprocess", tagname)) {
         log.warning("--- PostProcess mode for scripts is obsolete - please use the PageProcessed event trigger or call an initialisation function directly ---");
      }
      else if (!StrMatch("default", tagname)) {
         defaultscript = true;
      }
      else if (!StrMatch("external", tagname)) {
         // Reference an external script as the default for function calls
         if ((Self->Flags & DCF::UNRESTRICTED) != DCF::NIL) {
            OBJECTID id;
            if (!FindObject(Tag.Attribs[i].Value.c_str(), 0, FOF::NIL, &id)) {
               Self->DefaultScript = GetObjectPtr(id);
               return;
            }
            else {
               log.warning("Failed to find external script '%s'", Tag.Attribs[i].Value.c_str());
               return;
            }
         }
         else {
            log.warning("Security violation - cannot reference external script '%s'", Tag.Attribs[i].Value.c_str());
            return;
         }
      }
   }

   if ((persistent) and (name.empty())) name = "mainscript";

   if (src.empty()) {
      if ((Tag.Children.empty()) or (!Tag.Children[0].Attribs[0].Name.empty()) or (Tag.Children[0].Attribs[0].Value.empty())) {
         // Ignore if script holds no content
         log.warning("<script/> tag does not contain content.");
         return;
      }
   }

   // If the script is persistent and already exists in the resource cache, do nothing further.

   if (persistent) {
      for (auto &resource : Self->Resources) {
         if (resource.Type IS RTD::PERSISTENT_SCRIPT) {
            script = (objScript *)GetObjectPtr(resource.ObjectID);
            if (!StrMatch(name, script->Name)) {
               log.msg("Persistent script discovered.");
               if ((!Self->DefaultScript) or (defaultscript)) Self->DefaultScript = script;
               return;
            }
         }
      }
   }

   if (!StrMatch("fluid", type)) {
      error = NewObject(ID_FLUID, NF::INTEGRAL, &script);
   }
   else {
      error = ERR_NoSupport;
      log.warning("Unsupported script type '%s'", type.c_str());
   }

   if (!error) {
      if (!name.empty()) SetName(script, name.c_str());

      if (!src.empty()) script->setPath(src);
      else {
         std::string content = xmlGetContent(Tag);
         if (!content.empty()) script->setStatement(content);
      }

      if (!cachefile.empty()) script->setCacheFile(cachefile);

      // Object references are to be limited in scope to the Document object

      //script->setObjectScope(Self->Head.UID);

      // Pass custom arguments in the script tag

      for (unsigned i=1; i < Tag.Attribs.size(); i++) {
         auto tagname = Tag.Attribs[i].Name.c_str();
         if (*tagname IS '$') tagname++;
         if (*tagname IS '@') acSetVar(script, tagname+1, Tag.Attribs[i].Value.c_str());
      }

      if (!InitObject(script)) {
         // Pass document arguments to the script

         std::unordered_map<std::string, std::string> *vs;
         if (!script->getPtr(FID_Variables, &vs)) {
            Self->Vars   = *vs;
            Self->Params = *vs;
         }

         if (!acActivate(script)) { // Persistent scripts survive refreshes.
            Self->Resources.emplace_back(script->UID, persistent ? RTD::PERSISTENT_SCRIPT : RTD::OBJECT_UNLOAD_DELAY);

            if ((!Self->DefaultScript) or (defaultscript)) {
               log.msg("Script #%d is the default script for this document.", script->UID);
               Self->DefaultScript = script;
            }

            // Any results returned from the script are processed as XML

            CSTRING *results;
            LONG size;
            if ((!GetFieldArray(script, FID_Results, &results, &size)) and (size > 0)) {
               auto xmlinc = objXML::create::global(fl::Statement(results[0]), fl::Flags(XMF::PARSE_HTML|XMF::STRIP_HEADERS));
               if (xmlinc) {
                  parse_tags(Self, xmlinc, xmlinc->Tags, Index, Flags);

                  // Add the created XML object to the document rather than destroying it

                  Self->Resources.emplace_back(xmlinc->UID, RTD::OBJECT_TEMP);
               }
            }
         }
         else FreeResource(script);
      }
      else FreeResource(script);
   }
}

//********************************************************************************************************************
// Similar to <font/>, but the original font state is never saved and restored.

static void tag_setfont(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      switch (StrHash(Tag.Attribs[i].Name)) {
         case HASH_colour:
            Self->Style.StyleChange = true;
            Self->Style.FontStyle.Fill = Tag.Attribs[i].Value;
            break;

         case HASH_face:
            Self->Style.FaceChange = true;
            Self->Style.Face = Tag.Attribs[i].Value;
            break;

         case HASH_size:
            Self->Style.FaceChange = true;
            Self->Style.Point = StrToFloat(Tag.Attribs[i].Value);
            break;

         case HASH_style:
            if (!StrMatch("bold", Tag.Attribs[i].Value)) {
               Self->Style.FaceChange = true;
               Self->Style.FontStyle.Options |= FSO::BOLD;
            }
            else if (!StrMatch("italic", Tag.Attribs[i].Value)) {
               Self->Style.FaceChange = true;
               Self->Style.FontStyle.Options |= FSO::ITALIC;
            }
            else if (!StrMatch("bold italic", Tag.Attribs[i].Value)) {
               Self->Style.FaceChange = true;
               Self->Style.FontStyle.Options |= FSO::BOLD|FSO::ITALIC;
            }
            break;

         case HASH_preformat:
            Self->Style.StyleChange = true;
            Self->Style.FontStyle.Options |= FSO::PREFORMAT;
            break;
      }
   }

   //style_check(Self, Index);
}

//********************************************************************************************************************

static void tag_setmargins(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   bcSetMargins margins;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("top", Tag.Attribs[i].Name)) {
         margins.Top = StrToInt(Tag.Attribs[i].Value);
         if (margins.Top < -4000) margins.Top = -4000;
         else if (margins.Top > 4000) margins.Top = 4000;
      }
      else if (!StrMatch("bottom", Tag.Attribs[i].Name)) {
         margins.Bottom = StrToInt(Tag.Attribs[i].Value);
         if (margins.Bottom < -4000) margins.Bottom = -4000;
         else if (margins.Bottom > 4000) margins.Bottom = 4000;
      }
      else if (!StrMatch("right", Tag.Attribs[i].Name)) {
         margins.Right = StrToInt(Tag.Attribs[i].Value);
         if (margins.Right < -4000) margins.Right = -4000;
         else if (margins.Right > 4000) margins.Right = 4000;
      }
      else if (!StrMatch("left", Tag.Attribs[i].Name)) {
         margins.Left = StrToInt(Tag.Attribs[i].Value);
         if (margins.Left < -4000) margins.Left = -4000;
         else if (margins.Left > 4000) margins.Left = 4000;
      }
      else if (!StrMatch("all", Tag.Attribs[i].Name)) {
         LONG value;
         value = StrToInt(Tag.Attribs[i].Value);
         if (value < -4000) value = -4000;
         else if (value > 4000) value = 4000;
         margins.Left = margins.Top = margins.Right = margins.Bottom = value;
      }
   }

   Self->insertCode(Index, margins);
}

//********************************************************************************************************************

static void tag_savestyle(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   //style_check(Self, Index);
   Self->RestoreStyle = Self->Style; // Save the current style
}

//********************************************************************************************************************

static void tag_restorestyle(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   Self->Style = Self->RestoreStyle; // Restore the saved style
   Self->Style.FaceChange = true;
   //style_check(Self, Index);
}

//********************************************************************************************************************

static void tag_italic(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   if ((Self->Style.FontStyle.Options & FSO::ITALIC) IS FSO::NIL) {
      auto savestatus = Self->Style;
      Self->Style.FaceChange = true; // Italic fonts are typically a different typeset
      Self->Style.FontStyle.Options |= FSO::ITALIC;
      parse_tags(Self, XML, Children, Index);
      saved_style_check(Self, savestatus);
   }
   else parse_tags(Self, XML, Children, Index);
}

//********************************************************************************************************************
// List item parser

static void tag_li(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   if (!Self->Style.List) {
      log.warning("<li> not used inside a <list> tag.");
      return;
   }

   bcParagraph para;

   para.ListItem     = true;
   para.LeadingRatio = 0;
   para.applyStyle(Self->Style);

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      auto tagname = Tag.Attribs[i].Name.c_str();
      if (*tagname IS '$') tagname++;

      if (!StrMatch("value", tagname)) {
         para.Value = Tag.Attribs[i].Value;
      }
      else if (!StrMatch("leading", tagname)) {
         para.LeadingRatio = StrToFloat(Tag.Attribs[i].Value);
         if (para.LeadingRatio < MIN_LEADING) para.LeadingRatio = MIN_LEADING;
         else if (para.LeadingRatio > MAX_LEADING) para.LeadingRatio = MAX_LEADING;
      }
      else if (!StrMatch("vspacing", tagname)) {
         para.VSpacing = StrToFloat(Tag.Attribs[i].Value);
         if (para.VSpacing < MIN_LEADING) para.VSpacing = MIN_LEADING;
         else if (para.VSpacing > MAX_VSPACING) para.VSpacing = MAX_VSPACING;
      }
   }

   if ((Self->Style.List->Type IS bcList::CUSTOM) and (!para.Value.empty())) {
      style_check(Self, Index); // Font changes must take place prior to the printing of custom string items

      Self->insertCode(Index, para);

         parse_tags(Self, XML, Children, Index);

      Self->reserveCode<bcParagraphEnd>(Index);
   }
   else if (Self->Style.List->Type IS bcList::ORDERED) {
      style_check(Self, Index); // Font changes must take place prior to the printing of custom string items

      auto list_size = Self->Style.List->Buffer.size();
      Self->Style.List->Buffer.push_back(std::to_string(Self->Style.List->ItemNum) + ".");

      auto save_item = Self->Style.List->ItemNum;
      Self->Style.List->ItemNum = 1;

      Self->insertCode(Index, para);

         parse_tags(Self, XML, Children, Index);

      Self->reserveCode<bcParagraphEnd>(Index);

      Self->Style.List->ItemNum = save_item;
      Self->Style.List->Buffer.resize(list_size);
   }
   else {
      Self->insertCode(Index, para);

         parse_tags(Self, XML, Children, Index);

      Self->reserveCode<bcParagraphEnd>(Index);
      Self->NoWhitespace = true;
   }
}

//********************************************************************************************************************

static void tag_underline(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   if ((Self->Style.FontStyle.Options & FSO::UNDERLINE) IS FSO::NIL) {
      auto savestatus = Self->Style;
      Self->Style.StyleChange = true;
      Self->Style.FontStyle.Options |= FSO::UNDERLINE;
      parse_tags(Self, XML, Children, Index);
      saved_style_check(Self, savestatus);
   }
   else {
      auto parse_flags = Flags & (~IPF::FILTER_ALL);
      parse_tags(Self, XML, Children, Index, parse_flags);
   }
}

//********************************************************************************************************************

static void tag_repeat(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);
   LONG loopstart = 0;
   LONG loopend = 0;
   LONG count = 0;
   LONG step  = 0;
   std::string indexname;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("start", Tag.Attribs[i].Name)) {
         loopstart = StrToInt(Tag.Attribs[i].Value);
         if (loopstart < 0) loopstart = 0;
      }
      else if (!StrMatch("count", Tag.Attribs[i].Name)) {
         count = StrToInt(Tag.Attribs[i].Value);
         if (count < 0) {
            log.warning("Invalid count value of %d", count);
            return;
         }
      }
      else if (!StrMatch("end", Tag.Attribs[i].Name)) {
         loopend = StrToInt(Tag.Attribs[i].Value) + 1;
      }
      else if (!StrMatch("step", Tag.Attribs[i].Name)) {
         step = StrToInt(Tag.Attribs[i].Value);
      }
      else if (!StrMatch("index", Tag.Attribs[i].Name)) {
         // If an index name is specified, the programmer will need to refer to it as [@indexname] and [%index] will
         // remain unchanged from any parent repeat loop.

         indexname = Tag.Attribs[i].Value;
      }
   }

   if (!step) {
      if (loopend < loopstart) step = -1;
      else step = 1;
   }

   // Validation - ensure that it will be possible for the repeat loop to execute correctly without the chance of
   // infinite looping.
   //
   // If the user set both count and end attributes, the count attribute will be given the priority here.

   if (count > 0) loopend = loopstart + (count * step);

   if (step > 0) {
      if (loopend < loopstart) step = -step;
   }
   else if (loopend > loopstart) step = -step;

   log.traceBranch("Performing a repeat loop (start: %d, end: %d, step: %d).", loopstart, loopend, step);

   auto saveindex = Self->LoopIndex;

   while (loopstart < loopend) {
      if (indexname.empty()) Self->LoopIndex = loopstart;
      else {
         SetVar(Self, indexname.c_str(), std::to_string(loopstart).c_str());
      }

      parse_tags(Self, XML, Tag.Children, Index, Flags);
      loopstart += step;
   }

   if (indexname.empty()) Self->LoopIndex = saveindex;

   log.trace("insert_child:","Repeat loop ends.");
}

//********************************************************************************************************************
// <table columns="10%,90%" width="100" height="100" colour="#808080">
//  <row><cell>Activate<brk/>This activates the object.</cell></row>
//  <row><cell span="2">Reset</cell></row>
// </table>
//
// <table width="100" height="100" colour="#808080">
//  <cell>Activate</cell><cell>This activates the object.</cell>
//  <cell colspan="2">Reset</cell>
// </table>
//
// Columns:      The minimum width of each column in the table.
// Width/Height: Minimum width and height of the table.
// Colour:       Background colour for the table.
// Border:       Border colour for the table (see thickness).
// Thickness:    Thickness of the border colour.
//
// The only acceptable child tags inside a <table> section are row, brk and cell tags.  Command tags are acceptable
// (repeat, if statements, etc).  The table byte code is typically generated as ESC::TABLE_START, ESC::ROW, ESC::CELL...,
// ESC::ROW_END, ESC::TABLE_END.

static void tag_table(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   auto &start = Self->reserveCode<bcTable>(Index);
   start.MinWidth  = 1;
   start.MinHeight = 1;

   std::string columns;
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      switch (StrHash(Tag.Attribs[i].Name)) {
         case HASH_columns:
            // Column preferences are processed only when the end of the table marker has been reached.
            columns = Tag.Attribs[i].Value;
            break;

         case HASH_width:
            start.MinWidth = StrToInt(Tag.Attribs[i].Value);
            start.WidthPercent = false;
            if (Tag.Attribs[i].Value.find_first_of('%') != std::string::npos) {
               start.WidthPercent = true;
            }
            if (start.MinWidth < 1) start.MinWidth = 1;
            else if (start.MinWidth > 10000) start.MinWidth = 10000;
            break;

         case HASH_height:
            start.MinHeight = StrToInt(Tag.Attribs[i].Value);
            if (Tag.Attribs[i].Value.find_first_of('%') != std::string::npos) {
               start.HeightPercent = true;
            }
            if (start.MinHeight < 1) start.MinHeight = 1;
            else if (start.MinHeight > 10000) start.MinHeight = 10000;
            break;

         case HASH_fill:
            start.Fill = Tag.Attribs[i].Value;
            break;

         case HASH_stroke:
            start.Stroke = Tag.Attribs[i].Value;
            if (start.Thickness < 1) start.Thickness = 1;
            break;

         case HASH_spacing: // Spacing between the cells
            start.CellVSpacing = StrToInt(Tag.Attribs[i].Value);
            if (start.CellVSpacing < 0) start.CellVSpacing = 0;
            else if (start.CellVSpacing > 200) start.CellVSpacing = 200;
            start.CellHSpacing = start.CellVSpacing;
            break;

         case HASH_thin: // Thin tables do not have spacing (defined by 'spacing' or 'hspacing') on the sides
            start.Thin = true;
            break;

         case HASH_vspacing: // Spacing between the cells
            start.CellVSpacing = StrToInt(Tag.Attribs[i].Value);
            if (start.CellVSpacing < 0) start.CellVSpacing = 0;
            else if (start.CellVSpacing > 200) start.CellVSpacing = 200;
            break;

         case HASH_hspacing: // Spacing between the cells
            start.CellHSpacing = StrToInt(Tag.Attribs[i].Value);
            if (start.CellHSpacing < 0) start.CellHSpacing = 0;
            else if (start.CellHSpacing > 200) start.CellHSpacing = 200;
            break;

         case HASH_margins:
         case HASH_padding: // Padding inside the cells
            start.CellPadding = StrToInt(Tag.Attribs[i].Value);
            if (start.CellPadding < 0) start.CellPadding = 0;
            else if (start.CellPadding > 200) start.CellPadding = 200;
            break;

         case HASH_thickness: {
            auto j = StrToInt(Tag.Attribs[i].Value);
            if (j < 0) j = 0;
            else if (j > 255) j = 255;
            start.Thickness = j;
            break;
         }
      }
   }

   auto savevar = Self->Style.Table;
   process_table var;
   Self->Style.Table = &var;
   Self->Style.Table->bcTable = &start;

      parse_tags(Self, XML, Tag.Children, Index, IPF::NO_CONTENT|IPF::FILTER_TABLE);

   Self->Style.Table = savevar;

   if (!columns.empty()) { // The columns value, if supplied is arranged as a CSV list of column widths
      std::vector<std::string> list;
      for (unsigned i=0; i < columns.size(); ) {
         auto end = columns.find(',', i);
         if (end IS std::string::npos) end = columns.size();
         auto val = columns.substr(i, end-i);
         trim(val);
         list.push_back(val);
         i = end + 1;
      }

      unsigned i;
      for (i=0; (i < start.Columns.size()) and (i < list.size()); i++) {
         start.Columns[i].PresetWidth = StrToFloat(list[i]);
         if (list[i].find_first_of('%') != std::string::npos) start.Columns[i].PresetWidthRel = true;
      }

      if (i < start.Columns.size()) log.warning("Warning - columns attribute '%s' did not define %d columns.", columns.c_str(), LONG(start.Columns.size()));
   }

   bcTableEnd end;
   Self->insertCode(Index, end);

   //style_check(Self, Index);
   //Self->Style.StyleChange = false;

   Self->NoWhitespace = true; // Setting this to true will prevent the possibility of blank spaces immediately following the table.
}

//********************************************************************************************************************

static void tag_row(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   if (!Self->Style.Table) {
      log.warning("<row> not defined inside <table> section.");
      Self->Error = ERR_InvalidData;
      return;
   }

   bcRow escrow;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("height", Tag.Attribs[i].Name)) {
         escrow.MinHeight = StrToInt(Tag.Attribs[i].Value);
         if (escrow.MinHeight < 0) escrow.MinHeight = 0;
         else if (escrow.MinHeight > 4000) escrow.MinHeight = 4000;
      }
      else if (!StrMatch("fill", Tag.Attribs[i].Name))   escrow.Fill   = Tag.Attribs[i].Value;
      else if (!StrMatch("stroke", Tag.Attribs[i].Name)) escrow.Stroke = Tag.Attribs[i].Value;
   }

   Self->insertCode(Index, escrow);
   Self->Style.Table->bcTable->Rows++;
   Self->Style.Table->RowCol = 0;

   if (!Children.empty()) {
      parse_tags(Self, XML, Children, Index, IPF::NO_CONTENT|IPF::FILTER_ROW);
   }

   bcRowEnd end;
   Self->insertCode(Index, end);

   if (Self->Style.Table->RowCol > LONG(Self->Style.Table->bcTable->Columns.size())) {
      Self->Style.Table->bcTable->Columns.resize(Self->Style.Table->RowCol);
   }
}

//********************************************************************************************************************

static void tag_cell(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);
   style_status savestatus;
   static UBYTE edit_recurse = 0;

   if (!Self->Style.Table) {
      log.warning("<cell> not defined inside <table> section.");
      Self->Error = ERR_InvalidData;
      return;
   }

   bcCell cell(Self->UniqueID++, Self->Style.Table->RowCol);
   bool select = false;
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      switch (StrHash(Tag.Attribs[i].Name)) {
         case HASH_colspan:
            cell.ColSpan = StrToInt(Tag.Attribs[i].Value);
            if (cell.ColSpan < 1) cell.ColSpan = 1;
            else if (cell.ColSpan > 1000) cell.ColSpan = 1000;
            break;

         case HASH_rowspan:
            cell.RowSpan = StrToInt(Tag.Attribs[i].Value);
            if (cell.RowSpan < 1) cell.RowSpan = 1;
            else if (cell.RowSpan > 1000) cell.RowSpan = 1000;
            break;

         case HASH_edit: {
            if (edit_recurse) {
               log.warning("Edit cells cannot be embedded recursively.");
               Self->Error = ERR_Recursion;
               return;
            }
            cell.EditDef = Tag.Attribs[i].Value;

            if (!Self->EditDefs.contains(Tag.Attribs[i].Value)) {
               log.warning("Edit definition '%s' does not exist.", Tag.Attribs[i].Value.c_str());
               cell.EditDef.clear();
            }

            break;
         }

         case HASH_select: select = true; break;

         case HASH_fill: cell.Fill = Tag.Attribs[i].Value; break;

         case HASH_stroke: cell.Stroke = Tag.Attribs[i].Value; break;

         case HASH_nowrap:
            Self->Style.StyleChange = true;
            Self->Style.FontStyle.Options |= FSO::NO_WRAP;
            break;

         case HASH_onclick:
            cell.OnClick = Tag.Attribs[i].Value;
            break;

         default:
            if (Tag.Attribs[i].Name.starts_with('@')) {
               cell.Args.emplace_back(std::make_pair(Tag.Attribs[i].Name.substr(1), Tag.Attribs[i].Value));
            }
            else if (Tag.Attribs[i].Name.starts_with('_')) {
               cell.Args.emplace_back(std::make_pair(Tag.Attribs[i].Name, Tag.Attribs[i].Value));
            }
      }
   }

   if (!cell.EditDef.empty()) edit_recurse++;

   // Edit sections enforce preformatting, which means that all whitespace entered by the user
   // will be taken into account.  The following check sets FSO::PREFORMAT if it hasn't been set already.

   auto cell_index = Index;

   Self->insertCode(Index, cell);

   auto parse_flags = Flags & (~(IPF::NO_CONTENT|IPF::FILTER_ALL));
   if (!Children.empty()) {
      Self->NoWhitespace = true; // Reset whitespace flag: false allows whitespace at the start of the cell, true prevents whitespace

      if ((!cell.EditDef.empty()) and ((Self->Style.FontStyle.Options & FSO::PREFORMAT) IS FSO::NIL)) {
         savestatus = Self->Style;
         Self->Style.StyleChange = true;
         Self->Style.FontStyle.Options |= FSO::PREFORMAT;
         parse_tags(Self, XML, Children, Index, parse_flags);
         saved_style_check(Self, savestatus);
      }
      else parse_tags(Self, XML, Children, Index, parse_flags);
   }

   Self->Style.Table->RowCol += cell.ColSpan;

   bcCellEnd esccell_end;
   esccell_end.CellID = cell.CellID;
   Self->insertCode(Index, esccell_end);

   if (!cell.EditDef.empty()) {
      // Links are added to the list of tabbable points

      LONG tab = add_tabfocus(Self, TT_EDIT, cell.CellID);
      if (select) Self->FocusIndex = tab;
   }

   if (!cell.EditDef.empty()) edit_recurse--;
}

//********************************************************************************************************************
// This instruction can only be used from within a template.

static void tag_inject(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);
   if (Self->InTemplate) {
      if (Self->InjectTag) {
         parse_tags(Self, Self->InjectXML, Self->InjectTag[0], Index, Flags);
      }
   }
   else log.warning("<inject/> request detected but not used inside a template.");
}

//********************************************************************************************************************
// No response is required for page tags, but we can check for validity.

static void tag_page(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);
   if (auto name = Tag.attrib("name")) {
      auto str = name->c_str();
      while (*str) {
         if (((*str >= 'A') and (*str <= 'Z')) or
             ((*str >= 'a') and (*str <= 'z')) or
             ((*str >= '0') and (*str <= '9'))) {
            // Character is valid
         }
         else {
            log.warning("Page has an invalid name of '%s'.  Character support is limited to [A-Z,a-z,0-9].", name->c_str());
            break;
         }
         str++;
      }
   }
}

/*********************************************************************************************************************
** Usage: <trigger event="resize" function="script.function"/>
*/

static void tag_trigger(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, StreamChar &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);
   DRT trigger_code;
   OBJECTPTR script;
   LARGE function_id;

   std::string event, function_name;
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      switch (StrHash(Tag.Attribs[i].Name)) {
         case HASH_event: event = Tag.Attribs[i].Value; break;
         case HASH_function: function_name = Tag.Attribs[i].Value; break;
      }
   }

   if ((!event.empty()) and (!function_name.empty())) {
      // These are described in the documentation for the AddListener method

      switch(StrHash(event)) {
         case HASH_AfterLayout:       trigger_code = DRT::AFTER_LAYOUT; break;
         case HASH_BeforeLayout:      trigger_code = DRT::BEFORE_LAYOUT; break;
         case HASH_UserClick:         trigger_code = DRT::USER_CLICK; break;
         case HASH_UserClickRelease:  trigger_code = DRT::USER_CLICK_RELEASE; break;
         case HASH_UserMovement:      trigger_code = DRT::USER_MOVEMENT; break;
         case HASH_Refresh:           trigger_code = DRT::REFRESH; break;
         case HASH_GotFocus:          trigger_code = DRT::GOT_FOCUS; break;
         case HASH_LostFocus:         trigger_code = DRT::LOST_FOCUS; break;
         case HASH_LeavingPage:       trigger_code = DRT::LEAVING_PAGE; break;
         case HASH_PageProcessed:     trigger_code = DRT::PAGE_PROCESSED; break;
         default:
            log.warning("Trigger event '%s' for function '%s' is not recognised.", event.c_str(), function_name.c_str());
            return;
      }

      // Get the script

      std::string args;
      if (!extract_script(Self, function_name.c_str(), &script, function_name, args)) {
         if (!scGetProcedureID(script, function_name.c_str(), &function_id)) {
            Self->Triggers[LONG(trigger_code)].emplace_back(make_function_script(script, function_id));
         }
         else log.warning("Unable to resolve '%s' in script #%d to a function ID (the procedure may not exist)", function_name.c_str(), script->UID);
      }
      else log.warning("The script for '%s' is not available - check if it is declared prior to the trigger tag.", function_name.c_str());
   }
}
