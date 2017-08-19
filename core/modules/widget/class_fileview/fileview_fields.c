/*****************************************************************************

-FIELD-
Auto: Defines a default operation to use on activated file items.
Lookup: FVA

When the user activates a file item in the #View, you can choose to automatically execute a default operation
on that file.

-FIELD-
BytesFree: Indicates the number of free bytes on the file system being viewed.

This field reflects the total number of free bytes on the file system that the user is currently viewing.  It is set to
zero when the user is at the top level view.

*****************************************************************************/

static ERROR GET_BytesFree(objFileView *Self, LARGE *Value)
{
   if (Self->DeviceInfo) *Value = Self->DeviceInfo->BytesFree;
   else *Value = 0;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
BytesUsed: Indicates the number of used bytes on the file system being viewed.

This field reflects the total number of bytes used by the file system that the user is currently viewing.  It is set to
zero when the user is at the top level view.

*****************************************************************************/

static ERROR GET_BytesUsed(objFileView *Self, LARGE *Value)
{
   if (Self->DeviceInfo) *Value = Self->DeviceInfo->BytesUsed;
   else *Value = 0;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
ByteSize: Indicates the byte size of the file system being viewed.

This field reflects the total number of bytes available for storage in the file system that the user is currently
viewing.  It is set to zero when the user is at the top level view.

*****************************************************************************/

static ERROR GET_ByteSize(objFileView *Self, LARGE *Value)
{
   if (Self->DeviceInfo) *Value = Self->DeviceInfo->DeviceSize;
   else *Value = 0;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
EventCallback: Provides callbacks for global state changes.

Set this field with a function reference to receive event notifications.  It must be set in conjunction with
#EventMask so that you can select the type of notifications that will be received.

The callback function prototype is `Function(*FileView, LONG EventFlag)`.

The EventFlag value will indicate the event that occurred.  Please see the #EventMask field for a list of
supported events and additional details.

*****************************************************************************/

static ERROR GET_EventCallback(objFileView *Self, FUNCTION **Value)
{
   if (Self->EventCallback.Type != CALL_NONE) {
      *Value = &Self->EventCallback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_EventCallback(objFileView *Self, FUNCTION *Value)
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
Lookup: FEF

To receive event notifications, set EventCallback with a function reference and the EventMask field with a mask that
indicates the events that need to be received.

-FIELD-
Feedback: Provides instant feedback when a user interacts with the view.

Set the Feedback field with a callback function in order to receive instant feedback when user interaction occurs.  The
function prototype is `Function(*FileView)`.

*****************************************************************************/

static ERROR GET_Feedback(objFileView *Self, FUNCTION **Value)
{
   if (Self->Feedback.Type != CALL_NONE) {
      *Value = &Self->Feedback;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_Feedback(objFileView *Self, FUNCTION *Value)
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
Filter: Apply a file filter using wildcards.

To define a file filter, set this field using standard wild-card values.  Both the asterisk and question-mark
characters are accepted as wild-wards, while the OR operator is reserved for use in a future update.  Here are some
filter examples:

<types type="Filter">
<type name="*.fluid">Show Fluid files.</>
<type name="*.*">Show files with extensions.</>
<type name="???.fluid">Show Fluid files with three letter names.</>
<type name="?b*">Show files where 'b' is a second character.</>
<type name="a*b">Show files starting with a, ending in b.</>
</>

File filters are not case sensitive.

*****************************************************************************/

static ERROR GET_Filter(objFileView *Self, STRING *Value)
{
   if ((*Value = Self->Filter)) return ERR_Okay;
   else return ERR_NoData;
}

static ERROR SET_Filter(objFileView *Self, STRING Value)
{
   LogMsg("%s", Value);

   if (!Value) Self->Filter[0] = 0;
   else {
      LONG i;
      for (i=0; (i < sizeof(Self->Filter)-1) AND (Value[i]); i++) Self->Filter[i] = Value[i];
      Self->Filter[i] = 0;
   }

   if (Self->Head.Flags & NF_INITIALISED) {
      Self->Flags |= FVF_TOTAL_REFRESH;
      acRefresh(Self);
   }

   LogBack();
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Flags:  Optional flags are defined here.
Lookup: FVF

Optional flags can be defined here for object initialisation.

-FIELD-
Focus: Defines the surface to monitor for key presses.

The surface that the fileview monitors for key presses can be defined in this field.  If not defined on initialisation,
the surface ID that is referenced in the #View is automatically copied to this field.

-FIELD-
Path: The path of the current folder presented in the view.

If you want a fileview to analyse a specific directory, writing to this field will force the object to switch to the
new location and refresh the file list.

To change to the root directory, set the Path to a zero length string, or write the field with a NULL pointer.
-END-

*****************************************************************************/

static ERROR GET_Path(objFileView *Self, STRING *Value)
{
   if ((*Value = Self->Path)) return ERR_Okay;
   else return ERR_NoData;
}

static ERROR SET_Path(objFileView *Self, CSTRING Value)
{
   LONG i, len;

   // Check if the new string is any different to the current location

   if ((!Self->Path[0]) OR (Self->Path[0] IS ':')) {
      if ((!Value) OR (!Value[0]) OR (Value[0] IS ':')) return ERR_Okay;
   }

   if (!StrMatch(Value, Self->Path)) return ERR_Okay;

   // Set the new location string

   if ((!Value) OR (!Value[0])) {
      Self->Path[0] = 0;
   }
   else {
      // Guarantee that the location ends with a folder marker

      for (len=0; Value[len]; len++);
      while ((len > 0) AND (Value[len-1] != '/') AND (Value[len-1] != '\\') AND (Value[len-1] != ':')) len--;

      for (i=0; (i < len) AND (i < sizeof(Self->Path)-1); i++) Self->Path[i] = Value[i];
      Self->Path[i] = 0;
   }

   // If a root-path has been set, ensure that the first part of the location refers to that path.  If not, reset the
   // location to the root path.

   if (Self->RootPath) {
      if (StrCompare(Self->RootPath, Self->Path, StrLength(Self->RootPath), 0) != ERR_Okay) {
         StrCopy(Self->RootPath, Self->Path, sizeof(Self->Path));
      }
   }

   // If the object is initialised, update the location string and switch to the new directory.

   if (Self->Dir) { CloseDir(Self->Dir); Self->Dir = NULL; }

   if (Self->Watch) { acFree(Self->Watch); Self->Watch = NULL; }

   LogBranch("Path: '%s'", Self->Path);

   if (Self->Head.Flags & NF_INITIALISED) {
      Self->Flags |= FVF_TOTAL_REFRESH;

      check_docview(Self);

      if (acRefresh(Self) != ERR_Okay) {
         LogBack();
         return ERR_Refresh;
      }

      // Monitor the new location for file changes.  Note that not all platforms support this feature.

      if ((!Self->Path[0]) OR (Self->Path[0] IS ':')) {
         // From the root view we will listen to events from the Assign system, so no timer or file monitor is needed.
         if (Self->Timer) { UNSUB_TIMER(Self->Timer); Self->Timer = 0; }
      }
      else {
         ERROR error;
         if (!(error = CreateObject(ID_FILE, NF_INTEGRAL, &Self->Watch,
               FID_Path|TSTR, Self->Path,
               TAGEND))) {

            struct rkFunction callback;
            SET_FUNCTION_STDC(callback, &path_watch);

            if ((error = flWatch(Self->Watch, &callback, 0, MFF_CREATE|MFF_DELETE|MFF_ATTRIB|MFF_CLOSED|MFF_MOVED))) {
               acFree(Self->Watch);
               Self->Watch = NULL;
            }
         }

         if ((!error) AND (Self->Timer)) { UNSUB_TIMER(Self->Timer); Self->Timer = 0; } // Disable timer if watch initialisation was successful
         else SUB_TIMER(Self->RefreshRate * 1000, &Self->Timer);
      }
   }

   report_event(Self, FEF_LOCATION);

   LogBack();
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
RefreshRate: Defines the rate of refresh, in seconds.

The frequency at which the file view will check the file system for changes can be modified through this field.  It is
relevant only when automated reporting of file system changes is unavailable.  The refresh rate interval is measured in
seconds.

As the default refresh rate can be defined by the user, we recommended that you do not set this field under normal
conditions.

-FIELD-
RootPath: Sets a custom root path for the file view.

The root path for the file view can be defined here.  By default the root path is undefined, which ensures that the
user has access to all drives and file systems.  By setting this field to a valid folder location, the user will be
restricted to viewing that folder and all content within it.

*****************************************************************************/

static ERROR SET_RootPath(objFileView *Self, CSTRING Value)
{
   if (Self->RootPath) { FreeMemory(Self->RootPath); Self->RootPath = NULL; }
   if ((Value) AND (*Value) AND (*Value != ':')) Self->RootPath = StrClone(Value);
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Selection: Readable field that indicates the name of the currently selected item.

Read the Selection field to retrieve a string for the currently selected file or directory.  If no selection is active
then this function will return a value of NULL.  The path leading to the file <i>will not</i> be included in the
resulting string.  If the current selection is a directory or volume, any trailing symbols will be stripped from
the end of the directory name.

The Selection is returned exactly as it appears to the user - so if you have opted to strip all extensions from file
names, you will not get the 'exact' filename as it is described on the filesystem.

*****************************************************************************/

static ERROR GET_Selection(objFileView *Self, STRING *Value)
{
   *Value = NULL;

   struct XMLTag **tags;
   LONG tagindex;
   if ((!GetFields(Self->View, FID_SelectedTag|TLONG, &tagindex,
                               FID_Tags|TPTR,         &tags,
                               TAGEND)) AND (tagindex != -1)) {

      if (tags[tagindex]->Child) StrCopy(tags[tagindex]->Child->Attrib->Value, Self->Selection, sizeof(Self->Selection));
      else StrCopy(extract_filename(tags[tagindex]), Self->Selection, sizeof(Self->Selection));

      // Strip any trailing directory symbols

      WORD i;
      for (i=0; Self->Selection[i]; i++);
      while ((i > 0) AND ((Self->Selection[i-1] IS ':') OR (Self->Selection[i-1] IS '/'))) i--;
      Self->Selection[i] = 0;

      *Value = Self->Selection;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
SelectionFile: The currently selected file or directory (fully-qualified).

Read the SelectionFile field for the fully-qualified name of the currently selected file or directory.  If no selection
is active then this function will return a value of NULL.  The path leading to the file <i>will not</i> be included in
the resulting string.  If the current selection is a directory or volume, the correct trailing symbol will be
appended to the end of the string to indicate the file type.

*****************************************************************************/

static ERROR GET_SelectionFile(objFileView *Self, STRING *Value)
{
   *Value = NULL;

   objXML *xml;
   if ((xml = Self->View->XML)) {
      LONG tagindex;
      if ((!GetLong(Self->View, FID_SelectedTag, &tagindex)) AND (tagindex != -1)) {
         StrCopy(extract_filename(xml->Tags[tagindex]), Self->Selection, sizeof(Self->Selection));
         *Value = Self->Selection;
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
SelectionPath: The complete file-path of the current selection.

Read the SelectionFile field for the path of the currently selected file or directory.  If no selection is active then
this function will return a value of NULL.  The path leading to the file will be included in the resulting string.  The
path is fully-qualified, so a trailing slash or colon will be present in the event that the selection is a directory or
volume.

The resulting string will remain valid until the fileview object is freed or the SelectionPath is used a consecutive
time.

*****************************************************************************/

static ERROR GET_SelectionPath(objFileView *Self, STRING *Value)
{
   *Value = NULL;

   LONG tagindex;

   if ((!GetLong(Self->View, FID_SelectedTag, &tagindex)) AND (tagindex != -1)) {
      UBYTE buffer[400];
      StrFormat(buffer, sizeof(buffer), "%s%s", Self->Path, extract_filename(Self->View->XML->Tags[tagindex]));

      OBJECTPTR context = SetContext(Self);

         if (Self->SelectionPath) { FreeMemory(Self->SelectionPath); Self->SelectionPath = NULL; }
         Self->SelectionPath = StrClone(buffer);

      SetContext(context);

      if (Self->SelectionPath) {
         *Value = Self->SelectionPath;
         return ERR_Okay;
      }
      else return PostError(ERR_AllocMemory);
   }
   else return ERR_NoData;
}

/*****************************************************************************

-FIELD-
ShowDocs: Allows automated document viewing when set to TRUE.

If set to TRUE (the default), the file view will allow for the automatic display of documents within the
#View.  Documents can be displayed when the user views a file path that has been associated with a document
(the ~Core.SetDocView() function manages this behaviour).

This option is managed as a user preference - we recommend that you avoid setting it manually.

-FIELD-
ShowHidden: Shows hidden files if set to TRUE.

-FIELD-
ShowSystem: Shows system files if set to TRUE.

-FIELD-
View: Must refer to a @View object that will represent the content of the fileview.

*****************************************************************************/

static ERROR SET_View(objFileView *Self, OBJECTPTR Value)
{
   if (Value) {
      if (Value->ClassID IS ID_VIEW) {
         Self->View = (objView *)Value;
      }
      else return PostError(ERR_WrongClass);
   }
   else Self->View = NULL;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Window: The window related to the file view is referenced here.

This field is automatically set on initialisation.  It refers to the window object that contains the fileview object.
If the fileview is not contained by a window then this field will be set to zero.
-END-

*****************************************************************************/
