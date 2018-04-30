/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
ComboBox: The ComboBox class manages the display and interaction of user combo boxes.

The ComboBox class is used to create combo boxes, also known as 'drop-down menus' in application interfaces.  A
combobox typically looks like a text entry area, but features a button positioned to the right-hand side of the gadget.
Clicking on the button will pop-up a menu that the user can use to select a pre-defined menu item.  Clicking on one of
those items will paste the item text into the combobox.

A crucial feature of the combobox is the drop-down menu.  The combobox uses the @Menu class to support its menu
construction. To add items to the drop-down menu, you need to pass instructions to it using XML.  You can learn more
about the XML specification in the @Menu class manual.

When the user selects a combobox item, you may need to respond with an action.  You can do this by initialising child
objects to the combobox. These will be executed when the combobox is activated.  When programming, you can also
subscribe to the combobox's Activate action and write a customised response routine.

The id of the most recently selected menu item can be retrieved from the #SelectedID field.

To make modifications to the menu after initialisation, read the #Menu field and manipulate it directly.

-END-

*****************************************************************************/

#define PRV_COMBOBOX
#include <parasol/modules/widget.h>
#include <parasol/modules/document.h>
#include <parasol/modules/picture.h>
#include <parasol/modules/display.h>
#include <parasol/modules/font.h>
#include <parasol/modules/surface.h>
#include "defs.h"

#define MIN_MENU_WIDTH 120

static OBJECTPTR clCombobox = NULL;

static void draw_combobox(objComboBox *, objSurface *, objBitmap *);
static void text_validation(objText *);
static void text_activated(objText *);

//****************************************************************************

static ERROR COMBOBOX_ActionNotify(objComboBox *Self, struct acActionNotify *Args)
{
   if (Args->Error != ERR_Okay) return ERR_Okay;

   if ((Args->ActionID IS AC_Activate) AND (Args->ObjectID IS Self->Menu->Head.UniqueID)) {
      objMenuItem *item;
      STRING current;
      if ((!GetPointer(Self->Menu, FID_Selection, &item)) AND (item)) {
         if (GetString(Self->TextInput, FID_String, &current)) current = NULL;

         if (StrMatch(item->Text, current) != ERR_Okay) {
            SetString(Self, FID_String, item->Text);
            acActivate(Self);
         }
      }
      else LogErrorMsg("No item selected.");
   }
   else if (Args->ActionID IS AC_Redimension) {
      struct acRedimension *redimension = (struct acRedimension *)Args->Args;
      SetLong(Self->Menu, FID_Width, F2T(redimension->Width) - Self->LabelWidth);
   }
   else if (Args->ActionID IS AC_Disable) {
      Self->Flags |= CMF_DISABLED;
      DelayMsg(AC_Draw, Self->RegionID, NULL);
   }
   else if (Args->ActionID IS AC_Enable) {
      Self->Flags &= ~CMF_DISABLED;
      DelayMsg(AC_Draw, Self->RegionID, NULL);
   }
   else if (Args->ActionID IS AC_LostFocus) {
      acHide(Self->Menu);
   }
   else if (Args->ActionID IS AC_Free) {
      if ((Self->Feedback.Type IS CALL_SCRIPT) AND (Self->Feedback.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->Feedback.Type = CALL_NONE;
      }
   }
   else return ERR_NoSupport;

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Clear: Clears the content of the combobox list box.
-END-
*****************************************************************************/

static ERROR COMBOBOX_Clear(objComboBox *Self, APTR Void)
{
   acClear(Self->Menu);
   return ERR_Okay;
}

//****************************************************************************

static ERROR COMBOBOX_DataFeed(objComboBox *Self, struct acDataFeed *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   if (Args->DataType IS DATA_XML) {
      Action(AC_DataFeed, Self->Menu, Args); // This is for passing <item>'s to the menu.
   }
   else if (Args->DataType IS DATA_INPUT_READY) {
      struct InputMsg *input;

      while (!gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &input)) {
         if ((input->Type IS JET_LMB) AND (input->Value > 0)) {
            if (input->OverID IS Self->ButtonID) {
               // The button on the combobox has been pressed, so switch the menu visibility.
               // About 300ms must pass before a switch can occur.
               mnSwitch(Self->Menu, 200);
            }
            else if (input->X >= Self->LabelWidth) {
               if (!(Self->TextInput->Flags & TXF_EDIT)) {
                  mnSwitch(Self->Menu, 5);
               }
            }
         }
      }
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Disable: Turns the combobox off.
-END-
*****************************************************************************/

static ERROR COMBOBOX_Disable(objComboBox *Self, APTR Void)
{
   // See the ActionNotify routine to see what happens when the surface is disabled.
   LogAction(NULL);
   return acDisableID(Self->RegionID);
}

/*****************************************************************************
-ACTION-
Enable: Turns the combobox back on if it has previously been disabled.
-END-
*****************************************************************************/

static ERROR COMBOBOX_Enable(objComboBox *Self, APTR Void)
{
   // See the ActionNotify routine to see what happens when the surface is enabled.
   LogAction(NULL);
   return acEnableID(Self->RegionID);
}

/*****************************************************************************
-ACTION-
Focus: Sets the focus on the combobox.
-END-
*****************************************************************************/

static ERROR COMBOBOX_Focus(objComboBox *Self, APTR Void)
{
   if (Self->Flags & CMF_EDIT) acFocusID(Self->RegionID);
   else acFocusID(Self->ButtonID);

   return ERR_Okay;
}

//****************************************************************************

static ERROR COMBOBOX_Free(objComboBox *Self, APTR Void)
{
   if (Self->ButtonID)  { acFreeID(Self->ButtonID); Self->ButtonID = 0; }
   if (Self->Font)      { acFree(Self->Font); Self->Font = NULL; }
   if (Self->TextInput) { acFree(Self->TextInput); Self->TextInput = NULL; }
   if (Self->Menu)      { acFree(Self->Menu); Self->Menu = NULL; }
   if (Self->RegionID)  { acFreeID(Self->RegionID); Self->RegionID = 0; }
   gfxUnsubscribeInput(0);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Hide: Removes the combobox from the display.
-END-
*****************************************************************************/

static ERROR COMBOBOX_Hide(objComboBox *Self, APTR Void)
{
   return acHideID(Self->RegionID);
}

//****************************************************************************

static ERROR COMBOBOX_Init(objComboBox *Self, APTR Void)
{
   if (!Self->SurfaceID) { // Find our parent surface
      OBJECTID owner_id = GetOwner(Self);
      while ((owner_id) AND (GetClassID(owner_id) != ID_SURFACE)) {
         owner_id = GetOwnerID(owner_id);
      }
      if (owner_id) Self->SurfaceID = owner_id;
      else return PostError(ERR_UnsupportedOwner);
   }

   if (acInit(Self->Font) != ERR_Okay) return PostError(ERR_Init);

   if ((Self->LabelWidth < 1) AND (Self->Label[0])) {  // Calculate the width of the text label, if there is one
      Self->LabelWidth = fntStringWidth(Self->Font, Self->Label, -1) + 4;
   }

   objSurface *region;
   LONG region_height, region_width;
   UBYTE calc_width = FALSE;
   if (!AccessObject(Self->RegionID, 5000, &region)) { // Initialise the combobox region
      SetFields(region, FID_Parent|TLONG, Self->SurfaceID,
                        FID_Region|TLONG, TRUE,
                        TAGEND);

      region->Flags |= RNF_GRAB_FOCUS;

      if (!(region->Dimensions & DMF_HEIGHT)) {
         if ((!(region->Dimensions & DMF_Y)) OR (!(region->Dimensions & DMF_Y_OFFSET))) {
            LONG h = Self->Font->MaxHeight + (Self->Thickness*2) + Self->TextInput->Layout->TopMargin +
               Self->TextInput->Layout->BottomMargin;
            SetLong(region, FID_Height, h);
         }
      }

      if (!(region->Dimensions & DMF_WIDTH)) {
         if ((!(region->Dimensions & DMF_X)) OR (!(region->Dimensions & DMF_X_OFFSET))) {
            calc_width = TRUE;
         }
      }

      if (!acInit(region)) {
         SubscribeActionTags(region,
            AC_Disable,
            AC_Enable,
            AC_LostFocus,
            AC_Redimension,
            TAGEND);

         region_width  = region->Width;
         region_height = region->Height;
      }
      else {
         ReleaseObject(region);
         return PostError(ERR_Init);
      }

      ReleaseObject(region);
   }
   else return PostError(ERR_AccessObject);

   // Initialise the text area that the user will be able to interact with.

   LONG flags = 0;
   if (Self->Flags & CMF_EDIT) flags |= TXF_EDIT;

   SetFields(Self->TextInput,
      FID_Flags|TLONG,   flags,
      FID_Face|TSTR,     Self->Font->Face,
      FID_Point|TDOUBLE, (DOUBLE)Self->Font->Point,
      FID_X|TLONG,       Self->LabelWidth + Self->Thickness,
      FID_Y|TLONG,       Self->Thickness,
      FID_XOffset|TLONG, Self->Thickness,
      FID_YOffset|TLONG, Self->Thickness,
      FID_TopMargin|TLONG,    0,
      FID_BottomMargin|TLONG, 0,
      TAGEND);

   SetFunctionPtr(Self->TextInput, FID_ValidateInput, &text_validation);
   SetFunctionPtr(Self->TextInput, FID_Activated, &text_activated);

   if (!(Self->Flags & CMF_NO_TRANSLATION)) {
      STRING str;
      GetString(Self->TextInput, FID_String, &str);

      CSTRING translate;
      if ((translate = StrTranslateText(str)) != str) {
         SetString(Self->TextInput, FID_String, translate);
      }
   }

   SetFields(Self->Menu,
      FID_Relative|TLONG,  Self->RegionID,
      FID_X|TLONG,         Self->LabelWidth,
      FID_Y|TLONG,         region_height - 1,
      FID_VSpacing|TLONG,  4,
      FID_Face|TSTR,       Self->Font->Face,
      FID_Point|TDOUBLE,   Self->Font->Point,
      FID_Flags|TLONG,     MNF_IGNORE_FOCUS |
                           ((Self->Flags & CMF_NO_TRANSLATION) ? MNF_NO_TRANSLATION : 0) |
                           ((Self->Flags & CMF_SHOW_ICONS) ? MNF_SHOW_IMAGES : 0),
      FID_LineLimit|TLONG, 8,
      TAGEND);

   SubscribeAction(Self->Menu, AC_Activate);

   if (!calc_width) SetLong(Self->Menu, FID_Width, region_width - Self->LabelWidth);

   if (!drwApplyStyleGraphics(Self, Self->RegionID, NULL, NULL)) {
      Self->Flags |= CMF_NO_BKGD;

      if (!Self->ButtonID) { // Scan for a button object
         struct ChildEntry list[16];
         LONG count = ARRAYSIZE(list);
         if (!ListChildren(Self->RegionID, list, &count)) {
            WORD i;
            for (i=0; i < count; i++) {
               if (list[i].ClassID IS ID_BUTTON) {
                  Self->ButtonID = list[i].ObjectID;
                  break;
               }
            }
         }
      }
   }

   if (!AccessObject(Self->RegionID, 5000, &region)) {
      drwAddCallback(region, &draw_combobox);
      ReleaseObject(region);
   }
   else return ERR_AccessObject;

   if (acInit(Self->TextInput)) return ERR_Init;
   if (acInit(Self->Menu)) return ERR_Init;

   ERROR error = ERR_Okay;
   if (Self->ButtonID) {
      // Sometimes a button can be user-defined through the graphics script (the developer simply sets the button field
      // with a valid object.  The object in question does not necessarily have to be a true button - it can be
      // anything - although it is typically best for it to be a true Button object.

      OBJECTPTR button;
      if (!AccessObject(Self->ButtonID, 4000, &button)) {
         SubscribeActionTags(button, AC_Activate, TAGEND);
         ReleaseObject(button);
      }
   }
   else {
      objSurface *button;
      if (!NewLockedObject(ID_SURFACE, 0, &button, &Self->ButtonID)) {
         SetFields(button,
            FID_Owner|TLONG,   Self->RegionID,
            FID_XOffset|TLONG, 0,
            FID_Y|TLONG,       0,
            FID_YOffset|TLONG, 0,
            FID_Width|TLONG,   region_height,
            TAGEND);

         if (!acInit(button)) {
            gfxSubscribeInput(Self->ButtonID, JTYPE_BUTTON, 0);

            char icon[40];
            StrFormat(icon, sizeof(icon), "icons:arrows/down(%d)", (LONG)((DOUBLE)region_height * 0.6));
            if (!CreateObject(ID_IMAGE, 0, NULL,
                  FID_Owner|TLONG,     Self->ButtonID,
                  FID_Align|TLONG,     ALIGN_CENTER,
                  FID_IconFilter|TSTR, "pearl",
                  FID_Path|TSTR,       icon,
                  TAGEND)) {
               acShow(button);
               error = ERR_Okay;
            }
            else error = ERR_CreateObject;
         }
         else error = ERR_Init;

         ReleaseObject(button);
      }
      else error = ERR_CreateObject;
   }

   if (!error) {
      if (calc_width) {
         objSurface *region;
         if (!AccessObject(Self->RegionID, 3000, &region)) {
            LONG menuwidth;
            GetLong(Self->Menu, FID_Width, &menuwidth);
            menuwidth += region->Height + 4;
            if (menuwidth > 200) menuwidth = 200;
            if (Self->LabelWidth + menuwidth > MIN_MENU_WIDTH) {
               SetLong(region, FID_Width, Self->LabelWidth + menuwidth);
            }
            else SetLong(region, FID_Width, Self->LabelWidth + MIN_MENU_WIDTH);
            ReleaseObject(region);
         }
      }

      if (!(Self->Flags & CMF_HIDE)) acShow(Self);
   }

   return error;
}

/*****************************************************************************
-ACTION-
MoveToBack: Moves the combobox behind its siblings.
-END-
*****************************************************************************/

static ERROR COMBOBOX_MoveToBack(objComboBox *Self, APTR Void)
{
   return acMoveToBackID(Self->RegionID);
}

/*****************************************************************************
-ACTION-
MoveToFront: Moves the combobox in front of its siblings.
-END-
*****************************************************************************/

static ERROR COMBOBOX_MoveToFront(objComboBox *Self, APTR Void)
{
   return acMoveToFrontID(Self->RegionID);
}

//****************************************************************************

static ERROR COMBOBOX_NewObject(objComboBox *Self, APTR Void)
{
   if (!NewLockedObject(ID_SURFACE, NF_INTEGRAL, NULL, &Self->RegionID)) {
      if (!NewObject(ID_FONT, NF_INTEGRAL, &Self->Font)) {
         if (!NewObject(ID_TEXT, NF_INTEGRAL, &Self->TextInput)) {
            if (!NewObject(ID_MENU, NF_INTEGRAL, &Self->Menu)) {
               SetString(Self->Font, FID_Face, glLabelFace);

               SetLong(Self->TextInput, FID_Surface, Self->RegionID);
               SetString(Self->TextInput->Font, FID_Face, glWidgetFace);
               Self->TextInput->LineLimit = 1;
               Self->TextInput->Layout->LeftMargin   = 3;
               Self->TextInput->Layout->RightMargin  = 3;
               Self->TextInput->Layout->TopMargin    = 2;
               Self->TextInput->Layout->BottomMargin = 2;

               SetLong(Self->TextInput, FID_Align, ALIGN_VERTICAL);

               Self->ReleaseFrame = 1;

               // Internal colour
               Self->Colour.Red   = 0;
               Self->Colour.Green = 255;
               Self->Colour.Blue  = 255;
               Self->Colour.Alpha = 255;

               // Shadow colour
               Self->Shadow.Red   = 100;
               Self->Shadow.Green = 100;
               Self->Shadow.Blue  = 100;
               Self->Shadow.Alpha = 255;

               // Highlight colour
               Self->Highlight.Red   = 255;
               Self->Highlight.Green = 255;
               Self->Highlight.Blue  = 255;
               Self->Highlight.Alpha = 255;

               Self->Thickness = 1;
               drwApplyStyleValues(Self, NULL);
               return ERR_Okay;
            }
            else return ERR_NewObject;
         }
         else return ERR_NewObject;
      }
      else return ERR_NewObject;
   }
   else return ERR_NewObject;
}

/*****************************************************************************
-ACTION-
Redimension: Changes the size and position of the combobox.
-END-
*****************************************************************************/

static ERROR COMBOBOX_Redimension(objComboBox *Self, struct acRedimension *Args)
{
   return ActionMsg(AC_Redimension, Self->RegionID, Args);
}

/*****************************************************************************
-ACTION-
Resize: Alters the size of the combobox.
-END-
*****************************************************************************/

static ERROR COMBOBOX_Resize(objComboBox *Self, struct acResize *Args)
{
   return ActionMsg(AC_Resize, Self->RegionID, Args);
}

/*****************************************************************************
-ACTION-
SetVar: Arguments can be passed through to the combobox menu via unlisted fields.
-END-
*****************************************************************************/

static ERROR COMBOBOX_SetVar(objComboBox *Self, struct acSetVar *Args)
{
   return Action(AC_SetVar, Self->Menu, Args);
}

/*****************************************************************************
-ACTION-
Show: Puts the combobox on display.
-END-
*****************************************************************************/

static ERROR COMBOBOX_Show(objComboBox *Self, APTR Void)
{
   return acShowID(Self->RegionID);
}

/*****************************************************************************

-FIELD-
Align: Manages the alignment of a combobox surface within its container.

The position of a combobox object can be abstractly defined with alignment instructions by setting this field.  The
alignment feature takes precedence over values in coordinate fields such as #X and #Y.

*****************************************************************************/

static ERROR SET_Align(objComboBox *Self, LONG Value)
{
   objSurface *surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      surface->Align = Value;
      ReleaseObject(surface);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FIELD-
Border: String-based field for setting a single-colour border for the combobox.

The border colour for a combobox can be declared by writing to this field. The colour must be in hexadecimal or
separated-decimal format - for example to create a pure red colour, a setting of "#ff0000" or "255,0,0" would be valid.

*****************************************************************************/

static ERROR SET_Border(objComboBox *Self, CSTRING Colour)
{
   if (Colour) {
      StrToColour(Colour, &Self->Shadow);
      Self->Highlight = Self->Shadow;
   }
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Bottom: The bottom coordinate of the combobox (Y + Height).

*****************************************************************************/

static ERROR GET_Bottom(objComboBox *Self, LONG *Value)
{
   OBJECTPTR surface;

   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      GetLong(surface, FID_Bottom, Value);
      ReleaseObject(surface);
      return ERR_Okay;
   }
   else return PostError(ERR_AccessObject);
}

/*****************************************************************************

-FIELD-
Button: Refers to the button that controls the combobox list box.

A combobox widget will always show a button that displays the combobox list box when clicked.  This field refers to the
@Button that controls this functionality.

-FIELD-
Colour: The fill colour to use in the combobox.

-FIELD-
Disable: Disables the combobox on initialisation.

The combobox can be disabled on initialisation by setting this field to TRUE.  If you need to disable the combobox
after it has been activated, it is preferred that you use the Disable action.

To enable the combobox after it has been disabled, use the Enable action.

*****************************************************************************/

static ERROR GET_Disable(objComboBox *Self, LONG *Value)
{
   if (Self->Flags & CMF_DISABLED) *Value = TRUE;
   else *Value = FALSE;
   return ERR_Okay;
}

static ERROR SET_Disable(objComboBox *Self, LONG Value)
{
   if (Value IS TRUE) return acDisable(Self);
   else return acEnable(Self);
}

/*****************************************************************************

-FIELD-
Feedback: Provides instant feedback when a user interacts with the Combobox.

Set the Feedback field with a callback function in order to receive instant feedback when user interaction occurs.  The
function prototype is `routine(*ComboBox)`

*****************************************************************************/

static ERROR GET_Feedback(objComboBox *Self, FUNCTION **Value)
{
   if (Self->Feedback.Type != CALL_NONE) {
      *Value = &Self->Feedback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Feedback(objComboBox *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->Feedback.Type IS CALL_SCRIPT) UnsubscribeAction(Self->Feedback.Script.Script, AC_Free);
      Self->Feedback = *Value;
      if (Self->Feedback.Type IS CALL_SCRIPT) SubscribeAction(Self->Feedback.Script.Script, AC_Free);
   }
   else Self->Feedback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Flags: Optional flags may be defined here.

-FIELD-
FocusFrame: The graphics frame to display when the combobox has the focus.

This field specifies the surface frame to switch to when the user focusses on the combobox.  The default value is zero,
which has no effect on the surface frame.  When the user leaves the combobox, it will revert to the frame indicated by
the ReleaseFrame field.

-FIELD-
Font: The font used to draw the combobox label.

The font object that is used to draw the combobox label string is referenced from this field.  Fields in the font
object, such as the font face and colour can be set prior to initialisation.

-FIELD-
Height: Defines the height of the combobox.

An combobox can be given a fixed or relative height by setting this field to the desired value.  To set a relative
height, use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Height(objComboBox *Self, struct Variable *Value)
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
   else return PostError(ERR_AccessObject);
}

static ERROR SET_Height(objComboBox *Self, struct Variable *Value)
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
Highlight: Defines the border colour for highlighting.

-FIELD-
Label: The label is a string displayed to the left of the combobox area.

A label can be drawn next to the combobox area by setting the Label field.  The label should be a short, descriptive
string of one or two words.  It is common practice for the label to be followed with a colon character.

*****************************************************************************/

static ERROR GET_Label(objComboBox *Self, STRING *Value)
{
   *Value = Self->Label;
   return ERR_Okay;
}

static ERROR SET_Label(objComboBox *Self, CSTRING Value)
{
   if (Value) StrCopy(StrTranslateText(Value), Self->Label, sizeof(Self->Label));
   else Self->Label[0] = 0;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
LabelWidth: A set-width for the label area of the combobox may be defined here.

If you set a label for the combobox, the width of the label area is automatically calculated according to the width of
the label string.  You may override this behaviour by setting a value in the LabelWidth field.

*****************************************************************************/

// Internal field for supporting dynamic style changes when an object is used in a document.

static ERROR SET_LayoutStyle(objComboBox *Self, DOCSTYLE *Value)
{
   if (!Value) return ERR_Okay;

   if (Self->Head.Flags & NF_INITIALISED) docApplyFontStyle(Value->Document, Value, Self->Font);
   else docApplyFontStyle(Value->Document, Value, Self->Font);

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Region: The surface that represents the combobox is referenced through this field.

The surface area that represents the combobox display can be accessed through this field.  For further information,
refer to the Surface class.  Note that interfacing with the surface directly can have adverse effects on the combobox
control system.  Where possible, all communication should be limited to the combobox object itself.

*****************************************************************************/

static ERROR SET_Region(objComboBox *Self, LONG Value)
{
   // NOTE: For backwards compatibility with the Surface class, the region can be set to a value of TRUE to define the
   // combobox as a simple surface region.

   if ((Value IS FALSE) OR (Value IS TRUE)) {
      OBJECTPTR surface;
      if (!AccessObject(Self->RegionID, 4000, &surface)) {
         SetLong(surface, FID_Region, Value);
         ReleaseObject(surface);
         return ERR_Okay;
      }
      else return ERR_AccessObject;
   }
   else return PostError(ERR_InvalidValue);
}

/*****************************************************************************

-FIELD-
Menu: Provides direct access to the drop-down menu.

The drop-down menu that is used for the combobox can be accessed directly through this field.  You may find this useful
for manipulating the content of the drop-down menu following initialisation of the combobox.

-FIELD-
ReleaseFrame: The graphics frame to display when the combobox loses the focus.

If the FocusFrame field has been set, you may want to match that value by indicating the frame that should be used when
the click is released. By default, the value in this field will initially be set to 1.  This field is unused if the
FocusFrame field has not been set.

-FIELD-
Right: The right-most coordinate of the combobox (X + Width).

*****************************************************************************/

static ERROR GET_Right(objComboBox *Self, LONG *Value)
{
   OBJECTPTR surface;
   if (!AccessObject(Self->RegionID, 4000, &surface)) {
      GetLong(surface, FID_Right, Value);
      ReleaseObject(surface);
      return ERR_Okay;
   }
   else return PostError(ERR_AccessObject);
}

/*****************************************************************************

-FIELD-
SelectedID: Returns the menu ID of the selected combobox item.

This field returns the menu ID of the selected combobox item.  This requires that an id is set for each configured menu
item (the 'id' attribute).

Menu ID's are not guaranteed to be unique.  It is your responsibility to assign ID's and ensure that they are unique to
prevent an ID from matching multiple items.

If the combobox text does not reflect one of the available menu items, then the returned value will be -1.  If the
selected menu item has no identifier, the default return value is 0.

*****************************************************************************/

static ERROR GET_SelectedID(objComboBox *Self, LONG *Value)
{
   *Value = -1;

   STRING str;
   if (!GetString(Self->TextInput, FID_String, &str)) {
      objMenuItem *item;
      for (item=Self->Menu->Items; item; item=item->Next) {
         if (!StrMatch(str, item->Text)) {
            *Value = item->ID;
            return ERR_Okay;
         }
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Shadow: Defines the border colour for shadows.

-FIELD-
String: The string that is to be printed inside the combobox is declared here.

The string that you would like to be displayed in the combobox is specified in this field.  The string must be in UTF-8
format and may not contain line feeds.  You can read this field at any time to determine what the user has entered in
the combobox.

If the string is changed after initialisation, the combobox will be redrawn to show the updated text.  No feedback
notification will be sent as a result of updating this field manually.

*****************************************************************************/

static ERROR GET_String(objComboBox *Self, STRING *Value)
{
   STRING str;
   if (!GetString(Self->TextInput, FID_String, &str)) {
      *Value = str;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_String(objComboBox *Self, CSTRING Value)
{
   // Do nothing if the string will remain unchanged

   STRING original;
   if ((!GetString(Self->TextInput, FID_String, &original)) AND (original)) {
      if (!StrMatch(original, Value)) return ERR_Okay;
   }

   if (!SetString(Self->TextInput, FID_String, Value)) return ERR_Okay;
   else return ERR_Failed;
}

/*****************************************************************************

-FIELD-
Surface: The surface that will contain the combobox graphic.

The surface that will contain the combobox graphic is set here.  If this field is not set prior to initialisation, the
combobox will attempt to scan for the correct surface by analysing its parents until it finds a suitable candidate.

-FIELD-
TabFocus: Set this field to a TabFocus object to register the combobox in a tab-list.

The TabFocus field provides a convenient way of adding the combobox to a @TabFocus object, so that it can be
focussed on via the tab key.  Simply set this field to the ID of the @TabFocus object that is managing the
tab-list for the application window.

*****************************************************************************/

static ERROR SET_TabFocus(objComboBox *Self, OBJECTID Value)
{
   OBJECTPTR tabfocus;
   if (!AccessObject(Value, 5000, &tabfocus)) {
      if (tabfocus->ClassID IS ID_TABFOCUS) {
         tabAddObject(tabfocus, Self->Head.UniqueID);
      }
      ReleaseObject(tabfocus);
   }
   else return ERR_AccessObject;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
TextInput: The text control object created for the combobox is referenced here.

The text object that is referenced here manages the display and editing of text inside the combobox area.
Characteristics of the text object can be defined prior to initialisation, although we recommend that this be done from
the combobox style definition.

The face and point size of the text is derived from the #Font field on initialisation and therefore cannot be
changed through the TextInput object directly.

-FIELD-
Thickness: The thickness of the combobox border, in pixels.

-FIELD-
Width: Defines the width of a combobox.

A combobox can be given a fixed or relative width by setting this field to the desired value.  To set a relative width,
use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Width(objComboBox *Self, struct Variable *Value)
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
   else return PostError(ERR_AccessObject);
}

static ERROR SET_Width(objComboBox *Self, struct Variable *Value)
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
X: The horizontal position of a combobox.

The horizontal position of a combobox can be set to an absolute or relative coordinate by writing a value to the X
field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be interpreted as
fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_X(objComboBox *Self, struct Variable *Value)
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
   else return PostError(ERR_AccessObject);
}

static ERROR SET_X(objComboBox *Self, struct Variable *Value)
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
XOffset: The horizontal offset of a combobox.

The XOffset has a dual purpose depending on whether or not it is set in conjunction with an X coordinate or a Width
based field.

If set in conjunction with an X coordinate then the combobox will be drawn from that X coordinate up to the width of
the container, minus the value given in the XOffset.  This means that the width of the ComboBox is dynamically
calculated in relation to the width of the container.

If the XOffset field is set in conjunction with a fixed or relative width then the combobox will be drawn at an X
coordinate calculated from the formula `X = ContainerWidth - ComboBoxWidth - XOffset`.

*****************************************************************************/

static ERROR GET_XOffset(objComboBox *Self, struct Variable *Value)
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
   else return PostError(ERR_AccessObject);
}

static ERROR SET_XOffset(objComboBox *Self, struct Variable *Value)
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
Y: The vertical position of a combobox.

The vertical position of a ComboBox can be set to an absolute or relative coordinate by writing a value to the Y
field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be interpreted as
fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_Y(objComboBox *Self, struct Variable *Value)
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
   else return PostError(ERR_AccessObject);

}

static ERROR SET_Y(objComboBox *Self, struct Variable *Value)
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
YOffset: The vertical offset of a combobox.

The YOffset has a dual purpose depending on whether or not it is set in conjunction with a Y coordinate or a Height
based field.

If set in conjunction with a Y coordinate then the combobox will be drawn from that Y coordinate up to the height of
the container, minus the value given in the YOffset.  This means that the height of the combobox is dynamically
calculated in relation to the height of the container.

If the YOffset field is set in conjunction with a fixed or relative height then the combobox will be drawn at a Y
coordinate calculated from the formula `Y = ContainerHeight - ComboBoxHeight - YOffset`.
-END-

*****************************************************************************/

static ERROR GET_YOffset(objComboBox *Self, struct Variable *Value)
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
   else return PostError(ERR_AccessObject);
}

static ERROR SET_YOffset(objComboBox *Self, struct Variable *Value)
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

static void draw_combobox(objComboBox *Self, objSurface *Surface, objBitmap *Bitmap)
{
   if (!(Self->Flags & CMF_NO_BKGD)) {
      gfxDrawRectangle(Bitmap, Self->LabelWidth, 0, Surface->Width - Self->LabelWidth, Surface->Height,
         PackPixelRGBA(Bitmap, &Self->Colour), BAF_FILL|BAF_BLEND);

      // Draw the borders around the rectangular area

      ULONG highlight, shadow;
      if (Self->Flags & INF_SUNKEN) {
         // Reverse the border definitions in sunken mode
         highlight = PackPixelRGBA(Bitmap, &Self->Shadow);
         shadow = PackPixelRGBA(Bitmap, &Self->Highlight);
      }
      else {
         shadow = PackPixelRGBA(Bitmap, &Self->Shadow);
         highlight = PackPixelRGBA(Bitmap, &Self->Highlight);
      }

      LONG x = Self->LabelWidth;
      LONG width = Surface->Width - Self->LabelWidth;

      WORD i;
      for (i=0; i < Self->Thickness; i++) {
         // Top, Bottom
         gfxDrawRectangle(Bitmap, x+i, i, width-i-i, 1, highlight, BAF_FILL|BAF_BLEND);
         gfxDrawRectangle(Bitmap, x+i, Surface->Height-i-1, width-i-i, 1, shadow, BAF_FILL|BAF_BLEND);

         // Left, Right
         gfxDrawRectangle(Bitmap, x+i, i+1, 1, Surface->Height-i-i-2, highlight, BAF_FILL|BAF_BLEND);
         gfxDrawRectangle(Bitmap, x+width-i-1, i+1, 1, Surface->Height-i-i-2, shadow, BAF_FILL|BAF_BLEND);
      }
   }

   if (Self->Label[0]) {
      objFont *font = Self->Font;
      font->Bitmap = Bitmap;

      SetString(font, FID_String, Self->Label);

      if (Surface->Flags & RNF_DISABLED) SetLong(font, FID_Opacity, 25);

      font->X = 0;
      font->Y = 0;
      font->Flags |= FTF_CHAR_CLIP;
      font->WrapEdge = Self->LabelWidth - 3;
      font->Align |= ALIGN_VERTICAL;
      font->AlignWidth  = Surface->Width;
      font->AlignHeight = Surface->Height;
      acDraw(font);

      if (Surface->Flags & RNF_DISABLED) SetLong(font, FID_Opacity, 100);
   }
}

//**********************************************************************
// This callback is triggered when the user moves focus away from the text widget.

static void text_validation(objText *Text)
{
   objInput *Self = (objInput *)CurrentContext();

   if (Self->Flags & CMF_LIMIT_TO_LIST) {

   }
}

//**********************************************************************
// This callback is triggered when the user hits the enter key, or its equivalent.

static void text_activated(objText *Text)
{
   objComboBox *Self = (objComboBox *)CurrentContext();

   if (Self->Active) {
      LogErrorMsg("Warning - recursion detected");
      return;
   }

   LogBranch(NULL);

   Self->Active = TRUE;

   STRING value;
   GetString(Self->TextInput, FID_String, &value);

   if (Self->Feedback.Type IS CALL_STDC) {
      void (*routine)(objComboBox *, CSTRING) = Self->Feedback.StdC.Routine;
      if (Self->Feedback.StdC.Context) {
         OBJECTPTR context = SetContext(Self->Feedback.StdC.Context);
         routine(Self, value);
         SetContext(context);
      }
      else routine(Self, value);
   }
   else if (Self->Feedback.Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Self->Feedback.Script.Script)) {
         const struct ScriptArg args[] = {
            { "ComboBox", FD_OBJECTPTR, { .Address = Self } },
            { "Value",    FD_STRING, { .Address = value } }
         };
         scCallback(script, Self->Feedback.Script.ProcedureID, args, ARRAYSIZE(args));
      }
   }

   Self->Active = FALSE;

   LogBack();
}

//****************************************************************************

#include "class_combobox_def.c"

static const struct FieldDef Align[] = {
   { "Right",    ALIGN_RIGHT    }, { "Left",       ALIGN_LEFT    },
   { "Bottom",   ALIGN_BOTTOM   }, { "Top",        ALIGN_TOP     },
   { "Center",   ALIGN_CENTER   }, { "Middle",     ALIGN_MIDDLE  },
   { "Vertical", ALIGN_VERTICAL }, { "Horizontal", ALIGN_HORIZONTAL },
   { NULL, 0 }
};

static const struct FieldArray clFields[] = {
   { "Font",         FDF_INTEGRAL|FDF_R,      0, NULL, NULL },
   { "TextInput",    FDF_INTEGRAL|FDF_R,      0, NULL, NULL },
   { "Menu",         FDF_INTEGRAL|FDF_R,      0, NULL, NULL },
   { "LayoutSurface",FDF_VIRTUAL|FDF_OBJECTID|FDF_SYSTEM|FDF_R, ID_SURFACE, NULL, NULL }, // VIRTUAL: This is a synonym for the Region field
   { "Region",       FDF_OBJECTID|FDF_RW,  ID_SURFACE, NULL, SET_Region },
   { "Surface",      FDF_OBJECTID|FDF_RW,  ID_SURFACE, NULL, NULL },
   { "Button",       FDF_OBJECTID|FDF_RI,  0, NULL, NULL },
   { "Flags",        FDF_LONGFLAGS|FDF_RW, (MAXINT)&clComboBoxFlags, NULL, NULL },
   { "FocusFrame",   FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "ReleaseFrame", FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Thickness",    FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "LabelWidth",   FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "Colour",       FDF_RGB|FDF_RI,       0, NULL, NULL },
   { "Highlight",    FDF_RGB|FDF_RI,       0, NULL, NULL },
   { "Shadow",       FDF_RGB|FDF_RI,       0, NULL, NULL },
   // Virtual fields
   { "Align",         FDF_VIRTUAL|FDF_LONGFLAGS|FDF_I, (MAXINT)&Align,  NULL, SET_Align },
   { "Border",        FDF_VIRTUAL|FDF_STRING|FDF_W,    0, NULL, SET_Border },
   { "Bottom",        FDF_VIRTUAL|FDF_LONG|FDF_R,      0, GET_Bottom, NULL },
   { "Disable",       FDF_VIRTUAL|FDF_LONG|FDF_RW,     0, GET_Disable, SET_Disable },
   { "Feedback",      FDF_VIRTUAL|FDF_FUNCTIONPTR|FDF_RW, 0, GET_Feedback, SET_Feedback },
   { "Label",         FDF_VIRTUAL|FDF_STRING|FDF_RW,   0, GET_Label, SET_Label },
   { "LayoutStyle",   FDF_VIRTUAL|FDF_POINTER|FDF_SYSTEM|FDF_W, 0, NULL, SET_LayoutStyle },
   { "Right",         FDF_VIRTUAL|FDF_LONG|FDF_R,      0, GET_Right, NULL },
   { "SelectedID",    FDF_VIRTUAL|FDF_LONG|FDF_R,      0, GET_SelectedID, NULL },
   { "String",        FDF_VIRTUAL|FDF_STRING|FDF_RW,   0, GET_String, SET_String },
   { "TabFocus",      FDF_VIRTUAL|FDF_OBJECTID|FDF_I,  ID_TABFOCUS, NULL, SET_TabFocus },
   { "Text",          FDF_SYNONYM|FDF_VIRTUAL|FDF_STRING|FDF_RW, 0, GET_String, SET_String },
   // Variable Fields
   { "Height",  FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Height,  SET_Height },
   { "Width",   FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Width,   SET_Width },
   { "X",       FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_X,       SET_X },
   { "XOffset", FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_XOffset, SET_XOffset },
   { "Y",       FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Y,       SET_Y },
   { "YOffset", FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_YOffset, SET_YOffset },
   END_FIELD
};

//****************************************************************************

ERROR init_combobox(void)
{
   return(CreateObject(ID_METACLASS, 0, &clCombobox,
      FID_ClassVersion|TFLOAT, VER_COMBOBOX,
      FID_Name|TSTRING,   "ComboBox",
      FID_Category|TLONG, CCF_GUI,
      FID_Flags|TLONG,    CLF_PROMOTE_INTEGRAL,
      FID_Actions|TPTR,   clComboBoxActions,
      FID_Fields|TARRAY,  clFields,
      FID_Size|TLONG,     sizeof(objComboBox),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND));
}

void free_combobox(void)
{
   if (clCombobox) { acFree(clCombobox); clCombobox = NULL; }
}
