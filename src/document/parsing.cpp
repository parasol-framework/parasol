
static void check_para_attrib(extDocument *, const std::string &, const std::string &, bc_paragraph *);

//********************************************************************************************************************
// Check for a pending font and/or style change and respond appropriately.

static void style_check(extDocument *Self, stream_char &Cursor)
{
   if (Self->Style.face_change) {
      // Create a new font object for the current style

      auto style_name = get_font_style(Self->Style.font_style.options);
      Self->Style.font_style.font_index = create_font(Self->Style.face, style_name, Self->Style.point);
      //log.trace("Changed font to index %d, face %s, style %s, size %d.", Self->Style.font_style.index, Self->Style.Face, style_name, Self->Style.point);
      Self->Style.face_change  = false;
      Self->Style.style_change = true;
   }

   if (Self->Style.style_change) { // Insert a font change into the text stream
      // NB: Assigning a new UID is suboptimal in cases where we are reverting to a previously registered state
      // (i.e. anywhere where saved_style_check() has been used).  We could allow insert_code() to lookup formerly
      // allocated UID's and save some memory usage if we improved the management of saved styles.
      Self->Style.font_style.uid = glByteCodeID++;
      Self->insert_code(Cursor, Self->Style.font_style);
      Self->Style.style_change = false;
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
   std::string test(String, 0, i);

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
            satisfied = valid_objectid(Self, object_id) ? true : false;
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
//   IPF::NO_CONTENT: XML content data will be ignored
//   IPF::STRIP_FEEDS: Strip line feeds from content

static TRF parse_tag(extDocument *Self, objXML *XML, XMLTag &Tag, stream_char &Index, IPF &Flags)
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
   auto tag_hash = StrHash(tagname);
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
                  insert_text(Self, Index, content, ((Self->Style.font_style.options & FSO::PREFORMAT) != FSO::NIL));
               }
               else insert_text(Self, Index, Tag.Attribs[0].Value, ((Self->Style.font_style.options & FSO::PREFORMAT) != FSO::NIL));
            }
            Flags &= ~IPF::STRIP_FEEDS;
         }
         else if (Self->CurrentObject) {
            if (XML IS Self->InjectXML) acDataContent(Self->CurrentObject, Tag.Attribs[0].Value.c_str());
         }
         else if (Self->ParagraphDepth) { // We must be in a paragraph to accept content as text
            insert_text(Self, Index, Tag.Attribs[0].Value, ((Self->Style.font_style.options & FSO::PREFORMAT) != FSO::NIL));
         }
      }
      Tag.Attribs = saved_attribs;
      return result;
   }

   if (Self->Templates) { // Check for templates first, as they can be used to override the default RPL tag names.
      if (Self->RefreshTemplates) {
         Self->TemplateIndex.clear();

         for (XMLTag &scan : Self->Templates->Tags) {
            for (unsigned i=0; i < scan.Attribs.size(); i++) {
               if (!StrMatch("name", scan.Attribs[i].Name)) {
                  Self->TemplateIndex[StrHash(scan.Attribs[i].Value)] = &scan;
               }
            }
         }

         Self->RefreshTemplates = false;
      }

      if (Self->TemplateIndex.contains(tag_hash)) {
         // Process the template by jumping into it.  Arguments in the tag are added to a sequential
         // list that will be processed in reverse by translate_attrib_args().

         init_template block(Self, Tag.Children, XML); // Required for the <inject/> feature to work inside the template

         pf::Log log(__FUNCTION__);
         log.traceBranch("Executing template '%s'.", tagname.c_str());

         Self->TemplateArgs.push_back(&Tag);
         parse_tags(Self, Self->Templates, Self->TemplateIndex[tag_hash]->Children, Index, Flags);
         Self->TemplateArgs.pop_back();

         Tag.Attribs = saved_attribs;
         return result;
      }
   }

   if (auto tag = glTags.find(tag_hash); tag != glTags.end()) {
      auto &tr = tag->second;
      if (((tr.flags & TAG::FILTER_ALL) != TAG::NIL) and ((tr.flags & TAG(filter)) IS TAG::NIL)) {
         // A filter applies to this tag and the filter flags do not match
         log.warning("Invalid use of tag '%s' - Not applied to the correct tag parent.", tagname.c_str());
         Self->Error = ERR_InvalidData;
      }
      else if (tr.routine) {
         //log.traceBranch("%s", tagname);

         if ((Self->CurrentObject) and ((tr.flags & (TAG::OBJECTOK|TAG::CONDITIONAL)) IS TAG::NIL)) {
            log.warning("Illegal use of tag %s within object of class '%s'.", tagname.c_str(), Self->CurrentObject->Class->ClassName);
            result = TRF::BREAK;
         }
         else {
            if (((Flags & IPF::NO_CONTENT) != IPF::NIL) and ((tr.flags & TAG::CONTENT) != TAG::NIL)) {
               // Do nothing when content is not allowed
               log.trace("Content disabled on '%s', tag not processed.", tagname.c_str());
            }
            else if ((tr.flags & TAG::CHILDREN) != TAG::NIL) {
               // Child content is compulsory or tag has no effect
               if (!Tag.Children.empty()) tr.routine(Self, XML, Tag, Tag.Children, Index, Flags);
               else log.trace("No content found in tag '%s'", tagname.c_str());
            }
            else tr.routine(Self, XML, Tag, Tag.Children, Index, Flags);

         }
      }
   }
   else if (HASH_break IS tag_hash) {
      // Breaking stops executing all tags (within this section) beyond the breakpoint.  If in a loop, the loop
      // will stop executing.

      result = TRF::BREAK;
   }
   else if (HASH_continue IS tag_hash) {
      // Continuing - does the same thing as a break but the loop continues.
      // If used when not in a loop, then all sibling tags are skipped.

      result = TRF::CONTINUE;
   }
   else if (HASH_if IS tag_hash) {
      if (check_tag_conditions(Self, Tag)) { // Statement is true
         Flags &= ~IPF::CHECK_ELSE;
         result = parse_tags(Self, XML, Tag.Children, Index, Flags);
      }
      else Flags |= IPF::CHECK_ELSE;
   }
   else if (HASH_elseif IS tag_hash) {
      if ((Flags & IPF::CHECK_ELSE) != IPF::NIL) {
         if (check_tag_conditions(Self, Tag)) { // Statement is true
            Flags &= ~IPF::CHECK_ELSE;
            result = parse_tags(Self, XML, Tag.Children, Index, Flags);
         }
      }
   }
   else if (HASH_else IS tag_hash) {
      if ((Flags & IPF::CHECK_ELSE) != IPF::NIL) {
         Flags &= ~IPF::CHECK_ELSE;
         result = parse_tags(Self, XML, Tag.Children, Index, Flags);
      }
   }
   else if (HASH_while IS tag_hash) {
      auto saveindex = Self->LoopIndex;
      Self->LoopIndex = 0;

      if ((!Tag.Children.empty()) and (check_tag_conditions(Self, Tag))) {
         // Save/restore the statement string on each cycle to fully evaluate the condition each time.

         bool state = true;
         while (state) {
            state = check_tag_conditions(Self, Tag);
            Tag.Attribs = saved_attribs;
            translate_attrib_args(Self, Tag.Attribs);

            if ((state) and ((parse_tags(Self, XML, Tag.Children, Index, Flags) & TRF::BREAK) != TRF::NIL)) break;

            Self->LoopIndex++;
         }
      }

      Self->LoopIndex = saveindex;
   }
   else if ((Flags & IPF::NO_CONTENT) IS IPF::NIL) {
      log.warning("Tag '%s' unsupported as an instruction or template.", tagname.c_str());
   }
   else log.warning("Unrecognised tag '%s' used in a content-restricted area.", tagname.c_str());

   Tag.Attribs = saved_attribs;
   return result;
}

//********************************************************************************************************************

static TRF parse_tags(extDocument *Self, objXML *XML, objXML::TAGS &Tags, stream_char &Index, IPF Flags)
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

static void check_para_attrib(extDocument *Self, const std::string &Attrib, const std::string &Value, bc_paragraph *esc)
{
   switch (StrHash(Attrib)) {
      case HASH_inline:
      case HASH_anchor: // DEPRECATED
         Self->Style.style_change = true;
         Self->Style.font_style.options |= FSO::IN_LINE;
         break;

      case HASH_leading:
         if (esc) {
            esc->leading_ratio = StrToFloat(Value);
            if (esc->leading_ratio < MIN_LEADING) esc->leading_ratio = MIN_LEADING;
            else if (esc->leading_ratio > MAX_LEADING) esc->leading_ratio = MAX_LEADING;
         }
         break;

      case HASH_nowrap:
         Self->Style.style_change = true;
         Self->Style.font_style.options |= FSO::NO_WRAP;
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
            Self->Style.style_change = true;
            Self->Style.font_style.valign = (Self->Style.font_style.valign & (ALIGN::TOP|ALIGN::VERTICAL|ALIGN::BOTTOM)) | align;
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
         if (esc) esc->trim = true;
         break;

      case HASH_vspacing:
         // Vertical spacing between embedded paragraphs.  Ratio is expressed as a measure of the *default* lineheight (not the height of the
         // last line of the paragraph).  E.g. 1.5 is one and a half times the standard lineheight.  The default is 1.0.

         if (esc) {
            esc->vspacing = StrToFloat(Value);
            if (esc->vspacing < MIN_VSPACING) esc->vspacing = MIN_VSPACING;
            else if (esc->vspacing > MAX_VSPACING) esc->vspacing = MAX_VSPACING;
         }
         break;

      case HASH_indent:
         if (esc) {
            read_unit(Value.c_str(), esc->indent, esc->relative);
            if (esc->indent < 0) esc->indent = 0;
         }
         break;
   }
}

//********************************************************************************************************************

static void trim_preformat(extDocument *Self, stream_char &Index)
{
   auto i = Index.index - 1;
   for (; i > 0; i--) {
      if (Self->Stream[i].code IS SCODE::TEXT) {
         auto &text = stream_data<bc_text>(Self, i);

         static const std::string ws(" \t\f\v\n\r");
         auto found = text.text.find_last_not_of(ws);
         if (found != std::string::npos) {
            text.text.erase(found + 1);
            break;
         }
         else text.text.clear();
      }
      else break;
   }
}

/*********************************************************************************************************************
** This function is used to manage hierarchical styling:
**
** + Save font Style
**   + Execute child tags
** + Restore font Style
**
** If the last style that comes out of parse_tag() does not match the style stored in SaveStatus, we need to record a
** style change.
*/

static void saved_style_check(extDocument *Self, style_status &SavedStatus)
{
   auto face_change = Self->Style.face_change;
   auto style = Self->Style.style_change;

   if (SavedStatus.font_style.font_index != Self->Style.font_style.font_index) face_change = true;

   if ((SavedStatus.font_style.options != Self->Style.font_style.options) or
       (SavedStatus.font_style.fill != Self->Style.font_style.fill) or
       (SavedStatus.font_style.valign != Self->Style.font_style.valign)) {
      style = true;
   }

   if ((face_change) or (style)) {
      Self->Style = SavedStatus; // Restore the style that we had before processing the children

      // Reapply the fontstate and stylestate information

      Self->Style.face_change  = face_change;
      Self->Style.style_change = style;
   }
}

//********************************************************************************************************************
// Advances the cursor.  It is only possible to advance positively on either axis.

static void tag_advance(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   auto &adv = Self->reserve_code<bc_advance>(Index);

   for (unsigned i = 1; i < Tag.Attribs.size(); i++) {
      switch (StrHash(Tag.Attribs[i].Name)) {
         case HASH_x: adv.x = StrToInt(Tag.Attribs[i].Value); break;
         case HASH_y: adv.y = StrToInt(Tag.Attribs[i].Value); break;
      }
   }

   if (adv.x < 0) adv.x = 0;
   else if (adv.x > 4000) adv.x = 4000;

   if (adv.y < 0) adv.y = 0;
   else if (adv.y > 4000) adv.y = 4000;
}

//********************************************************************************************************************
// NB: If a <body> tag contains any children, it is treated as a template and must contain an <inject/> tag so that
// the XML insertion point is known.

static void tag_body(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   static const LONG MAX_BODY_MARGIN = 500;

   // Body tag needs to be placed before any content

   for (unsigned i = 1; i < Tag.Attribs.size(); i++) {
      switch (StrHash(Tag.Attribs[i].Name)) {
         case HASH_link:
            Self->LinkFill = Tag.Attribs[i].Value;
            break;

         case HASH_vlink:
            Self->VisitedLinkFill = Tag.Attribs[i].Value;
            break;

         case HASH_selectcolour: // Colour to use when a link is selected (using the tab key to get to a link will select it)
            Self->LinkSelectFill = Tag.Attribs[i].Value;
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
            log.msg("Page width forced to %g%s.", Self->PageWidth, Self->RelPageWidth ? "%%" : "");
            break;

         case HASH_colour: // Background fill
            if (Self->Background) FreeResource(Self->Background);
            Self->Background = StrClone(Tag.Attribs[i].Value.c_str());
            break;

         case HASH_face:
         case HASH_fontface:
            Self->FontFace = Tag.Attribs[i].Value;
            break;

         case HASH_fontsize: // Default font point size
            Self->FontSize = StrToFloat(Tag.Attribs[i].Value);
            break;

         case HASH_fontcolour: // Default font colour
            Self->FontFill = Tag.Attribs[i].Value;
            break;

         default:
            log.warning("Style attribute %s=%s not supported.", Tag.Attribs[i].Name.c_str(), Tag.Attribs[i].Value.c_str());
            break;
      }
   }

   Self->Style.font_style.font_index = create_font(Self->FontFace, "Regular", Self->FontSize);
   Self->Style.font_style.options   = FSO::NIL;
   Self->Style.font_style.fill      = Self->FontFill;

   Self->Style.face        = Self->FontFace;
   Self->Style.point       = Self->FontSize;
   Self->Style.face_change  = true;
   Self->Style.style_change = true;

   if (!Children.empty()) Self->BodyTag = &Children;
}

//********************************************************************************************************************
// In background mode, all objects are targeted to the View viewport rather than the Page viewport.

static void tag_background(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   Self->BkgdGfx++;
   parse_tags(Self, XML, Children, Index);
   Self->BkgdGfx--;
}

//********************************************************************************************************************

static void tag_bold(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   if ((Self->Style.font_style.options & FSO::BOLD) IS FSO::NIL) {
      auto savestatus = Self->Style; // Save the current style
      Self->Style.face_change = true; // Bold fonts are typically a different typeset
      Self->Style.font_style.options |= FSO::BOLD;
      parse_tags(Self, XML, Children, Index);
      saved_style_check(Self, savestatus);
   }
   else parse_tags(Self, XML, Children, Index, Flags & ~IPF::FILTER_ALL);
}

//********************************************************************************************************************

static void tag_br(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
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

static void tag_cache(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
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

static void tag_call(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
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

static void tag_debug(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   pf::Log log("DocMsg");
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("msg", Tag.Attribs[i].Name)) log.warning("%s", Tag.Attribs[i].Value.c_str());
   }
}

//********************************************************************************************************************
// Declaring <svg> anywhere can execute an SVG statement of any kind, with the caveat that it will be applied to the
// Page viewport.  This feature should only be used for the creation of resources that can then be referred to in the
// document as named patterns.

static void tag_svg(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   STRING def_statement;
   auto err = xmlGetString(XML, Tag.ID, XMF::NIL, &def_statement);
   if (!err) {
      objSVG::create svg = { fl::Statement(def_statement), fl::Target(Self->Page) };
   }
}

//********************************************************************************************************************
// Use div to structure the document in a similar way to paragraphs.  The main difference is that it avoids the
// declaration of paragraph start and end points and won't cause line breaks.

static void tag_div(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   auto savestatus = Self->Style;
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("align", Tag.Attribs[i].Name)) {
         if ((!StrMatch(Tag.Attribs[i].Value, "center")) or (!StrMatch(Tag.Attribs[i].Value, "horizontal"))) {
            Self->Style.style_change = true;
            Self->Style.font_style.options |= FSO::ALIGN_CENTER;
         }
         else if (!StrMatch(Tag.Attribs[i].Value, "right")) {
            Self->Style.style_change = true;
            Self->Style.font_style.options |= FSO::ALIGN_RIGHT;
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

static void tag_editdef(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   doc_edit edit;
   std::string name;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      switch (StrHash(Tag.Attribs[i].Name)) {
         case HASH_maxchars:
            edit.max_chars = StrToInt(Tag.Attribs[i].Value);
            if (edit.max_chars < 0) edit.max_chars = -1;
            break;

         case HASH_name: name = Tag.Attribs[i].Value; break;

         case HASH_selectcolour: break;

         case HASH_linebreaks: edit.line_breaks = StrToInt(Tag.Attribs[i].Value); break;

         case HASH_editfonts:
         case HASH_editimages:
         case HASH_edittables:
         case HASH_editall:
            break;

         case HASH_onchange:
            if (!Tag.Attribs[i].Value.empty()) edit.on_change = Tag.Attribs[i].Value;
            break;

         case HASH_onexit:
            if (!Tag.Attribs[i].Value.empty()) edit.on_exit = Tag.Attribs[i].Value;
            break;

         case HASH_onenter:
            if (!Tag.Attribs[i].Value.empty()) edit.on_enter = Tag.Attribs[i].Value;
            break;

         default:
            if (Tag.Attribs[i].Name[0] IS '@') {
               edit.args.emplace_back(make_pair(Tag.Attribs[i].Name, Tag.Attribs[i].Value));
            }
            else if (Tag.Attribs[i].Name[0] IS '_') {
               edit.args.emplace_back(make_pair(Tag.Attribs[i].Name, Tag.Attribs[i].Value));
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

static void tag_focus(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   Self->FocusIndex = Self->Tabs.size();
}

//********************************************************************************************************************

static void tag_footer(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   Self->FooterTag = &Children;
}

//********************************************************************************************************************

static void tag_header(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   Self->HeaderTag = &Children;
}

//********************************************************************************************************************
// Use of <meta> for custom information is allowed and is ignored by the parser.

static void tag_head(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
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
// Include XML from another RIPL file.

static void tag_include(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
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
// Parse a string value as XML

static void tag_parse(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
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
// Bitmap and vector images are supported as vector rectangles that reference a pattern name.  Images need to be
// loaded as resources in an <svg> tag and can then be referenced by name.  Technically any pattern type can be
// referenced as an image - so if the client wants to refer to a gradient for example, that is perfectly legal.
//
// Images are inline by default.  Whitespace on either side is never blocked, whether inline or floating.
// Blocking whitespace can be achieved by embedding the image within <p> tags.
//
// A benefit to rendering SVG images in the <defs> area is that they are converted to cached bitmap textures ahead of
// time.  This provides a considerable speed boost when drawing them, at a potential cost to image quality.

static void tag_image(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   bc_image img;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      auto hash = StrHash(Tag.Attribs[i].Name);
      auto &value = Tag.Attribs[i].Value;
      if (hash IS HASH_src) {
         img.src = value;
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

   if (!img.src.empty()) {
      if (img.width < 0) img.width = 0; // Zero is equivalent to 'auto', meaning on-the-fly computation
      if (img.height < 0) img.height = 0;

      if (!img.floating()) Self->NoWhitespace = false; // Images count as characters when inline.
      Self->insert_code(Index, img);
   }
   else {
      log.warning("No src defined for <image> tag.");
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

static void tag_index(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
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
      bc_index esc(name, Self->UniqueID++, 0, visible, Self->Invisible ? false : true);

      Self->insert_code(Index, esc);

      if (!Children.empty()) {
         if (!visible) Self->Invisible++;
         parse_tags(Self, XML, Children, Index);
         if (!visible) Self->Invisible--;
      }

      bc_index_end end(esc.id);
      Self->insert_code(Index, end);
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

static void tag_link(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   bc_link link;
   bool select = false;
   std::string colour, hint, pointermotion;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      switch (StrHash(Tag.Attribs[i].Name)) {
         case HASH_href:
            if (link.type IS LINK::NIL) {
               link.ref = Tag.Attribs[i].Value;
               link.type = LINK::HREF;
            }
            break;

         case HASH_onclick:
            if (link.type IS LINK::NIL) { // Function to execute on click
               link.ref = Tag.Attribs[i].Value;
               link.type = LINK::FUNCTION;
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
            if (Tag.Attribs[i].Name.starts_with('@')) link.args.push_back(make_pair(Tag.Attribs[i].Name, Tag.Attribs[i].Value));
            else if (Tag.Attribs[i].Name.starts_with('_')) link.args.push_back(make_pair(Tag.Attribs[i].Name, Tag.Attribs[i].Value));
            else log.warning("<a|link> unsupported attribute '%s'", Tag.Attribs[i].Name.c_str());
      }
   }

   std::ostringstream buffer;

   if ((link.type != LINK::NIL) or (!Tag.Children.empty())) {
      link.id    = ++Self->LinkID;
      link.align = Self->Style.font_style.options;

      auto pos = sizeof(link);
      if (link.type IS LINK::FUNCTION) buffer << link.ref << '\0';
      else buffer << link.ref << '\0';

      if (!pointermotion.empty()) {
         link.pointer_motion = pos;
         buffer << pointermotion << '\0';
      }

      Self->insert_code(Index, link);

      auto savestatus = Self->Style;

      Self->Style.style_change        = true;
      Self->Style.font_style.options |= FSO::UNDERLINE;

      if (!colour.empty()) Self->Style.font_style.fill = colour;
      else Self->Style.font_style.fill = Self->LinkFill;

      parse_tags(Self, XML, Tag.Children, Index);

      saved_style_check(Self, savestatus);

      Self->reserve_code<bc_link_end>(Index);

      // This style check will forcibly revert the font back to whatever it was rather than waiting for new content
      // to result in a change.  The reason why we want to do this is to make it easier to manage run-time insertion
      // of new content.  For instance if the user enters text on a new line following an <h1> heading, the user's
      // expectation would be for the new text to be in the format of the body's font and not the <h1> font.

      style_check(Self, Index);

      // Links are added to the list of tab-able points

      auto i = add_tabfocus(Self, TT_LINK, link.id);
      if (select) Self->FocusIndex = i;
   }
   else parse_tags(Self, XML, Tag.Children, Index, Flags & (~IPF::FILTER_ALL));
}

//********************************************************************************************************************

static void tag_list(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);
   bc_list esc, *savelist;

   esc.fill    = Self->Style.font_style.fill; // Default fill matches the current font colour
   esc.item_num = esc.start;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("fill", Tag.Attribs[i].Name)) {
         esc.fill = Tag.Attribs[i].Value;
      }
      else if (!StrMatch("indent", Tag.Attribs[i].Name)) {
         // Affects the indenting to apply to child items.
         esc.block_indent = StrToInt(Tag.Attribs[i].Value);
      }
      else if (!StrMatch("vspacing", Tag.Attribs[i].Name)) {
         esc.vspacing = StrToFloat(Tag.Attribs[i].Value);
         if (esc.vspacing < 0) esc.vspacing = 0;
      }
      else if (!StrMatch("type", Tag.Attribs[i].Name)) {
         if (!StrMatch("bullet", Tag.Attribs[i].Value)) {
            esc.type = bc_list::BULLET;
         }
         else if (!StrMatch("ordered", Tag.Attribs[i].Value)) {
            esc.type = bc_list::ORDERED;
            esc.item_indent = 0;
         }
         else if (!StrMatch("custom", Tag.Attribs[i].Value)) {
            esc.type = bc_list::CUSTOM;
            esc.item_indent = 0;
         }
      }
      else log.msg("Unknown list attribute '%s'", Tag.Attribs[i].Name.c_str());
   }

   style_check(Self, Index); // Font changes must take place prior to the list for correct bullet point alignment

   // Note: Paragraphs are not inserted because <li> does this

   Self->insert_code(Index, esc);

   savelist = Self->Style.list;
   Self->Style.list = &esc;

      if (!Children.empty()) parse_tags(Self, XML, Children, Index);

   Self->Style.list = savelist;

   Self->reserve_code<bc_list_end>(Index);

   Self->NoWhitespace = true;
}

//********************************************************************************************************************
// Also see check_para_attrib() for paragraph attributes.

static void tag_paragraph(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   Self->ParagraphDepth++;

   bc_paragraph esc;
   esc.leading_ratio = 0;

   auto savestatus = Self->Style;
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("align", Tag.Attribs[i].Name)) {
         if ((!StrMatch(Tag.Attribs[i].Value, "center")) or (!StrMatch(Tag.Attribs[i].Value, "horizontal"))) {
            Self->Style.style_change = true;
            Self->Style.font_style.options |= FSO::ALIGN_CENTER;
         }
         else if (!StrMatch(Tag.Attribs[i].Value, "right")) {
            Self->Style.style_change = true;
            Self->Style.font_style.options |= FSO::ALIGN_RIGHT;
         }
         else log.warning("Alignment type '%s' not supported.", Tag.Attribs[i].Value.c_str());
      }
      else check_para_attrib(Self, Tag.Attribs[i].Name, Tag.Attribs[i].Value, &esc);
   }

   Self->insert_code(Index, esc);

   Self->NoWhitespace = esc.trim;

   parse_tags(Self, XML, Children, Index);
   saved_style_check(Self, savestatus);

   bc_paragraph_end end;
   Self->insert_code(Index, end);
   Self->NoWhitespace = true;

   // This style check will forcibly revert the font back to whatever it was rather than waiting for new content to
   // result in a change.  The reason why we want to do this is to make it easier to manage run-time insertion of new
   // content.  For instance if the user enters text on a new line following an <h1> heading, the user's
   // expectation would be for the new text to be in the format of the body's font and not the <h1> font.

   style_check(Self, Index);

   Self->ParagraphDepth--;
}

//********************************************************************************************************************

static void tag_print(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
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
            insert_text(Self, Index, Tag.Attribs[1].Value, (Self->Style.font_style.options & FSO::PREFORMAT) != FSO::NIL);
         }
      }
      else if (!StrMatch("src", Tag.Attribs[1].Name)) {
         // This option is only supported in unrestricted mode
         if ((Self->Flags & DCF::UNRESTRICTED) != DCF::NIL) {
            CacheFile *cache;
            if (!LoadFile(Tag.Attribs[1].Value.c_str(), LDF::NIL, &cache)) {
               insert_text(Self, Index, std::string((CSTRING)cache->Data), (Self->Style.font_style.options & FSO::PREFORMAT) != FSO::NIL);
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

static void tag_set(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
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

static void tag_template(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   // Templates can be used to create custom tags.
   //
   // <template name="customimage">
   //   <image src="" background="#f0f0f0"/>
   // </template>

   if (Self->InTemplate) return;

   // Validate the template (must have a name)

   unsigned i;
   for (i=1; i < Tag.Attribs.size(); i++) {
      if ((!StrMatch("name", Tag.Attribs[i].Name)) and (!Tag.Attribs[i].Value.empty())) break;
      if ((!StrMatch("class", Tag.Attribs[i].Name)) and (!Tag.Attribs[i].Value.empty())) break;
   }

   if (i >= Tag.Attribs.size()) {
      log.warning("A <template> is missing a name or class attribute.");
      return;
   }

   Self->RefreshTemplates = true;

   // TODO: It would be nice if we scanned the existing templates and
   // replaced them correctly, however we're going to be lazy and override
   // styles by placing updated definitions at the end of the style list.

   STRING strxml;
   if (!xmlGetString(XML, Tag.ID, XMF::NIL, &strxml)) {
      xmlInsertXML(Self->Templates, 0, XMI::PREV, strxml, 0);
      FreeResource(strxml);
   }
   else log.warning("Failed to convert template %d to an XML string.", Tag.ID);
}

//********************************************************************************************************************
// Used to send XML data to an embedded object.
//
// NOTE: If no child tags or content is inside the XML string, or if attributes are attached to the XML tag, then the
// user is trying to create a new XML object (under the Data category), not the XML reserved word.

static void tag_xml(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   tag_xml_content(Self, XML, Tag, PXF::ARGS);
}

static void tag_xmlraw(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   tag_xml_content(Self, XML, Tag, PXF::NIL);
}

static void tag_xmltranslate(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
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

static void tag_font(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   auto savestatus = Self->Style;
   bool preformat = false;
   auto flags = IPF::NIL;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("colour", Tag.Attribs[i].Name)) {
         Self->Style.style_change = true;
         Self->Style.font_style.fill = Tag.Attribs[i].Value;
      }
      else if (!StrMatch("face", Tag.Attribs[i].Name)) {
         Self->Style.face_change = true;

         auto j = Tag.Attribs[i].Value.find(':');
         if (j != std::string::npos) { // Point size follows
            auto str = Tag.Attribs[i].Value.c_str();
            j++;
            Self->Style.point = StrToInt(str+j);
            j = Tag.Attribs[i].Value.find(':', j);
            if (j != std::string::npos) { // Style follows
               j++;
               if (!StrMatch("bold", str+j)) {
                  Self->Style.face_change = true;
                  Self->Style.font_style.options |= FSO::BOLD;
               }
               else if (!StrMatch("italic", str+j)) {
                  Self->Style.face_change = true;
                  Self->Style.font_style.options |= FSO::ITALIC;
               }
               else if (!StrMatch("bold italic", str+j)) {
                  Self->Style.face_change = true;
                  Self->Style.font_style.options |= FSO::BOLD|FSO::ITALIC;
               }
            }
         }

         Self->Style.face = Tag.Attribs[i].Value.substr(0, j);
      }
      else if (!StrMatch("size", Tag.Attribs[i].Name)) {
         Self->Style.face_change = true;
         Self->Style.point = StrToFloat(Tag.Attribs[i].Value);
      }
      else if (!StrMatch("style", Tag.Attribs[i].Name)) {
         if (!StrMatch("bold", Tag.Attribs[i].Value)) {
            Self->Style.face_change = true;
            Self->Style.font_style.options |= FSO::BOLD;
         }
         else if (!StrMatch("italic", Tag.Attribs[i].Value)) {
            Self->Style.face_change = true;
            Self->Style.font_style.options |= FSO::ITALIC;
         }
         else if (!StrMatch("bold italic", Tag.Attribs[i].Value)) {
            Self->Style.face_change = true;
            Self->Style.font_style.options |= FSO::BOLD|FSO::ITALIC;
         }
      }
      else if (!StrMatch("preformat", Tag.Attribs[i].Name)) {
         Self->Style.style_change = true;
         Self->Style.font_style.options |= FSO::PREFORMAT;
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
   objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
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
         if (StrMatch("data", scan.Attribs[0].name)) continue;

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
                     if ((!StrMatch("name", tmp.Attribs[i].name)) and (!StrMatch(t, tmp.Attribs[i].Value))) {
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
      bc_vector escobj;

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
            resource.class_id = class_id;
         }
         else {
            escobj.object_id = object->UID;
            escobj.class_id  = object->Class->ClassID;
            escobj.in_line = false;
            if (Self->CurrentObject) escobj.owned = true;

            // By default objects are assumed to be in the background (thus not embedded as part of the text stream).
            // This section is intended to confirm the graphical state of the object.

            if (object->Class->ClassID IS ID_VECTOR) {
               //if (layout->Layout & (LAYOUT_BACKGROUND|LAYOUT_FOREGROUND));
               //else if (layout->Layout & LAYOUT_EMBEDDED) escobj.Inline = true;
            }
            else escobj.in_line = true; // If the layout object is not present, the object is managing its own graphics and likely is embedded (button, combobox, checkbox etc are like this)

            style_check(Self, Index);
            Self->insert_code(Index, escobj);

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
               res.class_id = class_id;
            }

            // If the object is inline, we will allow whitespace to immediately follow the object.

            if (escobj.in_line) Self->NoWhitespace = false;

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
// The use of pre will turn off the automated whitespace management so that all whitespace is parsed as-is.  It does
// not switch to a monospaced font.

static void tag_pre(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   if ((Self->Style.font_style.options & FSO::PREFORMAT) IS FSO::NIL) {
      auto savestatus = Self->Style;
      Self->Style.style_change = true;
      Self->Style.font_style.options |= FSO::PREFORMAT;
      parse_tags(Self, XML, Children, Index, IPF::STRIP_FEEDS);
      saved_style_check(Self, savestatus);
   }
   else parse_tags(Self, XML, Children, Index, IPF::STRIP_FEEDS);

   trim_preformat(Self, Index);
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

static void tag_script(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
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
         if (resource.type IS RTD::PERSISTENT_SCRIPT) {
            script = (objScript *)GetObjectPtr(resource.object_id);
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

static void tag_setfont(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      switch (StrHash(Tag.Attribs[i].Name)) {
         case HASH_colour:
            Self->Style.style_change = true;
            Self->Style.font_style.fill = Tag.Attribs[i].Value;
            break;

         case HASH_face:
            Self->Style.face_change = true;
            Self->Style.face = Tag.Attribs[i].Value;
            break;

         case HASH_size:
            Self->Style.face_change = true;
            Self->Style.point = StrToFloat(Tag.Attribs[i].Value);
            break;

         case HASH_style:
            if (!StrMatch("bold", Tag.Attribs[i].Value)) {
               Self->Style.face_change = true;
               Self->Style.font_style.options |= FSO::BOLD;
            }
            else if (!StrMatch("italic", Tag.Attribs[i].Value)) {
               Self->Style.face_change = true;
               Self->Style.font_style.options |= FSO::ITALIC;
            }
            else if (!StrMatch("bold italic", Tag.Attribs[i].Value)) {
               Self->Style.face_change = true;
               Self->Style.font_style.options |= FSO::BOLD|FSO::ITALIC;
            }
            break;

         case HASH_preformat:
            Self->Style.style_change = true;
            Self->Style.font_style.options |= FSO::PREFORMAT;
            break;
      }
   }

   //style_check(Self, index);
}

//********************************************************************************************************************

static void tag_setmargins(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   bc_set_margins margins;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("top", Tag.Attribs[i].Name)) {
         margins.top = StrToInt(Tag.Attribs[i].Value);
         if (margins.top < -4000) margins.top = -4000;
         else if (margins.top > 4000) margins.top = 4000;
      }
      else if (!StrMatch("bottom", Tag.Attribs[i].Name)) {
         margins.bottom = StrToInt(Tag.Attribs[i].Value);
         if (margins.bottom < -4000) margins.bottom = -4000;
         else if (margins.bottom > 4000) margins.bottom = 4000;
      }
      else if (!StrMatch("right", Tag.Attribs[i].Name)) {
         margins.right = StrToInt(Tag.Attribs[i].Value);
         if (margins.right < -4000) margins.right = -4000;
         else if (margins.right > 4000) margins.right = 4000;
      }
      else if (!StrMatch("left", Tag.Attribs[i].Name)) {
         margins.left = StrToInt(Tag.Attribs[i].Value);
         if (margins.left < -4000) margins.left = -4000;
         else if (margins.left > 4000) margins.left = 4000;
      }
      else if (!StrMatch("all", Tag.Attribs[i].Name)) {
         LONG value;
         value = StrToInt(Tag.Attribs[i].Value);
         if (value < -4000) value = -4000;
         else if (value > 4000) value = 4000;
         margins.left = margins.top = margins.right = margins.bottom = value;
      }
   }

   Self->insert_code(Index, margins);
}

//********************************************************************************************************************

static void tag_savestyle(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   //style_check(Self, index);
   Self->RestoreStyle = Self->Style; // Save the current style
}

//********************************************************************************************************************

static void tag_restorestyle(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   Self->Style = Self->RestoreStyle; // Restore the saved style
   Self->Style.face_change = true;
   //style_check(Self, index);
}

//********************************************************************************************************************

static void tag_italic(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   if ((Self->Style.font_style.options & FSO::ITALIC) IS FSO::NIL) {
      auto savestatus = Self->Style;
      Self->Style.face_change = true; // Italic fonts are typically a different typeset
      Self->Style.font_style.options |= FSO::ITALIC;
      parse_tags(Self, XML, Children, Index);
      saved_style_check(Self, savestatus);
   }
   else parse_tags(Self, XML, Children, Index);
}

//********************************************************************************************************************
// List item parser

static void tag_li(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   if (!Self->Style.list) {
      log.warning("<li> not used inside a <list> tag.");
      return;
   }

   bc_paragraph para;

   para.list_item    = true;
   para.leading_ratio = 0;
   para.applyStyle(Self->Style);

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      auto tagname = Tag.Attribs[i].Name.c_str();
      if (*tagname IS '$') tagname++;

      if (!StrMatch("value", tagname)) {
         para.value = Tag.Attribs[i].Value;
      }
      else if (!StrMatch("leading", tagname)) {
         para.leading_ratio = StrToFloat(Tag.Attribs[i].Value);
         if (para.leading_ratio < MIN_LEADING) para.leading_ratio = MIN_LEADING;
         else if (para.leading_ratio > MAX_LEADING) para.leading_ratio = MAX_LEADING;
      }
      else if (!StrMatch("vspacing", tagname)) {
         para.vspacing = StrToFloat(Tag.Attribs[i].Value);
         if (para.vspacing < MIN_LEADING) para.vspacing = MIN_LEADING;
         else if (para.vspacing > MAX_VSPACING) para.vspacing = MAX_VSPACING;
      }
      else if (!StrMatch("aggregate", tagname)) {
         if (Tag.Attribs[i].Value == "true") para.aggregate = true;
         else if (Tag.Attribs[i].Value == "1") para.aggregate = true;
      }
   }

   Self->ParagraphDepth++;

   if ((Self->Style.list->type IS bc_list::CUSTOM) and (!para.value.empty())) {
      style_check(Self, Index); // font changes must take place prior to the printing of custom string items

      Self->insert_code(Index, para);

         parse_tags(Self, XML, Children, Index);

      Self->reserve_code<bc_paragraph_end>(Index);
   }
   else if (Self->Style.list->type IS bc_list::ORDERED) {
      style_check(Self, Index); // font changes must take place prior to the printing of custom string items

      auto list_size = Self->Style.list->buffer.size();
      Self->Style.list->buffer.push_back(std::to_string(Self->Style.list->item_num) + ".");

      // ItemNum is reset because a child list could be created

      auto save_item = Self->Style.list->item_num;
      Self->Style.list->item_num = 1;

      if (para.aggregate) for (auto &p : Self->Style.list->buffer) para.value += p;
      else para.value = Self->Style.list->buffer.back();

      Self->insert_code(Index, para);

         parse_tags(Self, XML, Children, Index);

      Self->reserve_code<bc_paragraph_end>(Index);

      Self->Style.list->item_num = save_item;
      Self->Style.list->buffer.resize(list_size);

      Self->Style.list->item_num++;
   }
   else { // BULLET
      Self->insert_code(Index, para);

         parse_tags(Self, XML, Children, Index);

      Self->reserve_code<bc_paragraph_end>(Index);
      Self->NoWhitespace = true;
   }

   Self->ParagraphDepth--;
}

//********************************************************************************************************************

static void tag_underline(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   if ((Self->Style.font_style.options & FSO::UNDERLINE) IS FSO::NIL) {
      auto savestatus = Self->Style;
      Self->Style.style_change = true;
      Self->Style.font_style.options |= FSO::UNDERLINE;
      parse_tags(Self, XML, Children, Index);
      saved_style_check(Self, savestatus);
   }
   else {
      auto parse_flags = Flags & (~IPF::FILTER_ALL);
      parse_tags(Self, XML, Children, Index, parse_flags);
   }
}

//********************************************************************************************************************

static void tag_repeat(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   std::string index_name;
   LONG loop_start = 0, loop_end = 0, count = 0, step  = 0;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("start", Tag.Attribs[i].Name)) {
         loop_start = StrToInt(Tag.Attribs[i].Value);
         if (loop_start < 0) loop_start = 0;
      }
      else if (!StrMatch("count", Tag.Attribs[i].Name)) {
         count = StrToInt(Tag.Attribs[i].Value);
         if (count < 0) {
            log.warning("Invalid count value of %d", count);
            return;
         }
      }
      else if (!StrMatch("end", Tag.Attribs[i].Name)) {
         loop_end = StrToInt(Tag.Attribs[i].Value) + 1;
      }
      else if (!StrMatch("step", Tag.Attribs[i].Name)) {
         step = StrToInt(Tag.Attribs[i].Value);
      }
      else if (!StrMatch("index", Tag.Attribs[i].Name)) {
         // If an index name is specified, the programmer will need to refer to it as [@indexname] and [%index] will
         // remain unchanged from any parent repeat loop.

         index_name = Tag.Attribs[i].Value;
      }
   }

   if (!step) {
      if (loop_end < loop_start) step = -1;
      else step = 1;
   }

   // Validation - ensure that it will be possible for the repeat loop to execute correctly without the chance of
   // infinite looping.
   //
   // If the user set both count and end attributes, the count attribute will be given the priority here.

   if (count > 0) loop_end = loop_start + (count * step);

   if (step > 0) {
      if (loop_end < loop_start) step = -step;
   }
   else if (loop_end > loop_start) step = -step;

   log.traceBranch("Performing a repeat loop (start: %d, end: %d, step: %d).", loop_start, loop_end, step);

   auto save_index = Self->LoopIndex;

   while (loop_start < loop_end) {
      if (index_name.empty()) Self->LoopIndex = loop_start;
      else SetVar(Self, index_name.c_str(), std::to_string(loop_start).c_str());

      parse_tags(Self, XML, Tag.Children, Index, Flags);
      loop_start += step;
   }

   if (index_name.empty()) Self->LoopIndex = save_index;

   log.trace("insert_child:","Repeat loop ends.");
}

//********************************************************************************************************************
// <table columns="10%,90%" width="100" height="100" fill="rgb(128,128,128)">
//  <row><cell>Activate<brk/>This activates the object.</cell></row>
//  <row><cell span="2">Reset</cell></row>
// </table>
//
// <table width="100" height="100" fill="rgb(128,128,128)">
//  <cell>Activate</cell><cell>This activates the object.</cell>
//  <cell colspan="2">Reset</cell>
// </table>
//
// The only acceptable child tags inside a <table> section are row, brk and cell tags.  Command tags are acceptable
// (repeat, if statements, etc).  The table byte code is typically generated as SCODE::TABLE_START, SCODE::ROW,
// SCODE::CELL..., SCODE::ROW_END, SCODE::TABLE_END.

static void tag_table(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   auto &start = Self->reserve_code<bc_table>(Index);
   start.min_width  = 1;
   start.min_height = 1;

   std::string columns;
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      switch (StrHash(Tag.Attribs[i].Name)) {
         case HASH_columns:
            // Column preferences are processed only when the end of the table marker has been reached.
            columns = Tag.Attribs[i].Value;
            break;

         case HASH_width:
            start.min_width = StrToInt(Tag.Attribs[i].Value);
            start.width_pct = false;
            if (Tag.Attribs[i].Value.find_first_of('%') != std::string::npos) {
               start.width_pct = true;
            }
            if (start.min_width < 1) start.min_width = 1;
            else if (start.min_width > 10000) start.min_width = 10000;
            break;

         case HASH_height:
            start.min_height = StrToInt(Tag.Attribs[i].Value);
            if (Tag.Attribs[i].Value.find_first_of('%') != std::string::npos) {
               start.height_pct = true;
            }
            if (start.min_height < 1) start.min_height = 1;
            else if (start.min_height > 10000) start.min_height = 10000;
            break;

         case HASH_fill:
            start.fill = Tag.Attribs[i].Value;
            break;

         case HASH_stroke:
            start.stroke = Tag.Attribs[i].Value;
            if (start.strokeWidth < 1) start.strokeWidth = 1;
            break;

         case HASH_spacing: // Spacing between the cells
            start.cell_vspacing = StrToInt(Tag.Attribs[i].Value);
            if (start.cell_vspacing < 0) start.cell_vspacing = 0;
            else if (start.cell_vspacing > 200) start.cell_vspacing = 200;
            start.cell_hspacing = start.cell_vspacing;
            break;

         case HASH_collapsed: // Collapsed tables do not have spacing (defined by 'spacing' or 'hspacing') on the sides
            start.collapsed = true;
            break;

         case HASH_vspacing: // Spacing between the cells
            start.cell_vspacing = StrToInt(Tag.Attribs[i].Value);
            if (start.cell_vspacing < 0) start.cell_vspacing = 0;
            else if (start.cell_vspacing > 200) start.cell_vspacing = 200;
            break;

         case HASH_hspacing: // Spacing between the cells
            start.cell_hspacing = StrToInt(Tag.Attribs[i].Value);
            if (start.cell_hspacing < 0) start.cell_hspacing = 0;
            else if (start.cell_hspacing > 200) start.cell_hspacing = 200;
            break;

         case HASH_margins:
         case HASH_padding: // Padding inside the cells
            start.cell_padding = StrToInt(Tag.Attribs[i].Value);
            if (start.cell_padding < 0) start.cell_padding = 0;
            else if (start.cell_padding > 200) start.cell_padding = 200;
            break;

         case HASH_strokeWidth: {
            auto j = StrToFloat(Tag.Attribs[i].Value);
            if (j < 0.0) j = 0.0;
            else if (j > 255.0) j = 255.0;
            start.strokeWidth = j;
            break;
         }
      }
   }

   auto savevar = Self->Style.table;
   process_table var;
   Self->Style.table = &var;
   Self->Style.table->table = &start;

      parse_tags(Self, XML, Tag.Children, Index, IPF::NO_CONTENT|IPF::FILTER_TABLE);

   Self->Style.table = savevar;

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
      for (i=0; (i < start.columns.size()) and (i < list.size()); i++) {
         start.columns[i].preset_width = StrToFloat(list[i]);
         if (list[i].find_first_of('%') != std::string::npos) start.columns[i].preset_width_rel = true;
      }

      if (i < start.columns.size()) log.warning("Warning - columns attribute '%s' did not define %d columns.", columns.c_str(), LONG(start.columns.size()));
   }

   bc_table_end end;
   Self->insert_code(Index, end);

   //style_check(Self, index);
   //Self->Style.style_change = false;

   Self->NoWhitespace = true; // Setting this to true will prevent the possibility of blank spaces immediately following the table.
}

//********************************************************************************************************************

static void tag_row(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);

   if (!Self->Style.table) {
      log.warning("<row> not defined inside <table> section.");
      Self->Error = ERR_InvalidData;
      return;
   }

   bc_row escrow;

   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      if (!StrMatch("height", Tag.Attribs[i].Name)) {
         escrow.min_height = StrToInt(Tag.Attribs[i].Value);
         if (escrow.min_height < 0) escrow.min_height = 0;
         else if (escrow.min_height > 4000) escrow.min_height = 4000;
      }
      else if (!StrMatch("fill", Tag.Attribs[i].Name))   escrow.fill   = Tag.Attribs[i].Value;
      else if (!StrMatch("stroke", Tag.Attribs[i].Name)) escrow.stroke = Tag.Attribs[i].Value;
   }

   Self->insert_code(Index, escrow);
   Self->Style.table->table->rows++;
   Self->Style.table->row_col = 0;

   if (!Children.empty()) {
      parse_tags(Self, XML, Children, Index, IPF::NO_CONTENT|IPF::FILTER_ROW);
   }

   bc_row_end end;
   Self->insert_code(Index, end);

   if (Self->Style.table->row_col > LONG(Self->Style.table->table->columns.size())) {
      Self->Style.table->table->columns.resize(Self->Style.table->row_col);
   }
}

//********************************************************************************************************************

static void tag_cell(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
{
   pf::Log log(__FUNCTION__);
   style_status savestatus;
   static UBYTE edit_recurse = 0;

   if (!Self->Style.table) {
      log.warning("<cell> not defined inside <table> section.");
      Self->Error = ERR_InvalidData;
      return;
   }

   bc_cell cell(Self->UniqueID++, Self->Style.table->row_col);
   bool select = false;
   for (unsigned i=1; i < Tag.Attribs.size(); i++) {
      switch (StrHash(Tag.Attribs[i].Name)) {
         case HASH_border: {
            std::vector<std::string> list;
            pf::split(Tag.Attribs[i].Value, std::back_inserter(list));

            for (auto &v : list) {
               if (!StrMatch("all", v)) cell.border = CB::ALL;
               else if (!StrMatch("top", v)) cell.border |= CB::TOP;
               else if (!StrMatch("left", v)) cell.border |= CB::LEFT;
               else if (!StrMatch("bottom", v)) cell.border |= CB::BOTTOM;
               else if (!StrMatch("right", v)) cell.border |= CB::RIGHT;
            }

            break;
         }

         case HASH_colspan:
            cell.col_span = StrToInt(Tag.Attribs[i].Value);
            if (cell.col_span < 1) cell.col_span = 1;
            else if (cell.col_span > 1000) cell.col_span = 1000;
            break;

         case HASH_rowspan:
            cell.row_span = StrToInt(Tag.Attribs[i].Value);
            if (cell.row_span < 1) cell.row_span = 1;
            else if (cell.row_span > 1000) cell.row_span = 1000;
            break;

         case HASH_edit: {
            if (edit_recurse) {
               log.warning("Edit cells cannot be embedded recursively.");
               Self->Error = ERR_Recursion;
               return;
            }
            cell.edit_def = Tag.Attribs[i].Value;

            if (!Self->EditDefs.contains(Tag.Attribs[i].Value)) {
               log.warning("Edit definition '%s' does not exist.", Tag.Attribs[i].Value.c_str());
               cell.edit_def.clear();
            }

            break;
         }

         case HASH_select: select = true; break;

         case HASH_fill: cell.fill = Tag.Attribs[i].Value; break;

         case HASH_stroke: 
            cell.stroke = Tag.Attribs[i].Value; 
            if (!cell.strokeWidth) {
               cell.strokeWidth = Self->Style.table->table->strokeWidth;
               if (!cell.strokeWidth) cell.strokeWidth = 1;
            }
            break;

         case HASH_strokeWidth: 
            cell.strokeWidth = StrToFloat(Tag.Attribs[i].Value); 
            break;

         case HASH_nowrap:
            Self->Style.style_change = true;
            Self->Style.font_style.options |= FSO::NO_WRAP;
            break;

         case HASH_onclick:
            cell.onclick = Tag.Attribs[i].Value;
            break;

         default:
            if (Tag.Attribs[i].Name.starts_with('@')) {
               cell.args.emplace_back(std::make_pair(Tag.Attribs[i].Name.substr(1), Tag.Attribs[i].Value));
            }
            else if (Tag.Attribs[i].Name.starts_with('_')) {
               cell.args.emplace_back(std::make_pair(Tag.Attribs[i].Name, Tag.Attribs[i].Value));
            }
      }
   }

   Self->ParagraphDepth++;

   if (!cell.edit_def.empty()) edit_recurse++;

   // Edit sections enforce preformatting, which means that all whitespace entered by the user will be taken
   // into account.  The following check sets FSO::PREFORMAT if it hasn't been set already.

   auto cell_index = Index;

   Self->insert_code(Index, cell);

   auto parse_flags = Flags & (~(IPF::NO_CONTENT|IPF::FILTER_ALL));
   if (!Children.empty()) {
      Self->NoWhitespace = true; // Reset whitespace flag: false allows whitespace at the start of the cell, true prevents whitespace

      if ((!cell.edit_def.empty()) and ((Self->Style.font_style.options & FSO::PREFORMAT) IS FSO::NIL)) {
         savestatus = Self->Style;
         Self->Style.style_change = true;
         Self->Style.font_style.options |= FSO::PREFORMAT;
         parse_tags(Self, XML, Children, Index, parse_flags);
         saved_style_check(Self, savestatus);
      }
      else parse_tags(Self, XML, Children, Index, parse_flags);
   }

   Self->Style.table->row_col += cell.col_span;

   bc_cell_end esccell_end;
   esccell_end.cell_id = cell.cell_id;
   Self->insert_code(Index, esccell_end);

   if (!cell.edit_def.empty()) {
      // Links are added to the list of tabbable points

      LONG tab = add_tabfocus(Self, TT_EDIT, cell.cell_id);
      if (select) Self->FocusIndex = tab;
   }

   if (!cell.edit_def.empty()) edit_recurse--;

   Self->ParagraphDepth--;
}

//********************************************************************************************************************
// This instruction can only be used from within a template.

static void tag_inject(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
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

static void tag_page(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
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

static void tag_trigger(extDocument *Self, objXML *XML, XMLTag &Tag, objXML::TAGS &Children, stream_char &Index, IPF Flags)
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
