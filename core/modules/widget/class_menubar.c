/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
MenuBar: The MenuBar class is used to create and manage groups of menus.

The MenuBar class creates and manages the use of menu bars in application interfaces.  A menu bar consists of a
horizontal strip of text buttons, each of which opens a drop-down menu.  Each drop-down menu is defined and managed
through the @Menu class.

-END-

*****************************************************************************/

#define PRV_MENUBAR
#include <parasol/modules/widget.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/display.h>
#include <parasol/modules/surface.h>
#include "defs.h"

static OBJECTPTR clMenuBar = NULL;

enum {
   STATE_ENTERED=1,
   STATE_EXITED,
   STATE_INSIDE
};

static void draw_menubar(objMenuBar *, objSurface *, objBitmap *);
static void draw_item(objMenuBar *, WORD);
static void process_xml(objMenuBar *, objXML *);
static void activate_item(objMenuBar *, LONG);
static void open_menu(objMenuBar *, LONG);

void free_menubar(void)
{
   if (clMenuBar) { acFree(clMenuBar); clMenuBar = NULL; }
}

static void item_feedback(objMenu *Menu, objMenuItem *Item)
{
   objMenuBar *Self = (objMenuBar *)CurrentContext();
   FUNCTION *fb = &Self->ItemFeedback;
   if (fb->Type IS CALL_STDC) {
      void (*routine)(objMenu *, objMenuItem *);
      routine = fb->StdC.Routine;

      if (fb->StdC.Context) {
         OBJECTPTR context = SetContext(fb->StdC.Context);
         routine(Menu, Item);
         SetContext(context);
      }
      else routine(fb->StdC.Context, Item);
   }
   else if (fb->Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = fb->Script.Script)) {
         const struct ScriptArg args[] = {
            { "Menu", FD_OBJECTPTR, { .Address = Menu } },
            { "Item", FD_OBJECTPTR, { .Address = Item } }
         };
         scCallback(script, fb->Script.ProcedureID, args, ARRAYSIZE(args));
      }
   }
}

//****************************************************************************

static ERROR MENUBAR_ActionNotify(objMenuBar *Self, struct acActionNotify *Args)
{
   if (!Args) return PostError(ERR_NullArgs);
   if (Args->Error != ERR_Okay) return ERR_Okay;

   if (Args->ActionID IS AC_Disable) {
      Self->Flags |= MBF_DISABLED;
      DelayMsg(AC_Draw, Self->RegionID, NULL);
   }
   else if (Args->ActionID IS AC_Enable) {
      Self->Flags &= ~MBF_DISABLED;
      DelayMsg(AC_Draw, Self->RegionID, NULL);
   }
   else if (Args->ActionID IS AC_LostFocus) {
      if (Self->LastMenu) {
         acHide(Self->LastMenu);
         Self->LastMenu = NULL;
      }
   }
   else return ERR_NoSupport;

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
AddMenu: Adds a new menu to the menubar.

New menu items can be added to the menubar by calling this method.  At a minimum you are required to specify a
Name for the new item.  The Name will double-up as the item text that is displayed in the menubar.

The Icon parameter allows icons to be displayed instead of text inside the menubar.  The icon string should
reference an image from the icon database, using the format `category/icon`.  You may specify a custom icon
image if you wish - the routine will detect this if it determines that you have specified a complete file path
in the Icon string.

To execute a script <i>statement</i> when the user clicks on the item, set the Script field.  The string that
you provide must be in a recognisable script format.  To execute a script <i>file</i> when the item is activated, use
the string format `script src="path:file"`.  Note: Your script must not create static objects that could linger
after the script has been executed.

-INPUT-
cstr Name: The name or text to display for the item.
cstr Icon: The icon to use for the item.
cstr Script: A script statement to be executed when the item is clicked by the user.
&obj(Menu) Menu: The created <class>Menu</class> object will be returned in this parameter.

-ERRORS-
Okay
Args: Invalid arguments (the Name must be specified at a minimum).
ArrayFull: No more items can be added because the menubar is at maximum capacity.
-END-

*****************************************************************************/

static ERROR MENUBAR_AddMenu(objMenuBar *Self, struct mbAddMenu *Args)
{
   if ((!Args) OR (!Args->Name)) return PostError(ERR_NullArgs);

   LogBranch("Name: %s, Icon: %s", Args->Name, Args->Icon);

   Args->Menu = NULL;

   if (Self->Total >= ARRAYSIZE(Self->Items)) return LogBackError(0, ERR_ArrayFull);

   LONG index = Self->Total;
   ClearMemory(Self->Items + index, sizeof(Self->Items[0]));

   StrCopy(Args->Name, Self->Items[index].Name, sizeof(Self->Items[index].Name));

   CSTRING value;
   if ((value = StrTranslateText(Args->Name))) {
      StrCopy(value, Self->Items[index].Translation, sizeof(Self->Items[index].Translation));
   }

   AdjustLogLevel(1);

   if (Args->Icon) { // Load the icon file as a picture
      char buffer[300];
      if (!StrCompare("icons:", Args->Icon, 6, 0)) {
         StrFormat(buffer, sizeof(buffer), "%s(16)", Args->Icon);
      }
      else {
         LONG i;
         for (i=0; (Args->Icon[i]) AND (Args->Icon[i] != ':'); i++);
         if (Args->Icon[i] IS ':') StrCopy(Args->Icon, buffer, sizeof(buffer));
         else StrFormat(buffer, sizeof(buffer), "icons:%s(16)", Args->Icon);
      }

      objPicture *picture;
      if (!CreateObject(ID_PICTURE, NF_INTEGRAL, &picture,
            FID_Path|TSTR,   buffer,
            FID_Flags|TLONG, PCF_FORCE_ALPHA_32,
            TAGEND)) {
         Self->Items[index].Picture = picture;
      }
      else LogErrorMsg("Failed to load menubar icon.");
   }

   LONG height;
   if (!drwGetSurfaceCoords(Self->RegionID, NULL, NULL, NULL, NULL, NULL, &height)) {
      objMenu *menu;
      if (!NewObject(ID_MENU, NF_INTEGRAL, &menu)) {
         // Variables must be set before the Statement

         CSTRING key = NULL, value;
         while (!VarIterate(Self->Keys, key, &key, &value, NULL)) {
            acSetVar(menu, key, value);
         }

         if (!SetFields(menu,
               FID_Target|TLONG,     Self->TargetID,
               FID_Relative|TLONG,   Self->RegionID,
               FID_Y|TLONG,          height - 1,
               FID_KeyMonitor|TLONG, Self->SurfaceID,
               FID_Style|TSTR,       Self->MenuStyle,
               FID_Config|TSTR,      Args->Script,
               TAGEND)) {

            FUNCTION func;
            SET_FUNCTION_STDC(func, &item_feedback);
            SetFunction(menu, FID_ItemFeedback, &func);

            menu->Flags |= MNF_CACHE;
            if (!acInit(menu)) {
               Self->Items[index].Menu = menu;
               Args->Menu = menu;
            }
            else acFree(menu);
         }
         else acFree(menu);
      }
   }

   Self->Total++;

   if (Self->RegionID) acDrawID(Self->RegionID);

   AdjustLogLevel(-1);
   LogBack();
   return ERR_Okay;
}

//****************************************************************************

static ERROR MENUBAR_DataFeed(objMenuBar *Self, struct acDataFeed *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   if (Args->DataType IS DATA_XML) {
      if (!Self->XML) {
         if (CreateObject(ID_XML, NF_INTEGRAL, &Self->XML,
               FID_Statement|TSTR, Args->Buffer,
               TAGEND) != ERR_Okay) {
            return PostError(ERR_CreateObject);
         }
      }
      else if (SetString(Self->XML, FID_Statement, Args->Buffer) != ERR_Okay) {
         return ERR_SetField;
      }

      process_xml(Self, Self->XML);
   }
   else if (Args->DataType IS DATA_INPUT_READY) {
      struct InputMsg *input, *scan;

      while (!gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &input)) {
         if (input->Flags & JTYPE_MOVEMENT) {
            LONG flags, index, oldindex, i;

            ERROR inputerror;
            while (!(inputerror = gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &scan))) {
               if (scan->Flags & JTYPE_MOVEMENT) input = scan;
               else break;
            }

            // Determine what item we are positioned over

            index = -1;
            if (input->OverID IS Self->RegionID) {
               for (i=0; i < Self->Total; i++) {
                  if ((input->X >= Self->Items[i].X) AND (input->X <  Self->Items[i].X + Self->Items[i].Width)) {
                     index = i;
                     break;
                  }
               }
            }

            // Do nothing if the selected item remains unchanged

            if (index IS Self->Index) continue;

            // Redraw the previous selected item

            if (Self->Index != -1) {
               oldindex = Self->Index;
               Self->Index = -1;
               draw_item(Self, oldindex);
            }

            // Draw the new selected index

            Self->Index = index;
            if (index != -1) draw_item(Self, index);

            if ((Self->LastMenu) AND (Self->LastMenu != Self->Items[Self->Index].Menu)) {
               if ((Self->LastMenu->MenuSurfaceID) AND (!drwGetSurfaceFlags(Self->LastMenu->MenuSurfaceID, &flags))) {
                  if (flags & RNF_VISIBLE) {
                     activate_item(Self, Self->Index);
                  }
               }
            }

            if (inputerror) break;
            else input = scan;

            // Note that this code has to 'drop through' due to the movement consolidation loop earlier in this subroutine.
         }

         /*
         if (input->Value > 0) {
            LONG flags;

            if (Self->Flags & MBF_DISABLED) continue;
            if (!(click->ButtonFlags & JD_LMB)) continue;

            if (click) {
               if (drwGetSurfaceFlags(Self->SurfaceID, &flags)) {
                  if (flags & RNF_HAS_FOCUS) {
                     open_menu(Self, Self->Index);
                  }
               }
            }
         }
         */
         if (input->Type IS JET_LMB) {
            if (input->Value IS 0) {
               // Menus need to pop-up on click-release and not standard click.  This is due to the order in which window focussing and click notifications are processed.

               if (Self->Flags & MBF_DISABLED) continue;

               open_menu(Self, Self->Index);
            }
         }
      }
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Disable: Turns the entire menubar off.
-END-
*****************************************************************************/

static ERROR MENUBAR_Disable(objMenuBar *Self, APTR Void)
{
   // See the ActionNotify routine to see what happens when the surface is disabled.

   LogAction(NULL);
   acDisableID(Self->RegionID);
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
DisableMenu: Disables a menubar item.

Individual menubar items can be disabled by calling this method.  You are required to provide the name of the item
that you want to disable.  If multiple items share the same name, they will all be disabled.  The menubar will be
automatically redrawn as a result of calling this method.

A disabled item can be re-enabled by calling the #EnableMenu() method.

-INPUT-
cstr Name: The name of the item to be disabled.

-ERRORS-
Okay: The item was either disabled or does not exist.
NullArgs
DoesNotExist: The Name does not refer to an existing menu.
-END-

*****************************************************************************/

static ERROR MENUBAR_DisableMenu(objMenuBar *Self, struct mbDisableMenu *Args)
{
   if ((!Args) OR (!Args->Name) OR (!Args->Name[0])) return ERR_NullArgs;

   LONG i;
   for (i=0; i < Self->Total; i++) {
      if (!StrMatch(Args->Name, Self->Items[i].Name)) {
         if (!(Self->Items[i].Flags & TIF_DISABLED)) {
            Self->Items[i].Flags |= TIF_DISABLED;
            draw_item(Self, i);
         }
         return ERR_Okay;
      }
   }

   return ERR_DoesNotExist;
}

/*****************************************************************************

-METHOD-
EnableMenu: Enables a menubar item that has been earlier disabled.

Menu items that have been disabled can be re-enabled by calling this method.  You are required to provide the name of
the item that you want to enable.  If multiple items share the same name, they will all be enabled.

-INPUT-
cstr Name: The name of the item to be enabled.

-ERRORS-
Okay: The item was either enabled or does not exist.
NullArgs: A valid Name was not specified.
DoesNotExist: The Name does not refer to an existing menu.
-END-

*****************************************************************************/

static ERROR MENUBAR_EnableMenu(objMenuBar *Self, struct mbEnableMenu *Args)
{
   if ((!Args) OR (!Args->Name) OR (!Args->Name[0])) return ERR_NullArgs;

   LONG i;
   for (i=0; i < Self->Total; i++) {
      if (!StrMatch(Args->Name, Self->Items[i].Name)) {
         if (Self->Items[i].Flags & TIF_DISABLED) {
            Self->Items[i].Flags &= ~TIF_DISABLED;
            draw_item(Self, i);
         }
         return ERR_Okay;
      }
   }

   return ERR_DoesNotExist;
}

/*****************************************************************************
-ACTION-
Enable: Turns the menubar on if it has been disabled.
-END-
*****************************************************************************/

static ERROR MENUBAR_Enable(objMenuBar *Self, APTR Void)
{
   // See the ActionNotify routine to see what happens when the surface is enabled.

   LogAction(NULL);
   acEnableID(Self->RegionID);
   return ERR_Okay;
}

//****************************************************************************

static ERROR MENUBAR_Free(objMenuBar *Self, APTR Void)
{
   LONG i;
   for (i=0; i < Self->Total; i++) {
      if (Self->Items[i].Menu) { acFree(Self->Items[i].Menu); Self->Items[i].Menu = NULL; }
      if (Self->Items[i].Picture) { acFree(Self->Items[i].Picture); Self->Items[i].Picture = NULL; }
   }

   if (Self->Keys)      { VarFree(Self->Keys); Self->Keys = NULL; }
   if (Self->XML)       { acFree(Self->XML); Self->XML = NULL; }
   if (Self->Font)      { acFree(Self->Font); Self->Font = NULL; }
   if (Self->Path)      { FreeMemory(Self->Path); Self->Path = NULL; }
   if (Self->MenuStyle) { FreeMemory(Self->MenuStyle); Self->MenuStyle = NULL; }
   if (Self->RegionID)  { acFreeID(Self->RegionID); Self->RegionID = 0; }

   OBJECTPTR object;
   if ((Self->SurfaceID) AND (!AccessObject(Self->SurfaceID, 5000, &object))) {
      UnsubscribeFeed(object);
      ReleaseObject(object);
   }

   gfxUnsubscribeInput(0);

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
GetMenu: Retrieves the menu object associated with a menubar item.

To retrieve a menu object created by a menubar, call the GetMenu method with the name of the menu that is to be
retrieved.  If the name matches an existing menu, a pointer to that menu object will be returned in the Menu
parameter.  Otherwise, a DoesNotExist error code is returned.

Sub-menus cannot be returned by this routine.  You will need to get the base menu and then use the Menu class'
GetSubMenu method to achieve this objective.

-INPUT-
cstr Name: The name of the item to retrieve.
&obj(Menu) Menu: A pointer to the menu will be returned here.

-ERRORS-
Okay: The item was either enabled or does not exist.
Args: A valid Name was not specified.
DoesNotExist: The Name does not refer to an existing menu.
-END-

*****************************************************************************/

static ERROR MENUBAR_GetMenu(objMenuBar *Self, struct mbGetMenu *Args)
{
   if ((!Args) OR (!Args->Name) OR (!Args->Name[0])) return ERR_NullArgs;

   Args->Menu = NULL;
   LONG i;
   for (i=0; i < Self->Total; i++) {
      if (!StrMatch(Args->Name, Self->Items[i].Name)) {
         Args->Menu = Self->Items[i].Menu;
         return ERR_Okay;
      }
   }

   return ERR_DoesNotExist;
}

/*****************************************************************************
-ACTION-
GetVar: Pass-through arguments can be retrieved through this action.
-END-
*****************************************************************************/

static ERROR MENUBAR_GetVar(objMenuBar *Self, struct acGetVar *Args)
{
   if ((!Args) OR (!Args->Field)) return ERR_NullArgs;

   CSTRING val;
   if ((val = VarGetString(Self->Keys, Args->Field))) {
      StrCopy(val, Args->Buffer, Args->Size);
      return ERR_Okay;
   }
   else return ERR_UnsupportedField;
}

/*****************************************************************************
-ACTION-
Hide: Removes the menubar from the display.
-END-
*****************************************************************************/

static ERROR MENUBAR_Hide(objMenuBar *Self, APTR Void)
{
   acHideID(Self->RegionID);
   return ERR_Okay;
}

//****************************************************************************

static ERROR MENUBAR_Init(objMenuBar *Self, APTR Void)
{
   // Find the parent surface

   if (!Self->SurfaceID) {
      OBJECTID owner_id = GetOwner(Self);
      while ((owner_id) AND (GetClassID(owner_id) != ID_SURFACE)) {
         owner_id = GetOwnerID(owner_id);
      }
      if (owner_id) Self->SurfaceID = owner_id;
      else return ERR_UnsupportedOwner;
   }

   if (acInit(Self->Font) != ERR_Okay) return ERR_Init;

   objSurface *surface;
   if (!AccessObject(Self->RegionID, 5000, &surface)) {
      //surface->Flags |= RNF_IGNORE_FOCUS;
      surface->Flags |= RNF_GRAB_FOCUS;

      SetFields(surface, FID_Parent|TLONG, Self->SurfaceID,
                         FID_Region|TLONG, TRUE,
                         TAGEND);

      if (!(surface->Dimensions & DMF_HEIGHT)) {
         if ((!(surface->Dimensions & DMF_Y)) OR (!(surface->Dimensions & DMF_Y_OFFSET))) {
            SetLong(surface, FID_Height, 24);
         }
      }

      if (!acInit(surface)) {
         SubscribeActionTags(surface,
            AC_Disable,
            AC_Enable,
            AC_LostFocus,
            TAGEND);

         gfxSubscribeInput(Self->RegionID, JTYPE_MOVEMENT|JTYPE_BUTTON, 0);
      }
      else {
         ReleaseObject(surface);
         return ERR_Init;
      }

      ReleaseObject(surface);
   }
   else return ERR_AccessObject;

   // Use the base template to create the menubar graphics

   drwApplyStyleGraphics(Self, Self->RegionID, NULL, NULL);

   if (!AccessObject(Self->RegionID, 5000, &surface)) {
      // Subscribe after setting the template in order to draw graphics in the foreground.

      drwAddCallback(surface, &draw_menubar);
      ReleaseObject(surface);
   }
   else return ERR_AccessObject;

   if (Self->Path) {
      objXML *xml;
      if (!CreateObject(ID_XML, NF_INTEGRAL, &xml,
            FID_Path|TSTR, Self->Path,
            TAGEND)) {
         process_xml(Self, xml);
         acFree(xml);
      }
      else return ERR_CreateObject;
   }

   if (!(Self->Flags & MBF_HIDE)) acShow(Self);

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToBack: Moves the menubar to the back of the display area.
-END-
*****************************************************************************/

static ERROR MENUBAR_MoveToBack(objMenuBar *Self, APTR Void)
{
   acMoveToBackID(Self->RegionID);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToFront: Moves the menubar to the front of the display area.
-END-
*****************************************************************************/

static ERROR MENUBAR_MoveToFront(objMenuBar *Self, APTR Void)
{
   acMoveToFrontID(Self->RegionID);
   return ERR_Okay;
}

//****************************************************************************

static ERROR MENUBAR_NewObject(objMenuBar *Self, APTR Void)
{
   if (!NewLockedObject(ID_SURFACE, NF_INTEGRAL|Self->Head.Flags, NULL, &Self->RegionID)) {
      if (!NewObject(ID_FONT, NF_INTEGRAL|Self->Head.Flags, &Self->Font)) {
         SetString(Self->Font, FID_Face, glDefaultFace);

         FastFindObject("desktop", ID_SURFACE, &Self->TargetID, 1, NULL);

         Self->Index          = -1;
         Self->LeftMargin     = 4;
         Self->RightMargin    = 4;
         Self->Gap            = 6;
         Self->HighlightFlags = MHG_LIGHT_BKGD;

         Self->Highlight.Red   = 255;
         Self->Highlight.Green = 255;
         Self->Highlight.Blue  = 255;
         Self->Highlight.Alpha = 255;
         Self->Shadow.Red   = 0;
         Self->Shadow.Green = 0;
         Self->Shadow.Blue  = 0;
         Self->Shadow.Alpha = 0;

         drwApplyStyleValues(Self, NULL);

         SetString(Self, FID_MenuStyle, "default");

         return ERR_Okay;
      }
      else return ERR_NewObject;
   }
   else return ERR_NewObject;
}

/*****************************************************************************

-METHOD-
RemoveMenu: Removes an item from the menubar.

This method will remove items from the menubar.  You need to provide the name of the item that you want to remove.
If items matching the name that you provide will be deleted.

The menubar will be automatically redrawn as a result of calling this method.

-INPUT-
cstr Name: The name of the item that you want to remove.

-ERRORS-
Okay
NullArgs
-END-

*****************************************************************************/

static ERROR MENUBAR_RemoveMenu(objMenuBar *Self, struct mbRemoveMenu *Args)
{
   if ((!Args) OR (!Args->Name) OR (!Args->Name[0])) return ERR_NullArgs;

   LONG i;
   for (i=0; i < Self->Total; i++) {
      if (!StrMatch(Args->Name, Self->Items[i].Name)) {
         if (Self->Items[i].Menu IS Self->LastMenu) Self->LastMenu = NULL;

         if (Self->Items[i].Menu)    acFree(Self->Items[i].Menu);
         if (Self->Items[i].Picture) acFree(Self->Items[i].Picture);

         if (i < Self->Total-1) {
            CopyMemory(Self->Items + i + 1, Self->Items + i, sizeof(Self->Items[0]) * (Self->Total - i - 1));
         }

         Self->Total--;
      }
   }

   if (Self->RegionID) acDrawID(Self->RegionID);

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
ReplaceMenu: Replaces an existing item in the menubar.

This method follows the same conventions as AddMenu, but replaces existing menu objects with new menu definitions.
Please refer to AddMenu for information on how to set the arguments for this method.

The Name that you provide to this method must match an existing menu item, otherwise ERR_Search will be returned.

-INPUT-
cstr Name: The name or text to display for the item.
cstr Icon: The icon to use for the item.
cstr Script: A script statement to be executed when the item is clicked by the user.

-ERRORS-
Okay:
Args: Invalid arguments (the Name must be specified at a minimum).
Search: The menu identified by Name does not exist.
GetSurfaceInfo: Failed to get information on the menubar surface.
CreateObject: Failed to create the new Menu object.
-END-

*****************************************************************************/

static ERROR MENUBAR_ReplaceMenu(objMenuBar *Self, struct mbReplaceMenu *Args)
{
   objPicture *picture;
   objMenu *menu;
   UBYTE buffer[300];
   LONG i, index;
   ERROR error;

   if ((!Args) OR (!Args->Name)) return PostError(ERR_NullArgs);

   LogBranch("Name: %s, Icon: %s %s", Args->Name, Args->Icon, Args->Script);

   for (index=0; index < Self->Total; index++) {
      if (!StrMatch(Args->Name, Self->Items[index].Name)) break;
   }

   if (index >= Self->Total) {
      LogBack();
      return ERR_Search;
   }

   if (Args->Icon) {
      // Load the icon file as a picture

      if (!StrCompare("icons:", Args->Icon, 6, 0)) {
         StrFormat(buffer, sizeof(buffer), "%s(16)", Args->Icon);
      }
      else {
         for (i=0; (Args->Icon[i]) AND (Args->Icon[i] != ':'); i++);
         if (Args->Icon[i] IS ':') StrCopy(Args->Icon, buffer, sizeof(buffer));
         else StrFormat(buffer, sizeof(buffer), "icons:%s(16)", Args->Icon);
      }

      if (!NewObject(ID_PICTURE, NF_INTEGRAL, &picture)) {
         SetString(picture, FID_Path, buffer);
         picture->Flags |= PCF_FORCE_ALPHA_32;

         if (!acInit(picture)) {
            if (Self->Items[index].Picture) acFree(Self->Items[index].Picture);
            Self->Items[index].Picture = picture;
         }
         else {
            acFree(picture);
            picture = NULL;
            LogErrorMsg("Failed to load menubar icon.");
         }
      }
   }

   error = ERR_Okay;
   LONG height;
   if (!drwGetSurfaceCoords(Self->RegionID, NULL, NULL, NULL, NULL, NULL, &height)) {
      if (!CreateObject(ID_MENU, NF_INTEGRAL, &menu,
            FID_Target|TLONG,     Self->TargetID,
            FID_Relative|TLONG,   Self->RegionID,
            FID_Y|TLONG,          height - 1,
            FID_KeyMonitor|TLONG, Self->SurfaceID,
            FID_Style|TSTR,       Self->MenuStyle,
            FID_Config|TSTR,      Args->Script,
            FID_Flags|TLONG,      MNF_CACHE,
            TAGEND)) {

         CSTRING key = NULL, value;
         while (!VarIterate(Self->Keys, key, &key, &value, NULL)) {
            acSetVar(menu, key, value);
         }

         if (Self->Items[index].Menu) {
            if (Self->Items[index].Menu IS Self->LastMenu) Self->LastMenu = NULL;
            acFree(Self->Items[index].Menu);
         }
         Self->Items[index].Menu = menu;
      }
      else error = ERR_CreateObject;
   }
   else error = ERR_GetSurfaceInfo;

   LogBack();
   return error;
}

/*****************************************************************************
-ACTION-
SetVar: Pass-through arguments can be set by using this action.
-END-
*****************************************************************************/

static ERROR MENUBAR_SetVar(objMenuBar *Self, struct acSetVar *Args)
{
   if ((!Args) OR (!Args->Field) OR (!Args->Field[0])) return ERR_NullArgs;

   // Check if the argument refers back to itself (e.g. 'path' = '{path}' would cause a loop-back)
   // Set the field value to a null-string if such an occurrence is detected.

   CSTRING field = Args->Field;
   CSTRING value = Args->Value;
   WORD i;
   if (value[0] IS '{') {
      UBYTE ch1, ch2;
      for (i=0; (field[i]) AND (value[i+1]); i++) {
         ch1 = field[i]; if ((ch1 >= 'A') AND (ch1 <= 'Z')) ch1 = ch1 - 'A' + 'a';
         ch2 = value[i+1];  if ((ch2 >= 'A') AND (ch2 <= 'Z')) ch2 = ch2 - 'A' + 'a';
         if (ch1 != ch2) break;
      }
      if ((!field[i]) AND (value[i+1] IS '}') AND (!value[i+2])) {
         LogErrorMsg("Warning: Resetting looped argument '%s = %s'", field, value);
         value = "";
      }
   }

   if (!Self->Keys) {
      if (!(Self->Keys = VarNew(0, 0))) return ERR_AllocMemory;
   }

   return VarSetString(Self->Keys, field, value);
}

/*****************************************************************************
-ACTION-
Show: Puts the menubar on display.
-END-
*****************************************************************************/

static ERROR MENUBAR_Show(objMenuBar *Self, APTR Void)
{
   acShowID(Self->RegionID);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Bottom: The bottom coordinate of the menubar.

The bottom coordinate of the menubar (calculated as Y + Height) is readable from this field.

*****************************************************************************/

static ERROR GET_Bottom(objMenuBar *Self, LONG *Value)
{
   SURFACEINFO *info;
   if (!drwGetSurfaceInfo(Self->RegionID, &info)) {
      *Value = info->Y + info->Height;
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FIELD-
BottomMargin: Defines empty white-space at the bottom of the menubar.

The amount of whitespace between the menubar items and the bottom of the surface is defined here.  A value between 3
and 6 is recommended.

-FIELD-
ItemFeedback: Provides instant feedback when a user interacts with a menu item.

Set the ItemFeedback field with a callback function in order to receive instant feedback when user interaction occurs
with a menu item.  The function prototype is `routine(*Menu, *MenuItem)`

*****************************************************************************/

static ERROR GET_ItemFeedback(objMenuBar *Self, FUNCTION **Value)
{
   if (Self->ItemFeedback.Type != CALL_NONE) {
      *Value = &Self->ItemFeedback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_ItemFeedback(objMenuBar *Self, FUNCTION *Value)
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
Flags: Optional flags may be defined here.

-FIELD-
Font: The font that will be used for drawing the menu items.

This field refers to the font object that will be used to draw the menu items.  You can set attributes such as the
colour, face and size of the font if you wish.

-FIELD-
Gap: Defines the gap between menu items.  A value of at least 6 is recommended.

-FIELD-
Height: Defines the height of the menubar.

A menubar can be given a fixed or relative height by setting this field to the desired value.  To set a relative
height, use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Height(objMenuBar *Self, struct Variable *Value)
{
   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      DOUBLE value;
      GetDouble(surface, FID_Height, &value);
      ReleaseObject(surface);

      if (Value->Type & FD_DOUBLE) Value->Double = value;
      else if (Value->Type & FD_LARGE) Value->Large = value;
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

static ERROR SET_Height(objMenuBar *Self, struct Variable *Value)
{
   if (((Value->Type & FD_DOUBLE) AND (!Value->Double)) OR
       ((Value->Type & FD_LARGE) AND (!Value->Large))) {
      return ERR_Okay;
   }

   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      SetVariable(surface, FID_Height, Value);
      ReleaseObject(surface);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FIELD-
Highlight: The colour to use for highlights.

-FIELD-
HighlightFlags: Defines the method to use for highlighting menu items.

The following flags determine the method that will be used when highlighting a menu item (highlighting occurs when
the user moves the mouse over one of the items).  The default is LIGHT_BKGD.

-FIELD-
LeftMargin: Offsets the menu items against the left side of the menubar.

The LeftMargin affects the amount of white space that is created at the left side of the menubar on initialisation.
Values between 3 and 6 are typical.

-FIELD-
Path: Identifies the location of the menu configuration file to load.

To configure the menubar using a configuration file, set the path of the file here.  The file must be in XML
format and contain embedded menu tags that are to be interpreted as items positioned across the menu bar.  The
configuration of the menu tags must match the requirements outlined in the Menu class.

Alternative options to using menu definition files include passing the menu tags as XML, using the data channel
system.

*****************************************************************************/

static ERROR GET_Path(objMenuBar *Self, STRING *Value)
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

static ERROR SET_Path(objMenuBar *Self, CSTRING Value)
{
   if (Self->Path) { FreeMemory(Self->Path); Self->Path = NULL; }
   if ((Value) AND (*Value)) Self->Path = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
MenuStyle: The name of a custom style to be applied to each menu.

The graphics style to use for the individual @Menu objects can be defined here.  The style name
will be written to the Style field of each Menu object prior to their initialisation.

*****************************************************************************/

static ERROR GET_MenuStyle(objMenuBar *Self, STRING *Value)
{
   *Value = Self->MenuStyle;
   return ERR_Okay;
}

static ERROR SET_MenuStyle(objMenuBar *Self, CSTRING Value)
{
   if (Self->MenuStyle) { FreeMemory(Self->MenuStyle); Self->MenuStyle = NULL; }
   if ((Value) AND (*Value)) Self->MenuStyle = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Region: The surface that represents the menubar graphics area.

-FIELD-
Right: The right coordinate of the menubar (X + Width).

*****************************************************************************/

static ERROR GET_Right(objMenuBar *Self, LONG *Value)
{
   LONG x, width;
   if (!drwGetSurfaceCoords(Self->RegionID, &x, NULL, NULL, NULL, &width, NULL)) {
      *Value = x + width;
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FIELD-
RightMargin: Offsets the menu items against the right side of the menubar.

The RightMargin affects the amount of white space that is created at the right side of the menubar on initialisation.
Values between 3 and 6 are typical.

-FIELD-
Shadow: Defines the colour of shadows.

-FIELD-
Surface: The surface that will contain the menubar graphic.

The surface that will contain the menubar graphic is set here.  If this field is not set prior to initialisation, the
menubar will attempt to scan for the correct surface by analysing its parents until it finds a suitable candidate.

-FIELD-
Target: The surface on which menus should be opened.

This field declares the surface that should be used to contain any menus that are created.  It is typically set to the
'desktop' object for application windows that are contained in the desktop area.

-FIELD-
TopMargin: Defines empty white-space at the top of the menubar.  A value between 3 and 6 is recommended.

-FIELD-
Total: The total number of items in the menubar object.

-FIELD-
Width: Defines the width of the menubar.

A menubar can be given a fixed or relative width by setting this field to the desired value.  To set a relative width,
use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Width(objMenuBar *Self, struct Variable *Value)
{
   OBJECTPTR surface;

   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      DOUBLE value;
      GetDouble(surface, FID_Width, &value);
      ReleaseObject(surface);

      if (Value->Type & FD_DOUBLE) Value->Double = value;
      else if (Value->Type & FD_LARGE) Value->Large = value;
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

static ERROR SET_Width(objMenuBar *Self, struct Variable *Value)
{
   if (((Value->Type & FD_DOUBLE) AND (!Value->Double)) OR ((Value->Type & FD_LARGE) AND (!Value->Large))) {
      return ERR_Okay;
   }

   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      SetVariable(surface, FID_Width, Value);
      ReleaseObject(surface);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FIELD-
X: The horizontal position of the menubar.

The horizontal position of the menubar can be set to an absolute or relative coordinate by writing a value to the X
field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be interpreted as
fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_X(objMenuBar *Self, struct Variable *Value)
{
   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      DOUBLE value;
      GetDouble(surface, FID_X, &value);
      ReleaseObject(surface);

      if (Value->Type & FD_DOUBLE) Value->Double = value;
      else if (Value->Type & FD_LARGE) Value->Large = value;
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

static ERROR SET_X(objMenuBar *Self, struct Variable *Value)
{
   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      SetVariable(surface, FID_X, Value);
      ReleaseObject(surface);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FIELD-
XOffset: The horizontal offset of the menubar.

The XOffset has a dual purpose depending on whether or not it is set in conjunction with an X coordinate or a Width
based field.

If set in conjunction with an X coordinate then the menubar will be drawn from that X coordinate up to the width of the
container, minus the value given in the XOffset.  This means that the width of the MenuBar is dynamically calculated in
relation to the width of the container.

If the XOffset field is set in conjunction with a fixed or relative width then the menubar will be drawn at an X
coordinate calculated from the formula `X = ContainerWidth - MenuBarWidth - XOffset`.

*****************************************************************************/

static ERROR GET_XOffset(objMenuBar *Self, struct Variable *Value)
{
   OBJECTPTR surface;

   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      DOUBLE value;
      GetDouble(surface, FID_XOffset, &value);
      ReleaseObject(surface);

      if (Value->Type & FD_DOUBLE) Value->Double = value;
      else if (Value->Type & FD_LARGE) Value->Large = value;
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

static ERROR SET_XOffset(objMenuBar *Self, struct Variable *Value)
{
   OBJECTPTR surface;

   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      SetVariable(surface, FID_XOffset, Value);
      ReleaseObject(surface);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FIELD-
Y: The vertical position of the menubar.

The vertical position of a MenuBar can be set to an absolute or relative coordinate by writing a value to the Y
field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be interpreted as
fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_Y(objMenuBar *Self, struct Variable *Value)
{
   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      DOUBLE value;
      GetDouble(surface, FID_Y, &value);
      ReleaseObject(surface);

      if (Value->Type & FD_DOUBLE) Value->Double = value;
      else if (Value->Type & FD_LARGE) Value->Large = value;
      return ERR_Okay;
   }
   else return ERR_AccessObject;

}

static ERROR SET_Y(objMenuBar *Self, struct Variable *Value)
{
   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      SetVariable(surface, FID_Y, Value);
      ReleaseObject(surface);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FIELD-
YOffset: The vertical offset of the menubar.

The YOffset has a dual purpose depending on whether or not it is set in conjunction with a Y coordinate or a Height
based field.

If set in conjunction with a Y coordinate then the menubar will be drawn from that Y coordinate up to the height of the
container, minus the value given in the YOffset.  This means that the height of the menubar is dynamically calculated
in relation to the height of the container.

If the YOffset field is set in conjunction with a fixed or relative height then the menubar will be drawn at a Y
coordinate calculated from the formula `Y = ContainerHeight - MenuBarHeight - YOffset`.
-END-

*****************************************************************************/

static ERROR GET_YOffset(objMenuBar *Self, struct Variable *Value)
{
   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      DOUBLE value;
      GetDouble(surface, FID_YOffset, &value);
      ReleaseObject(surface);

      if (Value->Type & FD_DOUBLE) Value->Double = value;
      else if (Value->Type & FD_LARGE) Value->Large = value;
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

static ERROR SET_YOffset(objMenuBar *Self, struct Variable *Value)
{
   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      SetVariable(surface, FID_YOffset, Value);
      ReleaseObject(surface);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

//****************************************************************************

static void draw_menubar(objMenuBar *Self, objSurface *Surface, objBitmap *Bitmap)
{
   objFont *font = Self->Font;
   font->Bitmap = Bitmap;

   LONG x = Self->LeftMargin;
   LONG index;
   for (index=0; index < Self->Total; index++) {
      // Draw background graphic for selected/highlighted items

      if ((index IS Self->Index) AND (!(Surface->Flags & RNF_DISABLED)) AND
          (!(Self->Items[index].Flags & TIF_DISABLED))){

         if (Self->HighlightFlags & MHG_LIGHT_BKGD) {
            gfxDrawRectangle(Bitmap, x, Self->TopMargin, Self->Items[index].Width, Surface->Height - Self->BottomMargin, PackPixelA(Bitmap, 255, 255, 255, 64), BAF_FILL|BAF_BLEND);
         }
         else if (Self->HighlightFlags & MHG_DARK_BKGD) {
            gfxDrawRectangle(Bitmap, x, Self->TopMargin, Self->Items[index].Width, Surface->Height - Self->BottomMargin, PackPixelA(Bitmap, 255, 255, 255, 96), BAF_FILL|BAF_BLEND);
         }

         if (Self->HighlightFlags & (MHG_BORDER|MHG_RAISED|MHG_SUNKEN)) {
            ULONG highlight, shadow;
            if (Self->HighlightFlags & MHG_BORDER) {
               shadow    = PackPixelRGBA(Bitmap, &Self->Shadow);
               highlight = PackPixelRGBA(Bitmap, &Self->Highlight);
            }
            else if (Self->HighlightFlags & MHG_RAISED) {
               highlight = PackPixelRGBA(Bitmap, &Self->Shadow);
               shadow    = PackPixelRGBA(Bitmap, &Self->Highlight);
            }
            else if (Self->HighlightFlags & MHG_SUNKEN) {
               highlight = PackPixelRGBA(Bitmap, &Self->Highlight);
               shadow    = highlight;
            }
            else {
               highlight = 0;
               shadow    = 0;
            }

            gfxDrawRectangle(Bitmap, x, Self->TopMargin, Self->Items[index].Width, 1, highlight, BAF_FILL|BAF_BLEND); // top
            gfxDrawRectangle(Bitmap, x, Surface->Height-Self->BottomMargin-1, Self->Items[index].Width, 1, shadow, BAF_FILL|BAF_BLEND); // bottom
            gfxDrawRectangle(Bitmap, x, Self->TopMargin + 1, 1, Surface->Height-Self->BottomMargin-2, highlight, BAF_FILL|BAF_BLEND); // left
            gfxDrawRectangle(Bitmap, x + Self->Items[index].Width-1, 1, 1, Surface->Height-Self->BottomMargin-2, shadow, BAF_FILL|BAF_BLEND); // right
         }
      }

      // Draw menu item

      Self->Items[index].X = x;

      x += Self->Gap;

      if (Self->Items[index].Picture) {
         objBitmap *srcbitmap = Self->Items[index].Picture->Bitmap;

         WORD opacity = Bitmap->Opacity;
         if ((Surface->Flags & RNF_DISABLED) OR (Self->Items[index].Flags & TIF_DISABLED)) srcbitmap->Opacity = 128;

         gfxCopyArea(srcbitmap, Bitmap, BAF_BLEND, 0, 0, srcbitmap->Width, srcbitmap->Height,
            x, ((Surface->Height - srcbitmap->Height)/2));

         srcbitmap->Opacity = opacity;

         x += srcbitmap->Width;
         if (Self->Items[index].Name[0]) x += 4;
      }

      if (Self->Items[index].Name[0]) {
         SetString(font, FID_String, Self->Items[index].Translation);

         if ((Surface->Flags & RNF_DISABLED) OR (Self->Items[index].Flags & TIF_DISABLED)) SetLong(font, FID_Opacity, 25);

         font->X = x;
         font->Y = 0;
         font->Align = ALIGN_VERTICAL;
         font->AlignHeight = Surface->Height;

         if ((Self->HighlightFlags & MHG_TEXT) AND (index IS Self->Index) AND (!(Surface->Flags & RNF_DISABLED))) {
            struct RGB8 rgb = font->Colour;
            font->Colour = Self->Highlight;
            acDraw(font);
            font->Colour = rgb;
         }
         else acDraw(font);

         if ((Surface->Flags & RNF_DISABLED) OR (Self->Items[index].Flags & TIF_DISABLED)) SetLong(font, FID_Opacity, 0);

         LONG strwidth;
         GetLong(font, FID_Width, &strwidth);
         x += strwidth;
      }

      x += Self->Gap;

      Self->Items[index].Width = x - Self->Items[index].X;
   }

   if (Self->Flags & MBF_BREAK) {
      gfxDrawRectangle(Bitmap, 0, Surface->Height-1, Surface->Width, 1, PackPixelA(Bitmap, 0, 0, 0, 128), BAF_FILL|BAF_BLEND);
   }
}

/*****************************************************************************
** Executes a draw for the item at a specific location.
*/

static void draw_item(objMenuBar *Self, WORD Index)
{
   if ((Index >= 0) AND (Index < Self->Total)) {
      acDrawAreaID(Self->RegionID, Self->Items[Index].X, 0, Self->Items[Index].Width, 0);
   }
}

/*****************************************************************************
** Search for <menu> tags and use them to create menu items.
*/

static void process_xml(objMenuBar *Self, objXML *XML)
{
   struct XMLTag *tag;

   for (tag=XML->Tags[0]; tag; tag=tag->Next) {
      STRING objectname = NULL;
      if (!StrMatch("menu", tag->Attrib[0].Name)) {
         struct mbAddMenu add;
         ClearMemory(&add, sizeof(add));

         LONG n;
         for (n=0; n < tag->TotalAttrib; n++) {
            if (!StrMatch("text", tag->Attrib[n].Name))         add.Name = tag->Attrib[n].Value;
            else if (!StrMatch("name", tag->Attrib[n].Name))    add.Name = tag->Attrib[n].Value;
            else if (!StrMatch("icon", tag->Attrib[n].Name))    add.Icon = tag->Attrib[n].Value;
            else if (!StrMatch("picture", tag->Attrib[n].Name)) add.Icon = tag->Attrib[n].Value;
            else if (!StrMatch("objectname", tag->Attrib[n].Name)) objectname = tag->Attrib[n].Value;
         }

         if (tag->Child) {
            STRING script;
            xmlGetString(XML, tag->Child->Index, XMF_INCLUDE_SIBLINGS, &script);
            add.Script = script;
         }

         if (!Action(MT_MbAddMenu, Self, &add)) {
            if ((add.Menu) AND (objectname)) SetName(add.Menu, objectname);
         }

         if (add.Script) FreeMemory(add.Script);
      }
   }
}

/*****************************************************************************
** Opens the menu item at a specific index.
*/

static void activate_item(objMenuBar *Self, LONG Index)
{
   if ((Index < 0) OR (Index >= Self->Total)) return;

   FMSG("~activate_item()","Index: %d", Index);

   SetLong(Self->Items[Index].Menu, FID_X, Self->Items[Index].X);
   if (!acActivate(Self->Items[Index].Menu)) {
      if ((Self->LastMenu) AND (Self->LastMenu != Self->Items[Index].Menu)) {
         acHide(Self->LastMenu);
      }
      Self->LastMenu = Self->Items[Index].Menu;
   }

   STEP();
}

//****************************************************************************

static void open_menu(objMenuBar *Self, LONG Index)
{
   LogF("~open_menu()","Index: %d", Index);

   if ((Index >= 0) AND (Index < Self->Total)) {
      if (!(Self->Items[Index].Flags & TIF_DISABLED)) {
         draw_item(Self, Index);
         activate_item(Self, Index);
      }
   }

   LogBack();
}

//****************************************************************************

#include "class_menubar_def.c"

static const struct FieldArray clMenuBarFields[] = {
   { "Region",         FDF_OBJECTID|FDF_RW,  ID_SURFACE,   NULL, NULL },
   { "Surface",        FDF_OBJECTID|FDF_RW,  ID_SURFACE,   NULL, NULL },
   { "Target",         FDF_OBJECTID|FDF_RI,  ID_SURFACE,   NULL, NULL },
   { "Flags",          FDF_LONGFLAGS|FDF_RW, (MAXINT)&clMenuBarFlags, NULL, NULL },
   { "Font",           FDF_INTEGRAL|FDF_R,   0, NULL, NULL },
   { "Total",          FDF_LONG|FDF_R,       0, NULL, NULL },
   { "HighlightFlags", FDF_LONGFLAGS|FDF_RW, (MAXINT)&clMenuBarHighlightFlags, NULL, NULL },
   { "LeftMargin",     FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "RightMargin",    FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Gap",            FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "TopMargin",      FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "BottomMargin",   FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Highlight",      FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "Shadow",         FDF_RGB|FDF_RW,       0, NULL, NULL },
   // Virtual fields
   { "Bottom",       FDF_LONG|FDF_R,    0, GET_Bottom, NULL },
   { "ItemFeedback", FDF_FUNCTIONPTR|FDF_RW, 0, GET_ItemFeedback, SET_ItemFeedback },
   { "MenuStyle",    FDF_STRING|FDF_RW, 0, GET_MenuStyle, SET_MenuStyle },
   { "Path",         FDF_STRING|FDF_RW, 0, GET_Path, SET_Path },
   { "Right",        FDF_LONG|FDF_R,    0, GET_Right, NULL },
   { "Location",     FDF_SYNONYM|FDF_STRING|FDF_RW, 0, GET_Path, SET_Path },
   // Variable Fields
   { "Height",  FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Height,  SET_Height },
   { "Width",   FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Width,   SET_Width },
   { "X",       FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_X, SET_X },
   { "XOffset", FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_XOffset, SET_XOffset },
   { "Y",       FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Y, SET_Y },
   { "YOffset", FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_YOffset, SET_YOffset },
   END_FIELD
};

//****************************************************************************

ERROR init_menubar(void)
{
   return(CreateObject(ID_METACLASS, 0, &clMenuBar,
      FID_ClassVersion|TFLOAT, VER_MENUBAR,
      FID_Name|TSTRING,   "MenuBar",
      FID_Category|TLONG, CCF_GUI,
      FID_Flags|TLONG,    CLF_PROMOTE_INTEGRAL|CLF_PRIVATE_ONLY,
      FID_Actions|TPTR,   clMenuBarActions,
      FID_Methods|TARRAY, clMenuBarMethods,
      FID_Fields|TARRAY,  clMenuBarFields,
      FID_Size|TLONG,     sizeof(objMenuBar),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND));
}
