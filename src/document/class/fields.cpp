
/*****************************************************************************

-FIELD-
Author: The author(s) of the document.

If a document declares the names of its author(s) under a head tag, the author string will be readable from this field.
This field is always NULL if a document does not declare an author string.

*****************************************************************************/

static ERROR SET_Author(objDocument *Self, CSTRING Value)
{
   if (Self->Author) { FreeResource(Self->Author); Self->Author = NULL; }
   if ((Value) and (*Value)) Self->Author = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Background: Optional background colour for the document.

Set the Background field to clear the document background to the colour specified.

-FIELD-
Border: Border colour around the document's surface.

This field enables the drawing of a 1-pixel border around the document's surface.  The edges that are drawn are
controlled by the #BorderEdge field.

-FIELD-
BorderEdge: Border edge flags.

This field controls the border edge that is drawn around the document's surface.  The colour of the border is defined
in the #Border field.

-FIELD-
BottomMargin: Defines the amount of whitespace to leave at the bottom of the document page.

The BottomMargin value determines the amount of whitespace at the bottom of the page.  The default margin can be
altered prior to initialisation of a document object, however the loaded content may declare its own margins and
overwrite this value during processing.

This value can be set as a fixed pixel coordinate only.

-FIELD-
Copyright: Copyright information for the document.

If a document declares copyright information under a head tag, the copyright string will be readable from this field.
This field is always NULL if a document does not declare a copyright string.

*****************************************************************************/

static ERROR SET_Copyright(objDocument *Self, CSTRING Value)
{
   if (Self->Copyright) { FreeResource(Self->Copyright); Self->Copyright = NULL; }
   if ((Value) and (*Value)) Self->Copyright = StrClone(Value);
   return ERR_Okay;
}

/****************************************************************************

-FIELD-
CursorColour: The colour used for the document cursor.

The colour used for the document cursor may be changed by setting this field.  This is relevant only when a document is
in edit mode.

-FIELD-
DefaultScript: Allows an external script object to be used by a document file.

Setting the DefaultScript field with a reference to a Script object will allow a document file to have access to
functionality outside of its namespace.  This feature is primarily intended for applications that need to embed
custom documents.

If a loaded document defines its own custom script, it will have priority over the script referenced here.

*****************************************************************************/

static ERROR SET_DefaultScript(objDocument *Self, OBJECTPTR Value)
{
   Self->UserDefaultScript = Value;
   return ERR_Okay;
}

/****************************************************************************

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

*****************************************************************************/

static ERROR GET_EventCallback(objDocument *Self, FUNCTION **Value)
{
   if (Self->EventCallback.Type != CALL_NONE) {
      *Value = &Self->EventCallback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_EventCallback(objDocument *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->EventCallback.Type IS CALL_SCRIPT) UnsubscribeAction(Self->EventCallback.Script.Script, AC_Free);
      Self->EventCallback = *Value;
      if (Self->EventCallback.Type IS CALL_SCRIPT) SubscribeAction(Self->EventCallback.Script.Script, AC_Free);
   }
   else Self->EventCallback.Type = CALL_NONE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
EventMask: Specifies events that need to be reported from the Document object.

To receive event notifications, set #EventCallback with a function reference and the EventMask field with a mask that
indicates the events that need to be received.

<types lookup="DEF"/>

-FIELD-
Flags: Optional flags that affect object behaviour.

<types lookup="DCF"/>

****************************************************************************/

static ERROR SET_Flags(objDocument *Self, LONG Value)
{
   if (Self->Head::Flags & NF_INITIALISED) {
      Self->Flags = Value & (~(DCF_NO_SCROLLBARS|DCF_UNRESTRICTED|DCF_DISABLED));
   }
   else Self->Flags = Value & (~(DCF_DISABLED));
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Focus: Refers to the object that will be monitored for user focusing.

By default, a document object will become active (i.e. capable of receiving keyboard input) when its surface container
receives the focus.  If you would like to change this so that a document becomes active when some other object receives
the focus, refer to that object by writing its ID to this field.

-FIELD-
FontColour: Default font colour.

This field defines the default font colour if the source document does not specify one.

-FIELD-
FontFace: Defines the default font face.

The default font face to use when processing a document is defined in this field.  A document may override the default
font face by declaring a body tag containing a face attribute.  If this occurs, the FontFace field will reflect the
default font face chosen by that document.

*****************************************************************************/

static ERROR SET_FontFace(objDocument *Self, CSTRING Value)
{
   if (Self->FontFace) FreeResource(Self->FontFace);
   if (Value) {
      Self->FontFace = StrClone(Value);

      // Check for facename:point usage

      auto str = Self->FontFace;
      while (*str) {
         if (*str IS ':') {
            *str = 0;
            Self->FontSize = StrToInt(str + 1);
            break;
         }
         str++;
      }
   }
   else Self->FontFace = NULL;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
FontSize: The point-size of the default font.

The point size of the default font is defined here.  Valid values range between 6 and 128.

*****************************************************************************/

static ERROR SET_FontSize(objDocument *Self, LONG Value)
{
   if (Value < 6) Self->FontSize = 6;
   else if (Value > 128) Self->FontSize = 128;
   else Self->FontSize = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Highlight: Defines the colour used to highlight document.

The Highlight field determines the colour that is used when highlighting selected document areas.

-FIELD-
Keywords: Includes keywords declared by the source document.

If a document declares keywords under a head tag, the keywords string will be readable from this field.   This field is
always NULL if a document does not declare any keywords.  It is recommended that keywords are separated with spaces or
commas.  It should not be assumed that the author of the document has adhered to the accepted standard for keyword
separation.

*****************************************************************************/

static ERROR SET_Keywords(objDocument *Self, STRING Value)
{
   if (Self->Keywords) FreeResource(Self->Keywords);
   if ((Value) and (*Value)) Self->Keywords = StrClone(Value);
   else Self->Keywords = NULL;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
LeftMargin: Defines the amount of whitespace to leave at the left of the page.

The LeftMargin value determines the amount of whitespace at the left of the page.  The default margin can be altered
prior to initialisation of a document object, however the loaded content may declare its own margins and overwrite this
value during processing.

This value can be set as a fixed pixel coordinate only.

-FIELD-
LineHeight: Default line height (taken as an average) for all text on the page.

-FIELD-
LinkColour: Default font colour for hyperlinks.

The default font colour for hyperlinks is defined here.  If the alpha component is zero, this feature is disabled.

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

*****************************************************************************/

static ERROR GET_Path(objDocument *Self, STRING *Value)
{
   if (Self->Path) {
      *Value = Self->Path;
      return ERR_Okay;
   }
   else {
      *Value = NULL;
      return ERR_FieldNotSet;
   }
}

static ERROR SET_Path(objDocument *Self, CSTRING Value)
{
   parasol::Log log;
   static BYTE recursion = 0;
   LONG i, len;

   if (recursion) return log.warning(ERR_Recursion);

   if ((!Value) or (!*Value)) return ERR_NoData;

   Self->Error = ERR_Okay;

   STRING newpath = NULL;
   UBYTE reload = TRUE;
   if ((Value[0] IS '#') or (Value[0] IS '?')) {
      reload = FALSE;

      if (Self->Path) {
         if (Value[0] IS '?') for (i=0; (Self->Path[i]) and (Self->Path[i] != '?'); i++);
         else for (i=0; (Self->Path[i]) and (Self->Path[i] != '#'); i++);

         len = StrLength(Value);

         if (!AllocMemory(i + len + 1, MEM_STRING|MEM_NO_CLEAR, &newpath, NULL)) {
            CopyMemory(Self->Path, newpath, i);
            CopyMemory(Value, newpath+i, len+1);
         }
         else return ERR_AllocMemory;
      }
      else newpath = StrClone(Value);
   }
   else if (Self->Path) {
      // Work out if the location has actually been changed

      for (len=0; (Value[len]) and (Value[len] != '#') and (Value[len] != '?'); len++);

      for (i=0; (Self->Path[i]) and (Self->Path[i] != '#') and (Self->Path[i] != '?'); i++);

      if ((i IS len) and ((!i) or (!StrCompare(Value, Self->Path, len, 0)))) {
         // The location remains unchanged.  A complete reload shouldn't be necessary.

         reload = FALSE;
         //if ((Self->Path[i] IS '?') or (Value[len] IS '?')) {
         //   reload = TRUE;
         //}
      }

      newpath = StrClone(Value);
   }
   else newpath = StrClone(Value);

   log.branch("%s (vs %s) Reload: %d", newpath, Self->Path, reload);

   // Signal that we are leaving the current page

   recursion++;
   for (auto trigger=Self->Triggers[DRT_LEAVING_PAGE]; trigger; trigger=trigger->Next) {
      if (trigger->Function.Type IS CALL_SCRIPT) {
         OBJECTPTR script;
         if ((script = trigger->Function.Script.Script)) {
            ScriptArg args[] = {
               { "OldURI", FD_STR, { .Address = Self->Path } },
               { "NewURI", FD_STR, { .Address = newpath } },
            };
            scCallback(script, trigger->Function.Script.ProcedureID, args, ARRAYSIZE(args), NULL);
         }
      }
      else if (trigger->Function.Type IS CALL_STDC) {
         auto routine = (void (*)(APTR, objDocument *, STRING, STRING))trigger->Function.StdC.Routine;
         if (routine) {
            parasol::SwitchContext context(trigger->Function.StdC.Context);
            routine(trigger->Function.StdC.Context, Self, Self->Path, newpath);
         }
      }
   }
   recursion--;

   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }
   if (Self->PageName) { FreeResource(Self->PageName); Self->PageName = NULL; }
   if (Self->Bookmark) { FreeResource(Self->Bookmark); Self->Bookmark = NULL; }

   if (newpath) {
      Self->Path = newpath;

      recursion++;
      unload_doc(Self, (reload IS FALSE) ? ULD_REFRESH : 0);

      if (Self->Head::Flags & NF_INITIALISED) {
         if ((Self->XML) and (reload IS FALSE)) {
            process_parameters(Self, Self->Path);
            process_page(Self, Self->XML);
         }
         else {
            load_doc(Self, Self->Path, FALSE, 0);
            DelayMsg(MT_DrwInvalidateRegion, Self->SurfaceID, NULL);
         }
      }
      recursion--;

      // If an error occurred, remove the location & page strings to show that no document is loaded.

      if (Self->Error) {
         FreeResource(Self->Path);
         Self->Path = NULL;

         if (Self->PageName) { FreeResource(Self->PageName); Self->PageName = NULL; }
         if (Self->Bookmark) { FreeResource(Self->Bookmark); Self->Bookmark = NULL; }
         if (Self->XML) { acFree(Self->XML); Self->XML = NULL; }

         DelayMsg(MT_DrwInvalidateRegion, Self->SurfaceID, NULL);
      }
   }
   else Self->Error = ERR_AllocMemory;

   report_event(Self, DEF_PATH, NULL, NULL);

   return Self->Error;
}

/****************************************************************************

-FIELD-
Origin: Similar to the Path field, but does not automatically load content if set.

This field is identical to the #Path field, with the exception that it does not update the content of a
document object if it is set after initialisation.  This may be useful if the location of a loaded document needs to be
changed without causing a load operation.

****************************************************************************/

static ERROR SET_Origin(objDocument *Self, STRING Value)
{
   if (Self->Path) { FreeResource(Self->Path); Self->Path = NULL; }

   if ((Value) and (*Value)) {
      LONG i;
      for (i=0; Value[i]; i++);
      if (!AllocMemory(i+1, MEM_STRING|MEM_NO_CLEAR, &Self->Path, NULL)) {
         for (i=0; Value[i]; i++) Self->Path[i] = Value[i];
         Self->Path[i] = 0;
      }
      else return ERR_AllocMemory;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
PageHeight: Measures the page height of the document, in pixels.

The exact height of the document is indicated in the PageHeight field.  This value includes the top and bottom page
margins.

-FIELD-
PageWidth: Measures the page width of the document, in pixels.

The width of the longest document line can be retrieved from this field.  The result includes the left and right page
margins.

*****************************************************************************/

static ERROR GET_PageWidth(objDocument *Self, Variable *Value)
{
   DOUBLE value;

   // Reading the PageWidth returns the pixel width of the page after parsing.

   if (Self->Head::Flags & NF_INITIALISED) {
      value = Self->CalcWidth;

      if (Value->Type & FD_PERCENTAGE) {
         if (Self->SurfaceWidth <= 0) return ERR_GetField;
         value = (DOUBLE)(value * Self->SurfaceWidth) * (1.0 / 100.0);
      }
   }
   else value = Self->PageWidth;

   if (Value->Type & FD_DOUBLE) Value->Double = value;
   else if (Value->Type & FD_LARGE) Value->Large = value;
   else return ERR_FieldTypeMismatch;
   return ERR_Okay;
}

static ERROR SET_PageWidth(objDocument *Self, Variable *Value)
{
   parasol::Log log;

   if (Value->Type & FD_DOUBLE) {
      if (Value->Double <= 0) {
         log.warning("A page width of %.2f is illegal.", Value->Double);
         return ERR_OutOfRange;
      }
      Self->PageWidth = Value->Double;
   }
   else if (Value->Type & FD_LARGE) {
      if (Value->Large <= 0) {
         log.warning("A page width of " PF64() " is illegal.", Value->Large);
         return ERR_OutOfRange;
      }
      Self->PageWidth = Value->Large;
   }
   else return ERR_FieldTypeMismatch;

   if (Value->Type & FD_PERCENTAGE) Self->RelPageWidth = TRUE;
   else Self->RelPageWidth = FALSE;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
RightMargin: Defines the amount of white-space to leave at the right side of the document page.

The RightMargin value determines the amount of white-space at the right of the page.  The default margin can be altered
prior to initialisation of a document object, however the loaded content may declare its own margins and overwrite this
value during processing.

This value can be set as a fixed pixel coordinate only.

-FIELD-
SelectColour: Default font colour to use when hyperlinks are selected.

This field defines the font colour for hyperlinks that are selected - for instance, when the user tabs to a link or
hovers over it.  If the alpha component is zero, this field has no effect.

-FIELD-
Surface: Defines the surface area for document graphics.

The Surface field refers to the object ID of the surface that will contain the document graphics.  This field must be
set prior to initialisation to target the graphics correctly - if left unset then the document object will attempt to
determine the correct surface object based on object ownership.

*****************************************************************************/

static ERROR SET_Surface(objDocument *Self, OBJECTID Value)
{
   if (Self->Head::Flags & NF_INITIALISED) {
      if (Self->SurfaceID IS Value) return ERR_Okay;
      return ERR_NoSupport;
   }
   else Self->SurfaceID = Value;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
TabFocus: Allows the user to hit the tab key to focus on other GUI objects.

If this field points to a TabFocus object, the user will be able to move between objects that are members of the
TabFocus by pressing the tab key.  Please refer to the TabFocus class for more details.

-FIELD-
Title: The title of the document.

If a document declares a title under a head tag, the title string will be readable from this field.   This field is
always NULL if a document does not declare a title.

*****************************************************************************/

static ERROR SET_Title(objDocument *Self, STRING Value)
{
   if (Self->Title) { FreeResource(Self->Title); Self->Title = NULL; }
   if ((Value) and (*Value)) Self->Title = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
TopMargin: Defines the amount of white-space to leave at the top of the document page.

The TopMargin value determines the amount of white-space at the top of the page.  The default margin can be altered
prior to initialisation of a document object, however the loaded content may declare its own margins and overwrite this
value during processing.

This value can be set as a fixed pixel coordinate only.

-FIELD-
UpdateLayout: When TRUE, forces the layout to update on the next redraw.

To force the document layout to be updated on the next redraw, set this field to TRUE. Redrawing can then be achieved
by calling the #Draw() action on the document.

Forcing the document to recompute its layout is rarely necessary as this is automatically managed when inserting and
removing content.  However, an action such as adjusting the size of graphical objects from a script would require this
field to be manually set.

*****************************************************************************/

static ERROR SET_UpdateLayout(objDocument *Self, LONG Value)
{
   if (Value) Self->UpdateLayout = TRUE;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
VLinkColour: Default font colour for visited hyperlinks.

The default font colour for visited hyperlinks is stored in this field.  The source document can specify its own
colour for visited links if the author desires.
-END-

*****************************************************************************/

/*****************************************************************************
PRIVATE: Variables
*****************************************************************************/

static ERROR GET_Variables(objDocument *Self, KeyStore **Value)
{
   *Value = Self->Vars;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
WorkingPath: Defines the working path (folder or URI).

The working path for a document is defined here.  By default this is defined as the location from which the document
was loaded, without the file name.  If this cannot be determined then the working path for the parent task is used
(this is usually set to the location of the parasol-gui program).

The working path is always fully qualified with a slash or colon at the end of the string unless the path cannot be
determined - in which case an empty string is returned.

You can manually change the working path by setting the #Origin field without affecting the loaded document.
-END-

*****************************************************************************/

static ERROR GET_WorkingPath(objDocument *Self, CSTRING *Value)
{
   parasol::Log log;

   if (!Self->Path) {
      log.warning("Document has no defined Path.");
      return ERR_FieldNotSet;
   }

   if (Self->WorkingPath) { FreeResource(Self->WorkingPath); Self->WorkingPath = NULL; }

   // Determine if an absolute path has been indicated

   BYTE path = FALSE;
   if (Self->Path[0] IS '/') path = TRUE;
   else {
     for (LONG j=0; (Self->Path[j]) and (Self->Path[j] != '/') and (Self->Path[j] != '\\'); j++) {
         if (Self->Path[j] IS ':') {
            path = TRUE;
            break;
         }
      }
   }

   LONG j = 0;
   for (LONG k=0; Self->Path[k]; k++) {
      if ((Self->Path[k] IS ':') or (Self->Path[k] IS '/') or (Self->Path[k] IS '\\')) j = k+1;
   }

   parasol::SwitchContext context(Self);

   STRING workingpath;
   if (path) { // Extract absolute path
      auto save = Self->Path[j];
      Self->Path[j] = 0;
      Self->WorkingPath = StrClone(Self->Path);
      Self->Path[j] = save;
   }
   else if ((!GetString(CurrentTask(), FID_Path, &workingpath)) and (workingpath)) {
      char buf[1024];

      // Using ResolvePath() can help to determine relative paths such as "../path/file"

      if (j > 0) {
         auto save = Self->Path[j];
         Self->Path[j] = 0;
         StrFormat(buf, sizeof(buf), "%s%s", workingpath, Self->Path);
         Self->Path[j] = save;
      }
      else StrFormat(buf, sizeof(buf), "%s", workingpath);

      if (ResolvePath(buf, RSF_APPROXIMATE, &Self->WorkingPath) != ERR_Okay) {
         Self->WorkingPath = StrClone(workingpath);
      }
   }
   else log.warning("No working path.");

   *Value = Self->WorkingPath;
   return ERR_Okay;
}
