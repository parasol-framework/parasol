/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Input: Manages the display and interactivity of user input boxes.

The Input class simplifies the creation and management of input boxes as part of the user interface.

When the user clicks or tabs away from the input box, your program may need to perform an action.  Set the
#Feedback field in order to receive a notification and formulate a response.

-END-

*****************************************************************************/

#define PRV_INPUT
#define PRV_WIDGET_MODULE
#include <parasol/modules/widget.h>
#include <parasol/modules/font.h>
#include <parasol/modules/surface.h>
#include <parasol/modules/vector.h>
#include "defs.h"

static OBJECTPTR clInput = NULL;

//****************************************************************************

static void style_trigger(objInput *Self, LONG Style)
{
   if (Self->prvStyleTrigger.Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Self->prvStyleTrigger.Script.Script)) {
         const ScriptArg args[] = {
            { "Input", FD_OBJECTPTR, { .Address = Self } },
            { "Style", FD_LONG,      { .Long = Style } }
         };
         scCallback(script, Self->prvStyleTrigger.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
      }
   }
}

//****************************************************************************
// Internal, to be called from the style code when the user hits enter.

static ERROR INPUT_Activate(objInput *Self, APTR Void)
{
   parasol::Log log;

   if (Self->prvActive) {
      log.warning("Warning - recursion detected");
      return ERR_Okay;
   }

   log.branch();

   Self->prvActive = TRUE;

   CSTRING str = NULL;
   GetString(Self->TextInput, FID_String, (STRING *)&str);

   ULONG hash = StrHash(str, FALSE);
   if (hash != Self->prvLastStringHash) { // Do nothing if the string hasn't changed.
      Self->prvLastStringHash = hash;

      if (Self->prvFeedback.Type IS CALL_STDC) {
         auto routine = (void (*)(objInput *, CSTRING, LONG))Self->prvFeedback.StdC.Routine;
         parasol::SwitchContext ctx(Self->prvFeedback.StdC.Context);
         routine(Self, str, TRUE);
      }
      else if (Self->prvFeedback.Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         if ((script = Self->prvFeedback.Script.Script)) {
            const ScriptArg args[] = {
               { "Input", FD_OBJECTPTR, { .Address = Self } },
               { "Value", FD_STRING,    { .Address = (STRING)str } },
               { "Activated", FD_LONG,  { .Long = TRUE } }
            };
            scCallback(script, Self->prvFeedback.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
         }
      }
   }

   Self->prvActive = FALSE;
   return ERR_Okay;
}

//****************************************************************************

static ERROR INPUT_ActionNotify(objInput *Self, struct acActionNotify *Args)
{
   if (Args->Error != ERR_Okay) return ERR_Okay;

   if (Args->ActionID IS AC_Free) {
      if ((Self->prvFeedback.Type IS CALL_SCRIPT) and (Self->prvFeedback.Script.Script->UID IS Args->ObjectID)) {
         Self->prvFeedback.Type = CALL_NONE;
      }

      if ((Self->prvStyleTrigger.Type IS CALL_SCRIPT) and (Self->prvStyleTrigger.Script.Script->UID IS Args->ObjectID)) {
         Self->prvStyleTrigger.Type = CALL_NONE;
      }
   }
   else return ERR_NoSupport;

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Disable: Turns the input box off.
-END-
*****************************************************************************/

static ERROR INPUT_Disable(objInput *Self, APTR Void)
{
   Self->Flags |= INF_DISABLED;
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Enable: Turns the input box back on if it has previously been disabled.
-END-
*****************************************************************************/

static ERROR INPUT_Enable(objInput *Self, APTR Void)
{
   Self->Flags &= ~INF_DISABLED;
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Focus: Sets the focus on the input box.
-END-
*****************************************************************************/

static ERROR INPUT_Focus(objInput *Self, APTR Void)
{
   return acFocus(Self->Viewport);
}

//****************************************************************************

static ERROR INPUT_Free(objInput *Self, APTR Void)
{
   if (Self->TextInput) { acFree(Self->TextInput); Self->TextInput = NULL; }

   if (Self->Viewport) {
      UnsubscribeAction(Self->Viewport, 0);
      acFree(Self->Viewport);
      Self->Viewport = NULL;
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Hide: Removes the input box from the display.
-END-
*****************************************************************************/

static ERROR INPUT_Hide(objInput *Self, APTR Void)
{
   return acHide(Self->Viewport);
}

//****************************************************************************

static ERROR INPUT_Init(objInput *Self, APTR Void)
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

   if (!acInit(Self->Viewport)) {
      if (drwApplyStyleGraphics(Self, Self->Viewport->Head.UID, NULL, NULL)) {
         return ERR_Failed; // Graphics styling is required.
      }

      return ERR_Okay;
   }
   else return ERR_Init;
}

/*****************************************************************************
-ACTION-
MoveToBack: Moves the input box to the back of the display area.
-END-
*****************************************************************************/

static ERROR INPUT_MoveToBack(objInput *Self, APTR Void)
{
   return acMoveToBack(Self->Viewport);
}

/*****************************************************************************
-ACTION-
MoveToFront: Moves the input box to the front of the display area.
-END-
*****************************************************************************/

static ERROR INPUT_MoveToFront(objInput *Self, APTR Void)
{
   return acMoveToFront(Self->Viewport);
}

//****************************************************************************

static ERROR INPUT_NewObject(objInput *Self, APTR Void)
{
   if (!NewObject(ID_VECTORVIEWPORT, NF_INTEGRAL, &Self->Viewport)) {
      if (!NewObject(ID_VECTORTEXT, NF_INTEGRAL, &Self->TextInput)) {
         SetOwner(Self->TextInput, Self->Viewport);
         SetString(Self->TextInput->Font, FID_Face, glWidgetFace);
         drwApplyStyleValues(Self, NULL);
         return ERR_Okay;
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
   return Action(AC_Redimension, Self->Viewport, Args);
}

/*****************************************************************************
-ACTION-
Resize: Alters the size of the input box.
-END-
*****************************************************************************/

static ERROR INPUT_Resize(objInput *Self, struct acResize *Args)
{
   return Action(AC_Resize, Self->Viewport, Args);
}

/*****************************************************************************
-ACTION-
Show: Display the input box.
-END-
*****************************************************************************/

static ERROR INPUT_Show(objInput *Self, APTR Void)
{
   return acShow(Self->Viewport);
}

/*****************************************************************************

-FIELD-
Bottom: The bottom coordinate of the input box (Y + Height).

*****************************************************************************/

static ERROR GET_Bottom(objInput *Self, LONG *Value)
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
   if (Value IS TRUE) return acDisable(Self);
   else if (Value IS FALSE) return acEnable(Self);
   else return ERR_InvalidValue;
}

/*****************************************************************************

-FIELD-
Feedback: Provides instant feedback when a user interacts with the object.

Set a callback function here to receive instant feedback when user interaction occurs.  The
function prototype is `Function(*Input, CSTRING Value, LONG Activated)`

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
Height: Defines the height of an input box.

An input box can be given a fixed or relative height by setting this field to the desired value.  To set a relative
height, use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Height(objInput *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_Height, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_Height, &Value->Large);
   else return ERR_FieldTypeMismatch;
}

static ERROR SET_Height(objInput *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_Height, Value);
}

/*****************************************************************************

-FIELD-
InputMask: Restricts user input with a validation filter.

User input can be automatically validated by defining an InputMask.  The pattern matching format that is used for
the mask must be compliant with Lua 5.1 specifications.  As an example `%d%d/%d%d/%d%d%d%d` could be used to match
to a valid date entry.

If the user inputs a string that does not match the InputMask filter, the string automatically reverts to its most
recent and valid value.

*****************************************************************************/

static ERROR GET_InputMask(objInput *Self, STRING *Value)
{
   *Value = Self->prvInputMask;
   return ERR_Okay;
}

static ERROR SET_InputMask(objInput *Self, CSTRING Value)
{
   if (Value) StrCopy(Value, Self->prvInputMask, sizeof(Self->prvInputMask));
   else Self->prvInputMask[0] = 0;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
InputWidth: The width of the input area.

A fixed width for the input area can be defined in this field (note that this does not include the width of the label,
which is handled separately by #LabelWidth.

*****************************************************************************/

static ERROR SET_InputWidth(objInput *Self, LONG Value)
{
   Self->InputWidth = Value;

   if (Self->Head.Flags & NF_INITIALISED) style_trigger(Self, STYLE_RESIZE);
   else Self->Flags |= INF_FIXED_INPUT_WIDTH; // Important not to set this if already initialised

   return ERR_Okay;
}

/*****************************************************************************

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
      SetLong(Self->TextInput, FID_X, Self->LabelWidth);
      style_trigger(Self, STYLE_RESIZE);
   }
   else Self->Flags |= INF_FIXED_LABEL_WIDTH;

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
Right: The right-most coordinate of the input box (X + Width).

*****************************************************************************/

static ERROR GET_Right(objInput *Self, LONG *Value)
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
   if (!SetString(Self->TextInput, FID_String, Value)) return ERR_Okay;
   else return ERR_Failed;
}

/*****************************************************************************

-FIELD-
ParentViewport: The viewport that will contain the input graphic.

The @VectorViewport that will contain the input graphic is defined here.  If this field is not set prior to initialisation, the
input object will scan its parent branch to find a suitable candidate.

-FIELD-
StyleTrigger: Requires a callback for reporting changes that can affect graphics styling.

This field is reserved for use by the style code that is managing the input graphics.

*****************************************************************************/

static ERROR SET_StyleTrigger(objInput *Self, FUNCTION *Value)
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
TabFocus: Setting this field to a valid TabFocus object will cause the input to add itself to the tab list.

The TabFocus field provides a convenient way of adding the input to a TabFocus object, so that it can be focussed on
via the tab key.  Simply set this field to the ID of the TabFocus object that is managing the tab-list for the
application window.

*****************************************************************************/

static ERROR SET_TabFocus(objInput *Self, OBJECTID Value)
{
   OBJECTPTR tabfocus;
   if (!AccessObject(Value, 5000, &tabfocus)) {
      if (tabfocus->ClassID IS ID_TABFOCUS) tabAddObject(tabfocus, Self->Head.UID);
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
Viewport: The VectorViewport that represents the input box.

The viewport area that represents the input display can be accessed through this field.  For further information, refer
to the @VectorViewport class.  Note that interfacing with the viewport directly can have adverse effects on the input
control system.  Where possible, all communication should be limited to the input object itself.

-FIELD-
Width: Defines the width of an input box.

An input box can be given a fixed or relative width by setting this field to the desired value.  To set a relative
width, use the FD_PERCENT flag when setting the field.

*****************************************************************************/

static ERROR GET_Width(objInput *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_Width, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_Width, &Value->Large);
   else return ERR_FieldTypeMismatch;
}

static ERROR SET_Width(objInput *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_Width, Value);
}

/*****************************************************************************

-FIELD-
X: The horizontal position of an input box.

The horizontal position of an input box can be set to an absolute or relative coordinate by writing a value to the
X field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be
interpreted as fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_X(objInput *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_X, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_X, &Value->Large);
   else return ERR_FieldTypeMismatch;
}

static ERROR SET_X(objInput *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_X, Value);
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

static ERROR GET_XOffset(objInput *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_XOffset, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_XOffset, &Value->Large);
   else return ERR_FieldTypeMismatch;
}

static ERROR SET_XOffset(objInput *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_XOffset, Value);
}

/*****************************************************************************

-FIELD-
Y: The vertical position of an input box.

The vertical position of an input box can be set to an absolute or relative coordinate by writing a value to the Y
field.  To set a relative/percentage based value, you must use the FD_PERCENT flag or the value will be interpreted as
fixed.  Negative values are permitted.

*****************************************************************************/

static ERROR GET_Y(objInput *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_Y, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_Y, &Value->Large);
   else return ERR_FieldTypeMismatch;
}

static ERROR SET_Y(objInput *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_Y, Value);
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

static ERROR GET_YOffset(objInput *Self, Variable *Value)
{
   if (Value->Type & FD_DOUBLE) return GetDouble(Self->Viewport, FID_YOffset, &Value->Double);
   else if (Value->Type & FD_LARGE) return GetLarge(Self->Viewport, FID_YOffset, &Value->Large);
   else return ERR_FieldTypeMismatch;
}

static ERROR SET_YOffset(objInput *Self, Variable *Value)
{
   return SetVariable(Self->Viewport, FID_YOffset, Value);
}

//**********************************************************************

#include "class_input_def.c"

static const FieldArray clFields[] = {
   { "TextInput",      FDF_OBJECT|FDF_R,     ID_VECTORTEXT, NULL, NULL },
   { "Viewport",       FDF_OBJECT|FDF_RI,    ID_VECTORVIEWPORT, NULL, NULL },
   { "ParentViewport", FDF_OBJECT|FDF_RI,    ID_VECTORVIEWPORT, NULL, NULL },
   { "Flags",          FDF_LONGFLAGS|FDF_RW, (MAXINT)&clInputFlags, NULL, NULL },
   { "LabelWidth",     FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_LabelWidth },
   { "InputWidth",     FDF_LONG|FDF_RI,      0, NULL, (APTR)SET_InputWidth },
   // Virtual fields
   { "Bottom",       FDF_VIRTUAL|FDF_LONG|FDF_R,         0, (APTR)GET_Bottom,    NULL },
   { "Disable",      FDF_VIRTUAL|FDF_LONG|FDF_RW,        0, (APTR)GET_Disable,   (APTR)SET_Disable },
   { "Feedback",     FDF_VIRTUAL|FDF_FUNCTIONPTR|FDF_RW, 0, (APTR)GET_Feedback,  (APTR)SET_Feedback },
   { "InputMask",    FDF_VIRTUAL|FDF_STRING|FDF_RW,      0, (APTR)GET_InputMask, (APTR)SET_InputMask },
   { "Label",        FDF_VIRTUAL|FDF_STRING|FDF_RW,      0, (APTR)GET_Label,     (APTR)SET_Label },
   { "PostLabel",    FDF_VIRTUAL|FDF_STRING|FDF_RW,      0, (APTR)GET_PostLabel, (APTR)SET_PostLabel },
   { "Right",        FDF_VIRTUAL|FDF_LONG|FDF_R,         0, (APTR)GET_Right,     NULL },
   { "String",       FDF_VIRTUAL|FDF_STRING|FDF_RW,      0, (APTR)GET_String,    (APTR)SET_String },
   { "TabFocus",     FDF_VIRTUAL|FDF_OBJECTID|FDF_W,     ID_TABFOCUS, NULL,      (APTR)SET_TabFocus },
   { "Text",         FDF_SYNONYM|FDF_VIRTUAL|FDF_STRING|FDF_RW, 0, (APTR)GET_String, (APTR)SET_String },
   { "StyleTrigger", FDF_VIRTUAL|FDF_FUNCTIONPTR|FDF_W,  0, NULL, (APTR)SET_StyleTrigger },
   // Variable Fields
   { "Height",  FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_Height, (APTR)SET_Height },
   { "Width",   FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_Width, (APTR)SET_Width },
   { "X",       FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_X, (APTR)SET_X },
   { "XOffset", FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_XOffset, (APTR)SET_XOffset },
   { "Y",       FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_Y, (APTR)SET_Y },
   { "YOffset", FDF_VIRTUAL|FDF_VARIABLE|FDF_DOUBLE|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_YOffset, (APTR)SET_YOffset },
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
