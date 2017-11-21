/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
View: The View class is used to display XML data in a number of possible formats to the user.

The View is an interface class that facilitates the display and management of multiple items within a surface.  It is
capable of displaying items in a number of formats, including a simple list, tree view and column view. The ability to
sort data and handle different types of data such as date and time information is provided.  The View class is
commonly used for file displays. Certain classes such as the @FileView are dependent on the functionality that
it provides.

Items are created and stored within each view as a large XML statement and are managed via the @XML class.
This simplifies the creation and retrieval of item data and also allows you to develop hierarchies and tree structures
for complex item arrangement.  The View class inherits the fields and functionality of the @XML class, which
you can use for scanning view items after they have been added.  The @Font class is also inherited, allowing
you to set details such as the default font face through the view.

When the user double-clicks on a view item, the object will activate itself.  Once activated, the view object will send
Activate actions on to any children that have been initialised to the view.  This allows you to develop a response for
user interaction with the view.  If the sensitive option has been enabled as one of the #Flags attributes,
activation will occur whenever the user selects an item (thus single clicks will be enough to cause object activation).

To respond to the user's selection of view items at run time, it is recommended that you monitor the #SelectedTag
field for changes.
-END-

*****************************************************************************/

//#define DEBUG
//#define PICDEBUG // Allows you to diagnose picture caching issues

#define MAX_DRAGITEMS 4
#define SWITCH_SIZE 13

#define PRV_VIEW
#include <parasol/modules/widget.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/font.h>
#include <parasol/modules/display.h>
#include <parasol/modules/iconserver.h>
#include <parasol/modules/surface.h>
#include <parasol/modules/vector.h>

#include "../defs.h"

static APTR glCache = NULL;
static OBJECTPTR clView = NULL;
static UBYTE glDateFormat[28] = "dd-mm-yy hh:nn";
static UBYTE glPreferDragDrop = TRUE;
static objPicture *glTick = NULL;
static const LONG KEY_TICK = 1;

#define MIN_COLWIDTH 6

//****************************************************************************
// Node management

struct view_node {
   objBitmap *Icon, *IconOpen;
   ULONG IconKey, IconOpenKey;
   struct RGB8 FontRGB;
   LONG String;           // This is an offset into Self->NodeStrings
   LONG X;
   LONG Y;
   UBYTE Datatype[4];
   LONG Width, Height;
   WORD Flags;
   WORD Indent;
   UBYTE ChildString:1;
};

#define NODE_SELECTED      0x00000001  // Item is selected
#define NODE_HIGHLIGHTED   0x00000002  // Item is temporarily highlighted (e.g. the mouse pointer is lying over it)
#define NODE_NEWCOLUMN     0x00000004  // The node forms the top of a new column
#define NODE_CAN_DESELECT  0x00000010
#define NODE_OPEN          0x00000020  // Set if the item is expanded
#define NODE_CHILDREN      0x00000040  // Set if there are child items under this one
#define NODE_ITEM          0x00000080  // Set if the node is attached to an item
#define NODE_STRIPPED      0x00000100  // Content has been stripped of whitespace
#define NODE_TREEBOX       0x00000200  // Set if an expand/collapse box is present

//****************************************************************************
// Column management

struct view_col {
   struct view_col *Next;
   UBYTE Name[20];
   UBYTE Text[32];
   BYTE  Type;         // The type of data displayed in the column
   BYTE  Sort;         // Sort order
   WORD  Flags;        // Column flags
   LONG  Width;        // Pixel width
   struct RGB8 Colour;  // Background colour
};

struct CachedIcon {
   OBJECTPTR Icon;
   LONG Counter;
};

#define CF_COLOUR     0x0001
#define CF_SHOWICONS  0x0002 // Show icons in this column (NB: will be overridden if VWF_NO_ICONS is in use)
#define CF_RIGHTALIGN 0x0004
#define CF_DELETE     0x0008 // Marked for deletion (internal use only)

enum {
   SORT_UNSORTED = 0,
   SORT_ASCENDING,
   SORT_DESCENDING
};

enum {
   CT_VARIANT=1,
   CT_BYTESIZE,
   CT_NUMERIC,
   CT_DATE,
   CT_SECONDS,
   CT_CHECKBOX
};

static const struct FieldArray clFields[];
static const struct MethodArray clViewMethods[];
static const struct ActionArray clViewActions[];

//****************************************************************************

static void   arrange_items(objView *);
static ERROR  calc_hscroll(objView *);
static ERROR  calc_vscroll(objView *);
static void   check_selected_items(objView *, struct XMLTag *);
static void   check_pointer_cursor(objView *, LONG, LONG);
static BYTE   check_item_visible(objView *, struct XMLTag *);
static BYTE   deselect_item(objView *);
static void   drag_items(objView *);
static void   draw_columns(objView *, objSurface *, objBitmap *, struct ClipRectangle *, LONG ax, LONG ay, LONG awidth, LONG aheight);
static void   draw_shadow(objView *, objBitmap *, LONG);
static void   draw_item(objView *, struct XMLTag *);
static void   draw_view(objView *, objSurface *, objBitmap *);
static void   gen_group_bkgd(objView *, CSTRING, objBitmap **, CSTRING);
static void   get_col_value(objView *, struct XMLTag *, struct view_col *, STRING, LONG, struct XMLTag **);
static objBitmap * get_collapse_bitmap(objView *, LONG BPP);
static objBitmap * get_expand_bitmap(objView *, LONG BPP);
static struct XMLTag * get_item_xy(objView *, struct XMLTag **, LONG, LONG);
static ERROR  get_selected_tags(objView *, LONG **, LONG *);
static void   key_event(objView *, evKey *, LONG);
static ERROR  load_icon(objView *, CSTRING, objBitmap **, ULONG *);
static BYTE   open_branch_callback(objView *, struct XMLTag *);
static LONG   prepare_xml(objView *, struct XMLTag *, CSTRING, LONG);
static void   process_style(objView *, objXML *, struct XMLTag *);
static ERROR  report_cellclick(objView *, LONG TagIndex, LONG Column, LONG Input, LONG X, LONG Y);
static void   report_selection(objView *, LONG Type, LONG TagIndex);
static BYTE   select_item(objView *, struct XMLTag *, LONG, BYTE, BYTE);
static ERROR  sort_items(objView *);
static ERROR  unload_icon(objView *, ULONG *);
static void   vwUserClick(objView *, struct InputMsg *);
static void   vwUserClickRelease(objView *, struct InputMsg *);
static void   vwUserMovement(objView *, struct InputMsg *);

static ERROR VIEW_SortColumnIndex(objView *, struct viewSortColumnIndex *Args);
static ERROR SET_HScroll(objView *, OBJECTPTR Value);
static ERROR SET_VScroll(objView *, OBJECTPTR Value);
static ERROR SET_SelectionIndex(objView *, LONG Value);

//****************************************************************************

static STRING get_nodestring(objView *Self, struct view_node *Node)
{
   if (Node->String IS -1) return "";
   return Self->NodeStrings + Node->String;
}

//****************************************************************************
// Strings are defined as offsets within the string buffer referred to by NodeStrings.

static void set_nodestring(objView *Self, struct view_node *Node, CSTRING String)
{
   Node->String = -1;
   if ((!String) OR (!String[0])) return;

   LONG len;
   for (len=0; (String[len] >= 0x20); len++); // Only printable characters are allowed

   if (!Self->NodeStrings) {
      Self->NSSize = 4096;
      if (len+1 > Self->NSSize) Self->NSSize = len + 1;
      if (AllocMemory(Self->NSSize, MEM_STRING, &Self->NodeStrings, NULL)) {
         return;
      }
   }
   else if (Self->NSIndex + len + 1 >= Self->NSSize) {
      // Extend the buffer size.  The size is doubled on each reallocation.

      LONG newsize;
      newsize = Self->NSSize + len + 1 + (Self->NSSize<<1);
      if (ReallocMemory(Self->NodeStrings, newsize, &Self->NodeStrings, NULL)) {
         return;
      }
      Self->NSSize = newsize;
   }

   Node->String = Self->NSIndex;
   CopyMemory(String, Self->NodeStrings + Self->NSIndex, len);
   Self->NodeStrings[Self->NSIndex + len] = 0;
   Self->NSIndex += len + 1;
}

//****************************************************************************

ERROR init_view(void)
{
   if (LoadModule("vector", MODVERSION_VECTOR, &modVector, &VectorBase) != ERR_Okay) return ERR_InitModule;

   CSTRING str;
   if (!StrReadLocale("FileDate", &str)) {
      StrCopy(str, glDateFormat, sizeof(glDateFormat));
   }

   char buffer[] = "[glStyle./fonts/font[@name='default']/@face]:[glStyle./fonts/font[@name='default']/@size]";
   if (!StrEvaluate(buffer, sizeof(buffer), SEF_STRICT, 0)) {
      StrCopy(buffer, glDefaultFace, sizeof(glDefaultFace));
   }

   OBJECTPTR config;
   if (!CreateObject(ID_CONFIG, 0, &config,
         FID_Path|TSTR, "user:config/filesystem.cfg",
         TAGEND)) {
      if (!cfgReadValue(config, "FileView", "DragDrop", &str)) {
         if (StrToInt(str) IS 1) glPreferDragDrop = TRUE;
         else glPreferDragDrop = FALSE;
      }
      acFree(config);
   }

   glCache = VarNew(0, KSF_THREAD_SAFE);

   return CreateObject(ID_METACLASS, 0, &clView,
      FID_ClassVersion|TFLOAT, VER_VIEW,
      FID_Name|TSTR,      "View",
      FID_Category|TLONG, CCF_GUI,
      FID_Flags|TLONG,    CLF_PROMOTE_INTEGRAL|CLF_PRIVATE_ONLY,
      FID_Actions|TPTR,   clViewActions,
      FID_Methods|TARRAY, clViewMethods,
      FID_Fields|TARRAY,  clFields,
      FID_Size|TLONG,     sizeof(objView),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND);
}

void free_view(void)
{
   if (glTick)    { acFree(glTick);    glTick = NULL; }
   if (glCache)   { VarFree(glCache);  glCache = NULL; }
   if (clView)    { acFree(clView);    clView = NULL; }
}

//****************************************************************************

static void resize_view(objView *Self)
{
   if ((Self->Style IS VIEW_DOCUMENT) OR (Self->Document)) return;  // Documents manage themselves, do not reprocess

   arrange_items(Self); // Rearrange/recalculate dimensions for all view items

   if ((Self->GroupBitmap) AND (Self->GroupBitmap->Width != Self->PageWidth)) {
      if (Self->GroupHeaderXML) {
         gen_group_bkgd(Self, Self->GroupHeaderXML, &Self->GroupBitmap, "redimension-width");
      }

      if (Self->GroupSelectXML) {
         gen_group_bkgd(Self, Self->GroupSelectXML, &Self->SelectBitmap, "redimension-width");
      }
   }
}

//****************************************************************************

static ERROR VIEW_ActionNotify(objView *Self, struct acActionNotify *Args)
{
   if (Args->Error != ERR_Okay) return ERR_Okay;

   if (Args->ActionID IS AC_DragDrop) { // Something has been dropped onto the view
      struct acDragDrop *drag;
      LONG i;

      if ((drag = (struct acDragDrop *)Args->Args)) {
         if ((drag->SourceID IS Self->Head.UniqueID) OR (drag->SourceID IS Self->DragSourceID)) {
            // If the items belong to our own view, we must check that the items aren't being dropped onto themselves.

            if ((Self->HighlightTag != -1)  AND (Self->DragItems)) {
               for (i=0; i < Self->DragItemCount; i++) {
                  if (Self->DragItems[i] IS Self->HighlightTag) {
                     MSG("Drag & drop items cannot be dragged onto themselves.");
                     return ERR_Okay;
                  }
               }
            }
         }

         NotifySubscribers(Self, AC_DragDrop, drag, 0, ERR_Okay);
      }
   }
   else if (Args->ActionID IS AC_Disable) {
      if (!Self->RedrawDue) {
         Self->RedrawDue = TRUE;
         DelayMsg(AC_Draw, Self->Layout->SurfaceID, NULL);
      }
   }
   else if (Args->ActionID IS AC_Enable) {
      if (!Self->RedrawDue) {
         Self->RedrawDue = TRUE;
         DelayMsg(AC_Draw, Self->Layout->SurfaceID, NULL);
      }
   }
   else if (Args->ActionID IS AC_Focus) {
      if (!Self->prvKeyEvent) {
         FUNCTION callback;
         SET_FUNCTION_STDC(callback, &key_event);
         SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, &callback, Self, &Self->prvKeyEvent);
      }
   }
   else if (Args->ActionID IS AC_LostFocus) {
      if (Self->prvKeyEvent) { UnsubscribeEvent(Self->prvKeyEvent); Self->prvKeyEvent = NULL; }
   }
   else if (Args->ActionID IS AC_Free) {
      if ((Self->CellClick.Type IS CALL_SCRIPT) AND (Self->CellClick.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->CellClick.Type = CALL_NONE;
      }
      else if ((Self->SelectCallback.Type IS CALL_SCRIPT) AND (Self->SelectCallback.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->SelectCallback.Type = CALL_NONE;
      }
      else if ((Self->ExpandCallback.Type IS CALL_SCRIPT) AND (Self->ExpandCallback.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->ExpandCallback.Type = CALL_NONE;
      }
   }
   else if (Args->ActionID IS AC_Hide) {
      if ((Self->HScrollbar) AND (Self->HScrollbar->RegionID IS Args->ObjectID)) {
         Self->HBarVisible = FALSE;
      }
      else if ((Self->VScrollbar) AND (Self->VScrollbar->RegionID IS Args->ObjectID)) {
         Self->VBarVisible = FALSE;
      }
   }
   else if (Args->ActionID IS AC_Show) {
      if ((Self->HScrollbar) AND (Self->HScrollbar->RegionID IS Args->ObjectID)) {
         Self->HBarVisible = TRUE;
      }
      else if ((Self->VScrollbar) AND (Self->VScrollbar->RegionID IS Args->ObjectID)) {
         Self->VBarVisible = TRUE;
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR VIEW_Activate(objView *Self, APTR Void)
{
   //if ((Self->SelectedTag IS -1) AND (Self->ActiveTag IS -1)) return ERR_Okay;

   struct ChildEntry list[20];
   LONG count = ARRAYSIZE(list);
   if (!ListChildren(Self->Head.UniqueID, list, &count)) {
      LogBranch("%d children to activate.", count);

      LONG i;
      for (i=0; i < count; i++) {
         //DelayMsg(AC_Activate, list[i].ObjectID, NULL);
         acActivateID(list[i].ObjectID);
      }

      LogBack();
      return ERR_Okay;
   }
   else {
      MSG("No children in the view to activate.");
      return ERR_ListChildren;
   }
}

/******************************************************************************
-ACTION-
Clear: Clears a view of all internal content and updates the display.
-END-
******************************************************************************/

static ERROR VIEW_Clear(objView *Self, APTR Void)
{
   LogBranch(NULL);

   BYTE activate = (Self->SelectedTag != -1) ? TRUE : FALSE;

   Self->XPos = 0;
   Self->YPos = 0;
   Self->HighlightTag = -1;
   Self->SelectedTag  = -1; // Use SetField in case the field is watched
   Self->ActiveTag    = -1;
   report_selection(Self, SLF_ACTIVE|SLF_SELECTED, -1);

   // Free any loaded icons

   AdjustLogLevel(3);

   LONG index;
   for (index=0; Self->XML->Tags[index]; index++) {
      struct view_node *node = Self->XML->Tags[index]->Private;
      unload_icon(Self, &node->IconKey);
      unload_icon(Self, &node->IconOpenKey);
   }

   AdjustLogLevel(-3);

   acClear(Self->XML);

   if (Self->NodeStrings) {
      FreeMemory(Self->NodeStrings);
      Self->NodeStrings = NULL;
      Self->NSIndex = 0;
      Self->NSSize = 0;
   }

   arrange_items(Self);

   if (!Self->RedrawDue) {
      Self->RedrawDue = TRUE;
      DelayMsg(AC_Draw, Self->Layout->SurfaceID, NULL);
   }

   if ((activate) AND (Self->Flags & (VWF_NOTIFY_ON_CLEAR|VWF_SENSITIVE))) acActivate(Self);

   LogBack();
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
CloseBranch: Closes open tree branches.

This method can be used when a view is using the TREE, GROUPTREE or COLUMNTREE #Style.  It will close the open
branch referred to in the XPath or TagIndex parameters.  The view is then redrawn.

If the branch is already closed, this method does nothing.

-INPUT-
cstr XPath: An XPath that targets the item to be collapsed.  Can be NULL if TagIndex is defined.
int TagIndex: The tag index of the XML item that needs to be collapsed (XPath must be NULL or this parameter is ignored).  If XPath is NULL and TagIndex is -1, the currently selected tag will be targeted.

-ERRORS-
Okay
NullArgs
Search
-END-

*****************************************************************************/

static ERROR VIEW_CloseBranch(objView *Self, struct viewCloseBranch *Args)
{
   struct XMLTag *tag;
   LONG tagindex;
   if ((Args) AND (Args->XPath) AND (Args->XPath[0])) {
      if (xmlFindTag(Self->XML, Args->XPath, 0, &tagindex)) return PostError(ERR_Search);
   }
   else if ((Args) AND (Args->TagIndex >= 0) AND (Args->TagIndex < Self->XML->TagCount)) {
      tagindex = Args->TagIndex;
   }
   else {
      for (tag=Self->XML->Tags[0]; (tag) AND (tag->Index != Self->SelectedTag); tag=tag->Next);
      if (!tag) return PostError(ERR_Search);
      tagindex = tag->Index;
   }

   if ((tag = Self->XML->Tags[tagindex])) {
      struct view_node *node = tag->Private;
      if (node->Flags & NODE_OPEN) {
         node->Flags &= ~NODE_OPEN;

         arrange_items(Self);

         if (!Self->RedrawDue) {
            Self->RedrawDue = TRUE;
            DelayMsg(AC_Draw, Self->Layout->SurfaceID, NULL);
         }
      }
   }
   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
DataFeed: Items can be added to the view using data channels.

To add new items to a view, it is recommended that you use the data feed features.  New items can be added as raw
text or as a series of XML statements.

When DATA_TEXT data is sent to a view, it will be added as a single item to the view list.  The client will need to
send multiple fragments of text data if you need to more than one item using this method.

DATA_XML information is supported in an abstract fashion.  By default, each tag with a name of 'item' will be
parsed as an independent element for display in the view.  Here is a simple example from the FileView class that
represents a file:

<pre>
&lt;item icon="filetypes/audio"&gt;audio.wav
  &lt;size sort="0002106"&gt;2106&lt;/size&gt;
  &lt;date&gt;20031118 14:10:32&lt;/date&gt;
&lt;/item&gt;
</pre>

In COLUMN and COLUMN_TREE modes, child items within each element (such as the size and date in the example) can be
used to display content in the view columns.  Please refer to the #Columns field for further information on
this feature.

Special attributes may be set in item elements for interpretation by the view object.  The "icon" attribute defines
the icon that should be displayed against the item when icon graphics are enabled in the view.  The "iconopen"
attribute defines the icon for display when the element branch is open (applies to tree mode only).  The "custom"
attribute is used in tree mode to indicate that there may be children in the element if the user wishes to expand it.
It is normally used in conjunction with the #ExpandCallback field.

The content of the item tag, in this case "audio.wav" is used as the item text.  An icon has been specified to
accompany the item.  The size and date tags are special tag types that are specific to column mode and will be
ignored when not in use.

By default, the name of an item will be pulled from the content in its tag (in the example, 'audio.wav').  You can
change this behaviour and pull the name from an attribute if you set the #TextAttrib field.

Finally, if item identification via the 'item' naming convention is too limiting or conflicts with your source data,
set the #ItemNames field to declare your own valid item names.
-END-

*****************************************************************************/

static ERROR VIEW_DataFeed(objView *Self, struct acDataFeed *Args)
{
   struct XMLTag *tag;
   ERROR error;
   UBYTE buffer[300];

   if (!Args) return PostError(ERR_NullArgs);

   if (Args->DataType IS DATA_XML) {
      if (!Self->XML) return ERR_Failed;

      MSG("Received XML:\n%s", (CSTRING)Args->Buffer);

      LONG tagcount = Self->XML->TagCount;

      if (Action(AC_DataFeed, Self->XML, Args) != ERR_Okay) { // Convert the data to XML
         return PostError(ERR_Failed);
      }

      if (!StrMatch("style", Self->XML->Tags[tagcount]->Attrib->Name)) {
         process_style(Self, Self->XML, Self->XML->Tags[tagcount]);
         xmlRemoveTag(Self->XML, tagcount, 0); // Remove the tags now that they've been processed
         if (!Self->XML->Tags[tagcount]) return ERR_Okay;    // Return if there are no more tags
      }

      Self->HighlightTag = -1;
      Self->ActiveTag    = -1;
      Self->SelectedTag  = -1;
      prepare_xml(Self, Self->XML->Tags[tagcount], 0, 0); // This will update ActiveTag/SelectedTag
      report_selection(Self, SLF_ACTIVE|SLF_SELECTED, Self->SelectedTag);

      // Refresh the display

      if ((Self->Style IS VIEW_TREE) OR (Self->Style IS VIEW_GROUP_TREE) OR (Self->Style IS VIEW_COLUMN_TREE)) {
         arrange_items(Self);
         acDrawID(Self->Layout->SurfaceID);
      }
      else {
         if (Self->Sort[0]) error = sort_items(Self);
         else error = ERR_NothingDone;

         arrange_items(Self);

         if (error IS ERR_Okay) {
            acDrawID(Self->Layout->SurfaceID);
         }
         else {
            // The list didn't need to be sorted, so just draw the new items. If a new column had to be added then we
            // will need to redraw the entire view though.

            for (tag=Self->XML->Tags[tagcount]; tag; tag=tag->Next) {
               struct view_node *node = tag->Private;
               if (node->Flags & NODE_NEWCOLUMN) {
                  acDrawID(Self->Layout->SurfaceID);
                  break;
               }
            }

            if (!tag) {
               for (tag=Self->XML->Tags[tagcount]; tag; tag=tag->Next) {
                  draw_item(Self, tag);
               }
            }
         }
      }

      return ERR_Okay;
   }
   else if (Args->DataType IS DATA_TEXT) {
      // Pass the data through to XML

      MSG("Received text: %s", (STRING)Args->Buffer);

      LONG tagcount;
      GetLong(Self->XML, FID_TagCount, &tagcount);

      StrFormat(buffer, sizeof(buffer), "<item>%s</item>", (STRING)Args->Buffer);

      if (acDataXML(Self->XML, buffer) != ERR_Okay) return ERR_Failed;

      // Set default colour for new items

      for (tag=Self->XML->Tags[tagcount]; tag; tag=tag->Next) {
         struct view_node *node = tag->Private;
         node->FontRGB = Self->ColItem;
      }

      // Refresh the display

      if ((Self->Style IS VIEW_TREE) OR (Self->Style IS VIEW_GROUP_TREE) OR (Self->Style IS VIEW_COLUMN_TREE)) {
         acDrawID(Self->Layout->SurfaceID);
      }
      else {
         if (Self->Sort[0]) error = sort_items(Self);
         else error = ERR_NothingDone;

         arrange_items(Self);

         if (error IS ERR_Okay) {
            acDrawID(Self->Layout->SurfaceID);
         }
         else {
            // The list didn't need to be sorted, so just draw the new items
            for (tag=Self->XML->Tags[tagcount]; tag; tag=tag->Next) {
               struct view_node *node = tag->Private;
               acDrawAreaID(Self->Layout->SurfaceID, Self->Layout->BoundX + node->X, Self->Layout->BoundY + node->Y,
                  node->Width, (node->Flags & NODE_NEWCOLUMN) ? 16000 : node->Height);
            }
         }
      }

      return ERR_Okay;
   }
   else if (Args->DataType IS DATA_REQUEST) {
      if (Self->DragSourceID) return ActionMsg(AC_DataFeed, Self->DragSourceID, Args);
      else return ERR_NoSupport;
   }
   else if (Args->DataType IS DATA_INPUT_READY) {
      struct InputMsg *input, *scan;

      while (!gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &input)) {
         ERROR inputerror;
         if (input->Flags & JTYPE_MOVEMENT) {
            while (!(inputerror = gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &scan))) {
               if (scan->Flags & JTYPE_MOVEMENT) input = scan;
               else break;
            }

            vwUserMovement(Self, input);

            if (inputerror) break;
            else input = scan;

            // Note that this code has to 'drop through' due to the movement consolidation loop earlier in this subroutine.
         }

         if (input->Flags & JTYPE_BUTTON) {
            if (input->Value > 0) vwUserClick(Self, input);
            else vwUserClickRelease(Self, input);
         }
         else MSG("Unrecognised input message type $%.8x", input->Flags);
      }

      return ERR_Okay;
   }
   else return ERR_NoSupport; // Unrecognised datatype not considered fatal
}

/*****************************************************************************
-ACTION-
Disable: Disables the view.
-END-
*****************************************************************************/

static ERROR VIEW_Disable(objView *Self, APTR Void)
{
   return acDisableID(Self->Layout->SurfaceID);
}

/*****************************************************************************
-ACTION-
Draw: Redraws the surfaces that hosts the view.
-END-
*****************************************************************************/

static ERROR VIEW_Draw(objView *Self, struct acDraw *Args)
{
   return ActionMsg(AC_Draw, Self->Layout->SurfaceID, Args);
}

/*****************************************************************************
-ACTION-
Enable: Enables a view that has been disabled.
-END-
*****************************************************************************/

static ERROR VIEW_Enable(objView *Self, APTR Void)
{
   return acEnableID(Self->Layout->SurfaceID);
}

/*****************************************************************************

-METHOD-
OpenBranch: Automates the expansion of closed tree branches.

If a view is in tree mode, individual tree branches can be manually expanded by calling this method.  The XPath or
TagIndex parameter must indicate the branch that needs to be opened.  If the branch is already open, this method does
nothing.

Parent branches will not be expanded unless the Parents parameter is set to TRUE.

-INPUT-
cstr XPath: The XML path of the item to expand, or NULL if expanding by TagIndex.
int TagIndex: The tag index of the XML item to expand (XPath must be NULL, as TagIndex is ignored if XPath is set).  If XPath is NULL and TagIndex is &lt; 0, the currently selected tag will be expanded.
int Parents: Set to TRUE if parent branches should be expanded.

-ERRORS-
Okay: The tag was found and expanded (if already expanded, ERR_Okay is still returned).
Args
NullArgs
Search
-END-

*****************************************************************************/

static ERROR VIEW_OpenBranch(objView *Self, struct viewOpenBranch *Args)
{
   if (!Args) return ERR_NullArgs;

   LogBranch("Path: %s, Index: %d, TagCount: %d", Args->XPath, Args->TagIndex, Self->XML->TagCount);

   struct XMLTag *tag = NULL;
   if ((Args->XPath) AND (Args->XPath[0])) {
      LONG i;
      if (xmlFindTag(Self->XML, Args->XPath, 0, &i)) {
         LogBack();
         return PostError(ERR_Search);
      }
      tag = Self->XML->Tags[i];
   }
   else if ((Args->TagIndex >= 0) AND (Args->TagIndex < Self->XML->TagCount)) {
      tag = Self->XML->Tags[Args->TagIndex];
   }
   else {
      // Find the most recently selected tag
      for (tag=Self->XML->Tags[0]; (tag) AND (tag->Index != Self->SelectedTag); tag=tag->Next);
   }

   if (!tag) {
      LogBack();
      return PostError(ERR_Search);
   }

   if (open_branch_callback(Self, tag) IS FALSE) {
      struct view_node *node = tag->Private;
      if (node->Flags & NODE_CHILDREN) {
         if (!(node->Flags & NODE_OPEN)) {
            node->Flags |= NODE_OPEN;
            Self->Deselect = FALSE;

            // Expand parent nodes if requested to do so

            if (Args->Parents) {
               MSG("Expanding parent branches.");

               LONG i = tag->Index;
               while (i >= 0) {
                  if (Self->XML->Tags[i]->Branch < tag->Branch) {
                     MSG("Find parent @ index %d, name: %s", i, Self->XML->Tags[i]->Attrib->Name);
                     tag = Self->XML->Tags[i];
                     node = tag->Private;
                     if (node->Flags & NODE_CHILDREN) {
                        node->Flags |= NODE_OPEN;
                     }
                  }
                  i--;
               }
            }

            arrange_items(Self);

            if (!Self->RedrawDue) {
               Self->RedrawDue = TRUE;
               DelayMsg(AC_Draw, Self->Layout->SurfaceID, NULL);
            }
         }
      }
      else MSG("There are no children for this branch.");
   }
   else MSG("Callback routine manually expanded the tree branch.");

   LogBack();
   return ERR_Okay;
}

//****************************************************************************

static ERROR VIEW_Free(objView *Self, APTR Void)
{
   if (Self->XML) { // Unload all icons from the cache first
      LONG index;
      for (index=0; index < Self->XML->TagCount; index++) {
         struct view_node *node = Self->XML->Tags[index]->Private;
         unload_icon(Self, &node->IconKey);
         unload_icon(Self, &node->IconOpenKey);
      }
   }

   OBJECTPTR object;
   if ((Self->FocusID) AND (!AccessObject(Self->FocusID, 3000, &object))) {

      ReleaseObject(object);
   }

   // Free column allocations

   struct view_col *col = Self->Columns;
   while (col) {
      struct view_col *next = col->Next;
      FreeMemory(col);
      col = next;
   }

   if (Self->prvKeyEvent)    { UnsubscribeEvent(Self->prvKeyEvent); Self->prvKeyEvent = NULL; }
   if (Self->Layout)         { acFree(Self->Layout); Self->Layout = NULL; }
   if (Self->GroupSurfaceID) { acFreeID(Self->GroupSurfaceID); Self->GroupSurfaceID = 0; }
   if (Self->SelectedTags)   { FreeMemory(Self->SelectedTags); Self->SelectedTags = NULL; }
   if (Self->DragItems)      { FreeMemory(Self->DragItems); Self->DragItems = NULL; }
   if (Self->Shadow)         { acFree(Self->Shadow); Self->Shadow = NULL; }
   if (Self->ItemNames)      { FreeMemory(Self->ItemNames); Self->ItemNames = NULL; }
   if (Self->TextAttrib)     { FreeMemory(Self->TextAttrib); Self->TextAttrib = NULL; }
   if (Self->NodeStrings)    { FreeMemory(Self->NodeStrings); Self->NodeStrings = NULL; }
   if (Self->XML)            { acFree(Self->XML); Self->XML = NULL; }
   if (Self->ExpandBitmap)   { acFree(Self->ExpandBitmap); Self->ExpandBitmap = NULL; }
   if (Self->CollapseBitmap) { acFree(Self->CollapseBitmap); Self->CollapseBitmap = NULL; }
   if (Self->GroupBitmap)    { acFree(Self->GroupBitmap); Self->GroupBitmap = NULL; }
   if (Self->SelectBitmap)   { acFree(Self->SelectBitmap); Self->SelectBitmap = NULL; }
   if (Self->GroupFont)      { acFree(Self->GroupFont); Self->GroupFont = NULL; }
   if (Self->Font)           { acFree(Self->Font); Self->Font = NULL; }
   if (Self->ColumnString)   { FreeMemory(Self->ColumnString); Self->ColumnString = NULL; }
   if (Self->GroupFace)      { FreeMemory(Self->GroupFace); Self->GroupFace = NULL; }
   if (Self->GroupHeaderXML) { FreeMemory(Self->GroupHeaderXML); Self->GroupHeaderXML = NULL; }
   if (Self->GroupSelectXML) { FreeMemory(Self->GroupSelectXML); Self->GroupSelectXML = NULL; }
   if (Self->BkgdXML)        { FreeMemory(Self->BkgdXML); Self->BkgdXML = NULL; }
   if (Self->DragSurface)    { acFreeID(Self->DragSurface); Self->DragSurface = 0; }
   if (Self->HScrollbar)     { acFree(Self->HScrollbar); Self->HScrollbar = NULL; }
   if (Self->VScrollbar)     { acFree(Self->VScrollbar); Self->VScrollbar = NULL; }

   gfxUnsubscribeInput(0);

   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
GetVar: Special field types are available via variable field support.

The following variables are supported:

`Selection(Index, Attrib)` Returns the content of the selection at the requested Index (the Index value may
not exceed the value in the TotalSelected field).  The Attrib parameter is optional.  It should be specified if there
is a certain XML attribute that needs to be read instead of the default item content.

`Active(Attrib)` Returns the content of the most recently active item.  The most recently active item is
defined as the one that the user has most recently clicked on or activated.  The item does not necessarily have to be
selected.

For direct item retrieval, searching or other complex lookups, the #XML object supports XPath lookups
through the GetVar action.

-END-

*****************************************************************************/

static ERROR VIEW_GetVar(objView *Self, struct acGetVar *Args)
{
   struct XMLTag *tag;
   struct view_node *node;
   char attrib[60];

   if (!Args) return PostError(ERR_NullArgs);

   if ((!Args->Field) OR (!Args->Buffer) OR (Args->Size < 1)) {
      return PostError(ERR_Args);
   }

   StrCopy(Self->VarDefault, Args->Buffer, Args->Size);

   LONG i, j;
   if (!StrCompare("active", Args->Field, 0, 0)) {
      if (Self->ActiveTag IS -1) return ERR_NoData;

      if (Self->ActiveTag >= Self->XML->TagCount) return ERR_NoData;
      if (!(tag = Self->XML->Tags[Self->ActiveTag])) return ERR_NoData;

      if (Args->Field[6] IS '(') {
         for (j=0,i=7; (j < sizeof(attrib)-1) AND (Args->Field[i]) AND (Args->Field[i] != ')'); j++) attrib[j] = Args->Field[i++];
         attrib[j] = 0;
      }
      else attrib[0] = 0;
   }
   else if (!StrCompare("selection(", Args->Field, 0, 0)) {
      LONG index = StrToInt(Args->Field) + 1;
      if (index < 1) return ERR_Okay;

      for (i=0; Args->Field[i] AND (Args->Field[i] != ','); i++);
      if (Args->Field[i] IS ',') {
         i++;
         for (j=0; (j < sizeof(attrib)-1) AND (Args->Field[i]) AND (Args->Field[i] != ')'); j++) attrib[j] = Args->Field[i++];
         attrib[j] = 0;
      }
      else attrib[0] = 0;

      // Find the requested item

      tag = NULL;
      for (i=0; Self->XML->Tags[i]; i++) {
         node = Self->XML->Tags[i]->Private;
         if (node->Flags & NODE_SELECTED) {
            if (--index < 1) {
               tag = Self->XML->Tags[i];
               break;
            }
         }
      }
   }
   else {
      LogErrorMsg("Field %s not supported.", Args->Field);
      return ERR_NoSupport;
   }

   if (!tag) return ERR_Okay; // Return the default variable value

   // Copy the item to the field buffer

   Args->Buffer[0] = 0;
   if (attrib[0]) {
      for (i=0; i < tag->TotalAttrib; i++) {
          if (!StrMatch(tag->Attrib[i].Name, attrib)) {
             StrCopy(tag->Attrib[i].Value, Args->Buffer, Args->Size);
             break;
          }
      }

      // If we didn't find anything, scan child tags (column names)

      if (i >= tag->TotalAttrib) {
         if ((tag = tag->Child)) {
            while (tag) {
               if (!StrMatch(tag->Attrib->Name, attrib)) {
                  xmlGetContent(Self->XML, tag->Index, Args->Buffer, Args->Size);
                  break;
               }
               tag = tag->Next;
            }
         }

         if (!tag) return ERR_Failed; // Requested attrib does not exist for this item
      }
   }
   else {
      node = tag->Private;
      StrCopy(get_nodestring(Self, node), Args->Buffer, Args->Size);
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Hide: Hides the view from the display.
-END-
*****************************************************************************/

static ERROR VIEW_Hide(objView *Self, APTR Void)
{
   return acHide(Self->Layout);
}

//****************************************************************************

static ERROR VIEW_Init(objView *Self, APTR Void)
{
   SetFunctionPtr(Self->Layout, FID_DrawCallback, &draw_view);
   SetFunctionPtr(Self->Layout, FID_ResizeCallback, &resize_view);
   if (acInit(Self->Layout) != ERR_Okay) {
      return ERR_Init;
   }

   if (!Self->FocusID) Self->FocusID = Self->Layout->SurfaceID;

   // If multi-select and drag-drop are both specified, the user's preference for drag and drop or multi-select is applied.

   if (Self->Flags & VWF_USER_DRAG) {
      if (glPreferDragDrop) Self->Flags |= VWF_DRAG_DROP;
      else Self->Flags &= ~VWF_DRAG_DROP;
   }

   if (acInit(Self->XML) != ERR_Okay) return ERR_Init;

   Self->Font->Flags |= FTF_CHAR_CLIP;
   Self->Font->WrapEdge = 8192;
   if (acInit(Self->Font) != ERR_Okay) return ERR_Init;

   if (Self->IconSize < 16) Self->IconSize = 16;

   objSurface *surface;
   if (!AccessObject(Self->Layout->SurfaceID, 5000, &surface)) {
      surface->Flags |= RNF_GRAB_FOCUS;

      if (Self->BkgdXML) { // Check for custom background graphics
          OBJECTPTR script;
          if (!CreateObject(ID_SCRIPT, NF_INTEGRAL, &script,
                FID_String|TSTR,  Self->BkgdXML,
                FID_Target|TLONG, Self->Layout->SurfaceID,
                TAGEND)) {

             if (!acActivate(script)) {
                Self->ColBackground.Alpha = 0;
             }
             acFree(script);
          }
          FreeMemory(Self->BkgdXML);
          Self->BkgdXML= NULL;
      }

      SubscribeActionTags(surface,
         AC_Disable,
         AC_DragDrop,
         AC_Enable,
         TAGEND);

      gfxSubscribeInput(Self->Layout->SurfaceID, JTYPE_MOVEMENT|JTYPE_BUTTON, 0);

      ReleaseObject(surface);
   }
   else return PostError(ERR_AccessObject);

   // Scan for scrollbars

   if ((!Self->VScroll) OR (!Self->HScroll)) {
      struct ChildEntry list[16];
      LONG count = ARRAYSIZE(list);

      if (!ListChildren(Self->Layout->SurfaceID, list, &count)) {
         for (--count; count >= 0; count--) {
            if (list[count].ClassID IS ID_SCROLLBAR) {
               objScrollbar *bar;
               if ((bar = (objScrollbar *)GetObjectPtr(list[count].ObjectID))) {
                  if ((bar->Direction IS SO_HORIZONTAL) AND (!Self->HScroll)) {
                     SET_HScroll(Self, (OBJECTPTR)bar->Scroll);
                     Self->HScrollbar = bar;
                  }
                  else if ((bar->Direction IS SO_VERTICAL) AND (!Self->VScroll)) {
                     SET_VScroll(Self, (OBJECTPTR)bar->Scroll);
                     Self->VScrollbar = bar;
                  }
               }
            }
         }
      }
   }

   if (!Self->VScroll) {
      if (!CreateObject(ID_SCROLLBAR, NF_INTEGRAL, &Self->VScrollbar,
            FID_Name|TSTR,     "sbv",
            FID_Surface|TLONG, Self->Layout->SurfaceID,
            FID_XOffset|TLONG, 0,
            FID_Y|TLONG,       0,
            FID_YOffset|TLONG, 0,
            FID_Direction|TSTR, "vertical",
            TAGEND)) {
         SET_VScroll(Self, (OBJECTPTR)Self->VScrollbar->Scroll);
         acShow(Self->VScroll);
      }
   }

   if (!Self->HScroll) {
      if (!CreateObject(ID_SCROLLBAR, NF_INTEGRAL, &Self->HScrollbar,
            FID_Name|TSTR,       "sbh",
            FID_Surface|TLONG,   Self->Layout->SurfaceID,
            FID_Intersect|TLONG, (Self->VScrollbar) ? Self->VScrollbar->Head.UniqueID : 0,
            FID_X|TLONG,         0,
            FID_XOffset|TLONG,   0,
            FID_YOffset|TLONG,   0,
            FID_Direction|TSTR,  "horizontal",
            TAGEND)) {
         SET_HScroll(Self, (OBJECTPTR)Self->HScrollbar->Scroll);
         acShow(Self->HScroll);
      }
   }

   if (Self->HScrollbar) {
      LONG value;
      GetLong(Self->HScrollbar, FID_Height, &value);
      Self->HBarHeight = value;
      if (!AccessObject(Self->HScrollbar->RegionID, 3000, &surface)) {
         SubscribeActionTags(surface, AC_Hide, AC_Show, TAGEND);
         if (surface->Flags & RNF_VISIBLE) Self->HBarVisible = TRUE;
         ReleaseObject(surface);
      }
   }

   MSG("Focus notification based on object #%d.", Self->FocusID);

   if (!AccessObject(Self->FocusID, 5000, &surface)) {
      if (surface->Head.ClassID IS ID_SURFACE) {
         SubscribeActionTags(surface, AC_Focus, AC_LostFocus, TAGEND);
         if (surface->Flags & RNF_HAS_FOCUS) {
            FUNCTION callback;
            SET_FUNCTION_STDC(callback, &key_event);
            SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, &callback, Self, &Self->prvKeyEvent);
         }
      }
      ReleaseObject(surface);
   }

   // Prepare the XML object in case it has been loaded with information prior to initialisation (the developer can
   // achieve this by setting the XML Location field).

   prepare_xml(Self, Self->XML->Tags[0], 0, 0);

   arrange_items(Self);

   if (Self->GroupHeaderXML) {
      gen_group_bkgd(Self, Self->GroupHeaderXML, &Self->GroupBitmap, "init");
   }

   if (Self->GroupSelectXML) {
      gen_group_bkgd(Self, Self->GroupSelectXML, &Self->SelectBitmap, "init");
   }

   if (Self->SelectionIndex != -1) {
      LogMsg("Selecting pre-selected item %d", Self->SelectionIndex);
      SET_SelectionIndex(Self, Self->SelectionIndex);
      Self->SelectionIndex = -1;
   }

   return ERR_Okay;
}

//****************************************************************************

static void gen_group_bkgd(objView *Self, CSTRING Script, objBitmap **Bitmap, CSTRING Caller)
{
   if (Self->Style != VIEW_GROUP_TREE) return;

   LONG width  = Self->Layout->BoundWidth;
   if (Self->PageWidth > width) width = Self->PageWidth;
   if (!width) width = 100;

   LONG height;
   if ((height = Self->GroupHeight) < 1) {
      if ((height = Self->LineHeight) < 1) {
         LogF("gen_group_bkgd()","Warning: GroupHeight or LineHeight not preset.");
         height = Self->IconSize + 6;
      }
   }

   LogF("~gen_group_bkgd()","Generating group background %dx%d, Caller: %s", width, height, Caller);

   if (Self->GroupSurfaceID) {
      objSurface *surface;

      if (!AccessObject(Self->GroupSurfaceID, 3000, &surface)) {
         acResize(surface, width, height, 0);

         if (!Bitmap[0]) {
            CreateObject(ID_BITMAP, NF_INTEGRAL, Bitmap,
               FID_Width|TLONG,  width,
               FID_Height|TLONG, height,
               TAGEND);
         }
         else acResize(*Bitmap, width, height, 0);

         if (*Bitmap) drwCopySurface(Self->GroupSurfaceID, *Bitmap, BDF_REDRAW, 0, 0, width, height, 0, 0);

         ReleaseObject(surface);
      }
   }
   else {
      objSurface *surface;
      ERROR error;
      if (!(error = NewLockedObject(ID_SURFACE, NF_INTEGRAL, &surface, &Self->GroupSurfaceID))) {
         SetFields(surface,
            FID_Parent|TLONG, 0,
            FID_Width|TLONG,  width,
            FID_Height|TLONG, height,
            FID_X|TLONG,      -10000,
            FID_Y|TLONG,      -10000,
            TAGEND);

         if (!acInit(surface)) {
            OBJECTPTR script;
            if (!CreateObject(ID_SCRIPT, 0, &script,
                  FID_Owner|TLONG,    Self->GroupSurfaceID,
                  FID_String|TSTRING, Script,
                  FID_Target|TLONG,   Self->GroupSurfaceID,
                  TAGEND)) {

               if (!acActivate(script)) {
                  objBitmap *bmp = NULL;
                  if (!CreateObject(ID_BITMAP, NF_INTEGRAL, &bmp,
                        FID_Width|TLONG,  width,
                        FID_Height|TLONG, height,
                        TAGEND)) {
                     drwCopySurface(Self->GroupSurfaceID, bmp, BDF_REDRAW, 0, 0, width, height, 0, 0);
                     if ((bmp) AND (*Bitmap)) acFree(*Bitmap);
                     *Bitmap = bmp;
                  }
                  else error = ERR_CreateObject;
               }
               else error = ERR_Activate;

               acFree(script);
            }
            else error = ERR_CreateObject;
         }
         else error = ERR_Init;

         if (error) { acFree(surface); Self->GroupSurfaceID = 0; }

         ReleaseObject(surface);
      }
   }

   LogBack();
}

/*****************************************************************************

-METHOD-
InsertItem: Inserts new items into the view's XML tree structure.

The InsertItem method parses a new XML string and inserts the data at a specific point in the existing XML tree.  The
tags can be inserted at a position indicated by an XPath, or through a TagIndex.

If XPath is NULL and the TagIndex is -1, the insertion point is the user's currently selected tag (if there is one).
If the TagIndex is -2, the insertion point is the end of the tag list.

The view will be redrawn as a result of calling this method.

-INPUT-
cstr XPath: An XML path to the item that will be targeted as the insertion point.  Set to NULL to use a TagIndex instead of an XPath.
int TagIndex: The tag index of the XML item that will be targeted as the insertion point (XPath must be NULL or this parameter is ignored).
int Insert: The insertion mode - one of XMI_CHILD, XMI_PREV or XMI_NEXT.
cstr XML: The XML string to be inserted.

-ERRORS-
Okay
NullArgs
Search: The XPath did not match a valid tag.
OutOfRange: The TagIndex is invalid.
InvalidReference: The XPath was matched to a tag that was not a valid item.
-END-

*****************************************************************************/

static ERROR VIEW_InsertItem(objView *Self, struct viewInsertItem *Args)
{
   if ((!Args) OR (!Args->XML)) return PostError(ERR_NullArgs);

   if (Self->XML->TagCount <= 0) {
      // There is no data in the view's XML

      if ((Args->XPath) AND (Args->XPath[0])) return ERR_Search;
      if (Args->TagIndex > 0) return ERR_OutOfRange;

      ERROR error;
      if (!(error = xmlInsertXML(Self->XML, 0, XMI_NEXT, Args->XML, NULL))) {
         prepare_xml(Self, Self->XML->Tags[0], 0, 0);

         arrange_items(Self);

         if (!Self->RedrawDue) {
            Self->RedrawDue = TRUE;
            DelayMsg(AC_Draw, Self->Layout->SurfaceID, NULL);
         }

         return ERR_Okay;
      }
      else return error;
   }
   else {
      struct XMLTag *tag = NULL;
      if ((Args->XPath) AND (Args->XPath != (STRING)-1) AND (Args->XPath[0])) {
         FMSG("~","Path: %s, Insert Mode: %d", Args->XPath, Args->Insert);
         LONG tagindex;
         if (xmlFindTag(Self->XML, Args->XPath, 0, &tagindex)) {
            STEP();
            return PostError(ERR_Search);
         }
         tag = Self->XML->Tags[tagindex];
      }
      else if ((Args->TagIndex >= 0) AND (Args->TagIndex < Self->XML->TagCount)) {
         FMSG("~","TagIndex: %d, Insert Mode: %d", Args->TagIndex, Args->Insert);
         tag = Self->XML->Tags[Args->TagIndex];
      }
      else if (Args->TagIndex IS -1) {
         FMSG("~","SelectedTag: %d, Insert Point: %d", Self->SelectedTag, Args->Insert);
         // Insertion point is the currently selected tag
         for (tag=Self->XML->Tags[0]; (tag) AND (tag->Index != Self->SelectedTag); tag=tag->Next);
      }
      else if (Args->TagIndex IS -2) {
         LONG i;
         FMSG("~","End: %d, Insert Point: %d", Self->XML->TagCount-1, Args->Insert);
         for (i=Self->XML->TagCount-1; i >= 0; i--) {
            tag = Self->XML->Tags[i];
            if (((struct view_node *)(tag->Private))->Flags & NODE_ITEM) break;
         }
      }
      else return ERR_Search;

      if (!tag) {
         LogErrorMsg("Failed to find '%s' / %d from %d tags.", Args->XPath, Args->TagIndex, Self->XML->TagCount);
         STEP();
         return ERR_Search;
      }

      if (!(((struct view_node *)(tag->Private))->Flags & NODE_ITEM)) {
         STEP();
         return PostError(ERR_InvalidReference);
      }

      ERROR error;
      if (!(error = xmlInsertXML(Self->XML, tag->Index, Args->Insert, Args->XML, NULL))) {

         //prepare_xml(Self, newtag, NULL, 1); // Note: You also need to re-prepare the parent tag.
         prepare_xml(Self, Self->XML->Tags[0], NULL, 0);

         // Items need to be rearranged (to calculate the new page size etc).

         arrange_items(Self);

         if (!Self->RedrawDue) {
            Self->RedrawDue = TRUE;
            DelayMsg(AC_Draw, Self->Layout->SurfaceID, NULL);
         }

         STEP();
         return ERR_Okay;
      }
      else {
         STEP();
         return error;
      }
   }
}

/*****************************************************************************

-METHOD-
InvertSelection: Inverts all currently selected items in the view.

This method will invert the selections in the view, so that selected items become deselected and all other items are
selected.  The view will be redrawn as a result of calling this method.

-ERRORS-
Okay
Failed: Invert is not possible (e.g. if the SENSITIVE flag is set in the view).
-END-

*****************************************************************************/

static ERROR VIEW_InvertSelection(objView *Self, APTR Void)
{
   if (Self->Flags & VWF_SENSITIVE) return ERR_Failed;

   struct view_node *node;
   LONG index;
   for (index=0; Self->XML->Tags[index]; index++) {
      node = Self->XML->Tags[index]->Private;
      if (node->Flags & NODE_ITEM) node->Flags ^= NODE_SELECTED;
   }

   Self->ActiveTag = -1;
   Self->HighlightTag = -1;
   report_selection(Self, SLF_ACTIVE|SLF_INVERTED, -1);

   // Select the first tag in the list

   for (index=0; Self->XML->Tags[index]; index++) {
      node = Self->XML->Tags[index]->Private;
      if (node->Flags & NODE_ITEM) {
         if (node->Flags & NODE_SELECTED) {
            Self->SelectedTag = index;
            report_selection(Self, SLF_SELECTED|SLF_INVERTED, index);
            break;
         }
      }
   }

   if (!Self->XML->Tags[index]) {
      Self->SelectedTag = -1;
      report_selection(Self, SLF_SELECTED|SLF_INVERTED, -1);
   }

   acDrawID(Self->Layout->SurfaceID);

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
ItemDimensions: Returns the graphical dimensions of an item in the view.

Retrieves the dimensions of an item that is in the view.  The coordinates are relative to the parent surface.

-INPUT-
int TagIndex: The index of the XML tag that will be analysed.
&int X: The X coordinate of the item is returned here.
&int Y: The Y coordinate of the item is returned here.
&int Width: The width of the item is returned here.
&int Height: The height of the item is returned here.

-ERRORS-
Okay
NullArgs
-END-

*****************************************************************************/

static ERROR VIEW_ItemDimensions(objView *Self, struct viewItemDimensions *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   struct XMLTag *tag = NULL;
   if (Args->TagIndex IS -1) {
      // Target the currently selected tag
      if ((Self->SelectedTag >= 0) AND (Self->SelectedTag < Self->XML->TagCount)) {
         tag = Self->XML->Tags[Self->SelectedTag];
      }
      else return ERR_Okay; // No tag is selected
   }
   else if (Args->TagIndex IS -2) {
      LONG i;
      // Target the last item in the list
      for (i=Self->XML->TagCount-1; i >= 0; i--) {
         tag = Self->XML->Tags[i];
         if (((struct view_node *)(tag->Private))->Flags & NODE_ITEM) break;
      }
   }
   else {
      if ((Args->TagIndex < 0) OR (Args->TagIndex >= Self->XML->TagCount)) return PostError(ERR_OutOfRange);
      tag = Self->XML->Tags[Args->TagIndex];
   }

   if (!tag) return PostError(ERR_SystemCorrupt); // A NULL tag indicates corrupt data

   struct view_node *node;
   if ((node = tag->Private)) {
      Args->X = node->X + Self->XPos;
      Args->Y = node->Y + Self->YPos;
      Args->Width  = node->Width;
      Args->Height = node->Height;

      if (Self->Style IS VIEW_LONG_LIST) {
         //Args->X = 0;
         //Args->Width = Self->
      }

      return ERR_Okay;
   }
   else return ERR_Failed;
}

/*****************************************************************************

-METHOD-
LowerItem: Moves an item towards the bottom of the view.

Moves an item down past the next item in the list.  The view will be redrawn using a delayed redraw.

-INPUT-
cstr XPath: An XPath that targets the item to be lowered.  Can be NULL if TagIndex is defined.
int TagIndex: The tag index of the XML item that needs to be lowered (XPath must be NULL or this parameter is ignored).  If XPath is NULL and TagIndex is -1, the currently selected tag will be targeted.

-ERRORS-
Okay
Search
-END-

*****************************************************************************/

static ERROR VIEW_LowerItem(objView *Self, struct viewLowerItem *Args)
{
   struct XMLTag *tag = NULL;
   if ((Args) AND (Args->XPath) AND (Args->XPath[0])) {
      LONG tagindex;
      if (xmlFindTag(Self->XML, Args->XPath, NULL, &tagindex)) {
         return PostError(ERR_Search);
      }
      tag = Self->XML->Tags[tagindex];
   }
   else if ((Args) AND (Args->TagIndex >= 0) AND (Args->TagIndex < Self->XML->TagCount)) {
      tag = Self->XML->Tags[Args->TagIndex];
   }
   else {
      for (tag=Self->XML->Tags[0]; (tag) AND (tag->Index != Self->SelectedTag); tag=tag->Next);
   }

   if (!tag) return PostError(ERR_Search);

   // Move the tag down

   if (tag->Next) {
      LONG tagindex = tag->Index;
      LONG newindex = tag->Next->Index;

      xmlMoveTags(Self->XML, tagindex, 1, newindex, -1);

      if (Self->HighlightTag IS tagindex) Self->HighlightTag = newindex;
      if (Self->ActiveTag IS tagindex)    { Self->ActiveTag = newindex;  report_selection(Self, SLF_ACTIVE|SLF_MOVED, newindex); }
      if (Self->SelectedTag IS tagindex)  { Self->SelectedTag = newindex;  report_selection(Self, SLF_SELECTED|SLF_MOVED, newindex); }

      arrange_items(Self);

      if (!Self->RedrawDue) {
         Self->RedrawDue = TRUE;
         DelayMsg(AC_Draw, Self->Layout->SurfaceID, NULL);
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR VIEW_NewObject(objView *Self, APTR Void)
{
   LONG i;

   if (!NewObject(ID_LAYOUT, NF_INTEGRAL, &Self->Layout)) {

   }
   else return ERR_NewObject;

   if (!NewObject(ID_XML, NF_INTEGRAL, &Self->XML)) {
      SetFields(Self->XML, FID_PrivateDataSize|TLONG, sizeof(struct view_node),
                           FID_Flags|TLONG,           XMF_STRIP_HEADERS,
                           TAGEND);

      if (!NewObject(ID_FONT, NF_INTEGRAL, &Self->Font)) {
         SetName(Self->Font, "ViewFont");
         SetString(Self->Font, FID_Face, glDefaultFace);

         if (!AllocMemory(sizeof(struct view_col), MEM_DATA, &Self->Columns, NULL)) {
            StrCopy("Default", Self->Columns->Name, sizeof(Self->Columns->Name));
            StrCopy("Default", Self->Columns->Text, sizeof(Self->Columns->Text));
            Self->ItemNames = StrClone("item");
            Self->Columns->Type  = CT_VARIANT;
            Self->Columns->Width = 160;
            Self->Columns->Sort  = SORT_UNSORTED;

            StrCopy(glDateFormat, Self->DateFormat, sizeof(Self->DateFormat));

            for (i=1; i < ARRAYSIZE(Self->Sort); i++) Self->Sort[i] = 0;

            Self->VarDefault[0] = '-';
            Self->VarDefault[1] = '1';
            Self->VarDefault[2] = 0;

            Self->Style           = VIEW_COLUMN;
            Self->MaxItemWidth    = 170;
            Self->HSpacing        = 10;
            Self->VSpacing        = 2;
            Self->HighlightTag    = -1;
            Self->SelectionIndex  = -1;
            Self->ActiveTag       = -1;
            Self->SelectedTag     = -1;
            Self->ButtonThickness = 2;
            Self->IconSize        = 16;
            Self->Layout->TopMargin    = 4;
            Self->Layout->BottomMargin = 4;
            Self->Layout->RightMargin  = 4;
            Self->Layout->LeftMargin   = 4;

            Self->ColHairline.Red    = 200;
            Self->ColHairline.Green  = 200;
            Self->ColHairline.Blue   = 200;
            Self->ColHairline.Alpha  = 255;

            Self->ColHighlight.Red   = 255;
            Self->ColHighlight.Green = 0;
            Self->ColHighlight.Blue  = 0;
            Self->ColHighlight.Alpha = 255;

            Self->ColSelect.Red   = 230; //220,220,245
            Self->ColSelect.Green = 230;
            Self->ColSelect.Blue  = 255;
            Self->ColSelect.Alpha = 255;

            Self->ColItem.Red   = 0;
            Self->ColItem.Green = 0;
            Self->ColItem.Blue  = 0;
            Self->ColItem.Alpha = 255;

            Self->ColAltBackground.Red   = 220;
            Self->ColAltBackground.Green = 220;
            Self->ColAltBackground.Blue  = 220;
            Self->ColAltBackground.Alpha = 0;

            Self->ColBackground.Red   = 90;
            Self->ColBackground.Green = 90;
            Self->ColBackground.Blue  = 90;
            Self->ColBackground.Alpha = 0;

            Self->ColTitleFont.Red    = 0;
            Self->ColTitleFont.Green  = 0;
            Self->ColTitleFont.Blue   = 0;
            Self->ColTitleFont.Alpha  = 255;

            Self->ColButtonFont.Red   = 0;
            Self->ColButtonFont.Green = 0;
            Self->ColButtonFont.Blue  = 0;
            Self->ColButtonFont.Alpha = 255;

            Self->ColBranch.Red   = 190;
            Self->ColBranch.Green = 190;
            Self->ColBranch.Blue  = 190;
            Self->ColBranch.Alpha = 255;

            Self->ButtonBackground.Red   = 210;
            Self->ButtonBackground.Green = 210;
            Self->ButtonBackground.Blue  = 210;
            Self->ButtonBackground.Alpha = 255;

            Self->ButtonHighlight.Red   = 255;
            Self->ButtonHighlight.Green = 255;
            Self->ButtonHighlight.Blue  = 255;
            Self->ButtonHighlight.Alpha = 255;

            Self->ButtonShadow.Red   = 0;
            Self->ButtonShadow.Green = 0;
            Self->ButtonShadow.Blue  = 0;
            Self->ButtonShadow.Alpha = 255;

            drwApplyStyleValues(Self, NULL);

            return ERR_Okay;
         }
         else return ERR_AllocMemory;
      }
      else return ERR_NewObject;
   }
   else return ERR_NewObject;
}

/*****************************************************************************

-METHOD-
RaiseItem: Moves an item towards the top of the view.

Moves an item up past the previous item in the list.  The view will be redrawn using a delayed redraw.

-INPUT-
cstr XPath: An XPath that targets the item to be raised.  Can be NULL if TagIndex is defined.
int TagIndex: The tag index of the XML item that needs to be raised (XPath must be NULL or this parameter is ignored).  If XPath is NULL and TagIndex is -1, the currently selected tag will be targeted.

-ERRORS-
Okay
Search
-END-

*****************************************************************************/

static ERROR VIEW_RaiseItem(objView *Self, struct viewRaiseItem *Args)
{
   LogBranch(NULL);

   struct XMLTag *tag = NULL;
   if ((Args) AND (Args->XPath) AND (Args->XPath != (STRING)-1) AND (Args->XPath[0])) {
      LONG tagindex;
      if (xmlFindTag(Self->XML, Args->XPath, 0, &tagindex)) {
         LogBack();
         return PostError(ERR_Search);
      }
   }
   else if ((Args) AND (Args->TagIndex >= 0) AND (Args->TagIndex < Self->XML->TagCount)) {
      tag = Self->XML->Tags[Args->TagIndex];
   }
   else for (tag=Self->XML->Tags[0]; (tag) AND (tag->Index != Self->SelectedTag); tag=tag->Next);

   if (!tag) {
      LogBack();
      return PostError(ERR_Search);
   }

   // Move the tag up

   if (tag->Prev) {
      LONG tagindex = tag->Index;
      LONG newindex = tag->Prev->Index;

      xmlMoveTags(Self->XML, tagindex, 1, newindex, -1);

      if (Self->HighlightTag IS tagindex) Self->HighlightTag = newindex;
      if (Self->ActiveTag IS tagindex)    { Self->ActiveTag = newindex; report_selection(Self, SLF_ACTIVE|SLF_MOVED, newindex); }
      if (Self->SelectedTag IS tagindex)  { Self->SelectedTag = newindex; report_selection(Self, SLF_SELECTED|SLF_MOVED, newindex); }

      arrange_items(Self);

      if (!Self->RedrawDue) {
         Self->RedrawDue = TRUE;
         DelayMsg(AC_Draw, Self->Layout->SurfaceID, NULL);
      }
   }

   LogBack();
   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
Refresh: Refreshes the display.

The Refresh action updates the view so that its display surface reflects the data in the #XML definition.  A
delayed draw message will be posted to the surface as a result of calling this action.

The Refresh action is typically called following manual changes to #XML content.
-END-

*****************************************************************************/

static ERROR VIEW_Refresh(objView *Self, APTR Void)
{
   LONG active   = Self->ActiveTag;
   LONG selected = Self->SelectedTag;
   Self->HighlightTag = -1;
   Self->ActiveTag    = -1;
   Self->SelectedTag  = -1;

   if (Self->NodeStrings) {
      FreeMemory(Self->NodeStrings);
      Self->NodeStrings = NULL;
      Self->NSIndex = 0;
      Self->NSSize = 0;
   }

   prepare_xml(Self, Self->XML->Tags[0], 0, 0); // Will reset ActiveTag and SelectedTag

   FMSG("~","Resetting selected and active tags.");

      LONG flags = 0;
      if (active != Self->ActiveTag) flags |= SLF_ACTIVE;
      if (selected != Self->SelectedTag) flags |= SLF_SELECTED;
      if (flags) report_selection(Self, flags, Self->SelectedTag);

   STEP();

   arrange_items(Self); // Expected to set HighlightTag

   MSG("Redrawing surface.");

   if (!Self->RedrawDue) {
      Self->RedrawDue = TRUE;
      DelayMsg(AC_Draw, Self->Layout->SurfaceID, NULL);
   }

   return ERR_Okay;
}

/******************************************************************************

-METHOD-
RemoveItem: Removes an item from the view.

Use the RemoveItem() method to remove an item from the view.  The view will be redrawn as a result of calling this
method.

-INPUT-
cstr XPath: An XML path that targets an item to be removed, or NULL if TagIndex is defined.
int TagIndex: The tag index of an XML item to remove (XPath must be NULL or this parameter is ignored).  If XPath is NULL and TagIndex is -1, the currently selected tag will be removed.

-ERRORS-
Okay
NullArgs
-END-

******************************************************************************/

static ERROR VIEW_RemoveItem(objView *Self, struct viewRemoveItem *Args)
{
   if (!Args) return ERR_NullArgs;

   LONG tagindex;
   struct XMLTag *tag = NULL;
   if ((Args->XPath) AND (Args->XPath != (STRING)-1) AND (Args->XPath[0])) {
      MSG("Path: %s", Args->XPath);
      if (xmlFindTag(Self->XML, Args->XPath, NULL, &tagindex)) return PostError(ERR_Search);
      tag = Self->XML->Tags[tagindex];
   }
   else if ((Args->TagIndex >= 0) AND (Args->TagIndex < Self->XML->TagCount)) {
      MSG("TagIndex: %d", Args->TagIndex);
      tag = Self->XML->Tags[Args->TagIndex];
   }
   else {
      MSG("SelectedTag: %d", Self->SelectedTag);
      for (tag=Self->XML->Tags[0]; (tag) AND (tag->Index != Self->SelectedTag); tag=tag->Next);
   }

   if (tag) {
      LONG activate;

      tagindex = tag->Index;

      // Check if the tag is currently selected.  If so, we'll need to send a reactivation message to children monitoring the view.

      struct view_node *node;
      if ((node = tag->Private)) {
         activate = node->Flags & NODE_SELECTED;
         unload_icon(Self, &node->IconKey);
         unload_icon(Self, &node->IconOpenKey);
      }
      else activate = FALSE;

      xmlRemoveTag(Self->XML, tag->Index, 1);

      // Fix selection indexes

      if (Self->HighlightTag IS tagindex) Self->HighlightTag = -1;
      if (Self->ActiveTag IS tagindex)   { Self->ActiveTag   = -1; report_selection(Self, SLF_ACTIVE, -1); }
      if (Self->SelectedTag IS tagindex) { Self->SelectedTag = -1; report_selection(Self, SLF_SELECTED, -1); }

      arrange_items(Self);
      acDrawID(Self->Layout->SurfaceID);

      // Send an Activate notification if necessary

      if ((activate) AND (Self->Flags & (VWF_SENSITIVE|VWF_NOTIFY_ON_CLEAR))) {
         LogMsg("Reactivating due to deleted selected item.");
         acActivate(Self);
      }

      return ERR_Okay;
   }
   else return ERR_Search;
}

/******************************************************************************

-METHOD-
RemoveTag: Removes an XML tag without updating the view (for optimisation only).

Use the RemoveTag() method to delete an XML tag without updating the view display.  This method is provided as a means
of optimising the deletion of multiple view items followed by a #Refresh().  It replaces the XML version of
this method, RemoveXMLTag(). By using this method, XML tags can be correctly deleted without having an adverse affect
on the view (e.g. if the tag is associated with an icon, the icon will be de-allocated from the system).

-INPUT-
int TagIndex: The tag index of the XML item that must be removed.
int Total: The total number of sibling items to remove.  The minimum value is 1.

-ERRORS-
Okay:
NullArgs:
-END-

******************************************************************************/

static ERROR VIEW_RemoveTag(objView *Self, struct viewRemoveTag *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   FMSG("~","Index: %d", Args->TagIndex);

   struct XMLTag *tag;
   if ((Args->TagIndex >= 0) AND (Args->TagIndex < Self->XML->TagCount)) {
      tag = Self->XML->Tags[Args->TagIndex];
   }
   else { STEP(); return PostError(ERR_OutOfRange); }

   LONG tagindex = tag->Index;

   // Remove the tag and associated resources from the system

   struct view_node *node;
   if ((node = tag->Private)) {
      unload_icon(Self, &node->IconKey);
      unload_icon(Self, &node->IconOpenKey);
   }

   LONG total = Args->Total;
   if (total < 1) total = 1;

   xmlRemoveTag(Self->XML, tagindex, total);

   // Fix selection indexes

   if (Self->HighlightTag IS tagindex) Self->HighlightTag = -1;
   if (Self->ActiveTag IS tagindex)    { Self->ActiveTag   = -1; report_selection(Self, SLF_ACTIVE, -1); }
   if (Self->SelectedTag IS tagindex)  { Self->SelectedTag = -1; report_selection(Self, SLF_SELECTED, -1); }

   // Note that this is a technical routine that does not run a cleanup sub-routine for item rearrangement and notifications.

   STEP();
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
ScrollToPoint: Scrolls the graphical content of a view.
-END-
*****************************************************************************/

static ERROR VIEW_ScrollToPoint(objView *Self, struct acScrollToPoint *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   if ((Args->X IS Self->XPos) AND (Args->Y IS Self->YPos)) return ERR_Okay;

   OBJECTPTR surface;
   if (!AccessObject(Self->Layout->SurfaceID, 5000, &surface)) {
      LONG x, y;
      if (Args->Flags & STP_X) x = -Args->X;
      else x = Self->XPos;

      if (Args->Flags & STP_Y) y = -Args->Y;
      else y = Self->YPos;

      Self->XPos = x;
      Self->YPos = y;

      LONG ax = Self->Layout->BoundX;
      LONG ay = Self->Layout->BoundY;
      LONG awidth = Self->Layout->BoundWidth;
      LONG aheight = Self->Layout->BoundHeight;

      if (Self->ColBorder.Alpha) {
         if (!(Self->GfxFlags & VGF_DRAW_TABLE)) {
            ax++;
            awidth -= 2;
            if ((Self->Style IS VIEW_COLUMN) OR (Self->Style IS VIEW_COLUMN_TREE)) {
               aheight--;
            }
            else {
               ay++;
               aheight -= 2;
            }
         }
      }

      acDrawArea(surface, ax, ay, awidth, aheight);

      ReleaseObject(surface);
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SelectAll: Selects all items in the view for the user.

This method will select all items in the view, as if the user had selected them himself.  The view will be redrawn as a
result of calling this method.

-ERRORS-
Okay
-END-

*****************************************************************************/

static ERROR VIEW_SelectAll(objView *Self, APTR Void)
{
   LONG index;
   for (index=0; Self->XML->Tags[index]; index++) {
      struct view_node *node = Self->XML->Tags[index]->Private;
      if (node->Flags & NODE_ITEM) node->Flags |= NODE_SELECTED;
   }

   Self->HighlightTag = -1;
   Self->ActiveTag = 0;
   Self->SelectedTag = 0;
   report_selection(Self, SLF_ACTIVE|SLF_SELECTED|SLF_MULTIPLE, 0);

   acDrawID(Self->Layout->SurfaceID);
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SelectItem: Manually select items for the user.

The SelectItem method is used to manually select items in the view, highlighting them for the user.  An XPath is used
to select the desired item (see the @XML class for more information).

In the event that multiple tags could match the given parameters, only the first tag will be selected by the selection
routine.  If there is a high probability that the data strings are repeated, you should implement a unique ID for each
tag and use that for the selection of specific tags.

Note that selections are inclusive with other selections when in MULTISELECT mode. Use the #SelectNone()
method first if you want the selection to be the only selected item in the list.

-INPUT-
cstr XPath: The XPath query that will be used to select the item.

-ERRORS-
Okay: The item was selected successfully.
Args
-END-

*****************************************************************************/

static ERROR VIEW_SelectItem(objView *Self, struct viewSelectItem *Args)
{
   LONG tagindex;

   if ((Args) AND (Args->XPath) AND (Args->XPath[0] IS '/')) {
      if (!xmlFindTag(Self->XML, Args->XPath, 0, &tagindex)) {
         select_item(Self, Self->XML->Tags[tagindex], SLF_MANUAL, TRUE, FALSE);
         return ERR_Okay;
      }
      else {
         LogErrorMsg("Unable to resolve xpath \"%s\"", Args->XPath);
         return ERR_Search;
      }
   }
   else return PostError(ERR_Args);
}

/*****************************************************************************

-METHOD-
SelectNone: Deselects all currently selected items.

This method will deselect all items in the view, as if the user had deselected them himself.  The view will be redrawn
as a result of calling this method.

-ERRORS-
Okay
-END-

*****************************************************************************/

static ERROR VIEW_SelectNone(objView *Self, APTR Void)
{
   LONG index;
   for (index=0; Self->XML->Tags[index]; index++) {
      struct view_node *node = Self->XML->Tags[index]->Private;
      if (node->Flags & NODE_ITEM) node->Flags &= ~NODE_SELECTED;
   }

   Self->ActiveTag    = -1;
   Self->HighlightTag = -1;
   Self->SelectedTag  = -1;
   report_selection(Self, SLF_ACTIVE|SLF_SELECTED, -1);

   acDrawID(Self->Layout->SurfaceID);

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SetItem: Changes the attributes of any item in the view.

Use the SetItem() method to change the attributes of items that are in the view.  This method is primarily for script
usage, as it can be faster to manipulate the XML object directly and use the Refresh action than to make multiple calls
to this method.

In order to call SetItem() successfully, you should have an awareness of the XML tag structure that you have used for
creating the view items. You will need to pass the ID of the item that you want to update (thus you must have used the
'id' parameter when constructing the original XML tags), the name of tag to change (NULL is acceptable for the default)
and the attribute to set (NULL is acceptable to change the XML content that represents the item data). The new string
to be set is defined in the Value parameter.

If you update the content of an item by passing a NULL Attrib value, then a completely redraw of the view will occur so
that the display accurately reflects the view data.

-INPUT-
cstr XPath:   An XML path to the item that is to be set.  Set this value to NULL if TagIndex is defined.
int TagIndex: The index of the XML tag that is to be set.  The XPath must be set to NULL.  Ignored if the XPath is set to any other value.
cstr Tag:     The name of the child tag that is to be set within the item.  Set to NULL for the default tag.
cstr Attrib:  The name of the attribute that is to be set in the XML tag.  May be NULL to set the tag's content.
cstr Value:   The value to set against the attribute.

-ERRORS-
Okay
Args
Search: The index, tag and/or attrib values did not lead to a match.
-END-

*****************************************************************************/

static ERROR VIEW_SetItem(objView *Self, struct viewSetItem *Args)
{
   FMSG("~","XPath: %s, Index: %d, Tag: %s, Attrib: %s, Value: %s", Args->XPath, Args->TagIndex, Args->Tag, Args->Attrib, Args->Value);

   // Find the root tag that we need to set

   LONG tagindex;
   struct XMLTag *tag = NULL;
   if ((Args) AND (Args->XPath) AND (Args->XPath != (STRING)-1) AND (Args->XPath[0])) {
      if (xmlFindTag(Self->XML, Args->XPath, 0, &tagindex)) {
         STEP();
         return PostError(ERR_Search);
      }
      tag = Self->XML->Tags[tagindex];
   }
   else if ((Args) AND (Args->TagIndex >= 0) AND (Args->TagIndex < Self->XML->TagCount)) {
      tag = Self->XML->Tags[Args->TagIndex];
   }
   else {
      STEP();
      return PostError(ERR_OutOfRange);
   }

   if (!tag) {
      LogErrorMsg("Failed to find the root tag for path/tag '%s' / %d", Args->XPath, Args->TagIndex);
      STEP();
      return ERR_Search;
   }

   if (!(((struct view_node *)(tag->Private))->Flags & NODE_ITEM)) {
      STEP();
      return PostError(ERR_InvalidReference);
   }

   // Scan for the correct tag within the discovered area

   if ((Args->Tag) AND (Args->Tag[0])) {
      if (StrMatch(Args->Tag, tag->Attrib->Name) != ERR_Okay) {
         if (tag->Child) {
            for (tag=tag->Child; tag; tag=tag->Next) {
               if (!StrMatch(Args->Tag, tag->Attrib->Name)) break;
            }
         }
         else LogErrorMsg("There are no children under tag '%s'.", tag->Attrib->Name);
      }

      if (!tag) {
         LogErrorMsg("Failed to find child tag '%s'", Args->Tag);
         STEP();
         return ERR_Search;
      }
   }

   if ((Args->Attrib) AND (Args->Attrib[0])) { // Update an attribute.  There is no need to preform a redraw in this case
      LONG tagindex = tag->Index;
      struct view_node *node = Self->XML->Tags[tagindex]->Private;

      LONG index;
      for (index=0; index < tag->TotalAttrib; index++) {
         if (!StrMatch(Args->Attrib, tag->Attrib[index].Name)) {
            if (!StrMatch(Args->Value, tag->Attrib[index].Value)) { // The new value is the same as the current value
               STEP();
               return ERR_Okay;
            }

            if (!StrMatch("icon", Args->Attrib)) {
               // An icon change has been made.  We need to delete the existing icon and load the one that has been specified.

               load_icon(Self, Args->Value, &node->Icon, &node->IconKey);
               draw_item(Self, tag);
            }
            else if (!StrMatch("iconopen", Args->Attrib)) {
               load_icon(Self, Args->Value, &node->IconOpen, &node->IconOpenKey);
               draw_item(Self, tag);
            }

            xmlSetAttrib(Self->XML, tagindex, index, 0, Args->Value);

            if ((Self->TextAttrib) AND (!StrMatch(Self->TextAttrib, Args->Attrib))) {
               set_nodestring(Self, Self->XML->Tags[tagindex]->Private, Args->Value);
               ((struct view_node *)Self->XML->Tags[tagindex]->Private)->ChildString = FALSE;
            }

            STEP();
            return ERR_Okay;
         }
      }
   }
   else { // Update content
      if (tag->Child) {
         LONG tagindex = tag->Index;
         xmlSetAttrib(Self->XML, tag->Child->Index, 0, 0, Args->Value);

         struct view_node *node = Self->XML->Tags[tagindex]->Private;
         if ((node->ChildString) OR (!Self->TextAttrib)) {
            set_nodestring(Self, node, Args->Value);
            node->ChildString = TRUE;
         }

         // Items need to be rearranged (due to issues like extending node widths, sorting etc).

         arrange_items(Self);

         if (!Self->RedrawDue) {
            Self->RedrawDue = TRUE;
            DelayMsg(AC_Draw, Self->Layout->SurfaceID, NULL);
         }
      }

      STEP();
      return ERR_Okay;
   }

   STEP();
   return PostError(ERR_Search);
}

/*****************************************************************************
-ACTION-
Show: Redisplays the view if it has been hidden.
-END-
*****************************************************************************/

static ERROR VIEW_Show(objView *Self, APTR Void)
{
   return acShow(Self->Layout);
}

/*****************************************************************************

-METHOD-
SortColumn: Sorts the view by column.

The SortColumn routine will sort the View on the column name that is indicated.  The sort will be ascending by
default, which can be reversed by setting the Descending parameter to TRUE.  The View will automatically redraw
itself to reflect the newly sorted content.

This method will fail if the referenced column does not exist.  Use a Column value of NULL to sort on item content
by default (useful if no columns have been defined for the view).

Sorting can be disabled if the NOSORTING flag is defined in the #Flags field.

-INPUT-
cstr Column: The name of the column to sort by.
int Descending: Set to TRUE if the sort should be descending.

-ERRORS-
Okay: The column was sorted successfully.
Args
NullArgs
Search: The referenced column could not be found.
-END-

*****************************************************************************/

static ERROR VIEW_SortColumn(objView *Self, struct viewSortColumn *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   if (Self->Flags & VWF_NO_SORTING) return ERR_Okay;

   if (!Args->Column) {
      struct viewSortColumnIndex sort;
      sort.Column     = 0;
      sort.Descending = Args->Descending;
      return VIEW_SortColumnIndex(Self, &sort);
   }

   // Find the column that has been referenced

   struct view_col *col;
   LONG colindex;
   for (col=Self->Columns, colindex=0; col; col=col->Next, colindex++) {
      if (!StrMatch(Args->Column, col->Name)) {
         struct viewSortColumnIndex sort;
         sort.Column     = colindex;
         sort.Descending = Args->Descending;
         return VIEW_SortColumnIndex(Self, &sort);
      }
   }

   return ERR_Search;
}

/*****************************************************************************

-METHOD-
SortColumnIndex: Sorts the view by column (index).

The SortColumnIndex() method will sort the view by the column index that is indicated.  The sort will be ascending by
default.  This can be reversed by setting the Descending parameter to TRUE.  The view will automatically redraw itself
to reflect the newly sorted content.

This method will fail if the referenced index is invalid.  Both hidden and visible columns are taken into account when
determining the column that the index refers to.

-INPUT-
int Column: The index of the column to sort by (indexes start from zero).
int Descending: Set to TRUE if the sort should be descending.

-ERRORS-
Okay
Args
-END-

*****************************************************************************/

static ERROR VIEW_SortColumnIndex(objView *Self, struct viewSortColumnIndex *Args)
{
   if ((!Args) OR (Args->Column < 0)) return ERR_Args;

   LogMsg("Column: %d, Descending: %d", Args->Column, Args->Descending);

   if (Self->Flags & VWF_NO_SORTING) return ERR_Okay;

   // Extend the sort list so that we have a history of sort attempts

   LONG j = Self->Sort[0];
   if (j < 0) j = -j;
   if (j-1 != Args->Column) {
      for (j=ARRAYSIZE(Self->Sort)-1; j > 0; j--) Self->Sort[j] = Self->Sort[j-1];
   }

   if (Args->Descending) Self->Sort[0] = -(Args->Column + 1);
   else Self->Sort[0] = Args->Column + 1;

   sort_items(Self);     // Do the sort based on the content of the Sort array
   arrange_items(Self);  // Rearrange the view items and do a complete redraw

   if (!Self->RedrawDue) {
      Self->RedrawDue = TRUE;
      DelayMsg(AC_Draw, Self->Layout->SurfaceID, NULL);
   }

   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
Sort: Re-sorts XML data in the view when it has been manually altered.
-END-

*****************************************************************************/

static ERROR VIEW_Sort(objView *Self, APTR Void)
{
   sort_items(Self);
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
RevealItem: Checks the visibility of an item, scrolling it into view if it is partially or fully hidden.

Use the RevealItem method to ensure that an item within the view is fully visible.  If the item is partially or fully
hidden, the item will be scrolled into view so that the user can see it without restriction.

If TagIndex is -1, the user's currently selected tag will be chosen.  If TagIndex is -2, the last item in the list is
targeted.  All other TagIndex values must be a valid lookup within the XML tag list.

-INPUT-
int TagIndex: The tag index of the item that needs to be viewable.

-ERRORS-
Okay
NullArgs
OutOfRange: The TagIndex is not within the valid range.
InvalidReference: The TagIndex does not refer to a valid XML item.
-END-

*****************************************************************************/

static ERROR VIEW_RevealItem(objView *Self, struct viewRevealItem *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   struct XMLTag *tag = NULL;
   if (Args->TagIndex IS -1) { // Reveal the currently selected tag
      if ((Self->SelectedTag >= 0) AND (Self->SelectedTag < Self->XML->TagCount)) {
         tag = Self->XML->Tags[Self->SelectedTag];
      }
      else return ERR_Okay; // No tag is selected
   }
   else if (Args->TagIndex IS -2) {  // Reveal the last item in the list
      LONG i;
      for (i=Self->XML->TagCount-1; i >= 0; i--) {
         tag = Self->XML->Tags[i];
         if (((struct view_node *)(tag->Private))->Flags & NODE_ITEM) break;
      }
   }
   else {
      if ((Args->TagIndex < 0) OR (Args->TagIndex >= Self->XML->TagCount)) return PostError(ERR_OutOfRange);
      tag = Self->XML->Tags[Args->TagIndex];
   }

   if (!tag) return ERR_InvalidReference;

   if (((struct view_node *)tag->Private)->Flags & NODE_ITEM) {
      check_item_visible(Self, tag);

      return ERR_Okay;
   }
   else return PostError(ERR_InvalidReference);
}

//****************************************************************************

#include "view_fields.c"
#include "view_functions.c"
#include "view_def.c"

static const struct FieldArray clFields[] = {
   { "Layout",            FDF_INTEGRAL|FDF_SYSTEM|FDF_R, 0, NULL, NULL },
   { "XML",               FDF_INTEGRAL|FDF_R,      ID_XML, NULL, NULL },
   { "Font",              FDF_INTEGRAL|FDF_R,      ID_FONT, NULL, NULL },
   { "Columns",           FDF_STRING|FDF_RW,    0, NULL, SET_Columns },
   { "ContextMenu",       FDF_OBJECT|FDF_RW,    ID_MENU, NULL, NULL },
   { "VScroll",           FDF_OBJECT|FDF_RW,    ID_SCROLL, NULL, SET_VScroll },
   { "HScroll",           FDF_OBJECT|FDF_RW,    ID_SCROLL, NULL, SET_HScroll },
   { "Document",          FDF_OBJECT|FDF_RW,    0, NULL, SET_Document },
   { "GroupFace",         FDF_STRING|FDF_RW,    0, NULL, SET_GroupFace },
   { "ItemNames",         FDF_STRING|FDF_RW,    0, NULL, SET_ItemNames },
   { "TextAttrib",        FDF_STRING|FDF_RW,    0, NULL, SET_TextAttrib },
   { "Focus",             FDF_OBJECTID|FDF_RI,  0, NULL, NULL },
   { "DragSource",        FDF_OBJECTID|FDF_RW,  0, NULL, NULL },
   { "Flags",             FDF_LONGFLAGS|FDF_RW, (MAXINT)&clViewFlags, NULL, SET_Flags },
   { "Style",             FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clViewStyle, NULL, SET_Style },
   { "HSpacing",          FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "VSpacing",          FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "SelectedTag",       FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "ActiveTag",         FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "HighlightTag",      FDF_LONG|FDF_R,       0, NULL, NULL },
   { "MaxItemWidth",      FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "ButtonThickness",   FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "IconSize",          FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "GfxFlags",          FDF_LONGFLAGS|FDF_RW, (MAXINT)&clViewGfxFlags, NULL, NULL },
   { "DragItemCount",     FDF_LONG|FDF_RW,      0, NULL, SET_DragItemCount },
   { "TotalItems",        FDF_LONG|FDF_R,       0, NULL, NULL },
   { "GroupHeight",       FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "ButtonBackground",  FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "ButtonHighlight",   FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "ButtonShadow",      FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "ColHighlight",      FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "ColSelect",         FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "ColItem",           FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "ColHairline",       FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "ColSelectHairline", FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "ColBackground",     FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "ColTitleFont",      FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "ColSelectFont",     FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "ColBkgdHighlight",  FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "ColBorder",         FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "ColButtonFont",     FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "ColAltBackground",  FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "ColGroupShade",     FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "ColBranch",         FDF_RGB|FDF_RW,       0, NULL, NULL },

   // Virtual fields
   { "BorderOffset",    FDF_LONG|FDF_W,            0, NULL, SET_BorderOffset },
   { "DateFormat",      FDF_STRING|FDF_RW,         0, GET_DateFormat, SET_DateFormat },
   { "DragItems",       FDF_ARRAY|FDF_LONG|FDF_RW, 0, GET_DragItems, SET_DragItems },
   { "IconFilter",      FDF_STRING|FDF_RW,         0, GET_IconFilter, SET_IconFilter },
   { "IconTheme",       FDF_STRING|FDF_RW,         0, GET_IconTheme, SET_IconTheme },
   { "LayoutStyle",     FDF_VIRTUAL|FDF_POINTER|FDF_SYSTEM|FDF_W, 0, NULL, SET_LayoutStyle },
   { "Selection",       FDF_STRING|FDF_RW,         0, GET_Selection, SET_Selection },
   { "SelectionIndex",  FDF_LONG|FDF_RW,           0, GET_SelectionIndex, SET_SelectionIndex },
   { "SelectedTags",    FDF_LONG|FDF_ARRAY|FDF_R,  0, GET_SelectedTags, NULL },
   { "Template",        FDF_STRING|FDF_RI,         0, NULL, SET_Template },
   { "TotalSelected",   FDF_LONG|FDF_R,            0, GET_TotalSelected, NULL },
   { "VarDefault",      FDF_STRING|FDF_W,          0, NULL, SET_VarDefault },
   { "ExpandCallback",  FDF_FUNCTIONPTR|FDF_RW,    0, GET_ExpandCallback, SET_ExpandCallback },
   { "SelectCallback",  FDF_FUNCTIONPTR|FDF_RW,    0, GET_SelectCallback, SET_SelectCallback },
   { "CellClick",       FDF_FUNCTIONPTR|FDF_RW,    0, GET_CellClick, SET_CellClick },
   END_FIELD
};
