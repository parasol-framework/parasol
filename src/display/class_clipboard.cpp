/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
Clipboard: The Clipboard class manages cut, copy and paste operations.

The Clipboard class manages data transfer between applications on behalf of the user.  It holds a data cache of clipped
items that originate from source applications, and these can be retrieved and 'pasted' into target applications.  The
Clipboard class is provided for the primary purpose of allowing applications to implement the traditional 'cut', 'copy'
and 'paste' actions.

Multiple clipboard objects can be created, but they all control the same group of clipped data for the logged-in user.
All items that are passed to the clipboard object are stored in the 'clipboard:' assignment, which defaults to
`temp:clipboard/`.

There is a limit on the amount of clipped items that can be stored in the clipboard.  Only 1 group of each datatype is
permitted (for example, only one group of image clips may exist at any time) and there is a preset limit on the total
number of clips that can be stored in the history cache.
-END-

*********************************************************************************************************************/

#include "defs.h"

#define MAX_CLIPS 10 // Maximum number of clips stored in the historical buffer

static const FieldDef glDatatypes[] = {
   { "data",   CLIPTYPE_DATA },
   { "audio",  CLIPTYPE_AUDIO },
   { "image",  CLIPTYPE_IMAGE },
   { "file",   CLIPTYPE_FILE },
   { "object", CLIPTYPE_OBJECT },
   { "text",   CLIPTYPE_TEXT },
   { NULL, 0 }
};

static LONG glCounter = 1;
std::vector<ClipEntry> glClips;

static std::string GetDatatype(LONG Datatype);

//********************************************************************************************************************

static ERROR add_clip(LONG, const std::vector<ClipItem> &, LONG = 0);
static ERROR CLIPBOARD_AddObjects(objClipboard *, struct clipAddObjects *);

//********************************************************************************************************************

ClipEntry::~ClipEntry() {
   pf::Log log(__FUNCTION__);

   if (Datatype != CLIPTYPE_FILE) {
      auto datatype = GetDatatype(Datatype);

      log.branch("Deleting %d clip files for datatype %s / %d.", LONG(Items.size()), datatype.c_str(), Datatype);

      // Delete cached clipboard files

      for (auto &item : Items) DeleteFile(item.Path.c_str(), NULL);
   }
   else log.branch("Datatype: File");
}

//********************************************************************************************************************

static std::string GetDatatype(LONG Datatype)
{
   for (unsigned i=0; glDatatypes[i].Name; i++) {
      if (Datatype IS glDatatypes[i].Value) return std::string(glDatatypes[i].Name);
   }

   return "unknown";
}

//********************************************************************************************************************

static void notify_script_free(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args)
{
   auto Self = (objClipboard *)CurrentContext();
   Self->RequestHandler.Type = CALL_NONE;
}

/*********************************************************************************************************************

-METHOD-
AddFile: Add files to the clipboard.

This method is used to add a file to the clipboard.  You are required to specify the type of data that is represented
by the file. This allows the file content to be pasted by other applications that understand the data.  Adding files
to the clipboard with a known datatype can be very efficient compared to other methods, as it saves loading the data
into memory until the user is ready to paste the content.

Recognised data types are:

<types lookup="CLIPTYPE"/>

Optional flags that may be passed to this method are as follows:

<types lookup="CEF"/>

-INPUT-
int(CLIPTYPE) Datatype: Set this argument to indicate the type of data you are copying to the clipboard.
cstr Path: The path of the file to add.
int(CEF) Flags: Optional flags.

-ERRORS-
Okay: The files were added to the clipboard.
Args
MissingLocation: The Files argument was not correctly specified.
LimitedSuccess: The file item was successfully added to the internal clipboard, but could not be added to the host.
-END-

*********************************************************************************************************************/

static ERROR CLIPBOARD_AddFile(objClipboard *Self, struct clipAddFile *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);
   if ((!Args->Path) or (!Args->Path[0])) return log.warning(ERR_MissingPath);

   log.branch("Path: %s", Args->Path);

   std::vector<ClipItem> items = { std::string(Args->Path) };
   ERROR error = add_clip(Args->Datatype, items, Args->Flags & (CEF_DELETE|CEF_EXTEND));

#ifdef _WIN32
   // Add the file to the host clipboard
   if ((!(Self->Flags & CLF_DRAG_DROP)) and (!error)) {
      // Build a list of resolved path names in a new buffer that is suitable for passing to Windows.

      std::stringstream win;
      for (auto &item : glClips[0].Items) {
         STRING path;
         if (!ResolvePath(item.Path.c_str(), 0, &path)) {
            win << path << '\0';
            FreeResource(path);
         }
      }
      win << '\0'; // An extra null byte is required to terminate the list for Windows HDROP

      auto str = win.str();
      if (winAddClip(CLIPTYPE_FILE, str.c_str(), str.size(), (Args->Flags & CEF_DELETE) ? TRUE : FALSE)) {
         error = ERR_LimitedSuccess;
      }
   }
#endif

   return error;
}

/*********************************************************************************************************************

-METHOD-
AddObjects: Extract data from objects and add it all to the clipboard.

Data can be saved to the clipboard directly from an object if the object's class supports the SaveToObject action.  The
clipboard will ask that the object save its data directly to a cache file, completely removing the need for the
client to save the object data to an interim file for the clipboard.

Certain classes are recognised by the clipboard system and will be added to the correct datatype automatically (for
instance, Picture objects will be put into the `CLIPTYPE_IMAGE` data category).  If an object's class is not recognised by
the clipboard system then the data will be stored in the `CLIPTYPE_OBJECT` category to signify that there is a class in the
system that recognises the data.  If you want to over-ride any aspect of this behaviour, force the Datatype
parameter with one of the available `CLIPTYPE*` types.

This method supports groups of objects in a single clip, thus requires an array of object ID's terminated
with a zero entry.

Optional flags that may be passed to this method are the same as those specified in the #AddFile() method.  The
`CEF_DELETE` flag has no effect on objects.

-INPUT-
int(CLIPTYPE) Datatype: The type of data representing the objects, or NULL for automatic recognition.
ptr(oid) Objects: Array of object ID's to add to the clipboard.
int(CEF) Flags: Optional flags.

-ERRORS-
Okay: The objects were added to the clipboard.
Args
-END-

*********************************************************************************************************************/

static ERROR CLIPBOARD_AddObjects(objClipboard *Self, struct clipAddObjects *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Objects) or (!Args->Objects[0])) return log.warning(ERR_NullArgs);

   log.branch();

   LONG counter = glCounter++;
   CLASSID classid = 0;
   LONG datatype = Args->Datatype;

   std::vector<ClipItem> items;
   for (unsigned i=0; Args->Objects[i]; i++) {
      pf::ScopedObjectLock<BaseClass> object(Args->Objects[i], 5000);
      if (object.granted()) {
         if (!classid) classid = object.obj->ClassID;

         if (classid IS object.obj->ClassID) { // The client may not mix and match classes.
            if (!datatype) {
               if (object.obj->ClassID IS ID_PICTURE) datatype = CLIPTYPE_IMAGE;
               else if (object.obj->ClassID IS ID_SOUND) datatype = CLIPTYPE_AUDIO;
               else datatype = CLIPTYPE_OBJECT;
            }

            char idx[5];
            snprintf(idx, sizeof(idx), ".%.3d", i);
            auto path = std::string("clipboard:") + GetDatatype(datatype) + std::to_string(counter) + idx;

            objFile::create file = { fl::Path(path), fl::Flags(FL_WRITE|FL_NEW) };
            if (file.ok()) acSaveToObject(object.obj, file->UID, 0);
            else return ERR_CreateFile;
         }
      }
      else return ERR_Lock;
   }

   if (!add_clip(datatype, items, Args->Flags & CEF_EXTEND)) {
      return ERR_Okay;
   }
   else return ERR_Failed;
}

/*********************************************************************************************************************

-METHOD-
AddText: Adds a block of text to the clipboard.

Plain UTF-8 text can be added to the clipboard using the AddText method.

-INPUT-
cstr String: The text to add to the clipboard.

-ERRORS-
Okay
NullArgs
CreateFile
-END-

*********************************************************************************************************************/

static ERROR CLIPBOARD_AddText(objClipboard *Self, struct clipAddText *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->String)) return log.warning(ERR_NullArgs);
   if (!Args->String[0]) return ERR_Okay;

#ifdef _WIN32
   if (!(Self->Flags & CLF_DRAG_DROP)) {
      // Copy text to the windows clipboard.  This requires that we convert from UTF-8 to UTF-16.  For consistency
      // and interoperability purposes, we interact with both the Windows and internal clipboards.

      std::vector<UWORD> utf16(size_t(UTF8Length(Args->String) + 1) * sizeof(WORD));

      auto str = Args->String;
      auto buf = utf16.data();
      while (*str) {
         *buf++ = UTF8ReadValue(str, NULL);
         str += UTF8CharLength(str);
      }
      *buf++ = 0;

      auto error = winAddClip(CLIPTYPE_TEXT, utf16.data(), utf16.size(), FALSE);

      if (error) return log.warning(error);
   }
#endif

   log.branch();

   std::vector<ClipItem> items = { std::string("clipboard:text") + std::to_string(glCounter++) + ".000" };
   if (auto error = add_clip(CLIPTYPE_TEXT, items); !error) {
      pf::Create<objFile> file = { fl::Path(items[0].Path), fl::Flags(FL_WRITE|FL_NEW), fl::Permissions(PERMIT_READ|PERMIT_WRITE) };
      if (file.ok()) {
         file->write(Args->String, StrLength(Args->String), 0);
         return ERR_Okay;
      }
      else return log.warning(ERR_CreateFile);
   }
   else return log.warning(error);
}

/*********************************************************************************************************************
-ACTION-
Clear: Destroys all cached data that is stored in the clipboard.
-END-
*********************************************************************************************************************/

static ERROR CLIPBOARD_Clear(objClipboard *Self, APTR Void)
{
   STRING path;
   if (!ResolvePath("clipboard:", RSF_NO_FILE_CHECK, &path)) {
      DeleteFile(path, NULL);
      CreateFolder(path, PERMIT_READ|PERMIT_WRITE);
      FreeResource(path);
   }

   glClips.clear();
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
DataFeed: This action can be used to place data in a clipboard.

Data can be sent to a clipboard object via the DataFeed action. Currently, only the `DATA_TEXT` type is supported.
All data that is sent to a clipboard object through this action will replace any stored information that matches the
given data type.
-END-
*********************************************************************************************************************/

static ERROR CLIPBOARD_DataFeed(objClipboard *Self, struct acDataFeed *Args)
{
   pf::Log log;
   char buffer[200];

   if (!Args) return log.warning(ERR_NullArgs);

   if (Args->DataType IS DATA_TEXT) {
      log.msg("Copying text to the clipboard.");

      #ifdef _WIN32
      if (!(Self->Flags & CLF_DRAG_DROP)) {
         // Copy text to the windows clipboard.  This requires a conversion from UTF-8 to UTF-16.  For consistency
         // and interoperability purposes, we interact with both the Windows and internal clipboards.

         UWORD *utf16;

         ERROR error = ERR_Okay;
         LONG chars  = UTF8Length((CSTRING)Args->Buffer);
         CSTRING str = (STRING)Args->Buffer;

         LONG bytes = 0;
         for (chars=0; (str[bytes]) and (bytes < Args->Size); chars++) {
            for (++bytes; (bytes < Args->Size) and ((str[bytes] & 0xc0) IS 0x80); bytes++);
         }

        if (!AllocMemory((chars+1) * sizeof(WORD), MEM_DATA|MEM_NO_CLEAR, &utf16)) {
            LONG i = 0;
            while (i < bytes) {
               LONG len = UTF8CharLength(str);
               if (i + len >= bytes) break; // Avoid corrupt UTF-8 sequences resulting in minor buffer overflow
               utf16[i++] = UTF8ReadValue(str, NULL);
               str += len;
            }
            utf16[i] = 0;
            error = winAddClip(CLIPTYPE_TEXT, utf16, (chars+1) * sizeof(WORD), FALSE);
            FreeResource(utf16);
         }
         else error = ERR_AllocMemory;

         if (error) return log.warning(error);
      }
      #endif

      std::vector<ClipItem> items = { std::string("clipboard:text") + std::to_string(glCounter++) + std::string(".000") };
      if (!add_clip(CLIPTYPE_TEXT, items)) {
         objFile::create file = {
            fl::Path(buffer),
            fl::Flags(FL_NEW|FL_WRITE),
            fl::Permissions(PERMIT_READ|PERMIT_WRITE)
         };

         if (file.ok()) {
            if (file->write(Args->Buffer, Args->Size, 0) != ERR_Okay) {
               return log.warning(ERR_Write);
            }

            return ERR_Okay;
         }
         else return log.warning(ERR_CreateObject);
      }
      else return log.warning(ERR_Failed);
   }
   else if ((Args->DataType IS DATA_REQUEST) and (Self->Flags & CLF_DRAG_DROP))  {
      auto request = (struct dcRequest *)Args->Buffer;
      log.branch("Data request from #%d received for item %d, datatype %d", Args->ObjectID, request->Item, request->Preference[0]);

      ERROR error = ERR_Okay;
      if (Self->RequestHandler.Type IS CALL_STDC) {
         auto routine = (ERROR (*)(objClipboard *, OBJECTID, LONG, char *))Self->RequestHandler.StdC.Routine;
         pf::SwitchContext ctx(Self->RequestHandler.StdC.Context);
         error = routine(Self, Args->ObjectID, request->Item, request->Preference);
      }
      else if (Self->RequestHandler.Type IS CALL_SCRIPT) {
         const ScriptArg args[] = {
            { "Clipboard", FD_OBJECTPTR,     { .Address = Self } },
            { "Requester", FD_OBJECTID,      { .Long = Args->ObjectID } },
            { "Item",      FD_LONG,          { .Long = request->Item } },
            { "Datatypes", FD_ARRAY|FD_BYTE, { .Address = request->Preference } },
            { "Size",      FD_LONG|FD_ARRAYSIZE, { .Long = ARRAYSIZE(request->Preference) } }
         };
         auto script = Self->RequestHandler.Script.Script;
         if (scCallback(script, Self->RequestHandler.Script.ProcedureID, args, ARRAYSIZE(args), &error)) error = ERR_Terminate;
      }
      else error = log.warning(ERR_FieldNotSet);

      if (error IS ERR_Terminate) Self->RequestHandler.Type = 0;

      return ERR_Okay;
   }
   else log.warning("Unrecognised data type %d.", Args->DataType);

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR CLIPBOARD_Free(objClipboard *Self, APTR Void)
{
   if (Self->RequestHandler.Type IS CALL_SCRIPT) {
      UnsubscribeAction(Self->RequestHandler.Script.Script, AC_Free);
      Self->RequestHandler.Type = CALL_NONE;
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-METHOD-
GetFiles: Retrieve the most recently clipped data as a list of files.

This method returns a list of items that are on the clipboard.  The caller must declare the types of data that it
supports (or zero if all datatypes are recognised).

The most recently clipped datatype is always returned.  To scan for all available clip items, set the Datatype
parameter to zero and repeatedly call this method with incremented Index numbers until the error code ERR_OutOfRange
is returned.

On success this method will return a list of files (terminated with a NULL entry) in the Files parameter.  Each file is
a readable clipboard entry - how the client reads it depends on the resulting Datatype.  Additionally, the
~Core.IdentifyFile() function could be used to find a class that supports the data.  The resulting Files array is a
memory allocation that must be freed with a call to ~Core.FreeResource().

If this method returns the `CEF_DELETE` flag in the Flags parameter, the client must delete the source files after
successfully copying the data.  When cutting and pasting files within the file system, using ~Core.MoveFile() is
recommended as the most efficient method.

-INPUT-
&int(CLIPTYPE) Datatype: Specify accepted data types here as OR'd flags.  This parameter will be updated to reflect the retrieved data type when the method returns.
int Index: If the Datatype parameter is zero, this parameter may be set to the index of the desired clip item.
!array(cstr) Files: The resulting location(s) of the requested clip data are returned in this parameter; terminated with a NULL entry.  You are required to free the returned array with FreeResource().
&int(CEF) Flags: Result flags are returned in this parameter.  If CEF_DELETE is set, you need to delete the files after use in order to support the 'cut' operation.

-ERRORS-
Okay: A matching clip was found and returned.
Args:
OutOfRange: The specified Index is out of the range of the available clip items.
NoData: No clip was available that matched the requested data type.
-END-

*********************************************************************************************************************/

static ERROR CLIPBOARD_GetFiles(objClipboard *Self, struct clipGetFiles *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   log.branch("Datatype: $%.8x", Args->Datatype);

   Args->Files = NULL;

   // Find the first clipboard entry to match what has been requested

   LONG index;

   if (!Args->Datatype) { // Retrieve the most recent clip item, or the one indicated in the Index parameter.
      if ((Args->Index < 0) or (Args->Index >= LONG(glClips.size()))) return ERR_OutOfRange;
      index = Args->Index;
   }
   else {
      for (index=0; index < LONG(glClips.size()); index++) {
         if (Args->Datatype & glClips[index].Datatype) break;
      }

      if (index >= LONG(glClips.size())) {
         log.warning("No clips available for datatype $%x", Args->Datatype);
         return ERR_NoData;
      }
   }

   auto &clip = glClips[index];

   if (clip.Items.empty()) {
      log.warning("No items are allocated to datatype $%x at clip index %d", clip.Datatype, index);
      return ERR_NoData;
   }

   Args->Flags    = clip.Flags;
   Args->Datatype = clip.Datatype;

   CSTRING *list = NULL;
   LONG str_len = 0;
   for (auto &item : clip.Items) str_len += item.Path.size() + 1;
   if (!AllocMemory(((clip.Items.size()+1) * sizeof(STRING)) + str_len, MEM_DATA|MEM_CALLER, &list)) {
      Args->Files = list;

      auto dest = (char *)list + ((clip.Items.size() + 1) * sizeof(STRING));
      for (auto &item : clip.Items) {
         *list++ = dest;
         CopyMemory(item.Path.c_str(), dest, item.Path.size() + 1);
         dest += item.Path.size() + 1;
      }
      *list = NULL;

      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

/*********************************************************************************************************************

-METHOD-
Remove: Remove items from the clipboard.

The Remove method will clear all items that match a specified datatype.  Clear multiple datatypes by combining flags
in the Datatype parameter.  To clear all content from the clipboard, use the #Clear() action instead of this method.

-INPUT-
int(CLIPTYPE) Datatype: The datatype(s) that will be deleted (datatypes may be logically-or'd together).

-ERRORS-
Okay
NullArgs
AccessMemoryID: The clipboard memory data was not accessible.
-END-

*********************************************************************************************************************/

static ERROR CLIPBOARD_Remove(objClipboard *Self, struct clipRemove *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Datatype)) return log.warning(ERR_NullArgs);

   log.branch("Datatype: $%x", Args->Datatype);

   for (auto it=glClips.begin(); it != glClips.end();) {
      if (it->Datatype & Args->Datatype) {
         if (it IS glClips.begin()) {
            #ifdef _WIN32
            winClearClipboard();
            #endif
         }
         it = glClips.erase(it);
      }
      else it++;
   }

   return ERR_Okay;
}

/*********************************************************************************************************************

-ACTION-
GetVar: Special field types are supported as variables.

The following variable field types are supported by the Clipboard class:

<fields>
<fld type="STRING" name="File(Datatype,Index)">Where Datatype is a recognised data format (e.g. TEXT) and Index is between 0 and the Items() field.  If you don't support multiple clipped items, use an index of zero.  On success, this field will return a file location that points to the clipped data.</>
<fld type="NUMBER" name="Items(Datatype)">Returns the total number of items available for the specified data type.</>
</fields>

-END-

*********************************************************************************************************************/

static ERROR CLIPBOARD_GetVar(objClipboard *Self, struct acGetVar *Args)
{
   pf::Log log;
   char datatype[20], index[20];
   WORD i, j;

   if (!Args) return log.warning(ERR_NullArgs);

   if ((!Args->Field) or (!Args->Buffer) or (Args->Size < 1)) return log.warning(ERR_Args);

   if (!Self->initialised()) return log.warning(ERR_Failed);

   Args->Buffer[0] = 0;

   if (!StrCompare("File(", Args->Field, 0, 0)) {
      // Extract the datatype

      for (j=0, i=6; (Args->Field[i]) and (Args->Field[i] != ',') and (Args->Field[i] != ')'); i++) datatype[j++] = Args->Field[i];
      datatype[j] = 0;

      // Convert the datatype string into its equivalent value

      LONG value = 0;
      for (i=0; glDatatypes[i].Name; i++) {
         if (!StrMatch(datatype, glDatatypes[i].Name)) {
            value = glDatatypes[i].Value;
            break;
         }
      }

      if (Args->Field[i] IS ',') i++;
      while ((Args->Field[i]) and (Args->Field[i] <= 0x20)) i++;

      for (j=0; (Args->Field[i]) and (Args->Field[i] != ')'); i++) index[j++] = Args->Field[i];
      index[j] = 0;

      // Find the clip for the requested datatype

      for (auto &clip : glClips) {
         if (clip.Datatype IS value) {
            LONG item = StrToInt(index);
            if ((item >= 0) and (item < LONG(clip.Items.size()))) {
               StrCopy(clip.Items[item].Path.c_str(), Args->Buffer, Args->Size);
            }
            return ERR_Okay;
         }
      }

      return ERR_Okay;
   }
   else if (!StrCompare("Items(", Args->Field, 0, 0)) {
      // Extract the datatype
      for (j=0, i=6; (Args->Field[i]) and (Args->Field[i] != ')'); i++) datatype[j++] = Args->Field[i];
      datatype[j] = 0;

      // Convert the datatype string into its equivalent value

      LONG value = 0;
      for (LONG i=0; glDatatypes[i].Name; i++) {
         if (!StrMatch(datatype, glDatatypes[i].Name)) {
            value = glDatatypes[i].Value;
            break;
         }
      }

      // Calculate the total number of items available for this datatype

      LONG total = 0;
      if (value) {
         for (unsigned i=0; i < glClips.size(); i++) {
            if (glClips[i].Datatype IS value) {
               total = glClips[i].Items.size();
               break;
            }
         }
      }

      IntToStr(total, Args->Buffer, Args->Size);
      return ERR_Okay;
   }
   else return ERR_NoSupport;
}

//********************************************************************************************************************

static ERROR CLIPBOARD_Init(objClipboard *Self, APTR Void)
{
   pf::Log log;

   // Create a directory under temp: to store clipboard data

   CreateFolder("clipboard:", PERMIT_READ|PERMIT_WRITE);

   // Scan the clipboard: directory for existing clips.  If they exist, add them to our clipboard object.
   // This allows the user to continue using clipboard data from the last time that the machine was powered on.

/*
   DirInfo *dir = NULL;
   if (!OpenDir("clipboard:", 0, &dir)) {

   }
*/
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR CLIPBOARD_NewObject(objClipboard *Self, APTR Void)
{
   return ERR_Okay;
}

/*********************************************************************************************************************

-FIELD-
Flags: Optional flags.

-FIELD-
RequestHandler: Provides a hook for responding to drag and drop requests.

Applications can request data from a clipboard if it is in drag-and-drop mode by sending a `DATA_REQUEST` to the
Clipboard's DataFeed action.  Doing so will result in a callback to the function that is referenced in the
RequestHandler, which must be defined by the source application.  The RequestHandler function must follow this
template:

`ERROR RequestHandler(*Clipboard, OBJECTID Requester, LONG Item, BYTE Datatypes[4])`

The function will be expected to send a `DATA_RECEIPT` to the object referenced in the Requester paramter.  The
receipt must provide coverage for the referenced Item and use one of the indicated Datatypes as the data format.
If this cannot be achieved then `ERR_NoSupport` should be returned by the function.

*********************************************************************************************************************/

static ERROR GET_RequestHandler(objClipboard *Self, FUNCTION **Value)
{
   if (Self->RequestHandler.Type != CALL_NONE) {
      *Value = &Self->RequestHandler;
      return ERR_Okay;
   }
   else return ERR_FieldNotSet;
}

static ERROR SET_RequestHandler(objClipboard *Self, FUNCTION *Value)
{
   if (Value) {
      if (Self->RequestHandler.Type IS CALL_SCRIPT) UnsubscribeAction(Self->RequestHandler.Script.Script, AC_Free);
      Self->RequestHandler = *Value;
      if (Self->RequestHandler.Type IS CALL_SCRIPT) {
         auto callback = make_function_stdc(notify_script_free);
         SubscribeAction(Self->RequestHandler.Script.Script, AC_Free, &callback);
      }
   }
   else Self->RequestHandler.Type = CALL_NONE;
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR add_clip(LONG Datatype, const std::vector<ClipItem> &Items, LONG Flags)
{
   pf::Log log(__FUNCTION__);

   log.branch("Datatype: $%x, Flags: $%x, Total Items: %d", Datatype, Flags, LONG(Items.size()));

   if (Items.empty()) return ERR_Args;

   if (Flags & CEF_EXTEND) {
      // Search for an existing clip that matches the requested datatype
      for (auto it = glClips.begin(); it != glClips.end(); it++) {
         if (it->Datatype IS Datatype) {
            log.msg("Extending existing clip record for datatype $%x.", Datatype);

            auto clip = *it;
            clip.Items.insert(clip.Items.end(), Items.begin(), Items.end());

            // Move clip to the front of the queue.

            glClips.erase(it);
            glClips.insert(glClips.begin(), clip);
            return ERR_Okay;
         }
      }
   }

   ClipEntry clip;

   clip.Datatype = Datatype;
   clip.Flags    = Flags & CEF_DELETE;
   clip.Items    = Items;

   // Remove any existing clips that match this datatype

   for (auto it = glClips.begin(); it != glClips.end(); ) {
      if (it->Datatype IS Datatype) it = glClips.erase(it);
      else it++;
   }

   if (glClips.size() >= MAX_CLIPS) glClips.pop_back(); // Remove oldest clip if history buffer is full.
   glClips.insert(glClips.begin(), clip); // Insert the new clip entry at start of the history buffer
   return ERR_Okay;
}

//********************************************************************************************************************
// Called when the windows clipboard holds new text.  We respond by copying this into our internal clipboard system.

#ifdef _WIN32
#ifdef  __cplusplus
extern "C" void report_windows_clip_text(CSTRING String)
#else
void report_windows_clip_text(CSTRING String)
#endif
{
   pf::Log log("Clipboard");
   log.branch("Application has detected text on the clipboard.");

   objClipboard::create clipboard = { fl::Flags(CLF_HOST) };

   if (clipboard.ok()) {
      clipAddText(*clipboard, String);
   }
   else log.warning(ERR_CreateObject);
}
#endif

//********************************************************************************************************************
// Called when the windows clipboard holds new file references.

#ifdef _WIN32
extern "C" void report_windows_files(APTR Data, LONG CutOperation)
{
   pf::Log log("Clipboard");

   log.branch("Application has detected files on the clipboard.  Cut: %d", CutOperation);

   std::vector<ClipItem> items;
   char buffer[256];
   for (LONG i=0; winExtractFile(Data, i, buffer, sizeof(buffer)); i++) {
      items.push_back(std::string(buffer));
   }
   add_clip(CLIPTYPE_FILE, items, CutOperation ? CEF_DELETE : 0);
}
#endif

//********************************************************************************************************************

#ifdef _WIN32
extern "C" void report_windows_hdrop(STRING Data, LONG CutOperation)
{
   pf::Log log("Clipboard");

   log.branch("Application has detected files on the clipboard.  Cut: %d", CutOperation);

   std::vector<ClipItem> items;
   for (LONG i=0; *Data; i++) {
      items.push_back(std::string(Data));
      while (*Data) Data++; // Next file path
      Data++; // Skip null byte
   }

   add_clip(CLIPTYPE_FILE, items, CutOperation ? CEF_DELETE : 0);
}
#endif

//********************************************************************************************************************
// Called when the windows clipboard holds new text in UTF-16 format.

#ifdef _WIN32
extern "C" void report_windows_clip_utf16(UWORD *String)
{
   pf::Log log("Clipboard");

   log.branch("Application has detected unicode text on the clipboard.");

   objClipboard::create clipboard = { fl::Flags(CLF_HOST) };
   if (clipboard.ok()) {
      std::stringstream buffer;

      for (unsigned chars=0; String[chars]; chars++) {
         auto value = String[chars];
         if (value < 128) buffer << (UBYTE)value;
         else if (value < 0x800) {
            UBYTE b = (value & 0x3f) | 0x80;
            value = value>>6;
            buffer << (value | 0xc0) << b;
         }
         else {
            UBYTE c = (value & 0x3f)|0x80;
            value = value>>6;
            UBYTE b = (value & 0x3f)|0x80;
            value = value>>6;
            buffer << (value | 0xe0) << b << c;
         }
      }

      clipAddText(*clipboard, buffer.str().c_str());
   }
   else log.warning(ERR_CreateObject);
}
#endif

#include "class_clipboard_def.c"

static const FieldArray clFields[] = {
   { "Flags",          FDF_LONGFLAGS|FDF_RI, NULL, NULL, &clClipboardFlags },
   { "RequestHandler", FDF_FUNCTIONPTR|FDF_RW, GET_RequestHandler, SET_RequestHandler },
   END_FIELD
};

//********************************************************************************************************************

ERROR create_clipboard_class(void)
{
   clClipboard = objMetaClass::create::global(
      fl::BaseClassID(ID_CLIPBOARD),
      fl::ClassVersion(VER_CLIPBOARD),
      fl::Name("Clipboard"),
      fl::Category(CCF_IO),
      fl::Actions(clClipboardActions),
      fl::Methods(clClipboardMethods),
      fl::Fields(clFields),
      fl::Size(sizeof(objClipboard)),
      fl::Path(MOD_PATH));

   return clClipboard ? ERR_Okay : ERR_AddClass;
}
