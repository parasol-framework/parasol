/*********************************************************************************************************************

-CLASS-
Document: Provides document display and editing facilities.

The Document class offers a complete page layout engine, providing rich text display features for creating complex
documents and text-based interfaces.  Internally, document data is maintained as a serial byte stream and all
object model information from the source is discarded.  This simplification of the data makes it possible to
edit the document in-place, much the same as any word processor.  Alternatively it can be used for presentation
purposes only, similarly to PDF or HTML formats.  Presentation is achieved by building a vector scene graph in
conjunction with the @Vector module.  This means that the output is compatible with SVG and can be manipulated in
detail with our existing vector API.  Consequently, document formatting is closely integrated with SVG concepts
and seamlessly inherits SVG functionality such as filling and stroking commands.

The native document format in Parasol is RIPL.  Documentation for RIPL is available in the Parasol Wiki.  Other 
document formats may be supported as sub-classes, but bear in mind that document parsing is a one-way trip and
stateful information such as the HTML DOM is not supported.

The Document class does not include a security barrier in its current form.  Documents that include scripted code
should not be processed unless they originate from a trusted source and are confirmed as such.
To mitigate security problems, we recommend that the application is built with some form of sandbox that can prevent
the system being compromised by bad actors.  Utilising a project such as Win32 App Isolation
https://github.com/microsoft/win32-app-isolation is one potential way of doing this.

-END-

*********************************************************************************************************************/

static void notify_disable_viewport(OBJECTPTR Object, ACTIONID ActionID, ERR Result, APTR Args)
{
   if (Result IS ERR::Okay) acDisable(CurrentContext());
}

static void notify_enable_viewport(OBJECTPTR Object, ACTIONID ActionID, ERR Result, APTR Args)
{
   if (Result IS ERR::Okay) acEnable(CurrentContext());
}

static void notify_free_viewport(OBJECTPTR Object, ACTIONID ActionID, ERR Result, APTR Args)
{
   auto Self = (extDocument *)CurrentContext();
   Self->Scene = NULL;
   Self->Viewport = NULL;
   Self->Page = NULL;
   Self->View = NULL;

   // If the viewport is being forcibly terminated (e.g. by window closure) then the cleanest way to deal with
   // lingering page resources is to remove them now.

   Self->Resources.clear();
}

// Used by EventCallback for subscribers that disappear without notice.

static void notify_free_event(OBJECTPTR Object, ACTIONID ActionID, ERR Result, APTR Args)
{
   auto Self = (extDocument *)CurrentContext();
   Self->EventCallback.clear();
}

//********************************************************************************************************************

static void notify_focus_viewport(OBJECTPTR Object, ACTIONID ActionID, ERR Result, APTR Args)
{
   auto Self = (extDocument *)CurrentContext();

   if (Result != ERR::Okay) return;

   Self->HasFocus = true;

   if (Self->FocusIndex != -1) set_focus(Self, Self->FocusIndex, "FocusNotify");
}

static void notify_lostfocus_viewport(OBJECTPTR Object, ACTIONID ActionID, ERR Result, APTR Args)
{
   if (Result != ERR::Okay) return;

   auto Self = (extDocument *)CurrentContext();
   Self->HasFocus = false;

   // Redraw any selected link so that it is unhighlighted

   if ((Self->FocusIndex >= 0) and (Self->FocusIndex < LONG(Self->Tabs.size()))) {
      if (Self->Tabs[Self->FocusIndex].type IS TT::LINK) {
         for (auto &link : Self->Links) {
            if (link.origin.uid IS std::get<BYTECODE>(Self->Tabs[Self->FocusIndex].ref)) {
               Self->Page->draw();
               break;
            }
         }
      }
   }
}

static void notify_listener_free(OBJECTPTR Listener, ACTIONID ActionID, ERR Result, APTR Args)
{
   auto Self = (extDocument *)CurrentContext();

   for (LONG t=0; t < LONG(DRT::END); t++) {
restart:
      auto &triggers = Self->Triggers[t];
      for (auto cb=triggers.begin(); cb != triggers.end(); cb++) {
         if (cb->Context IS Listener) {
            Self->Triggers[t].erase(cb);
            goto restart;
         }
      }
   }
}

//********************************************************************************************************************
// Receiver for events from Self->View, primarily path changes.
// 
// Bear in mind that the XOffset and YOffset of the document's View must be zero initially, and will be controlled by 
// the scrollbar.  For that reason we don't need to do much here other than update the layout of the page.

static ERR feedback_view(objVectorViewport *View, FM Event)
{
   pf::Log log(__FUNCTION__);
   auto Self = (extDocument *)CurrentContext();

   auto width  = View->get<double>(FID_ViewWidth);
   auto height = View->get<double>(FID_ViewHeight);
   if (!width) width = View->get<double>(FID_Width);
   if (!height) height = View->get<double>(FID_Height);

   if ((Self->VPWidth IS width) and (Self->VPHeight IS height)) return ERR::Okay;

   log.traceBranch("Redimension: %gx%g -> %gx%g", Self->VPWidth, Self->VPHeight, width, height);

   Self->VPWidth = width;
   Self->VPHeight = height;
   
   // The resize event is triggered just prior to the layout of the document.  The recipient
   // function can resize elements on the page in advance of the new layout.

   for (auto &trigger : Self->Triggers[LONG(DRT::BEFORE_LAYOUT)]) {
      if (trigger.isScript()) {
         sc::Call(trigger, std::to_array<ScriptArg>({ { "ViewWidth",  Self->VPWidth }, { "ViewHeight", Self->VPHeight } }));
      }
      else if (trigger.isC()) {
         auto routine = (void (*)(APTR, extDocument *, LONG, LONG, APTR))trigger.Routine;
         pf::SwitchContext context(trigger.Context);
         routine(trigger.Context, Self, Self->VPWidth, Self->VPHeight, trigger.Meta);
      }
   }

   Self->UpdatingLayout = true;

#ifndef RETAIN_LOG_LEVEL
   pf::LogLevel level(2);
#endif

   layout_doc(Self);

   return ERR::Okay;
}

/*********************************************************************************************************************

-ACTION-
Activate: Activates all child objects of the document.

Calling the Activate() action on a document object will forward Activate() calls to its child objects.

*********************************************************************************************************************/

static ERR DOCUMENT_Activate(extDocument *Self)
{
   pf::Log log;
   log.branch();

   pf::vector<ChildEntry> list;
   if (ListChildren(Self->UID, &list) IS ERR::Okay) {
      for (unsigned i=0; i < list.size(); i++) {
         pf::ScopedObjectLock obj(list[i].ObjectID);
         if (obj.granted()) acActivate(*obj);
      }
   }

   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
AddListener: Adds a listener to a document trigger for receiving special callbacks.

Use the AddListener() method to receive feedback whenever a document event is triggered.  Triggers are a fundamental part
of document page development, accessible through the `&lt;trigger/&gt;` tag.  Triggers are normally configured within the
document's page code, however if you need to monitor triggers from outside the loaded document's code, then AddListener()
will give you that option.

The following triggers are supported:

<types lookup="DRT">
<type name="BEFORE_LAYOUT">Document layout is about to be processed.  C/C++: `void BeforeLayout(*Caller, *Document, LONG ViewWidth, LONG ViewHeight)`</>
<type name="AFTER_LAYOUT">Document layout has been processed.  C/C++: `void AfterLayout(*Caller, *Document, LONG ViewWidth, LONG ViewHeight, LONG PageWidth, LONG PageHeight)`</>
<type name="USER_CLICK">User has clicked the document.</>
<type name="USER_CLICK_RELEASE">User click has been released.</>
<type name="USER_MOVEMENT">User is moving the pointer over the document.</>
<type name="REFRESH">Page has been refreshed.  C/C++: `void Refresh(*Caller, *Document)`</>
<type name="GOT_FOCUS">The document has received the focus.  C/C++: `void GotFocus(*Caller, *Document)`</>
<type name="LOST_FOCUS">The document has lost the focus.  C/C++: `void LostFocus(*Caller, *Document)`</>
<type name="LEAVING_PAGE">The currently loaded page is closing (either a new page is being loaded, or the document object is being freed).  C/C++: `void LeavingPage(*Caller, *Document)`</>
</type>

A listener can be removed by calling #RemoveListener(), however this is normally unnecessary. Listeners are removed
automatically if a new document source is loaded, or the document object is terminated.

Note that a trigger can have multiple listeners attached to it, so a new subscription will not replace any prior
subscriptions, nor is there any handling for multiple copies of a subscription to a trigger.

-INPUT-
int(DRT) Trigger: The unique identifier for the trigger.
ptr(func) Function: The function to call when the trigger activates.

-ERRORS-
Okay
NullArgs
-END-

*********************************************************************************************************************/

static ERR DOCUMENT_AddListener(extDocument *Self, doc::AddListener *Args)
{
   if ((!Args) or (Args->Trigger IS DRT::NIL) or (!Args->Function)) return ERR::NullArgs;

   Self->Triggers[LONG(Args->Trigger)].push_back(*Args->Function);

   // Scripts can't auto-remove listeners, so a Free subscription is necessary.  Functional
   // subscribers are expected to self-manage however.

   if (Args->Function->isScript()) {
      SubscribeAction(Args->Function->Context, AC::Free, C_FUNCTION(notify_listener_free));
   }
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
CallFunction: Executes any registered function in the currently open document.

This method will execute any registered function in the currently open document.  The name of the function must be
specified in the first parameter and that function must exist in the document's default script.  If the document
contains multiple scripts, then a specific script can be referenced by using the name format `script.function` where
`script` is the name of the script that contains the function.

Arguments can be passed to the function by setting the `Args` and `TotalArgs` parameters.  These need to be specially
formatted - please refer to the @Script class' Exec method for more information on how to configure these
parameters.

-INPUT-
cstr Function:  The name of the function that will be called.
struct(*ScriptArg) Args: Pointer to an optional list of parameters to pass to the procedure.
int TotalArgs: The total number of entries in the `Args` array.

-ERRORS-
Okay
NullArgs

*********************************************************************************************************************/

static ERR DOCUMENT_CallFunction(extDocument *Self, doc::CallFunction *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Function)) return log.warning(ERR::NullArgs);

   // Function is in the format 'function()' or 'script.function()'

   objScript *script;
   std::string function_name, args;
   if (auto error = extract_script(Self, Args->Function, &script, function_name, args); error IS ERR::Okay) {
      return script->exec(function_name.c_str(), Args->Args, Args->TotalArgs);
   }
   else return error;
}

/*********************************************************************************************************************

-ACTION-
Clear: Clears all content from the object.

Using the Clear() action will delete all of the document's content.  The UI will be updated to reflect a clear
document.

*********************************************************************************************************************/

static ERR DOCUMENT_Clear(extDocument *Self)
{
   pf::Log log;

   log.branch();
   unload_doc(Self);
   redraw(Self, false);
   return ERR::Okay;
}

/*********************************************************************************************************************

-ACTION-
Clipboard: Full support for clipboard activity is provided through this action.
-END-

*********************************************************************************************************************/

static ERR DOCUMENT_Clipboard(extDocument *Self, struct acClipboard *Args)
{
   pf::Log log;

   if ((!Args) or (Args->Mode IS CLIPMODE::NIL)) return log.warning(ERR::NullArgs);

   if ((Args->Mode IS CLIPMODE::CUT) or (Args->Mode IS CLIPMODE::COPY)) {
      if (Args->Mode IS CLIPMODE::CUT) log.branch("Operation: Cut");
      else log.branch("Operation: Copy");

      // Calculate the length of the highlighted document

      if (Self->SelectEnd != Self->SelectStart) {
         auto buffer = stream_to_string(Self->Stream, Self->SelectStart, Self->SelectEnd);

         // Send the document to the clipboard object

         objClipboard::create clipboard = { };
         if (clipboard.ok()) {
            if (clipboard->addText(buffer.c_str()) IS ERR::Okay) {
               // Delete the highlighted document if the CUT mode was used
               if (Args->Mode IS CLIPMODE::CUT) {
                  //delete_selection(Self);
               }
            }
            else error_dialog("Clipboard Error", "Failed to add document to the system clipboard.");
         }
      }

      return ERR::Okay;
   }
   else if (Args->Mode IS CLIPMODE::PASTE) {
      log.branch("Operation: Paste");

      if ((Self->Flags & DCF::EDIT) IS DCF::NIL) {
         log.warning("Edit mode is not enabled, paste operation aborted.");
         return ERR::Failed;
      }

      objClipboard::create clipboard = { };
      if (clipboard.ok()) {
         CSTRING *files;
         if (clipboard->getFiles(CLIPTYPE::TEXT, 0, NULL, &files, NULL) IS ERR::Okay) {
            objFile::create file = { fl::Path(files[0]), fl::Flags(FL::READ) };
            if (file.ok()) {
               LONG size;
               if ((file->get(FID_Size, size) IS ERR::Okay) and (size > 0)) {
                  if (auto buffer = new (std::nothrow) char[size+1]) {
                     LONG result;
                     if (file->read(buffer, size, &result) IS ERR::Okay) {
                        buffer[result] = 0;
                        acDataText(Self, buffer);
                     }
                     else error_dialog("Clipboard Paste Error", ERR::Read);
                     delete[] buffer;
                  }
                  else error_dialog("Clipboard Paste Error", ERR::AllocMemory);
               }
            }
            else error_dialog("Paste Error", "Failed to load clipboard file \"" + std::string(files[0]) + "\"");
         }
      }

      return ERR::Okay;
   }
   else return log.warning(ERR::Args);
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

static ERR DOCUMENT_DataFeed(extDocument *Self, struct acDataFeed *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Buffer)) return log.warning(ERR::NullArgs);

   if ((Args->Datatype IS DATA::TEXT) or (Args->Datatype IS DATA::XML)) {
      // Incoming data is translated on the fly and added to the end of the current document page.  The original XML
      // information is retained in case of refresh.
      //
      // NOTE: Content identified by DATA::TEXT is assumed to be in a serialised XML format.

      if (!Self->initialised()) return log.warning(ERR::NotInitialised);
      if (Self->Processing) return log.warning(ERR::Recursion);

      objXML::create xml = {
         fl::Flags(XMF::INCLUDE_WHITESPACE|XMF::PARSE_HTML|XMF::STRIP_HEADERS|XMF::WELL_FORMED),
         fl::Statement(CSTRING(Args->Buffer)),
         fl::ReadOnly(true)
      };

      if (xml.ok()) {
         if (Self->Stream.data.empty()) {
            // If the document is empty then we use the same process as load_doc()
            parser parse(Self, &Self->Stream);
            parse.process_page(*xml);
         }
         else Self->Error = insert_xml(Self, &Self->Stream, *xml, xml->Tags, Self->Stream.size(), STYLE::NIL);

         Self->UpdatingLayout = true;
         if (Self->initialised()) redraw(Self, true);

         #ifdef DBG_STREAM
            print_stream(Self->Stream);
         #endif
         return Self->Error;
      }
      else return log.warning(ERR::CreateObject);
   }
   else return log.warning(ERR::Mismatch);
}

/*********************************************************************************************************************
-ACTION-
Disable: Disables user interactivity.
-END-
*********************************************************************************************************************/

static ERR DOCUMENT_Disable(extDocument *Self)
{
   Self->Flags |= DCF::DISABLED;
   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
Draw: Force a page layout update (if changes are pending) and redraw to the display.
-END-
*********************************************************************************************************************/

static ERR DOCUMENT_Draw(extDocument *Self)
{
   if (Self->Viewport) {
      if (Self->Processing) Self->Viewport->draw();
      else redraw(Self, false);
      return ERR::Okay;
   }
   else return ERR::FieldNotSet;
}

/*********************************************************************************************************************

-METHOD-
Edit: Activates a user editing section within a document.

The Edit() method will manually activate an editable section in the document.  This results in the text cursor being
placed at the start of the editable section, where the user may immediately begin editing the section via the keyboard.

If the editable section is associated with an `OnEnter` trigger, the trigger will be called when the Edit method is
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

static ERR DOCUMENT_Edit(extDocument *Self, doc::Edit *Args)
{
   if (!Args) return ERR::NullArgs;

   if (!Args->Name) {
      if ((!Self->CursorIndex.valid()) or (!Self->ActiveEditDef)) return ERR::Okay;
      deactivate_edit(Self, true);
      return ERR::Okay;
   }
   else if (auto cellindex = Self->Stream.find_editable_cell(Args->Name); cellindex >= 0) {
      return activate_cell_edit(Self, cellindex, stream_char(0,0));
   }
   else return ERR::Search;
}

/*********************************************************************************************************************
-ACTION-
Enable: Enables object functionality.
-END-
*********************************************************************************************************************/

static ERR DOCUMENT_Enable(extDocument *Self)
{
   Self->Flags &= ~DCF::DISABLED;
   return ERR::Okay;
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

static ERR DOCUMENT_FeedParser(extDocument *Self, doc::FeedParser *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->String)) return ERR::NullArgs;

   if (!Self->Processing) return log.warning(ERR::Failed);





   return ERR::NoSupport;
}

/*********************************************************************************************************************

-METHOD-
FindIndex: Searches the document stream for an index, returning the start and end points if found.

Use the FindIndex() method to search for indexes that have been declared in a loaded document.  Indexes are declared
using the `&lt;index/&gt;` tag and must be given a unique name.  They are useful for marking areas of interest - such as
a section of content that may change during run-time viewing, or as place-markers for rapid scrolling to an exact
document position.

If the named index exists, then the start and end points (as determined by the opening and closing of the index tag)
will be returned as byte indexes in the document stream.  The starting byte will refer to an `SCODE::INDEX_START` code and
the end byte will refer to an `SCODE::INDEX_END` code.

-INPUT-
cstr Name:  The name of the index to search for.
&int Start: The byte position of the index is returned in this parameter.
&int End:   The byte position at which the index ends is returned in this parameter.

-ERRORS-
Okay: The index was found and the `Start` and `End` parameters reflect its position.
NullArgs:
Search: The index was not found.

*********************************************************************************************************************/

static ERR DOCUMENT_FindIndex(extDocument *Self, doc::FindIndex *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Name)) return log.warning(ERR::NullArgs);

   log.trace("Name: %s", Args->Name);

   auto name_hash = strihash(Args->Name);
   for (INDEX i=0; i < INDEX(Self->Stream.size()); i++) {
      if (Self->Stream[i].code IS SCODE::INDEX_START) {
         auto &index = Self->Stream.lookup<bc_index>(i);
         if (name_hash IS index.name_hash) {
            auto end_id = index.id;
            Args->Start = i;

            // Search for the end (ID match)

            for (++i; i < INDEX(Self->Stream.size()); i++) {
               if (Self->Stream[i].code IS SCODE::INDEX_END) {
                  if (end_id IS Self->Stream.lookup<bc_index_end>(i).id) {
                     Args->End = i;
                     log.trace("Found index at range %d - %d", Args->Start, Args->End);
                     return ERR::Okay;
                  }
               }
            }
         }
      }
   }

   log.detail("Failed to find index '%s'", Args->Name);
   return ERR::Search;
}

/*********************************************************************************************************************
-ACTION-
Focus: Sets the user focus on the document page.
-END-
*********************************************************************************************************************/

static ERR DOCUMENT_Focus(extDocument *Self, APTR Args)
{
   acFocus(Self->Page);
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR DOCUMENT_Free(extDocument *Self)
{
   if (Self->FlashTimer)  { UpdateTimer(Self->FlashTimer, 0); Self->FlashTimer = 0; }

   if ((Self->Focus) and (Self->Focus != Self->Viewport)) UnsubscribeAction(Self->Focus, AC::NIL);

   if (Self->PretextXML) { FreeResource(Self->PretextXML); Self->PretextXML = NULL; }

   if (Self->Viewport) UnsubscribeAction(Self->Viewport, AC::NIL);

   if (Self->EventCallback.isScript()) {
      UnsubscribeAction(Self->EventCallback.Context, AC::Free);
      Self->EventCallback.clear();
   }

   unload_doc(Self, ULD::TERMINATE);

   if (Self->Templates) { FreeResource(Self->Templates); Self->Templates = NULL; }

   if (Self->Page) { FreeResource(Self->Page); Self->Page = NULL; }
   if (Self->View) { FreeResource(Self->View); Self->View = NULL; }

   Self->~extDocument();
   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
GetKey: Retrieves global variables and URI parameters.

Use GetKey() to access the global variables and URI parameters of a document.  Priority is given to global
variables if there is a name clash.

The current value of each document widget is also available as a global variable accessible from GetKey().  The 
key-value will be given the same name as that specified in the widget's element.

-END-
*********************************************************************************************************************/

static ERR DOCUMENT_GetKey(extDocument *Self, struct acGetKey *Args)
{
   if ((!Args) or (!Args->Value) or (!Args->Key) or (Args->Size < 2)) return ERR::Args;

   if (Self->Vars.contains(Args->Key)) {
      strcopy(Self->Vars[Args->Key], Args->Value, Args->Size);
      return ERR::Okay;
   }
   else if (Self->Params.contains(Args->Key)) {
      strcopy(Self->Params[Args->Key], Args->Value, Args->Size);
      return ERR::Okay;
   }

   Args->Value[0] = 0;
   return ERR::UnsupportedField;
}

/*********************************************************************************************************************

-METHOD-
HideIndex: Hides the content held within a named index.

The HideIndex() and #ShowIndex() methods allow the display of document content to be controlled at code level.  To control
content visibility, start by encapsulating the content in the source document with an `&lt;index&gt;` tag and ensure that
it is named.  Then make calls to HideIndex() and #ShowIndex() with the index name to manipulate visibility.

The document layout is automatically updated and pushed to the display when this method is called.

-INPUT-
cstr Name: The name of the index.

-ERRORS-
Okay
NullArgs
Search
-END-

*********************************************************************************************************************/

static ERR DOCUMENT_HideIndex(extDocument *Self, doc::HideIndex *Args)
{
   pf::Log log(__FUNCTION__);
   LONG tab;

   if ((!Args) or (!Args->Name)) return log.warning(ERR::NullArgs);

   log.msg("Index: %s", Args->Name);

   auto &stream = Self->Stream;
   auto name_hash = strihash(Args->Name);
   for (INDEX i=0; i < INDEX(stream.size()); i++) {
      if (stream[i].code IS SCODE::INDEX_START) {
         auto &index = Self->Stream.lookup<bc_index>(i);
         if (name_hash IS index.name_hash) {
            if (!index.visible) return ERR::Okay; // It's already invisible!

            index.visible = false;

            {
               #ifndef RETAIN_LOG_LEVEL
               pf::LogLevel level(2);
               #endif
               Self->UpdatingLayout = true;
               layout_doc(Self);
            }

            // Any objects within the index will need to be hidden.  Also, set ParentVisible markers to false.

            for (++i; i < INDEX(stream.size()); i++) {
               auto code = stream[i].code;
               if (code IS SCODE::INDEX_END) {
                  auto &end = Self->Stream.lookup<bc_index_end>(i);
                  if (index.id IS end.id) break;
               }
               else if (code IS SCODE::IMAGE) {
                  auto &vec = Self->Stream.lookup<bc_image>(i);
                  if (!vec.rect.empty()) {
                     pf::ScopedObjectLock obj(vec.rect->UID);
                     if (obj.granted()) acHide(*obj);
                  }

                  if (auto tab = find_tabfocus(Self, TT::VECTOR, vec.rect->UID); tab >= 0) {
                     Self->Tabs[tab].active = false;
                  }
               }
               else if (code IS SCODE::LINK) {
                  auto &esclink = Self->Stream.lookup<bc_link>(i);
                  if ((tab = find_tabfocus(Self, TT::LINK, esclink.uid)) >= 0) {
                     Self->Tabs[tab].active = false;
                  }
               }
               else if (code IS SCODE::INDEX_START) {
                  auto &index = Self->Stream.lookup<bc_index>(i);
                  index.parent_visible = false;
               }
            }

            Self->Viewport->draw();
            return ERR::Okay;
         }
      }
   }

   return ERR::Okay;
}

//********************************************************************************************************************

static ERR DOCUMENT_Init(extDocument *Self)
{
   pf::Log log;

   if (!Self->Viewport) {
      if ((Self->Owner) and (Self->Owner->classID() IS CLASSID::VECTORVIEWPORT)) {
         Self->Viewport = (objVectorViewport *)Self->Owner;
      }
      else return log.warning(ERR::UnsupportedOwner);
   }

   if (!Self->Focus) Self->Focus = Self->Viewport;

   if (Self->Focus->classID() != CLASSID::VECTORVIEWPORT) {
      return log.warning(ERR::WrongObjectType);
   }

   if ((Self->Focus->Flags & VF::HAS_FOCUS) != VF::NIL) Self->HasFocus = true;

   if (Self->Viewport->Scene->SurfaceID) { // Make UI subscriptions as long as we're not headless
      Self->Viewport->subscribeKeyboard(C_FUNCTION(key_event));
      SubscribeAction(Self->Focus, AC::Focus, C_FUNCTION(notify_focus_viewport));
      SubscribeAction(Self->Focus, AC::LostFocus, C_FUNCTION(notify_lostfocus_viewport));
      SubscribeAction(Self->Viewport, AC::Disable, C_FUNCTION(notify_disable_viewport));
      SubscribeAction(Self->Viewport, AC::Enable, C_FUNCTION(notify_enable_viewport));
   }

   SubscribeAction(Self->Viewport, AC::Free, C_FUNCTION(notify_free_viewport));

   Self->VPWidth  = Self->Viewport->get<double>(FID_ViewWidth);
   Self->VPHeight = Self->Viewport->get<double>(FID_ViewHeight);
   if (!Self->VPWidth) Self->VPWidth = Self->Viewport->get<double>(FID_Width);
   if (!Self->VPHeight) Self->VPHeight = Self->Viewport->get<double>(FID_Height);

   float bkgd[4] = { 1.0, 1.0, 1.0, 1.0 };
   Self->Viewport->setFillColour(bkgd, 4);

   // Allocate the view and page areas.  NB: If the parent Viewport is terminated then the
   // Page and View references will be nullified automatically.

   //if ((Self->Scene = objVectorScene::create::local(
   //      fl::Name("docScene"),
   //      fl::Owner(Self->Viewport->UID)))) {
   //}
   //else return ERR::CreateObject;

   Self->Scene = Self->Viewport->Scene;
   
   // Note: Initially the view is set to match the size of its container and the document will automatically
   // adjust the page width if the container is resized.  If the client wants to maintain a fixed size
   // document, e.g. for scaling, the the Width and Height of the View can be overridden at any time -
   // this is considered a legitimate approach to enforcing a fixed size document for scaling.

   if ((Self->View = objVectorViewport::create::global(
         fl::Name("docView"),
         fl::Owner(Self->Viewport->UID),
         fl::Overflow(VOF::HIDDEN),
         fl::X(0), fl::Y(0),
         fl::Width(SCALE(1.0)), fl::Height(SCALE(1.0))))) {
   }
   else return ERR::CreateObject;

   if ((Self->Page = objVectorViewport::create::global(
         fl::Name("docPage"),
         fl::Owner(Self->View->UID),
         fl::X(0), fl::Y(0),
         fl::Width(MAX_PAGE_WIDTH), fl::Height(MAX_PAGE_HEIGHT)))) {

      // Recent changes mean that page input handling could be merged with inputevent_cell()
      // if necessary (VectorScene already manages existing use-cases).
      //if (Self->Page->Scene->SurfaceID) {
      //   vecSubscribeInput(Self->Page,  JTYPE::MOVEMENT|JTYPE::BUTTON, C_FUNCTION(consume_input_events));
      //}
   }
   else return ERR::CreateObject;

   Self->View->subscribeFeedback(FM::PATH_CHANGED, C_FUNCTION(feedback_view));

   // Flash the cursor via the timer

   if ((Self->Flags & DCF::EDIT) != DCF::NIL) {
      SubscribeTimer(0.5, C_FUNCTION(flash_cursor), &Self->FlashTimer);
   }

   // Load a document file into the line array if required

   Self->UpdatingLayout = true;
   if (!Self->Path.empty()) {
      if ((Self->Path[0] != '#') and (Self->Path[0] != '?')) {
         if (auto error = load_doc(Self, Self->Path, false); error != ERR::Okay) {
            return error;
         }
      }
      else {
         // XML data is probably forthcoming and the location just contains the page name and/or parameters to use.

         process_parameters(Self, Self->Path);
      }
   }

   redraw(Self, true);

   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
InsertXML: Inserts new content into a loaded document (XML format).

Use the InsertXML() method to insert new content into an initialised document.

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
NoData
CreateObject
OutOfRange
-END-

*********************************************************************************************************************/

static ERR DOCUMENT_InsertXML(extDocument *Self, doc::InsertXML *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->XML)) return log.warning(ERR::NullArgs);
   if ((Args->Index < -1) or (Args->Index > LONG(Self->Stream.size()))) return log.warning(ERR::OutOfRange);

   if (Self->Stream.data.empty()) return ERR::NoData;

   objXML::create xml = {
      fl::Flags(XMF::INCLUDE_WHITESPACE|XMF::PARSE_HTML|XMF::STRIP_HEADERS),
      fl::Statement(Args->XML)
   };

   if (!xml.ok()) {
      Self->UpdatingLayout = true;

      ERR error = insert_xml(Self, &Self->Stream, *xml, xml->Tags, (Args->Index IS -1) ? Self->Stream.size() : Args->Index, STYLE::NIL);
      if (error != ERR::Okay) log.warning("Insert failed for: %s", Args->XML);

      return error;
   }
   else return ERR::CreateObject;
}

/*********************************************************************************************************************

-METHOD-
InsertText: Inserts new content into a loaded document (raw text format).

Use the InsertText() method to insert new content into an initialised document.

Caution must be exercised when inserting document content.  Inserting an image in-between a set of table rows for
instance, would cause unknown results.  Corruption of the document data may lead to a program crash when the document
is refreshed.

The document view will not be automatically redrawn by this method.  This must be done manually once all modifications
to the document are complete.

-INPUT-
cstr Text: A UTF-8 text string.
int Index: Reference to a `TEXT` control code that will receive the content.  If `-1`, the text will be inserted at the end of the document stream.
int Char: A character offset within the `TEXT` control code that will be injected with content.  If `-1`, the text will be injected at the end of the target string.
int Preformat: If `true`, the text will be treated as pre-formatted (all whitespace, including consecutive whitespace will be recognised).

-ERRORS-
Okay
NullArgs
OutOfRange
Failed
-END-

*********************************************************************************************************************/

static ERR DOCUMENT_InsertText(extDocument *Self, doc::InsertText *Args)
{
   pf::Log log(__FUNCTION__);

   if ((!Args) or (!Args->Text)) return log.warning(ERR::NullArgs);
   if ((Args->Index < -1) or (Args->Index > std::ssize(Self->Stream))) return log.warning(ERR::OutOfRange);

   log.traceBranch("Index: %d, Preformat: %d", Args->Index, Args->Preformat);

   Self->UpdatingLayout = true;

   INDEX index = Args->Index;
   if (index < 0) index = Self->Stream.size();

   stream_char sc(index, 0);
   ERR error = insert_text(Self, &Self->Stream, sc, std::string(Args->Text), Args->Preformat);

   #ifdef DBG_STREAM
      print_stream(Self->Stream);
   #endif

   return error;
}

//********************************************************************************************************************

static ERR DOCUMENT_NewObject(extDocument *Self)
{
   unload_doc(Self);
   return ERR::Okay;
}

static ERR DOCUMENT_NewPlacement(extDocument *Self)
{
   new (Self) extDocument;
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
ReadContent: Returns selected content from the document, either as plain text or original byte code.

The ReadContent() method extracts content from the document stream, covering a specific area.  It can return the data as
a RIPL binary stream, or translate the content into plain-text (control codes are removed).

If data is extracted in its original format, no post-processing is performed to fix validity errors that may arise from
an invalid data range.  For instance, if an opening paragraph code is not closed with a matching paragraph end point,
this will remain the case in the resulting data.

-INPUT-
int(DATA) Format: Set to `TEXT` to receive plain-text, or `RAW` to receive the original byte-code.
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

static ERR DOCUMENT_ReadContent(extDocument *Self, doc::ReadContent *Args)
{
   pf::Log log(__FUNCTION__);

   if (!Args) return log.warning(ERR::NullArgs);

   Args->Result = NULL;

   if ((Args->Start < 0) or (Args->Start >= std::ssize(Self->Stream))) return log.warning(ERR::OutOfRange);
   if ((Args->End < 0) or (Args->End >= std::ssize(Self->Stream))) return log.warning(ERR::OutOfRange);
   if (Args->End <= Args->Start) return log.warning(ERR::Args);

   if (Args->Format IS DATA::TEXT) {
      std::ostringstream buffer;

      for (INDEX i=Args->Start; i < Args->End; i++) {
         if (Self->Stream[i].code IS SCODE::TEXT) {
            buffer << Self->Stream.lookup<bc_text>(i).text;
         }
      }

      auto str = buffer.str();
      if (str.empty()) return ERR::NoData;
      if ((Args->Result = strclone(str))) return ERR::Okay;
      else return log.warning(ERR::AllocMemory);
   }
   else if (Args->Format IS DATA::RAW) {
      STRING output;
      if (AllocMemory(Args->End - Args->Start + 1, MEM::NO_CLEAR, &output) IS ERR::Okay) {
         copymem(Self->Stream.data.data() + Args->Start, output, Args->End - Args->Start);
         output[Args->End - Args->Start] = 0;
         Args->Result = output;
         return ERR::Okay;
      }
      else return log.warning(ERR::AllocMemory);
   }
   else return log.warning(ERR::Args);
}

/*********************************************************************************************************************
-ACTION-
Refresh: Reloads the document data from the original source location.
-END-
*********************************************************************************************************************/

static ERR DOCUMENT_Refresh(extDocument *Self)
{
   pf::Log log;

   if (Self->Processing) {
      log.msg("Recursion detected - refresh will be delayed.");
      QueueAction(AC::Refresh, Self->UID);
      return ERR::Okay;
   }

   Self->Processing++;

   for (auto &trigger : Self->Triggers[LONG(DRT::REFRESH)]) {
      if (trigger.isScript()) {
         // The refresh trigger can return ERR::Skip to prevent a complete reload of the document.

         ERR error;
         if (sc::Call(trigger, error) IS ERR::Okay) {
            if (error IS ERR::Skip) {
               log.msg("The refresh request has been handled by an event trigger.");
               return ERR::Okay;
            }
         }
      }
      else if (trigger.isC()) {
         auto routine = (void (*)(APTR, extDocument *))trigger.Routine;
         pf::SwitchContext context(trigger.Context);
         routine(trigger.Context, Self);
      }
   }

   ERR error = ERR::Okay;
   if ((!Self->Path.empty()) and (Self->Path[0] != '#') and (Self->Path[0] != '?')) {
      log.branch("Refreshing from path '%s'", Self->Path.c_str());
      error = load_doc(Self, Self->Path, true, ULD::REFRESH);
   }
   else log.msg("No source Path defined in the document.");

   Self->Processing--;

   return error;
}

/*********************************************************************************************************************

-METHOD-
RemoveContent: Removes content from a loaded document.

This method will remove all document content between the `Start` and `End` indexes provided as parameters.  The document
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

static ERR DOCUMENT_RemoveContent(extDocument *Self, doc::RemoveContent *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);

   if ((Args->Start < 0) or (Args->Start >= std::ssize(Self->Stream))) return log.warning(ERR::OutOfRange);
   if ((Args->End < 0) or (Args->End >= std::ssize(Self->Stream))) return log.warning(ERR::OutOfRange);
   if (Args->End <= Args->Start) return log.warning(ERR::Args);

   copymem(Self->Stream.data.data() + Args->End, Self->Stream.data.data() + Args->Start, Self->Stream.data.size() - Args->End);
   Self->Stream.data.resize(Self->Stream.data.size() - Args->End - Args->Start);

   Self->UpdatingLayout = true;
   return ERR::Okay;
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

static ERR DOCUMENT_RemoveListener(extDocument *Self, doc::RemoveListener *Args)
{
   if ((!Args) or (!Args->Trigger) or (!Args->Function)) return ERR::NullArgs;

   if (Args->Function->isC()) {
      for (auto it = Self->Triggers[Args->Trigger].begin(); it != Self->Triggers[Args->Trigger].end(); it++) {
         if ((it->isC()) and (it->Routine IS Args->Function->Routine)) {
            Self->Triggers[Args->Trigger].erase(it);
            return ERR::Okay;
         }
      }
   }
   else if (Args->Function->isScript()) {
      for (auto it = Self->Triggers[Args->Trigger].begin(); it != Self->Triggers[Args->Trigger].end(); it++) {
         if ((it->isScript()) and
             (it->Context IS Args->Function->Context) and
             (it->ProcedureID IS Args->Function->ProcedureID)) {
            Self->Triggers[Args->Trigger].erase(it);
            return ERR::Okay;
         }
      }
   }

   return ERR::Okay;
}

/*********************************************************************************************************************
-ACTION-
SaveToObject: Use this action to save edited information as an XML document file.

*********************************************************************************************************************/

static ERR DOCUMENT_SaveToObject(extDocument *Self, struct acSaveToObject *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);

   log.branch("Destination: %d", Args->Dest->UID);
   acWrite(Args->Dest, "Save not supported.", 0, NULL);
   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
SelectLink: Selects links in the document.

This method will select a link in the document.  Selecting a link will mean that the link in question will take on a
different appearance (e.g. if a text link, the text will change colour).  If the user presses the enter key when a
hyperlink is selected, that link will be activated.

Selecting a link may also enable drag and drop functionality for that link.

Links are referenced either by their `Index` in the links array, or by name for links that have named references.  It
should be noted that objects that can receive the focus - such as input boxes and buttons - are also treated as
selectable links due to the nature of their functionality.

-INPUT-
int Index: Index to a link (links are in the order in which they are created in the document, zero being the first link).  Ignored if the `Name` parameter is set.
cstr Name: The name of the link to select (set to `NULL` if an `Index` is defined).

-ERRORS-
Okay
NullArgs
OutOfRange
-END-

*********************************************************************************************************************/

static ERR DOCUMENT_SelectLink(extDocument *Self, doc::SelectLink *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR::NullArgs);

   if ((Args->Name) and (Args->Name[0])) {
/*
      LONG i;
      for (i=0; i < Self->Tabs.size(); i++) {
         if (Self->Tabs[i].Type IS TT::OBJECT) {
            name = GetObjectName(?)
            if (iequals(args->name, name)) {

            }
         }
         else if (Self->Tabs[i].Type IS TT::LINK) {

         }
      }
*/

      return log.warning(ERR::NoSupport);
   }
   else if ((Args->Index >= 0) and (Args->Index < std::ssize(Self->Tabs))) {
      Self->FocusIndex = Args->Index;
      set_focus(Self, Args->Index, "SelectLink");
      return ERR::Okay;
   }
   else return log.warning(ERR::OutOfRange);
}

/*********************************************************************************************************************
-ACTION-
SetKey: Set a global key-value in the document.
-END-
*********************************************************************************************************************/

static ERR DOCUMENT_SetKey(extDocument *Self, struct acSetKey *Args)
{
   // Note: Zero-length parameter values are permitted.

   if ((!Args) or (!Args->Key)) return ERR::NullArgs;
   if (!Args->Key[0]) return ERR::Args;

   Self->Vars[Args->Key] = Args->Value;

   return ERR::Okay;
}

/*********************************************************************************************************************

-METHOD-
ShowIndex: Shows the content held within a named index.

The #HideIndex() and ShowIndex() methods allow the display of document content to be controlled at code level.  To control
content visibility, start by encapsulating the content in the source document with an `&lt;index&gt;` tag and ensure that
it is named.  Then make calls to #HideIndex() and ShowIndex() with the index name to manipulate visibility.

The document layout is automatically updated and pushed to the display when this method is called.

-INPUT-
cstr Name: The name of the index.

-ERRORS-
Okay
NullArgs
Search: The index could not be found.
-END-

*********************************************************************************************************************/

static ERR DOCUMENT_ShowIndex(extDocument *Self, doc::ShowIndex *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Name)) return log.warning(ERR::NullArgs);

   log.branch("Index: %s", Args->Name);

   auto &stream = Self->Stream;
   auto name_hash = strihash(Args->Name);
   for (INDEX i=0; i < INDEX(stream.size()); i++) {
      if (stream[i].code IS SCODE::INDEX_START) {
         auto &index = Self->Stream.lookup<bc_index>(i);
         if (name_hash != index.name_hash) continue;
         if (index.visible) return ERR::Okay; // It's already visible!

         index.visible = true;
         if (index.parent_visible) { // We are visible, but parents must also be visible to show content
            // Show all objects and manage the ParentVisible status of any child indexes

            {
               #ifndef RETAIN_LOG_LEVEL
               pf::LogLevel level(2);
               #endif
               Self->UpdatingLayout = true;
               layout_doc(Self);
            }

            for (++i; i < INDEX(stream.size()); i++) {
               auto code = stream[i].code;
               if (code IS SCODE::INDEX_END) {
                  if (index.id IS Self->Stream.lookup<bc_index_end>(i).id) break;
               }
               else if (code IS SCODE::IMAGE) {
                  auto &img = Self->Stream.lookup<bc_image>(i);
                  if (!img.rect.empty()) acShow(*img.rect);

                  if (auto tab = find_tabfocus(Self, TT::VECTOR, img.rect->UID); tab >= 0) {
                     Self->Tabs[tab].active = true;
                  }
               }
               else if (code IS SCODE::LINK) {
                  if (auto tab = find_tabfocus(Self, TT::LINK, Self->Stream.lookup<bc_link>(i).uid); tab >= 0) {
                     Self->Tabs[tab].active = true;
                  }
               }
               else if (code IS SCODE::INDEX_START) {
                  auto &index = Self->Stream.lookup<bc_index>(i);
                  index.parent_visible = true;

                  if (!index.visible) {
                     for (++i; i < INDEX(stream.size()); i++) {
                        if (stream[i].code IS SCODE::INDEX_END) {
                           if (index.id IS Self->Stream.lookup<bc_index_end>(i).id) break;
                        }
                     }
                  }
               }
            }

            Self->Viewport->draw();
         }

         return ERR::Okay;
      }
   }

   return ERR::Search;
}

//********************************************************************************************************************

#include "document_def.c"

static const FieldArray clFields[] = {
   { "Description",  FDF_STRING|FDF_R },
   { "Title",        FDF_STRING|FDF_R },
   { "Author",       FDF_STRING|FDF_R },
   { "Copyright",    FDF_STRING|FDF_R },
   { "Keywords",     FDF_STRING|FDF_R },
   { "Viewport",     FDF_OBJECT|FDF_RW, NULL, SET_Viewport, CLASSID::VECTORVIEWPORT },
   { "Focus",        FDF_OBJECT|FDF_RI, NULL, NULL, CLASSID::VECTORVIEWPORT },
   { "View",         FDF_OBJECT|FDF_R, NULL, NULL, CLASSID::VECTORVIEWPORT },
   { "Page",         FDF_OBJECT|FDF_R, NULL, NULL, CLASSID::VECTORVIEWPORT },
   { "TabFocus",     FDF_OBJECTID|FDF_RW },
   { "EventMask",    FDF_INTFLAGS|FDF_FLAGS|FDF_RW, NULL, NULL, &clDocumentEventMask },
   { "Flags",        FDF_INTFLAGS|FDF_RI, NULL, SET_Flags, &clDocumentFlags },
   { "PageHeight",   FDF_INT|FDF_R },
   { "Error",        FDF_INT|FDF_R },
   // Virtual fields
   { "ClientScript",  FDF_OBJECT|FDF_I,        NULL, SET_ClientScript },
   { "EventCallback", FDF_FUNCTIONPTR|FDF_RW,  GET_EventCallback, SET_EventCallback },
   { "Path",          FDF_STRING|FDF_RW,       GET_Path, SET_Path },
   { "Origin",        FDF_STRING|FDF_RW,       GET_Path, SET_Origin },
   { "PageWidth",     FDF_UNIT|FDF_INT|FDF_SCALED|FDF_RW, GET_PageWidth, SET_PageWidth },
   { "Pretext",       FDF_STRING|FDF_W,        NULL, SET_Pretext },
   { "Src",           FDF_SYNONYM|FDF_STRING|FDF_RW, GET_Path, SET_Path },
   { "WorkingPath",   FDF_STRING|FDF_R,        GET_WorkingPath, NULL },
   END_FIELD
};
