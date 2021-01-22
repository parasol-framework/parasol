/*****************************************************************************

-CLASS-
Document: Provides document display and editing facilities.

The Document class is a complete Page Layout Engine, providing rich text display
features for creating complex documents and manuals.

-END-

*****************************************************************************/

static ERROR DOCUMENT_ActionNotify(objDocument *Self, struct acActionNotify *Args)
{
   parasol::Log log;

   if (!Args) return ERR_NullArgs;

   if (Args->Error != ERR_Okay) {
      log.trace("Action %d returned error %d", Args->ActionID, Args->Error);
      return ERR_Okay;
   }

   if (Args->ActionID IS AC_Disable) {
      acDisable(Self);
   }
   else if (Args->ActionID IS AC_Enable) {
      acEnable(Self);
   }
   else if (Args->ActionID IS MT_DrwInheritedFocus) {
      // Check that the FocusIndex is accurate (it may have changed if the user clicked on a gadget).

      struct drwInheritedFocus *inherit = (struct drwInheritedFocus *)(Args->Args);
      for (LONG i=0; i < Self->TabIndex; i++) {
         if (Self->Tabs[i].XRef IS inherit->FocusID) {
            Self->FocusIndex = i;
            acDrawID(Self->PageID);
            break;
         }
      }
   }
   else if (Args->ActionID IS AC_Focus) {
      Self->HasFocus = TRUE;

      if (!Self->prvKeyEvent) {
         auto callback = make_function_stdc(key_event);
         SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, &callback, Self, &Self->prvKeyEvent);
      }

      if (Self->FocusIndex != -1) set_focus(Self, Self->FocusIndex, "FocusNotify");
   }
   else if (Args->ActionID IS AC_Free) {
      if ((Self->EventCallback.Type IS CALL_SCRIPT) and (Self->EventCallback.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->EventCallback.Type = CALL_NONE;
      }
   }
   else if (Args->ActionID IS AC_LostFocus) {
      if (Self->prvKeyEvent) { UnsubscribeEvent(Self->prvKeyEvent); Self->prvKeyEvent = NULL; }

      Self->HasFocus = FALSE;

      // Redraw any selected link so that it is unhighlighted

      if ((Self->FocusIndex >= 0) and (Self->FocusIndex < Self->TabIndex)) {
         if (Self->Tabs[Self->FocusIndex].Type IS TT_LINK) {
            for (LONG i=0; i < Self->TotalLinks; i++) {
               if ((Self->Links[i].EscapeCode IS ESC_LINK) and (Self->Links[i].Link->ID IS Self->Tabs[Self->FocusIndex].Ref)) {
                  acDrawAreaID(Self->PageID, Self->Links[i].X, Self->Links[i].Y, Self->Links[i].Width, Self->Links[i].Height);
                  break;
               }
            }
         }
      }
   }
   else if (Args->ActionID IS AC_Redimension) {
      struct acRedimension *redimension;

      if ((redimension = (struct acRedimension *)Args->Args)) {
         drwGetSurfaceCoords(Self->SurfaceID, NULL, NULL, NULL, NULL, &Self->SurfaceWidth, &Self->SurfaceHeight);

         log.traceBranch("Redimension: %dx%d", Self->SurfaceWidth, Self->SurfaceHeight);

         Self->AreaX = (Self->BorderEdge & DBE_LEFT) ? BORDER_SIZE : 0;
         Self->AreaY = (Self->BorderEdge & DBE_TOP) ? BORDER_SIZE : 0;
         Self->AreaWidth  = Self->SurfaceWidth - (((Self->BorderEdge & DBE_RIGHT) ? BORDER_SIZE : 0)<<1);
         Self->AreaHeight = Self->SurfaceHeight - (((Self->BorderEdge & DBE_BOTTOM) ? BORDER_SIZE : 0)<<1);

         acRedimensionID(Self->ViewID, Self->AreaX, Self->AreaY, 0, Self->AreaWidth, Self->AreaHeight, 0);

         DocTrigger *trigger;
         for (trigger=Self->Triggers[DRT_BEFORE_LAYOUT]; trigger; trigger=trigger->Next) {
            if (trigger->Function.Type IS CALL_SCRIPT) {
               // The resize event is triggered just prior to the layout of the document.  This allows the trigger
               // function to resize elements on the page in preparation of the new layout.

               OBJECTPTR script;
               if ((script = trigger->Function.Script.Script)) {
                  const ScriptArg args[] = {
                     { "ViewWidth",  FD_LONG, { .Long = Self->AreaWidth } },
                     { "ViewHeight", FD_LONG, { .Long = Self->AreaHeight } }
                  };
                  scCallback(script, trigger->Function.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
               }
            }
            else if (trigger->Function.Type IS CALL_STDC) {
               auto routine = (void (*)(APTR, objDocument *, LONG Width, LONG Height))trigger->Function.StdC.Routine;
               if (routine) {
                  parasol::SwitchContext context(trigger->Function.StdC.Context);
                  routine(trigger->Function.StdC.Context, Self, Self->AreaWidth, Self->AreaHeight);
               }
            }
         }

         Self->UpdateLayout = TRUE;

         AdjustLogLevel(2);
         layout_doc(Self);
         AdjustLogLevel(-2);
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
Activate: Opens the current document selection.

Calling the Activate action on a document object will cause it to send Activate messages to the child objects that
belong to the document object.

*****************************************************************************/

static ERROR DOCUMENT_Activate(objDocument *Self, APTR Void)
{
   parasol::Log log;
   log.branch();

   ChildEntry list[16];
   LONG count = ARRAYSIZE(list);
   if (!ListChildren(Self->Head.UniqueID, TRUE, list, &count)) {
      for (LONG i=0; i < count; i++) acActivateID(list[i].ObjectID);
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
AddListener: Adds a listener to a document trigger for receiving special callbacks.

Use the AddListener method to receive feedback whenever a document event is triggered.  Triggers are a fundamental part
of document page development, accessible through the &lt;trigger/&gt; tag.  Triggers are normally configured within the
document's page code, however if you need to monitor triggers from outside the loaded document's code, then AddTrigger
will give you that option.

The following triggers are supported:

<types lookup="DRT">
<type name="BEFORE_LAYOUT">Document layout is about to be processed.  C/C++: void BeforeLayout(*Caller, *Document, LONG ViewWidth, LONG ViewHeight)</>
<type name="AFTER_LAYOUT">Document layout has been processed.  C/C++: void AfterLayout(*Caller, *Document, LONG ViewWidth, LONG ViewHeight, LONG PageWidth, LONG PageHeight)</>
<type name="USER_CLICK">User has clicked the document.</>
<type name="USER_CLICK_RELEASE">User click has been released.</>
<type name="USER_MOVEMENT">User is moving the pointer over the document.</>
<type name="REFRESH">Page has been refreshed.  C/C++: void Refresh(*Caller, *Document)</>
<type name="GOT_FOCUS">The document has received the focus.  C/C++: void GotFocus(*Caller, *Document)</>
<type name="LOST_FOCUS">The document has lost the focus.  C/C++: void LostFocus(*Caller, *Document)</>
<type name="LEAVING_PAGE">The currently loaded page is closing (either a new page is being loaded, or the document object is being freed).  C/C++: void LeavingPage(*Caller, *Document)</>
</type>

A listener can be manually removed by calling #RemoveListener(), however this is normally unnecessary. Your
listener will be removed automatically if a new document source is loaded or the document object is terminated.

Please note that a trigger can have multiple listeners attached to it, so a new subscription will not replace any prior
subscriptions, nor is their any detection for multiple copies of a subscription against a trigger.

-INPUT-
int(DRT) Trigger: The unique identifier for the trigger.
ptr(func) Function: The function to call when the trigger activates.

-ERRORS-
Okay
NullArgs
-END-

*****************************************************************************/

static ERROR DOCUMENT_AddListener(objDocument *Self, struct docAddListener *Args)
{
   DocTrigger *trigger;

   if ((!Args) or (!Args->Trigger) or (!Args->Function)) return ERR_NullArgs;

   if (!AllocMemory(sizeof(DocTrigger), MEM_DATA|MEM_NO_CLEAR, &trigger, NULL)) {
      trigger->Function = *Args->Function;
      trigger->Next = Self->Triggers[Args->Trigger];
      Self->Triggers[Args->Trigger] = trigger;
      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

/*****************************************************************************

-METHOD-
ApplyFontStyle: Applies DocStyle information to a font.

This method applies font information from DocStyle structures to new @Font objects.  The Font object
should be uninitialised as it is not possible to apply changes to the font face, style or size after initialisation.

-INPUT-
struct(*DocStyle) Style: Pointer to a DocStyle structure.
obj(Font) Font:  Pointer to a Font object that the style information will be applied to.

-ERRORS-
Okay
NullArgs
-END-

*****************************************************************************/

static ERROR DOCUMENT_ApplyFontStyle(objDocument *Self, struct docApplyFontStyle *Args)
{
   parasol::Log log;
   objFont *font;
   DOCSTYLE *style;

   if ((!Args) or (!(style = Args->Style)) or (!(font = Args->Font))) return log.warning(ERR_NullArgs);

   log.traceBranch("Apply font styling - Face: %s, Style: %s", style->Font->Face, style->Font->Style);

   if (font->Head.Flags & NF_INITIALISED) {
      font->Colour = style->FontColour;
      font->Underline = style->FontUnderline;
   }
   else {
      SetString(font, FID_Face, style->Font->Face);
      SetString(font, FID_Style, style->Font->Style);
      font->Point     = style->Font->Point;
      font->Colour    = style->FontColour;
      font->Underline = style->FontUnderline;
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
CallFunction: Executes any registered function in the currently open document.

This method will execute any registered function in the currently open document.  The name of the function must be
specified in the first parameter and that function must exist in the document's default script.  If the document
contains multiple scripts, then a specific script can be referenced by using the name format 'script.function' where
'script' is the name of the script that contains the function.

Arguments can be passed to the function by setting the Args and TotalArgs parameters.  These need to be specially
formatted - please refer to the @Script class' Exec method for more information on how to configure these
parameters.

-INPUT-
cstr Function:  The name of the function that will be called.
struct(*ScriptArg) Args: Pointer to an optional list of arguments to pass to the procedure.
int TotalArgs: The total number of entries in the Args array.

-ERRORS-
Okay
NullArgs

*****************************************************************************/

static ERROR DOCUMENT_CallFunction(objDocument *Self, struct docCallFunction *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Function)) return log.warning(ERR_NullArgs);

   // Function is in the format 'function()' or 'script.function()'

   OBJECTPTR script;
   CSTRING function_name;
   ERROR error;
   if (!(error = extract_script(Self, Args->Function, &script, &function_name, NULL))) {
      return scExec(script, function_name, Args->Args, Args->TotalArgs);
   }
   else return error;
}

/*****************************************************************************

-ACTION-
Clear: Clears all content from the object.

You can delete all of the document information from a document object by calling the Clear action.  All of the document
data will be deleted from the object and the graphics will be automatically updated as a result of calling this action.

*****************************************************************************/

static ERROR DOCUMENT_Clear(objDocument *Self, APTR Void)
{
   parasol::Log log;

   log.branch();
   unload_doc(Self, 0);
   if (Self->XML) { acFree(Self->XML); Self->XML = NULL; }
   redraw(Self, FALSE);
   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
Clipboard: Full support for clipboard activity is provided through this action.
-END-

*****************************************************************************/

static ERROR DOCUMENT_Clipboard(objDocument *Self, struct acClipboard *Args)
{
   parasol::Log log;
   OBJECTPTR file;
   STRING buffer;
   LONG size;

   if ((!Args) or (!Args->Mode)) return log.warning(ERR_NullArgs);

   if ((Args->Mode IS CLIPMODE_CUT) or (Args->Mode IS CLIPMODE_COPY)) {
      if (Args->Mode IS CLIPMODE_CUT) log.branch("Operation: Cut");
      else log.branch("Operation: Copy");

      // Calculate the length of the highlighted document

      if (Self->SelectEnd != Self->SelectStart) {
         if (!AllocMemory(Self->SelectEnd - Self->SelectStart + 1, MEM_STRING|MEM_NO_CLEAR, &buffer, NULL)) {
            // Copy the selected area into the buffer

            LONG i = Self->SelectStart;
            LONG pos = 0;
            auto str = Self->Stream;
            if (str) {
               while (str[i]) {
                  NEXT_CHAR(str, i);
               }
            }

            buffer[pos] = 0;

            // Send the document to the clipboard object

            objClipboard *clipboard;
            if (!CreateObject(ID_CLIPBOARD, 0, &clipboard, TAGEND)) {
               if (!ActionTags(MT_ClipAddText, clipboard, buffer)) {
                  // Delete the highlighted document if the CUT mode was used
                  if (Args->Mode IS CLIPMODE_CUT) {
                     //delete_selection(Self);
                  }
               }
               else error_dialog("Clipboard Error", "Failed to add document to the system clipboard.", 0);
               acFree(clipboard);
            }

            FreeResource(buffer);
         }
         else return ERR_AllocMemory;
      }

      return ERR_Okay;
   }
   else if (Args->Mode IS CLIPMODE_PASTE) {
      log.branch("Operation: Paste");

      if (!(Self->Flags & DCF_EDIT)) {
         log.warning("Edit mode is not enabled, paste operation aborted.");
         return ERR_Failed;
      }

      objClipboard *clipboard;
      if (!CreateObject(ID_CLIPBOARD, 0, &clipboard, TAGEND)) {
         struct clipGetFiles get = { .Datatype = CLIPTYPE_TEXT, .Index = 0 };
         if (!Action(MT_ClipGetFiles, clipboard, &get)) {
            if (!CreateObject(ID_FILE, 0, &file,
                  FID_Path|TSTR,   get.Files[0],
                  FID_Flags|TLONG, FL_READ,
                  TAGEND)) {

               if ((GetLong(file, FID_Size, &size) IS ERR_Okay) and (size > 0)) {
                  if (!AllocMemory(size+1, MEM_STRING, &buffer, NULL)) {
                     LONG result;
                     if (!acRead(file, buffer, size, &result)) {
                        buffer[result] = 0;
                        acDataText(Self, buffer);
                     }
                     else error_dialog("Clipboard Paste Error", NULL, ERR_Read);
                     FreeResource(buffer);
                  }
                  else error_dialog("Clipboard Paste Error", NULL, ERR_AllocMemory);
               }

               acFree(file);
            }
            else {
               char msg[200];
               StrFormat(msg, sizeof(msg), "Failed to load clipboard file \"%s\"", get.Files[0]);
               error_dialog("Paste Error", msg, 0);
            }
         }
         acFree(clipboard);
      }

      return ERR_Okay;
   }
   else return log.warning(ERR_Args);
}

/*****************************************************************************

-ACTION-
DataFeed: Document data can be sent and consumed via feeds.

Appending content to an active document can be achieved via the data feed feature.  The Document class currently
supports the DATA_DOCUMENT and DATA_XML types for this purpose.

The surface that is associated with the Document object will be redrawn as a result of calling this action.

-ERRORS-
Okay
NullArgs
AllocMemory: The Document's memory buffer could not be expanded.
Mismatch:    The data type that was passed to the action is not supported by the Document class.
-END-

*****************************************************************************/

static ERROR DOCUMENT_DataFeed(objDocument *Self, struct acDataFeed *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Buffer)) return log.warning(ERR_NullArgs);

   if ((Args->DataType IS DATA_TEXT) or (Args->DataType IS DATA_XML)) {
      // Incoming data is translated on the fly and added to the end of the current document page.  The original XML
      // information is retained in case of refresh.
      //
      // Note that in the case of incoming text identified by DATA_TEXT, it is assumed to be in XML format.

      if (Self->Processing) return log.warning(ERR_Recursion);

      if (!Self->XML) {
         if (CreateObject(ID_XML, NF_INTEGRAL, &Self->XML,
               FID_Flags|TLONG, XMF_ALL_CONTENT|XMF_PARSE_HTML|XMF_STRIP_HEADERS,
               TAGEND) != ERR_Okay) {
            return log.warning(ERR_CreateObject);
         }
      }

      log.trace("Appending data to XML #%d at tag index %d.", Self->XML->Head.UniqueID, Self->XML->TagCount);

      if (acDataXML(Self->XML, Args->Buffer) != ERR_Okay) {
         return log.warning(ERR_SetField);
      }

      if (Self->Head.Flags & NF_INITIALISED) {
         // Document is initialised.  Refresh the document from the XML source.

         acRefresh(Self);
      }
      else {
         // Document is not yet initialised.  Processing of the XML will be handled in Init() as required.

      }

      return ERR_Okay;
   }
   else if (Args->DataType IS DATA_INPUT_READY) {
      InputMsg *input, *scan;
      ERROR inputerror;

      while (!gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &input)) {
         if (input->Flags & JTYPE_MOVEMENT) {
            while (!(inputerror = gfxGetInputMsg((struct dcInputReady *)Args->Buffer, 0, &scan))) {
               if (scan->Flags & JTYPE_MOVEMENT) input = scan;
               else break;
            }

            if (input->OverID IS Self->PageID) Self->MouseOver = TRUE;
            else Self->MouseOver = FALSE;

            check_mouse_pos(Self, input->X, input->Y);

            if (inputerror) break;
            else input = scan;

            // Note that this code has to 'drop through' due to the movement consolidation loop earlier in this subroutine.
         }

         if (input->Type IS JET_LMB) {
            if (input->Value > 0) {
               Self->LMB = TRUE;
               check_mouse_click(Self, input->X, input->Y);
            }
            else {
               Self->LMB = FALSE;
               check_mouse_release(Self, input->X, input->Y);
            }
         }
      }
      return ERR_Okay;
   }
   else {
      log.msg("Datatype %d not supported.", Args->DataType);
      return ERR_Mismatch;
   }
}

/*****************************************************************************
-ACTION-
Disable: Disables object functionality.
-END-
*****************************************************************************/

static ERROR DOCUMENT_Disable(objDocument *Self, APTR Void)
{
   Self->Flags |= DCF_DISABLED;
   return ERR_Okay;
}

//****************************************************************************

static ERROR DOCUMENT_Draw(objDocument *Self, APTR Void)
{
   if (Self->SurfaceID) {
      if (Self->Processing) DelayMsg(AC_Draw, Self->Head.UniqueID, NULL);
      else redraw(Self, FALSE);
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

/*****************************************************************************

-METHOD-
Edit: Activates a user editing section within a document.

The Edit method will manually activate an editable section in the document.  This results in the text cursor being
placed at the start of the editable section, where the user may immediately begin editing the section via the keyboard.

If the editable section is associated with an OnEnter trigger, the trigger will be called when the Edit method is
invoked.

-INPUT-
cstr Name: The name of the edit cell that will be activated.
int Flags: Optional flags.

-ERRORS-
Okay
NullArgs
Search: The cell was not found.
-END-

*****************************************************************************/

static ERROR DOCUMENT_Edit(objDocument *Self, struct docEdit *Args)
{
   if (!Args) return ERR_NullArgs;

   if (!Args->Name) {
      if ((Self->CursorIndex IS -1) or (!Self->ActiveEditDef)) return ERR_Okay;
      deactivate_edit(Self, TRUE);
      return ERR_Okay;
   }
   else {
      LONG cellindex;
      ULONG hashname = StrHash(Args->Name, 0);
      if ((cellindex = find_cell(Self, 0, hashname)) >= 0) {
         return activate_edit(Self, cellindex, 0);
      }
      else return ERR_Search;
   }
}

/*****************************************************************************
-ACTION-
Enable: Enables object functionality.
-END-
*****************************************************************************/

static ERROR DOCUMENT_Enable(objDocument *Self, APTR Void)
{
   Self->Flags &= ~DCF_DISABLED;
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
FeedParser: Private. Inserts content into a document during the parsing stage.

Private

-INPUT-
cstr String: Content to insert

-ERRORS-
Okay
NullArgs

*****************************************************************************/

static ERROR DOCUMENT_FeedParser(objDocument *Self, struct docFeedParser *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->String)) return ERR_NullArgs;

   if (!Self->Processing) return log.warning(ERR_Failed);





   return ERR_NoSupport;
}

/*****************************************************************************

-METHOD-
FindIndex: Searches the document stream for an index, returning the start and end points if found.

Use the FindIndex method to search for indexes that have been declared in a loaded document.  Indexes are declared
using the &lt;index/&gt; tag and must be given a unique name.  They are useful for marking areas of interest - such as
a section of content that may change during run-time viewing, or as place-markers for rapid scrolling to an exact
document position.

If the named index exists, then the start and end points (as determined by the opening and closing of the index tag)
will be returned as byte indexes in the document stream.  The starting byte will refer to an ESC_INDEX_START code and
the end byte will refer to an ESC_INDEX_END code.

-INPUT-
cstr Name:  The name of the index to search for.
&int Start: The byte position of the index is returned in this parameter.
&int End:   The byte position at which the index ends is returned in this parameter.

-ERRORS-
Okay: The index was found and the Start and End parameters reflect its position.
NullArgs:
Search: The index was not found.

*****************************************************************************/

static ERROR DOCUMENT_FindIndex(objDocument *Self, struct docFindIndex *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Name)) return log.warning(ERR_NullArgs);

   auto stream = Self->Stream;
   if (!stream) return ERR_Search;

   log.trace("Name: %s", Args->Name);

   ULONG name_hash = StrHash(Args->Name, 0);
   LONG i = 0;
   while (stream[i]) {
      if (stream[i] IS CTRL_CODE) {
         if (ESCAPE_CODE(stream, i) IS ESC_INDEX_START) {
            auto index = escape_data<escIndex>(stream, i);
            if (name_hash IS index->NameHash) {
               LONG end_id = index->ID;
               Args->Start = i;

               NEXT_CHAR(stream, i);

               // Search for the end (ID match)

               while (stream[i]) {
                  if (stream[i] IS CTRL_CODE) {
                     if (ESCAPE_CODE(stream, i) IS ESC_INDEX_END) {
                        auto end = escape_data<escIndexEnd>(stream, i);
                        if (end_id IS end->ID) {
                           Args->End = i;
                           log.trace("Found index at range %d - %d", Args->Start, Args->End);
                           return ERR_Okay;
                        }
                     }
                  }
                  NEXT_CHAR(stream, i);
               }
            }
         }
      }
      NEXT_CHAR(stream, i);
   }

   log.extmsg("Failed to find index '%s'", Args->Name);
   return ERR_Search;
}

/*****************************************************************************
-ACTION-
Focus: Sets the user focus on the document page.
-END-
*****************************************************************************/

static ERROR DOCUMENT_Focus(objDocument *Self, APTR Args)
{
   acFocusID(Self->PageID);
   return ERR_Okay;
}

//****************************************************************************

static ERROR DOCUMENT_Free(objDocument *Self, APTR Void)
{
   if (Self->prvKeyEvent) { UnsubscribeEvent(Self->prvKeyEvent); Self->prvKeyEvent = NULL; }
   if (Self->FlashTimer) { UpdateTimer(Self->FlashTimer, 0); Self->FlashTimer = 0; }

   if ((Self->VScroll) and (Self->FreeVScroll)) { acFree(Self->VScroll); Self->VScroll = NULL; }
   if ((Self->HScroll) and (Self->FreeHScroll)) { acFree(Self->HScroll); Self->HScroll = NULL; }

   if (Self->PageID)    { acFreeID(Self->PageID); Self->PageID = 0; }
   if (Self->ViewID)    { acFreeID(Self->ViewID); Self->ViewID = 0; }
   if (Self->InsertXML) { acFree(Self->InsertXML); Self->InsertXML = NULL; }

   if ((Self->FocusID) and (Self->FocusID != Self->SurfaceID)) {
      OBJECTPTR object;
      if (!AccessObject(Self->FocusID, 5000, &object)) {
         UnsubscribeAction(object, 0);
         ReleaseObject(object);
      }
   }

   if (Self->SurfaceID) {
      OBJECTPTR object;
      if (!AccessObject(Self->SurfaceID, 5000, &object)) {
         drwRemoveCallback(object, NULL);
         UnsubscribeAction(object, 0);
         ReleaseObject(object);
      }
   }

   if (Self->PointerLocked) {
      gfxRestoreCursor(PTR_DEFAULT, Self->Head.UniqueID);
      Self->PointerLocked = FALSE;
   }

   if (Self->PageName) { FreeResource(Self->PageName); Self->PageName = NULL; }
   if (Self->Bookmark) { FreeResource(Self->Bookmark); Self->Bookmark = NULL; }

   unload_doc(Self, ULD_TERMINATE);

   if (Self->Path)        { FreeResource(Self->Path); Self->Path = NULL; }
   if (Self->XML)         { acFree(Self->XML); Self->XML = NULL; }
   if (Self->FontFace)    { FreeResource(Self->FontFace); Self->FontFace = NULL; }
   if (Self->Buffer)      { FreeResource(Self->Buffer); Self->Buffer = NULL; }
   if (Self->Temp)        { FreeResource(Self->Temp); Self->Temp = NULL; }
   if (Self->WorkingPath) { FreeResource(Self->WorkingPath); Self->WorkingPath = NULL; }
   if (Self->Templates)   { acFree(Self->Templates); Self->Templates = NULL; }

   gfxUnsubscribeInput(0);

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
GetVar: Script arguments can be retrieved through this action.
-END-
*****************************************************************************/

static ERROR DOCUMENT_GetVar(objDocument *Self, struct acGetVar *Args)
{
   if ((!Args) or (!Args->Buffer) or (!Args->Field) or (Args->Size < 2)) return ERR_Args;

   CSTRING arg = VarGetString(Self->Vars, Args->Field);
   if (!arg) arg = VarGetString(Self->Params, Args->Field);

   if (arg) {
      StrCopy(arg, Args->Buffer, Args->Size);
      return ERR_Okay;
   }
   else {
      Args->Buffer[0] = 0;
      return ERR_UnsupportedField;
   }
}

//****************************************************************************

static ERROR DOCUMENT_Init(objDocument *Self, APTR Void)
{
   parasol::Log log;
   ERROR error;

   if (!Self->SurfaceID) return log.warning(ERR_UnsupportedOwner);

   if (!Self->FocusID) Self->FocusID = Self->SurfaceID;

   objSurface *surface;
   if (!AccessObject(Self->FocusID, 5000, &surface)) {
      if (surface->Head.ClassID != ID_SURFACE) {
         ReleaseObject(surface);
         return log.warning(ERR_WrongObjectType);
      }

      if (surface->Flags & RNF_HAS_FOCUS) {
         Self->HasFocus = TRUE;

         FUNCTION callback;
         SET_FUNCTION_STDC(callback, (APTR)&key_event);
         SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, &callback, Self, &Self->prvKeyEvent);
      }

      SubscribeActionTags(surface, AC_Focus, AC_LostFocus, MT_DrwInheritedFocus, TAGEND);

      ReleaseObject(surface);
   }

   // Setup the target surface

   if (!AccessObject(Self->SurfaceID, 5000, &surface)) {
      Self->SurfaceWidth = surface->Width;
      Self->SurfaceHeight = surface->Height;

      SetString(surface, FID_Colour, "255,255,255");

      SubscribeActionTags(surface,
         AC_Disable,
         AC_Enable,
         AC_Redimension,
         TAGEND);

      if (Self->Border.Alpha > 0) {
         if (!Self->BorderEdge) Self->BorderEdge = DBE_TOP|DBE_BOTTOM|DBE_RIGHT|DBE_LEFT;
         drwAddCallback(surface, (APTR)&draw_border);
      }

      Self->AreaX = (Self->BorderEdge & DBE_LEFT) ? BORDER_SIZE : 0;
      Self->AreaY = (Self->BorderEdge & DBE_TOP) ? BORDER_SIZE : 0;
      Self->AreaWidth  = Self->SurfaceWidth - (((Self->BorderEdge & DBE_RIGHT) ? BORDER_SIZE : 0)<<1);
      Self->AreaHeight = Self->SurfaceHeight - (((Self->BorderEdge & DBE_BOTTOM) ? BORDER_SIZE : 0)<<1);

      ReleaseObject(surface);
   }
   else return ERR_AccessObject;

   // Allocate the view and page areas

   if (!NewLockedObject(ID_SURFACE, NF_INTEGRAL, &surface, &Self->ViewID)) {
      SetFields(surface,
         FID_Name|TSTR,    "rgnDocView",   // Do not change this name - it can be used by objects to detect if they are placed in a document
         FID_Parent|TLONG, Self->SurfaceID,
         FID_X|TLONG,      Self->AreaX,
         FID_Y|TLONG,      Self->AreaY,
         FID_Width|TLONG,  Self->AreaWidth,
         FID_Height|TLONG, Self->AreaHeight,
         TAGEND);
      if (!acInit(surface)) {
         drwAddCallback(surface, (APTR)&draw_background);
         error = ERR_Okay;
      }
      else { acFree(surface); error = ERR_Init; Self->ViewID = 0; }

      ReleaseObject(surface);
   }
   else error = ERR_NewObject;

   if (error) return error;

   if (!NewLockedObject(ID_SURFACE, NF_INTEGRAL, &surface, &Self->PageID)) {
      SetFields(surface,
         FID_Name|TSTR,       "rgnDocPage",  // Do not change this name - it can be used by objects to detect if they are placed in a document
         FID_Parent|TLONG,    Self->ViewID,
         FID_X|TLONG,         0,
         FID_Y|TLONG,         0,
         FID_MaxWidth|TLONG,  0x7fffffff,
         FID_MaxHeight|TLONG, 0x7fffffff,
         FID_Width|TLONG,     MAX_PAGEWIDTH,
         FID_Height|TLONG,    MAX_PAGEHEIGHT,
         FID_Flags|TLONG,     RNF_TRANSPARENT|RNF_GRAB_FOCUS,
         TAGEND);
      if (!acInit(surface)) {
         drwAddCallback(surface, (APTR)&draw_document);
         gfxSubscribeInput(Self->PageID, JTYPE_MOVEMENT|JTYPE_BUTTON, 0);
         error = ERR_Okay;
      }
      else { acFree(surface); error = ERR_Init; Self->PageID = 0; }

      ReleaseObject(surface);
   }
   else error = ERR_NewObject;

   if (error) return error;

   acShowID(Self->ViewID);
   acShowID(Self->PageID);

   // Scan for scrollbars

   if (!(Self->Flags & DCF_NO_SCROLLBARS)) {
      if ((!Self->VScroll) or (!Self->HScroll)) {
         ChildEntry list[16];
         LONG count = ARRAYSIZE(list);
         if (!ListChildren(Self->SurfaceID, TRUE, list, &count)) {
            for (--count; count >= 0; count--) {
               if ((list[count].ObjectID > 0) and (list[count].ClassID IS (CLASSID)ID_SCROLLBAR)) {
                  objScrollbar *scrollbar;
                  if ((scrollbar = (objScrollbar *)GetObjectAddress(list[count].ObjectID))) {
                     LONG direction;
                     GetLong(scrollbar, FID_Direction, &direction);

                     if ((direction IS SO_HORIZONTAL) and (!Self->HScroll)) {
                        Self->HScroll = scrollbar;
                     }
                     else if ((direction IS SO_VERTICAL) and (!Self->VScroll)) {
                        Self->VScroll = scrollbar;
                     }
                  }
               }
            }
         }
      }

      if (!Self->VScroll) {
         if (CreateObject(ID_SCROLLBAR, NF_INTEGRAL, &Self->VScroll,
               FID_Name|TSTR,      "DocVScrollbar",
               FID_Surface|TLONG,  Self->SurfaceID,
               FID_Monitor|TLONG,  Self->PageID,   // Surface to monitor for wheel-scroll requests
               FID_View|TLONG,     Self->ViewID,
               FID_Direction|TSTR, "vertical",
               FID_Object|TLONG,   Self->Head.UniqueID,
               FID_Opacity|TLONG,  100,
               TAGEND)) {
            return ERR_CreateObject;
         }
         else Self->FreeVScroll = TRUE;
      }

      SURFACEINFO *info;
      if (!drwGetSurfaceInfo(Self->VScroll->RegionID, &info)) {
         Self->ScrollWidth = info->Width;
      }
      else Self->ScrollWidth = 16;

      if (!Self->HScroll) {
         if (CreateObject(ID_SCROLLBAR, NF_INTEGRAL, &Self->HScroll,
               FID_Name|TSTR,       "DocHScrollbar",
               FID_Surface|TLONG,   Self->SurfaceID,
               FID_Monitor|TLONG,   Self->PageID,
               FID_Direction|TSTR,  "horizontal",
               FID_Object|TLONG,    Self->Head.UniqueID,
               FID_Intersect|TLONG, (Self->VScroll) ? Self->VScroll->Head.UniqueID : 0,
               FID_Opacity|TLONG,   100,
               TAGEND)) {
            return ERR_CreateObject;
         }
         else Self->FreeHScroll = TRUE;
      }
   }

   // Flash the cursor via the timer

   if (Self->Flags & DCF_EDIT) {
      FUNCTION function;
      SET_FUNCTION_STDC(function, (APTR)&flash_cursor);
      SubscribeTimer(0.5, &function, &Self->FlashTimer);
   }

   // Load a document file into the line array if required

   Self->UpdateLayout = TRUE;
   if (Self->XML) { // If XML data is already present, it's probably come in through the data channels.
      log.trace("XML data already loaded.");
      if (Self->Path) process_parameters(Self, Self->Path);
      AdjustLogLevel(2);
      process_page(Self, Self->XML);
      AdjustLogLevel(-2);
   }
   else if (Self->Path) {
      if ((Self->Path[0] != '#') and (Self->Path[0] != '?')) {
         if ((error = load_doc(Self, Self->Path, FALSE, 0))) {
            return error;
         }
      }
      else {
         // XML data is probably forthcoming and the location just contains the page name and/or parameters to use.

         process_parameters(Self, Self->Path);
      }
   }

   redraw(Self, TRUE);
   calc_scroll(Self);
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
HideIndex: Hides the content held within a named index.

The HideIndex and ShowIndex methods allow the display of document content to be controlled at code level.  To control
content visibility, start by encapsulating the content in the source document with an &lt;index&gt; tag and ensure that
it is named.  Then make calls to HideIndex and ShowIndex with the index name to manipulate visibility.

The document layout is automatically updated and pushed to the display when this method is called.

-INPUT-
cstr Name: The name of the index.

-ERRORS-
Okay
NullArgs
Search
-END-

*****************************************************************************/

static ERROR DOCUMENT_HideIndex(objDocument *Self, struct docHideIndex *Args)
{
   parasol::Log log(__FUNCTION__);
   LONG tab;

   if ((!Args) or (!Args->Name)) return log.warning(ERR_NullArgs);

   log.msg("Index: %s", Args->Name);

   auto stream = Self->Stream;
   if (!stream) return ERR_Search;

   ULONG name_hash = StrHash(Args->Name, 0);
   LONG i = 0;
   while (stream[i]) {
      if (stream[i] IS CTRL_CODE) {
         if (ESCAPE_CODE(stream, i) IS ESC_INDEX_START) {
            auto index = escape_data<escIndex>(stream, i);
            if (name_hash IS index->NameHash) {
               if (!index->Visible) return ERR_Okay; // It's already invisible!

               index->Visible = FALSE;

               drwForbidDrawing();

                  AdjustLogLevel(2);
                  Self->UpdateLayout = TRUE;
                  layout_doc(Self);
                  AdjustLogLevel(-2);

                  // Any objects within the index will need to be hidden.  Also, set ParentVisible markers to FALSE.

                  NEXT_CHAR(stream, i);
                  while (stream[i]) {
                     if (stream[i] IS CTRL_CODE) {
                        UBYTE code = ESCAPE_CODE(stream, i);
                        if (code IS ESC_INDEX_END) {
                           auto end = escape_data<escIndexEnd>(stream, i);
                           if (index->ID IS end->ID) break;
                        }
                        else if (code IS ESC_OBJECT) {
                           auto escobj = escape_data<escObject>(stream, i);
                           if (escobj->ObjectID) acHideID(escobj->ObjectID);

                           if ((tab = find_tabfocus(Self, TT_OBJECT, escobj->ObjectID)) >= 0) {
                              Self->Tabs[tab].Active = FALSE;
                           }
                        }
                        else if (code IS ESC_LINK) {
                           auto esclink = escape_data<escLink>(stream, i);
                           if ((tab = find_tabfocus(Self, TT_LINK, esclink->ID)) >= 0) {
                              Self->Tabs[tab].Active = FALSE;
                           }
                        }
                        else if (code IS ESC_INDEX_START) {
                           auto index = escape_data<escIndex>(stream, i);
                           index->ParentVisible = FALSE;
                        }
                     }
                     NEXT_CHAR(stream, i);
                  }

               drwPermitDrawing();
               DRAW_PAGE(Self);
               return ERR_Okay;
            }
         }
      }
      NEXT_CHAR(stream, i);
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
InsertXML: Inserts new content into a loaded document (XML format).

Use the InsertXML method to insert new content into an initialised document.

Caution must be exercised when inserting document content.  Inserting an image in-between a set of table rows for
instance, would cause unknown results.  Corruption of the document data may lead to a program crash when the document
is refreshed.

The document view will not be automatically redrawn by this method.  This must be done manually once all modifications
to the document are complete.

-INPUT-
cstr XML: An XML string in RIPL format.
int Index: The byte position at which to insert the new content.

-ERRORS-
Okay
NullArgs
-END-

*****************************************************************************/

static ERROR DOCUMENT_InsertXML(objDocument *Self, struct docInsertXML *Args)
{
   parasol::Log log;
   ERROR error;

   if ((!Args) or (!Args->XML)) return log.warning(ERR_NullArgs);
   if ((Args->Index < -1) or (Args->Index > Self->StreamLen)) return log.warning(ERR_OutOfRange);

   if (!Self->Stream) return ERR_NoData;

   if (!Self->InsertXML) {
      error = CreateObject(ID_XML, NF_INTEGRAL, &Self->InsertXML, FID_Statement|TSTR, Args->XML, TAGEND);
   }
   else error = SetString(Self->InsertXML, FID_Statement, Args->XML);

   if (!error) {
      if (!Self->Buffer) {
         Self->BufferSize = 65536;
         if (AllocMemory(Self->BufferSize, MEM_NO_CLEAR, &Self->Buffer, NULL) != ERR_Okay) {
            return ERR_AllocMemory;
         }
      }

      if (!Self->Temp) {
         Self->TempSize = 65536;
         if (AllocMemory(Self->TempSize, MEM_NO_CLEAR, &Self->Temp, NULL) != ERR_Okay) {
            return ERR_AllocMemory;
         }
      }

      if (!Self->VArg) {
         if (AllocMemory(sizeof(Self->VArg[0]) * MAX_ARGS, MEM_NO_CLEAR, &Self->VArg, NULL) != ERR_Okay) {
            return ERR_AllocMemory;
         }
      }

      Self->UpdateLayout = TRUE;

      Self->ParagraphDepth++; // We have to override the paragraph-content sanity check since we're inserting content on post-processing of the original XML

      if (!(error = insert_xml(Self, Self->InsertXML, Self->InsertXML->Tags[0], (Args->Index IS -1) ? Self->StreamLen : Args->Index, IXF_SIBLINGS|IXF_CLOSESTYLE))) {

      }
      else log.warning("Insert failed for: %s", Args->XML);

      Self->ParagraphDepth--;

      acClear(Self->InsertXML); // Reduce memory usage
   }

   return error;
}

/*****************************************************************************

-METHOD-
InsertText: Inserts new content into a loaded document (raw text format).

Use the InsertXML method to insert new content into an initialised document.

Caution must be exercised when inserting document content.  Inserting an image in-between a set of table rows for
instance, would cause unknown results.  Corruption of the document data may lead to a program crash when the document
is refreshed.

The document view will not be automatically redrawn by this method.  This must be done manually once all modifications
to the document are complete.

-INPUT-
cstr Text: A UTF-8 text string.
int Index: The byte position at which to insert the new content.  If -1, the text will be inserted at the end of the document stream.
int Preformat: If TRUE, the text will be treated as pre-formatted (all whitespace, including consecutive whitespace will be recognised).

-ERRORS-
Okay
NullArgs
-END-

*****************************************************************************/

static ERROR DOCUMENT_InsertText(objDocument *Self, struct docInsertText *Args)
{
   parasol::Log log(__FUNCTION__);

   if ((!Args) or (!Args->Text)) return log.warning(ERR_NullArgs);
   if ((Args->Index < -1) or (Args->Index > Self->StreamLen)) return log.warning(ERR_OutOfRange);

   if (!Self->Stream) return ERR_NoData;

   log.traceBranch("Index: %d, Preformat: %d", Args->Index, Args->Preformat);

   Self->UpdateLayout = TRUE;

   LONG index = Args->Index;
   if (index < 0) index = Self->StreamLen;

   // Clear the font style

   ClearMemory(&Self->Style, sizeof(Self->Style));
   Self->Style.FontStyle.Index = -1;
   Self->Style.FontChange = FALSE;
   Self->Style.StyleChange = FALSE;

   // Find the most recent style at the insertion point

   auto str = Self->Stream;
   LONG i   = Args->Index;
   PREV_CHAR(str, i);
   while (i > 0) {
      if ((str[i] IS CTRL_CODE) and (ESCAPE_CODE(str, i) IS ESC_FONT)) {
         CopyMemory(escape_data<UBYTE>(str, i), &Self->Style.FontStyle, sizeof(escFont));
         log.trace("Found existing font style, font index %d, flags $%.8x.", Self->Style.FontStyle.Index, Self->Style.FontStyle.Options);
         break;
      }
      PREV_CHAR(str, i);
   }

   // If no style is available, we need to create a default font style and insert it at the start of the stream.

   if (Self->Style.FontStyle.Index IS -1) {
      if ((Self->Style.FontStyle.Index = create_font(Self->FontFace, "Regular", Self->FontSize)) IS -1) {
         if ((Self->Style.FontStyle.Index = create_font("Open Sans", "Regular", 12)) IS -1) {
            return ERR_Failed;
         }
      }

      Self->Style.FontStyle.Colour = Self->FontColour;
      Self->Style.FontChange = TRUE;
   }

   objFont *font;
   if ((font = lookup_font(Self->Style.FontStyle.Index, "insert_xml"))) {
      StrCopy(font->Face, Self->Style.Face, sizeof(Self->Style.Face));
      Self->Style.Point = font->Point;
   }

   // Insert the text

   ERROR error = insert_text(Self, &index, Args->Text, StrLength(Args->Text), Args->Preformat);

   #ifdef DBG_STREAM
      print_stream(Self, Self->Stream);
   #endif

   return error;
}

//****************************************************************************

static ERROR DOCUMENT_NewObject(objDocument *Self, APTR Void)
{
   Self->UniqueID = 1000;
   unload_doc(Self, 0);
   return ERR_Okay;
}

//****************************************************************************

static ERROR DOCUMENT_NewOwner(objDocument *Self, struct acNewOwner *Args)
{
   if (!(Self->Head.Flags & NF_INITIALISED)) {
      OBJECTID owner_id = Args->NewOwnerID;
      while ((owner_id) and (GetClassID(owner_id) != ID_SURFACE)) {
         owner_id = GetOwnerID(owner_id);
      }
      if (owner_id) Self->SurfaceID = owner_id;
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
ReadContent: Returns selected content from the document, either as plain text or original byte code.

The ReadContent method extracts content from the document stream, covering a specific area.  It can return the data in
its original RIPPLE based format or translate the content into plain-text (control codes are removed).

If data is extracted in its original format, no post-processing is performed to fix validity errors that may arise from
an invalid data range.  For instance, if an opening paragraph code is not closed with a matching paragraph end point,
this will remain the case in the resulting data.

-INPUT-
int Format: Set to DATA_TEXT to receive plain-text, or DATA_RAW to receive the original byte-code.
int Start:  An index in the document stream from which data will be extracted.
int End:    An index in the document stream at which extraction will stop.
!str Result: The data is returned in this parameter as an allocated string.

-ERRORS-
Okay
NullArgs
OutOfRange: The Start and/or End indexes are not within the stream.
Args
NoData: Operation successful, but no data was present for extraction.

*****************************************************************************/

static ERROR DOCUMENT_ReadContent(objDocument *Self, struct docReadContent *Args)
{
   parasol::Log log(__FUNCTION__);

   if (!Args) return log.warning(ERR_NullArgs);

   Args->Result = NULL;

   if ((Args->Start < 0) or (Args->Start >= Self->StreamLen)) return log.warning(ERR_OutOfRange);
   if ((Args->End < 0) or (Args->End >= Self->StreamLen)) return log.warning(ERR_OutOfRange);
   if (Args->End <= Args->Start) return log.warning(ERR_Args);

   if (Args->Format IS DATA_TEXT) {
      STRING output;
      if (!AllocMemory(Args->End - Args->Start + 1, MEM_NO_CLEAR, &output, NULL)) {
         LONG j = 0;
         LONG i = Args->Start;
         while (i < Args->End) {
            if (Self->Stream[i] IS CTRL_CODE) {
               // Ignore escape codes
            }
            else output[j++] = Self->Stream[i];
            NEXT_CHAR(Self->Stream, i);
         }
         output[j] = 0;

         if (!j) {
            FreeResource(output);
            return ERR_NoData;
         }

         Args->Result = output;
         return ERR_Okay;
      }
      else return log.warning(ERR_AllocMemory);
   }
   else if (Args->Format IS DATA_RAW) {
      STRING output;
      if (!AllocMemory(Args->End - Args->Start + 1, MEM_NO_CLEAR, &output, NULL)) {
         CopyMemory(Self->Stream + Args->Start, output, Args->End - Args->Start);
         output[Args->End - Args->Start] = 0;
         Args->Result = output;
         return ERR_Okay;
      }
      else return log.warning(ERR_AllocMemory);
   }
   else return log.warning(ERR_Args);
}

/*****************************************************************************
-ACTION-
Refresh: Reloads the document data from the original source location.
-END-
*****************************************************************************/

static ERROR DOCUMENT_Refresh(objDocument *Self, APTR Void)
{
   parasol::Log log;

   if (Self->Processing) {
      log.msg("Recursion detected - refresh will be delayed.");
      DelayMsg(AC_Refresh, Self->Head.UniqueID, NULL);
      return ERR_Okay;
   }

   Self->Processing++;

   for (auto trigger=Self->Triggers[DRT_REFRESH]; trigger; trigger=trigger->Next) {
      if (trigger->Function.Type IS CALL_SCRIPT) {
         // The refresh trigger can return ERR_Skip to prevent a complete reload of the document.

         OBJECTPTR script;
         if ((script = trigger->Function.Script.Script)) {
            ERROR error;
            if (!scCallback(script, trigger->Function.Script.ProcedureID, NULL, 0, &error)) {
               if (error IS ERR_Skip) {
                  log.msg("The refresh request has been handled by an event trigger.");
                  return ERR_Okay;
               }
            }
         }
      }
      else if (trigger->Function.Type IS CALL_STDC) {
         auto routine = (void (*)(APTR, objDocument *))trigger->Function.StdC.Routine;
         if (routine) {
            parasol::SwitchContext context(trigger->Function.StdC.Context);
            routine(trigger->Function.StdC.Context, Self);
         }
      }
   }

   ERROR error;
   if ((Self->Path) and (Self->Path[0] != '#') and (Self->Path[0] != '?')) {
      log.branch("Refreshing from path '%s'", Self->Path);
      error = load_doc(Self, Self->Path, TRUE, ULD_REFRESH);
   }
   else if (Self->XML) {
      log.branch("Refreshing from preloaded XML data.");

      AdjustLogLevel(2);
      unload_doc(Self, ULD_REFRESH);
      process_page(Self, Self->XML);
      AdjustLogLevel(-2);

      if (Self->FocusIndex != -1) set_focus(Self, Self->FocusIndex, "Refresh-XML");

      error = ERR_Okay;
   }
   else {
      log.msg("No location or XML data is present in the document.");
      error = ERR_Okay;
   }

   Self->Processing--;

   return error;
}

/*****************************************************************************

-METHOD-
RemoveContent: Removes content from a loaded document.

This method will remove all document content between the Start and End indexes provided as parameters.  The document
layout will also be marked for an update for the next redraw.

-INPUT-
int Start: The byte position at which to start the removal.
int End: The byte position at which the removal ends.

-ERRORS-
Okay
NullArgs
OutOfRange: The area to be removed is outside the bounds of the document's data stream.
Args

*****************************************************************************/

static ERROR DOCUMENT_RemoveContent(objDocument *Self, struct docRemoveContent *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if ((Args->Start < 0) or (Args->Start >= Self->StreamLen)) return log.warning(ERR_OutOfRange);
   if ((Args->End < 0) or (Args->End >= Self->StreamLen)) return log.warning(ERR_OutOfRange);
   if (Args->End <= Args->Start) return log.warning(ERR_Args);

   CopyMemory(Self->Stream + Args->End, Self->Stream + Args->Start, Self->StreamLen - Args->End);
   Self->StreamLen -= Args->End - Args->Start;

   Self->UpdateLayout = TRUE;
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
RemoveListener: Removes a previously configured listener from the document.

This method removes a previously configured listener from the document.  The original parameters that were passed to
#AddListener() must be provided.

-INPUT-
int Trigger: The unique identifier for the trigger.
ptr(func) Function: The function that is called when the trigger activates.

-ERRORS-
Okay
NullArgs

*****************************************************************************/

static ERROR DOCUMENT_RemoveListener(objDocument *Self, struct docRemoveListener *Args)
{
   if ((!Args) or (!Args->Trigger) or (!Args->Function)) return ERR_NullArgs;

   DocTrigger *prev = NULL;
   if (Args->Function->Type IS CALL_STDC) {
      for (auto trigger=Self->Triggers[Args->Trigger]; trigger; trigger=trigger->Next) {
         if ((trigger->Function.Type IS CALL_STDC) and (trigger->Function.StdC.Routine IS Args->Function->StdC.Routine)) {
            if (prev) prev->Next = trigger->Next;
            else Self->Triggers[Args->Trigger] = trigger->Next;
            FreeResource(trigger);
            return ERR_Okay;
         }
         prev = trigger;
      }
   }
   else if (Args->Function->Type IS CALL_SCRIPT) {
      for (auto trigger=Self->Triggers[Args->Trigger]; trigger; trigger=trigger->Next) {
         if ((trigger->Function.Type IS CALL_SCRIPT) and
             (trigger->Function.Script.Script IS Args->Function->Script.Script) and
             (trigger->Function.Script.ProcedureID IS Args->Function->Script.ProcedureID)) {
            if (prev) prev->Next = trigger->Next;
            else Self->Triggers[Args->Trigger] = trigger->Next;
            FreeResource(trigger);
            return ERR_Okay;
         }
         prev = trigger;
      }
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
SaveToObject: Use this action to save edited information as an XML document file.
-END-
*****************************************************************************/

static ERROR DOCUMENT_SaveToObject(objDocument *Self, struct acSaveToObject *Args)
{
   parasol::Log log;
   OBJECTPTR Object;

   if ((!Args) or (!Args->DestID)) return log.warning(ERR_NullArgs);

   log.branch("Destination: %d, Lines: %d", Args->DestID, Self->SegCount);

   if (!AccessObject(Args->DestID, 5000, &Object)) {
      acWrite(Object, "Save not supported.", 0, NULL);
      ReleaseObject(Object);
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
ScrollToPoint: Scrolls a document object's graphical content.
-END-
*****************************************************************************/

static ERROR DOCUMENT_ScrollToPoint(objDocument *Self, struct acScrollToPoint *Args)
{
   if (!Args) return ERR_NullArgs;

   if (Args->Flags & STP_X) Self->XPosition = -Args->X;
   if (Args->Flags & STP_Y) Self->YPosition = -Args->Y;

   // Validation: coordinates must be negative offsets

   if (-Self->YPosition  > Self->PageHeight - Self->AreaHeight) {
      Self->YPosition = -(Self->PageHeight - Self->AreaHeight);
   }

   if (Self->YPosition > 0) Self->YPosition = 0;
   if (Self->XPosition > 0) Self->XPosition = 0;

   //log.msg("%d, %d / %d, %d", (LONG)Args->X, (LONG)Args->Y, Self->XPosition, Self->YPosition);

   acMoveToPointID(Self->PageID, Self->XPosition, Self->YPosition, 0, MTF_X|MTF_Y);
   calc_scroll(Self);
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SelectLink: Selects links in the document.

This method will select a link in the document.  Selecting a link will mean that the link in question will take on a
different appearance (e.g. if a text link, the text will change colour).  If the user presses the enter key when a
hyperlink is selected, that link will be activated.

Selecting a link may also enable drag and drop functionality for that link.

Links are referenced either by their Index in the links array, or by name for links that have named references.  It
should be noted that objects that can receive the focus - such as input boxes and buttons - are also treated as
selectable links due to the nature of their functionality.

-INPUT-
int Index: Index to a link (links are in the order in which they are created in the document, zero being the first link).  Ignored if the Name parameter is set.
cstr Name: The name of the link to select (set to NULL if an Index is defined).

-ERRORS-
Okay
NullArgs
OutOfRange
-END-

*****************************************************************************/

static ERROR DOCUMENT_SelectLink(objDocument *Self, struct docSelectLink *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if ((Args->Name) and (Args->Name[0])) {
/*
      LONG i;
      for (i=0; i < Self->TabIndex; i++) {
         if (Self->Tabs[i].Type IS TT_OBJECT) {
            name = GetObjectName(?)
            if (!(StrMatch(Args->Name, name))) {

            }
         }
         else if (Self->Tabs[i].Type IS TT_LINK) {

         }
      }
*/

      return log.warning(ERR_NoSupport);
   }
   else if ((Args->Index >= 0) and (Args->Index < Self->TabIndex)) {
      Self->FocusIndex = Args->Index;
      set_focus(Self, Args->Index, "SelectLink");
      return ERR_Okay;
   }
   else return log.warning(ERR_OutOfRange);
}

/*****************************************************************************
-ACTION-
SetVar: Passes variable parameters to loaded documents.
-END-
*****************************************************************************/

static ERROR DOCUMENT_SetVar(objDocument *Self, struct acSetVar *Args)
{
   // Please note that it is okay to set zero-length arguments

   if ((!Args) or (!Args->Field)) return ERR_NullArgs;
   if (!Args->Field[0]) return ERR_Args;

   if (!Self->Vars) {
      if (!(Self->Vars = VarNew(0, 0))) return ERR_AllocMemory;
   }

   return VarSetString(Self->Vars, Args->Field, Args->Value);
}

/*****************************************************************************

-METHOD-
ShowIndex: Shows the content held within a named index.

The HideIndex and ShowIndex methods allow the display of document content to be controlled at code level.  To control
content visibility, start by encapsulating the content in the source document with an &lt;index&gt; tag and ensure that
it is named.  Then make calls to HideIndex and ShowIndex with the index name to manipulate visibility.

The document layout is automatically updated and pushed to the display when this method is called.

-INPUT-
cstr Name: The name of the index.

-ERRORS-
Okay
NullArgs
Search: The index could not be found.
-END-

*****************************************************************************/

static ERROR DOCUMENT_ShowIndex(objDocument *Self, struct docShowIndex *Args)
{
   parasol::Log log;

   if ((!Args) or (!Args->Name)) return log.warning(ERR_NullArgs);

   log.branch("Index: %s", Args->Name);

   auto stream = Self->Stream;
   if (!stream) return ERR_Search;

   ULONG name_hash = StrHash(Args->Name, 0);
   LONG i = 0;
   while (stream[i]) {
      if (stream[i] IS CTRL_CODE) {
         if (ESCAPE_CODE(stream, i) IS ESC_INDEX_START) {
            auto index = escape_data<escIndex>(stream, i);
            if (name_hash IS index->NameHash) {

               if (index->Visible) return ERR_Okay; // It's already visible!

               index->Visible = TRUE;
               if (index->ParentVisible) { // We are visible, but parents must also be visible to show content
                  // Show all objects and manage the ParentVisible status of any child indexes

                  drwForbidDrawing();

                     AdjustLogLevel(2);
                     Self->UpdateLayout = TRUE;
                     layout_doc(Self);
                     AdjustLogLevel(-2);

                     NEXT_CHAR(stream, i);
                     while (stream[i]) {
                        if (stream[i] IS CTRL_CODE) {
                           UBYTE code = ESCAPE_CODE(stream, i);
                           if (code IS ESC_INDEX_END) {
                              auto end = escape_data<escIndexEnd>(stream, i);
                              if (index->ID IS end->ID) break;
                           }
                           else if (code IS ESC_OBJECT) {
                              auto escobj = escape_data<escObject>(stream, i);
                              if (escobj->ObjectID) acShowID(escobj->ObjectID);

                              LONG tab;
                              if ((tab = find_tabfocus(Self, TT_OBJECT, escobj->ObjectID)) >= 0) {
                                 Self->Tabs[tab].Active = TRUE;
                              }
                           }
                           else if (code IS ESC_LINK) {
                              auto esclink = escape_data<escLink>(stream, i);

                              LONG tab;
                              if ((tab = find_tabfocus(Self, TT_LINK, esclink->ID)) >= 0) {
                                 Self->Tabs[tab].Active = TRUE;
                              }
                           }
                           else if (code IS ESC_INDEX_START) {
                              auto index = escape_data<escIndex>(stream, i);
                              index->ParentVisible = TRUE;

                              if (!index->Visible) {
                                 // The child index is not visible, so skip to the end of it before continuing this
                                 // process.

                                 NEXT_CHAR(stream, i);
                                 while (stream[i]) {
                                    if (stream[i] IS CTRL_CODE) {
                                       if (ESCAPE_CODE(stream, i) IS ESC_INDEX_END) {
                                          auto end = escape_data<escIndexEnd>(stream, i);
                                          if (index->ID IS end->ID) {
                                             NEXT_CHAR(stream, i);
                                             break;
                                          }
                                       }
                                    }

                                    NEXT_CHAR(stream, i);
                                 }

                                 continue; // Needed to avoid the NEXT_CHAR at the end of the while
                              }
                           }
                        }

                        NEXT_CHAR(stream, i);
                     } // while

                  drwPermitDrawing();

                  DRAW_PAGE(Self);
               }

               return ERR_Okay;
            }
         }
         if (stream[i]) NEXT_CHAR(stream, i);
      }
      else NEXT_CHAR(stream, i);
   }

   return ERR_Search;
}

//****************************************************************************

#include "document_def.c"

static const FieldArray clFields[] = {
   { "EventMask",    FDF_LARGE|FDF_FLAGS|FDF_RW, (MAXINT)&clDocumentEventMask, NULL, NULL },
   { "Description",  FDF_STRING|FDF_R,     0, NULL, NULL },
   { "FontFace",     FDF_STRING|FDF_RW,    0, NULL, (APTR)SET_FontFace },
   { "Title",        FDF_STRING|FDF_RW,    0, NULL, (APTR)SET_Title },
   { "Author",       FDF_STRING|FDF_RW,    0, NULL, (APTR)SET_Author },
   { "Copyright",    FDF_STRING|FDF_RW,    0, NULL, (APTR)SET_Copyright },
   { "Keywords",     FDF_STRING|FDF_RW,    0, NULL, (APTR)SET_Keywords },
   { "TabFocus",     FDF_OBJECTID|FDF_RW,  0, NULL, NULL },
   { "Surface",      FDF_OBJECTID|FDF_RW,  ID_SURFACE, NULL, (APTR)SET_Surface },
   { "Focus",        FDF_OBJECTID|FDF_RI,  0, NULL, NULL },
   { "Flags",        FDF_LONGFLAGS|FDF_RI, (MAXINT)&clDocumentFlags, NULL, (APTR)SET_Flags },
   { "LeftMargin",   FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "TopMargin",    FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "RightMargin",  FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "BottomMargin", FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "FontSize",     FDF_LONG|FDF_RW,      0, NULL, (APTR)SET_FontSize },
   { "PageHeight",   FDF_LONG|FDF_R,       0, NULL, NULL },
   { "BorderEdge",   FDF_LONGFLAGS|FDF_RI, (MAXINT)&clDocumentBorderEdge, NULL, NULL },
   { "LineHeight",   FDF_LONG|FDF_R,       0, NULL, NULL },
   { "Error",        FDF_LONG|FDF_R,       0, NULL, NULL },
   { "FontColour",   FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "Highlight",    FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "Background",   FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "CursorColour", FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "LinkColour",   FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "VLinkColour",  FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "SelectColour", FDF_RGB|FDF_RW,       0, NULL, NULL },
   { "Border",       FDF_RGB|FDF_RW,       0, NULL, NULL },
   // Virtual fields
   { "DefaultScript", FDF_OBJECT|FDF_I,    0, NULL, (APTR)SET_DefaultScript },
   { "EventCallback", FDF_FUNCTIONPTR|FDF_RW, 0, (APTR)GET_EventCallback, (APTR)SET_EventCallback },
   { "Path",         FDF_STRING|FDF_RW,    0, (APTR)GET_Path, (APTR)SET_Path },
   { "Origin",       FDF_STRING|FDF_RW,    0, (APTR)GET_Path, (APTR)SET_Origin },
   { "PageWidth",    FDF_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, 0, (APTR)GET_PageWidth, (APTR)SET_PageWidth },
   { "Src",          FDF_SYNONYM|FDF_STRING|FDF_RW, 0, (APTR)GET_Path, (APTR)SET_Path },
   { "UpdateLayout", FDF_LONG|FDF_W,       0, NULL, (APTR)SET_UpdateLayout },
   { "Variables",    FDF_POINTER|FDF_SYSTEM|FDF_R, 0, (APTR)GET_Variables, NULL },
   { "WorkingPath",  FDF_STRING|FDF_R,     0, (APTR)GET_WorkingPath, NULL },
   END_FIELD
};
