/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Menu: Creates and manages program menus.

The Menu class provides a means to create and maintain menus in the graphical user interface.

This class is still in development.

-END-

To modify multiple items in a menu, we recommend calling the Clear action and re-submitting the item definitions
through the XML data feed.

*****************************************************************************/

//#define DEBUG

#define PRV_MENU
#define PRV_MENUITEM

#include "menu.h"

#include <parasol/main.h>
#include <parasol/modules/xml.h>
#include <parasol/system/keymaptable.h>
#include <parasol/modules/display.h>
#include <parasol/modules/iconserver.h>
#include <parasol/modules/surface.h>
#include <parasol/modules/widget.h>
#include "../defs.h"
#include "../hashes.h"

#define MAX_EXTITEM 40 // The maximum number of characters allowed in a menu extension item

static OBJECTPTR clMenu = NULL, clMenuItem = NULL;
struct Translation { LONG Code; CSTRING Name; };

// Class definition at the end of this source file.
static const struct FieldDef clMenuFlags[];
static const struct FieldArray clMenuFields[];
static const struct ActionArray clMenuActions[];
static const struct MethodArray clMenuMethods[];

static ERROR add_xml_item(objMenu *, objXML *, struct XMLTag *);
static ERROR calc_menu_size(objMenu *);
static void calc_scrollbar(objMenu *);
static ERROR create_menu(objMenu *);
static ERROR create_menu_file(objMenu *, objMenu *, objMenuItem *);
static void draw_default_bkgd(objMenu *, objSurface *, objBitmap *);
static void draw_menu(objMenu *, objSurface *, objBitmap *);
static void ensure_on_display(objMenu *);
static LONG get_item_height(objMenu *);
static void key_event(objMenu *, evKey *, LONG);
static ERROR load_icon(objMenu *, CSTRING, objBitmap **);
static ERROR process_menu_content(objMenu *);
static ERROR write_string(OBJECTPTR, CSTRING);
static ERROR highlight_item(objMenu *, objMenuItem *);
static void parse_xmltag(objMenu *, objXML *, struct XMLTag *);
static UBYTE scan_keys(objMenu *, LONG, LONG);
static ERROR fade_timer(objMenu *, LARGE, LARGE);
static ERROR item_motion_timer(objMenu *, LARGE, LARGE);
static ERROR motion_timer(objMenu *, LARGE, LARGE);

//****************************************************************************

ERROR init_menu(void)
{
   return CreateObject(ID_METACLASS, 0, &clMenu,
      FID_ClassVersion|TFLOAT, VER_MENU,
      FID_Name|TSTR,      "Menu",
      FID_Category|TLONG, CCF_GUI,
      FID_Flags|TLONG,    CLF_PROMOTE_INTEGRAL,
      FID_Actions|TPTR,   clMenuActions,
      FID_Methods|TARRAY, clMenuMethods,
      FID_Fields|TARRAY,  clMenuFields,
      FID_Size|TLONG,     sizeof(objMenu),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND);
}

void free_menu(void)
{
   if (clMenu) { acFree(clMenu); clMenu = NULL; }
}

//****************************************************************************

static ERROR MENU_ActionNotify(objMenu *Self, struct acActionNotify *NotifyArgs)
{
   if (!NotifyArgs) return ERR_NullArgs;
   if (NotifyArgs->Error != ERR_Okay) return ERR_Okay;

   LONG action = NotifyArgs->ActionID;

   if (action IS AC_Hide) {
      FMSG("~","My menu surface has been hidden.");

      Self->HighlightItem = NULL;

      if (Self->CurrentMenu) acHide(Self->CurrentMenu);

      if (!(Self->Flags & MNF_CACHE)) {
         if (Self->Scrollbar) { acFree(Self->Scrollbar); Self->Scrollbar = NULL; }
         if (Self->MenuSurfaceID) { acFreeID(Self->MenuSurfaceID); Self->MenuSurfaceID = 0; }
      }

      Self->TimeHide = PreciseTime();
      Self->Visible = FALSE;

      STEP();
   }
   else if (action IS AC_Focus) {
      if ((Self->KeyMonitorID IS NotifyArgs->ObjectID) AND (!Self->prvKeyEvent)) {
         FUNCTION callback;
         SET_FUNCTION_STDC(callback, &key_event);
         SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, &callback, Self, &Self->prvKeyEvent);
      }
   }
   else if (action IS AC_LostFocus) {
      if (Self->KeyMonitorID IS NotifyArgs->ObjectID) {
         if (Self->prvKeyEvent) { UnsubscribeEvent(Self->prvKeyEvent); Self->prvKeyEvent = NULL; }
      }

      if (NotifyArgs->ObjectID IS Self->RelativeID) {
         FMSG("~","Hiding because my relative surface (%d) lost the focus.", Self->RelativeID);
         acHide(Self);
         STEP();
      }
      else if ((NotifyArgs->ObjectID IS Self->MenuSurfaceID) AND (!Self->ParentID)) {
         FMSG("~","Hiding because my surface (%d) lost the focus and I am without a parent menu.", Self->MenuSurfaceID);
         acHide(Self);
         STEP();
      }
      else MSG("Surface %d has lost its focus, no action taken.", NotifyArgs->ObjectID);
   }
   else if (action IS AC_Show) {
      if ((Self->FadeDelay > 0) AND (!Self->Scrollbar)) {
         MSG("(Show) Starting fade-in.");
         Self->prvFade = MENUFADE_FADE_IN;
         Self->FadeTime = PreciseTime();

         if (Self->TimerID) UpdateTimer(Self->TimerID, 0.02);
         else {
            FUNCTION callback;
            SET_FUNCTION_STDC(callback, &fade_timer);
            SubscribeTimer(0.02, &callback, &Self->TimerID);
         }
      }
      else {
         MSG("(Show) Raising opacity to maximum.");
         Self->prvFade = 0;

         drwSetOpacityID(Self->MenuSurfaceID, 100, 0);
      }

      if ((Self->Flags & MNF_POPUP) AND (Self->RootMenu IS Self)) {
         // Give the focus to popup menus at the root level.  This allows the menu to hide itself if the user clicks away from it.

         MSG("Giving focus to the popup menu.");
         acFocusID(Self->MenuSurfaceID);
      }

      Self->TimeShow = PreciseTime();
      Self->Visible = TRUE;
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Activate: Switches the visibility state of the menu.
-END-
*****************************************************************************/

static ERROR MENU_Activate(objMenu *Self, APTR Void)
{
   // This routine returns ERF_Notified because Activate notification is manually generated on MenuItem.acActivate()

   LogBranch(NULL);
   Action(MT_MnSwitch, Self, NULL);
   LogBack();
   return ERR_Okay|ERF_Notified;
}

/*****************************************************************************
-ACTION-
Clears: Clears the content of the menu list.
-END-
*****************************************************************************/

static ERROR MENU_Clear(objMenu *Self, APTR Void)
{
   LogBranch(NULL);

   while (Self->Items) acFree(Self->Items);

   Self->prvLastItem   = NULL;
   Self->HighlightItem = NULL;
   Self->CurrentMenu   = NULL;
   Self->Selection     = NULL;

   if (Self->MenuSurfaceID) {
      OBJECTPTR object;
      if (!AccessObject(Self->MenuSurfaceID, 4000, &object)) {
         UnsubscribeAction(object, 0);
         gfxUnsubscribeInput(Self->MenuSurfaceID);
         acFree(object);
         ReleaseObject(object);
      }
      Self->MenuSurfaceID = 0;
   }

   LogBranch("Destroying all child menus.");

   struct ChildEntry list[16];
   LONG count = ARRAYSIZE(list);
   if (!ListChildren(Self->Head.UniqueID, list, &count)) {
      LONG i;
      for (i=0; i < count; i++) {
         if (list[i].ClassID IS ID_MENU) acFreeID(list[i].ObjectID);
      }
   }

   LogBack();

   LogBack();
   return ERR_Okay;
}

//****************************************************************************

static ERROR MENU_DataFeed(objMenu *Self, struct acDataFeed *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Args->DataType IS DATA_XML) {
      // Incoming XML is treated as being part of the menu content definition

      LogBranch(NULL);

      objXML *xml;
      if (!CreateObject(ID_XML, NF_INTEGRAL, &xml,
            FID_Statement|TSTR, Args->Buffer,
            TAGEND)) {

         struct XMLTag *tag;
         for (tag=xml->Tags[0]; tag; tag=tag->Next) {
            parse_xmltag(Self, xml, tag);
         }

         acFree(xml);

         // Recalculate the menu size

         if (Self->Head.Flags & NF_INITIALISED) {
            calc_menu_size(Self);
            if (Self->MenuSurfaceID) acResizeID(Self->MenuSurfaceID, Self->Width, Self->Height, 0);
            calc_scrollbar(Self);
            ensure_on_display(Self);
         }
      }
      else {
         LogBack();
         return PostError(ERR_CreateObject);
      }

      LogBack();
      return ERR_Okay;
   }
   else if (Args->DataType IS DATA_INPUT_READY) {
      struct InputMsg *input;

      while (!gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &input)) {
         if (input->Flags & JTYPE_MOVEMENT) {
            if (Self->MotionTimer) { UpdateTimer(Self->MotionTimer, 0); Self->MotionTimer = 0; }
            if (Self->ItemMotionTimer) { UpdateTimer(Self->ItemMotionTimer, 0); Self->ItemMotionTimer = 0; }

            if (input->RecipientID IS Self->MonitorID) {
               // Mouse movement over the monitored area for mouse clicks / hovering
               FUNCTION callback;
               SET_FUNCTION_STDC(callback, &motion_timer);
               SubscribeTimer(Self->HoverDelay, &callback, &Self->MotionTimer);
            }
            else if (input->RecipientID IS Self->MenuSurfaceID) {
               // Mouse movement over the menu itself

               UBYTE highlight_found = FALSE;

               if (input->OverID IS Self->MenuSurfaceID) {

                  objMenuItem *item;
                  LONG y = Self->TopMargin + Self->YPosition;
                  for (item=Self->Items; item; item=item->Next) {
                     if (!(item->Flags & MIF_BREAK)) {
                        if ((input->Y >= y) AND (input->Y < y + item->Height)) {
                           if (Self->HighlightItem != item) {
                              highlight_item(Self, item);
                           }
                           highlight_found = TRUE;
                           break;
                        }
                     }

                     y += item->Height;
                  }
               }

               // Remove existing menu highlighting if the cursor is no longer positioned over a highlight-able item.

               if ((!highlight_found) AND (Self->HighlightItem)) {
                  highlight_item(Self, NULL);
               }

               if (highlight_found) {
                  FUNCTION callback;
                  SET_FUNCTION_STDC(callback, &item_motion_timer);
                  SubscribeTimer(Self->AutoExpand, &callback, &Self->ItemMotionTimer);
               }
            }
         }
         else if (input->Type IS JET_LEFT_SURFACE) {
            if (Self->MotionTimer) { UpdateTimer(Self->MotionTimer, 0); Self->MotionTimer = 0; }
            if (Self->ItemMotionTimer) { UpdateTimer(Self->ItemMotionTimer, 0); Self->ItemMotionTimer = 0; }
         }
         else if (input->Flags & JTYPE_BUTTON) {
            if (input->Value > 0) {
               if (input->RecipientID IS Self->MonitorID) {
                  // The monitored surface has received a mouse click (this is normally used for popup menus or clickable zones that show the menu).

                  FMSG("~","Menu clicked (monitored area)");

                  if ((input->Type IS JET_LMB) OR (input->Type IS JET_RMB)) {
                     SURFACEINFO *info;
                     if ((Self->MenuSurfaceID) AND (((!drwGetSurfaceInfo(Self->MenuSurfaceID, &info))) AND (info->Flags & RNF_VISIBLE))) {
                        MSG("Menu is visible.");
                        if (Self->HoverDelay > 0) {
                           // Do nothing (menu stays visible)
                           MSG("Menu staying active as hoverdelay > 0");
                        }
                        else acHide(Self);
                     }
                     else acShow(Self);
                  }

                  STEP();
               }
               else if ((input->RecipientID IS Self->MenuSurfaceID) AND (input->Type IS JET_LMB)) {
                  // The menu surface has been clicked
                  FMSG("~","Menu clicked (menu surface)");

                  LONG y = Self->TopMargin + Self->YPosition;
                  objMenuItem *item;
                  for (item=Self->Items; item; item=item->Next) {
                     if (!(item->Flags & MIF_BREAK)) {
                        if ((input->Y >= y) AND (input->Y < y + item->Height)) {
                           acActivate(item);
                           break;
                        }
                     }

                     y += item->Height;
                  }

                  STEP();
               }
               else {
                  // A surface outside of the menu's area has been clicked
                  MSG("Clicked away from menu - hiding.");
                  acHide(Self);
               }
            }
         }
      }
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR MENU_Free(objMenu *Self, APTR Void)
{
   if (Self->Translation) { acFree(Self->Translation); Self->Translation = NULL; }
   if (Self->LocalArgs) { FreeResource(Self->LocalArgs); Self->LocalArgs = NULL; }

   acClear(Self); // Remove all items

   if (Self->prvKeyEvent)     { UnsubscribeEvent(Self->prvKeyEvent); Self->prvKeyEvent = NULL; }
   if (Self->MotionTimer)     { UpdateTimer(Self->MotionTimer, 0); Self->MotionTimer = 0; }
   if (Self->ItemMotionTimer) { UpdateTimer(Self->ItemMotionTimer, 0); Self->ItemMotionTimer = 0; }
   if (Self->TimerID)         { UpdateTimer(Self->TimerID, 0); Self->TimerID = 0; }

   if (Self->Checkmark)      { acFree(Self->Checkmark); Self->Checkmark = NULL; }
   if (Self->Style)          { FreeResource(Self->Style); Self->Style = NULL; }
   if (Self->Config)         { FreeResource(Self->Config); Self->Config = NULL; }
   if (Self->Path)           { FreeResource(Self->Path); Self->Path = NULL; }
   if (Self->Font)           { acFree(Self->Font); Self->Font = NULL; }
   if (Self->prvXML)         { acFree(Self->prvXML); Self->prvXML = NULL; }

   if (Self->KeyMonitorID) {
      objSurface *surface;
      if (!AccessObject(Self->KeyMonitorID, 3000, &surface)) {
         UnsubscribeAction(surface, AC_Focus);
         UnsubscribeAction(surface, AC_LostFocus);
         ReleaseObject(surface);
      }
   }

   if (Self->MenuSurfaceID) {
      OBJECTPTR object;
      if (!AccessObject(Self->MenuSurfaceID, 4000, &object)) {
         UnsubscribeAction(object, 0);
         drwRemoveCallback(object, &draw_menu);
         acFree(object);
         ReleaseObject(object);
      }
      Self->MenuSurfaceID = 0;
   }

   gfxUnsubscribeInput(0);

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
GetItem: Retrieves the MenuItem for a given ID.

This method will search for a MenuItem by ID and return it if discovered.  Failure to find the item will result in
an ERR_DoesNotExist error code.

-INPUT-
int ID: The ID of the menu item to retrieve.
&obj(MenuItem) Item: The discovered menu item is returned in this field.

-ERRORS-
Okay
NullArgs
DoesNotExist
-END-

*****************************************************************************/

static ERROR MENU_GetItem(objMenu *Self, struct mnGetItem *Args)
{
   if ((!Args) OR (!Args->ID) OR (!Args->Item)) return PostError(ERR_NullArgs);

   objMenuItem *item;
   for (item=Self->Items; item; item=item->Next) {
      if (item->ID IS Args->ID) {
         Args->Item = item;
         return ERR_Okay;
      }
   }

   Args->Item = NULL;
   return PostError(ERR_DoesNotExist);
}

/*****************************************************************************

-ACTION-
GetVar: Simplifies the reading of menu item information.

The GetVar method simplifies the retrieval of menu item information when using scripting languages.  Menu items are
referenced in the format 'item(id).field', where 'id' is a valid menu item ID and 'field' is a supported field name
found in the MenuItem structure.  It is also possible to substitute the ID for index lookups from 0 to the total
number of menu items available.  To do this, use a # prior to the index number.

This example reads the menu item text identified with ID 35: `item(35).text`.

This example reads the ID of the first available menu item: `item(0).id`.

Supported menu item fields include: GfxScript, Path, ActionScript, Name, Text, Flags, Key, Qualifiers, Colour,
Background, Index, Group, ID.
-END-

*****************************************************************************/

static ERROR MENU_GetVar(objMenu *Self, struct acGetVar *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   if ((!Args->Field) OR (!Args->Buffer) OR (Args->Size < 1)) {
      return PostError(ERR_Args);
   }

   if (!(Self->Head.Flags & NF_INITIALISED)) return PostError(ERR_Failed);

   CSTRING field = Args->Field;
   Args->Buffer[0] = 0;

   if (!StrCompare("item(", field, 0, 0)) {
      // Find the relevant menu item

      objMenuItem *item;
      LONG index = -1;
      if (field[5] IS '#') {
         index = StrToInt(field+6);
         for (item=Self->Items; item; item=item->Next) {
            if (!index) break;
            index--;
         }
      }
      else {
         LONG id = StrToInt(field+5);
         for (item=Self->Items; item; item=item->Next) {
            if (item->ID IS id) break;
         }
      }

      if (!item) {
         LogErrorMsg("Failed to lookup '%s'", field);
         return ERR_Search;
      }

      field += 5;
      while ((*field) AND (*field != ')')) field++;
      while ((*field) AND (*field != '.')) field++;
      if (*field IS '.') {
         field++;
         ULONG hash = StrHash(field, 0);
         switch(hash) {
            //case HASH_GfxScript:  StrCopy(item->GfxScript, Args->Buffer, Args->Size); break;
            case HASH_Path:       StrCopy(item->Path, Args->Buffer, Args->Size); break;
            case HASH_Name:       StrCopy(item->Name, Args->Buffer, Args->Size); break;
            case HASH_Text:       StrCopy(item->Text, Args->Buffer, Args->Size); break;
            case HASH_Flags:      IntToStr(item->Flags, Args->Buffer, Args->Size); break;
            case HASH_Key:        IntToStr(item->Flags, Args->Buffer, Args->Size); break;
            case HASH_Colour:     StrFormat(Args->Buffer, Args->Size, "%d,%d,%d,%d", item->Colour.Red, item->Colour.Green, item->Colour.Blue, item->Colour.Alpha); break;
            case HASH_Index:      IntToStr(item->Index, Args->Buffer, Args->Size); break;
            case HASH_Group:      IntToStr(item->Group, Args->Buffer, Args->Size); break;
            case HASH_ID:         IntToStr(item->ID, Args->Buffer, Args->Size); break;
            case HASH_Background: StrFormat(Args->Buffer, Args->Size, "%d,%d,%d,%d", item->Background.Red, item->Background.Green, item->Background.Blue, item->Background.Alpha); break;
            case HASH_Qualifiers: IntToStr(item->Flags, Args->Buffer, Args->Size); break;
            default: LogErrorMsg("Field name '%s' not recognised.", field); return ERR_Failed;
         }
         return ERR_Okay;
      }
      else {
         LogErrorMsg("Malformed item reference '%s'", Args->Field);
         return ERR_Failed;
      }
   }
   else return ERR_Failed;
}

/*****************************************************************************
-ACTION-
Hide: Hides the menu and open sub-menus.
-END-
*****************************************************************************/

static ERROR MENU_Hide(objMenu *Self, APTR Void)
{
   LogBranch(NULL);

   if ((Self->FadeDelay > 0) AND (!Self->Scrollbar)) {
      // NB: We must always use the timer to delay the hide, otherwise we get problems with the Activate()
      // support not switching menus on and off correctly.

      Self->prvFade = MENUFADE_FADE_OUT;
      Self->FadeTime = PreciseTime();

      if (Self->TimerID) UpdateTimer(Self->TimerID, 0.02);
      else {
         FUNCTION callback;
         SET_FUNCTION_STDC(callback, &fade_timer);
         SubscribeTimer(0.02, &callback, &Self->TimerID);
      }
   }
   else if (Self->MenuSurfaceID) {
      acHideID(Self->MenuSurfaceID);
      ProcessMessages(0, 0);
   }

   if (Self->CurrentMenu) { // Hide any sub-menus
      acHide(Self->CurrentMenu);
      Self->CurrentMenu = NULL;
   }

   LogBack();
   return ERR_Okay;
}

//****************************************************************************

static ERROR MENU_Init(objMenu *Self, APTR Void)
{
   if (Self->HighlightLM IS -1) Self->HighlightLM = Self->LeftMargin;
   if (Self->HighlightRM IS -1) Self->HighlightRM = Self->RightMargin;

   // Create a font object for drawing the menu text

   if (acInit(Self->Font) != ERR_Okay) return ERR_Init;
   if (acInit(Self->prvXML) != ERR_Okay) return ERR_Init;

   // If we have no parent, we are the root menu

   if (!Self->ParentID) Self->RootMenu = Self;

   // Mouse click monitoring

   if (Self->MonitorID) gfxSubscribeInput(Self->MonitorID, JTYPE_MOVEMENT|JTYPE_BUTTON|JTYPE_FEEDBACK, 0);

   // If no target was given, set the target to the top-most surface object

   BYTE find_target;
   if (!Self->TargetID) {
      OBJECTID desktop_id;
      find_target = TRUE;
      if (gfxGetDisplayType() != DT_NATIVE) {
         if (FastFindObject("desktop", ID_SURFACE, &desktop_id, 1, NULL) != ERR_Okay) {
            find_target = FALSE;
         }
      }
   }
   else find_target = FALSE;

   if (find_target) {
      OBJECTID ownerid = GetOwner(Self);
      while (ownerid) {
         CLASSID class_id = GetClassID(ownerid);
         if (class_id IS ID_SURFACE) {
            Self->TargetID = ownerid;
            SURFACEINFO *info;
            if (!drwGetSurfaceInfo(ownerid, &info)) {
               // Stop searching if we found a host surface (e.g. the desktop)
               if (info->Flags & RNF_HOST) break;
               ownerid = info->ParentID;
            }
            else ownerid = GetOwnerID(ownerid);
         }
         else if (class_id IS ID_WINDOW) {
            OBJECTPTR object;
            if (!AccessObject(ownerid, 5000, &object)) {
               GetLong(object, FID_Surface, &ownerid);
               ReleaseObject(object);
            }
            else ownerid = GetOwnerID(ownerid);
         }
         else ownerid = GetOwnerID(ownerid);
      }
      if (!Self->TargetID) return PostError(ERR_UnsupportedOwner);

      MSG("Target search found surface #%d.", Self->TargetID);
   }

   if (!Self->TargetID) Self->FadeDelay = 0;

   // The root menu monitors the keyboard

   if (Self->RootMenu IS Self) {
      if (!Self->KeyMonitorID) {
         if (Self->RelativeID) Self->KeyMonitorID = Self->RelativeID;
         else if (Self->TargetID) Self->KeyMonitorID = Self->TargetID;
      }

      objSurface *surface;
      if ((Self->KeyMonitorID) AND (!AccessObject(Self->KeyMonitorID, 4000, &surface))) {
         if (surface->Head.ClassID IS ID_SURFACE) {
            SubscribeActionTags(surface, AC_Focus, AC_LostFocus, TAGEND);
         }
         else Self->KeyMonitorID = 0;
         ReleaseObject(surface);
      }
   }

   AdjustLogLevel(1);
   ERROR error = process_menu_content(Self);
   AdjustLogLevel(-1);

   return error;
}

/*****************************************************************************
-ACTION-
MoveToPoint: Move the menu to a new display position.
-END-
*****************************************************************************/

static ERROR MENU_MoveToPoint(objMenu *Self, struct acMoveToPoint *Args)
{
   ActionMsg(AC_MoveToPoint, Self->MenuSurfaceID, Args);
   return ERR_Okay;
}

//****************************************************************************

static ERROR MENU_NewObject(objMenu *Self, APTR Void)
{
   if (NewObject(ID_FONT, NF_INTEGRAL, &Self->Font)) return ERR_NewObject;
   if (NewObject(ID_XML, NF_INTEGRAL, &Self->prvXML)) return ERR_NewObject;
   if (!(Self->LocalArgs = VarNew(0, 0))) return ERR_AllocMemory;

   SetString(Self->Font, FID_Face, "Open Sans");

   Self->LineLimit      = 200;
   Self->FadeDelay      = 0.5;
   Self->AutoExpand     = 40.0 / 1000.0;
   Self->BorderSize     = 1;
   Self->BreakHeight    = 6;
   Self->LeftMargin     = 5;
   Self->RightMargin    = 5;
   Self->TopMargin      = 3;
   Self->BottomMargin   = 3;
   Self->ImageGap       = 8;
   Self->ImageSize      = 16;
   Self->KeyGap         = 20;
   Self->VSpacing       = 4;
   Self->HighlightItem  = NULL;
   Self->HighlightLM    = -1;
   Self->HighlightRM    = -1;
   Self->ExtensionGap   = 20;

   Self->Highlight.Red   = 0;
   Self->Highlight.Green = 0;
   Self->Highlight.Blue  = 128;
   Self->Highlight.Alpha = 0; // Off by default.  Template can either use frames or set this colour

   Self->FontHighlight.Red   = 255;
   Self->FontHighlight.Green = 255;
   Self->FontHighlight.Blue  = 255;
   Self->FontHighlight.Alpha = 0;

   Self->FontColour.Red   = 0;
   Self->FontColour.Green = 0;
   Self->FontColour.Blue  = 0;
   Self->FontColour.Alpha = 255;

   // Assume that the menu is in english

   Self->Language[0] = 'E';
   Self->Language[1] = 'N';
   Self->Language[2] = 'G';
   Self->Language[3] = 0;

   StrCopy("lang", Self->LanguageDir, sizeof(Self->LanguageDir));

   drwApplyStyleValues(Self, NULL);

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Refresh: Refreshes a menu from its source file.
-END-
*****************************************************************************/

static ERROR MENU_Refresh(objMenu *Self, APTR Void)
{
   drwApplyStyleValues(Self, NULL);

   acClear(Self);

   MSG("Generating the new menu set.");

   AdjustLogLevel(1);
   ERROR error = process_menu_content(Self);
   AdjustLogLevel(-1);

   if (error) return error;

   if (!(error = create_menu(Self))) {
      return ERR_Okay;
   }
   else return PostError(error);
}

//****************************************************************************

static ERROR MENU_ScrollToPoint(objMenu *Self, struct acScrollToPoint *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   if (Args->Y IS Self->YPosition) return ERR_Okay;

   objSurface *surface;
   if (!AccessObject(Self->MenuSurfaceID, 5000, &surface)) {
      LONG y;
      if (Args->Flags & STP_Y) y = -Args->Y;
      else y = Self->YPosition;
      Self->YPosition = y;

      acDrawID(Self->MenuSurfaceID);
      ReleaseObject(surface);
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SelectItem: Toggle selectable menu items.

The state of selectable menu items can be modified with the SelectItem method.  The ID of the menu item to be toggled
is required, and the new State value must be indicated.  The State values are as follows:

<types type="State">
<type name="0">Turn the selection indicator off.</>
<type name="1">Turn the selection indicator on.</>
<type name="-1">Toggle the selection state.</>
</>

-INPUT-
int ID: The ID of the item to be executed (an ID must have been attributed to the item on creation).
int State: A state value of 0 (off), 1 (on) or -1 (toggle).

-ERRORS-
Okay
NullArgs
Args
DoesNotExist: The ID does not refer to a known menu item.
-END-

*****************************************************************************/

static ERROR MENU_SelectItem(objMenu *Self, struct mnSelectItem *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   FMSG("~","ID: %d, State: %d", Args->ID, Args->State);

   objMenuItem *item;
   for (item=Self->Items; item; item=item->Next) {
      if (item->ID IS Args->ID) {
         if (Args->State IS 0) {
            // Turn the selection off
            item->Flags &= ~MIF_SELECTED;
         }
         else if (Args->State IS 1) {
            // Turn the selection on
            if (item->Group) {
               objMenuItem *scan;
               for (scan=Self->Items; scan; scan=scan->Next) {
                  if (scan->Group IS item->Group) scan->Flags &= ~MIF_SELECTED;
               }
            }
            item->Flags |= MIF_SELECTED;
         }
         else if (Args->State IS -1) {
            // Toggle the selection
            if (item->Flags & MIF_SELECTED) item->Flags &= ~MIF_SELECTED;
            else item->Flags |= MIF_SELECTED;
         }
         else {
            STEP();
            return PostError(ERR_Args);
         }

         STEP();
         return ERR_Okay;
      }
   }

   STEP();
   return PostError(ERR_DoesNotExist);
}

/*****************************************************************************
-ACTION-
SetVar: Parameters to be passed on to item scripts are stored as variables.
-END-
*****************************************************************************/

static ERROR MENU_SetVar(objMenu *Self, struct acSetVar *Args)
{
   if ((!Args) OR (!Args->Field) OR (!Args->Field[0])) return ERR_NullArgs;

   return VarSetString(Self->LocalArgs, Args->Field, Args->Value);
}

/*****************************************************************************
-ACTION-
Show: Shows the menu.
-END-
*****************************************************************************/

static ERROR MENU_Show(objMenu *Self, APTR Void)
{
   if (!Self->MenuSurfaceID) {
      ERROR error;
      if ((error = create_menu(Self)) != ERR_Okay) return error;
   }

   objSurface *surface;
   if (!AccessObject(Self->MenuSurfaceID, 4000, &surface)) {
      if (surface->Flags & RNF_VISIBLE) {
         ReleaseObject(surface);
         return ERR_Okay;
      }

      LogBranch("Parent: %d, Surface: %d, Relative: %d %s", Self->ParentID, Self->MenuSurfaceID, Self->RelativeID, (Self->Flags & MNF_POPUP) ? "POPUP" : "");

      Self->prvReverseX = (Self->Flags & MNF_REVERSE_X) ? TRUE : FALSE;

      if (Self->ParentID) { // Display this menu relative to its parent in the hierarchy
         LONG parent_x = 0, parent_y = 0, parent_width = 0;

         objMenu *parent;
         if (!AccessObject(Self->ParentID, 1000, &parent)) {
            if (parent->MenuSurfaceID) {
               SURFACEINFO *info;
               if (!drwGetSurfaceInfo(parent->MenuSurfaceID, &info)) {
                  parent_x = info->X;
                  parent_y = info->Y;
                  parent_width = info->Width;
               }
            }
            parent->CurrentMenu = Self;
            ReleaseObject(parent);

            LONG x = parent_x + parent_width - Self->RightMargin;
            if (Self->prvReverseX) {
               x = parent_x - surface->Width + Self->RightMargin;
               if (x < 2) {
                  x = 2;
                  Self->prvReverseX = FALSE;
               }
            }
            else {
               SURFACEINFO *target;
               if ((Self->TargetID) AND (!drwGetSurfaceInfo(Self->TargetID, &target))) {
                  // A specific target surface is hosting the menu layer; adjust the coordinate if necessary to keep
                  // it from being partially hidden.
                  if (x + surface->Width >= target->Width) {
                     x = target->X - surface->Width + Self->RightMargin;
                     Self->prvReverseX = TRUE;
                  }
               }
            }

            acMoveToPoint(surface, x, parent_y + Self->VOffset, 0, MTF_X|MTF_Y);

            ensure_on_display(Self);
         }
         else PostError(ERR_AccessObject);
      }
      else if (Self->Flags & MNF_POINTER_PLACEMENT) {
         LONG cursor_x, cursor_y;
         if (!gfxGetCursorPos(&cursor_x, &cursor_y)) {
            LONG x, p_width, p_height, p_absx, p_absy;
            SURFACEINFO *parentinfo;
            DISPLAYINFO *scrinfo;
            if ((surface->ParentID) AND (!drwGetSurfaceInfo(surface->ParentID, &parentinfo))) {
               p_width  = parentinfo->Width;
               p_height = parentinfo->Height;
               p_absx   = parentinfo->AbsX;
               p_absy   = parentinfo->AbsY;
            }
            else if (!gfxGetDisplayInfo(0, &scrinfo)) {
               p_width  = scrinfo->Width;
               p_height = scrinfo->Height;
               p_absx   = 0; //scrinfo.X;
               p_absy   = 0; //scrinfo.Y;
            }
            else p_absx = p_absy = p_width = p_height = 0;

            if ((p_width) AND (p_height)) {
               // Determine the position at which the pop-up menu will open at, relative to the parent surface.  Notice that we don't want the menu to appear off the edge of the parent if we can help it.

               if (Self->prvReverseX) {
                  x = cursor_x - p_absx - 1 - surface->Width + Self->RightMargin;
                  if (x < 0) {
                     x = 0;
                     Self->prvReverseX = FALSE;
                  }
               }
               else {
                  x = cursor_x - p_absx - 1;
                  if (x + surface->Width > p_width-2) {
                     x -= surface->Width + Self->RightMargin;
                     Self->prvReverseX = TRUE;
                  }
               }

               LONG y = cursor_y - p_absy - 1;
               if (y + surface->Height > p_height-2) y -= surface->Height + Self->BottomMargin;

               if (x < 2) x = 2;
               if (y < 2) y = 2;
               acMoveToPoint(surface, x, y, 0, MTF_X|MTF_Y);
            }
         }
      }
      else if (Self->RelativeID) {
         // Correct the position of the menu according to the relative object that it is offset from.

         SURFACEINFO *target;
         if (!drwGetSurfaceInfo(Self->RelativeID, &target)) {
            LONG rel_absx = target->AbsX;
            LONG rel_absy = target->AbsY;

            LONG t_absx = 0, t_absy = 0, t_height = 4096;

            DISPLAYINFO *display;
            if (Self->TargetID) {
               if (!drwGetSurfaceInfo(Self->TargetID, &target)) {
                  t_absx = target->AbsX;
                  t_absy = target->AbsY;
                  t_height = target->Height;
               }
            }
            else if (!gfxGetDisplayInfo(0, &display)) {
               t_height = display->Height;
            }

            LONG x = rel_absx + Self->X - t_absx;
            LONG y = rel_absy + Self->Y - t_absy;

            if (Self->Flags & MNF_REVERSE_Y) y = rel_absy + Self->Y - t_absy - surface->Height;

            if ((y + surface->Height) > t_height) {
               if (Self->ParentID) { // Use this code if we are a child menu
                  y = y - surface->Height + get_item_height(Self) + Self->VWhiteSpace;
               }
               else y = y - surface->Height - Self->Y + Self->VWhiteSpace;
            }

            acMoveToPoint(surface, x, y, 0, MTF_X|MTF_Y);
         }
         else PostError(ERR_Failed);
      }

      acMoveToFront(surface);
      if (Self->FadeDelay > 0) drwSetOpacity(surface, 0, 0);
      acShow(surface);

      ReleaseObject(surface);

      LogBack();
      return ERR_Okay;
   }
   else return PostError(ERR_AccessObject);
}

/*****************************************************************************

-METHOD-
Switch: Switches the visible state of the menu.

The Switch method alternates the the visible state of the menu - for example, if the menu is hidden, calling Switch
will show the menu.  A time-lapse feature is supported so that a rapid changes to menu visibility can be avoided.  For
example, if the TimeLapse option is set to 10 milliseconds, the menu state will not change unless the specified amount
of time has elapsed since the last Show or Hide action.

-INPUT-
int TimeLapse: The amount of time that must elapse

-ERRORS-
Okay
-END-

*****************************************************************************/

static ERROR MENU_Switch(objMenu *Self, struct mnSwitch *Args)
{
   if ((Self->prvFade) AND (Self->FadeDelay > 0)) {
      // Do not interfere with fading menus
      MSG("Menu is currently fading.");
      return ERR_Okay;
   }

   LARGE timelapse;
   if ((Args) AND (Args->TimeLapse >= 0)) timelapse = Args->TimeLapse * 1000LL;
   else timelapse = 5000LL;

   LARGE time = PreciseTime();
   if (Self->TimeShow > Self->TimeHide) { // Hide the menu
      FMSG("~","Hiding the menu if time-lapse is met: " PF64() " / " PF64(), time - Self->TimeShow, timelapse);
      if (time - Self->TimeShow >= timelapse) acHide(Self);
      STEP();
   }
   else {
      FMSG("~","Showing the menu if time-lapse is met: " PF64() " / " PF64(), time - Self->TimeHide, timelapse);
      if (time - Self->TimeHide >= timelapse) acShow(Self);
      STEP();
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR motion_timer(objMenu *Self, LARGE Elapsed, LARGE CurrentTime)
{
   FMSG("~","Motion timer activated.");
   acShow(Self);
   Self->MotionTimer = 0;
   STEP();
   return ERR_Terminate;
}

//****************************************************************************

static ERROR item_motion_timer(objMenu *Self, LARGE Elapsed, LARGE CurrentTime)
{
   if (Self->HighlightItem) {
      if ((Self->HighlightItem->Flags & MIF_EXTENSION) AND (!(Self->HighlightItem->Flags & MIF_DISABLED))) {
         FMSG("~","Auto-exec activated.");
         acActivate(Self->HighlightItem);
         STEP();
      }
   }

   Self->ItemMotionTimer = 0;
   return ERR_Terminate;
}

//****************************************************************************

static ERROR fade_timer(objMenu *Self, LARGE Elapsed, LARGE CurrentTime)
{
   if (Self->Scrollbar) return ERR_Terminate;

   DOUBLE opacity = ((DOUBLE)(CurrentTime - Self->FadeTime)) / (Self->FadeDelay * 1000000.0) * 100.0;
   if (opacity >= 100) opacity = 100;

   UBYTE unsubscribe = TRUE;
   if ((Self->prvFade IS MENUFADE_FADE_IN) AND (Self->FadeDelay > 0)) {
      struct drwSetOpacity setopacity;
      setopacity.Value      = opacity;
      setopacity.Adjustment = 0;
      if ((ActionMsg(MT_DrwSetOpacity, Self->MenuSurfaceID, &setopacity) != ERR_Okay) OR (opacity >= 100)) {
         Self->prvFade = 0;
      }
      else unsubscribe = FALSE;
   }
   else if (Self->prvFade IS MENUFADE_FADE_OUT) {
      if (Self->FadeDelay > 0) {
         struct drwSetOpacity setopacity;
         setopacity.Value      = 100.0 - opacity;
         setopacity.Adjustment = 0;
         if ((ActionMsg(MT_DrwSetOpacity, Self->MenuSurfaceID, &setopacity) != ERR_Okay) OR (opacity < 1)) {
            Self->prvFade = 0;
            if (Self->MenuSurfaceID) acHideID(Self->MenuSurfaceID);
         }
         else unsubscribe = FALSE;
      }
      else {
         Self->prvFade = 0;
         if (Self->MenuSurfaceID) acHideID(Self->MenuSurfaceID);
      }
   }

   if (unsubscribe) {
      Self->TimerID = 0;
      Self->prvFade = 0;
      return ERR_Terminate;
   }
   else return ERR_Okay;
}

/*****************************************************************************

-FIELD-
AutoExpand: The number of seconds to wait before automatically expanding sub-menus.

When the user's pointer hovers over an expandable menu item, the AutoExpand value will define the number of seconds that
must pass before the sub-menu can be automatically expanded.  This value defaults to the user's preference settings,
so it is strongly recommended that it is not modified unless there is good reason to do so.

-FIELD-
BorderSize: The size of the border at the menu edge, measured in pixels.  Defaults to 1.

-FIELD-
BottomMargin: Total pixel white-space at the bottom edge of the menu.

-FIELD-
BreakHeight: The height of menu-break items, in pixels.

-FIELD-
Checkmark: An image to use for item checkmarks may be defined here.

This field allows an image to be used when drawing checkmarks in the menu items.  It must refer to the path of an
image that is in a recognised picture format (PNG is strongly recommended).

*****************************************************************************/

static ERROR SET_Checkmark(objMenu *Self, STRING Value)
{
   if (Self->Checkmark) { acFree(Self->Checkmark); Self->Checkmark = NULL; }

   if (!CreateObject(ID_PICTURE, NF_INTEGRAL, &Self->Checkmark,
         FID_Path|TSTR,   Value,
         FID_Flags|TLONG, PCF_FORCE_ALPHA_32,
         TAGEND)) {
      if (!acActivate(Self->Checkmark)) return ERR_Okay;
      else return ERR_Activate;
   }
   else return ERR_CreateObject;
}

/*****************************************************************************
-FIELD-
Config: The menu configuration, expressed as a string.

The menu configuration can be parsed from an XML string by setting this field.  This must be done prior to
initialisation.  Alternatively, set the #Path field to load the configuration from an @XML file.

*****************************************************************************/

static ERROR SET_Config(objMenu *Self, CSTRING Value)
{
   if (Self->Config) { FreeResource(Self->Config); Self->Config = NULL; }

   if ((Value) AND (*Value)) {
      Self->Config = StrClone(Value);
      if (!Self->Config) return ERR_AllocMemory;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
ExtensionGap: Adds additional space for accommodating sub-menu indicators.

If a menu contains sub-menus, extra space will be needed to accommodate the graphics indicators that identify the
items that are sub-menus.  By default this value is set to 20 pixels.

If the menu contains no sub-menus in the item configuration, the ExtensionGap value is ignored.

-FIELD-
FadeDelay: Maximum number of seconds that a fade-in or fade-out effect must be completed.

The FadeDelay defines the window of time, in seconds, that a fade-in or fade-out effect must be completed.  If zero,
menu fading will be disabled.

The default value for the FadeDelay originates from the user preferences, so it is strongly recommended that this
field is not configured manually.

-FIELD-
ItemFeedback: Provides instant feedback when a user interacts with a menu item.

Set the ItemFeedback field with a callback function in order to receive instant feedback when user interaction occurs
with a menu item.  The function prototype is `Function(*Menu, *MenuItem)`.

*****************************************************************************/

static ERROR GET_ItemFeedback(objMenu *Self, FUNCTION **Value)
{
   if (Self->ItemFeedback.Type != CALL_NONE) {
      *Value = &Self->ItemFeedback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_ItemFeedback(objMenu *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->ItemFeedback.Type IS CALL_SCRIPT) UnsubscribeAction(Self->ItemFeedback.Script.Script, AC_Free);
      Self->ItemFeedback = *Value;
      if (Self->ItemFeedback.Type IS CALL_SCRIPT) SubscribeAction(Self->ItemFeedback.Script.Script, AC_Free);
   }
   else Self->ItemFeedback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Flags: Optional flags.

The following flags are supported.  The majority of these options are technical and normally specified in the menu
template.

-FIELD-
Font: Refers to the @Font object that will be used for rendering text in the menu.

This field refers to the @Font object that will be used for rendering text in the menu.  Prior to
initialisation, the font can be configured if it is desirable to override the default font style.

-FIELD-
FontColour: The default font colour for menu items.

-FIELD-
FontHighlight: The default font colour for highlighted menu items.

-FIELD-
Highlight: Renders a background rectangle when an item is highlighted.

If the Highlight colour is set with an alpha component other than zero, item backgrounds will be rendered with this
colour when highlighted.

-FIELD-
HighlightBorder: Renders a border around highlighted items.

If the HighlightBorder colour is set with an alpha component other than zero, item borders will be rendered with this
colour when highlighted.

-FIELD-
HighlightLM: Overrides the #LeftMargin for highlighted items.

This field overrides the #LeftMargin value when an item is highlighted.  By default HighlightLM is set to a
value of -1, which disables this feature.

-FIELD-
HighlightRM: Overrides the #RightMargin for highlighted items.

This field overrides the #RightMargin value when an item is highlighted.  By default HighlightRM is set to a
value of -1, which disables this feature.

-FIELD-
HoverDelay: The number of seconds that must elapse before a hover event occurs in relation to the #Monitor surface.

Used in conjunction with #Monitor, this field defines the minimum number of seconds that must elapse before a
hover event is promoted to a 'click' and the menu is displayed.  This feature is disabled by default (set to zero).

Sub-second timing is supported, so values such as 0.25 for a quarter second are valid.

-FIELD-
IconFilter: Sets the preferred icon filter.

Setting the IconFilter will change the default graphics filter used for loading all future icons.  Existing
loaded icons are not affected by the change.

*****************************************************************************/

static ERROR GET_IconFilter(objMenu *Self, STRING *Value)
{
   if (Self->IconFilter[0]) *Value = Self->IconFilter;
   else *Value = NULL;
   return ERR_Okay;
}

static ERROR SET_IconFilter(objMenu *Self, CSTRING Value)
{
   if (!Value) Self->IconFilter[0] = 0;
   else StrCopy(Value, Self->IconFilter, sizeof(Self->IconFilter));
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
ImageGap: Defines the minimum amount of whitespace between images and text in menu items.

This field defines the minimum amount of whitespace between images and text in menu items.  The value is measured in
pixels and a minimum setting of 8 is recommended.

-FIELD-
ImageSize: The maximum width and height of image icons used in menu items.  The default value is 16.

-FIELD-
ItemHeight: The minimum allowable height of items, in pixels.

All menu items, with the exception of line-breaks, must have a height that is greater or equal to the value specified
here.  This field is set to zero by default, allowing items to be of any height.

-FIELD-
Items: A linked-list of @MenuItem objects.

Parsing the menu configuration data will result in the generation of this linked-list of @MenuItem objects.
The list can be traversed manually, however if only one item needs to be found then it is recommended that the
#GetItem() method is used.

-FIELD-
KeyGap: The minimum amount of white-space between menu item text and key hints (e.g. 'CTRL-C').

-FIELD-
KeyMonitor: Monitor a target surface for keypresses.

This field is normally set to the window surface that relates to the menu.  By doing so, the menu will monitor
that surface for keypresses and match them to keyboard shortcuts defined for the menu items.  A match will result in
the item behaviour being executed immediately.

-FIELD-
LeftMargin: Total pixel white-space on the left side of the menu.

-FIELD-
LineLimit: Limits the total number of items that can be displayed before resorting to scrollbars.

The LineLimit restricts the total number of items that can be displayed in the menu.  If there are more items than
there is space available, a vertical scrollbar will be displayed that scrolls the menu content.

-FIELD-
Path: Identifies the location of a menu configuration file to load.

To load a menu configuration file on initialisation, a menu path must be specified in this field.  Alternatively, set
the #Config field if the configuration is already in memory.

The validity of the path string will not be checked until the menu object is initialised.

*****************************************************************************/

static ERROR GET_Path(objMenu *Self, STRING *Value)
{
   if (Self->Path) {
      *Value = Self->Path;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
}

static ERROR SET_Path(objMenu *Self, CSTRING Value)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }

   if ((Value) AND (*Value)) {
      Self->Path = StrClone(Value);
      if (!Self->Path) return ERR_AllocMemory;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
MenuSurface: The surface used to render the menu.

This pre-allocated @Surface will host the menu's rendered graphics.

-FIELD-
Monitor: Respond to user clicks on this referenced surface by showing the menu.

This field can be set to a foreign surface prior to initialisation.  The surface will be monitored for user clicks,
which will cause the menu to be shown when an interaction occurs.  In addition, if #HoverDelay is defined,
hovering over the monitored surface for a set length of time will also cause the menu to be displayed.

-FIELD-
Node: The name of the menu node that will be used to configure the menu.

To configure a menu from a source that contains multiple menu elements, it may be desirable to specify which menu
should be used as the source material.  To do so, specify the name of the menu element here, and ensure that there
is a menu element with a matching 'name' attribute in the XML source.

*****************************************************************************/

static ERROR GET_Node(objMenu *Self, STRING *Value)
{
   if (Self->prvNode[0]) {
      *Value = Self->prvNode;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
}

static ERROR SET_Node(objMenu *Self, CSTRING Value)
{
   if (!Value) Self->prvNode[0] = 0;
   else StrCopy(Value, Self->prvNode, sizeof(Self->prvNode));
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Parent: If this is a sub-menu, this field refers to the parent.

This field will refer to the parent menu if the object was generated as a sub-menu.

-FIELD-
Relative: The primary surface to which the menu relates.

The Relative field should refer to a foreign surface to which the menu relates.  It is normally used to refer to an
application window so that the menu can be correctly offset at all times, as well as ensuring that the user focus
between the menu and the application is handled efficiently.

-FIELD-
RightMargin: Total pixel white-space on the right side of the menu.

-FIELD-
Selection: Returns the MenuItem structure for the most recently selected item.

This field returns the MenuItem structure for the most recently selected item.  It will return NULL if no item has been
selected, or if deselection of an item has occurred.

*****************************************************************************/

static ERROR GET_Selection(objMenu *Self, objMenuItem **Value)
{
   *Value = Self->Selection;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
SelectionIndex: The index of the item that was most recently selected.

-FIELD-
Target: Refers to the surface that will host the menu.

The surface in which the menu will be hosted is defined here.  Most commonly, it refers to the desktop surface, and
this will be the default if the Target is not set manually.

-FIELD-
Style: Use a style definition other than the default.

The style definition used by a menu can be changed by setting the Style field.  The string must refer to the name of
a menu style in one of the system-wide style scripts.

Setting the Style does nothing if the style name is not recognised (an appropriate error code will be returned).
-END-

*****************************************************************************/

static ERROR SET_Style(objMenu *Self, CSTRING Value)
{
   if (Self->Style) { FreeResource(Self->Style); Self->Style = NULL; }
   Self->Style = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
TopMargin: Total pixel white-space at the top edge of the menu.

-FIELD-
X: The horizontal position of the menu.

The X and Y fields define the position of the menu within its target surface.  If #Relative is defined, the
coordinates will be offset from the position of the #Relative surface.

*****************************************************************************/

static ERROR SET_X(objMenu *Self, LONG Value)
{
   Self->X = Value;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Y: The vertical position of the menu.

The X and Y fields define the position of the menu within its target surface.  If #Relative is defined, the
coordinates will be offset from the position of the #Relative surface.

*****************************************************************************/

static ERROR SET_Y(objMenu *Self, LONG Value)
{
   Self->Y = Value;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
VSpacing: The amount of vertical white-space between menu items.

-FIELD-
Width: A fixed menu width can be applied by setting this field.

To set a pre-calculated width against a menu, set this field. By default this field is normally set to zero, which
results in the Menu class calculating the menu width automatically.  Because an automatic calculation is usually
desirable, the Width should only be set manually if circumstances require it.  The ComboBox class is one such example
where the drop-down menu needs to match the width of the widget.
-END-
*****************************************************************************/

static ERROR GET_Width(objMenu *Self, LONG *Value)
{
   if (Self->FixedWidth) *Value = Self->FixedWidth;
   else {
      if ((Self->Head.Flags & NF_INITIALISED) AND (!Self->Width)) {
         calc_menu_size(Self);
      }
      *Value = Self->Width;
   }
   return ERR_Okay;
}

static ERROR SET_Width(objMenu *Self, LONG Value)
{
   Self->FixedWidth = Value;
   if (Self->FixedWidth < 0) Self->FixedWidth = 0;
   if (Self->Head.Flags & NF_INITIALISED) {
      if (Self->MenuSurfaceID) acResizeID(Self->MenuSurfaceID, Self->FixedWidth, 0, 0);
   }
   return ERR_Okay;
}

//****************************************************************************

static void key_event(objMenu *Self, evKey *Event, LONG Size)
{
   if (Self->Visible) {
      if (Self->CurrentMenu) {
         key_event(Self->CurrentMenu, Event, Size);
         return;
      }

      if (!(Event->Qualifiers & KQ_PRESSED)) return;

      MSG("Keypress detected.  Highlight: %p", Self->HighlightItem);

      objMenuItem *item;
      if (Event->Code IS K_DOWN) {
         if ((item = Self->HighlightItem)) item = item->Next;
         else item = Self->Items;

         while ((item) AND (item->Flags & MIF_BREAK)) item = item->Next;

         if (item) highlight_item(Self, item);
      }
      else if (Event->Code IS K_UP) {
         if ((item = Self->HighlightItem)) item = item->Prev;
         else item = Self->Items;
         while ((item) AND (item->Flags & MIF_BREAK)) item = item->Prev;
         if (item) highlight_item(Self, item);
      }
      else if (Event->Code IS K_LEFT) {
         acHide(Self);

         if (Self->ParentID) {
            highlight_item(Self, NULL);

            objMenu *menu;
            if (!AccessObject(Self->ParentID, 4000, &menu)) {
               menu->CurrentMenu = NULL;
               highlight_item(menu, Self->ParentItem);
               ReleaseObject(menu);
            }
         }
      }
      else if (Event->Code IS K_RIGHT) {
         if (Self->HighlightItem) {
            if ((Self->HighlightItem->Flags & MIF_EXTENSION) AND (!(Self->HighlightItem->Flags & MIF_DISABLED))) {
               if (Self->HighlightItem->SubMenu) {
                  Self->HighlightItem->SubMenu->HighlightItem = NULL;
               }

               acActivate(Self->HighlightItem);

               if (Self->HighlightItem->SubMenu) {
                  highlight_item(Self, NULL);  // Kill our current item selection and highlight the sub-menu
                  highlight_item(Self->HighlightItem->SubMenu, Self->HighlightItem->SubMenu->Items);
               }
            }
         }
         else {
            item = Self->Items;
            while ((item) AND (item->Flags & MIF_BREAK)) item = item->Next;
            if (item) highlight_item(Self, item);
         }
      }
      else if (Event->Code IS K_ESCAPE) {
         if (Self->RootMenu) acHide(Self->RootMenu);
      }
      else if ((Event->Code IS K_ENTER) OR (Event->Code IS K_SPACE)) {
         if (Self->HighlightItem) {
            if (Self->HighlightItem->SubMenu) {
               Self->HighlightItem->SubMenu->HighlightItem = NULL;
            }

            acActivate(Self->HighlightItem);

            if (Self->HighlightItem->SubMenu) {
               highlight_item(Self, NULL);  // Kill our current item selection and highlight the sub-menu
               highlight_item(Self->HighlightItem->SubMenu, Self->HighlightItem->SubMenu->Items);
            }
         }
      }
   }
   else {
      if (!(Event->Qualifiers & KQ_PRESSED)) return;
      scan_keys(Self, Event->Qualifiers, Event->Code);
   }
}

//****************************************************************************

static UBYTE scan_keys(objMenu *Self, LONG Flags, LONG Value)
{
   objMenuItem *item;

   for (item=Self->Items; item; item=item->Next) {
      if (item->SubMenu) {
         UBYTE result = scan_keys(item->SubMenu, Flags, Value);
         if (result) return TRUE;
      }

      if (Value IS item->Key) {
         if (item->Flags & MIF_NO_KEY_RESPONSE) break;

         if (!(item->Flags & MIF_KEY_REPEAT)) {
            if (Flags & KQ_REPEAT) break;
         }

         if (item->Qualifiers) {
            if (item->Qualifiers & (KQ_L_CONTROL|KQ_R_CONTROL)) {
               if (!((Flags & (KQ_L_CONTROL|KQ_R_CONTROL)) & item->Qualifiers)) continue;
            }

            if (item->Qualifiers & (KQ_L_SHIFT|KQ_R_SHIFT)) {
               if (!((Flags & (KQ_L_SHIFT|KQ_R_SHIFT)) & item->Qualifiers)) continue;
            }

            if (item->Qualifiers & (KQ_L_ALT|KQ_R_ALT)) {
               if (!((Flags & (KQ_L_ALT|KQ_R_ALT)) & item->Qualifiers)) continue;
            }

            if (item->Qualifiers & (KQ_L_COMMAND|KQ_R_COMMAND)) {
               if (!((Flags & (KQ_L_COMMAND|KQ_R_COMMAND)) & item->Qualifiers)) continue;
            }
         }

         if (item->Flags & MIF_DISABLED) return TRUE;

         acActivate(item);
         return TRUE;
      }
   }

   return FALSE;
}

//****************************************************************************

#include "functions.c"

#include "menu_def.c"

static const struct FieldArray clMenuFields[] = {
   { "HoverDelay",      FDF_DOUBLE|FDF_RW,    0, NULL, NULL },
   { "AutoExpand",      FDF_DOUBLE|FDF_RW,    0, NULL, NULL },
   { "FadeDelay",       FDF_DOUBLE|FDF_RW,    0, NULL, NULL },
   { "Items",           FDF_POINTER|FDF_R,    0, NULL, NULL },
   { "Font",            FDF_INTEGRAL|FDF_R,   0, NULL, NULL },
   { "Style",           FDF_STRING|FDF_RI,    0, NULL, SET_Style },
   { "Target",          FDF_OBJECTID|FDF_RI,  0, NULL, NULL },
   { "Parent",          FDF_OBJECTID|FDF_RI,  0, NULL, NULL },
   { "Relative",        FDF_OBJECTID|FDF_RW,  0, NULL, NULL },
   { "KeyMonitor",      FDF_OBJECTID|FDF_RW,  0, NULL, NULL },
   { "MenuSurface",     FDF_OBJECTID|FDF_R,   0, NULL, NULL },
   { "Monitor",         FDF_OBJECTID|FDF_RW,  0, NULL, NULL },
   { "Flags",           FDF_LONGFLAGS|FDF_RW, (MAXINT)&clMenuFlags, NULL, NULL },
   { "VSpacing",        FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "BreakHeight",     FDF_LONG|FDF_R,       0, NULL, NULL },
   { "Width",           FDF_LONG|FDF_RW,      0, GET_Width, SET_Width },
   { "LeftMargin",      FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "RightMargin",     FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "TopMargin",       FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "BottomMargin",    FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "HighlightLM",     FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "HighlightRM",     FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "ItemHeight",      FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "ImageSize",       FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "LineLimit",       FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "BorderSize",      FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "SelectionIndex",  FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "FontColour",      FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "FontHighlight",   FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "Highlight",       FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "HighlightBorder", FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "ImageGap",        FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "KeyGap",          FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "ExtensionGap",    FDF_LONG|FDF_RI,      0, NULL, NULL },

   // Virtual fields
   { "Checkmark",       FDF_STRING|FDF_W,    0, NULL, SET_Checkmark },
   { "IconFilter",      FDF_STRING|FDF_RW,   0, GET_IconFilter, SET_IconFilter },
   { "ItemFeedback",    FDF_FUNCTIONPTR|FDF_RW, 0, GET_ItemFeedback, SET_ItemFeedback },
   { "Path",            FDF_STRING|FDF_RW,   0, GET_Path, SET_Path },
   { "Node",            FDF_STRING|FDF_RW,   0, GET_Node, SET_Node },
   { "Selection",       FDF_POINTER|FDF_R,   0, GET_Selection, NULL },
   { "Config",          FDF_STRING|FDF_W,    0, NULL, SET_Config },
   { "X",               FDF_LONG|FDF_RW,     0, NULL, SET_X },
   { "Y",               FDF_LONG|FDF_RW,     0, NULL, SET_Y },
   END_FIELD
};

//****************************************************************************

#include "menuitem.c"
