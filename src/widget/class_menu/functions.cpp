
static ERROR set_translation(objMenu *, OBJECTPTR Target, FIELD Field, CSTRING Text);

static THREADVAR BYTE tlSatisfied = FALSE;

/*****************************************************************************
** Translates arguments within the available buffer space of the string.
*/

static void translate_value(objMenu *Self, STRING Source, STRING Buffer, LONG BufferSize)
{
   parasol::Log log(__FUNCTION__);

   if ((Source) and (Source != Buffer)) StrCopy(Source, Buffer, BufferSize);

   // Search for an argument.  If there are no arguments in the string, return immediately.

   LONG i, j;
   if ((i = StrSearch("[@", Buffer, 0)) >= 0) {
      LONG count = 0;
      for (; (Buffer[i]); i++) {
         if ((Buffer[i] IS '[') and (Buffer[i+1] IS '@')) {
            char compare[60];
            STRING str = Buffer + i + 2;
            for (j=0; ((size_t)j < sizeof(compare)) and (*str) and (*str != ']'); j++) compare[j] = *str++;
            compare[j] = 0;

            CSTRING val;
            if ((val = VarGetString(Self->LocalArgs, compare))) {
               StrInsert(val, Buffer, BufferSize, i, j+3);
               count = 0;
               break;
            }

            if (++count > 30) {
               log.warning("Recursion in line %.20s", Buffer);
               break;
            }
         }
      }
   }

   StrEvaluate(Buffer, BufferSize, 0, 0);
}

//****************************************************************************

static LONG if_satisfied(objMenu *Self, XMLTag *Tag)
{
   bool reverse = FALSE;
   tlSatisfied = FALSE; // Reset the satisfied variable
   LONG index = 1;

   if (!StrMatch("not", Tag->Attrib[index].Name)) {
      reverse = TRUE;
      index++;
   }

   char buffer[600];
   if (!StrMatch("exists", Tag->Attrib[index].Name)) {
      translate_value(Self, Tag->Attrib[index].Value, buffer, sizeof(buffer));
      if (*buffer) {
         LONG count = 1;
         OBJECTID object_id;
         if (!FindObject(buffer, 0, FOF_INCLUDE_SHARED|FOF_SMART_NAMES, &object_id, &count)) {
            tlSatisfied = TRUE;
         }
      }
   }
   else if (!StrMatch("fileexists", Tag->Attrib[index].Name)) {
      translate_value(Self, Tag->Attrib[index].Value, buffer, sizeof(buffer));

      LONG flags, i;
      if (buffer[0] IS '~') {
         flags = FL_APPROXIMATE;
         i = 1;
      }
      else {
         flags = 0;
         i = 0;
      }

      objFile *file;
      if (!CreateObject(ID_FILE, 0, &file,
            FID_Path|TSTR,   buffer + i,
            FID_Flags|TLONG, flags,
            TAGEND)) {
         tlSatisfied = TRUE;
         acFree(file);
      }
   }
   else if ((!StrMatch("directory", Tag->Attrib[index].Name)) or
            (!StrMatch("isdirectory", Tag->Attrib[index].Name))) {
      // This option checks if a path explicitly refers to a directory
      translate_value(Self, Tag->Attrib[index].Value, buffer, sizeof(buffer));

      objFile *file;
      if (!CreateObject(ID_FILE, 0, &file, FID_Path|TSTR, buffer, TAGEND)) {
         if (file->Flags & FL_FOLDER) tlSatisfied = TRUE;
         acFree(file);
      }
   }
   else if (!StrMatch("isnull", Tag->Attrib[index].Name)) {
      translate_value(Self, Tag->Attrib[index].Value, buffer, sizeof(buffer));

      if ((!buffer[0]) or (buffer[0] IS '0')) tlSatisfied = TRUE;
      else tlSatisfied = FALSE;
   }
   else if (!StrMatch("notnull", Tag->Attrib[index].Name)) {
      translate_value(Self, Tag->Attrib[index].Value, buffer, sizeof(buffer));

      if ((!buffer[0]) or (buffer[0] IS '0')) tlSatisfied = FALSE;
      else tlSatisfied = TRUE;
   }
   else if (!StrMatch("statement", Tag->Attrib[index].Name)) {
      translate_value(Self, Tag->Attrib[index].Value, buffer, sizeof(buffer));
      tlSatisfied = StrEvalConditional(buffer);
   }

   if (reverse) tlSatisfied ^= 1;

   return tlSatisfied;
}

//****************************************************************************
// MenuBase, MenuItem and MenuBreak XML tags can be used to specify the look and feel of the menu.
// Menu, Item and Break XML tags can be used to specify elements to be placed within the menu.

static void parse_xmltag(objMenu *Self, objXML *XML, XMLTag *Tag)
{
   parasol::Log log(__FUNCTION__);

   if (!StrMatch("if", Tag->Attrib->Name)) {
      if (if_satisfied(Self, Tag)) {
         for (Tag=Tag->Child; Tag; Tag=Tag->Next) parse_xmltag(Self, XML, Tag);
      }
   }
   else if (!StrMatch("else", Tag->Attrib->Name)) {
      // Execute the contents of the <else> tag if the last <if> statement was not satisfied
      if (!tlSatisfied) {
         for (Tag=Tag->Child; Tag; Tag=Tag->Next) parse_xmltag(Self, XML, Tag);
      }
   }
   else if (!StrMatch("style", Tag->Attrib->Name)) {
      if (Tag->Child) {
         if (Self->Style) { FreeResource(Self->Style); Self->Style = NULL; }
         xmlGetString(XML, Tag->Child->Index, XMF_INCLUDE_SIBLINGS, &Self->Style);
      }
   }
   else if (!StrMatch("values", Tag->Attrib->Name)) {
      for (LONG a=1; a < Tag->TotalAttrib; a++) {
         char value[500];
         translate_value(Self, Tag->Attrib[a].Value, value, sizeof(value));
         SetFieldEval(Self, Tag->Attrib[a].Name, value);
      }
   }
   else if (!StrMatch("graphics", Tag->Attrib->Name)) {
      add_xml_item(Self, XML, Tag);
   }
   else if (!StrMatch("menu", Tag->Attrib->Name)) {
      add_xml_item(Self, XML, Tag);
   }
   else if (!StrMatch("item", Tag->Attrib->Name)) {
      add_xml_item(Self, XML, Tag);
   }
   else if (!StrMatch("break", Tag->Attrib->Name)) {
      add_xml_item(Self, XML, Tag);
   }
   else if (!StrMatch("cache", Tag->Attrib->Name)) { // Force caching of the menu
      Self->Flags |= MNF_CACHE;
   }
   else if (!StrMatch("translation", Tag->Attrib->Name)) {
      for (LONG j=1; j < Tag->TotalAttrib; j++) {
      	if (!StrMatch("language", Tag->Attrib[j].Name)) {
            StrCopy(Tag->Attrib[j].Value, Self->Language, sizeof(Self->Language));
         }
         else if (!StrMatch("dir", Tag->Attrib[j].Name)) {
            StrCopy(Tag->Attrib[j].Value, Self->LanguageDir, sizeof(Self->LanguageDir));
         }
      }

      // Load a translation file if the menu language does not match the user's language.
      //
      // The format is: [@scriptdir]/lang/[@filename].languagecode
      //
      // The translation arguments will be expected to be in the MENU group of the config file.

      CSTRING locale = "eng";
      StrReadLocale("Language", &locale);

      if (StrCompare(locale, Self->Language, 0, 0) != ERR_Okay) {
         LONG i;
         for (i=0; Self->Path[i]; i++);
         while ((i > 0) and (Self->Path[i-1] != ':') and (Self->Path[i-1] != '/') and (Self->Path[i-1] != '\\')) i--;

         char filename[300];
         LONG j = StrCopy(Self->Path, filename, i); // script dir
         StrFormat(filename+j, sizeof(filename)-j, "%s/%s.%s", Self->LanguageDir, Self->Path + i, locale);

         CreateObject(ID_CONFIG, NF_INTEGRAL, &Self->Translation, FID_Path|TSTR, filename, TAGEND);
      }
   }
   else log.warning("Unsupported menu element '%s'", Tag->Attrib->Name);
}

//****************************************************************************
// Translate strings to other languages.

static ERROR set_translation(objMenu *Self, OBJECTPTR Target, FIELD Field, CSTRING Text)
{
   if (Self->Flags & MNF_NO_TRANSLATION) return SetString(Target, Field, Text);

   // System translation
   CSTRING translation;
   if ((translation = StrTranslateText(Text))) {
      if (translation != Text) return SetString(Target, Field, translation);
   }

   // Custom translation
   if (Self->Translation) {
      ConfigGroups *groups;
      if (!GetPointer(Self->Translation, FID_Data, &groups)) {
         for (auto& [group, keys] : groups[0]) {
            if ((!group.compare("MENU")) and (keys.contains(Text))) {
               return SetString(Target, Field, keys[Text].c_str());
            }
         }
      }
   }

   return SetString(Target, Field, Text);
}

//****************************************************************************

static void draw_menu(objMenu *Self, objSurface *Surface, objBitmap *Bitmap)
{
   auto font = Self->Font;
   font->Bitmap = Bitmap;

   LONG x = Self->LeftMargin;
   LONG y = Self->TopMargin + Self->YPosition;

   ClipRectangle clip = Bitmap->Clip;
   if (Self->BorderSize > Bitmap->Clip.Left) Bitmap->Clip.Left = Self->BorderSize;
   if (Self->BorderSize > Bitmap->Clip.Top)  Bitmap->Clip.Top  = Self->BorderSize;
   if (Bitmap->Width - (Self->BorderSize) < Bitmap->Clip.Right)   Bitmap->Clip.Right  = Bitmap->Width - (Self->BorderSize);
   if (Bitmap->Height - (Self->BorderSize) < Bitmap->Clip.Bottom) Bitmap->Clip.Bottom = Bitmap->Height - (Self->BorderSize);

   for (auto item=Self->Items; item; item=item->Next) {
      if (Self->HighlightItem IS item) { // Draw highlighting rectangle in the background
         if (Self->Highlight.Alpha > 0) {
            gfxDrawRectangle(Bitmap, Self->HighlightLM, item->Y + Self->YPosition,
               Surface->Width - Self->HighlightRM - Self->HighlightLM, item->Height-1,
               PackPixelRGBA(Bitmap, &Self->Highlight), BAF_FILL);
         }

         if (Self->HighlightBorder.Alpha > 0) {
            gfxDrawRectangle(Bitmap, Self->HighlightLM, item->Y + Self->YPosition,
               Surface->Width - Self->HighlightRM - Self->HighlightLM, item->Height-1,
               PackPixelRGBA(Bitmap, &Self->HighlightBorder), 0);
         }
      }

      if (item->Background.Alpha) { // If the menu has a background colour, draw it
         gfxDrawRectangle(Bitmap, Self->HighlightLM, item->Y + Self->YPosition,
            Surface->Width - Self->HighlightRM - Self->HighlightLM, item->Height,
            PackPixelRGBA(Bitmap, &item->Background), BAF_FILL);
      }

      // Draw associated menu icon

      if (item->Flags & MIF_SELECTED) {
         if (!Self->Checkmark) {
            gfxDrawEllipse(Bitmap, x+(Self->ImageSize>>1), y+(Self->ImageSize>>1), Self->ImageSize>>1, Self->ImageSize>>1, 0, TRUE);
         }
         else {
            objPicture *picture = Self->Checkmark;
            gfxCopyArea(picture->Bitmap, Bitmap, BAF_BLEND, 0, 0, picture->Bitmap->Width, picture->Bitmap->Height,
               x + ((Self->ImageSize - picture->Bitmap->Width)>>1), y + ((item->Height - picture->Bitmap->Height)>>1));
         }
      }
      else if ((item->Bitmap) and (Self->Flags & MNF_SHOW_IMAGES)) {
         objBitmap *imgbmp = item->Bitmap;
         gfxCopyArea(imgbmp, Bitmap, BAF_BLEND, 0, 0, imgbmp->Width, imgbmp->Height,
            x + ((Self->ImageSize - imgbmp->Width)>>1), y + ((item->Height - imgbmp->Height)>>1));
      }

      // Set the correct font colour

      if (Self->HighlightItem IS item) font->Colour = Self->FontHighlight;
      else if (item->Colour.Alpha) font->Colour = item->Colour;
      else font->Colour = Self->FontColour;

      if (item->Flags & MIF_DISABLED) font->Colour.Alpha = font->Colour.Alpha>>1;

      // Draw control key text

      if ((Self->Flags & MNF_SHOW_KEYS) and (item->KeyString[0])) {
         if (((Self->Flags & MNF_SHOW_IMAGES) and (Self->ImageSize)) or (Self->ShowCheckmarks)) {
            font->X = x + Self->ImageSize + Self->ImageGap + Self->TextWidth + Self->KeyGap;
         }
         else font->X = x + Self->TextWidth + Self->KeyGap;

         font->Y = y + ((get_item_height(Self) - font->MaxHeight)>>1) + font->Leading;
         SetString(font, FID_String, item->KeyString);
         font->Align |= ALIGN_RIGHT;
         font->AlignWidth = Self->KeyWidth;
         acDraw(font);
         font->Align &= ~ALIGN_RIGHT;
      }

      if (item->Text) { // Draw menu item text
         if (((Self->Flags & MNF_SHOW_IMAGES) and (Self->ImageSize)) or (Self->ShowCheckmarks)) font->X = x + Self->ImageSize + Self->ImageGap;
         else font->X = x;

         font->Y = y + ((get_item_height(Self) - font->MaxHeight)>>1) + font->Leading;
         SetString(font, FID_String, item->Text);

         acDraw(font);
      }

      if (item->Flags & MIF_EXTENSION) { // Draw arrow for menu extensions
         LONG awidth  = 5;
         LONG aheight = 9;
         LONG ax = Surface->Width - Self->RightMargin - awidth - 4;
         LONG ay = y + ((item->Height - 9)>>1);

         ULONG colour;
         if (Self->HighlightItem IS item) colour = bmpGetColourRGB(Bitmap, &Self->FontHighlight);
         else colour = bmpGetColourRGB(Bitmap, &Self->FontColour);

         if (aheight & 1) {
            LONG ey = ay;
            while (ey < ay + aheight) {
               gfxDrawLine(Bitmap, ax + awidth - 1, ay + (aheight>>1), ax, ey, colour);
               ey++;
            }
         }
         else {
            LONG ey = ay;
            while (ey <= ay+(aheight>>1)-1) {
               gfxDrawLine(Bitmap, ax+awidth-1, ay+(aheight>>1)-1, ax, ey, colour);
               ey++;
            }

            while (ey < ay+aheight) {
               gfxDrawLine(Bitmap, ax + awidth - 1, ay + (aheight>>1), ax, ey, colour);
               ey++;
            }
         }
      }

      y += item->Height;
   }

   Bitmap->Clip = clip;
}

//****************************************************************************

static void draw_default_bkgd(objMenu *Self, objSurface *Surface, objBitmap *Bitmap)
{
   static RGB8 rgbColour = { .Red = 250, .Green = 250, .Blue = 250, .Alpha = 255 };
   static RGB8 rgbBorder = { .Red = 50, .Green = 50, .Blue = 50, .Alpha = 255 };
   ULONG colour = PackPixelRGBA(Bitmap, &rgbColour);
   ULONG border = PackPixelRGBA(Bitmap, &rgbBorder);
   gfxDrawRectangle(Bitmap, 0, 0, Surface->Width, Surface->Height, colour, BAF_FILL);
   gfxDrawRectangle(Bitmap, 0, 0, Surface->Width, Surface->Height, border, 0);
}

//****************************************************************************
// Calculates the height to be used for individual items.

static LONG get_item_height(objMenu *Self)
{
   LONG itemheight = Self->Font->MaxHeight + Self->VSpacing;
   if (((Self->Flags & MNF_SHOW_IMAGES) or (Self->ShowCheckmarks)) and (itemheight < 16 + Self->VSpacing)) {
      itemheight = 16 + Self->VSpacing;
   }

   // Height must be >= to the minimum allowed

   if (itemheight < Self->ItemHeight) itemheight = Self->ItemHeight;
   return itemheight;
}

//****************************************************************************
// Calculates the size of the menu's surface based on the available items.

static ERROR calc_menu_size(objMenu *Self)
{
   if (Self->FixedWidth) Self->Width = Self->FixedWidth;
   else if (!Self->Items) {
      Self->Width = 100;
      Self->Height = Self->TopMargin + Self->BottomMargin + get_item_height(Self);
      return ERR_Okay;
   }
   else {
      Self->Width = 0;
      Self->TextWidth = 0;
      Self->KeyWidth = 0;

      for (auto scan=Self->Items; scan; scan=scan->Next) {
         if (scan->Text) {
            LONG strwidth;
            SetString(Self->Font, FID_String, scan->Text);
            GetLong(Self->Font, FID_Width, &strwidth);

            strwidth += 8; // Add a reasonable pixel gap so that text doesn't go too near the right edge

            if (strwidth > Self->TextWidth) Self->TextWidth = strwidth;

            if ((scan->KeyString[0]) and (Self->Flags & MNF_SHOW_KEYS)) {
               SetString(Self->Font, FID_String, scan->KeyString);
               GetLong(Self->Font, FID_Width, &strwidth);
               if (strwidth > Self->KeyWidth) Self->KeyWidth = strwidth;
            }
         }
      }

      if (Self->KeyWidth > 0) Self->Width = Self->TextWidth + Self->KeyGap + Self->KeyWidth;
      else Self->Width = Self->TextWidth;

      if ((Self->Flags & MNF_SHOW_IMAGES) or (Self->ShowCheckmarks)) {
         Self->Width += Self->ImageSize + Self->ImageGap;
      }

      Self->Width += Self->LeftMargin + Self->RightMargin;

      // If extension menu items are present then add some extra space for arrow graphics

      for (auto scan=Self->Items; scan; scan=scan->Next) {
         if (scan->Flags & MIF_EXTENSION) {
            Self->Width += Self->ExtensionGap;
            break;
         }
      }
   }

   if (Self->prvLastItem) Self->PageHeight = Self->prvLastItem->Y + Self->prvLastItem->Height + Self->BottomMargin;
   else Self->PageHeight = Self->TopMargin + get_item_height(Self) + Self->BottomMargin;
   Self->Height = Self->PageHeight;

   LONG total = 1;
   for (auto scan=Self->Items; scan; scan=scan->Next, total++) {
      if (total >= Self->LineLimit) {
         Self->Height = scan->Y + scan->Height + Self->BottomMargin;
         break;
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static void calc_scrollbar(objMenu *Menu)
{
   if (!Menu->MenuSurfaceID) return;

   LONG total = 1;
   for (auto scan=Menu->Items; scan; scan=scan->Next, total++) {
      if (total >= Menu->LineLimit) {
         if (!Menu->Scrollbar) {
            if (!CreateObject(ID_SCROLLBAR, 0, &Menu->Scrollbar,
                  FID_Owner|TLONG,    Menu->MenuSurfaceID,
                  FID_Y|TLONG,        0,
                  FID_XOffset|TLONG,  0,
                  FID_YOffset|TLONG,  0,
                  FID_Direction|TSTR, "Vertical",
                  TAGEND)) {
               objScroll *vscroll = Menu->Scrollbar->Scroll;
               SetLong(vscroll, FID_Object, Menu->Head.UniqueID);
            }
         }

         struct scUpdateScroll scroll = {
            .PageSize = Menu->PageHeight,
            .ViewSize = Menu->Height,
            .Position = -Menu->YPosition,
            .Unit     = get_item_height(Menu)
         };
         Action(MT_ScUpdateScroll, Menu->Scrollbar->Scroll, &scroll);

         acShow(Menu->Scrollbar);
         return;
      }
   }

   if (Menu->Scrollbar) { acFree(Menu->Scrollbar); Menu->Scrollbar = NULL; }
}

/*****************************************************************************
** Prevents the menu from dropping off the edge of the screen.
*/

static void ensure_on_display(objMenu *Self)
{
   parasol::Log log(__FUNCTION__);

   if ((Self->TargetID) and (Self->MenuSurfaceID)) {
      SURFACEINFO *target;
      if (!drwGetSurfaceInfo(Self->TargetID, &target)) {
         LONG target_width = target->Width; // Take a copy before the next call.
         LONG target_height = target->Height;

         SURFACEINFO *info;
         if (!drwGetSurfaceInfo(Self->MenuSurfaceID, &info)) {
            LONG mx = info->X, my = info->Y, mwidth = info->Width, mheight = info->Height;
            LONG y = 0;
            LONG flags = 0;
            if ((my + mheight) > target_height) {
               // The menu goes past the viewable area, so reverse its position
               y = my - mheight;
               if (Self->ParentID) y += get_item_height(Self) + Self->VWhiteSpace;
               if (y < 2) y = 2; // Don't allow the menu to shoot off the top of the display
               flags |= MTF_Y;
            }

            LONG x = 0;
            if ((mx + mwidth) >= target_width) {
               Self->prvReverseX = TRUE; // Set reverse opening order
               x = mx - mwidth;
               if (Self->ParentID) {
                  objMenu *parent;
                  if (!AccessObject(Self->ParentID, 3000, &parent)) {
                     SURFACEINFO *pinfo;
                     if (!drwGetSurfaceInfo(parent->MenuSurfaceID, &pinfo)) {
                        x = pinfo->X - mwidth + Self->RightMargin;
                     }
                     ReleaseObject(parent);
                  }
               }
               if (x < 2) x = 2;
               flags |= MTF_X;
            }

            acMoveToPointID(Self->MenuSurfaceID, x, y, 0, flags);
         }
         else log.warning(ERR_Failed);
      }
      else log.warning(ERR_Failed);
   }
}

//*****************************************************************************

static ERROR create_menu(objMenu *Self)
{
   parasol::Log log(__FUNCTION__);

   log.branch();

   if ((Self->MenuSurfaceID) and (!(Self->Flags & MNF_CACHE))) {
      acFreeID(Self->MenuSurfaceID);
      Self->MenuSurfaceID = 0;
   }

   if (Self->InputHandle) { gfxUnsubscribeInput(Self->InputHandle); Self->InputHandle = 0; }

   calc_menu_size(Self);

   ERROR error;

   objSurface *surface;
   if (!NewLockedObject(ID_SURFACE, NF_INTEGRAL, &surface, &Self->MenuSurfaceID)) {
      if (Self->TargetID) SetLong(surface, FID_Owner, Self->TargetID);
      else SetLong(surface, FID_Owner, CurrentTaskID()); // Menu will open on the host desktop with this option.

      SetFields(surface,
         FID_X|TLONG,      Self->X,
         FID_Y|TLONG,      Self->Y,
         FID_Width|TLONG,  Self->Width,
         FID_Height|TLONG, Self->Height,
         FID_Flags|TLONG,  surface->Flags | RNF_STICK_TO_FRONT,
         FID_WindowType|TLONG, SWIN_NONE,
         TAGEND);

      // If the fade-in feature has been enabled, set the surface's opacity to zero

      if ((Self->FadeDelay > 0) and (!Self->Scrollbar)) SetLong(surface, FID_Opacity, 0);

      if ((Self->Flags & MNF_POPUP) and (!Self->ParentID)) {
         // Root popup menus are allowed to gain the focus
      }
      else SetLong(surface, FID_Flags, surface->Flags|RNF_NO_FOCUS);

      if (drwGetModalSurface(CurrentTaskID())) SetLong(surface, FID_Modal, TRUE);

      if (!(error = acInit(surface))) {
         if (drwApplyStyleGraphics(Self, Self->MenuSurfaceID, "menu", NULL)) {
            drwAddCallback(surface, (APTR)&draw_default_bkgd);
         }

         // If a modal surface is active for the task, then the menu surface must be modal in order for it to function
         // correctly.

         drwAddCallback(surface, (APTR)&draw_menu);

         SubscribeActionTags(surface, AC_Show, AC_Hide, AC_LostFocus, TAGEND);

         auto callback = make_function_stdc(consume_input_events);
         gfxSubscribeInput(&callback, surface->Head.UniqueID, JTYPE_MOVEMENT|JTYPE_BUTTON, 0, &Self->InputHandle);

         // Calculate the correct coordinates for our menu.  This may mean retrieving the absolute coordinates of the
         // relative surface and using them to offset the menu coordinates.

         if (Self->RelativeID) {
            OBJECTPTR relative;
            if (!AccessObject(Self->RelativeID, 5000, &relative)) {
               SubscribeAction(relative, AC_LostFocus);

               LONG x, y;
               if (!GetFields(relative, FID_AbsX|TLONG, &x, FID_AbsY|TLONG, &y, TAGEND)) {
                  acMoveToPoint(surface, Self->X + x, Self->Y + y, 0, MTF_X|MTF_Y);
               }
               ReleaseObject(relative);
            }
         }

         Self->VWhiteSpace = surface->BottomMargin;
      }
      else {
         acFree(surface);
         Self->MenuSurfaceID = 0;
      }

      ReleaseObject(surface);
   }
   else error = ERR_NewObject;

   if (error) return error;

   for (auto item=Self->Items; item; item=item->Next) { // Regenerate item breaks and custom item graphics
      if (item->Flags & MIF_BREAK) {
         if (drwApplyStyleGraphics(item, Self->MenuSurfaceID, "menu", "brk")) {

         }
      }
   }

   calc_scrollbar(Self);
   ensure_on_display(Self);

   return ERR_Okay;
}

//****************************************************************************

static ERROR process_menu_content(objMenu *Self)
{
   parasol::Log log(__FUNCTION__);

   if (Self->Config) {
      objXML *xml;
      if (!CreateObject(ID_XML, NF_INTEGRAL, &xml, FID_Statement|TSTR, Self->Config, TAGEND)) {
         for (auto tag=xml->Tags[0]; tag; tag=tag->Next) parse_xmltag(Self, xml, tag);
         acFree(xml);
         return ERR_Okay;
      }
      else return log.warning(ERR_CreateObject);
   }

   // Identify the type of data that has been set in the path field.  XML files are supported, but the developer can
   // also write a complete XML string to the path field if desired.

   CLASSID classid;
   if (Self->Path) {
      if (*Self->Path IS '<') classid = ID_XML;
      else IdentifyFile(Self->Path, 0, 0, &classid, 0, 0);
   }
   else classid = 0;

   if (classid IS ID_XML) {
      if (Self->Path[0] IS '<') SetString(Self->prvXML, FID_Statement, Self->Path);
      else SetString(Self->prvXML, FID_Path, Self->Path);

      // Find the first <menu> tag

      LONG i, j;
      for (i=0; Self->prvXML->Tags[i]; i++) {
         if (!StrMatch("menu", Self->prvXML->Tags[i]->Attrib->Name)) {
            if (Self->prvNode[0]) {
               // If a node is specified, we have to check that the menu name matches the node name.  If it doesn't,
               // we'll keep searching for a specific menu definition.

               for (j=1; j < Self->prvXML->Tags[i]->TotalAttrib; j++) {
                  if (!StrMatch("name", Self->prvXML->Tags[i]->Attrib[j].Name)) {
                     if (!StrMatch(Self->prvNode, Self->prvXML->Tags[i]->Attrib[j].Value)) {
                        break;
                     }
                  }
               }

               if (j < Self->prvXML->Tags[i]->TotalAttrib) break;
            }
            else break;
         }
      }

      if (!Self->prvXML->Tags[i]) {
         log.warning("No <menu> tag was found in file \"%s\".", Self->Path);
         return ERR_InvalidData;
      }

      for (auto tag=Self->prvXML->Tags[i]->Child; tag; tag=tag->Next) {
         add_xml_item(Self, Self->prvXML, tag);
      }
   }
   else if (classid) log.warning("File \"%s\" belongs to unsupported class #%d.", Self->Path, classid);

   return ERR_Okay;
}

//****************************************************************************

void select_item(objMenu *Self, objMenuItem *Item, BYTE Toggle)
{
   // Record the most recent item to be executed.

   SetLong(Self, FID_SelectionIndex, Item->Index);

   Self->Selection = Item;

   if (Item->Group) {
      if (!(Item->Flags & MIF_SELECTED)) {
         for (auto scan=Self->Items; scan; scan=scan->Next) {
            if (scan->Group IS Item->Group) {
               if (scan->Flags & MIF_SELECTED) {
                  scan->Flags &= ~MIF_SELECTED;
                  break;
               }
            }
         }

         Item->Flags |= MIF_SELECTED;
      }
   }
   else if (Item->Flags & MIF_TOGGLE) {
      if (Toggle) {
         if (Item->Flags & MIF_SELECTED) Item->Flags &= ~MIF_SELECTED;
         else Item->Flags |= MIF_SELECTED;
      }
      else Item->Flags |= MIF_SELECTED;
   }
}

//****************************************************************************
// The path of the icon should be supplied in the format:  category/name
// Any size references will be stripped from the path.

static ERROR load_icon(objMenu *Self, CSTRING Path, objBitmap **Bitmap)
{
   parasol::Log log(__FUNCTION__);

   log.branch("Path: %s", Path);

   // Load the icon graphic for this menu.  Failure should not be considered terminal if the image cannot be loaded -
   // the image should simply not appear in the menu bar.

   if (StrCompare("icons:", Path, 6, 0)) Path += 6;
   char buffer[120];
   LONG i = StrCopy(Path, buffer, sizeof(buffer));

   // Strip out any existing size references

   if (buffer[i-1] IS ')') {
      while ((i > 0) and (buffer[i] != '(')) i--;
      if (buffer[i] IS '(') buffer[i] = 0;
   }

   ERROR error = widgetCreateIcon(buffer, "Menu", Self->IconFilter, 16, Bitmap);
   return error;
}

//****************************************************************************

static ERROR write_string(OBJECTPTR File, CSTRING String)
{
   struct acWrite write;
   write.Buffer = String;
   write.Length = StrLength(String);
   return Action(AC_Write, File, &write);
}

//****************************************************************************

static ERROR highlight_item(objMenu *Self, objMenuItem *Item)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("Item %p, Existing %p)", Item, Self->HighlightItem);

   if (Item IS Self->HighlightItem) return ERR_Okay;

   if ((Item) and (Item->Flags & (MIF_BREAK|MIF_DISABLED))) return ERR_Okay;

   objSurface *surface;
   if (!AccessObject(Self->MenuSurfaceID, 3000, &surface)) {
      // Redraw the previously highlighted item

      if (Self->HighlightItem) {
         LONG y      = Self->HighlightItem->Y + Self->YPosition;
         LONG height = Self->HighlightItem->Height;
         Self->HighlightItem = NULL;
         surface->Frame = 1;
         acDrawArea(surface, 0, y, 10000, height);
      }

      Self->HighlightItem = Item;

      // Draw the newly highlighted area

      if (Item) {
         surface->Frame = 2;
         acDrawArea(surface, 0, Item->Y + Self->YPosition, 10000, Item->Height);
         surface->Frame = 1;
      }

      ReleaseObject(surface);
   }
   else log.warning(ERR_AccessObject);

   return ERR_Okay;
}
