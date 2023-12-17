
/*********************************************************************************************************************

-FIELD-
Author: The author(s) of the document.

If a document declares the names of its author(s) under a head tag, the author string will be readable from this field.
This field is always NULL if a document does not declare an author string.

-FIELD-
Background: Optional background colour for the document.

Set the Background field to clear the document background to the colour specified.

*********************************************************************************************************************/

static ERROR SET_Background(extDocument *Self, CSTRING Value)
{
   if (Self->Background) { FreeResource(Self->Background); Self->Background = NULL; }
   if ((Value) and (*Value)) Self->Background = StrClone(Value);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
BorderStroke: The stroke to use for drawing a border around the document window.

This field enables the drawing of a stroke along the border of the document window.
The edges are controlled by the #BorderEdge field.

*********************************************************************************************************************/

static ERROR SET_BorderStroke(extDocument *Self, CSTRING Value)
{
   if (Self->BorderStroke) { FreeResource(Self->BorderStroke); Self->BorderStroke = NULL; }
   if ((Value) and (*Value)) Self->BorderStroke = StrClone(Value);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
BorderEdge: Border edge flags.

This field controls the border edge that is drawn around the document's surface.  The colour of the border is defined
in the #BorderStroke field.

-FIELD-
Copyright: Copyright information for the document.

If a document declares copyright information under a head tag, the copyright string will be readable from this field.
This field is always NULL if a document does not declare a copyright string.

-FIELD-
CursorStroke: The colour or brush stroke to use for the document cursor.

The colour or brush stroke used for the document cursor may be changed by setting this field.  Formatting is
equivalent to the SVG stroke property.  This is relevant only when a document is in edit mode.

*********************************************************************************************************************/

static ERROR SET_CursorStroke(extDocument *Self, CSTRING Value)
{
   if (Self->CursorStroke) { FreeResource(Self->CursorStroke); Self->CursorStroke = NULL; }
   if ((Value) and (*Value)) Self->CursorStroke = StrClone(Value);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
DefaultScript: Allows an external script object to be used by a document file.

Setting the DefaultScript field with a reference to a Script object will allow a document file to have access to
functionality outside of its namespace.  This feature is primarily intended for applications that need to embed
custom documents.

If a loaded document defines its own custom script, it will have priority over the script referenced here.

*********************************************************************************************************************/

static ERROR SET_DefaultScript(extDocument *Self, OBJECTPTR Value)
{
   Self->UserDefaultScript = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Description: A description of the document, provided by its author.

If the source document includes a description, it will be copied to this field.

-FIELD-
Error: The most recently generated error code.

The most recently generated error code is stored in this field.

-FIELD-
EventCallback: Provides callbacks for global state changes.

Set this field with a function reference to receive event notifications.  It must be set in conjunction with
#EventMask so that notifications are limited to those of interest.

The callback function prototype is `ERROR Function(*Document, LARGE EventFlag)`.

The EventFlag value will indicate the event that occurred.  Please see the #EventMask field for a list of
supported events and additional details.

Error codes returned from the callback will normally be discarded, however in some cases ERR_Skip can be returned in
order to prevent the event from being processed any further.

*********************************************************************************************************************/

static ERROR GET_EventCallback(extDocument *Self, FUNCTION **Value)
{
   if (Self->EventCallback.Type != CALL_NONE) {
      *Value = &Self->EventCallback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_EventCallback(extDocument *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->EventCallback.Type IS CALL_SCRIPT) UnsubscribeAction(Self->EventCallback.Script.Script, AC_Free);
      Self->EventCallback = *Value;
      if (Self->EventCallback.Type IS CALL_SCRIPT) {
         auto callback = make_function_stdc(notify_free_event);
         SubscribeAction(Self->EventCallback.Script.Script, AC_Free, &callback);
      }
   }
   else Self->EventCallback.Type = CALL_NONE;
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
EventMask: Specifies events that need to be reported from the Document object.

To receive event notifications, set #EventCallback with a function reference and the EventMask field with a mask that
indicates the events that need to be received.

<types lookup="DEF"/>

-FIELD-
Flags: Optional flags that affect object behaviour.

<types lookup="DCF"/>

*********************************************************************************************************************/

static ERROR SET_Flags(extDocument *Self, DCF Value)
{
   if (Self->initialised()) {
      Self->Flags = Value & (~(DCF::NO_SCROLLBARS|DCF::UNRESTRICTED|DCF::DISABLED));
   }
   else Self->Flags = Value & (~(DCF::DISABLED));
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Focus: Refers to the object that will be monitored for user focusing.

By default, a document object will become active (i.e. capable of receiving keyboard input) when its surface container
receives the focus.  If you would like to change this so that a document becomes active when some other object receives
the focus, refer to that object by writing its ID to this field.

-FIELD-
Keywords: Includes keywords declared by the source document.

If a document declares keywords under a head tag, the keywords string will be readable from this field.   This field is
always NULL if a document does not declare any keywords.  It is recommended that keywords are separated with spaces or
commas.  It should not be assumed that the author of the document has adhered to the accepted standard for keyword
separation.

-FIELD-
LineHeight: Default line height (taken as an average) for all text on the page.

-FIELD-
Path: Identifies the location of a document file to load.

To load a document file into a document object, set the Path field.  If this field is set after initialisation, the
object will automatically clear its content and reload data from the location that you specify.  It is also possible to
change the current page and parameters by setting the Path.

The string format for setting the path is `volume:folder/filename.rpl#Page?param1&param2=value`.

This example changes the current document by loading from a new file source: `documents:index.rpl`.

This example changes the current page if a document is already loaded (note: if the page does not exist in the
currently loaded document, a message is displayed to bring the error to the user's attention): `#introduction`.

This example changes the page and passes it new parameters: `#introduction?username=Paul`.

To leap to a bookmark in the page that has been specified with the &lt;index&gt; element, use the colon as a separator
after the pagename, i.e. `#pagename:bookmark`.

Other means of opening a document include loading the data manually and feeding it through with the #DataFeed() action.

The new document layout will be displayed when incoming messages are next processed by the running task.
-END-

*********************************************************************************************************************/

static ERROR GET_Path(extDocument *Self, CSTRING *Value)
{
   *Value = Self->Path.c_str();
   return ERR_Okay;
}

static ERROR SET_Path(extDocument *Self, CSTRING Value)
{
   pf::Log log;
   static BYTE recursion = 0;

   if (recursion) return log.warning(ERR_Recursion);

   if ((!Value) or (!*Value)) return ERR_NoData;

   Self->Error = ERR_Okay;

   std::string newpath;
   bool reload = true;
   if ((Value[0] IS '#') or (Value[0] IS '?')) {
      reload = false;

      if (!Self->Path.empty()) {
         unsigned i;
         if (Value[0] IS '?') for (i=0; (i < Self->Path.size()) and (Self->Path[i] != '?'); i++);
         else for (i=0; (i < Self->Path.size()) and (Self->Path[i] != '#'); i++);

         newpath.assign(Self->Path, 0, i);
         newpath.append(Value);
      }
      else newpath.assign(Value);
   }
   else if (!Self->Path.empty()) {
      // Work out if the location has actually been changed

      unsigned len;
      for (len=0; (Value[len]) and (Value[len] != '#') and (Value[len] != '?'); len++);

      unsigned i;
      for (i=0; (i < Self->Path.size()) and (Self->Path[i] != '#') and (Self->Path[i] != '?'); i++);

      if ((i IS len) and ((!i) or (!StrCompare(Value, Self->Path, len)))) {
         // The location remains unchanged.  A complete reload shouldn't be necessary.

         reload = false;
         //if ((Self->Path[i] IS '?') or (Value[len] IS '?')) {
         //   reload = true;
         //}
      }

      newpath = Value;
   }
   else newpath = Value;

   log.branch("%s (vs %s) Reload: %d", newpath.c_str(), Self->Path.c_str(), reload);

   // Signal that we are leaving the current page

   recursion++;
   for (auto &trigger : Self->Triggers[LONG(DRT::LEAVING_PAGE)]) {
      if (trigger.Type IS CALL_SCRIPT) {
         auto script = trigger.Script.Script;
         ScriptArg args[] = {
            { "OldURI", Self->Path },
            { "NewURI", newpath },
         };
         scCallback(script, trigger.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
      }
      else if (trigger.Type IS CALL_STDC) {
         auto routine = (void (*)(APTR, extDocument *, CSTRING, CSTRING))trigger.StdC.Routine;
         pf::SwitchContext context(trigger.StdC.Context);
         routine(trigger.StdC.Context, Self, Self->Path.c_str(), newpath.c_str());
      }
   }
   recursion--;

   Self->Path.clear();
   Self->PageName.clear();
   Self->Bookmark.clear();

   if (!newpath.empty()) {
      Self->Path = newpath;

      recursion++;
      unload_doc(Self, (!reload) ? ULD::REFRESH : ULD::NIL);

      if (Self->initialised()) {
         if ((Self->XML) and (!reload)) {
            process_parameters(Self, Self->Path);
            parser parse(Self, Self->XML);
            parse.process_page();
         }
         else {
            load_doc(Self, Self->Path, false, ULD::NIL);
            Self->Viewport->draw();
         }
      }
      recursion--;

      // If an error occurred, remove the location & page strings to show that no document is loaded.

      if (Self->Error) {
         Self->Path.clear();
         Self->PageName.clear();
         Self->Bookmark.clear();
         if (Self->XML) { FreeResource(Self->XML); Self->XML = NULL; }

         Self->Viewport->draw();
      }
   }
   else Self->Error = ERR_AllocMemory;

   report_event(Self, DEF::PATH, NULL);

   return Self->Error;
}

/*********************************************************************************************************************

-FIELD-
Origin: Similar to the Path field, but does not automatically load content if set.

This field is identical to the #Path field, with the exception that it does not update the content of a
document object if it is set after initialisation.  This may be useful if the location of a loaded document needs to be
changed without causing a load operation.

*********************************************************************************************************************/

static ERROR SET_Origin(extDocument *Self, CSTRING Value)
{
   Self->Path.clear();
   if ((Value) and (*Value)) Self->Path.assign(Value);
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
PageHeight: Measures the page height of the document, in pixels.

The exact height of the document is indicated in the PageHeight field.  This value includes the top and bottom page
margins.

-FIELD-
PageWidth: Measures the page width of the document, in pixels.

The width of the longest document line can be retrieved from this field.  The result includes the left and right page
margins.

*********************************************************************************************************************/

static ERROR GET_PageWidth(extDocument *Self, Variable *Value)
{
   DOUBLE value;

   // Reading the PageWidth returns the pixel width of the page after parsing.

   if (Self->initialised()) {
      value = Self->CalcWidth;

      if (Value->Type & FD_PERCENTAGE) {
         if (Self->VPWidth <= 0) return ERR_GetField;
         value = (DOUBLE)(value * Self->VPWidth);
      }
   }
   else value = Self->PageWidth;

   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = value;
   else return ERR_FieldTypeMismatch;
   return ERR_Okay;
}

static ERROR SET_PageWidth(extDocument *Self, Variable *Value)
{
   pf::Log log;

   if (Value->Type & FD_DOUBLE) {
      if (Value->Double <= 0) {
         log.warning("A page width of %.2f is illegal.", Value->Double);
         return ERR_OutOfRange;
      }
      Self->PageWidth = Value->Double;
   }
   else if (Value->Type & FD_LARGE) {
      if (Value->Large <= 0) {
         log.warning("A page width of %" PF64 " is illegal.", Value->Large);
         return ERR_OutOfRange;
      }
      Self->PageWidth = Value->Large;
   }
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) Self->RelPageWidth = true;
   else Self->RelPageWidth = false;

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Viewport: A target viewport that will host the document graphics.

The Viewport field must refer to a @VectorViewport that will host the document graphics.  If not initialised,
the nearest viewport container will be determined based on object ownership.

*********************************************************************************************************************/

static ERROR SET_Viewport(extDocument *Self, objVectorViewport *Value)
{
   if (Value->CLASS_ID != ID_VECTORVIEWPORT) return ERR_InvalidObject;

   if (Self->initialised()) {
      if (Self->Viewport IS Value) return ERR_Okay;
      return ERR_NoSupport;
   }
   else Self->Viewport = Value;

   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
TabFocus: Allows the user to hit the tab key to focus on other GUI objects.

If this field points to a TabFocus object, the user will be able to move between objects that are members of the
TabFocus by pressing the tab key.  Please refer to the TabFocus class for more details.

-FIELD-
Title: The title of the document.

If a document declares a title under a head tag, the title string will be readable from this field.   This field is
always NULL if a document does not declare a title.

-FIELD-
WorkingPath: Defines the working path (folder or URI).

The working path for a document is defined here.  By default this is defined as the location from which the document
was loaded, without the file name.  If this cannot be determined then the working path for the parent task is used
(this is usually set to the location of the parasol-gui program).

The working path is always fully qualified with a slash or colon at the end of the string unless the path cannot be
determined - in which case an empty string is returned.

You can manually change the working path by setting the #Origin field without affecting the loaded document.
-END-

*********************************************************************************************************************/

static ERROR GET_WorkingPath(extDocument *Self, CSTRING *Value)
{
   pf::Log log;

   if (Self->Path.empty()) {
      log.warning("Document has no defined Path.");
      return ERR_FieldNotSet;
   }

   Self->WorkingPath.clear();

   // Determine if an absolute path has been indicated

   bool path = false;
   if (Self->Path[0] IS '/') path = true;
   else {
     for (LONG j=0; (Self->Path[j]) and (Self->Path[j] != '/') and (Self->Path[j] != '\\'); j++) {
         if (Self->Path[j] IS ':') {
            path = true;
            break;
         }
      }
   }

   LONG j = 0;
   for (LONG k=0; Self->Path[k]; k++) {
      if ((Self->Path[k] IS ':') or (Self->Path[k] IS '/') or (Self->Path[k] IS '\\')) j = k+1;
   }

   pf::SwitchContext context(Self);

   STRING workingpath;
   if (path) { // Extract absolute path
      Self->WorkingPath.assign(Self->Path, 0, j);
   }
   else if ((!CurrentTask()->get(FID_Path, &workingpath)) and (workingpath)) {
      std::string buf(workingpath);

      // Using ResolvePath() can help to determine relative paths such as "../path/file"

      if (j > 0) buf += Self->Path.substr(0, j);

      if (!ResolvePath(buf.c_str(), RSF::APPROXIMATE, &workingpath)) {
         Self->WorkingPath.assign(workingpath);
      }
   }
   else log.warning("No working path.");

   *Value = Self->WorkingPath.c_str();
   return ERR_Okay;
}
