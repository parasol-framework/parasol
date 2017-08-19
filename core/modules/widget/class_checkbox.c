/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
CheckBox: The CheckBox class manages check boxes in the user interface.

The CheckBox class simplifies the creation and management of checkbox widgets in the user interface.  Check boxes are
simple widgets that are limited to exhibiting an on/off state.  The CheckBox class allows for its graphics to be
customised, so it is possible to redefine how the on/off states are displayed.

The standard mechanism for configuring a response to changing state in a CheckBox object is to set the #Feedback field
with a callback function.
-END-

*****************************************************************************/

#define PRV_CHECKBOX
#include <parasol/modules/document.h>
#include <parasol/modules/picture.h>
#include <parasol/modules/display.h>
#include <parasol/modules/font.h>
#include <parasol/modules/surface.h>
#include <parasol/modules/widget.h>
#include "defs.h"

static OBJECTPTR clCheckBox = NULL;

static const struct FieldDef Align[] = {
   { "Right",      ALIGN_RIGHT      }, { "Left",     ALIGN_LEFT },
   { "Bottom",     ALIGN_BOTTOM     }, { "Top",      ALIGN_TOP },
   { "Horizontal", ALIGN_HORIZONTAL }, { "Vertical", ALIGN_VERTICAL },
   { "Center",     ALIGN_CENTER     }, { "Middle",   ALIGN_MIDDLE },
   { NULL, 0 }
};

static const struct FieldArray clFields[];

static void draw_checkbox(objCheckBox *, objSurface *, objBitmap *);
static void key_event(objCheckBox *, evKey *, LONG);
static ERROR SET_CheckImage(objCheckBox *, CSTRING);

//****************************************************************************

static ERROR CHECKBOX_ActionNotify(objCheckBox *Self, struct acActionNotify *Args)
{
   if (Args->ActionID IS AC_Focus) {
      if (!Self->prvKeyEvent) {
         FUNCTION callback;
         SET_FUNCTION_STDC(callback, &key_event);
         SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, &callback, Self, &Self->prvKeyEvent);
      }

      DelayMsg(AC_Draw, Self->RegionID, NULL);
   }
   else if (Args->ActionID IS AC_LostFocus) {
      if (Self->prvKeyEvent) { UnsubscribeEvent(Self->prvKeyEvent); Self->prvKeyEvent = NULL; }

      DelayMsg(AC_Draw, Self->RegionID, NULL);
   }
   else if (Args->ActionID IS AC_Disable) {
      Self->Flags |= CBF_DISABLED;
      DelayMsg(AC_Draw, Self->RegionID, NULL);
   }
   else if (Args->ActionID IS AC_Enable) {
      Self->Flags &= ~CBF_DISABLED;
      DelayMsg(AC_Draw, Self->RegionID, NULL);
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
Activate: Activates the checkbox.
-END-
*****************************************************************************/

static ERROR CHECKBOX_Activate(objCheckBox *Self, APTR Void)
{
   LogBranch(NULL);

   if (Self->Active) {
      LogErrorMsg("Warning - recursion detected");
      LogBack();
      return ERR_Failed;
   }

   Self->Active = TRUE;

   SURFACEINFO *info;
   if (!drwGetSurfaceInfo(Self->RegionID, &info)) {
      if (!(info->Flags & RNF_DISABLED)) {
         Self->Value ^= 1;

         acDrawID(Self->RegionID);

         if (Self->Feedback.Type IS CALL_STDC) {
            void (*routine)(OBJECTPTR Context, objCheckBox *, LONG);
            routine = Self->Feedback.StdC.Routine;

            if (Self->Feedback.StdC.Context) {
               OBJECTPTR context = SetContext(Self->Feedback.StdC.Context);
               routine(Self->Feedback.StdC.Context, Self, Self->Value);
               SetContext(context);
            }
            else routine(Self->Feedback.StdC.Context, Self, Self->Value);
         }
         else if (Self->Feedback.Type IS CALL_SCRIPT) {
            OBJECTPTR script;
            if ((script = Self->Feedback.Script.Script)) {
               const struct ScriptArg args[] = {
                  { "CheckBox", FD_OBJECTPTR, { .Address = Self } },
                  { "State", FD_LONG, { .Long = Self->Value } }
               };
               scCallback(script, Self->Feedback.Script.ProcedureID, args, ARRAYSIZE(args));
            }
         }

         struct ChildEntry list[16];
         LONG count = ARRAYSIZE(list);
         if (!ListChildren(Self->Head.UniqueID, list, &count)) {
            WORD i;
            for (i=0; i < count; i++) DelayMsg(AC_Activate, list[i].ObjectID, NULL);
         }
      }
   }

   Self->Active = FALSE;

   LogBack();
   return ERR_Okay;
}

//****************************************************************************

static ERROR CHECKBOX_DataFeed(objCheckBox *Self, struct acDataFeed *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   if (Args->DataType IS DATA_INPUT_READY) {
      struct InputMsg *input;

      while (!gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &input)) {
         if ((input->Type IS JET_LMB) AND (input->Value > 0)) {
            if (Self->Flags & CBF_DISABLED) continue;
            if (input->X < Self->LabelWidth) continue;

            if (Self->ClickFrame) {
               OBJECTPTR surface;
               if (!AccessObject(Self->RegionID, 3000, &surface)) {
                  SetLong(surface, FID_Frame, Self->ClickFrame);
                  ReleaseObject(surface);
               }
            }

            acActivate(Self);
         }
         else if (input->Type IS JET_ENTERED_SURFACE) {
            Self->Entered = TRUE;

            if (!(Self->Flags & CBF_DISABLED)) {
               OBJECTPTR surface;
               if (!AccessObject(Self->RegionID, 2000, &surface)) {
                  SetLong(surface, FID_Frame, Self->EnterFrame);
                  DelayMsg(AC_Draw, Self->RegionID, NULL);
                  ReleaseObject(surface);
               }
            }
         }
         else if (input->Type IS JET_LEFT_SURFACE) {
            Self->Entered = FALSE;

            if (!(Self->Flags & CBF_DISABLED)) {
               OBJECTPTR surface;
               if (!AccessObject(Self->RegionID, 2000, &surface)) {
                  SetLong(surface, FID_Frame, Self->ExitFrame);
                  DelayMsg(AC_Draw, Self->RegionID, NULL);
                  ReleaseObject(surface);
               }
            }
         }
         else MSG("Unrecognised input message type $%.8x", input->Type);
      }
   }
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Disable: Disables the checkbox.
-END-
*****************************************************************************/

static ERROR CHECKBOX_Disable(objCheckBox *Self, APTR Void)
{
   // See the ActionNotify routine to see what happens when the surface is disabled.

   acDisableID(Self->RegionID);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Enable: Turns the checkbox on if it has been disabled.
-END-
*****************************************************************************/

static ERROR CHECKBOX_Enable(objCheckBox *Self, APTR Void)
{
   // See the ActionNotify routine to see what happens when the surface is enabled.
   acEnableID(Self->RegionID);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Focus: Sets the focus on the checkbox and activates keyboard monitoring.
-END-
*****************************************************************************/

static ERROR CHECKBOX_Focus(objCheckBox *Self, APTR Void)
{
   return acFocusID(Self->RegionID);
}

//****************************************************************************

static ERROR CHECKBOX_Free(objCheckBox *Self, APTR Void)
{
   if (Self->prvKeyEvent) { UnsubscribeEvent(Self->prvKeyEvent); Self->prvKeyEvent = NULL; }
   if (Self->Picture)  { acFree(Self->Picture); Self->Picture = NULL; }
   if (Self->Font)     { acFree(Self->Font); Self->Font = NULL; }
   if (Self->RegionID) { acFreeID(Self->RegionID); Self->RegionID = 0; }
   gfxUnsubscribeInput(0);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Hide: Removes the checkbox from the display.
-END-
*****************************************************************************/

static ERROR CHECKBOX_Hide(objCheckBox *Self, APTR Void)
{
   Self->Flags |= CBF_HIDE;
   acHideID(Self->RegionID);
   return ERR_Okay;
}

//****************************************************************************

static ERROR CHECKBOX_Init(objCheckBox *Self, APTR Void)
{
   if (!Self->SurfaceID) { // Find the parent surface
      OBJECTID owner_id = GetOwner(Self);
      while ((owner_id) AND (GetClassID(owner_id) != ID_SURFACE)) {
         owner_id = GetOwnerID(owner_id);
      }
      if (owner_id) Self->SurfaceID = owner_id;
      else return PostError(ERR_UnsupportedOwner);
   }

   if (acInit(Self->Font) != ERR_Okay) return PostError(ERR_Init);

   if ((Self->LabelWidth < 1) AND (Self->Label[0])) { // Calculate the width of the text label, if there is one
      Self->LabelWidth = fntStringWidth(Self->Font, Self->Label, -1) + 8;
   }

   objSurface *region;
   if (!AccessObject(Self->RegionID, 5000, &region)) { // Initialise the checkbox region
      SetFields(region, FID_Parent|TLONG, Self->SurfaceID,
                        FID_Region|TLONG, TRUE,
                        TAGEND);

      if (!(region->Dimensions & DMF_HEIGHT)) {
         if ((!(region->Dimensions & DMF_Y)) OR (!(region->Dimensions & DMF_Y_OFFSET))) {
            SetLong(region, FID_Height, 18);
         }
      }

      if (!(region->Dimensions & DMF_WIDTH)) {
         if ((!(region->Dimensions & DMF_X)) OR (!(region->Dimensions & DMF_X_OFFSET))) {
            SetLong(region, FID_Width, Self->LabelWidth + region->Height);
         }
      }

      region->Flags |= RNF_GRAB_FOCUS;

      if (!acInit(region)) {
         SubscribeActionTags(region,
            AC_Disable,
            AC_Enable,
            AC_Focus,
            AC_LostFocus,
            TAGEND);
      }

      gfxSubscribeInput(Self->RegionID, JTYPE_FEEDBACK|JTYPE_BUTTON, 0);

      ReleaseObject(region);
   }
   else return ERR_AccessObject;

   if (!(Self->Flags & CBF_NO_BKGD)) { // Use the base template to create the checkbox graphics
      if (!drwApplyStyleGraphics(Self, Self->RegionID, NULL, NULL)) {
         Self->Flags |= CBF_NO_BKGD;
      }
   }

   if (!AccessObject(Self->RegionID, 5000, &region)) {
      drwAddCallback(region, &draw_checkbox);
      ReleaseObject(region);
   }
   else return ERR_AccessObject;

   if (!Self->Picture) { // Load the picture to be displayed when the checkbox is ticked
      SET_CheckImage(Self, "icons:items/checkmark(16)");
   }

   if (!(Self->Flags & CBF_HIDE)) acShow(Self);

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToBack: Moves the checkbox to the back of the display area.
-END-
*****************************************************************************/

static ERROR CHECKBOX_MoveToBack(objCheckBox *Self, APTR Void)
{
   acMoveToBackID(Self->RegionID);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToFront: Moves the checkbox to the front of the display area.
-END-
*****************************************************************************/

static ERROR CHECKBOX_MoveToFront(objCheckBox *Self, APTR Void)
{
   acMoveToFrontID(Self->RegionID);
   return ERR_Okay;
}

//****************************************************************************

static ERROR CHECKBOX_NewObject(objCheckBox *Self, APTR Void)
{
   if (!NewLockedObject(ID_SURFACE, Self->Head.Flags|NF_INTEGRAL, NULL, &Self->RegionID)) {
      if (!NewObject(ID_FONT, Self->Head.Flags|NF_INTEGRAL, &Self->Font)) {
         SetString(Self->Font, FID_Face, glDefaultFace);

         Self->ExitFrame = 1;
         Self->ReleaseFrame = 1;

         // Shadow colour
         Self->Shadow.Red   = 100;
         Self->Shadow.Green = 100;
         Self->Shadow.Blue  = 100;
         Self->Shadow.Alpha = 255;

         // Internal colour
         Self->Colour.Red   = 255;
         Self->Colour.Green = 255;
         Self->Colour.Blue  = 255;
         Self->Colour.Alpha = 255;

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

/*****************************************************************************
-ACTION-
Redimension: Changes the size and position of the checkbox.
-END-
*****************************************************************************/

static ERROR CHECKBOX_Redimension(objCheckBox *Self, struct acRedimension *Args)
{
   return ActionMsg(AC_Redimension, Self->RegionID, Args);
}

/*****************************************************************************
-ACTION-
Resize: Alters the size of the checkbox.
-END-
*****************************************************************************/

static ERROR CHECKBOX_Resize(objCheckBox *Self, struct acResize *Args)
{
   return ActionMsg(AC_Resize, Self->RegionID, Args);
}

/*****************************************************************************
-ACTION-
Show: Make the checkbox visible.
-END-
*****************************************************************************/

static ERROR CHECKBOX_Show(objCheckBox *Self, APTR Void)
{
   Self->Flags &= ~CBF_HIDE;
   acShowID(Self->RegionID);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Align: Affects the alignment of the checkbox widget within its target surface.

By default the checkbox widget will be aligned to the top left of its target surface.  The checkbox can be aligned to
the right by setting the ALIGN_RIGHT flag.

-FIELD-
Border: String-based field for setting a single-colour border for the checkbox.

The border colour for a checkbox can be declared by writing to this field.  The colour must be in hexadecimal or
separated-decimal format - for example to create a pure red colour, a setting of "#ff0000" or "255,0,0" would be
valid.

*****************************************************************************/

static ERROR SET_Border(objCheckBox *Self, CSTRING Colour)
{
   SetString(Self, FID_Highlight, Colour);
   Self->Shadow = Self->Highlight;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Bottom: The bottom coordinate of the checkbox (Y + Height).

*****************************************************************************/

static ERROR GET_Bottom(objCheckBox *Self, LONG *Value)
{
   LONG y, height;
   if (!drwGetSurfaceCoords(Self->RegionID, NULL, &y, NULL, NULL, NULL, &height)) {
      *Value = y + height;
      return ERR_Okay;
   }
   else return ERR_GetSurfaceInfo;
}

/*****************************************************************************

-FIELD-
CheckImage: Defines the image that is displayed when the checkbox state is set to 'on'.

When the checkbox state is set to 'on', it will display an image inside the checkbox area.  The default image can be
redefined by setting CheckImage with a path to an image file.  If the image file cannot be loaded for any reason, the
checkbox will revert to its default image on initialisation.

*****************************************************************************/

static ERROR SET_CheckImage(objCheckBox *Self, CSTRING Value)
{
   if (!CreateObject(ID_PICTURE, Self->Head.Flags|NF_INTEGRAL, &Self->Picture,
         FID_Path|TSTR,   Value,
         FID_Flags|TLONG, PCF_FORCE_ALPHA_32,
         TAGEND)) {
      return ERR_Okay;
   }
   else return ERR_CreateObject;
}

/*****************************************************************************

-FIELD-
Colour: Defines the colour inside of the checkbox.

-FIELD-
ClickFrame: The graphics frame to display when the checkbox is clicked.

This field specifies the surface frame to switch to when the user clicks on the checkbox.  By default this field is
initially set to zero, which has no effect on the surface frame.  When the user releases the checkbox, it will revert
to the frame indicated by the #ReleaseFrame field.

-FIELD-
Disable: Disables the checkbox on initialisation.

The checkbox can be disabled on initialisation by setting this field to TRUE.  If you need to disable the combobox
after it has been activated, it is preferred that you use the Disable action.

To enable the combobox after it has been disabled, use the Enable action.

*****************************************************************************/

static ERROR GET_Disable(objCheckBox *Self, LONG *Value)
{
   if (Self->Flags & CBF_DISABLED) *Value = TRUE;
   else *Value = FALSE;
   return ERR_Okay;
}

static ERROR SET_Disable(objCheckBox *Self, LONG Value)
{
   if (Value IS TRUE) acDisable(Self);
   else acEnable(Self);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
EnterFrame: The graphics frame to display when the user's pointer enters the checkbox area.

This field specifies the surface frame to switch to when the user's pointer enters the checkbox area.  By default
this field is initially set to zero, which has no effect on the surface frame.

-FIELD-
ExitFrame: The graphics frame to display when the user's pointer leaves the checkbox area.

This field specifies the surface frame to switch to when the user's pointer leaves the checkbox area.  This field is
unused if the EnterFrame field has not been set.

-FIELD-
Feedback: Provides instant feedback when a user interacts with the checkbox.

Set the Feedback field with a callback function in order to receive instant feedback when user interaction occurs.  The
function prototype is `routine(*CheckBox, LONG State)`

*****************************************************************************/

static ERROR GET_Feedback(objCheckBox *Self, FUNCTION **Value)
{
   if (Self->Feedback.Type != CALL_NONE) {
      *Value = &Self->Feedback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Feedback(objCheckBox *Self, FUNCTION *Value)
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
Flags: Optional flags.
Lookup: CBF

-FIELD-
Font: The font used to draw the checkbox label.

The font object that is used to draw the checkbox label string can be referenced from this field.  Fields in the font
object, such as the font face and colour can be set prior to initialisation.

-FIELD-
Height: Defines the height of a checkbox.

A checkbox can be given a fixed or relative height by setting this field to the desired value.  To set a relative
height, use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Height(objCheckBox *Self, struct Variable *Value)
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

static ERROR SET_Height(objCheckBox *Self, struct Variable *Value)
{
   if (((Value->Type & FD_DOUBLE) AND (!Value->Double)) OR ((Value->Type & FD_LARGE) AND (!Value->Large))) {
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
Highlight: Defines the checkbox highlight colour.

-FIELD-
Label: The label is a string displayed to the left of the input area.

A label can be drawn next to the input area by setting the Label field.  The label should be a short, descriptive
string of one or two words.  It is common practice for the label to be followed with a colon character.

*****************************************************************************/

static ERROR GET_Label(objCheckBox *Self, STRING *Value)
{
   *Value = Self->Label;
   return ERR_Okay;
}

static ERROR SET_Label(objCheckBox *Self, CSTRING Value)
{
   if (Value) StrCopy(StrTranslateText(Value), Self->Label, sizeof(Self->Label));
   else Self->Label[0] = 0;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
LabelWidth: The fixed pixel width allocated for drawing the label string.

If a label is assigned to a checkbox, the width of the label will be calculated on initialisation and the value will be
readable from this field.  It is also possible to set the LabelWidth prior to initialisation, in which case the label
string will be restricted to the space available.

*****************************************************************************/

// Internal field for supporting dynamic style changes when a GUI object is used in a document.

static ERROR SET_LayoutStyle(objCheckBox *Self, DOCSTYLE *Value)
{
   if (!Value) return ERR_Okay;

   if (Self->Head.Flags & NF_INITIALISED) docApplyFontStyle(Value->Document, Value, Self->Font);
   else docApplyFontStyle(Value->Document, Value, Self->Font);

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Region: The surface that represents the checkbox is referenced here.

The drawable area that represents the checkbox display can be accessed through this field.  For further information,
refer to the @Surface class.  Note that talking to the surface directly can have adverse effects on the
checkbox control system.  Where possible, all communication should be limited to the checkbox object itself.

-FIELD-
ReleaseFrame: The graphics frame to display when a user-click is released.

If the #ClickFrame field has been set, you may want to match that value by indicating the frame that should be
used when the click is released.  By default, the value in this field will initially be set to 1.  This field is unused
if the #ClickFrame field has not been set.

-FIELD-
Right: The right coordinate of the checkbox (X + Width).

*****************************************************************************/

static ERROR GET_Right(objCheckBox *Self, LONG *Value)
{
   LONG x, width;
   if (!drwGetSurfaceCoords(Self->RegionID, &x, NULL, NULL, NULL, &width, NULL)) {
      *Value = x + width;
      return ERR_Okay;
   }
   else return ERR_GetSurfaceInfo;
}

/*****************************************************************************

-FIELD-
Shadow: Defines the colour of the checkbox border shadow.

-FIELD-
Surface: The surface that will represent the checkbox widget.

The surface that will contain the checkbox widget is set here.  If this field is not set prior to initialisation, the
checkbox will attempt to scan for the correct surface by analysing its parents until it finds a suitable candidate.

-FIELD-
TabFocus: Set this field to a TabFocus object to register the checkbox in a tab-list.

The TabFocus field provides a convenient way of adding the checkbox to a TabFocus object, so that it can be focussed on
via the tab key.  Simply set this field to the ID of the TabFocus object that is managing the tab-list for the
application window.

*****************************************************************************/

static ERROR SET_TabFocus(objCheckBox *Self, OBJECTID Value)
{
   OBJECTPTR tabfocus;
   if (!AccessObject(Value, 5000, &tabfocus)) {
      if (tabfocus->ClassID IS ID_TABFOCUS) {
         tabAddObject(tabfocus, Self->RegionID);
      }
      ReleaseObject(tabfocus);
   }
   else return ERR_AccessObject;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Thickness: The thickness of the checkbox border.

-FIELD-
Value: Indicates the current on/off state of the checkbox.

To get the on/off state of the checkbox, read this field.  It can also be set at run-time to force the checkbox into
an on or off state.  Only values of 0 (off) and 1 (on) are valid.

*****************************************************************************/

static ERROR GET_Value(objCheckBox *Self, LONG *Value)
{
   *Value = Self->Value;
   return ERR_Okay;
}

static ERROR SET_Value(objCheckBox *Self, LONG Value)
{
   if (Self->Head.Flags & NF_INITIALISED) {
      if ((Value IS TRUE) AND (Self->Value != TRUE)) {
         Self->Value = TRUE;
         acDrawID(Self->RegionID);
      }
      else if ((Value IS FALSE) AND (Self->Value != FALSE)) {
         Self->Value = FALSE;
         acDrawID(Self->RegionID);
      }
   }
   else Self->Value = Value;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Width: Defines the width of a checkbox.

A checkbox can be given a fixed or relative width by setting this field to the desired value.  To set a relative width,
use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Width(objCheckBox *Self, struct Variable *Value)
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

static ERROR SET_Width(objCheckBox *Self, struct Variable *Value)
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
X: The horizontal position of a checkbox.

The horizontal position of a checkbox can be set to an absolute or relative coordinate by writing a value to the X
field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be interpreted as
fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_X(objCheckBox *Self, struct Variable *Value)
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

static ERROR SET_X(objCheckBox *Self, struct Variable *Value)
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
XOffset: The horizontal offset of a checkbox.

The XOffset has a dual purpose depending on whether or not it is set in conjunction with an X coordinate or a Width
based field.

If set in conjunction with an X coordinate then the checkbox will be drawn from that X coordinate up to the width of
the container, minus the value given in the XOffset.  This means that the width of the CheckBox is dynamically
calculated in relation to the width of the container.

If the XOffset field is set in conjunction with a fixed or relative width then the checkbox will be drawn at an X
coordinate calculated from the formula `X = ContainerWidth - CheckBoxWidth - XOffset`.

*****************************************************************************/

static ERROR GET_XOffset(objCheckBox *Self, struct Variable *Value)
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

static ERROR SET_XOffset(objCheckBox *Self, struct Variable *Value)
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
Y: The vertical position of a checkbox.

The vertical position of a CheckBox can be set to an absolute or relative coordinate by writing a value to the Y
field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be interpreted
as fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_Y(objCheckBox *Self, struct Variable *Value)
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

static ERROR SET_Y(objCheckBox *Self, struct Variable *Value)
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
YOffset: The vertical offset of a checkbox.

The YOffset has a dual purpose depending on whether or not it is set in conjunction with a Y coordinate or a Height
based field.

If set in conjunction with a Y coordinate then the checkbox will be drawn from that Y coordinate up to the height of
the container, minus the value given in the YOffset.  This means that the height of the checkbox is dynamically
calculated in relation to the height of the container.

If the YOffset field is set in conjunction with a fixed or relative height then the checkbox will be drawn at a Y
coordinate calculated from the formula `Y = ContainerHeight - CheckBoxHeight - YOffset`.
-END-

*****************************************************************************/

static ERROR GET_YOffset(objCheckBox *Self, struct Variable *Value)
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

static ERROR SET_YOffset(objCheckBox *Self, struct Variable *Value)
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

static void draw_checkbox(objCheckBox *Self, objSurface *Surface, objBitmap *Bitmap)
{
   LONG chkx = Self->LabelWidth;
   if (!(Self->Flags & CBF_NO_BKGD)) {

      ULONG colour;
      if (Self->Entered) {
         if (!(Surface->Flags & RNF_DISABLED)) {
            colour = PackPixel(Bitmap,
                (Self->Colour.Red   + 20 > 255) ? 255 : Self->Colour.Red + 20,
                (Self->Colour.Green + 20 > 255) ? 255 : Self->Colour.Green + 20,
                (Self->Colour.Blue  + 20 > 255) ? 255 : Self->Colour.Blue + 20);
         }
         else colour = PackPixelRGBA(Bitmap, &Self->Colour);
      }
      else colour = PackPixelRGBA(Bitmap, &Self->Colour);

      if (Self->Align & ALIGN_RIGHT) chkx = Surface->Width - Surface->Height;

      gfxDrawRectangle(Bitmap, chkx, 0, Surface->Height, Surface->Height, colour, BAF_FILL);

      ULONG shadow    = PackPixelRGBA(Bitmap, &Self->Shadow);
      ULONG highlight = PackPixelRGBA(Bitmap, &Self->Highlight);

      LONG i = 0;
      if (Self->Thickness > 0) {
         for (i=0; i < Self->Thickness; i++) {
            // Top, Bottom
            gfxDrawRectangle(Bitmap, chkx + i, i, Surface->Height-i-i, 1, shadow, BAF_FILL);
            gfxDrawRectangle(Bitmap, chkx + i, Surface->Height-i-1, Surface->Height-i-i, 1, highlight, BAF_FILL);

            // Left, Right
            gfxDrawRectangle(Bitmap, chkx + i, i+1, 1, Surface->Height-i-i-2, shadow, BAF_FILL);
            gfxDrawRectangle(Bitmap, chkx + Surface->Height-i-1, i+1, 1, Surface->Height-i-i-2, highlight, BAF_FILL);
         }
      }

      if (Surface->Flags & RNF_HAS_FOCUS) {
         if (!(Surface->Flags & RNF_DISABLED)) {
            gfxDrawRectangle(Bitmap, chkx + i, i, Surface->Height-i-i, Surface->Height-i-i, PackPixelA(Bitmap, 0, 0, 0, 32), BAF_BLEND);
         }
      }
   }

   if (Self->Label[0]) {
      objFont *font = Self->Font;

      SetPointer(font, FID_Bitmap, Bitmap);
      SetString(font, FID_String, Self->Label);

      if (Surface->Flags & RNF_DISABLED) SetLong(font, FID_Opacity, 25);

      font->X = 0;
      font->Y = 0;
      font->Flags      |= FTF_CHAR_CLIP;
      font->WrapEdge    = Self->LabelWidth - 3;
      font->Align       = ALIGN_VERTICAL;
      font->AlignWidth  = Surface->Width;
      font->AlignHeight = Surface->Height;
      if (Self->Align & ALIGN_RIGHT) {
         font->Align |= ALIGN_RIGHT;
         font->AlignWidth -= Surface->Height + 4;
      }
      acDraw(font);

      if (Surface->Flags & RNF_DISABLED) SetLong(font, FID_Opacity, 100);
   }

   if (Self->Picture) {
      objBitmap *src = Self->Picture->Bitmap;
      if (!Self->Value) src->Opacity = 25;

      gfxCopyArea(src, Bitmap, BAF_BLEND, 0, 0, src->Width, src->Height,
         chkx + ((Surface->Height - src->Width)/2), ((Surface->Height - src->Height)/2));

      src->Opacity = 255;
   }
}

//****************************************************************************

static void key_event(objCheckBox *Self, evKey *Event, LONG Size)
{
   if (!(Event->Qualifiers & KQ_PRESSED)) return;

   if ((Event->Code IS K_ENTER) OR (Event->Code IS K_SPACE)) {
      acActivate(Self);
   }
}

//****************************************************************************

#include "class_checkbox_def.c"

static const struct FieldArray clFields[] = {
   { "Font",         FDF_INTEGRAL|FDF_R,      0, NULL, NULL },
   { "LayoutSurface",FDF_VIRTUAL|FDF_OBJECTID|FDF_SYSTEM|FDF_R, ID_SURFACE, NULL, NULL }, // VIRTUAL: This is a synonym for the Region field
   { "Region",       FDF_OBJECTID|FDF_RW,  ID_SURFACE, NULL, NULL },
   { "Surface",      FDF_OBJECTID|FDF_RW,  ID_SURFACE, NULL, NULL },
   { "Flags",        FDF_LONGFLAGS|FDF_RW, (MAXINT)&clCheckBoxFlags, NULL, NULL },
   { "EnterFrame",   FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "ExitFrame",    FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "ClickFrame",   FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "ReleaseFrame", FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Thickness",    FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "LabelWidth",   FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Value",        FDF_LONG|FDF_RW,      0, NULL, SET_Value },
   { "Align",        FDF_LONGFLAGS|FDF_RW, (MAXINT)&Align, NULL, NULL },
   { "Colour",       FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "Highlight",    FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "Shadow",       FDF_RGB|FDF_RW,       0, NULL, NULL },
   // Virtual fields
   { "Border",       FDF_VIRTUAL|FDF_STRING|FDF_W,    0, NULL, SET_Border },
   { "Bottom",       FDF_VIRTUAL|FDF_LONG|FDF_R,      0, GET_Bottom, NULL },
   { "CheckImage",   FDF_VIRTUAL|FDF_STRING|FDF_W,    0, NULL, SET_CheckImage },
   { "Disable",      FDF_VIRTUAL|FDF_LONG|FDF_RW,     0, GET_Disable, SET_Disable },
   { "Feedback",     FDF_VIRTUAL|FDF_FUNCTIONPTR|FDF_RW, 0, GET_Feedback, SET_Feedback },
   { "Label",        FDF_VIRTUAL|FDF_STRING|FDF_RW,   0, GET_Label, SET_Label },
   { "LayoutStyle",  FDF_VIRTUAL|FDF_POINTER|FDF_SYSTEM|FDF_W, 0, NULL, SET_LayoutStyle },
   { "Right",        FDF_VIRTUAL|FDF_LONG|FDF_R,      0, GET_Right, NULL },
   { "Selected",     FDF_SYNONYM|FDF_VIRTUAL|FDF_LONG|FDF_RW, 0, GET_Value, SET_Value },
   { "TabFocus",     FDF_VIRTUAL|FDF_OBJECTID|FDF_W,  ID_TABFOCUS, NULL,   SET_TabFocus },
   // Variable Fields
   { "Height",       FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Height,  SET_Height },
   { "Width",        FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Width,   SET_Width },
   { "X",            FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_X,       SET_X },
   { "XOffset",      FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_XOffset, SET_XOffset },
   { "Y",            FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Y,       SET_Y },
   { "YOffset",      FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_YOffset, SET_YOffset },
   END_FIELD
};

//****************************************************************************

ERROR init_checkbox(void)
{
   return(CreateObject(ID_METACLASS, 0, &clCheckBox,
      FID_ClassVersion|TFLOAT, VER_CHECKBOX,
      FID_Name|TSTRING,   "CheckBox",
      FID_Category|TLONG, CCF_GUI,
      FID_Flags|TLONG,    CLF_PROMOTE_INTEGRAL|CLF_PRIVATE_ONLY,
      FID_Actions|TPTR,   clCheckBoxActions,
      FID_Fields|TARRAY,  clFields,
      FID_Size|TLONG,     sizeof(objCheckBox),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND));
}

void free_checkbox(void)
{
   if (clCheckBox) { acFree(clCheckBox); clCheckBox = NULL; }
}
