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
#define PRV_WIDGET_MODULE
#include <parasol/modules/display.h>
#include <parasol/modules/surface.h>
#include <parasol/modules/widget.h>
#include <parasol/modules/vector.h>

#include "defs.h"

static OBJECTPTR clButton = NULL;

//****************************************************************************

static void style_trigger(objButton *Self, LONG Style)
{
   if (Self->prvStyleTrigger.Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Self->prvStyleTrigger.Script.Script)) {
         const ScriptArg args[] = {
            { "Button", FD_OBJECTPTR, { .Address = Self } },
            { "Style", FD_LONG,       { .Long = Style } }
         };
         scCallback(script, Self->prvStyleTrigger.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
      }
   }
}

//****************************************************************************

static ERROR BUTTON_ActionNotify(objButton *Self, struct acActionNotify *Args)
{
   if (Args->ActionID IS AC_Free) {
      if ((Self->prvFeedback.Type IS CALL_SCRIPT) and (Self->prvFeedback.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->prvFeedback.Type = CALL_NONE;
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
   parasol::Log log;
   log.branch();

   if (Self->Active) {
      log.warning("Warning - recursion detected");
      return ERR_Failed;
   }

   Self->Active = TRUE;

   if (Self->prvFeedback.Type IS CALL_STDC) {
      parasol::SwitchContext context(Self->prvFeedback.StdC.Context);
      auto routine = (void (*)(objButton *))Self->prvFeedback.StdC.Routine;
      routine(Self);
   }
   else if (Self->prvFeedback.Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Self->prvFeedback.Script.Script)) {
         const ScriptArg args[] = { { "Button", FD_OBJECTPTR, { .Address = Self } } };
         scCallback(script, Self->prvFeedback.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
      }
   }

   Self->Active = FALSE;
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Disable: Turns the button off.
-END-
*****************************************************************************/

static ERROR BUTTON_Disable(objButton *Self, APTR Void)
{
   Self->Flags |= BTF_DISABLED;
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Enable: Turns the button on if it has been disabled.
-END-
*****************************************************************************/

static ERROR BUTTON_Enable(objButton *Self, APTR Void)
{
   Self->Flags &= ~BTF_DISABLED;
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Focus: Sets the focus on the button and activates keyboard monitoring.
-END-
*****************************************************************************/

static ERROR BUTTON_Focus(objButton *Self, APTR Void)
{
   return acFocus(Self->Viewport);
}

//****************************************************************************

static ERROR BUTTON_Free(objButton *Self, APTR Void)
{
   if (Self->Icon)     { FreeResource(Self->Icon); Self->Icon = NULL; }
   if (Self->Viewport) { acFree(Self->Viewport); Self->Viewport = NULL; }
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Hide: Removes the button from the display.
-END-
*****************************************************************************/

static ERROR BUTTON_Hide(objButton *Self, APTR Void)
{
   Self->Flags |= BTF_HIDE;
   return acHide(Self->Viewport);
}

//****************************************************************************

static ERROR BUTTON_Init(objButton *Self, APTR Void)
{
   if (!Self->ParentViewport) { // Find our parent viewport
      OBJECTID owner_id;
      for (owner_id=GetOwner(Self); (owner_id); owner_id=GetOwnerID(owner_id)) {
         if (GetClassID(owner_id) IS ID_VECTOR) {
            Self->ParentViewport = (objVector *)GetObjectPtr(owner_id);
            if ((Self->ParentViewport->Head.SubID != ID_VECTORVIEWPORT) and
                (Self->ParentViewport->Head.SubID != ID_VECTORSCENE)) return ERR_UnsupportedOwner;
            else break;
         }
      }
      if (!owner_id) return ERR_UnsupportedOwner;
   }

   Self->Viewport->Parent = &Self->ParentViewport->Head;

   if (Self->Flags & BTF_HIDE) Self->Viewport->Visibility = VIS_HIDDEN;

   if (!acInit(Self->Viewport)) {
      if (drwApplyStyleGraphics(Self, Self->Viewport->Head.UniqueID, NULL, NULL)) {
         return ERR_Failed; // Graphics styling is required.
      }
/*
   Self->Viewport->Flags |= VF_GRAB_FOCUS;
   //if (Self->Flags & BTF_NO_FOCUS)
   Self->Viewport->Flags |= VF_IGNORE_FOCUS;
*/
      return ERR_Okay;
   }
   else return ERR_Init;
}

/*****************************************************************************
-ACTION-
Move: Move the button to a new position.
-END-
*****************************************************************************/

static ERROR BUTTON_Move(objButton *Self, struct acMove *Args)
{
   return Action(AC_Move, Self->Viewport, Args);
}

/*****************************************************************************
-ACTION-
MoveToBack: Moves the button to the back of the display area.
-END-
*****************************************************************************/

static ERROR BUTTON_MoveToBack(objButton *Self, APTR Void)
{
   return acMoveToBack(Self->Viewport);
}

/*****************************************************************************
-ACTION-
MoveToFront: Moves the button to the front of the display area.
-END-
*****************************************************************************/

static ERROR BUTTON_MoveToFront(objButton *Self, APTR Void)
{
   return acMoveToFront(Self->Viewport);
}

/*****************************************************************************
-ACTION-
MoveToPoint: Move the button to a new position.
-END-
*****************************************************************************/

static ERROR BUTTON_MoveToPoint(objButton *Self, struct acMoveToPoint *Args)
{
   return Action(AC_MoveToPoint, Self->Viewport, Args);
}

//****************************************************************************

static ERROR BUTTON_NewObject(objButton *Self, APTR Void)
{
   if (!NewObject(ID_VECTORVIEWPORT, NF_INTEGRAL, &Self->Viewport)) {
      drwApplyStyleValues(Self, NULL);
      return ERR_Okay;
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
   return Action(AC_Redimension, Self->Viewport, Args);
}

/*****************************************************************************
-ACTION-
Resize: Alters the size of the button.
-END-
*****************************************************************************/

static ERROR BUTTON_Resize(objButton *Self, struct acResize *Args)
{
   return Action(AC_Resize, Self->Viewport, Args);
}

/*****************************************************************************
-ACTION-
Show: Puts the button on display.
-END-
*****************************************************************************/

static ERROR BUTTON_Show(objButton *Self, APTR Void)
{
   return acShow(Self->Viewport);
}

/*****************************************************************************

-FIELD-
Bottom: The bottom coordinate of the button (Y + Height).

*****************************************************************************/

static ERROR GET_Bottom(objButton *Self, LONG *Value)
{
   DOUBLE y, height;
   if (!GetFields(Self->Viewport, FID_Y|TDOUBLE, &y, FID_Height|TDOUBLE, &height, TAGEND)) {
      *Value = F2T(y + height);
      return ERR_Okay;
   }
   else return ERR_GetField;
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
   if (Self->prvFeedback.Type != CALL_NONE) {
      *Value = &Self->prvFeedback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Feedback(objButton *Self, FUNCTION *Value)
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
Flags: Optional flags may be defined here.

-FIELD-
Height: Defines the height of a button.

A button can be given a fixed or relative height by setting this field to the desired value.  To set a relative
height, use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Height(objButton *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_Height, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_Height, &Value->Large);
   else return ERR_FieldTypeMismatch;
}

static ERROR SET_Height(objButton *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_Height, Value);
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
   if (Self->Hint) { FreeResource(Self->Hint); Self->Hint = NULL; }
   if (Value) Self->Hint = StrClone(StrTranslateText(Value));
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Icon: Reference to an icon that will be displayed inside the button.

To display an image inside the button, set the Icon field with a string in the format of 'category/iconname'.  The icon
will be displayed on the left side of the text inside the button.  If the button is unlabelled, the icon will be shown
in the exact center of the button.
-END-

*****************************************************************************/

static ERROR SET_Icon(objButton *Self, CSTRING Value)
{
   if (Self->Icon) { FreeResource(Self->Icon); Self->Icon = NULL; }
   if (Value) Self->Icon = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Right: The right coordinate of the button (X + Width).

*****************************************************************************/

static ERROR GET_Right(objButton *Self, LONG *Value)
{
   DOUBLE x, width;
   if (!GetFields(Self->Viewport, FID_X|TDOUBLE, &x, FID_Width|TDOUBLE, &width, TAGEND)) {
      *Value = F2T(x + width);
      return ERR_Okay;
   }
   else return ERR_GetField;
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

   if (Self->Head.Flags & NF_INITIALISED) style_trigger(Self, STYLE_CONTENT);

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
StyleTrigger: Requires a callback for reporting changes that can affect graphics styling.

This field is reserved for use by the style code that is managing the widget graphics.

*****************************************************************************/

static ERROR SET_StyleTrigger(objButton *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->prvStyleTrigger.Type IS CALL_SCRIPT) UnsubscribeAction(Self->prvStyleTrigger.Script.Script, AC_Free);
      Self->prvStyleTrigger = *Value;
      if (Self->prvStyleTrigger.Type IS CALL_SCRIPT) SubscribeAction(Self->prvStyleTrigger.Script.Script, AC_Free);
   }
   else Self->prvStyleTrigger.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
TabFocus: Set this field to a TabFocus object to register the button in a tab-list.

The TabFocus field provides a convenient way of linking the button to a @TabFocus object, allowing it to
receive the user focus via the tab key.  Do so by setting this field to the ID of the TabFocus object that is
representing the application's window.

*****************************************************************************/

static ERROR SET_TabFocus(objButton *Self, OBJECTPTR Value)
{
   if ((Value) and (Value->ClassID IS ID_TABFOCUS)) {
      tabAddObject(Value, Self->Viewport->Head.UniqueID);
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Width: Defines the width of a button.

A button can be given a fixed or relative width by setting this field to the desired value.  To set a relative width,
use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Width(objButton *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_Width, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_Width, &Value->Large);
   else return ERR_FieldTypeMismatch;
}

static ERROR SET_Width(objButton *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_Width, Value);
}

/*****************************************************************************

-FIELD-
X: The horizontal position of a button.

The horizontal position of a button can be set to an absolute or relative coordinate by writing a value to the X
field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be interpreted
as fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_X(objButton *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_X, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_X, &Value->Large);
   else return ERR_FieldTypeMismatch;
}

static ERROR SET_X(objButton *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_X, Value);
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

static ERROR GET_XOffset(objButton *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_XOffset, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_XOffset, &Value->Large);
   else return ERR_FieldTypeMismatch;
}

static ERROR SET_XOffset(objButton *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_XOffset, Value);
}

/*****************************************************************************

-FIELD-
Y: The vertical position of a button.

The vertical position of a Button can be set to an absolute or relative coordinate by writing a value to the Y
field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be interpreted
as fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_Y(objButton *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_Y, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_Y, &Value->Large);
   else return ERR_FieldTypeMismatch;
}

static ERROR SET_Y(objButton *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_Y, Value);
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

static ERROR GET_YOffset(objButton *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_YOffset, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_YOffset, &Value->Large);
   else return ERR_FieldTypeMismatch;
}

static ERROR SET_YOffset(objButton *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_YOffset, Value);
}

//****************************************************************************

#include "class_button_def.c"

static const FieldArray clFields[] = {
   { "Hint",           FDF_STRING|FDF_RW,    0, NULL, (APTR)SET_Hint },
   { "Icon",           FDF_STRING|FDF_RW,    0, NULL, (APTR)SET_Icon },
   { "Viewport",       FDF_OBJECT|FDF_R,     ID_VECTORVIEWPORT, NULL, NULL },
   { "ParentViewport", FDF_OBJECT|FDF_RI,    ID_VECTORVIEWPORT, NULL, NULL },
   { "Flags",          FDF_LONGFLAGS|FDF_RW, (MAXINT)&clButtonFlags, NULL, NULL },
   { "Clicked",        FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "HoverState",     FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clButtonHoverState, NULL, NULL },
   // Virtual fields
   { "Bottom",       FDF_VIRTUAL|FDF_LONG|FDF_R,      0, (APTR)GET_Bottom, NULL },
   { "Disabled",     FDF_VIRTUAL|FDF_LONG|FDF_RW,     0, (APTR)GET_Disabled, (APTR)SET_Disabled },
   { "Feedback",     FDF_VIRTUAL|FDF_FUNCTIONPTR|FDF_RW, 0, (APTR)GET_Feedback, (APTR)SET_Feedback },
   { "Right",        FDF_VIRTUAL|FDF_LONG|FDF_R,      0, (APTR)GET_Right, NULL },
   { "String",       FDF_VIRTUAL|FDF_STRING|FDF_RW,   0, (APTR)GET_String, (APTR)SET_String },
   { "StyleTrigger", FDF_VIRTUAL|FDF_FUNCTIONPTR|FDF_W,  0, NULL, (APTR)SET_StyleTrigger },
   { "TabFocus",     FDF_VIRTUAL|FDF_OBJECT|FDF_I,    ID_TABFOCUS, NULL, (APTR)SET_TabFocus },
   { "Text",         FDF_SYNONYM|FDF_VIRTUAL|FDF_STRING|FDF_RW, 0, (APTR)GET_String, (APTR)SET_String },
   // Variable Fields
   { "Height",       FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_Height,  (APTR)SET_Height },
   { "Width",        FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_Width,   (APTR)SET_Width },
   { "X",            FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_X,       (APTR)SET_X },
   { "XOffset",      FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_XOffset, (APTR)SET_XOffset },
   { "Y",            FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_Y,       (APTR)SET_Y },
   { "YOffset",      FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_YOffset, (APTR)SET_YOffset },
   END_FIELD
};

//****************************************************************************

ERROR init_button(void)
{
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
