/*********************************************************************************************************************

-CLASS-
Document: Provides document display and editing facilities.

The Document class is a complete Page Layout Engine, providing rich text display
features for creating complex documents and manuals.

-END-

*********************************************************************************************************************/

static void notify_disable_viewport(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args)
{
   if (!Result) acDisable(CurrentContext());
}

static void notify_enable_viewport(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args)
{
   if (!Result) acEnable(CurrentContext());
}

static void notify_free_viewport(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args)
{
   auto Self = (extDocument *)CurrentContext();
   Self->Scene = NULL;
   Self->Viewport = NULL;

   // If the viewport is being forcibly terminated (e.g. by window closure) then the cleanest way to deal with
   // lingering page resources is to remove them now.

   Self->Resources.clear();
}

// Used by EventCallback for subscribers that disappear without notice.

static void notify_free_event(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args)
{
   auto Self = (extDocument *)CurrentContext();
   Self->EventCallback.Type = CALL_NONE;
}

//********************************************************************************************************************

static void notify_focus_viewport(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args)
{
   auto Self = (extDocument *)CurrentContext();

   if (Result) return;

   Self->HasFocus = true;

   if (Self->FocusIndex != -1) set_focus(Self, Self->FocusIndex, "FocusNotify");
}

static void notify_lostfocus_viewport(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args)
{
   if (Result) return;

   auto Self = (extDocument *)CurrentContext();
   Self->HasFocus = false;

   // Redraw any selected link so that it is unhighlighted

   if ((Self->FocusIndex >= 0) and (Self->FocusIndex < LONG(Self->Tabs.size()))) {
      if (Self->Tabs[Self->FocusIndex].Type IS TT_LINK) {
         for (auto &link : Self->Links) {
            if (link.BaseCode IS ESC::LINK) {
               if (link.asLink()->ID IS Self->Tabs[Self->FocusIndex].Ref) {
                  Self->Page->draw();
                  break;
               }
            }
         }
      }
   }
}

//********************************************************************************************************************
// Receiver for incoming redimension messages from Self->Viewport

static void notify_redimension_viewport(objVectorViewport *Viewport, objVector *Vector, DOUBLE X, DOUBLE Y, DOUBLE Width, DOUBLE Height)
{
   pf::Log log(__FUNCTION__);
   auto Self = (extDocument *)CurrentContext();

   log.traceBranch("Redimension: %.2fx%.2f -> %.2fx%.2f", Self->VPWidth, Self->VPHeight, Width, Height);

   Self->VPWidth = Width;
   Self->VPHeight = Height;

   Self->Area.X = ((Self->BorderEdge & DBE::LEFT) != DBE::NIL) ? BORDER_SIZE : 0;
   Self->Area.Y = ((Self->BorderEdge & DBE::TOP) != DBE::NIL) ? BORDER_SIZE : 0;
   Self->Area.Width  = Self->VPWidth - ((((Self->BorderEdge & DBE::RIGHT) != DBE::NIL) ? BORDER_SIZE : 0)<<1);
   Self->Area.Height = Self->VPHeight - ((((Self->BorderEdge & DBE::BOTTOM) != DBE::NIL) ? BORDER_SIZE : 0)<<1);

   acRedimension(Self->View, Self->Area.X, Self->Area.Y, 0, Self->Area.Width, Self->Area.Height, 0);

   for (auto &trigger : Self->Triggers[LONG(DRT::BEFORE_LAYOUT)]) {
      if (trigger.Type IS CALL_SCRIPT) {
         // The resize event is triggered just prior to the layout of the document.  This allows the trigger
         // function to resize elements on the page in preparation of the new layout.

         const ScriptArg args[] = {
            { "ViewWidth",  Self->Area.Width },
            { "ViewHeight", Self->Area.Height }
         };
         scCallback(trigger.Script.Script, trigger.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
      }
      else if (trigger.Type IS CALL_STDC) {
         auto routine = (void (*)(APTR, extDocument *, LONG, LONG))trigger.StdC.Routine;
         pf::SwitchContext context(trigger.StdC.Context);
         routine(trigger.StdC.Context, Self, Self->Area.Width, Self->Area.Height);
      }
   }

   Self->UpdatingLayout = true;

   pf::LogLevel level(2);
   layout_doc(Self);
}

/*********************************************************************************************************************

-ACTION-
Activate: Opens the current document selection.

Calling the Activate action on a document object will cause it to send Activate messages to the child objects that
belong to the document object.

*********************************************************************************************************************/

static ERROR DOCUMENT_Activate(extDocument *Self, APTR Void)
{
   pf::Log log;
   log.branch();

   pf::vector<ChildEntry> list;
   if (!ListChildren(Self->UID, &list)) {
      for (unsigned i=0; i < list.size(); i++) acActivate(list[i].ObjectID);
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

static ERROR DOCUMENT_AddListener(extDocument *Self, struct docAddListener *Args)
{
   if ((!Args) or (Args->Trigger IS DRT::NIL) or (!Args->Function)) return ERR_NullArgs;

   Self->Triggers[LONG(Args->Trigger)].push_back(*Args->Function);
   return ERR_Okay;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

static ERROR DOCUMENT_CallFunction(extDocument *Self, struct docCallFunction *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Function)) return log.warning(ERR_NullArgs);

   // Function is in the format 'function()' or 'script.function()'

   OBJECTPTR script;
   std::string function_name, args;
   if (auto error = extract_script(Self, Args->Function, &script, function_name, args); !error) {
      return scExec(script, function_name.c_str(), Args->Args, Args->TotalArgs);
   }
   else return error;
}

/*********************************************************************************************************************

-ACTION-
Clear: Clears all content from the object.

You can delete all of the document information from a document object by calling the Clear action.  All of the document
data will be deleted from the object and the graphics will be automatically updated as a result of calling this action.

*********************************************************************************************************************/

static ERROR DOCUMENT_Clear(extDocument *Self, APTR Void)
{
   pf::Log log;

   log.branch();
   unload_doc(Self, ULD::NIL);
   if (Self->XML) { FreeResource(Self->XML); Self->XML = NULL; }
   redraw(Self, false);
   return ERR_Okay;
}

/*********************************************************************************************************************

-ACTION-
Clipboard: Full support for clipboard activity is provided through this action.
-END-

*********************************************************************************************************************/

static ERROR DOCUMENT_Clipboard(extDocument *Self, struct acClipboard *Args)
{
   pf::Log log;

   if ((!Args) or (Args->Mode IS CLIPMODE::NIL)) return log.warning(ERR_NullArgs);

   if ((Args->Mode IS CLIPMODE::CUT) or (Args->Mode IS CLIPMODE::COPY)) {
      if (Args->Mode IS CLIPMODE::CUT) log.branch("Operation: Cut");
      else log.branch("Operation: Copy");

      // Calculate the length of the highlighted document

      if (Self->SelectEnd != Self->SelectStart) {
         auto buffer = stream_to_string(Self, Self->SelectStart, Self->SelectEnd);

         // Send the document to the clipboard object

         objClipboard::create clipboard = { };
         if (clipboard.ok()) {
            if (!clipAddText(*clipboard, buffer.c_str())) {
               // Delete the highlighted document if the CUT mode was used
               if (Args->Mode IS CLIPMODE::CUT) {
                  //delete_selection(Self);
               }
            }
            else error_dialog("Clipboard Error", "Failed to add document to the system clipboard.");
         }
      }

      return ERR_Okay;
   }
   else if (Args->Mode IS CLIPMODE::PASTE) {
      log.branch("Operation: Paste");

      if ((Self->Flags & DCF::EDIT) IS DCF::NIL) {
         log.warning("Edit mode is not enabled, paste operation aborted.");
         return ERR_Failed;
      }

      objClipboard::create clipboard = { };
      if (clipboard.ok()) {
         struct clipGetFiles get = { .Datatype = CLIPTYPE::TEXT, .Index = 0 };
         if (!Action(MT_ClipGetFiles, *clipboard, &get)) {
            objFile::create file = { fl::Path(get.Files[0]), fl::Flags(FL::READ) };
            if (file.ok()) {
               LONG size;
               if ((!file->get(FID_Size, &size)) and (size > 0)) {
                  if (auto buffer = new (std::nothrow) char[size+1]) {
                     LONG result;
                     if (!file->read(buffer, size, &result)) {
                        buffer[result] = 0;
                        acDataText(Self, buffer);
                     }
                     else error_dialog("Clipboard Paste Error", ERR_Read);
                     delete[] buffer;
                  }
                  else error_dialog("Clipboard Paste Error", ERR_AllocMemory);
               }
            }
            else error_dialog("Paste Error", "Failed to load clipboard file \"" + std::string(get.Files[0]) + "\"");
         }
      }

      return ERR_Okay;
   }
   else return log.warning(ERR_Args);
}

/*********************************************************************************************************************

-ACTION-
DataFeed: Document data can be sent and consumed via feeds.

Appending content to an active document can be achieved via the data feed feature.  The Document class currently
supports the `DATA::TEXT` and `DATA::XML` types for this purpose.

-ERRORS-
Okay
NullArgs
AllocMemory: The Document's memory buffer could not be expanded.
Mismatch:    The data type that was passed to the action is not supported by the Document class.
-END-

*********************************************************************************************************************/

static ERROR DOCUMENT_DataFeed(extDocument *Self, struct acDataFeed *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Buffer)) return log.warning(ERR_NullArgs);

   if ((Args->Datatype IS DATA::TEXT) or (Args->Datatype IS DATA::XML)) {
      // Incoming data is translated on the fly and added to the end of the current document page.  The original XML
      // information is retained in case of refresh.
      //
      // Note that in the case of incoming text identified by DATA::TEXT, it is assumed to be in XML format.

      if (Self->Processing) return log.warning(ERR_Recursion);

      if (!Self->XML) {
         if (!(Self->XML = objXML::create::integral(fl::Flags(XMF::ALL_CONTENT|XMF::PARSE_HTML|XMF::STRIP_HEADERS)))) {
            return log.warning(ERR_CreateObject);
         }
      }

      log.trace("Appending data to XML #%d", Self->XML->UID);

      if (acDataXML(Self->XML, Args->Buffer) != ERR_Okay) return log.warning(ERR_SetField);

      if (Self->initialised()) {
         // Document is initialised.  Refresh the document from the XML source.

         acRefresh(Self);
      }
      else {
         // Document is not yet initialised.  Processing of the XML will be handled in Init() as required.

      }

      return ERR_Okay;
   }
   else {
      log.msg("Datatype %d not supported.", LONG(Args->Datatype));
      return ERR_Mismatch;
   }
}

/*********************************************************************************************************************
-ACTION-
Disable: Disables user interactivity.
-END-
*********************************************************************************************************************/

static ERROR DOCUMENT_Disable(extDocument *Self, APTR Void)
{
   Self->Flags |= DCF::DISABLED;
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
Draw: Force a page layout update (if changes are pending) and redraw to the display.
-END-
*********************************************************************************************************************/

static ERROR DOCUMENT_Draw(extDocument *Self, APTR Void)
{
   if (Self->Viewport) {
      if (Self->Processing) Self->Viewport->draw();
      else redraw(Self, false);
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

static ERROR DOCUMENT_Edit(extDocument *Self, struct docEdit *Args)
{
   if (!Args) return ERR_NullArgs;

   if (!Args->Name) {
      if ((!Self->CursorIndex.valid()) or (!Self->ActiveEditDef)) return ERR_Okay;
      deactivate_edit(Self, true);
      return ERR_Okay;
   }
   else if (auto cellindex = find_editable_cell(Self, Args->Name); cellindex >= 0) {
      return activate_cell_edit(Self, cellindex, StreamChar(0,0));
   }
   else return ERR_Search;
}

/*********************************************************************************************************************
-ACTION-
Enable: Enables object functionality.
-END-
*********************************************************************************************************************/

static ERROR DOCUMENT_Enable(extDocument *Self, APTR Void)
{
   Self->Flags &= ~DCF::DISABLED;
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
FeedParser: Private. Inserts content into a document during the parsing stage.

Private

-INPUT-
cstr String: Content to insert

-ERRORS-
Okay
NullArgs

*********************************************************************************************************************/

static ERROR DOCUMENT_FeedParser(extDocument *Self, struct docFeedParser *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->String)) return ERR_NullArgs;

   if (!Self->Processing) return log.warning(ERR_Failed);





   return ERR_NoSupport;
}

/*********************************************************************************************************************

-METHOD-
FindIndex: Searches the document stream for an index, returning the start and end points if found.

Use the FindIndex method to search for indexes that have been declared in a loaded document.  Indexes are declared
using the &lt;index/&gt; tag and must be given a unique name.  They are useful for marking areas of interest - such as
a section of content that may change during run-time viewing, or as place-markers for rapid scrolling to an exact
document position.

If the named index exists, then the start and end points (as determined by the opening and closing of the index tag)
will be returned as byte indexes in the document stream.  The starting byte will refer to an ESC::INDEX_START code and
the end byte will refer to an ESC::INDEX_END code.

-INPUT-
cstr Name:  The name of the index to search for.
&int Start: The byte position of the index is returned in this parameter.
&int End:   The byte position at which the index ends is returned in this parameter.

-ERRORS-
Okay: The index was found and the Start and End parameters reflect its position.
NullArgs:
Search: The index was not found.

*********************************************************************************************************************/

static ERROR DOCUMENT_FindIndex(extDocument *Self, struct docFindIndex *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Name)) return log.warning(ERR_NullArgs);

   log.trace("Name: %s", Args->Name);

   auto name_hash = StrHash(Args->Name);
   for (INDEX i=0; i < INDEX(Self->Stream.size()); i++) {
      if (Self->Stream[i].Code IS ESC::INDEX_START) {
         auto &index = escape_data<bcIndex>(Self, i);
         if (name_hash IS index.NameHash) {
            auto end_id = index.ID;
            Args->Start = i;

            // Search for the end (ID match)

            for (++i; i < INDEX(Self->Stream.size()); i++) {
               if (Self->Stream[i].Code IS ESC::INDEX_END) {
                  if (end_id IS escape_data<bcIndexEnd>(Self, i).ID) {
                     Args->End = i;
                     log.trace("Found index at range %d - %d", Args->Start, Args->End);
                     return ERR_Okay;
                  }
               }
            }
         }
      }
   }

   log.extmsg("Failed to find index '%s'", Args->Name);
   return ERR_Search;
}

/*********************************************************************************************************************
-ACTION-
Focus: Sets the user focus on the document page.
-END-
*********************************************************************************************************************/

static ERROR DOCUMENT_Focus(extDocument *Self, APTR Args)
{
   acFocus(Self->Page);
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR DOCUMENT_Free(extDocument *Self, APTR Void)
{
   if (Self->FlashTimer)  { UpdateTimer(Self->FlashTimer, 0); Self->FlashTimer = 0; }

   Self->Page = NULL; // Page and View are freed by their parent Viewport.
   Self->View = NULL;

   if (Self->InsertXML)      { FreeResource(Self->InsertXML);      Self->InsertXML      = NULL; }
   if (Self->FontFill)       { FreeResource(Self->FontFill);       Self->FontFill       = NULL; }
   if (Self->Highlight)      { FreeResource(Self->Highlight);      Self->Highlight      = NULL; }
   if (Self->Background)     { FreeResource(Self->Background);     Self->Background     = NULL; }
   if (Self->CursorStroke)   { FreeResource(Self->CursorStroke);   Self->CursorStroke   = NULL; }
   if (Self->LinkFill)       { FreeResource(Self->LinkFill);       Self->LinkFill       = NULL; }
   if (Self->VLinkFill)      { FreeResource(Self->VLinkFill);      Self->VLinkFill      = NULL; }
   if (Self->LinkSelectFill) { FreeResource(Self->LinkSelectFill); Self->LinkSelectFill = NULL; }
   if (Self->BorderStroke)   { FreeResource(Self->BorderStroke);   Self->BorderStroke   = NULL; }

   if ((Self->Focus) and (Self->Focus != Self->Viewport)) UnsubscribeAction(Self->Focus, 0);

   if (Self->Viewport) UnsubscribeAction(Self->Viewport, 0);

   if (Self->EventCallback.Type IS CALL_SCRIPT) {
      UnsubscribeAction(Self->EventCallback.Script.Script, AC_Free);
      Self->EventCallback.Type = CALL_NONE;
   }

   unload_doc(Self, ULD::TERMINATE);

   if (Self->XML)       { FreeResource(Self->XML); Self->XML = NULL; }
   if (Self->FontFace)  { FreeResource(Self->FontFace); Self->FontFace = NULL; }
   if (Self->Templates) { FreeResource(Self->Templates); Self->Templates = NULL; }

   Self->~extDocument();
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
GetVar: Script arguments can be retrieved through this action.
-END-
*********************************************************************************************************************/

static ERROR DOCUMENT_GetVar(extDocument *Self, struct acGetVar *Args)
{
   if ((!Args) or (!Args->Buffer) or (!Args->Field) or (Args->Size < 2)) return ERR_Args;

   if (Self->Vars.contains(Args->Field)) {
      StrCopy(Self->Vars[Args->Field], Args->Buffer, Args->Size);
      return ERR_Okay;
   }
   else if (Self->Params.contains(Args->Field)) {
      StrCopy(Self->Params[Args->Field], Args->Buffer, Args->Size);
      return ERR_Okay;
   }

   Args->Buffer[0] = 0;
   return ERR_UnsupportedField;
}

//********************************************************************************************************************

static ERROR DOCUMENT_Init(extDocument *Self, APTR Void)
{
   pf::Log log;

   if (!Self->Viewport) {
      auto owner = GetObjectPtr(Self->ownerID());
      if (owner->Class->ClassID IS ID_VECTORVIEWPORT) {
         Self->Viewport = (objVectorViewport *)owner;
      }
      else return log.warning(ERR_UnsupportedOwner);
   }

   if (!Self->Focus) Self->Focus = Self->Viewport;

   if (Self->Focus->Class->ClassID != ID_VECTORVIEWPORT) {
      return log.warning(ERR_WrongObjectType);
   }

   if ((Self->Focus->Flags & VF::HAS_FOCUS) != VF::NIL) Self->HasFocus = true;

   auto call = make_function_stdc(key_event);
   vecSubscribeKeyboard(Self->Viewport, &call);

   call = make_function_stdc(notify_focus_viewport);
   SubscribeAction(Self->Focus, AC_Focus, &call);

   call = make_function_stdc(notify_lostfocus_viewport);
   SubscribeAction(Self->Focus, AC_LostFocus, &call);

   call = make_function_stdc(notify_disable_viewport);
   SubscribeAction(Self->Viewport, AC_Disable, &call);

   call = make_function_stdc(notify_enable_viewport);
   SubscribeAction(Self->Viewport, AC_Enable, &call);

   call = make_function_stdc(notify_free_viewport);
   SubscribeAction(Self->Viewport, AC_Free, &call);

   call = make_function_stdc(notify_redimension_viewport);
   Self->Viewport->setResizeEvent(call);

   Self->Viewport->get(FID_Width, &Self->VPWidth);
   Self->Viewport->get(FID_Height, &Self->VPHeight);

   FLOAT bkgd[4] = { 1.0, 1.0, 1.0, 1.0 };
   Self->Viewport->setFillColour(bkgd, 4);

   if (Self->BorderStroke) {
      // TODO: Use a VectorPolygon with a custom path based on the BorderEdge values.
      if (Self->BorderEdge IS DBE::NIL) Self->BorderEdge = DBE::TOP|DBE::BOTTOM|DBE::RIGHT|DBE::LEFT;

      objVectorRectangle::create::global(fl::Owner(Self->Page->UID), fl::X(0), fl::Y(0), fl::Width("100%"), fl::Height("100%"),
         fl::StrokeWidth(1), fl::Stroke(Self->BorderStroke));
   }

   Self->Area.X = ((Self->BorderEdge & DBE::LEFT) != DBE::NIL) ? BORDER_SIZE : 0;
   Self->Area.Y = ((Self->BorderEdge & DBE::TOP) != DBE::NIL) ? BORDER_SIZE : 0;
   Self->Area.Width  = Self->VPWidth - ((((Self->BorderEdge & DBE::RIGHT) != DBE::NIL) ? BORDER_SIZE : 0)<<1);
   Self->Area.Height = Self->VPHeight - ((((Self->BorderEdge & DBE::BOTTOM) != DBE::NIL) ? BORDER_SIZE : 0)<<1);

   // Allocate the view and page areas

   //if ((Self->Scene = objVectorScene::create::integral(
   //      fl::Name("docScene"),
   //      fl::Owner(Self->Viewport->UID)))) {
   //}
   //else return ERR_CreateObject;

   Self->Scene = Self->Viewport->Scene;

   if ((Self->View = objVectorViewport::create::integral(
         fl::Name("docView"),
         fl::Owner(Self->Viewport->UID),
         fl::X(Self->Area.X), fl::Y(Self->Area.Y),
         fl::Width(Self->Area.Width), fl::Height(Self->Area.Height)))) {
   }
   else return ERR_CreateObject;

   if ((Self->Page = objVectorViewport::create::integral(
         fl::Name("docPage"),
         fl::Owner(Self->View->UID),
         fl::X(0), fl::Y(0),
         fl::Width(MAX_PAGEWIDTH), fl::Height(MAX_PAGEHEIGHT)))) {

      auto callback = make_function_stdc(consume_input_events);
      vecSubscribeInput(Self->Page,  JTYPE::MOVEMENT|JTYPE::BUTTON, &callback);
   }
   else return ERR_CreateObject;

#ifdef GUIDELINES
   // Temporary rectangle to help analyse page sizing

   objVectorRectangle::create::global({
      fl::Name("pageBorder"),
      fl::Owner(Self->Page->UID),
      fl::X(0), fl::Y(0),
      fl::Width("100%"),
      fl::Height("100%"),
      fl::Stroke("rgb(0,0,255,48)"),
      fl::StrokeWidth(3)});

   objVectorRectangle::create::global({
      fl::Name("viewBorder"),
      fl::Owner(Self->View->UID),
      fl::X(0), fl::Y(0),
      fl::Width("100%"),
      fl::Height("100%"),
      fl::Stroke("rgb(0,255,0,48)"),
      fl::StrokeWidth(3)});
#endif

   // TODO: Create a scrollbar with references to our Target, Page and View viewports

   if ((Self->Flags & DCF::NO_SCROLLBARS) IS DCF::NIL) {


   }

   // Flash the cursor via the timer

   if ((Self->Flags & DCF::EDIT) != DCF::NIL) {
      auto call = make_function_stdc(flash_cursor);
      SubscribeTimer(0.5, &call, &Self->FlashTimer);
   }

   // Load a document file into the line array if required

   Self->UpdatingLayout = true;
   if (Self->XML) { // If XML data is already present, it's probably come in through the data channels.
      log.trace("XML data already loaded.");
      if (!Self->Path.empty()) process_parameters(Self, Self->Path);
      pf::LogLevel level(2);
      process_page(Self, Self->XML);
   }
   else if (!Self->Path.empty()) {
      if ((Self->Path[0] != '#') and (Self->Path[0] != '?')) {
         if (auto error = load_doc(Self, Self->Path, false, ULD::NIL)) {
            return error;
         }
      }
      else {
         // XML data is probably forthcoming and the location just contains the page name and/or parameters to use.

         process_parameters(Self, Self->Path);
      }
   }

   redraw(Self, true);

   return ERR_Okay;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

static ERROR DOCUMENT_HideIndex(extDocument *Self, struct docHideIndex *Args)
{
   pf::Log log(__FUNCTION__);
   LONG tab;

   if ((!Args) or (!Args->Name)) return log.warning(ERR_NullArgs);

   log.msg("Index: %s", Args->Name);

   auto &stream = Self->Stream;
   auto name_hash = StrHash(Args->Name);
   for (INDEX i=0; i < INDEX(stream.size()); i++) {
      if (stream[i].Code IS ESC::INDEX_START) {
         auto &index = escape_data<bcIndex>(Self, i);
         if (name_hash IS index.NameHash) {
            if (!index.Visible) return ERR_Okay; // It's already invisible!

            index.Visible = false;

            {
               pf::LogLevel level(2);
               Self->UpdatingLayout = true;
               layout_doc(Self);
            }

            // Any objects within the index will need to be hidden.  Also, set ParentVisible markers to false.

            for (++i; i < INDEX(stream.size()); i++) {
               auto code = stream[i].Code;
               if (code IS ESC::INDEX_END) {
                  auto &end = escape_data<bcIndexEnd>(Self, i);
                  if (index.ID IS end.ID) break;
               }
               else if (code IS ESC::VECTOR) {
                  auto &vec = escape_data<bcVector>(Self, i);
                  if (vec.ObjectID) acHide(vec.ObjectID);

                  if (auto tab = find_tabfocus(Self, TT_OBJECT, vec.ObjectID); tab >= 0) {
                     Self->Tabs[tab].Active = false;
                  }
               }
               else if (code IS ESC::LINK) {
                  auto &esclink = escape_data<bcLink>(Self, i);
                  if ((tab = find_tabfocus(Self, TT_LINK, esclink.ID)) >= 0) {
                     Self->Tabs[tab].Active = false;
                  }
               }
               else if (code IS ESC::INDEX_START) {
                  auto &index = escape_data<bcIndex>(Self, i);
                  index.ParentVisible = false;
               }
            }

            Self->Viewport->draw();
            return ERR_Okay;
         }
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

static ERROR DOCUMENT_InsertXML(extDocument *Self, struct docInsertXML *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->XML)) return log.warning(ERR_NullArgs);
   if ((Args->Index < -1) or (Args->Index > LONG(Self->Stream.size()))) return log.warning(ERR_OutOfRange);

   if (Self->Stream.empty()) return ERR_NoData;

   ERROR error = ERR_Okay;
   if (!Self->InsertXML) {
      if (!(Self->InsertXML = objXML::create::integral(fl::Statement(Args->XML)))) error = ERR_CreateObject;
   }
   else error = Self->InsertXML->setStatement(Args->XML);

   if (!error) {
      Self->UpdatingLayout = true;

      Self->ParagraphDepth++; // We have to override the paragraph-content sanity check since we're inserting content on post-processing of the original XML

      if (!(error = insert_xml(Self, Self->InsertXML, Self->InsertXML->Tags, (Args->Index IS -1) ? Self->Stream.size() : Args->Index, IXF::SIBLINGS|IXF::CLOSESTYLE))) {

      }
      else log.warning("Insert failed for: %s", Args->XML);

      Self->ParagraphDepth--;

      acClear(Self->InsertXML); // Reduce memory usage
   }

   return error;
}

/*********************************************************************************************************************

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
int Index: Reference to a TEXT control code that will receive the content.  If -1, the text will be inserted at the end of the document stream.
int Char: A character offset within the TEXT control code that will be injected with content.  If -1, the text will be injected at the end of the target string.
int Preformat: If TRUE, the text will be treated as pre-formatted (all whitespace, including consecutive whitespace will be recognised).

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERROR DOCUMENT_InsertText(extDocument *Self, struct docInsertText *Args)
{
   pf::Log log(__FUNCTION__);

   if ((!Args) or (!Args->Text)) return log.warning(ERR_NullArgs);
   if ((Args->Index < -1) or (Args->Index > LONG(Self->Stream.size()))) return log.warning(ERR_OutOfRange);

   log.traceBranch("Index: %d, Preformat: %d", Args->Index, Args->Preformat);

   Self->UpdatingLayout = true;

   INDEX index = Args->Index;
   if (index < 0) index = Self->Stream.size();

   Self->Style = style_status();

   // Find the most recent style at the insertion point

   for (INDEX i = Args->Index - 1; i > 0; i--) {
      if (Self->Stream[i].Code IS ESC::FONT) {
         Self->Style.FontStyle = escape_data<bcFont>(Self, i);
         log.trace("Found existing font style, font index %d, flags $%.8x.", Self->Style.FontStyle.FontIndex, Self->Style.FontStyle.Options);
         break;
      }
   }

   // If no style is available, we need to create a default font style and insert it at the start of the stream.

   if (Self->Style.FontStyle.FontIndex IS -1) {
      if ((Self->Style.FontStyle.FontIndex = create_font(Self->FontFace, "Regular", Self->FontSize)) IS -1) {
         if ((Self->Style.FontStyle.FontIndex = create_font("Open Sans", "Regular", 12)) IS -1) {
            return ERR_Failed;
         }
      }

      Self->Style.FontStyle.Fill = Self->FontFill;
      Self->Style.FaceChange = true;
   }

   if (auto font = Self->Style.FontStyle.getFont()) {
      Self->Style.Face  = font->Face;
      Self->Style.Point = font->Point;
   }

   StreamChar sc(index, 0);
   ERROR error = insert_text(Self, sc, std::string(Args->Text), Args->Preformat);

   #ifdef DBG_STREAM
      print_stream(Self);
   #endif

   return error;
}

//********************************************************************************************************************

static ERROR DOCUMENT_NewObject(extDocument *Self, APTR Void)
{
   new (Self) extDocument;
   Self->UniqueID = 1000;
   unload_doc(Self, ULD::NIL);
   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
ReadContent: Returns selected content from the document, either as plain text or original byte code.

The ReadContent method extracts content from the document stream, covering a specific area.  It can return the data as
a RIPPLE binary stream, or translate the content into plain-text (control codes are removed).

If data is extracted in its original format, no post-processing is performed to fix validity errors that may arise from
an invalid data range.  For instance, if an opening paragraph code is not closed with a matching paragraph end point,
this will remain the case in the resulting data.

-INPUT-
int(DATA) Format: Set to TEXT to receive plain-text, or RAW to receive the original byte-code.
int Start:  An index in the document stream from which data will be extracted.
int End:    An index in the document stream at which extraction will stop.
!str Result: The data is returned in this parameter as an allocated string.

-ERRORS-
Okay
NullArgs
OutOfRange: The Start and/or End indexes are not within the stream.
Args
NoData: Operation successful, but no data was present for extraction.

*********************************************************************************************************************/

static ERROR DOCUMENT_ReadContent(extDocument *Self, struct docReadContent *Args)
{
   pf::Log log(__FUNCTION__);

   if (!Args) return log.warning(ERR_NullArgs);

   Args->Result = NULL;

   if ((Args->Start < 0) or (Args->Start >= LONG(Self->Stream.size()))) return log.warning(ERR_OutOfRange);
   if ((Args->End < 0) or (Args->End >= LONG(Self->Stream.size()))) return log.warning(ERR_OutOfRange);
   if (Args->End <= Args->Start) return log.warning(ERR_Args);

   if (Args->Format IS DATA::TEXT) {
      std::ostringstream buffer;

      for (INDEX i=Args->Start; i < Args->End; i++) {
         if (Self->Stream[i].Code IS ESC::TEXT) {
            buffer << escape_data<bcText>(Self, i).Text;
         }
      }

      auto str = buffer.str();
      if (str.empty()) return ERR_NoData;
      if ((Args->Result = StrClone(str.c_str()))) return ERR_Okay;
      else return log.warning(ERR_AllocMemory);
   }
   else if (Args->Format IS DATA::RAW) {
      STRING output;
      if (!AllocMemory(Args->End - Args->Start + 1, MEM::NO_CLEAR, &output)) {
         CopyMemory(Self->Stream.data() + Args->Start, output, Args->End - Args->Start);
         output[Args->End - Args->Start] = 0;
         Args->Result = output;
         return ERR_Okay;
      }
      else return log.warning(ERR_AllocMemory);
   }
   else return log.warning(ERR_Args);
}

/*********************************************************************************************************************
-ACTION-
Refresh: Reloads the document data from the original source location.
-END-
*********************************************************************************************************************/

static ERROR DOCUMENT_Refresh(extDocument *Self, APTR Void)
{
   pf::Log log;

   if (Self->Processing) {
      log.msg("Recursion detected - refresh will be delayed.");
      QueueAction(AC_Refresh, Self->UID);
      return ERR_Okay;
   }

   Self->Processing++;

   for (auto &trigger : Self->Triggers[LONG(DRT::REFRESH)]) {
      if (trigger.Type IS CALL_SCRIPT) {
         // The refresh trigger can return ERR_Skip to prevent a complete reload of the document.

         if (auto script = trigger.Script.Script) {
            ERROR error;
            if (!scCallback(script, trigger.Script.ProcedureID, NULL, 0, &error)) {
               if (error IS ERR_Skip) {
                  log.msg("The refresh request has been handled by an event trigger.");
                  return ERR_Okay;
               }
            }
         }
      }
      else if (trigger.Type IS CALL_STDC) {
         if (auto routine = (void (*)(APTR, extDocument *))trigger.StdC.Routine) {
            pf::SwitchContext context(trigger.StdC.Context);
            routine(trigger.StdC.Context, Self);
         }
      }
   }

   ERROR error = ERR_Okay;
   if ((!Self->Path.empty()) and (Self->Path[0] != '#') and (Self->Path[0] != '?')) {
      log.branch("Refreshing from path '%s'", Self->Path.c_str());
      error = load_doc(Self, Self->Path, true, ULD::REFRESH);
   }
   else if (Self->XML) {
      log.branch("Refreshing from preloaded XML data.");

      {
         pf::LogLevel level(2);
         unload_doc(Self, ULD::REFRESH);
         process_page(Self, Self->XML);
      }

      if (Self->FocusIndex != -1) set_focus(Self, Self->FocusIndex, "Refresh-XML");
   }
   else log.msg("No location or XML data is present in the document.");

   Self->Processing--;

   return error;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

static ERROR DOCUMENT_RemoveContent(extDocument *Self, struct docRemoveContent *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if ((Args->Start < 0) or (Args->Start >= LONG(Self->Stream.size()))) return log.warning(ERR_OutOfRange);
   if ((Args->End < 0) or (Args->End >= LONG(Self->Stream.size()))) return log.warning(ERR_OutOfRange);
   if (Args->End <= Args->Start) return log.warning(ERR_Args);

   CopyMemory(Self->Stream.data() + Args->End, Self->Stream.data() + Args->Start, Self->Stream.size() - Args->End);
   Self->Stream.resize(Self->Stream.size() - Args->End - Args->Start);

   Self->UpdatingLayout = true;
   return ERR_Okay;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

static ERROR DOCUMENT_RemoveListener(extDocument *Self, struct docRemoveListener *Args)
{
   if ((!Args) or (!Args->Trigger) or (!Args->Function)) return ERR_NullArgs;

   if (Args->Function->Type IS CALL_STDC) {
      for (auto it = Self->Triggers[Args->Trigger].begin(); it != Self->Triggers[Args->Trigger].end(); it++) {
         if ((it->Type IS CALL_STDC) and (it->StdC.Routine IS Args->Function->StdC.Routine)) {
            Self->Triggers[Args->Trigger].erase(it);
            return ERR_Okay;
         }
      }
   }
   else if (Args->Function->Type IS CALL_SCRIPT) {
      for (auto it = Self->Triggers[Args->Trigger].begin(); it != Self->Triggers[Args->Trigger].end(); it++) {
         if ((it->Type IS CALL_SCRIPT) and
             (it->Script.Script IS Args->Function->Script.Script) and
             (it->Script.ProcedureID IS Args->Function->Script.ProcedureID)) {
            Self->Triggers[Args->Trigger].erase(it);
            return ERR_Okay;
         }
      }
   }

   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
SaveToObject: Use this action to save edited information as an XML document file.
-END-
*********************************************************************************************************************/

static ERROR DOCUMENT_SaveToObject(extDocument *Self, struct acSaveToObject *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   log.branch("Destination: %d, Lines: %d", Args->Dest->UID, LONG(Self->Segments.size()));
   acWrite(Args->Dest, "Save not supported.", 0, NULL);
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
ScrollToPoint: Scrolls a document object's graphical content.
-END-
*********************************************************************************************************************/

static ERROR DOCUMENT_ScrollToPoint(extDocument *Self, struct acScrollToPoint *Args)
{
   if (!Args) return ERR_NullArgs;

   if ((Args->Flags & STP::X) != STP::NIL) Self->XPosition = -Args->X;
   if ((Args->Flags & STP::Y) != STP::NIL) Self->YPosition = -Args->Y;

   // Validation: coordinates must be negative offsets

   if (-Self->YPosition > Self->PageHeight - Self->Area.Height) {
      Self->YPosition = -(Self->PageHeight - Self->Area.Height);
   }

   if (Self->YPosition > 0) Self->YPosition = 0;
   if (Self->XPosition > 0) Self->XPosition = 0;

   //log.msg("%d, %d / %d, %d", (LONG)Args->X, (LONG)Args->Y, Self->XPosition, Self->YPosition);

   acMoveToPoint(Self->Page, Self->XPosition, Self->YPosition, 0, MTF::X|MTF::Y);
   return ERR_Okay;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

static ERROR DOCUMENT_SelectLink(extDocument *Self, struct docSelectLink *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if ((Args->Name) and (Args->Name[0])) {
/*
      LONG i;
      for (i=0; i < Self->Tabs.size(); i++) {
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
   else if ((Args->Index >= 0) and (Args->Index < LONG(Self->Tabs.size()))) {
      Self->FocusIndex = Args->Index;
      set_focus(Self, Args->Index, "SelectLink");
      return ERR_Okay;
   }
   else return log.warning(ERR_OutOfRange);
}

/*********************************************************************************************************************
-ACTION-
SetVar: Passes variable parameters to loaded documents.
-END-
*********************************************************************************************************************/

static ERROR DOCUMENT_SetVar(extDocument *Self, struct acSetVar *Args)
{
   // Please note that it is okay to set zero-length arguments

   if ((!Args) or (!Args->Field)) return ERR_NullArgs;
   if (!Args->Field[0]) return ERR_Args;

   Self->Vars[Args->Field] = Args->Value;

   return ERR_Okay;
}

/*********************************************************************************************************************

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

*********************************************************************************************************************/

static ERROR DOCUMENT_ShowIndex(extDocument *Self, struct docShowIndex *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Name)) return log.warning(ERR_NullArgs);

   log.branch("Index: %s", Args->Name);

   auto &stream = Self->Stream;
   auto name_hash = StrHash(Args->Name);
   for (INDEX i=0; i < INDEX(stream.size()); i++) {
      if (stream[i].Code IS ESC::INDEX_START) {
         auto &index = escape_data<bcIndex>(Self, i);
         if (name_hash != index.NameHash) continue;
         if (index.Visible) return ERR_Okay; // It's already visible!

         index.Visible = true;
         if (index.ParentVisible) { // We are visible, but parents must also be visible to show content
            // Show all objects and manage the ParentVisible status of any child indexes

            {
               pf::LogLevel level(2);
               Self->UpdatingLayout = true;
               layout_doc(Self);
            }

            for (++i; i < INDEX(stream.size()); i++) {
               auto code = stream[i].Code;
               if (code IS ESC::INDEX_END) {
                  if (index.ID IS escape_data<bcIndexEnd>(Self, i).ID) break;
               }
               else if (code IS ESC::VECTOR) {
                  auto &vec = escape_data<bcVector>(Self, i);
                  if (vec.ObjectID) acShow(vec.ObjectID);

                  if (auto tab = find_tabfocus(Self, TT_OBJECT, vec.ObjectID); tab >= 0) {
                     Self->Tabs[tab].Active = true;
                  }
               }
               else if (code IS ESC::LINK) {
                  if (auto tab = find_tabfocus(Self, TT_LINK, escape_data<bcLink>(Self, i).ID); tab >= 0) {
                     Self->Tabs[tab].Active = true;
                  }
               }
               else if (code IS ESC::INDEX_START) {
                  auto &index = escape_data<bcIndex>(Self, i);
                  index.ParentVisible = true;

                  if (!index.Visible) {
                     for (++i; i < INDEX(stream.size()); i++) {
                        if (stream[i].Code IS ESC::INDEX_END) {
                           if (index.ID IS escape_data<bcIndexEnd>(Self, i).ID) break;
                        }
                     }
                  }
               }
            }

            Self->Viewport->draw();
         }

         return ERR_Okay;
      }
   }

   return ERR_Search;
}

//********************************************************************************************************************

#include "document_def.c"

static const FieldArray clFields[] = {
   { "Description",    FDF_STRING|FDF_R },
   { "FontFace",       FDF_STRING|FDF_RW, NULL, SET_FontFace },
   { "Title",          FDF_STRING|FDF_RW, NULL, SET_Title },
   { "Author",         FDF_STRING|FDF_RW, NULL, SET_Author },
   { "Copyright",      FDF_STRING|FDF_RW, NULL, SET_Copyright },
   { "Keywords",       FDF_STRING|FDF_RW, NULL, SET_Keywords },
   { "FontFill",       FDF_STRING|FDF_RW, NULL, SET_FontFill },
   { "Highlight",      FDF_STRING|FDF_RW, NULL, SET_Highlight },
   { "Background",     FDF_STRING|FDF_RW, NULL, SET_Background },
   { "CursorStroke",   FDF_STRING|FDF_RW, NULL, SET_CursorStroke },
   { "LinkFill",       FDF_STRING|FDF_RW, NULL, SET_LinkFill },
   { "VLinkFill",      FDF_STRING|FDF_RW, NULL, SET_VLinkFill },
   { "LinkSelectFill", FDF_STRING|FDF_RW, NULL, SET_LinkSelectFill },
   { "BorderStroke",   FDF_STRING|FDF_RW, NULL, SET_BorderStroke },
   { "Viewport",       FDF_OBJECT|FDF_RW, NULL, SET_Viewport, ID_VECTORVIEWPORT },
   { "Focus",          FDF_OBJECT|FDF_RI, NULL, NULL, ID_VECTORVIEWPORT },
   { "TabFocus",       FDF_OBJECTID|FDF_RW },
   { "EventMask",      FDF_LONGFLAGS|FDF_FLAGS|FDF_RW, NULL, NULL, &clDocumentEventMask },
   { "Flags",          FDF_LONGFLAGS|FDF_RI, NULL, SET_Flags, &clDocumentFlags },
   { "LeftMargin",     FDF_LONG|FDF_RI },
   { "TopMargin",      FDF_LONG|FDF_RI },
   { "RightMargin",    FDF_LONG|FDF_RI },
   { "BottomMargin",   FDF_LONG|FDF_RI },
   { "FontSize",       FDF_LONG|FDF_RW, NULL, SET_FontSize },
   { "PageHeight",     FDF_LONG|FDF_R, NULL, NULL },
   { "BorderEdge",     FDF_LONGFLAGS|FDF_RI, NULL, NULL, &clDocumentBorderEdge },
   { "LineHeight",     FDF_LONG|FDF_R },
   { "Error",          FDF_LONG|FDF_R },
   // Virtual fields
   { "DefaultScript", FDF_OBJECT|FDF_I,        NULL, SET_DefaultScript },
   { "EventCallback", FDF_FUNCTIONPTR|FDF_RW,  GET_EventCallback, SET_EventCallback },
   { "Path",          FDF_STRING|FDF_RW,       GET_Path, SET_Path },
   { "Origin",        FDF_STRING|FDF_RW,       GET_Path, SET_Origin },
   { "PageWidth",     FDF_VARIABLE|FDF_LONG|FDF_PERCENTAGE|FDF_RW, GET_PageWidth, SET_PageWidth },
   { "Src",           FDF_SYNONYM|FDF_STRING|FDF_RW, GET_Path, SET_Path },
   { "WorkingPath",   FDF_STRING|FDF_R,        GET_WorkingPath, NULL },
   END_FIELD
};
