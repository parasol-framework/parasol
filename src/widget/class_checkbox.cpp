/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
CheckBox: The CheckBox class displays a checkbox widget in the UI.

The CheckBox class simplifies the creation and management of checkbox widgets in the user interface.  Check boxes are
simple widgets that are limited to exhibiting an on/off state.  The CheckBox class allows for its graphics to be
customised, so it is possible to redefine how the on/off states are displayed.

To respond to user interaction with the Checkbox, set the #Feedback field with a callback function.
-END-

*****************************************************************************/

#define PRV_CHECKBOX
#define PRV_WIDGET_MODULE
#include <parasol/modules/widget.h>
#include <parasol/modules/vector.h>
#include "defs.h"

static OBJECTPTR clCheckBox = NULL;

//****************************************************************************

static void style_trigger(objCheckBox *Self, LONG Style)
{
   if (Self->prvStyleTrigger.Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Self->prvStyleTrigger.Script.Script)) {
         const ScriptArg args[] = {
            { "CheckBox", FD_OBJECTPTR, { .Address = Self } },
            { "Style", FD_LONG,         { .Long = Style } }
         };
         scCallback(script, Self->prvStyleTrigger.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
      }
   }
}

//****************************************************************************

static ERROR CHECKBOX_ActionNotify(objCheckBox *Self, struct acActionNotify *Args)
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
Disable: Disables the checkbox.
-END-
*****************************************************************************/

static ERROR CHECKBOX_Disable(objCheckBox *Self, APTR Void)
{
   Self->Flags |= CBF_DISABLED;
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Enable: Turns the checkbox on if it has been disabled.
-END-
*****************************************************************************/

static ERROR CHECKBOX_Enable(objCheckBox *Self, APTR Void)
{
   Self->Flags &= ~CBF_DISABLED;
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Focus: Sets the focus on the checkbox and activates keyboard monitoring.
-END-
*****************************************************************************/

static ERROR CHECKBOX_Focus(objCheckBox *Self, APTR Void)
{
   return acFocus(Self->Viewport);
}

//****************************************************************************

static ERROR CHECKBOX_Free(objCheckBox *Self, APTR Void)
{
   if (Self->prvKeyEvent) { UnsubscribeEvent(Self->prvKeyEvent); Self->prvKeyEvent = NULL; }
   if (Self->Viewport)    { acFree(Self->Viewport); Self->Viewport = NULL; }
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
   return acHide(Self->Viewport);
}

//****************************************************************************

static ERROR CHECKBOX_Init(objCheckBox *Self, APTR Void)
{
   parasol::Log log;

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

   if (Self->Flags & CBF_HIDE) Self->Viewport->Visibility = VIS_HIDDEN;

   if (!acInit(Self->Viewport)) {
      if (drwApplyStyleGraphics(Self, Self->Viewport->Head.UniqueID, NULL, NULL)) {
         return ERR_Failed; // Graphics styling is required.
      }

      //region->Flags |= RNF_GRAB_FOCUS;
   }
   else return ERR_AccessObject;

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
MoveToBack: Moves the checkbox to the back of the display area.
-END-
*****************************************************************************/

static ERROR CHECKBOX_MoveToBack(objCheckBox *Self, APTR Void)
{
   return acMoveToBack(Self->Viewport);
}

/*****************************************************************************
-ACTION-
MoveToFront: Moves the checkbox to the front of the display area.
-END-
*****************************************************************************/

static ERROR CHECKBOX_MoveToFront(objCheckBox *Self, APTR Void)
{
   return acMoveToFront(Self->Viewport);
}

//****************************************************************************

static ERROR CHECKBOX_NewObject(objCheckBox *Self, APTR Void)
{
   if (!NewObject(ID_VECTORVIEWPORT, NF_INTEGRAL, &Self->Viewport)) {
      drwApplyStyleValues(Self, NULL);
      return ERR_Okay;
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
   return Action(AC_Redimension, Self->Viewport, Args);
}

/*****************************************************************************
-ACTION-
Resize: Alters the size of the checkbox.
-END-
*****************************************************************************/

static ERROR CHECKBOX_Resize(objCheckBox *Self, struct acResize *Args)
{
   return Action(AC_Resize, Self->Viewport, Args);
}

/*****************************************************************************
-ACTION-
Show: Make the checkbox visible.
-END-
*****************************************************************************/

static ERROR CHECKBOX_Show(objCheckBox *Self, APTR Void)
{
   Self->Flags &= ~CBF_HIDE;
   return acShow(Self->Viewport);
}

/*****************************************************************************

-FIELD-
Align: Affects the alignment of the checkbox widget within its target surface.

By default the checkbox widget will be aligned to the top left of its target surface.  The checkbox can be aligned to
the right by setting the ALIGN_RIGHT flag.

-FIELD-
Bottom: The bottom coordinate of the checkbox (Y + Height).

*****************************************************************************/

static ERROR GET_Bottom(objCheckBox *Self, LONG *Value)
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
Disable: Disables the checkbox on initialisation.

The checkbox can be disabled on initialisation by setting this field to TRUE.  If you need to disable the combobox
after it has been activated, it is preferred that you use the #Disable() action.

To enable the combobox after it has been disabled, use the #Enable() action.

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
Feedback: Provides instant feedback when a user interacts with the checkbox.

Set the Feedback field with a callback function in order to receive instant feedback when user interaction occurs.  The
function prototype is `routine(*CheckBox, LONG Status)`

*****************************************************************************/

static ERROR GET_Feedback(objCheckBox *Self, FUNCTION **Value)
{
   if (Self->prvFeedback.Type != CALL_NONE) {
      *Value = &Self->prvFeedback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Feedback(objCheckBox *Self, FUNCTION *Value)
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
Flags: Optional flags.
Lookup: CBF

-FIELD-
Height: Defines the height of a checkbox.

A checkbox can be given a fixed or relative height by setting this field to the desired value.  To set a relative
height, use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Height(objCheckBox *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_Height, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_Height, &Value->Large);
   else return ERR_FieldTypeMismatch;
}

static ERROR SET_Height(objCheckBox *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_Height, Value);
}

/*****************************************************************************

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

/*****************************************************************************

-FIELD-
Right: The right coordinate of the checkbox (X + Width).

*****************************************************************************/

static ERROR GET_Right(objCheckBox *Self, LONG *Value)
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
StyleTrigger: Requires a callback for reporting changes that can affect graphics styling.

This field is reserved for use by the style code that is managing the widget graphics.

*****************************************************************************/

static ERROR SET_StyleTrigger(objCheckBox *Self, FUNCTION *Value)
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
         tabAddObject(tabfocus, Self->Viewport->Head.UniqueID);
      }
      ReleaseObject(tabfocus);
   }
   else return ERR_AccessObject;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Status: Indicates the current on/off state of the checkbox.

To get the on/off state of the checkbox, read this field.  It can also be set at run-time to change the checkbox to
an on or off state.  Only values of 0 (off) and 1 (on) are valid.

If the state is altered post-initialisation, the UI will be updated and the #Feedback function will be called
with the new state value.

*****************************************************************************/

static ERROR SET_Status(objCheckBox *Self, LONG Value)
{
   parasol::Log log;

   log.branch();

   if ((Value != TRUE) and (Value != FALSE)) return log.warning(ERR_InvalidValue);

   if (Self->Head.Flags & NF_INITIALISED) {
      if (Self->Status != Value) {
         if (Self->Active) return log.warning(ERR_Recursion);
         Self->Active = TRUE;
         Self->Status = Value;
         style_trigger(Self, STYLE_CONTENT);

         if (Self->prvFeedback.Type IS CALL_STDC) {
            parasol::SwitchContext context(Self->prvFeedback.StdC.Context);
            auto routine = (void (*)(APTR, objCheckBox *, LONG))Self->prvFeedback.StdC.Routine;
            routine(Self->prvFeedback.StdC.Context, Self, Self->Status);
         }
         else if (Self->prvFeedback.Type IS CALL_SCRIPT) {
            OBJECTPTR script;
            if ((script = Self->prvFeedback.Script.Script)) {
               const ScriptArg args[] = {
                  { "CheckBox", FD_OBJECTPTR, { .Address = Self } },
                  { "Status", FD_LONG, { .Long = Self->Status } }
               };
               scCallback(script, Self->prvFeedback.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
            }
         }

         Self->Active = FALSE;
      }
   }
   else Self->Status = Value;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Width: Defines the width of a checkbox.

A checkbox can be given a fixed or relative width by setting this field to the desired value.  To set a relative width,
use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Width(objCheckBox *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_Width, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_Width, &Value->Large);
   else return ERR_FieldTypeMismatch;
}

static ERROR SET_Width(objCheckBox *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_Width, Value);
}

/*****************************************************************************

-FIELD-
X: The horizontal position of a checkbox.

The horizontal position of a checkbox can be set to an absolute or relative coordinate by writing a value to the X
field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be interpreted as
fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_X(objCheckBox *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_X, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_X, &Value->Large);
   else return ERR_FieldTypeMismatch;
}

static ERROR SET_X(objCheckBox *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_X, Value);
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

static ERROR GET_XOffset(objCheckBox *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_XOffset, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_XOffset, &Value->Large);
   else return ERR_FieldTypeMismatch;
}

static ERROR SET_XOffset(objCheckBox *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_XOffset, Value);
}

/*****************************************************************************

-FIELD-
Y: The vertical position of a checkbox.

The vertical position of a CheckBox can be set to an absolute or relative coordinate by writing a value to the Y
field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be interpreted
as fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_Y(objCheckBox *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_Y, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_Y, &Value->Large);
   else return ERR_FieldTypeMismatch;}

static ERROR SET_Y(objCheckBox *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_Y, Value);
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

static ERROR GET_YOffset(objCheckBox *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_YOffset, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_YOffset, &Value->Large);
   else return ERR_FieldTypeMismatch;
}

static ERROR SET_YOffset(objCheckBox *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_YOffset, Value);
}

//****************************************************************************

#include "class_checkbox_def.c"

static const FieldArray clFields[] = {
   { "Viewport",       FDF_OBJECT|FDF_R,     ID_VECTORVIEWPORT, NULL, NULL },
   { "ParentViewport", FDF_OBJECT|FDF_RI,    ID_VECTORVIEWPORT, NULL, NULL },
   { "Flags",          FDF_LONGFLAGS|FDF_RW, (MAXINT)&clCheckBoxFlags, NULL, NULL },
   { "LabelWidth",     FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Status",         FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_Status },
   { "Align",          FDF_LONGFLAGS|FDF_RW, (MAXINT)&clCheckBoxAlign, NULL, NULL },
   // Virtual fields
   { "Bottom",       FDF_VIRTUAL|FDF_LONG|FDF_R,               0, (APTR)GET_Bottom, NULL },
   { "Disable",      FDF_VIRTUAL|FDF_LONG|FDF_RW,              0, (APTR)GET_Disable, (APTR)SET_Disable },
   { "Feedback",     FDF_VIRTUAL|FDF_FUNCTIONPTR|FDF_RW,       0, (APTR)GET_Feedback, (APTR)SET_Feedback },
   { "Label",        FDF_VIRTUAL|FDF_STRING|FDF_RW,            0, (APTR)GET_Label, (APTR)SET_Label },
   { "Right",        FDF_VIRTUAL|FDF_LONG|FDF_R,               0, (APTR)GET_Right, NULL },
   { "StyleTrigger", FDF_VIRTUAL|FDF_FUNCTIONPTR|FDF_W,        0, NULL, (APTR)SET_StyleTrigger },
   { "TabFocus",     FDF_VIRTUAL|FDF_OBJECTID|FDF_W,           ID_TABFOCUS, NULL,   (APTR)SET_TabFocus },
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
