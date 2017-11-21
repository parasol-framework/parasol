/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Dialog: The Dialog class is used to pose a question and retrieve an answer from the user.

The Dialog class provides the means for the creation of simple dialog windows, typically for the purpose of posing a
question to the user and then waiting for a response before continuing.  You will need to specify the text to be
printed inside the dialog box and the options for the user to click on. Optionally you may also specify an image to
accompany the text for the purposes of enhancing the message.

This example creates a basic dialog box to elicit a Yes/No response from the user:

<pre>
obj.new('dialog', {
   image   = 'icons:items/question(48)',
   options = 'yes;no',
   title   = 'Confirmation Required',
   flags   = 'wait'
})
</pre>

A simple input box can be created inside the dialog window if you need the user to type in a one-line string as part of
the dialog response.  To do this, set the INPUT flag and write a string to the #UserInput field if you wish to set a
pre-defined response.  On successful completion, the UserInput field will be updated to reflect the user's string entry.

If a dialog box needs to be used multiple times, create it as static and then use the Show action to display the dialog
window as required.  This effectively caches the window so that it does not need to be recreated from scratch each time
that the dialog window needs to be displayed.

Any child objects that are initialised to a dialog will be activated in the event that a successful response is given
by the user.  Failure to respond, or a response of 'cancel', 'quit' or 'none' will prevent the activation of the child
objects.

The expected methodology for receiving a user's response to a dialog box is to set the #Feedback field with a callback
function.

<header>Custom Dialogs</>

It is possible to create complex dialogs that use your own GUI controls and scripted functionality, all within the
document that is presented by the dialog object.  This is done by injecting content into the dialog document.  Content
is injected by setting the #Template and #Inject fields, or by using the #DataFeed() action.  Please refer to the
documentation for the aforementioned areas for further information.

-END-

*****************************************************************************/

//#define DEBUG

#define PRV_DIALOG
#include <parasol/modules/document.h>
#include <parasol/modules/window.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/widget.h>
#include <parasol/modules/surface.h>
#include "defs.h"

#include "class_dialog_script.h"

static objXML *glXML = NULL;
static OBJECTPTR clDialog = NULL;
static LONG glBreakMessageID = 0;

static const struct FieldArray clFields[];
static const struct FieldDef clDialogResponse[];

static ERROR create_window(objDialog *);

static struct CacheFile *glTemplate = NULL;

//****************************************************************************

static ERROR msgbreak(APTR Custom, LONG MsgID, LONG MsgType, APTR Message, LONG MsgSize)
{
   return ERR_Terminate;
}

//****************************************************************************

static void window_close(objWindow *Window)
{
   acFree(Window);
}

//****************************************************************************

static ERROR DIALOG_Activate(objDialog *Self, APTR Void)
{
   acShow(Self); // Display the dialog box on activation

   // Do not notify on activation (the Response field sends Activate notifications)
   return ERR_Okay|ERF_Notified;
}

//****************************************************************************

static ERROR DIALOG_ActionNotify(objDialog *Self, struct acActionNotify *Args)
{
   if (Args->ActionID IS AC_Free) {
      if (Args->ObjectID IS Self->WindowID) {
         Self->Document = NULL; // Document will go down with the window

         if (Self->AwaitingResponse) SetLong(Self, FID_Response, RSF_CLOSED);
         if (Self->Active) SendMessage(0, glBreakMessageID, 0, 0, 0);
         Self->WindowID = 0;
      }
      else if ((Self->Feedback.Type IS CALL_SCRIPT) AND
               (Self->Feedback.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->Feedback.Type = CALL_NONE;
      }
   }
   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
DataFeed: Refer to long description for supported feed types.

XML data is supported for setting the #Inject and #Template fields with XML definitions.  This is achieved by
encapsulating the XML data with a tag name of either 'inject' or 'template' to match the target field.  This is
recommended when customising a dialog, for example:

<pre>
dlg = obj.new('dialog')
dlg.acDataFeed(0, DATA_XML, [[
&lt;template&gt;
  ...
&lt;/template&gt;

&lt;inject&gt;
  ...
&lt;/inject&gt;
]])
</pre>

-END-

*****************************************************************************/

static ERROR DIALOG_DataFeed(objDialog *Self, struct acDataFeed *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   if (Args->DataType IS DATA_XML) {
      if (!glXML) {
         if (CreateObject(ID_XML, NF_UNTRACKED, &glXML,
               FID_Statement|TSTR, Args->Buffer,
               FID_Flags|TLONG,    XMF_ALL_CONTENT|XMF_PARSE_HTML|XMF_STRIP_HEADERS,
               TAGEND) != ERR_Okay) {
            return PostError(ERR_CreateObject);
         }
      }
      else {
         acClear(glXML);
         if (acDataXML(glXML, Args->Buffer) != ERR_Okay) {
            return PostError(ERR_Failed);
         }
      }

      struct XMLTag *tag;
      for (tag=glXML->Tags[0]; tag; tag=tag->Next) {
         if (!StrMatch("template", tag->Attrib->Name)) {
            STRING str;
            if (!xmlGetString(glXML, tag->Child->Index, XMF_INCLUDE_SIBLINGS, &str)) {
               if (Self->Template) FreeMemory(Self->Template);

               if (!AllocMemory(StrLength(str)+8, MEM_STRING|MEM_NO_CLEAR, &Self->Template, NULL)) {
                  StrCopy("STRING:", Self->Template, COPY_ALL);
                  StrCopy(str, Self->Template+7, COPY_ALL);
                  MSG("Inserting template: %.80s", Self->Template);
               }

               FreeMemory(str);
            }
            else FMSG("@","Failed to read any data in <template> tag.");
         }
         else if (!StrMatch("inject", tag->Attrib->Name)) {
            if (Self->Inject) FreeMemory(Self->Inject);

            if (!xmlGetString(glXML, tag->Child->Index, XMF_INCLUDE_SIBLINGS, &Self->Inject)) {
               MSG("<inject> statement: %.80s", Self->Inject);
            }
            else FMSG("@","Failed to to read any data inside <inject> tag.");
         }
      }

      return ERR_Okay;
   }

   return ERR_NoSupport;
}

//****************************************************************************

static ERROR DIALOG_Free(objDialog *Self, APTR Void)
{
   if ((Self->WindowID) AND (!CheckObjectExists(Self->WindowID, NULL))) {
      OBJECTPTR window;
      if (!AccessObject(Self->WindowID, 5000, &window)) {
         UnsubscribeAction(window, 0);
         ReleaseObject(window);
      }

      acFreeID(Self->WindowID);
      Self->WindowID = 0;
   }

   if (Self->Message) { FreeMemory(Self->Message); Self->Message = NULL; }
   if (Self->Vars)    { VarFree(Self->Vars); Self->Vars = NULL; }
   if (Self->Inject)  { FreeMemory(Self->Inject); Self->Inject = NULL; }
   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
GetVar: Retrieves named variables.

Supported variable templates are:

`Option(Index, Response)` Returns the response value (an integer) of an option.  The Index is a number that indicates a option in the Options field.

`Option(Index, Text)` Returns the text value of an option.  The Index is a number that indicates an option in the Options field.

`Option(Index, Icon)` Returns the recommended icon for an option, e.g. "icons:items/checkmark".  The Index is a number that indicates an option in the Options field.
-END-

*****************************************************************************/

static ERROR DIALOG_GetVar(objDialog *Self, struct acGetVar *Args)
{
   if ((!Args) OR (!Args->Buffer)) return PostError(ERR_NullArgs);

   Args->Buffer[0] = 0;

   if (!StrCompare("option(", Args->Field, 7, 0)) {
      // Format: option(Index,Response|Text)
      CSTRING str = Args->Field + 7;
      LONG j = StrToInt(str);
      if ((j >= 0) AND (j < Self->TotalOptions)) {
         while ((*str) AND (*str != ',')) str++;
         if (*str IS ',') str++;
         while ((*str) AND (*str <= 0x20)) str++;

         if (*str) {
            if (!StrCompare("Response", str, 8, 0)) {
               IntToStr(Self->Options[j].Response, Args->Buffer, Args->Size);
               return ERR_Okay;
            }
            else if (!StrCompare("Text", str, 4, 0)) {
               StrCopy(Self->Options[j].Text, Args->Buffer, Args->Size);
               return ERR_Okay;
            }
            else if (!StrCompare("Icon", str, 4, 0)) {
               if (Self->Options[j].Response & (RSF_CANCEL|RSF_NO|RSF_NO_ALL|RSF_QUIT)) {
                  StrCopy("icons:items/cancel", Args->Buffer, Args->Size);
               }
               else if (Self->Options[j].Response & (RSF_YES|RSF_OKAY|RSF_YES_ALL)) {
                  StrCopy("icons:items/checkmark", Args->Buffer, Args->Size);
               }

               return ERR_Okay;
            }
            else return ERR_NoSupport;
         }
         else return ERR_Failed;
      }
      else return PostError(ERR_OutOfRange);
   }

   // User variables

   CSTRING arg = VarGetString(Self->Vars, Args->Field);
   if (arg) {
      StrCopy(arg, Args->Buffer, Args->Size);
      return ERR_Okay;
   }
   else {
      LogErrorMsg("The variable \"%s\" does not exist.", Args->Field);
      Args->Buffer[0] = 0;
      return ERR_UnsupportedField;
   }
}

//****************************************************************************

static ERROR DIALOG_Init(objDialog *Self, APTR Void)
{
   if (Self->Flags & DF_INPUT_REQUIRED) Self->Flags |= DF_INPUT;

   if (Self->PopOverID) {
      SURFACEINFO *info;
      if (!drwGetSurfaceInfo(Self->PopOverID, &info)) {
         Self->TargetID = info->ParentID;
      }
      else {
         Self->PopOverID = 0;
         if (!Self->TargetID) FastFindObject("desktop", ID_SURFACE, &Self->TargetID, 1, 0);
      }
   }
   else if (!Self->TargetID) FastFindObject("desktop", ID_SURFACE, &Self->TargetID, 1, 0);

   ERROR error = create_window(Self);

   if (error != ERR_Okay) LogErrorMsg("Failed to create window, error %d.  Use a log level > 5 for info.", error);

   return error;
}

//****************************************************************************

static ERROR DIALOG_NewObject(objDialog *Self, APTR Void)
{
   StrCopy("Confirmation Required", Self->Title, sizeof(Self->Title));
   StrCopy("items/question", Self->Icon, sizeof(Self->Icon));
   Self->Width  = 290;
   Self->Height = 102;
   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
Refresh: Refreshes the state of the dialog box that is on display.

This action is typically called from custom dialogs that need to refresh the dialog due to a change in content.  The
dialog's document object will be refreshed and then the window size will be automatically adjusted and repositioned to
match the new size of the document (if necessary).

*****************************************************************************/

static ERROR DIALOG_Refresh(objDialog *Self, APTR Void)
{
   Self->Response = 0; // Reset the response value

   if ((Self->WindowID) AND (Self->Document)) {
      ERROR error;

      if (Self->Document->Head.Flags & NF_INITIALISED) error = acRefresh(Self->Document);
      else error = acInit(Self->Document);

      if (error) return error;

      objWindow *win;
      if (!AccessObject(Self->WindowID, 3000, &win)) {
         LONG page_width, page_height;
         if (!GetLong(Self->Document, FID_PageHeight, &page_height)) {
            if (page_height > win->Surface->Height) {
               SetFields(win,
                  FID_MaxHeight|TLONG,    page_height,
                  FID_MinHeight|TLONG,    page_height,
                  FID_InsideHeight|TLONG, page_height,
                  TAGEND);
            }
         }

         if (!GetLong(Self->Document, FID_PageWidth, &page_width)) {
            if (page_width > win->Surface->Width) {
               SetFields(win,
                  FID_MaxWidth|TLONG,    page_width,
                  FID_MinWidth|TLONG,    page_width,
                  FID_InsideWidth|TLONG, page_width,
                  TAGEND);
            }
         }

         ReleaseObject(win);
      }
      return error;
   }
   else return ERR_Failed;
}

/*****************************************************************************
-ACTION-
SetVar: Sets named variables that are relevant to the developer only.
-END-
*****************************************************************************/

static ERROR DIALOG_SetVar(objDialog *Self, struct acSetVar *Args)
{
   if ((!Args) OR (!Args->Field)) return ERR_NullArgs;
   if (!Args->Field[0]) return ERR_EmptyString;

   if (!Self->Vars) {
      if (!(Self->Vars = VarNew(0, 0))) return ERR_AllocMemory;
   }

   return VarSetString(Self->Vars, Args->Field, Args->Value);
}

/*****************************************************************************

-ACTION-
Show: Displays the dialog window.

Call the Show action to display the dialog window.  If you have set the WAIT option in the #Flags field, the process
will be put to sleep in a message processing loop while it waits for the user to respond to the dialog box.  After the
Show action returns, you will be able to read the #Response field for the user's response to the dialog box.

-END-

*****************************************************************************/

static ERROR DIALOG_Show(objDialog *Self, APTR Void)
{
   if (!(Self->Head.Flags & NF_INITIALISED)) return PostError(ERR_NotInitialised); // Check for user programming errors

   LogBranch(NULL);

   if (Self->Active) return ERR_Okay; // If we are active, do not continue

   // If our dialog window has disappeared (e.g. the user killed it on a previous activation), we'll need to recreate it.

   ERROR error;

   if ((!Self->WindowID) OR (CheckObjectExists(Self->WindowID, NULL) != ERR_Okay)) {
      Self->WindowID = 0;

      error = create_window(Self);
      if (error) return error;
   }

   error = acRefresh(Self);
   if (error) return error;

   // If INPUT is on, the default link is 0.  Otherwise the default should be the option marked with a *.

   docSelectLink(Self->Document, 0, NULL);

   acMoveToFrontID(Self->WindowID);
   acShowID(Self->WindowID);

   Self->AwaitingResponse = TRUE;

   if (Self->Flags & DF_WAIT) {
      Self->Active = TRUE;

      // Wait for a user response.  We will awaken if the Response field is updated, or if the dialog window is killed.
      // See the code for the Response field for further details.

      LogBranch("Entering sleep mode...");

      APTR handle;
      FUNCTION call;
      SET_FUNCTION_STDC(call, &msgbreak);
      if (!AddMsgHandler(0, glBreakMessageID, &call, &handle)) {
         ProcessMessages(0, -1); // Processing continues until either a QUIT or glBreakMessageID is intercepted
         RemoveMsgHandler(handle);
      }

      LogBack();

      Self->Active = FALSE;
   }

   LogBack();
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Options: Options for the dialog box are defined through this field.

Use the Options field to define a series of options that will appear in the dialog box.  Setting this field is
compulsory in order for a dialog object to initialise.  This field is set using the following field format:
`"response:text; response:text; ..."`.

Each option definition is separated by a semi-colon and the order that you use reflects the option creation, scanning
from left to right in the dialog window. You must define a response type for each option, which may be one of Cancel,
Yes, YesAll, No, NoAll, Quit and Okay.  A special response type of None is also allowed if you want to create a dummy
option that only closes the dialog window.  The response definition may be followed with a colon and then a text
description to be displayed inside the option area.  If you do not wish to declare a text description, you can
follow-up with a semi-colon and then the next option's description.

When an option is selected, the matching response value will be written to the #Response field and then the
dialog window will be closed.

*****************************************************************************/

static ERROR SET_Options(objDialog *Self, CSTRING Value)
{
   if ((!Value) OR (!*Value)) {
      Self->TotalOptions = 0;
      return ERR_Okay;
   }

   WORD index = 0;

   while ((*Value) AND (index < ARRAYSIZE(Self->Options))) {
      while ((*Value) AND (*Value <= 0x20)) Value++;

      // Extract the response type

      char response[30];
      WORD i, j;
      for (i=0; (*Value) AND (*Value != ';') AND (*Value != ':'); i++) {
         response[i] = *Value++;
      }
      response[i] = 0;

      // Convert the response to a value

      Self->Options[index].Response = RSF_NONE; // No response by default
      for (j=0; clDialogResponse[j].Name; j++) {
         if (!StrMatch(clDialogResponse[j].Name, response)) {
            Self->Options[index].Response = clDialogResponse[j].Value;
         }
      }

      // Extract text

      if (*Value IS ':') {
         Value++;
         while ((*Value) AND (*Value <= 0x20)) Value++;
         for (j=0; (*Value) AND (*Value != ';'); j++) Self->Options[index].Text[j] = *Value++;
         Self->Options[index].Text[j] = 0;
      }
      else Self->Options[index].Text[0] = 0;

      if (!Self->Options[index].Text[0]) {
         CSTRING text;
         switch (Self->Options[index].Response) {
            case RSF_CANCEL:  text = "Cancel";     break;
            case RSF_QUIT:    text = "Quit";       break;
            case RSF_NO:      text = "No";         break;
            case RSF_NO_ALL:  text = "No to All";  break;
            case RSF_YES:     text = "Yes";        break;
            case RSF_YES_ALL: text = "Yes to All"; break;
            case RSF_OKAY:    text = "Okay";       break;
            default:          text = "-";          break;
         }
         StrCopy(text, Self->Options[index].Text, sizeof(Self->Options[0].Text));
      }

      // Go to the next option entry

      while ((*Value) AND (*Value != ';')) Value++;
      if (*Value IS ';') Value++;
      while ((*Value) AND (*Value <= 0x20)) Value++;

      index++;
   }

   Self->TotalOptions = index;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Document: Private. Internal document reference.

The dialog content is constructed using a @Document object that is referenced here.

-FIELD-
EnvTemplate: Private

Returns the content of glTemplate, which is loaded from `templates:dialog.rpl`.  This field is intended for use
by the internal dialog script only.

*****************************************************************************/

static ERROR GET_EnvTemplate(objDialog *Self, STRING *Value)
{
   if (glTemplate) {
      *Value = glTemplate->Data;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

/*****************************************************************************

-FIELD-
Flags: Optional flags may be defined here.

-FIELD-
Height: The internal height of the dialog window.

-FIELD-
Icon: The icon that appears in the window title bar may be set here.

A question-mark icon is set in the dialog window by default, however you may change to a different icon image if you
wish.  If you would like to refer to a stock icon, use the file format, `icons:category/name`.

*****************************************************************************/

static ERROR GET_Icon(objDialog *Self, STRING *Value)
{
   if (Self->Icon[0]) {
      *Value = Self->Icon;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Icon(objDialog *Self, CSTRING Value)
{
   if (Value) StrCopy(Value, Self->Icon, sizeof(Self->Icon));
   else Self->Icon[0] = 0;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Image: An icon file may be specified here in order to visually enhance the dialog message.

Images may be used inside a dialog window to enhance the message that is presented to the user.  A number of icons are
available in Parasol's icon library that are suitable for display in dialog boxes (the icons:items/ directory contains
most of these).  The image should be no larger than 48x48 pixels and no less than 32x32 pixels in size.

*****************************************************************************/

static ERROR GET_Image(objDialog *Self, STRING *Value)
{
   if (Self->Image[0]) {
      *Value = Self->Image;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Image(objDialog *Self, CSTRING Value)
{
   if (Value) StrCopy(Value, Self->Image, sizeof(Self->Image));
   else Self->Image[0] = 0;

   // Destroy the existing image and replace it with the new image.

   if (Self->Head.Flags & NF_INITIALISED) {
      LogErrorMsg("Missing support for changing the image in the dialog window.");
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Inject: Allows formatted text to be injected into the dialog window's document.

The Inject field allows customised formatted text to be inserted into the dialog (for example images, GUI controls and
other presentation concepts).  The injected content must be in the RIPPLE document format.  As no restrictions are
applied to the injected content, you may use all of the available RIPPLE tags.

The injected content is inserted immediately after the dialog's content - for example the #Message - and before
the #Options.

We recommend that you keep the styling of your content to a minimum, or your content may contrast poorly with the
dialog theme that is active.  Please use the #Template field if you would like to redefine the presentation of
the dialog.

*****************************************************************************/

static ERROR GET_Inject(objDialog *Self, STRING *Value)
{
   if (Self->Inject) {
      *Value = Self->Inject;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Inject(objDialog *Self, CSTRING Value)
{
   if (Self->Inject) { FreeMemory(Self->Inject); Self->Inject = NULL; }
   if (Value) Self->Inject = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Option: Enables a user option box in the dialog when set.

A dialog box can contain a single user option by setting this field on initialisation.  The option is typically
presented as a checkbox and has a state of either 1 (on) or 0 (off).

The string value set in this field is used to present the option to the user.

*****************************************************************************/

static ERROR GET_Option(objDialog *Self, STRING *Value)
{
   if (Self->Option[0]) {
      *Value = Self->Option;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Option(objDialog *Self, CSTRING Value)
{
   if (Value) StrCopy(Value, Self->Option, sizeof(Self->Option));
   else Self->Option[0] = 0;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
PopOver: Open the dialog window at a position relative to the surface specified here.

Specifying the PopOver option will open the dialog window at a position relative to the surface specified here.  Please
refer to the @Window.PopOver field for further information.

*****************************************************************************/

static ERROR SET_PopOver(objDialog *Self, OBJECTID Value)
{
   if (Value) {
      CLASSID class_id = GetClassID(Value);
      if (class_id == ID_WINDOW) {
         objWindow *win;
         if (!AccessObject(Value, 3000, &win)) {
            Self->PopOverID = win->Surface->Head.UniqueID;
            ReleaseObject(win);
         }
      }
      else Self->PopOverID = Value;
   }
   else Self->PopOverID = 0;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Response: Holds the response value when an option is selected.

This field holds the response value when an option is selected by the user.  If no response was returned (for
example, the user closed the dialog window rather than clicking an option) then the value will be zero.

Please use #Feedback to pro-actively receive the user's response to the dialog.

*****************************************************************************/

static ERROR SET_Response(objDialog *Self, LONG Value)
{
   Self->AwaitingResponse = FALSE;

   if (Value IS RSF_NONE) Self->Response = 0;
   else Self->Response = Value;

   // If we are sleeping, send a break message because the user has clicked one of our options.

   if (Self->Active) SendMessage(0, glBreakMessageID, 0, 0, 0);

   if (Self->Head.Flags & NF_INITIALISED) {
      acHideID(Self->WindowID);

      if (Self->Response) {
         LogF("~","Received response $%.8x", Self->Response);

         if (Self->Feedback.Type) {
            if (Self->Feedback.Type IS CALL_STDC) {
               void (*routine)(objDialog *, LONG);
               OBJECTPTR context = SetContext(Self->Feedback.StdC.Context);
                  routine = Self->Feedback.StdC.Routine;
                  routine(Self, Self->Response);
               SetContext(context);
            }
            else if (Self->Feedback.Type IS CALL_SCRIPT) {
               OBJECTPTR script;
               if ((script = Self->Feedback.Script.Script)) {
                  const struct ScriptArg args[] = {
                     { "Dialog",   FD_OBJECTPTR, { .Address = Self } },
                     { "Response", FD_LONG,      { .Long = Self->Response } }
                  };
                  scCallback(script, Self->Feedback.Script.ProcedureID, args, ARRAYSIZE(args));
               }
            }
         }

         LogBack();
      }
      else MSG("No response code was given.");
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Feedback: A callback for receiving the user's response to the dialog.

Set this field with a reference to a callback function to receive notifications when the user responds to a dialog.
The synopsis for the callback function is `ERROR Function(objDialog *Dialog, LONG Response)`.

Please refer to the #Response field to view the available values that can be returned in the Response parameter.

*****************************************************************************/

static ERROR GET_Feedback(objDialog *Self, FUNCTION **Value)
{
   if (Self->Feedback.Type != CALL_NONE) {
      *Value = &Self->Feedback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Feedback(objDialog *Self, FUNCTION *Value)
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
StickToFront: If TRUE, the dialog window will stick to the front of the display.

-FIELD-
Message: A message to print inside the dialog box must be declared here.

The message to display in the dialog box is declared in this field. The string must be in UTF-8 format and may contain
line feeds if the text needs to be separated.

*****************************************************************************/

static ERROR GET_Message(objDialog *Self, STRING *Value)
{
   if (Self->Message) {
      *Value = Self->Message;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Message(objDialog *Self, CSTRING Value)
{
   if (Self->Message) { FreeMemory(Self->Message); Self->Message = NULL; }

   if ((Value) AND (*Value)) {
      LONG len, i;

      for (i=0,len=0; Value[i]; i++) {
         if (Value[i] IS '\n') len += sizeof("</p><p>")-1;
         else if (Value[i] IS '<') len += sizeof("&lt;")-1;
         else if (Value[i] IS '>') len += sizeof("&gt;")-1;
         else if (Value[i] IS '&') len += sizeof("&amp;")-1;
         else len++;
      }

      if (!AllocMemory(len+1+7, MEM_STRING|MEM_NO_CLEAR, &Self->Message, NULL)) {
         STRING str = Self->Message;
         str += StrCopy("<p>", str, 3);
         for (i=0; Value[i]; i++) {
            if (Value[i] IS '\n') str += StrCopy("</p><p>", str, 7);
            else if (Value[i] IS '<') str += StrCopy("&lt;", str, 4);
            else if (Value[i] IS '>') str += StrCopy("&gt;", str, 4);
            else if (Value[i] IS '&') str += StrCopy("&amp;", str, 5);
            else { *str = Value[i]; str++; }
         }
         str += StrCopy("</p>", str, 4);
         *str = 0;
      }
      else return PostError(ERR_AllocMemory);
   }

   // Update the text in the dialog box

   if ((Self->Head.Flags & NF_INITIALISED) AND (Self->Document)) {
      acRefresh(Self->Document);
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Target: The target for the dialog box window is specified here.

The window for a dialog box will normally be created on the desktop.  On occasion it may be useful to have the window
appear in a different area, such as inside another window or screen.  To do this, point the Target field to the unique
ID of the surface that you want to open the window on.

The target may not be changed after initialisation.

-FIELD-
Template: Injects style information into the dialog's document object.

The presentation of the dialog window is controlled by an internal document object.  A default style is set for the
document which you may override by defining your own template and referring io it here.  The template can be referenced
as a path to a file that contains the template information, or you may use the `STRING:file content...` format
to store the template data in memory.

Alternatively a default dialog template can be stored at the location "templates:dialog.rpl".  Please store your
template here if you are designing a system-wide template for an environment.

To alter the document style, use the body tag in your template to redefine attributes such as the default font face,
background colour and the colour of links.  GUI controls can be re-styled by using class templates.  The header and
footer tags may also be used to add content to the top and bottom of the dialog.

Two special arguments are available to use in the document - dialog translates to an object ID referring to the dialog
object; window translates to an object ID referring to the dialog window.

To intercept responses when the user clicks on a dialog option, create a script in your template with the name
'dlgCustom'.  Declare a procedure called 'DialogResponse' that accepts a parameter that will receive the response
value.  The dialog object will call this function whenever the user selects an option.  The following example
illustrates:

<pre>
dlg = obj.new('dialog', { @arg="hello" })
dlg.acDataFeed(0, DATA_XML, [[
&lt;template&gt;
  &lt;script type="fluid" name="dlgCustom"&gt;
    &lt;![NDATA[
   local self   = obj.find("self")
   local dialog = obj.find(arg("dialog"))

function dlgResponse(Dialog, Response)
   Response = tonumber(Response)

   local doc = obj.find(self.owner)
   if (doc) then
      if (Response == RSF_OKAY) then
         // User clicked the OK option
      else
         dialog.response = Response
      end
   end
end
    ]]&gt;
  &lt;/script&gt;
&lt;/template&gt;
]])
</pre>

If you wish to ignore the response so that it is handled in the normal manner (this will result in the dialog window
closing) then set the dialog's #Response field from your code.  If you would like to refresh the dialog to
display new custom content, use your script to call the #Refresh() action on the dialog.

*****************************************************************************/

static ERROR GET_Template(objDialog *Self, STRING *Value)
{
   if (Self->Template) {
      *Value = Self->Template;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Template(objDialog *Self, CSTRING Value)
{
   if (Self->Template) { FreeMemory(Self->Template); Self->Template = NULL; }
   if (Value) Self->Template = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
TotalOptions: Indicates the total number of options declared in the Options field.

-FIELD-
Title: The window title for the dialog box.

*****************************************************************************/

static ERROR GET_Title(objDialog *Self, STRING *Value)
{
   if (Self->Title[0]) {
      *Value = Self->Title;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Title(objDialog *Self, CSTRING Value)
{
   if (Value) {
      WORD i;
      for (i=0; (i < sizeof(Self->Title)-1) AND (Value[i] >= 0x20); i++) Self->Title[i] = Value[i];
      Self->Title[i] = 0;
   }
   else Self->Title[0] = 0;

   // Update the window title

   if (Self->WindowID) {
      OBJECTPTR window;
      if (!AccessObject(Self->WindowID, 3000, &window)) {
         SetString(window, FID_Title, Value);
         ReleaseObject(window);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Type: Indicates the type of dialog presented to the user.
Lookup: DT

The type of the dialog that is being presented to the user can be indicated here.  Defining the Type is recommended as
it can enhance the presentation of the dialog in certain situations.  Enhanced user feedback, such as the inclusion of
an appropriate image and audio playback may also be presented to the user as a result of setting the Type.

*****************************************************************************/

static ERROR SET_Type(objDialog *Self, LONG Value)
{
   Self->Type = Value;

   if (!Self->Image[0]) {
      switch (Self->Type) {
         case DT_ERROR:     SET_Image(Self, "icons:items/error(48)"); break;
         case DT_CRITICAL:  SET_Image(Self, "icons:items/error(48)"); break;
         case DT_WARNING:   SET_Image(Self, "icons:items/warning(48)"); break;
         case DT_ATTENTION: SET_Image(Self, "icons:items/info(48)"); break;
         case DT_ALARM:     SET_Image(Self, "icons:time/alarm(48)"); break;
         case DT_HELP:      SET_Image(Self, "icons:items/question(48)"); break;
         case DT_QUESTION:  SET_Image(Self, "icons:items/question(48)"); break;
         case DT_REQUEST:   SET_Image(Self, "icons:items/info(48)"); break;
         case DT_INFO:      SET_Image(Self, "icons:items/info(48)"); break;
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
UserInput: Text for the dialog input box may be retrieved or defined here.

If you are creating a dialog box with a user input area, you may optionally specify an input string to be displayed
inside the input box.  The user will be able to edit the string as he sees fit.  Once the user has responded to the
dialog window, you can read this field to discover what the user has entered.

Note: When the user responds to an input entry field by pressing the enter key, the dialog object will set a
#Response of OKAY.  To simplify the interpretation of dialog responses, we recommend that an 'okay'
#Option setting accompanies the dialog (as opposed to a 'yes' option for example).

*****************************************************************************/

static ERROR GET_UserInput(objDialog *Self, STRING *Value)
{
   if (Self->Response) {
      if (Self->UserResponse[0]) {
         *Value = Self->UserResponse;
         return ERR_Okay;
      }
      else return PostError(ERR_FieldNotSet);
   }
   else if (Self->UserInput[0]) {
      *Value = Self->UserInput;
      return ERR_Okay;
   }
   else return PostError(ERR_FieldNotSet);
}

static ERROR SET_UserInput(objDialog *Self, CSTRING Value)
{
   if (Self->AwaitingResponse) {
      if (Value) StrCopy(Value, Self->UserResponse, sizeof(Self->UserResponse));
      else Self->UserResponse[0] = 0;
   }
   else if (Value) StrCopy(Value, Self->UserInput, sizeof(Self->UserInput));
   else Self->UserInput[0] = 0;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Value: Indicates the user state of the dialog's option box, if enabled.

This field has meaning if the #Option field has been set in the creation of the dialog box.  When the dialog
box is closed following presentation, the Value will be set to 1 if the option box was checked or 0 if unchecked.

-FIELD-
Width: The internal width of the dialog window.

-FIELD-
Window: Refers to the ID of the window created by the dialog object.

This readable field references the ID of the dialog box's window.  It is only usable on successful initialisation of a
dialog box.  It is recommended that you avoid tampering with the generated window, but direct access may be useful for
actions such as altering the window position.
-END-

*****************************************************************************/

static ERROR create_window(objDialog *Self)
{
   objWindow *win;
   ERROR error;

   LogF("~create_window()", NULL);

   AdjustLogLevel(1);

   Self->Document = NULL;

   if (!NewLockedObject(ID_WINDOW, NF_INTEGRAL, &win, &Self->WindowID)) {
      SetFields(win,
         FID_Title|TSTR,         Self->Title,
         FID_InsideWidth|TLONG,  Self->Width,
         FID_InsideHeight|TLONG, Self->Height,
         FID_MinWidth|TLONG,     Self->Width,
         FID_MaxWidth|TLONG,     Self->Width,
         FID_MinHeight|TLONG,    Self->Height,
         FID_MaxHeight|TLONG,    Self->Height,
         FID_Icon|TSTR,          Self->Icon,
         FID_Quit|TLONG,         (Self->Flags & DF_QUIT) ? TRUE : FALSE,
         FID_StickToFront|TLONG, Self->StickToFront,
         FID_PopOver|TLONG,      Self->PopOverID,
         FID_Center|TLONG,       TRUE,
         FID_Parent|TLONG,       Self->TargetID,
         FID_Flags|TLONG,        WNF_NO_MARGINS,
         TAGEND);
      SetFunctionPtr(win, FID_CloseFeedback, &window_close);
      if (!acInit(win)) {
         SubscribeActionTags(win, AC_Free, TAGEND);

         if (Self->Flags & DF_MODAL) SetLong(win->Surface, FID_Modal, TRUE);

         objSurface *surface;
         OBJECTID surface_id;
         if (!NewLockedObject(ID_SURFACE, 0, &surface, &surface_id)) {
            if (!SetFields(surface,
                  FID_Owner|TLONG,    win->Surface->Head.UniqueID,
                  FID_X|TLONG,        win->Surface->LeftMargin,
                  FID_Y|TLONG,        win->Surface->TopMargin,
                  FID_XOffset|TLONG,  win->Surface->RightMargin,
                  FID_YOffset|TLONG,  win->Surface->BottomMargin,
                  FID_Flags|TLONG,    RNF_GRAB_FOCUS,
                  TAGEND)) {

               if (!acInit(surface)) {
                  acShowID(surface_id);

                  SetLong(win, FID_UserFocus, surface_id);

                  if (!NewObject(ID_DOCUMENT, 0, &Self->Document)) {
                     SetFields(Self->Document,
                        FID_Owner|TLONG, surface_id,
                        FID_Flags|TSTR,  "!UNRESTRICTED|NOSCROLLBARS",
                        FID_Path|TSTR,   "#Index",
                        TAGEND);

                     struct KeyStore *docvars;
                     if (!GetPointer(Self->Document, FID_Variables, &docvars)) {
                        VarCopy(Self->Vars, docvars);
                     }

                     char buffer[24];
                     StrFormat(buffer, sizeof(buffer), "#%d", Self->Head.UniqueID);
                     acSetVar(Self->Document, "Dialog", buffer);

                     StrFormat(buffer, sizeof(buffer), "#%d", Self->WindowID);
                     acSetVar(Self->Document, "Window", buffer);

                     STRING scriptfile;
                     if (!AllocMemory(glDocumentXMLLength+1, MEM_STRING|MEM_NO_CLEAR, &scriptfile, NULL)) {
                        CopyMemory(glDocumentXML, scriptfile, glDocumentXMLLength);
                        scriptfile[glDocumentXMLLength] = 0;
                        acDataXML(Self->Document, scriptfile);
                        FreeMemory(scriptfile);
                        error = ERR_Okay;
                     }
                     else error = ERR_AllocMemory;
                  }
                  else error = ERR_NewObject;
               }
               else error = ERR_Init;
            }
            else error = ERR_SetField;

            if (error) { acFree(surface); surface_id = 0; }

            ReleaseObject(surface);
         }
         else error = ERR_NewObject;
      }
      else error = ERR_Init;

      if (error) { acFree(win); Self->WindowID = 0; }

      ReleaseObject(win);
   }
   else error = ERR_NewObject;

   AdjustLogLevel(-1);

   LogBack();
   return error;
}

//****************************************************************************

#include "class_dialog_def.c"

static const struct FieldArray clFields[] = {
   { "Document",     FDF_OBJECT|FDF_R,     ID_DOCUMENT, NULL, NULL },
   { "Window",       FDF_OBJECTID|FDF_RW,  0, NULL, NULL },
   { "Target",       FDF_OBJECTID|FDF_RI,  0, NULL, NULL },
   { "Flags",        FDF_LONGFLAGS|FDF_RW, (MAXINT)&clDialogFlags, NULL, NULL },
   { "Response",     FDF_LONGFLAGS|FDF_RW, (MAXINT)&clDialogResponse, NULL, SET_Response },
   { "Value",        FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "StickToFront", FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "PopOver",      FDF_OBJECTID|FDF_RW,  0, NULL, SET_PopOver },
   { "Type",         FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clDialogType, NULL, SET_Type },
   { "TotalOptions", FDF_LONG|FDF_R,       0, NULL, NULL },
   { "Width",        FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "Height",       FDF_LONG|FDF_RW,      0, NULL, NULL },
   // VIRTUAL FIELDS
   { "Options",   FDF_STRING|FDF_W, 0, NULL, SET_Options },
   { "Icon",      FDF_STRING|FDF_RW, 0, GET_Icon, SET_Icon },
   { "Image",     FDF_STRING|FDF_RW, 0, GET_Image, SET_Image },
   { "Inject",    FDF_STRING|FDF_RW, 0, GET_Inject, SET_Inject },
   { "Option",    FDF_STRING|FDF_RW, 0, GET_Option, SET_Option },
   { "Feedback",  FDF_FUNCTIONPTR|FDF_RW, 0, GET_Feedback, SET_Feedback },
   { "Message",   FDF_STRING|FDF_RW, 0, GET_Message, SET_Message },
   { "Template",  FDF_STRING|FDF_RW, 0, GET_Template, SET_Template },
   { "Title",     FDF_STRING|FDF_RW, 0, GET_Title, SET_Title },
   { "UserInput", FDF_STRING|FDF_RW, 0, GET_UserInput, SET_UserInput },
   // PRIVATE FIELDS
   { "EnvTemplate", FDF_SYSTEM|FDF_STRING|FDF_R, 0, GET_EnvTemplate, NULL },
   { "String",      FDF_SYNONYM|FDF_STRING|FDF_RW, 0, GET_Message, SET_Message },
   END_FIELD
};

//****************************************************************************

ERROR init_dialog(void)
{
   glBreakMessageID = AllocateID(IDTYPE_MESSAGE);

   // Load the default template if the environment specifies one

   if (!LoadFile("templates:dialog.rpl", 0, &glTemplate)) {

   }

   return CreateObject(ID_METACLASS, 0, &clDialog,
      FID_ClassVersion|TFLOAT, VER_DIALOG,
      FID_Name|TSTRING,   "Dialog",
      FID_Category|TLONG, CCF_TOOL,
      FID_Flags|TLONG,    CLF_PROMOTE_INTEGRAL|CLF_PRIVATE_ONLY,
      FID_Actions|TPTR,   clDialogActions,
      FID_Fields|TARRAY,  clFields,
      FID_Size|TLONG,     sizeof(objDialog),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND);
}

//****************************************************************************

void free_dialog(void)
{
   if (glTemplate) { UnloadFile(glTemplate); glTemplate = NULL; }
   if (clDialog)   { acFree(clDialog); clDialog = NULL; }
}
