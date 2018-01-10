/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Button: The Button class is used to create button widgets in the UI.

The Button class simplifies the creation and management of buttons as part of the user interface.  New buttons are
typically created by declaring the graphical dimensions and the text to be displayed within them.  The Button class
allows for the specifics of the button to be altered, such as the colours and the font style.

Default button values and the look and feel are applied using styles.

You will need to configure the button so that when it is clicked, it performs an action.  The methods to achieve this
are: Initialise child objects to the button for execution on activation; Listen to the Activate action by calling the
~Core.SubscribeAction() function on the button.

-END-

*****************************************************************************/

#define PRV_BUTTON
#include <parasol/modules/display.h>
#include <parasol/modules/document.h>
#include <parasol/modules/font.h>
#include <parasol/modules/iconserver.h>
#include <parasol/modules/picture.h>
#include <parasol/modules/surface.h>
#include <parasol/modules/widget.h>

#include "defs.h"

static const struct FieldDef clAlign[] = {
   { "Right",    ALIGN_RIGHT    }, { "Left",       ALIGN_LEFT    },
   { "Bottom",   ALIGN_BOTTOM   }, { "Top",        ALIGN_TOP     },
   { "Center",   ALIGN_CENTER   }, { "Middle",     ALIGN_MIDDLE  },
   { "Vertical", ALIGN_VERTICAL }, { "Horizontal", ALIGN_HORIZONTAL },
   { NULL, 0 }
};

static const struct FieldArray clFields[];
static const struct FieldDef clButtonHoverState[];
static OBJECTPTR clButton = NULL;

static char glDefaultButtonFace[] = "[glStyle./fonts/font(@name='button')/@face]:[glStyle./fonts/font(@name='button')/@size]";

static void draw_button(objButton *, objSurface *, objBitmap *);
static void key_event(objButton *, evKey *, LONG);

//****************************************************************************

static ERROR BUTTON_ActionNotify(objButton *Self, struct acActionNotify *Args)
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
      Self->Flags |= BTF_DISABLED;
      DelayMsg(AC_Draw, Self->RegionID, NULL);
   }
   else if (Args->ActionID IS AC_Enable) {
      Self->Flags &= ~BTF_DISABLED;
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
Activate: Activates the button.
-END-
*****************************************************************************/

static ERROR BUTTON_Activate(objButton *Self, APTR Void)
{
   LogBranch(NULL);

   if (Self->Active) {
      LogErrorMsg("Warning - recursion detected");
      LogBack();
      return ERR_Failed;
   }

   Self->Active = TRUE;

   if (Self->Feedback.Type IS CALL_STDC) {
      void (*routine)(objButton *);
      routine = Self->Feedback.StdC.Routine;

      if (Self->Feedback.StdC.Context) {
         OBJECTPTR context = SetContext(Self->Feedback.StdC.Context);
         routine(Self);
         SetContext(context);
      }
      else routine(Self);
   }
   else if (Self->Feedback.Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Self->Feedback.Script.Script)) {
         const struct ScriptArg args[] = {
            { "Button", FD_OBJECTPTR, { .Address = Self } }
         };
         scCallback(script, Self->Feedback.Script.ProcedureID, args, ARRAYSIZE(args));
      }
   }

   if ((Self->Onclick) AND (Self->Document)) {
      docCallFunction(Self->Document, Self->Onclick, NULL, 0);
   }

   Self->Active = FALSE;

   LogBack();
   return ERR_Okay;
}

//****************************************************************************

static ERROR BUTTON_DataFeed(objButton *Self, struct acDataFeed *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   if (Args->DataType IS DATA_INPUT_READY) {
      struct InputMsg *input;
      objSurface *surface;

      while (!gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &input)) {
         if (input->Type IS JET_ENTERED_SURFACE) {
            Self->HoverState = BHS_INSIDE;

            if (!(Self->Flags & BTF_DISABLED)) {
               if (!AccessObject(Self->RegionID, 2000, &surface)) {
                  if (!(surface->Flags & RNF_DISABLED)) {
                     DelayMsg(AC_Draw, Self->RegionID, NULL);
                  }
                  ReleaseObject(surface);
               }
            }
         }
         else if (input->Type IS JET_LEFT_SURFACE) {
            Self->HoverState = BHS_OUTSIDE;

            if (!AccessObject(Self->RegionID, 2000, &surface)) {
               if (!(surface->Flags & RNF_DISABLED)) {
                  DelayMsg(AC_Draw, Self->RegionID, NULL);
               }
               ReleaseObject(surface);
            }
         }
         else if (input->Type IS JET_LMB) {
            if (input->Value > 0) {
               if (Self->Flags & BTF_DISABLED) continue;

               if (input->Flags & JTYPE_REPEATED) {
                  if (Self->Flags & BTF_PULSE) acActivate(Self);
               }
               else {
                  Self->Clicked = TRUE;
                  Self->ClickX  = input->X;
                  Self->ClickY  = input->Y;
                  DelayMsg(AC_Draw, Self->RegionID, NULL);
               }
            }
            else if (Self->Clicked) {
               Self->Clicked = FALSE;
               LONG clickx = input->X - Self->ClickX;
               LONG clicky = input->Y - Self->ClickY;
               if (clickx < 0) clickx = -clickx;
               if (clicky < 0) clicky = -clicky;

               acDrawID(Self->RegionID);

               if (((clickx < 4) AND (clicky < 4)) OR (Self->Flags & BTF_PULSE)) acActivate(Self);
            }
         }
      }
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Disable: Turns the button off.
-END-
*****************************************************************************/

static ERROR BUTTON_Disable(objButton *Self, APTR Void)
{
   // See the ActionNotify routine to see what happens when the surface is disabled.

   LogAction(NULL);
   acDisableID(Self->RegionID);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Enable: Turns the button on if it has been disabled.
-END-
*****************************************************************************/

static ERROR BUTTON_Enable(objButton *Self, APTR Void)
{
   // See the ActionNotify routine to see what happens when the surface is enabled.

   LogAction(NULL);
   acEnableID(Self->RegionID);
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Focus: Sets the focus on the button and activates keyboard monitoring.
-END-
*****************************************************************************/

static ERROR BUTTON_Focus(objButton *Self, APTR Void)
{
   return acFocusID(Self->RegionID);
}

//****************************************************************************

static ERROR BUTTON_Free(objButton *Self, APTR Void)
{
   if (Self->prvKeyEvent) { UnsubscribeEvent(Self->prvKeyEvent); Self->prvKeyEvent = NULL; }
   if (Self->IconFilter)  { FreeMemory(Self->IconFilter); Self->IconFilter = NULL; }
   if (Self->Image)       { FreeMemory(Self->Image); Self->Image = NULL; }
   if (Self->Picture)     { acFree(Self->Picture); Self->Picture = NULL; }
   else if (Self->Bitmap) { acFree(Self->Bitmap); Self->Bitmap = NULL; }
   if (Self->Font)        { acFree(Self->Font); Self->Font = NULL; }
   if (Self->RegionID)    { acFreeID(Self->RegionID); Self->RegionID = 0; }

   gfxUnsubscribeInput(0); // Unsubscribe our object from all surfaces

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Hide: Removes the button from the display.
-END-
*****************************************************************************/

static ERROR BUTTON_Hide(objButton *Self, APTR Void)
{
   acHideID(Self->RegionID);
   return ERR_Okay;
}

//****************************************************************************

static ERROR BUTTON_Init(objButton *Self, APTR Void)
{
   if (!Self->SurfaceID) { // Find our parent surface
      OBJECTID owner_id = GetOwner(Self);
      while ((owner_id) AND (GetClassID(owner_id) != ID_SURFACE)) {
         owner_id = GetOwnerID(owner_id);
      }
      if (owner_id) Self->SurfaceID = owner_id;
      else return ERR_UnsupportedOwner;
   }

   if (acInit(Self->Font) != ERR_Okay) return ERR_Init;

   if (Self->Image) {
      objPicture *picture;
      ERROR error = ERR_Okay;

      if (!StrCompare("icons:", Self->Image, 6, 0)) {
         objBitmap *bitmap;
         if (!(error = iconCreateIcon(Self->Image+6, "Button", "Default", Self->IconFilter, 0, &bitmap))) {
            Self->Bitmap = bitmap;
         }
      }
      else if (!CreateObject(ID_PICTURE, NF_INTEGRAL, &picture,
            FID_Flags|TLONG, PCF_FORCE_ALPHA_32,
            FID_Path|TSTR,   Self->Image,
            TAGEND)) {
         if (!acActivate(picture)) {
            Self->Picture = picture;
            Self->Bitmap = picture->Bitmap;
         }
         else {
            acFree(picture);
            error = ERR_Activate;
         }
      }
      else error = ERR_CreateObject;

      if (error) return error;
   }

   objSurface *region;
   if (!AccessObject(Self->RegionID, 5000, &region)) {
      region->Flags |= RNF_GRAB_FOCUS;

      if (Self->Flags & BTF_NO_FOCUS) region->Flags |= RNF_IGNORE_FOCUS;

      SetFields(region, FID_Parent|TLONG, Self->SurfaceID,
                        FID_Region|TLONG, TRUE,
                        TAGEND);

      if (!(region->Dimensions & DMF_HEIGHT)) {
         if ((!(region->Dimensions & DMF_Y)) OR (!(region->Dimensions & DMF_Y_OFFSET))) {
            SetLong(region, FID_Height, Self->Font->MaxHeight + (glMargin * 2)); // glMargin added for both the top and bottom of the button.
         }
      }

      if (!(region->Dimensions & DMF_WIDTH)) {
         if ((!(region->Dimensions & DMF_X)) OR (!(region->Dimensions & DMF_X_OFFSET))) {
            LONG w = (glMargin * 4) + fntStringWidth(Self->Font, Self->String, -1);
            if (Self->Bitmap) w += Self->Bitmap->Width + glMargin;
            SetLong(region, FID_Width, w);
         }
      }

      if (!acInit(region)) {
         SubscribeActionTags(region,
            AC_Disable,
            AC_Enable,
            AC_Focus,
            AC_LostFocus,
            TAGEND);
      }
      else {
         ReleaseObject(region);
         return ERR_Init;
      }

      gfxSubscribeInput(Self->RegionID, JTYPE_FEEDBACK|JTYPE_BUTTON|JTYPE_REPEATED, 0);

      ReleaseObject(region);
   }
   else return ERR_AccessObject;

   if (drwApplyStyleGraphics(Self, Self->RegionID, NULL, NULL)) {
      return ERR_Failed; // Graphics styling is required.
   }

   // Subscription comes after creation of template graphics

   if (!AccessObject(Self->RegionID, 5000, &region)) {
      drwAddCallback(region, &draw_button);
      ReleaseObject(region);
   }
   else return ERR_AccessObject;

   if (!(Self->Flags & BTF_HIDE)) acShowID(Self->RegionID);

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Move: Move the button to a new location.
-END-
*****************************************************************************/

static ERROR BUTTON_Move(objButton *Self, struct acMove *Args)
{
   return ActionMsg(AC_Move, Self->RegionID, Args);
}

/*****************************************************************************
-ACTION-
MoveToPoint: Move the button to a new location.
-END-
*****************************************************************************/

static ERROR BUTTON_MoveToPoint(objButton *Self, struct acMoveToPoint *Args)
{
   return ActionMsg(AC_MoveToPoint, Self->RegionID, Args);
}

/*****************************************************************************
-ACTION-
MoveToBack: Moves the button to the back of the display area.
-END-
*****************************************************************************/

static ERROR BUTTON_MoveToBack(objButton *Self, APTR Void)
{
   return acMoveToBackID(Self->RegionID);
}

/*****************************************************************************
-ACTION-
MoveToFront: Moves the button to the front of the display area.
-END-
*****************************************************************************/

static ERROR BUTTON_MoveToFront(objButton *Self, APTR Void)
{
   return acMoveToFrontID(Self->RegionID);
}

//****************************************************************************

static ERROR BUTTON_NewObject(objButton *Self, APTR Void)
{
   if (!NewLockedObject(ID_SURFACE, NF_INTEGRAL|Self->Head.Flags, NULL, &Self->RegionID)) {
      if (!NewObject(ID_FONT, NF_INTEGRAL|Self->Head.Flags, &Self->Font)) {
         SetString(Self->Font, FID_Face, glDefaultButtonFace);

         drwApplyStyleValues(Self, NULL);

         return ERR_Okay;
      }
      else return ERR_NewObject;
   }
   else return ERR_NewObject;
}

/*****************************************************************************
-ACTION-
Redimension: Changes the size and position of the button.
-END-
*****************************************************************************/

static ERROR BUTTON_Redimension(objButton *Self, struct acRedimension *Args)
{
   return ActionMsg(AC_Redimension, Self->RegionID, Args);
}

/*****************************************************************************
-ACTION-
Resize: Alters the size of the button.
-END-
*****************************************************************************/

static ERROR BUTTON_Resize(objButton *Self, struct acResize *Args)
{
   return ActionMsg(AC_Resize, Self->RegionID, Args);
}

/*****************************************************************************
-ACTION-
Show: Puts the button on display.
-END-
*****************************************************************************/

static ERROR BUTTON_Show(objButton *Self, APTR Void)
{
   acShowID(Self->RegionID);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Align: Manages the alignment of a button surface within its container.

This field is a proxy for the @Surface.Align field and will align the button within its container.

*****************************************************************************/

static ERROR SET_Align(objButton *Self, LONG Value)
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
Bottom: The bottom coordinate of the button (Y + Height).

*****************************************************************************/

static ERROR GET_Bottom(objButton *Self, LONG *Value)
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
Disabled: TRUE if the button is disabled, otherwise FALSE.

Read the Disabled to determine if the button is disabled (TRUE) or not (FALSE). It is possible to set this field to
change the disabled state, however we recommend that you use the #Disable() and #Enable() actions to do
this.

*****************************************************************************/

static ERROR GET_Disabled(objButton *Self, LONG *Value)
{
   if (Self->Flags & BTF_DISABLED) *Value = TRUE;
   else *Value = FALSE;
   return ERR_Okay;
}

static ERROR SET_Disabled(objButton *Self, LONG Value)
{
   if (Value IS TRUE) acDisable(Self);
   else if (Value IS FALSE) acEnable(Self);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Feedback: Provides instant feedback when a user interacts with the button.

Set the Feedback field with a callback function in order to receive instant feedback when user interaction occurs.  The
function prototype is `routine(*Button)`

*****************************************************************************/

static ERROR GET_Feedback(objButton *Self, FUNCTION **Value)
{
   if (Self->Feedback.Type != CALL_NONE) {
      *Value = &Self->Feedback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Feedback(objButton *Self, FUNCTION *Value)
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
Font: References the font that will draw text inside the button.

All buttons have a font object that is referenced from this field.  The fields of the font object may be set prior to
initialisation in order to configure the style of the button's text.  It is recommended that if you wish to configure
the font style, please do so applying a style to the button.

-FIELD-
Flags: Optional flags may be defined here.

-FIELD-
Height: Defines the height of a button.

A button can be given a fixed or relative height by setting this field to the desired value.  To set a relative
height, use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Height(objButton *Self, struct Variable *Value)
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

static ERROR SET_Height(objButton *Self, struct Variable *Value)
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
Hint: Applies a hint to a button.

A hint can be displayed when the mouse pointer remains motionless over a button for a short period of time.  The text
that is displayed in the hint box is set in this field.  The string must be in UTF-8 format and be no longer than one
line.  The string should be written in english and will be automatically translated to the user's native language when
the field is set.

*****************************************************************************/

static ERROR SET_Hint(objButton *Self, CSTRING Value)
{
   if (Self->Hint) { FreeMemory(Self->Hint); Self->Hint = NULL; }
   if (Value) Self->Hint = StrClone(StrTranslateText(Value));
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
IconFilter: Sets the preferred icon filter.

Setting the IconFilter will change the default graphics filter when loading an icon (identified when using the 'icons:'
volume name).

*****************************************************************************/

static ERROR SET_IconFilter(objButton *Self, CSTRING Value)
{
   if (Self->IconFilter) { FreeMemory(Self->IconFilter); Self->IconFilter = NULL; }
   if (Value) Self->IconFilter = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Image: The image field can be set in order to load a bitmap into the button.

To display an image inside the button, set the Image field.  The image will be displayed on the left side of the text
inside the button.  If no text string has been set, the image will be shown in the exact center of the button.
-END-

*****************************************************************************/

static ERROR SET_Image(objButton *Self, CSTRING Value)
{
   if (Self->Image) { FreeMemory(Self->Image); Self->Image = NULL; }
   if (Value) Self->Image = StrClone(Value);
   return ERR_Okay;
}

//****************************************************************************
// Internal field for supporting dynamic style changes when a GUI object is used in a Document.

static ERROR SET_LayoutStyle(objButton *Self, DOCSTYLE *Value)
{
   if (!Value) return ERR_Okay;

   if (Self->Head.Flags & NF_INITIALISED) {
      docApplyFontStyle(Value->Document, Value, Self->Font);
   }
   else docApplyFontStyle(Value->Document, Value, Self->Font);

   Self->Document = Value->Document;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Onclick: Available when a button is declared in a document.  References a function to call when clicked.

This field can only be used if the button has been created within a @Document.  It must reference the name of
a function that will be called when the button is clicked.

A function from a specific script can be referenced by using the name format 'script.function'.

*****************************************************************************/

static ERROR GET_Onclick(objButton *Self, STRING *Value)
{
   *Value = Self->Onclick;
   return ERR_Okay;
}

static ERROR SET_Onclick(objButton *Self, CSTRING Value)
{
   if (Self->Onclick) { FreeMemory(Self->Onclick); Self->Onclick = NULL; }
   if (Value) Self->Onclick = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Region: The surface that represents the button graphic.

The surface area that represents the button display can be accessed through this field.  For further information, refer
to the @Surface class.  Note that interfacing with the surface directly can have adverse effects on the button
control system.  Where possible, all communication should be limited to the button object itself.

-FIELD-
Right: The right coordinate of the button (X + Width).

*****************************************************************************/

static ERROR GET_Right(objButton *Self, LONG *Value)
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
String: The string that is to be printed inside the button is declared here.

The string that you would like to be displayed in the button is specified in this field.  The string must be in UTF-8
format and be no longer than one line.  The string should be written in English and will be automatically translated
to the user's native language when the field is set.

If the string is changed after initialisation, the button will be redrawn to show the updated text.

*****************************************************************************/

static ERROR GET_String(objButton *Self, STRING *Value)
{
   if (Self->String[0]) {
      *Value = Self->String;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_String(objButton *Self, CSTRING Value)
{
   if (Value) StrCopy(StrTranslateText(Value), Self->String, sizeof(Self->String));
   else Self->String[0] = 0;

   // Send a redraw message

   if (Self->Head.Flags & NF_INITIALISED) DelayMsg(AC_Draw, Self->RegionID, NULL);

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Surface: The surface that will contain the button graphic.

The surface that will contain the button graphic is set here.  If this field is not set prior to initialisation, the
button will attempt to scan for the correct surface by analysing its parents until it finds a suitable candidate.

-FIELD-
TabFocus: Set this field to a TabFocus object to register the button in a tab-list.

The TabFocus field provides a convenient way of adding the button to a @TabFocus object, so that it can
receive the user focus via the tab key.  Simply set this field to the ID of the TabFocus object that is managing the
tab-list for the application window.

*****************************************************************************/

static ERROR SET_TabFocus(objButton *Self, OBJECTPTR Value)
{
   if ((Value) AND (Value->ClassID IS ID_TABFOCUS)) {
      tabAddObject(Value, Self->RegionID);
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Width: Defines the width of a button.

A button can be given a fixed or relative width by setting this field to the desired value.  To set a relative width,
use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Width(objButton *Self, struct Variable *Value)
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

static ERROR SET_Width(objButton *Self, struct Variable *Value)
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
X: The horizontal position of a button.

The horizontal position of a button can be set to an absolute or relative coordinate by writing a value to the X
field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be interpreted
as fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_X(objButton *Self, struct Variable *Value)
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

static ERROR SET_X(objButton *Self, struct Variable *Value)
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
XOffset: The horizontal offset of a button.

The XOffset has a dual purpose depending on whether or not it is set in conjunction with an X coordinate or a Width
based field.

If set in conjunction with an X coordinate then the button will be drawn from that X coordinate up to the width of the
container, minus the value given in the XOffset.  This means that the width of the Button is dynamically calculated in
relation to the width of the container.

If the XOffset field is set in conjunction with a fixed or relative width then the button will be drawn at an X
coordinate calculated from the formula `X = ContainerWidth - ButtonWidth - XOffset`.

*****************************************************************************/

static ERROR GET_XOffset(objButton *Self, struct Variable *Value)
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

static ERROR SET_XOffset(objButton *Self, struct Variable *Value)
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
Y: The vertical position of a button.

The vertical position of a Button can be set to an absolute or relative coordinate by writing a value to the Y
field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be interpreted
as fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_Y(objButton *Self, struct Variable *Value)
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

static ERROR SET_Y(objButton *Self, struct Variable *Value)
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
YOffset: The vertical offset of a button.

The YOffset has a dual purpose depending on whether or not it is set in conjunction with a Y coordinate or a Height
based field.

If set in conjunction with a Y coordinate then the button will be drawn from that Y coordinate up to the height of the
container, minus the value given in the YOffset.  This means that the height of the button is dynamically calculated in
relation to the height of the container.

If the YOffset field is set in conjunction with a fixed or relative height then the button will be drawn at a Y
coordinate calculated from the formula `Y = ContainerHeight - ButtonHeight - YOffset`.
-END-

*****************************************************************************/

static ERROR GET_YOffset(objButton *Self, struct Variable *Value)
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

static ERROR SET_YOffset(objButton *Self, struct Variable *Value)
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

static void key_event(objButton *Self, evKey *Event, LONG Size)
{
   if (!(Event->Qualifiers & KQ_PRESSED)) return;

   if ((Event->Code IS K_ENTER) OR (Event->Code IS K_NP_ENTER) OR (Event->Code IS K_SPACE)) {
      LogBranch("Enter or Space key detected.");
         acActivate(Self);
      LogBack();
   }
}

//****************************************************************************

static void draw_button(objButton *Self, objSurface *Surface, objBitmap *Bitmap)
{
   if (Self->Bitmap) {
      objBitmap *bmp = Self->Bitmap;
      gfxCopyArea(bmp, Bitmap, BAF_BLEND, 0, 0, bmp->Width, bmp->Height, glMargin * 2, (Surface->Height - bmp->Height)/2);
   }

   objFont *font = Self->Font;
   font->Bitmap = Bitmap;

   {
      SetString(font, FID_String, Self->String);

      if (Surface->Flags & RNF_DISABLED) SetLong(font, FID_Opacity, 25);

      if (Self->Bitmap) {
         font->X = (glMargin * 2) + Self->Bitmap->Width + glMargin;
         font->Y = 0;
         font->Align = ALIGN_VERTICAL;
         font->AlignWidth = Surface->Width;
         font->AlignHeight = Surface->Height;
      }
      else {
         font->X = 0;
         font->Y = 0;
         font->Align = ALIGN_CENTER;
         font->AlignWidth = Surface->Width;
         font->AlignHeight = Surface->Height;
      }

      acDraw(font);
   }

   if (Surface->Flags & RNF_DISABLED) SetLong(font, FID_Opacity, 100);
}

//****************************************************************************

#include "class_button_def.c"

static const struct FieldArray clFields[] = {
   { "Font",         FDF_INTEGRAL|FDF_R,   0, NULL, NULL },
   { "Hint",         FDF_STRING|FDF_RW,    0, NULL, SET_Hint },
   { "IconFilter",   FDF_STRING|FDF_RW,    0, NULL, SET_IconFilter },
   { "Image",        FDF_STRING|FDF_RW,    0, NULL, SET_Image },
   { "LayoutSurface",FDF_VIRTUAL|FDF_OBJECTID|FDF_SYSTEM|FDF_R, ID_SURFACE, NULL, NULL }, // VIRTUAL: This is a synonym for the Region field
   { "Region",       FDF_OBJECTID|FDF_R,   ID_SURFACE, NULL, NULL },
   { "Surface",      FDF_OBJECTID|FDF_RW,  ID_SURFACE, NULL, NULL },
   { "Flags",        FDF_LONGFLAGS|FDF_RW, (MAXINT)&clButtonFlags, NULL, NULL },
   { "Clicked",      FDF_LONG|FDF_R,       0, NULL, NULL },
   { "HoverState",   FDF_LONG|FDF_LOOKUP|FDF_R, (MAXINT)&clButtonHoverState, NULL, NULL },
   // Virtual fields
   { "Align",        FDF_VIRTUAL|FDF_LONGFLAGS|FDF_I, (MAXINT)&clAlign, NULL, SET_Align },
   { "Bottom",       FDF_VIRTUAL|FDF_LONG|FDF_R,      0, GET_Bottom, NULL },
   { "Disabled",     FDF_VIRTUAL|FDF_LONG|FDF_RW,     0, GET_Disabled, SET_Disabled },
   { "Feedback",     FDF_VIRTUAL|FDF_FUNCTIONPTR|FDF_RW, 0, GET_Feedback, SET_Feedback },
   { "LayoutStyle",  FDF_VIRTUAL|FDF_POINTER|FDF_SYSTEM|FDF_W, 0, NULL, SET_LayoutStyle },
   { "Onclick",      FDF_VIRTUAL|FDF_STRING|FDF_RW,   0, GET_Onclick, SET_Onclick },
   { "Right",        FDF_VIRTUAL|FDF_LONG|FDF_R,      0, GET_Right, NULL },
   { "String",       FDF_VIRTUAL|FDF_STRING|FDF_RW,   0, GET_String, SET_String },
   { "TabFocus",     FDF_VIRTUAL|FDF_OBJECT|FDF_W,    ID_TABFOCUS, NULL, SET_TabFocus },
   { "Text",         FDF_SYNONYM|FDF_VIRTUAL|FDF_STRING|FDF_RW, 0, GET_String, SET_String },
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

ERROR init_button(void)
{
   // Note that if the app wants to set its own default face, it should do this prior to GUI initialisation.

   if (StrEvaluate(glDefaultButtonFace, sizeof(glDefaultButtonFace), SEF_STRICT, 0) != ERR_Okay) {
      StrCopy("Open Sans,Source Sans Pro,*:100%", glDefaultButtonFace, sizeof(glDefaultButtonFace));
   }

   return(CreateObject(ID_METACLASS, 0, &clButton,
      FID_ClassVersion|TFLOAT, VER_BUTTON,
      FID_Name|TSTR,      "Button",
      FID_Category|TLONG, CCF_GUI,
      FID_Flags|TLONG,    CLF_PROMOTE_INTEGRAL|CLF_PRIVATE_ONLY,
      FID_Actions|TPTR,   clButtonActions,
      FID_Fields|TARRAY,  clFields,
      FID_Size|TLONG,     sizeof(objButton),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND));
}

void free_button(void)
{
   if (clButton) { acFree(clButton); clButton = NULL; }
}
