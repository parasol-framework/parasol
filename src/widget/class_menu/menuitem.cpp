/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
MenuItem: Manages the data of menu items.

The MenuItem is an integral part of the @Menu class.  It is used to represent the individual items that
are listed in a menu.  Following initialisation, any MenuItem can be modified at run-time to make simple changes to
the menu.  For complex or extensive changes, it may be more efficient to recreate the menu from scratch.

It is not possible for a MenuItem to be owned by any object other than a @Menu.

-END-

*****************************************************************************/

static ERROR load_submenu(objMenu *, objMenu **, objMenuItem *);
static ERROR set_qualifier(objMenuItem *, CSTRING);
static ERROR set_key(objMenuItem *, CSTRING);

//****************************************************************************

static ERROR ITEM_Activate(objMenuItem *Item, APTR Void)
{
   parasol::Log log;
   log.traceBranch("Executing item \"%s\".", Item->Text);

   if (Item->Flags & MIF_EXTENSION) {
      if (Item->Flags & MIF_DISABLED) return ERR_Okay;

      select_item(Item->Menu, Item, TRUE);

      log.trace("exec_item: Item is an extension (%d).  Hiding %d", (Item->Menu) ? Item->Menu->Head.UniqueID : 0, (Item->Menu->CurrentMenu) ? Item->Menu->CurrentMenu->Head.UniqueID : 0);

      // Hide any currently open sub-menu

      if ((Item->Menu->CurrentMenu) and (Item->Menu->CurrentMenu != Item->SubMenu)) {
         acHide(Item->Menu->CurrentMenu);
      }

      // Either set up the existing sub-menu or create a new one if it does not exist yet.

      objMenu *menu;
      if ((menu = Item->SubMenu)) {
         log.trace("exec_item: Activating existing child menu #%d.", menu->Head.UniqueID);

         // Hide any active sub menus that belong to the child

         if (menu->CurrentMenu) {
            acHide(menu->CurrentMenu);
            menu->CurrentMenu = NULL;
         }

         menu->prvReverseX = Item->Menu->prvReverseX; // Inherit current reverse status

         acShow(menu);
      }
      else if (!load_submenu(Item->Menu, &menu, Item)) {
         Item->SubMenu = menu;
         if (!acShow(Item->SubMenu)) Item->Menu->CurrentMenu = Item->SubMenu;
      }
      else return log.warning(ERR_NewObject);

      menu->ParentItem = Item;
   }
   else {
      // Instantly hide the root menu surface (no fading).  We also switch the focus to the object that we are relative to.

      LONG flags;
      if ((!drwGetSurfaceFlags(Item->Menu->MenuSurfaceID, &flags)) and (flags & RNF_VISIBLE)) {
         if (Item->Menu->RootMenu IS Item->Menu) {
            if (Item->Menu->MenuSurfaceID) acHideID(Item->Menu->MenuSurfaceID);
            if (Item->Menu->RelativeID) acFocusID(Item->Menu->RelativeID);
         }
         else {
            if (Item->Menu->RootMenu) {
               if (Item->Menu->RootMenu->MenuSurfaceID) acHideID(Item->Menu->RootMenu->MenuSurfaceID);
               if (Item->Menu->RootMenu->RelativeID) acFocusID(Item->Menu->RootMenu->RelativeID);
            }
         }
      }

      if (Item->Flags & MIF_DISABLED) return ERR_Okay;

      select_item(Item->Menu, Item, TRUE);

      if (Item->Menu->RootMenu->ItemFeedback.Type) {
         FUNCTION *fb = &Item->Menu->RootMenu->ItemFeedback;
         if (fb->Type IS CALL_STDC) {
            auto routine = (void (*)(objMenu *, objMenuItem *))fb->StdC.Routine;
            parasol::SwitchContext ctx(fb->StdC.Context);
            routine(Item->Menu, Item);
         }
         else if (fb->Type IS CALL_SCRIPT) {
            OBJECTPTR script;
            if ((script = fb->Script.Script)) {
               const ScriptArg args[] = {
                  { "Menu", FD_OBJECTPTR, { .Address = Item->Menu } },
                  { "Item", FD_OBJECTPTR, { .Address = Item } }
               };
               scCallback(script, fb->Script.ProcedureID, args, ARRAYSIZE(args));
            }
         }
      }

      // User notification for the parent menu occurs when an item is clicked

      NotifySubscribers(Item->Menu, AC_Activate, 0, 0, ERR_Okay);
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR ITEM_DataFeed(objMenuItem *Self, struct acDataFeed *Args)
{
   if (Args->DataType IS DATA_XML) { // For menu items that open sub-menus
      if (Self->ChildXML) { FreeResource(Self->ChildXML); Self->ChildXML = NULL; }
      Self->ChildXML = StrClone((CSTRING)Args->Buffer);
      return ERR_Okay;
   }
   else return ERR_NoSupport;
}

/*****************************************************************************
-ACTION-
Disable: Disables a menu item, preventing user interaction.
-END-
*****************************************************************************/

static ERROR ITEM_Disable(objMenuItem *Self, APTR Void)
{
   Self->Flags |= MIF_DISABLED;
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Enable: Enables user interaction with the item.
-END-
*****************************************************************************/

static ERROR ITEM_Enable(objMenuItem *Self, APTR Void)
{
   Self->Flags &= ~MIF_DISABLED;
   return ERR_Okay;
}

//****************************************************************************

static ERROR ITEM_Free(objMenuItem *Self, APTR Void)
{
   if (Self->Bitmap)       { acFree(Self->Bitmap);           Self->Bitmap = NULL; }
   if (Self->SubMenu)      { acFree(Self->SubMenu);          Self->SubMenu = NULL; }
   if (Self->Name)         { FreeResource(Self->Name);       Self->Name = NULL; }
   if (Self->Text)         { FreeResource(Self->Text);       Self->Text = NULL; }
   if (Self->Path)         { FreeResource(Self->Path);       Self->Path = NULL; }
   if (Self->ChildXML)     { FreeResource(Self->ChildXML);   Self->ChildXML = NULL; }
   if (Self->ObjectName)   { FreeResource(Self->ObjectName); Self->ObjectName = NULL; }

   if (Self->Prev) Self->Prev->Next = Self->Next;
   if (Self->Next) Self->Next->Prev = Self->Prev;
   if (Self->Menu) {
      if (Self IS Self->Menu->Items) Self->Menu->Items = Self->Next;
      if (Self IS Self->Menu->prvLastItem) Self->Menu->prvLastItem = Self->Prev;
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR ITEM_Init(objMenuItem *Self, APTR Void)
{
   parasol::Log log;

   if (!Self->Menu) return log.warning(ERR_UnsupportedOwner);

   objMenu *menu = Self->Menu;

   if (menu->prvLastItem) {
      Self->Index = menu->prvLastItem->Index + 1;
      menu->prvLastItem->Next = Self;
      Self->Prev = menu->prvLastItem;
   }
   else {
      Self->Index = 1;
      menu->Items = Self;
   }

   if (!Self->Prev) Self->Y = menu->TopMargin;
   else Self->Y = Self->Prev->Y + Self->Prev->Height;

   menu->prvLastItem = Self;

   return ERR_Okay;
}

//****************************************************************************

static ERROR ITEM_NewObject(objMenuItem *Self, APTR Void)
{
   Self->ID = 0x7fffffff;
   return ERR_Okay;
}

//****************************************************************************

static ERROR ITEM_NewOwner(objMenuItem *Self, struct acNewOwner *Args)
{
   parasol::Log log;
   if (Self->Menu) return log.warning(ERR_UnsupportedOwner); // Re-modification is not supported.
   if (GetClassID(Args->NewOwnerID) != ID_MENU) return ERR_UnsupportedOwner;
   Self->Menu = (objMenu *)GetObjectPtr(Args->NewOwnerID);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Background: Background colour for the item.

The background colour for the item is defined here.

-FIELD-
Bitmap: A @Bitmap icon to display next to item text.

If the configuration of a menu item refers to a valid picture file, this field will be set to a rendered Bitmap
icon after initialisation.

-FIELD-
Colour: The colour to use when rendering text for this item.

This field defines the colour that will be used for rendering the item's text.  The alpha component is supported
for blending with the background.

-FIELD-
Flags: Optional flags.

Optional flags are are defined here.

-FIELD-
Group: Group identifier, relevant for checkmark items.

Grouping can be used on checkmark items that need to behave in a way that is functionally identical to radio buttons.
When multiple checkmark items are the same group ID, activating any one of the items will cause the others to become
unmarked.

This field is set to zero by default, which turns off the grouping feature.

-FIELD-
Height: The pixel height of the item.

The current pixel height of the menu item is reflected in this field.  This field is calculated as required and
cannot be customised.

-FIELD-
ID: User-defined unique identifier.

It is recommended that all MenuItem's are given a unique, custom ID so that they can be easily referenced by methods
that support ID lookups.

-FIELD-
Index: Item index.  Follows the order of the items as listed in the menu.

The Index value reflects the position of this item within the menu.

-FIELD-
Key: Shortcut key for this item.

If a MenuItem is to be accessible via a keyboard shortcut, a valid key-code must be referenced here in conjunction with
a #Qualifiers value.

-FIELD-
Path: The path of a menu configuration file, if this item links to a sub-menu.

If the MenuItem opens a sub-menu, the path of the configuration file can be specified here.  If using an external
file is undesirable, consider passing the configuration through the XML #DataFeed() instead.

*****************************************************************************/

static ERROR ITEM_SET_Path(objMenuItem *Self, CSTRING Value)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }
   if ((Value) and (*Value)) Self->Path = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Name: The menu item can be given a name here.  It is not necessary for the string to be unique.

This field allows non-unique names to be assigned to menu items.

*****************************************************************************/

static ERROR ITEM_SET_Name(objMenuItem *Self, CSTRING Value)
{
   if (Self->Name) { FreeResource(Self->Name); Self->Name = NULL; }
   if ((Value) and (*Value)) Self->Name = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Next: The next MenuItem in the list.

Refers to the next MenuItem in the linked list, or NULL if this is the last entry.

-FIELD-
Prev: The previous item in the list.

Refers to the previous MenuItem in the linked list, or NULL if this is the first entry.

-FIELD-
Qualifiers: Key qualifiers for this item's keyboard shortcut.

If a MenuItem is to be accessible via a keyboard shortcut, at least one valid qualifier must be referenced here in
conjunction with a #Key value.  Valid qualifier flags are as follows:

<types lookup="KQ"/>

-FIELD-
SubMenu: Refers to a sub-menu if this item is a menu extension.

If an item refers to a sub-menu, the generated @Menu can be read from this field.

*****************************************************************************/

static ERROR ITEM_GET_SubMenu(objMenuItem *Self, objMenu **Value)
{
   if (Self->SubMenu) { *Value = Self->SubMenu; return ERR_Okay; }

   if ((Self->Head.Flags & NF_INITIALISED) and (Self->Flags & MIF_EXTENSION)) {
      ERROR error = load_submenu(Self->Menu, &Self->SubMenu, Self);
      *Value = Self->SubMenu;
      return error;
   }
   else return ERR_BadState;
}

/*****************************************************************************
-FIELD-
Text: The text string to display for this item.

The text string that is rendered in the item is declared here.
-END-
*****************************************************************************/

static ERROR ITEM_SET_Text(objMenuItem *Self, CSTRING Value)
{
   if (Self->Text) { FreeResource(Self->Text); Self->Text = NULL; }
   if ((Value) and (*Value)) Self->Text = StrClone(Value);
   return ERR_Okay;
}

static ERROR ITEM_GET_Y(objMenuItem *Self, LONG *Value)
{
   *Value = Self->Y;
   return ERR_Okay;
}

//****************************************************************************

static ERROR load_submenu(objMenu *ParentMenu, objMenu **SubMenu, objMenuItem *Item)
{
   parasol::Log log(__FUNCTION__);

   log.branch("");

   SURFACEINFO *info;
   if (drwGetSurfaceInfo(ParentMenu->MenuSurfaceID, &info) != ERR_Okay) {
      return log.warning(ERR_GetSurfaceInfo);
   }

   objMenu *menu;
   if (!NewObject(ID_MENU, NF_INTEGRAL, &menu)) {
      SetName(menu, Item->ObjectName ? (CSTRING)Item->ObjectName : (CSTRING)"submenu");
      menu->TargetID        = ParentMenu->TargetID;
      menu->ParentID        = ParentMenu->Head.UniqueID;
      menu->RootMenu        = ParentMenu->RootMenu;
      menu->Flags           = ParentMenu->Flags;
      menu->X               = info->X + info->Width - ParentMenu->RightMargin;
      menu->Y               = info->Y + Item->Y;
      menu->VSpacing        = ParentMenu->VSpacing;
      menu->VWhiteSpace     = ParentMenu->VWhiteSpace;
      menu->ParentItem      = NULL;
      menu->KeyMonitorID    = ParentMenu->KeyMonitorID;
      menu->LeftMargin      = ParentMenu->LeftMargin;
      menu->TopMargin       = ParentMenu->TopMargin;
      menu->BottomMargin    = ParentMenu->BottomMargin;
      menu->RightMargin     = ParentMenu->RightMargin;
      menu->ImageGap        = ParentMenu->ImageGap;
      menu->KeyGap          = ParentMenu->KeyGap;
      menu->ExtensionGap    = ParentMenu->ExtensionGap;
      menu->HighlightLM     = ParentMenu->HighlightLM;
      menu->HighlightRM     = ParentMenu->HighlightRM;
      menu->ItemHeight      = ParentMenu->ItemHeight;
      menu->BreakHeight     = ParentMenu->BreakHeight;
      menu->AutoExpand      = ParentMenu->AutoExpand;
      menu->FadeDelay       = ParentMenu->FadeDelay;
      menu->ImageSize       = ParentMenu->ImageSize;
      menu->FontColour      = ParentMenu->FontColour;
      menu->FontHighlight   = ParentMenu->FontHighlight;
      menu->Highlight       = ParentMenu->Highlight;
      menu->HighlightBorder = ParentMenu->HighlightBorder;
      menu->prvReverseX     = ParentMenu->prvReverseX;
      menu->VOffset         = Item->Y;
      menu->Font->Colour    = ParentMenu->Font->Colour;

      SetString(menu, FID_IconFilter, ParentMenu->IconFilter);

      SetFields(menu->Font, FID_Face|TSTR,     ParentMenu->Font->Face,
                            FID_Point|TDOUBLE, ParentMenu->Font->Point,
                            TAGEND);

      if (ParentMenu->Style) SetString(menu, FID_Style, ParentMenu->Style);

      // If the menu refers to a configuration file that needs to be categorised, we need to load the file and turn it
      // into an XML-Menu definition file.

      if (Item->Flags & MIF_CATEGORISE) {
         ERROR error;
         if ((error = create_menu_file(ParentMenu, menu, Item)) != ERR_Okay) {
            acFree(menu);
            ReleaseObject(menu);
            return log.warning(error);
         }
      }
      else {
         if (Item->Path) SetString(menu, FID_Path, Item->Path);

         if (acInit(menu) != ERR_Okay) {
            acFree(menu);
            ReleaseObject(menu);
            return log.warning(ERR_Init);
         }
      }

      VarCopy(ParentMenu->LocalArgs, menu->LocalArgs);

      // If there are child tags in our menu that we need to associate with this sub-menu, add them into the sub-menu
      // item list.  This is where each <item> tag is added.

      if (Item->ChildXML) {
         SetString(ParentMenu->prvXML, FID_Statement, Item->ChildXML);

         parasol::SwitchContext ctx(menu); // Ensure that any allocations are against the sub-menu
         for (auto tag=ParentMenu->prvXML->Tags[0]; tag; tag=tag->Next) {
            add_xml_item(menu, ParentMenu->prvXML, tag);
         }
         calc_menu_size(menu);
         acResizeID(menu->MenuSurfaceID, menu->Width, menu->Height, 0);
         calc_scrollbar(menu);
         ensure_on_display(menu);
      }
   }

   *SubMenu = menu;
   return ERR_Okay;
}

/*****************************************************************************
** This function turns configuration files into menu files.  The menu is sorted and organised according to the Category
** item in each section.  Multiple categories are allowed to organise the menu structure into sub-trees, e.g.
** "Development/SDK/Documentation"
*/

#define SIZE_MENU_BUFFER 4000 // Must be big enough to hold all category names

static void write_menu_items(objMenu *, objConfig *, OBJECTPTR, STRING, STRING *, LONG *, ConfigEntry *);

static void add_string(CSTRING String, STRING Buffer, LONG *Index, LONG *Total)
{
   // Check if the string is already in the buffer

   LONG i = 0;
   for (LONG j=0; j < Total[0]; j++) {
      if (!StrCompare(String, Buffer+i, 0, STR_MATCH_LEN|STR_MATCH_CASE)) return;
      while (Buffer[i]) i++;
      i++;
   }

   // Add the string to the end of the sequential string list

   for (LONG j=0; (String[j]) and (Index[0] < SIZE_MENU_BUFFER-1); j++) Buffer[Index[0]++] = String[j];
   Buffer[Index[0]++] = 0;
   Total[0] += 1;
}

static ERROR create_menu_file(objMenu *Self, objMenu *Menu, objMenuItem *item)
{
   parasol::Log log(__FUNCTION__);
   char buffer[SIZE_MENU_BUFFER], category[256];

   log.branch("");

   ERROR error = ERR_Failed;

   objConfig *config;
   if (CreateObject(ID_CONFIG, NF_INTEGRAL, &config,
         FID_Path|TSTR, item->Path,
         TAGEND) != ERR_Okay) return ERR_CreateObject;

   // Sort the configuration file immediately after loading.  Note that sorting occurs on the Text item, which
   // represents the text for each menu item.

   if ((Self->Flags & MNF_SORT) or (item->Flags & MIF_SORT)) {
      cfgSortByKey(config, "Text", FALSE);
   }

   // Gather all category fields in the config file into a sequential string list (string after string separated with
   // nulls) that we can send to StrBuildArray()

   LONG pos   = 0;
   LONG total = 0;
   ConfigEntry *entries = config->Entries;
   for (LONG i=0; i < config->AmtEntries; i++) {
      if (!StrMatch("category", entries[i].Key)) {
         LONG j = 0;
         while (entries[i].Data[j]) {
            while ((entries[i].Data[j]) and (entries[i].Data[j] != '/')) {
               category[j] = entries[i].Data[j];
               j++;
            }
            category[j] = 0;
            add_string(category, buffer, &pos, &total);
            if (entries[i].Data[j] IS '/') category[j++] = '/';
         }
      }
   }

   STRING *list;
   if ((list = StrBuildArray(buffer, pos, total, SBF_SORT|SBF_NO_DUPLICATES))) {
      OBJECTPTR file;
      if (!CreateObject(ID_FILE, NF_INTEGRAL, &file,
            FID_Path|TSTR,   "temp:menu.xml",
            FID_Flags|TLONG, FL_NEW|FL_WRITE,
            TAGEND)) {

         write_string(file, "<?xml version=\"1.0\"?>\n\n");
         write_string(file, "<menu>\n");

         LONG index = 0;
         while (list[index]) write_menu_items(Self, config, file, buffer, list, &index, entries);

         write_string(file, "</menu>\n");

         SetString(Menu, FID_Path, "temp:menu.xml");

         if (acInit(Menu) != ERR_Okay) {
            acFree(Menu);
            ReleaseObject(Menu);
            return ERR_Init;
         }

         flDelete(file, 0);
         acFree(file);

         error = ERR_Okay;
      }
      else error = ERR_CreateObject;

      FreeResource(list);
   }
   else error = ERR_InvalidData;

   return error;
}

static void write_menu_items(objMenu *Self, objConfig *config, OBJECTPTR file, STRING Buffer, STRING *List,
   LONG *Index, ConfigEntry *entries)
{
   CSTRING category = List[*Index];
   LONG i;
   for (i=0; category[i]; i++);
   while ((i > 0) and (category[i-1] != '/')) i--;

   StrFormat(Buffer, SIZE_MENU_BUFFER, "  <menu text=\"%s\" icon=\"folders/programfolder\">\n", category+i);
   write_string(file, Buffer);

   // Test the next category in the list.  If it is a sub-category, recurse into it

   CSTRING path = List[Index[0]];
   while (List[Index[0]+1]) {
      CSTRING str = List[Index[0]+1];
      for (i=0; (path[i]) and (path[i] IS str[i]); i++);
      if ((!path[i]) and (str[i] IS '/')) {
         // We've found a sub-category
         Index[0] = Index[0] + 1;
         write_menu_items(Self, config, file, Buffer, List, Index, entries);
         Index[0] = Index[0] - 1;
      }
      else break;
   }

   // Write out all items in the current category

   LONG section = 0;
   for (LONG i=0; i < config->AmtEntries; i++) {
      if (StrMatch(entries[i].Section, entries[section].Section) != ERR_Okay) {
         section = i;
      }

      if ((!StrMatch("category", entries[i].Key)) and (!StrMatch(category, entries[i].Data))) {
         write_string(file, "    <item");

         CSTRING str;
         if (!cfgReadValue(config, entries[i].Section, "Icon", &str)) {
            StrFormat(Buffer, SIZE_MENU_BUFFER, " icon=\"%s\"", str);
            write_string(file, Buffer);
         }

         if (!cfgReadValue(config, entries[i].Section, "Text", &str)) {
            StrFormat(Buffer, SIZE_MENU_BUFFER, " text=\"%s\"", str);
            write_string(file, Buffer);
         }

         write_string(file, ">\n");

         if (!cfgReadValue(config, entries[i].Section, "Command", &str)) {
            StrFormat(Buffer, SIZE_MENU_BUFFER, "      <%s/>\n", str);
            write_string(file, Buffer);
         }

         write_string(file, "    </item>\n");
      }
   }

   write_string(file, "  </menu>\n\n");

   // Increment the current list position before returning

   Index[0] = Index[0] + 1;
}

//****************************************************************************

static ERROR add_xml_item(objMenu *Self, objXML *XML, XMLTag *Tag)
{
   parasol::Log log(__FUNCTION__);

   if (!Tag) return ERR_NullArgs;
   if (!Tag->Attrib->Name) return ERR_Okay;

   ULONG hash_element = StrHash(Tag->Attrib->Name, 0);

   if (hash_element IS HASH_If) {
      if (if_satisfied(Self, Tag)) {
         for (XMLTag *tag=Tag->Child; tag; tag=tag->Next) {
            add_xml_item(Self, XML, tag);
         }
      }
      return ERR_Okay;
   }
   else if (hash_element IS HASH_Else) {
      // Execute the contents of the <else> tag if the last <if> statement was not satisfied
      if (tlSatisfied IS FALSE) {
         for (XMLTag *tag=Tag->Child; tag; tag=tag->Next) {
            add_xml_item(Self, XML, tag);
         }
      }
      return ERR_Okay;
   }
   else if (hash_element IS HASH_Menu) {
      objMenuItem *item;
      if (!NewObject(ID_MENUITEM, NF_INTEGRAL, &item)) {
         for (LONG i=1; i < Tag->TotalAttrib; i++) {
            ULONG hash = StrHash(Tag->Attrib[i].Name, 0);
            CSTRING value = Tag->Attrib[i].Value;

            switch(hash) {
               case HASH_ID:         SetString(item, FID_ID, value); break;
               case HASH_Icon:       if (Self->Flags & MNF_SHOW_IMAGES) load_icon(Self, value, &item->Bitmap); break;
               case HASH_Text:       set_translation(Self, &item->Head, FID_Text, value); break;
               case HASH_Sort:       item->Flags |= MIF_SORT; break;
               case HASH_Name:       SetString(item, FID_Name, value); break;
               case HASH_Categorise: item->Flags |= MIF_CATEGORISE; break;
               case HASH_ObjectName: SetString(item, FID_ObjectName, value); break;
               case HASH_Path:
               case HASH_Src:        SetString(item, FID_Path, value); break;
               default: log.warning("Unsupported menu attribute \"%s\".", Tag->Attrib[i].Name);
            }
         }

         item->Flags |= MIF_EXTENSION;
         item->Height = get_item_height(Self);

         if ((XML) and (Tag->Child)) {
            STRING childxml;
            if (!xmlGetString(XML, Tag->Child->Index, XMF_INCLUDE_SIBLINGS, &childxml)) {
               acDataXML(item, childxml);
               FreeResource(childxml);
            }
         }

         if (!acInit(item)) {
            if (Self->Flags & MNF_CACHE) {  // All sub-menus are pre-loaded if MNF_CACHE is used
               objMenu *submenu;
               GetPointer(item, FID_SubMenu, &submenu);
            }
         }
      }
      else return ERR_NewObject;
   }
   else if (hash_element IS HASH_Item) {
      objMenuItem *item;
      STRING qualifier = NULL;
      STRING key = NULL;

      if (!NewObject(ID_MENUITEM, NF_INTEGRAL, &item)) {
         for (LONG i=1; i < Tag->TotalAttrib; i++) {
            ULONG hash = StrHash(Tag->Attrib[i].Name, 0);
            STRING value = Tag->Attrib[i].Value;

            switch (hash) {
               case HASH_Icon:       if (Self->Flags & MNF_SHOW_IMAGES) load_icon(Self, value, &item->Bitmap); break;
               case HASH_Colour:     StrToColour(value, &item->Colour); break;
               case HASH_Background: StrToColour(value, &item->Background); break;
               case HASH_Disabled:   item->Flags |= MIF_DISABLED; break;
               case HASH_ID:         SetString(item, FID_ID, value); break;
               case HASH_KeyRepeat:  item->Flags |= MIF_KEY_REPEAT; break;
               case HASH_Select:
               case HASH_Selected:   item->Flags |= MIF_SELECTED; break;
               case HASH_Text:       set_translation(Self, &item->Head, FID_Text, value); break;
               case HASH_Key:        if (!set_key(item, value)) key = value; break;
               case HASH_Qualifier:  if (!set_qualifier(item, value)) qualifier = value; break;
               case HASH_NoKeyResponse: item->Flags |= MIF_NO_KEY_RESPONSE; break;
               case HASH_Group:
                  item->Group = StrToInt(value);
                  if ((!Self->Checkmark) and (item->CheckmarkFailed IS FALSE)) {
                     Self->ShowCheckmarks = TRUE;
                     if (SET_Checkmark(Self, "icons:items/checkmark(16)") != ERR_Okay) {
                        item->CheckmarkFailed = TRUE;
                     }
                  }
                  break;
               case HASH_Toggle:
                  item->Flags |= MIF_TOGGLE;
                  if ((!Self->Checkmark) and (item->CheckmarkFailed IS FALSE)) {
                     Self->ShowCheckmarks = TRUE;
                     if (SET_Checkmark(Self, "icons:items/checkmark(16)") != ERR_Okay) {
                        item->CheckmarkFailed = TRUE;
                     }
                  }
                  break;
            }
         }

         if (key) {
            LONG pos = 0;
            if (qualifier) {
               for (pos=0; (qualifier[pos]) and ((size_t)pos < sizeof(item->KeyString)-2); pos++) item->KeyString[pos] = qualifier[pos];
               item->KeyString[pos++] = '+';
            }
            for (LONG i=0; (key[i]) and ((size_t)pos < sizeof(item->KeyString)-1); i++) item->KeyString[pos++] = key[i];
            item->KeyString[pos] = 0;
         }

         item->Height = get_item_height(Self);

         if ((XML) and (Tag->Child)) {
            STRING childxml;
            if (!xmlGetString(XML, Tag->Child->Index, XMF_INCLUDE_SIBLINGS, &childxml)) {
               acDataXML(item, childxml);
               FreeResource(childxml);
            }
         }

         return acInit(item);
      }
      else return ERR_NewObject;
   }
   else if (hash_element IS HASH_Cache) {
      Self->Flags |= MNF_CACHE;
   }
   else if (hash_element IS HASH_Break) {
      objMenuItem *item;
      return(CreateObject(ID_MENUITEM, NF_INTEGRAL, &item,
         FID_Flags|TLONG,  MIF_BREAK,
         FID_Height|TLONG, Self->BreakHeight,
         TAGEND));
   }
   else {
      log.warning("Unsupported tag <%s>.", Tag->Attrib->Name);
      return ERR_Okay;
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR set_key(objMenuItem *Item, CSTRING Value)
{
   parasol::Log log(__FUNCTION__);

   Item->Key = 0;
   if ((Value) and (*Value)) {
      for (LONG i=0; i < K_LIST_END; i++) {
         if (!StrMatch(Value, glKeymapTable[i])) {
            Item->Key = i;
            return ERR_Okay;
         }
      }
   }

   log.warning("Unable to find a key symbol for '%s'.", Value);
   return ERR_Search;
}

//****************************************************************************

static const FieldDef clItemFlags[] = {
   { "Disabled",      MIF_DISABLED },
   { "Break",         MIF_BREAK },
   { "Extension",     MIF_EXTENSION },
   { "Categorise",    MIF_CATEGORISE },
   { "NoKeyResponse", MIF_NO_KEY_RESPONSE },
   { "KeyRepeat",     MIF_KEY_REPEAT },
   { "Sort",          MIF_SORT },
   { "Option",        MIF_OPTION },
   { "Selected",      MIF_SELECTED },
   { "Toggle",        MIF_TOGGLE },
   { NULL, 0 }
};

static const FieldDef clQualifiers[] = {
   { "LShift",   KQ_L_SHIFT   },
   { "RShift",   KQ_R_SHIFT   },
   { "CapsLock", KQ_CAPS_LOCK },
   { "LCtrl",    KQ_L_CONTROL },
   { "RCtrl",    KQ_R_CONTROL },
   { "LAlt",     KQ_L_ALT     },
   { "RAlt",     KQ_R_ALT     },
   { "LCommand", KQ_L_COMMAND },
   { "RCommand", KQ_R_COMMAND },
   { "NumPad",   KQ_NUM_PAD   },
   // Pairs
   { "Shift",    KQ_SHIFT   },
   { "Command",  KQ_COMMAND },
   { "Alt",      KQ_ALT     },
   { "Ctrl",     KQ_CONTROL },
   { "Control",  KQ_CONTROL },
   { NULL, 0 }
};

static const ActionArray clItemActions[] = {
   { AC_Activate,    (APTR)ITEM_Activate },
   { AC_DataFeed,    (APTR)ITEM_DataFeed },
   { AC_Disable,     (APTR)ITEM_Disable },
   { AC_Enable,      (APTR)ITEM_Enable },
   { AC_Free,        (APTR)ITEM_Free },
   { AC_Init,        (APTR)ITEM_Init },
   { AC_NewObject,   (APTR)ITEM_NewObject },
   { AC_NewOwner,    (APTR)ITEM_NewOwner },
   { 0, NULL }
};

//static const FunctionField argsSelect[] = { { "Toggle", FD_LONG }, { NULL, 0 } };

static const MethodArray clItemMethods[] = {
   { 0, NULL, NULL, 0 }
};

static const FieldArray clItemFields[] = {
   { "Prev",         FDF_OBJECT|FDF_R,     ID_MENUITEM, NULL, NULL },
   { "Next",         FDF_OBJECT|FDF_R,     ID_MENUITEM, NULL, NULL },
   { "Bitmap",       FDF_OBJECT|FDF_RW,    ID_BITMAP, NULL, NULL },
   { "SubMenu",      FDF_INTEGRAL|FDF_RW,  ID_MENU, (APTR)ITEM_GET_SubMenu, NULL },
   { "Path",         FDF_STRING|FDF_RW,    0, NULL, (APTR)ITEM_SET_Path },
   { "Name",         FDF_STRING|FDF_RW,    0, NULL, (APTR)ITEM_SET_Name },
   { "Text",         FDF_STRING|FDF_RW,    0, NULL, (APTR)ITEM_SET_Text },
   { "Flags",        FDF_LONGFLAGS|FDF_RW, (MAXINT)&clItemFlags, NULL, NULL },
   { "Key",          FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Qualifiers",   FDF_LONG|FDF_RW,      (MAXINT)&clQualifiers, NULL, NULL },
   { "Index",        FDF_LONG|FDF_R,       0, NULL, NULL },
   { "Group",        FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "ID",           FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Height",       FD_LONG|FDF_R,        0, NULL, NULL },
   { "Colour",       FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "Background",   FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "Y",            FDF_LONG|FDF_R,       0, (APTR)ITEM_GET_Y, NULL },
   END_FIELD
};

//****************************************************************************

static ERROR set_qualifier(objMenuItem *Item, CSTRING Value)
{
   Item->Qualifiers = 0;
   for (LONG i=0; clQualifiers[i].Value != 0; i++) {
      if (!StrMatch(Value, clQualifiers[i].Name)) {
         Item->Qualifiers |= clQualifiers[i].Value;
         return ERR_Okay;
      }
   }
   return ERR_Search;
}

//****************************************************************************

ERROR init_menuitem(void)
{
   return(CreateObject(ID_METACLASS, 0, &clMenuItem,
      FID_ClassVersion|TFLOAT, VER_MENUITEM,
      FID_Name|TSTRING,   "MenuItem",
      FID_Category|TLONG, CCF_GUI,
      FID_Flags|TLONG,    CLF_PROMOTE_INTEGRAL|CLF_PRIVATE_ONLY,
      FID_Actions|TPTR,   clItemActions,
      FID_Methods|TARRAY, clItemMethods,
      FID_Fields|TARRAY,  clItemFields,
      FID_Size|TLONG,     sizeof(objMenuItem),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND));
}

void free_menuitem(void)
{
   if (clMenuItem) { acFree(clMenuItem); clMenuItem = NULL; }
}
