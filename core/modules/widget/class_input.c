/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Input: The Input class manages the display and interactivity of user input boxes.

The Input class simplifies the creation and management of input boxes as part of the user interface.  New input areas
can be created by specifying as little as the graphical dimensions for the box area.  The Input class allows for the
specifics of the graphics to be altered, such as the colours and the font used.

The definitions for new input boxes are loaded by default from the environment file `style:input.xml`.  You can
change the template file prior to initialisation by setting the Template field.  Note that any values set in the
template will override your original field settings for the input object.

It is likely that when when the user clicks or tabs away from the input box, you will need it to perform an action.
Set the #Feedback field in order to receive this notification and respond with your own custom functionality.

-END-

*****************************************************************************/

#define PRV_INPUT
#include <parasol/modules/document.h>
#include <parasol/modules/widget.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/font.h>
#include <parasol/modules/surface.h>
#include "defs.h"

static OBJECTPTR clInput = NULL;

enum {
   STATE_ENTERED=1,
   STATE_EXITED,
   STATE_INSIDE
};

static void draw_input(objInput *, objSurface *, objBitmap *);
static void text_validation(objText *);
static void text_activated(objText *);

//****************************************************************************

static ERROR INPUT_ActionNotify(objInput *Self, struct acActionNotify *Args)
{
   if (Args->Error != ERR_Okay) return ERR_Okay;

   if (Args->ActionID IS AC_Disable) {
      Self->Flags |= INF_DISABLED;
      DelayMsg(AC_Draw, Self->RegionID, NULL);
   }
   else if (Args->ActionID IS AC_Enable) {
      Self->Flags &= ~INF_DISABLED;
      DelayMsg(AC_Draw, Self->RegionID, NULL);
   }
   else if (Args->ActionID IS AC_Free) {
      if ((Self->prvFeedback.Type IS CALL_SCRIPT) AND (Self->prvFeedback.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->prvFeedback.Type = CALL_NONE;
      }
   }
   else return ERR_NoSupport;

   return ERR_Okay;
}

//****************************************************************************

static ERROR INPUT_DataFeed(objInput *Self, struct acDataFeed *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   if (Args->DataType IS DATA_INPUT_READY) {
      struct InputMsg *input;

      while (!gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &input)) {
         if (input->Flags & JTYPE_MOVEMENT) {
            OBJECTPTR surface;

            if (input->OverID IS Self->RegionID) {
               if (Self->prvState IS STATE_ENTERED) Self->prvState = STATE_INSIDE;
               else if (Self->prvState != STATE_INSIDE) Self->prvState = STATE_ENTERED;
            }
            else {
               if (Self->prvState IS STATE_EXITED) continue;
               else Self->prvState = STATE_EXITED;
            }

            // Change the surface's frame if necessary

            if ((Self->prvState != STATE_INSIDE) AND (Self->EnterFrame)) {
               if (!AccessObject(Self->RegionID, 2000, &surface)) {
                  if (Self->prvState IS STATE_EXITED) {
                     SetLong(surface, FID_Frame, Self->ExitFrame);
                     DelayMsg(AC_Draw, Self->RegionID, NULL);
                  }
                  else if (Self->prvState IS STATE_ENTERED) {
                     if (!(Self->Flags & INF_DISABLED)) {
                        SetLong(surface, FID_Frame, Self->EnterFrame);
                        DelayMsg(AC_Draw, Self->RegionID, NULL);
                     }
                  }
                  ReleaseObject(surface);
               }
            }

            if (Self->prvState IS STATE_ENTERED) Self->prvState = STATE_INSIDE;
         }
         else MSG("Unrecognised input message type $%.8x", input->Flags);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Disable: Turns the input box off.
-END-
*****************************************************************************/

static ERROR INPUT_Disable(objInput *Self, APTR Void)
{
   // See the ActionNotify routine to see what happens when the surface is disabled.  Disabling the region will have
   // the desired effect of turning off input box editing.

   acDisableID(Self->RegionID);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Enable: Turns the input box back on if it has previously been disabled.
-END-
*****************************************************************************/

static ERROR INPUT_Enable(objInput *Self, APTR Void)
{
   // See the ActionNotify routine to see what happens when the surface is enabled.

   acEnableID(Self->RegionID);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Focus: Sets the focus on the input box.
-END-
*****************************************************************************/

static ERROR INPUT_Focus(objInput *Self, APTR Void)
{
   acFocusID(Self->RegionID);
   return ERR_Okay;
}

//****************************************************************************

static ERROR INPUT_Free(objInput *Self, APTR Void)
{
   if (Self->Font) { acFree(Self->Font); Self->Font = NULL; }
   if (Self->TextInput) { acFree(Self->TextInput); Self->TextInput = NULL; }

   if (Self->RegionID) {
      OBJECTPTR object;
      if (!AccessObject(Self->RegionID, 3000, &object)) {
         UnsubscribeAction(object, 0);
         ReleaseObject(object);
      }
      acFreeID(Self->RegionID);
      Self->RegionID = 0;
   }

   gfxUnsubscribeInput(0);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Hide: Removes the input box from the display.
-END-
*****************************************************************************/

static ERROR INPUT_Hide(objInput *Self, APTR Void)
{
   Self->Flags |= INF_HIDE;
   acHideID(Self->RegionID);
   return ERR_Okay;
}

//****************************************************************************

static ERROR INPUT_Init(objInput *Self, APTR Void)
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

   // Calculate the width of the text label, if there is one

   if ((Self->LabelWidth < 1) AND (Self->prvLabel[0])) {
      Self->LabelWidth = fntStringWidth(Self->Font, Self->prvLabel, -1) + 4;
   }

   // Initialise the input region

   objSurface *region;
   if (!AccessObject(Self->RegionID, 5000, &region)) {
      region->Flags |= RNF_GRAB_FOCUS|RNF_REGION;

      SetLong(region, FID_Parent, Self->SurfaceID);

      if (!(region->Dimensions & DMF_HEIGHT)) {
         if ((!(region->Dimensions & DMF_Y)) OR (!(region->Dimensions & DMF_Y_OFFSET))) {
            LONG h = Self->Font->MaxHeight + (Self->Thickness * 2) + Self->TextInput->Layout->TopMargin +
               Self->TextInput->Layout->BottomMargin;
            SetLong(region, FID_Height, h);
         }
      }

      if (!(region->Dimensions & (DMF_FIXED_WIDTH|DMF_RELATIVE_WIDTH|DMF_FIXED_X_OFFSET|DMF_RELATIVE_X_OFFSET))) {
         SetLong(region, FID_Width, Self->LabelWidth + ((Self->InputWidth) ? Self->InputWidth : 30));
      }

      if (!acInit(region)) SubscribeActionTags(region, AC_Disable, AC_Enable, TAGEND);

      gfxSubscribeInput(Self->RegionID, JTYPE_MOVEMENT, 0);

      // The user may set the margins and alignment values in the input template (this is sometimes done to align text
      // to the bottom of the surface instead of the centre).

      ReleaseObject(region);
   }
   else return PostError(ERR_AccessObject);

   // Use the base template to create the input graphics

   if (!(Self->Flags & INF_NO_BKGD)) {
      if (!drwApplyStyleGraphics(Self, Self->RegionID, NULL, NULL)) {
         Self->Flags |= INF_NO_BKGD;
      }
   }

   if (!AccessObject(Self->RegionID, 5000, &region)) {
      drwAddCallback(region, &draw_input);
      ReleaseObject(region);
   }
   else return ERR_AccessObject;

   // Initialise the text area that the user will be able to interact with.

   LONG flags;
   GetLong(Self->TextInput, FID_Flags, &flags);
   flags |= TXF_EDIT;

   if (Self->Flags & INF_ENTER_TAB) flags |= TXF_ENTER_TAB;
   if (Self->Flags & INF_SECRET)    flags |= TXF_SECRET;
   if (Self->Flags & INF_NO_BKGD)   flags |= TXF_PRESERVE_BKGD;

   SetFields(Self->TextInput,
      FID_Surface|TLONG,      Self->RegionID,
      FID_Flags|TLONG,        flags,
      FID_Point|TDOUBLE,      Self->Font->Point,
      FID_X|TLONG,            Self->LabelWidth + Self->Thickness,
      FID_Y|TLONG,            Self->Thickness,
      FID_YOffset|TLONG,      Self->Thickness,
      FID_TopMargin|TLONG,    0,
      FID_BottomMargin|TLONG, 0,
      FID_LineLimit|TLONG,    1,
      TAGEND);

   SetFunctionPtr(Self->TextInput, FID_ValidateInput, &text_validation);
   SetFunctionPtr(Self->TextInput, FID_Activated, &text_activated);

   if (Self->InputWidth) SetLong(Self->TextInput, FID_Width, Self->InputWidth - (Self->Thickness * 2));
   else SetLong(Self->TextInput, FID_XOffset, Self->Thickness);

   if (acInit(Self->TextInput)) return PostError(ERR_Init);

   if (Self->Flags & INF_SELECT_TEXT) txtSelectArea(Self->TextInput, 0, 0, 20000, 20000);
   if (!(Self->Flags & (INF_SUNKEN|INF_RAISED))) Self->Flags |= INF_SUNKEN;
   if (Self->Flags & INF_DISABLED) acDisable(Self);
   if (!(Self->Flags & INF_HIDE)) acShowID(Self->RegionID);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToBack: Moves the input box to the back of the display area.
-END-
*****************************************************************************/

static ERROR INPUT_MoveToBack(objInput *Self, APTR Void)
{
   acMoveToBackID(Self->RegionID);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToFront: Moves the input box to the front of the display area.
-END-
*****************************************************************************/

static ERROR INPUT_MoveToFront(objInput *Self, APTR Void)
{
   acMoveToFrontID(Self->RegionID);
   return ERR_Okay;
}

//****************************************************************************

static ERROR INPUT_NewObject(objInput *Self, APTR Void)
{
   if (!NewLockedObject(ID_SURFACE, NF_INTEGRAL|Self->Head.Flags, NULL, &Self->RegionID)) {
      if (!NewObject(ID_FONT, NF_INTEGRAL|Self->Head.Flags, &Self->Font)) {
         SetString(Self->Font, FID_Face, glLabelFace);
         if (!NewObject(ID_TEXT, NF_INTEGRAL, &Self->TextInput)) {
            SetString(Self->TextInput->Font, FID_Face, glWidgetFace);

            Self->ExitFrame = 1;
            Self->ReleaseFrame = 1;
            Self->Flags |= INF_SUNKEN;
            Self->Thickness = 1;

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

            Self->TextInput->Layout->Align = ALIGN_VERTICAL;
            Self->TextInput->Layout->LeftMargin   = 3;
            Self->TextInput->Layout->RightMargin  = 3;
            Self->TextInput->Layout->TopMargin    = 2;
            Self->TextInput->Layout->BottomMargin = 2;

            drwApplyStyleValues(Self, NULL);

            return ERR_Okay;
         }
         else return ERR_NewObject;
      }
      else return ERR_NewObject;
   }
   else return ERR_NewObject;
}

/*****************************************************************************
-ACTION-
Redimension: Changes the size and position of the input box.
-END-
*****************************************************************************/

static ERROR INPUT_Redimension(objInput *Self, struct acRedimension *Args)
{
   return ActionMsg(AC_Redimension, Self->RegionID, Args);
}

/*****************************************************************************
-ACTION-
Resize: Alters the size of the input box.
-END-
*****************************************************************************/

static ERROR INPUT_Resize(objInput *Self, struct acResize *Args)
{
   return ActionMsg(AC_Resize, Self->RegionID, Args);
}

/*****************************************************************************
-ACTION-
Show: Puts the input box on display.
-END-
*****************************************************************************/

static ERROR INPUT_Show(objInput *Self, APTR Void)
{
   Self->Flags &= ~INF_HIDE;
   acShowID(Self->RegionID);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Bottom: The bottom coordinate of the input box (Y + Height).

*****************************************************************************/

static ERROR GET_Bottom(objInput *Self, LONG *Value)
{
   SURFACEINFO *info;
   if (!drwGetSurfaceInfo(Self->RegionID, &info)) {
      *Value = info->Y + info->Height;
      return ERR_Okay;
   }
   else return ERR_GetSurfaceInfo;
}

/*****************************************************************************

-FIELD-
Colour: The colour inside of the input box.

-FIELD-
Disable: If TRUE, the input box is disabled.

The Disable field can be used to disable the input box in advance of being initialised, by setting the field value to
TRUE.  It can also be read at any time to determine the current interactive state of the input box.

Post-initialisation, it is recommended that only the #Disable() and #Enable() actions are used to change the
interactive state of the input box.

*****************************************************************************/

static ERROR GET_Disable(objInput *Self, LONG *Value)
{
   if (Self->Flags & INF_DISABLED) *Value = TRUE;
   else *Value = FALSE;
   return ERR_Okay;
}

static ERROR SET_Disable(objInput *Self, LONG Value)
{
   if (Value IS TRUE) acDisable(Self);
   else if (Value IS FALSE) acEnable(Self);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
EnterFrame: The graphics frame to display when the user's cursor enters the input area.

-FIELD-
ExitFrame: The graphics frame to display when the user's cursor leaves the input area.

-FIELD-
Feedback: Provides instant feedback when a user interacts with the object.

Set the Feedback field with a callback function that will receive instant feedback when user interaction occurs.  The
function prototype is `Function(*Input, STRING Value, LONG Activated)`

The Activated parameter is a boolean value that will be set to TRUE if the user has affirmed the input by pressing the
enter key or its equivalent.

*****************************************************************************/

static ERROR GET_Feedback(objInput *Self, FUNCTION **Value)
{
   if (Self->prvFeedback.Type != CALL_NONE) {
      *Value = &Self->prvFeedback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Feedback(objInput *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->prvFeedback.Type IS CALL_SCRIPT) UnsubscribeAction(Self->prvFeedback.Script.Script, AC_Free);
      Self->prvFeedback = *Value;
      if (Self->prvFeedback.Type IS CALL_SCRIPT) SubscribeAction(Self->prvFeedback.Script.Script, AC_Free);
   }
   else Self->prvFeedback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Flags: Optional flags can be defined here.

-FIELD-
FocusFrame: The graphics frame to display when the input box has the focus.

This field specifies the surface frame to switch to when the user focuses on the input box.  By default this field is
initially set to zero, which has no effect on the surface frame.  When the user leaves the input box, it will revert to
the frame indicated by the #ReleaseFrame field.

-FIELD-
Font: Refers to the font used to draw the input label.

The font object is used to draw the label for the input box.  To prevent clashes with the input box code, it is
recommended that configuring of the font object is limited to the face, point size and colour.

In addition, do not attempt to reconfigure the font after initialisation.

-FIELD-
Height: Defines the height of an input box.

An input box can be given a fixed or relative height by setting this field to the desired value.  To set a relative
height, use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Height(objInput *Self, struct Variable *Value)
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

static ERROR SET_Height(objInput *Self, struct Variable *Value)
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
Highlight: The colour of the border highlight is defined here.

-FIELD-
InputWidth: The width of the input area.

A fixed width for the input area can be defined in this field (note that this does not include the width of the label,
which is handled separately by #LabelWidth.  By default the InputWidth is set to zero, which leads the input
area to span the entire width of its container.

-FIELD-
Label: The label is a string displayed to the left of the input area.

A label can be drawn next to the input area by setting the Label field.  The label should be a short, descriptive
string of one or two words.  It is common practice for the label to be followed with a colon character.

*****************************************************************************/

static ERROR GET_Label(objInput *Self, STRING *Value)
{
   *Value = Self->prvLabel;
   return ERR_Okay;
}

static ERROR SET_Label(objInput *Self, CSTRING Value)
{
   if (Value) StrCopy(StrTranslateText(Value), Self->prvLabel, sizeof(Self->prvLabel));
   else Self->prvLabel[0] = 0;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
LabelWidth: The width of the input label.

If a label has been set for an input box, its width may be read and adjusted at any time via the LabelWidth field.  The
input area will be arranged so that it immediately follows the width defined for the text label.  If you define a width
that is too short for the text that is to be printed in the label, the text will be trimmed to fit the defined area.

If you specify a label without setting the label width, the correct width will be automatically calculated for you on
initialisation.

*****************************************************************************/

static ERROR SET_LabelWidth(objInput *Self, LONG Value)
{
   Self->LabelWidth = Value;

   if (Self->Head.Flags & NF_INITIALISED) {
      LONG x = Self->LabelWidth + Self->Thickness;
      SetLong(Self->TextInput, FID_X, x);
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Layout: Private. Overrides the Layout in the TextInput child object (because our layout is reflected in the Surface object).

*****************************************************************************/

static ERROR GET_Layout(objInput *Self, objLayout **Value)
{
   *Value = NULL;
   return ERR_NoSupport;
}

/*****************************************************************************

-FIELD-
LayoutStyle: Private field for supporting dynamic style changes when an input object is used in a document.

*****************************************************************************/

static ERROR SET_LayoutStyle(objInput *Self, DOCSTYLE *Value)
{
   if (!Value) return ERR_Okay;

   if (Self->Head.Flags & NF_INITIALISED) {
      docApplyFontStyle(Value->Document, Value, Self->Font);
   }
   else docApplyFontStyle(Value->Document, Value, Self->Font);

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
PostLabel: The post-label is a string displayed to the right of the input area.

A label can be drawn after the input area by setting the PostLabel field.  The PostLabel is commonly linked with the
Label field for constructing sentences around the input box, for example "Disable account after X days." where X
represents the input box, we would set a PostLabel string of "days.".

*****************************************************************************/

static ERROR GET_PostLabel(objInput *Self, STRING *Value)
{
   *Value = Self->prvPostLabel;
   return ERR_Okay;
}

static ERROR SET_PostLabel(objInput *Self, CSTRING Value)
{
   if (Value) StrCopy(StrTranslateText(Value), Self->prvPostLabel, sizeof(Self->prvPostLabel));
   else Self->prvPostLabel[0] = 0;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Raised: If set to TRUE the input box will appear to be raised into the foreground.

If you have set the Highlight and Shadow fields of an input object then you will need to decide whether or not the box
should be given a sunken or raised effect when it is drawn.  To give it a raised effect you will need to set this field
to TRUE, if not then you should set the Sunken field.

*****************************************************************************/

static ERROR GET_Raised(objInput *Self, LONG *Value)
{
   if (Self->Flags & INF_SUNKEN) *Value = TRUE;
   else *Value = FALSE;
   return ERR_Okay;
}

static ERROR SET_Raised(objInput *Self, LONG Value)
{
   if (Value) {
      if (Self->Flags & INF_RAISED) return ERR_Okay;
      Self->Flags = (Self->Flags & ~INF_SUNKEN) | INF_RAISED;
   }
   else Self->Flags &= ~INF_RAISED;

   if (Self->Flags & INF_ACTIVE_DRAW) acDrawID(Self->RegionID);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Region: The surface that represents the input box is referenced through this field.

The surface area that represents the input display can be accessed through this field.  For further information, refer
to the @Surface class.  Note that interfacing with the surface directly can have adverse effects on the input
control system.  Where possible, all communication should be limited to the input object itself.

*****************************************************************************/

static ERROR SET_Region(objInput *Self, LONG Value)
{
   // NOTE: For backwards compatibility with the Surface class, the region can be set to a value of TRUE
   // to define the input as a simple surface region.

   if ((Value IS FALSE) OR (Value IS TRUE)) {
      OBJECTPTR surface;
      if (!AccessObject(Self->RegionID, 4000, &surface)) {
         SetLong(surface, FID_Region, Value);
         ReleaseObject(surface);
         return ERR_Okay;
      }
      else return ERR_AccessObject;
   }
   else return ERR_Failed;
}

/*****************************************************************************

-FIELD-
ReleaseFrame: The graphics frame to display when the input box loses the focus.

If the FocusFrame field has been set, you may want to match that value by indicating the frame that should be used when
the click is released.  By default, the value in this field will initially be set to 1.  This field is unused if the
FocusFrame field has not been set.

-FIELD-
Right: The right-most coordinate of the input box (X + Width).

*****************************************************************************/

static ERROR GET_Right(objInput *Self, LONG *Value)
{
   SURFACEINFO *info;
   if (!drwGetSurfaceInfo(Self->RegionID, &info)) {
      *Value = info->X + info->Width;
      return ERR_Okay;
   }
   else return PostError(ERR_GetSurfaceInfo);
}

/*****************************************************************************

-FIELD-
Shadow: The colour of the input box shadow is defined here.

-FIELD-
String: The string that is to be printed inside the input box is declared here.

The string that you would like to be displayed in the input box is specified in this field.  The string must be in
UTF-8 format and may not contain line feeds.  The client can read this field at any time to determine what the user has
entered in the input box.

If the string is changed after initialisation, the input box will be redrawn to show the updated text.

*****************************************************************************/

static ERROR GET_String(objInput *Self, STRING *Value)
{
   STRING str;
   ERROR error;
   if (!(error = GetString(Self->TextInput, FID_String, &str))) {
      *Value = str;
      return ERR_Okay;
   }
   else return error;
}

static ERROR SET_String(objInput *Self, CSTRING Value)
{
   Self->prvStringReset = TRUE;
   if (!SetString(Self->TextInput, FID_String, Value)) {
      return ERR_Okay;
   }
   else return ERR_Failed;
}

/*****************************************************************************

-FIELD-
Sunken: Set to TRUE to make the input box appear to sink into the background.

If you have set the Highlight and Shadow fields of an input object then you will need to decide whether or not the box
should be given a sunken or raised effect when it is drawn.  To give it a sunken effect you will need to set this field
to TRUE, if not then you should set the Raised field.

*****************************************************************************/

static ERROR GET_Sunken(objInput *Self, LONG *Value)
{
   if (Self->Flags & INF_SUNKEN) *Value = TRUE;
   else *Value = FALSE;
   return ERR_Okay;
}

static ERROR SET_Sunken(objInput *Self, LONG Value)
{
   if (Value) {
      if (Self->Flags & INF_SUNKEN) return ERR_Okay;
      Self->Flags = (Self->Flags & ~INF_RAISED) | INF_SUNKEN;
   }
   else Self->Flags &= ~INF_SUNKEN;

   if (Self->Flags & INF_ACTIVE_DRAW) acDrawID(Self->RegionID);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Surface: The surface that will contain the input graphic.

The surface that will contain the input graphic is set here.  If this field is not set prior to initialisation, the
input will attempt to scan for the correct surface by analysing its parents until it finds a suitable candidate.

-FIELD-
TabFocus: Setting this field to a valid TabFocus object will cause the input to add itself to the tab list.

The TabFocus field provides a convenient way of adding the input to a TabFocus object, so that it can be focussed on
via the tab key.  Simply set this field to the ID of the TabFocus object that is managing the tab-list for the
application window.

*****************************************************************************/

static ERROR SET_TabFocus(objInput *Self, OBJECTID Value)
{
   OBJECTPTR tabfocus;
   if (!AccessObject(Value, 5000, &tabfocus)) {
      if (tabfocus->ClassID IS ID_TABFOCUS) tabAddObject(tabfocus, Self->Head.UniqueID); //RegionID);
      ReleaseObject(tabfocus);
      return ERR_Okay;
   }
   else return ERR_AccessObject;
}

/*****************************************************************************

-FIELD-
TextInput: Refers to a Text object that handles the text inside the input area.

This field refers to a @Text object that handles the text inside the input area.  Please exercise caution if
interacting with this object directly, as doing so may interfere with the expected behaviour of the input class.

-FIELD-
Thickness: The thickness of the input border, in pixels.

-FIELD-
Width: Defines the width of an input box.

An input box can be given a fixed or relative width by setting this field to the desired value.  To set a relative
width, use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Width(objInput *Self, struct Variable *Value)
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

static ERROR SET_Width(objInput *Self, struct Variable *Value)
{
   if (((Value->Type & FD_DOUBLE) AND (!Value->Double)) OR
       ((Value->Type & FD_LARGE) AND (!Value->Large))) {
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
X: The horizontal position of an input box.

The horizontal position of an input box can be set to an absolute or relative coordinate by writing a value to the
X field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be
interpreted as fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_X(objInput *Self, struct Variable *Value)
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

static ERROR SET_X(objInput *Self, struct Variable *Value)
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
XOffset: The horizontal offset of an input box.

The XOffset has a dual purpose depending on whether or not it is set in conjunction with an X coordinate or a Width
based field.

If set in conjunction with an X coordinate then the input will be drawn from that X coordinate up to the width of the
container, minus the value given in the XOffset.  This means that the width of the widget is dynamically calculated in
relation to the width of the container.

If the XOffset field is set in conjunction with a fixed or relative width then the input will be drawn at an X
coordinate calculated from the formula `X = ContainerWidth - InputWidth - XOffset`.

*****************************************************************************/

static ERROR GET_XOffset(objInput *Self, struct Variable *Value)
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

static ERROR SET_XOffset(objInput *Self, struct Variable *Value)
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
Y: The vertical position of an input box.

The vertical position of an input box can be set to an absolute or relative coordinate by writing a value to the Y
field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be interpreted as
fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_Y(objInput *Self, struct Variable *Value)
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

static ERROR SET_Y(objInput *Self, struct Variable *Value)
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
YOffset: The vertical offset of an input box.

The YOffset has a dual purpose depending on whether or not it is set in conjunction with a Y coordinate or a Height
based field.

If set in conjunction with a Y coordinate then the input will be drawn from that Y coordinate up to the height of the
container, minus the value given in the YOffset.  This means that the height of the widget is dynamically calculated in
relation to the height of the container.

If the YOffset field is set in conjunction with a fixed or relative height then the input will be drawn at a Y
coordinate calculated from the formula `Y = ContainerHeight - InputHeight - YOffset`.
-END-

*****************************************************************************/

static ERROR GET_YOffset(objInput *Self, struct Variable *Value)
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

static ERROR SET_YOffset(objInput *Self, struct Variable *Value)
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

static void draw_input(objInput *Self, objSurface *Surface, objBitmap *Bitmap)
{
   WORD width;

   if (!(Self->Flags & INF_NO_BKGD)) {
      gfxDrawRectangle(Bitmap, Self->LabelWidth, 0, (Self->InputWidth > 0) ? Self->InputWidth : Surface->Width - Self->LabelWidth,
         Surface->Height, PackPixelRGBA(Bitmap, &Self->Colour), BAF_FILL|BAF_BLEND);

      // Draw the borders around the rectangular area

      ULONG highlight, shadow;
      if (Self->Flags & INF_SUNKEN) { // Reverse the border definitions in sunken mode
         highlight = PackPixelRGBA(Bitmap, &Self->Shadow);
         shadow = PackPixelRGBA(Bitmap, &Self->Highlight);
      }
      else {
         shadow = PackPixelRGBA(Bitmap, &Self->Shadow);
         highlight = PackPixelRGBA(Bitmap, &Self->Highlight);
      }

      LONG x = Self->LabelWidth;
      if (Self->InputWidth > 0) width = Self->InputWidth;
      else width = Surface->Width - Self->LabelWidth;

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

   if (Self->prvLabel[0]) {
      objFont *font = Self->Font;
      font->Bitmap = Bitmap;

      SetString(font, FID_String, Self->prvLabel);

      if (Surface->Flags & RNF_DISABLED) SetLong(font, FID_Opacity, 25);

      font->X = Surface->LeftMargin;
      font->Y = Surface->TopMargin;
      font->Flags |= FTF_CHAR_CLIP;
      font->WrapEdge = Self->LabelWidth - 3;
      font->Align |= ALIGN_VERTICAL;
      font->AlignWidth  = Surface->Width - Surface->RightMargin - Surface->LeftMargin;
      font->AlignHeight = Surface->Height - Surface->BottomMargin - Surface->TopMargin;
      acDraw(font);

      if (Self->prvPostLabel[0]) {
         font->X = Self->LabelWidth + Self->InputWidth;
         font->WrapEdge = Surface->Width;
         SetString(font, FID_String, Self->prvPostLabel);
         acDraw(font);
      }

      if (Surface->Flags & RNF_DISABLED) SetLong(font, FID_Opacity, 100);
   }
}

//**********************************************************************
// This callback is triggered when the user moves focus away from the text widget.

static void text_validation(objText *Text)
{
   objInput *Self = (objInput *)CurrentContext();

   if (Self->prvActive) {
      LogErrorMsg("Warning - recursion detected");
      return;
   }

   LogBranch(NULL);

   Self->prvActive = TRUE;

   CSTRING str;

   /* 2017-03: Not sure what this code is meant for
   if (Self->prvStringReset) {
      LogMsg("String reset requested.");
      Self->prvStringReset = FALSE;
      str = "";
   }
   else */

   ULONG hash = 0; // Do nothing if the string hasn't changed.
   if (!GetString(Text, FID_String, (STRING *)&str)) hash = StrHash(str, TRUE);
   if (hash IS Self->prvLastStringHash) goto exit;
   Self->prvLastStringHash = hash;

   if (Self->prvFeedback.Type IS CALL_STDC) {
      void (*routine)(OBJECTPTR Context, objInput *, CSTRING, LONG) = Self->prvFeedback.StdC.Routine;
      if (Self->prvFeedback.StdC.Context) {
         OBJECTPTR context = SetContext(Self->prvFeedback.StdC.Context);
         routine(Self->prvFeedback.StdC.Context, Self, str, FALSE);
         SetContext(context);
      }
      else routine(Self->prvFeedback.StdC.Context, Self, str, FALSE);
   }
   else if (Self->prvFeedback.Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Self->prvFeedback.Script.Script)) {
         const struct ScriptArg args[] = {
            { "Input", FD_OBJECTPTR, { .Address = Self } },
            { "Value", FD_STRING,    { .Address = (STRING)str } },
            { "Activated", FD_LONG,  { .Long = FALSE } }
         };
         scCallback(script, Self->prvFeedback.Script.ProcedureID, args, ARRAYSIZE(args));
      }
   }

exit:
   Self->prvActive = FALSE;
   LogBack();
}

//**********************************************************************
// This callback is triggered when the user hits the enter key, or its equivalent.

static void text_activated(objText *Text)
{
   objInput *Self = (objInput *)CurrentContext();

   if (Self->prvActive) {
      LogErrorMsg("Warning - recursion detected");
      return;
   }

   LogBranch(NULL);

   Self->prvActive = TRUE;

   CSTRING str;

   /* 2017-03: Not sure what this code is meant for
   if (Self->prvStringReset) {
      LogMsg("String reset requested.");
      Self->prvStringReset = FALSE;
      str = "";
   }
   else */

   ULONG hash = 0; // Do nothing if the string hasn't changed.
   if (!GetString(Text, FID_String, (STRING *)&str)) {
      hash = StrHash(str, FALSE);
   }
   if (hash IS Self->prvLastStringHash) goto exit;
   Self->prvLastStringHash = hash;

   if (Self->prvFeedback.Type IS CALL_STDC) {
      void (*routine)(OBJECTPTR Context, objInput *, CSTRING, LONG) = Self->prvFeedback.StdC.Routine;
      if (Self->prvFeedback.StdC.Context) {
         OBJECTPTR context = SetContext(Self->prvFeedback.StdC.Context);
         routine(Self->prvFeedback.StdC.Context, Self, str, TRUE);
         SetContext(context);
      }
      else routine(Self->prvFeedback.StdC.Context, Self, str, TRUE);
   }
   else if (Self->prvFeedback.Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Self->prvFeedback.Script.Script)) {
         const struct ScriptArg args[] = {
            { "Input", FD_OBJECTPTR, { .Address = Self } },
            { "Value", FD_STRING,    { .Address = (STRING)str } },
            { "Activated", FD_LONG,  { .Long = TRUE } }
         };
         scCallback(script, Self->prvFeedback.Script.ProcedureID, args, ARRAYSIZE(args));
      }
   }

exit:
   Self->prvActive = FALSE;
   LogBack();
}

//**********************************************************************

#include "class_input_def.c"

static const struct FieldArray clFields[] = {
   { "Font",         FDF_INTEGRAL|FDF_R,      ID_FONT, NULL, NULL },
   { "TextInput",    FDF_INTEGRAL|FDF_R,      ID_TEXT, NULL, NULL },
   { "LayoutSurface",FDF_VIRTUAL|FDF_OBJECTID|FDF_SYSTEM|FDF_R, ID_SURFACE, NULL, NULL }, // VIRTUAL: This is a synonym for the Region field
   { "Region",       FDF_OBJECTID|FDF_RW,  ID_SURFACE, NULL, SET_Region },
   { "Surface",      FDF_OBJECTID|FDF_RW,  ID_SURFACE, NULL, NULL },
   { "Flags",        FDF_LONGFLAGS|FDF_RW, (MAXINT)&clInputFlags, NULL, NULL },
   { "EnterFrame",   FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "ExitFrame",    FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "FocusFrame",   FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "ReleaseFrame", FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Thickness",    FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "LabelWidth",   FDF_LONG|FDF_RW,      0, NULL, SET_LabelWidth },
   { "InputWidth",   FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "Colour",       FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "Highlight",    FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "Shadow",       FDF_RGB|FDF_RW,       0, NULL, NULL },
   // Virtual fields
   { "Bottom",       FDF_VIRTUAL|FDF_LONG|FDF_R,         0, GET_Bottom,        NULL },
   { "Disable",      FDF_VIRTUAL|FDF_LONG|FDF_RW,        0, GET_Disable,       SET_Disable },
   { "Feedback",     FDF_VIRTUAL|FDF_FUNCTIONPTR|FDF_RW, 0, GET_Feedback,      SET_Feedback },
   { "Label",        FDF_VIRTUAL|FDF_STRING|FDF_RW,      0, GET_Label,         SET_Label },
   { "LayoutStyle",  FDF_VIRTUAL|FDF_POINTER|FDF_SYSTEM|FDF_W, 0, NULL,        SET_LayoutStyle },
   { "PostLabel",    FDF_VIRTUAL|FDF_STRING|FDF_RW,      0, GET_PostLabel,     SET_PostLabel },
   { "Raised",       FDF_VIRTUAL|FDF_LONG|FDF_RW,        0, GET_Raised,        SET_Raised },
   { "Right",        FDF_VIRTUAL|FDF_LONG|FDF_R,         0, GET_Right,         NULL },
   { "Sunken",       FDF_VIRTUAL|FDF_LONG|FDF_RW,        0, GET_Sunken,        SET_Sunken },
   { "String",       FDF_VIRTUAL|FDF_STRING|FDF_RW,      0, GET_String,        SET_String },
   { "TabFocus",     FDF_VIRTUAL|FDF_OBJECTID|FDF_W,     ID_TABFOCUS, NULL,    SET_TabFocus },
   { "Text",         FDF_SYNONYM|FDF_VIRTUAL|FDF_STRING|FDF_RW, 0, GET_String, SET_String },
   { "Layout",       FDF_SYSTEM|FDF_VIRTUAL|FDF_OBJECT|FDF_R,   0, GET_Layout, NULL },  // Dummy field.  Prevents the Layout in the TextInput child from being used
   // Variable Fields
   { "Height",  FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Height, SET_Height },
   { "Width",   FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Width, SET_Width },
   { "X",       FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_X, SET_X },
   { "XOffset", FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_XOffset, SET_XOffset },
   { "Y",       FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_Y, SET_Y },
   { "YOffset", FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, GET_YOffset, SET_YOffset },
   END_FIELD
};

//****************************************************************************

ERROR init_input(void)
{
   return(CreateObject(ID_METACLASS, 0, &clInput,
      FID_ClassVersion|TFLOAT, 1.0,
      FID_Name|TSTRING,   "Input",
      FID_Category|TLONG, CCF_GUI,
      FID_Flags|TLONG,    CLF_PROMOTE_INTEGRAL|CLF_PRIVATE_ONLY,
      FID_Actions|TPTR,   clInputActions,
      FID_Fields|TARRAY,  clFields,
      FID_Size|TLONG,     sizeof(objInput),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND));
}

void free_input(void)
{
   if (clInput) { acFree(clInput); clInput = NULL; }
}
