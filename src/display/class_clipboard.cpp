/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
Clipboard: The Clipboard class manages cut, copy and paste between applications.

The Clipboard class manages data transfer between applications on behalf of the user.  Depending on the host system,
behaviour between platforms can vary.

On Windows the clipboard is tightly integrated by default, allowing it to support native Windows applications.  This
reduces the default feature set, but ensures that the clipboard behaves in a way that the user would expect it to.
If historical buffering is enabled with the `CPF::HISTORY_BUFFER` option then the clipboard API will actively monitor
the clipboard and store copied data in the local `clipboard:` file cache.  This results in additional overhead to
clipboard management.

On Linux the clipboard is localised and data is shared between Parasol applications only.

Multiple clipboard objects can be created, but they will share the same group of clipped data for the logged-in user.

There is a limit on the number of clipped items that can be stored in the clipboard.  Only 1 grouping of each
datatype is permitted (for example, only one group of image clips may exist at any time).  In historical buffer mode
there is a fixed limit to the clip count and the oldest members are automatically removed.
-END-

*********************************************************************************************************************/

#include "defs.h"

#ifdef _WIN32
using namespace display;
#endif

#define MAX_CLIPS 10 // Maximum number of clips stored in the historical buffer

static const FieldDef glDatatypes[] = {
   { "data",   CLIPTYPE::DATA },
   { "audio",  CLIPTYPE::AUDIO },
   { "image",  CLIPTYPE::IMAGE },
   { "file",   CLIPTYPE::FILE },
   { "object", CLIPTYPE::OBJECT },
   { "text",   CLIPTYPE::TEXT },
   { NULL, 0 }
};

std::list<ClipRecord> glClips;
static LONG glCounter = 1;
static LONG glHistoryLimit = 1;
static std::string glProcessID;
#ifdef _WIN32
static LONG glLastClipID = -1;
#endif

//********************************************************************************************************************

static std::string get_datatype(CLIPTYPE);
static ERROR add_clip(CLIPTYPE, const std::vector<ClipItem> &, CEF = CEF::NIL);
static ERROR add_clip(CSTRING);
static ERROR CLIPBOARD_AddObjects(objClipboard *, struct clipAddObjects *);

//********************************************************************************************************************

ClipRecord::~ClipRecord() {
   pf::Log log(__FUNCTION__);

   if (Datatype != CLIPTYPE::FILE) {
      log.branch("Deleting clip files for %s datatype.", get_datatype(Datatype).c_str());
      for (auto &item : Items) DeleteFile(item.Path.c_str(), NULL);
   }
   else log.branch("Datatype: File");
}

//********************************************************************************************************************

static std::string get_datatype(CLIPTYPE Datatype)
{
   for (unsigned i=0; glDatatypes[i].Name; i++) {
      if (LONG(Datatype) IS glDatatypes[i].Value) return std::string(glDatatypes[i].Name);
   }

   return "unknown";
}

//********************************************************************************************************************

static void notify_script_free(OBJECTPTR Object, ACTIONID ActionID, ERROR Result, APTR Args)
{
   auto Self = (objClipboard *)CurrentContext();
   Self->RequestHandler.Type = CALL_NONE;
}

//********************************************************************************************************************

static ERROR add_file_to_host(objClipboard *Self, const std::vector<ClipItem> &Items, bool Cut)
{
   pf::Log log;

   if ((Self->Flags & CPF::DRAG_DROP) != CPF::NIL) return ERR_NoSupport;

#ifdef _WIN32
   // Build a list of resolved path names in a new buffer that is suitable for passing to Windows.

   std::stringstream list;
   for (auto &item : Items) {
      STRING path;
      if (!ResolvePath(item.Path.c_str(), RSF::NIL, &path)) {
         list << path << '\0';
         FreeResource(path);
      }
   }
   list << '\0'; // An extra null byte is required to terminate the list for Windows HDROP

   auto str = list.str();
   winAddClip(LONG(CLIPTYPE::FILE), str.c_str(), str.size(), Cut);
   return ERR_Okay;
#else
   return ERR_NoSupport;
#endif
}

//********************************************************************************************************************

static ERROR add_text_to_host(objClipboard *Self, CSTRING String, LONG Length = 0x7fffffff)
{
   pf::Log log(__FUNCTION__);

   if ((Self->Flags & CPF::DRAG_DROP) != CPF::NIL) return ERR_NoSupport;

#ifdef _WIN32
   // Copy text to the windows clipboard.  This requires a conversion from UTF-8 to UTF-16.

   auto str = String;
   LONG chars, bytes = 0;
   for (chars=0; (str[bytes]) and (bytes < Length); chars++) {
      for (++bytes; (bytes < Length) and ((str[bytes] & 0xc0) IS 0x80); bytes++);
   }

   std::vector<UWORD> utf16(size_t(chars + 1) * sizeof(UWORD));

   LONG i = 0;
   while (i < bytes) {
      LONG len = UTF8CharLength(str);
      if (i + len >= bytes) break; // Avoid corrupt UTF-8 sequences resulting in minor buffer overflow
      utf16[i++] = UTF8ReadValue(str, NULL);
      str += len;
   }
   utf16[i] = 0;

   auto error = winAddClip(LONG(CLIPTYPE::TEXT), utf16.data(), utf16.size() * sizeof(UWORD), false);
   if (error) log.warning(error);
   return error;
#else
   return ERR_NoSupport;
#endif
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
NullArgs
MissingPath: The Files argument was not correctly specified.
-END-

*********************************************************************************************************************/

static ERROR CLIPBOARD_AddFile(objClipboard *Self, struct clipAddFile *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);
   if ((!Args->Path) or (!Args->Path[0])) return log.warning(ERR_MissingPath);

   log.branch("Path: %s", Args->Path);

   std::vector<ClipItem> items = { std::string(Args->Path) };
   if (!add_file_to_host(Self, items, ((Args->Flags & CEF::DELETE) != CEF::NIL) ? true : false)) {
      if (glHistoryLimit <= 1) return ERR_Okay;
   }

   return add_clip(Args->Datatype, items, Args->Flags & (CEF::DELETE|CEF::EXTEND));
}

/*********************************************************************************************************************

-METHOD-
AddObjects: Extract data from objects and add it all to the clipboard.

Data can be saved to the clipboard directly from an object if the object's class supports the SaveToObject action.  The
clipboard will ask that the object save its data directly to a cache file, completely removing the need for the
client to save the object data to an interim file for the clipboard.

Certain classes are recognised by the clipboard system and will be added to the correct datatype automatically (for
instance, Picture objects will be put into the `CLIPTYPE::IMAGE` data category).  If an object's class is not recognised by
the clipboard system then the data will be stored in the `CLIPTYPE::OBJECT` category to signify that there is a class in the
system that recognises the data.  If you want to over-ride any aspect of this behaviour, force the Datatype
parameter with one of the available `CLIPTYPE` values.

This method supports groups of objects in a single clip, thus requires an array of object ID's terminated
with a zero entry.

Optional flags that may be passed to this method are the same as those specified in the #AddFile() method.  The
`CEF::DELETE` flag has no effect on objects.

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
   auto datatype = Args->Datatype;

   std::vector<ClipItem> items;
   for (unsigned i=0; Args->Objects[i]; i++) {
      pf::ScopedObjectLock<BaseClass> object(Args->Objects[i], 5000);
      if (object.granted()) {
         if (!classid) classid = object.obj->Class->ClassID;

         if (classid IS object.obj->Class->ClassID) { // The client may not mix and match classes.
            if (datatype IS CLIPTYPE::NIL) {
               if (object.obj->Class->ClassID IS ID_PICTURE) datatype = CLIPTYPE::IMAGE;
               else if (object.obj->Class->ClassID IS ID_SOUND) datatype = CLIPTYPE::AUDIO;
               else datatype = CLIPTYPE::OBJECT;
            }

            char idx[5];
            snprintf(idx, sizeof(idx), ".%.3d", i);
            auto path = std::string("clipboard:") + glProcessID + "_" + get_datatype(datatype) + std::to_string(counter) + idx;

            objFile::create file = { fl::Path(path), fl::Flags(FL::WRITE|FL::NEW) };
            if (file.ok()) acSaveToObject(*object, *file);
            else return ERR_CreateFile;
         }
      }
      else return ERR_Lock;
   }

   if (!add_file_to_host(Self, items, ((Args->Flags & CEF::DELETE) != CEF::NIL) ? true : false)) {
      if (glHistoryLimit <= 1) return ERR_Okay;
   }

   return add_clip(datatype, items, Args->Flags & CEF::EXTEND);
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

   if (!add_text_to_host(Self, Args->String)) {
      if (glHistoryLimit <= 1) return ERR_Okay;
   }

   return add_clip(Args->String);
}

/*********************************************************************************************************************
-ACTION-
Clear: Destroys all cached data that is stored in the clipboard.
-END-
*********************************************************************************************************************/

static ERROR CLIPBOARD_Clear(objClipboard *Self, APTR Void)
{
   STRING path;
   if (!ResolvePath("clipboard:", RSF::NO_FILE_CHECK, &path)) {
      DeleteFile(path, NULL);
      CreateFolder(path, PERMIT::READ|PERMIT::WRITE);
      FreeResource(path);
   }

   glClips.clear();
   return ERR_Okay;
}

/*********************************************************************************************************************
-ACTION-
DataFeed: This action can be used to place data in a clipboard.

Data can be sent to a clipboard object via the DataFeed action. Currently, only the `DATA::TEXT` type is supported.
All data that is sent to a clipboard object through this action will replace any stored information that matches the
given data type.
-END-
*********************************************************************************************************************/

static ERROR CLIPBOARD_DataFeed(objClipboard *Self, struct acDataFeed *Args)
{
   pf::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   if (Args->Datatype IS DATA::TEXT) {
      log.msg("Copying text to the clipboard.");

      add_text_to_host(Self, (CSTRING)Args->Buffer, Args->Size);

      std::vector<ClipItem> items = { std::string("clipboard:") + glProcessID + "_text" + std::to_string(glCounter++) + std::string(".000") };
      if (auto error = add_clip(CLIPTYPE::TEXT, items); !error) {
         objFile::create file = { fl::Path(items[0].Path), fl::Flags(FL::NEW|FL::WRITE), fl::Permissions(PERMIT::READ|PERMIT::WRITE) };
         if (file.ok()) {
            if (file->write(Args->Buffer, Args->Size, 0)) return log.warning(ERR_Write);
            return ERR_Okay;
         }
         else return log.warning(ERR_CreateObject);
      }
      else return log.warning(error);
   }
   else if ((Args->Datatype IS DATA::REQUEST) and ((Self->Flags & CPF::DRAG_DROP) != CPF::NIL))  {
      auto request = (struct dcRequest *)Args->Buffer;
      log.branch("Data request from #%d received for item %d, datatype %d", Args->Object->UID, request->Item, request->Preference[0]);

      ERROR error = ERR_Okay;
      if (Self->RequestHandler.Type IS CALL_STDC) {
         auto routine = (ERROR (*)(objClipboard *, OBJECTPTR, LONG, char *))Self->RequestHandler.StdC.Routine;
         pf::SwitchContext ctx(Self->RequestHandler.StdC.Context);
         error = routine(Self, Args->Object, request->Item, request->Preference);
      }
      else if (Self->RequestHandler.Type IS CALL_SCRIPT) {
         const ScriptArg args[] = {
            { "Clipboard", FD_OBJECTPTR,     { .Address = Self } },
            { "Requester", FD_OBJECTPTR,     { .Address = Args->Object } },
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
   else log.warning("Unrecognised data type %d.", LONG(Args->Datatype));

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

If this method returns the `CEF::DELETE` flag in the Flags parameter, the client must delete the source files after
successfully copying the data.  When cutting and pasting files within the file system, using ~Core.MoveFile() is
recommended as the most efficient method.

-INPUT-
&int(CLIPTYPE) Datatype: Filter down to the specified data types.  This parameter will be updated to reflect the retrieved data type when the method returns.  Set to zero to disable.
int Index: If the Datatype parameter is zero, this parameter may be set to the index of the desired clip item.
!array(cstr) Files: The resulting location(s) of the requested clip data are returned in this parameter; terminated with a NULL entry.  You are required to free the returned array with FreeResource().
&int(CEF) Flags: Result flags are returned in this parameter.  If DELETE is set, you need to delete the files after use in order to support the 'cut' operation.

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

   log.branch("Datatype: $%.8x", LONG(Args->Datatype));

   Args->Files = NULL;

   if ((Self->Flags & CPF::HISTORY_BUFFER) IS CPF::NIL) {
#ifdef _WIN32
      // If the history buffer is disabled then we need to actively retrieve whatever Windows has on the clipboard.
      if (winCurrentClipboardID() != glLastClipID) winCopyClipboard();
#endif
   }

   if (glClips.empty()) return ERR_NoData;

   ClipRecord *clip = &glClips.front();

   // Find the first clipboard entry to match what has been requested

   if ((Self->Flags & CPF::HISTORY_BUFFER) != CPF::NIL) {
      if (Args->Datatype IS CLIPTYPE::NIL) { // Retrieve the most recent clip item, or the one indicated in the Index parameter.
         if ((Args->Index < 0) or (Args->Index >= LONG(glClips.size()))) return log.warning(ERR_OutOfRange);
         std::advance(clip, Args->Index);
      }
      else {
         bool found = false;
         for (auto &scan : glClips) {
            if ((Args->Datatype & scan.Datatype) != CLIPTYPE::NIL) {
               found = true;
               clip = &scan;
               break;
            }
         }

         if (!found) {
            log.warning("No clips available for datatype $%x", LONG(Args->Datatype));
            return ERR_NoData;
         }
      }
   }
   else if (Args->Datatype != CLIPTYPE::NIL) {
      if ((clip->Datatype & Args->Datatype) IS CLIPTYPE::NIL) return ERR_NoData;
   }

   CSTRING *list = NULL;
   LONG str_len = 0;
   for (auto &item : clip->Items) str_len += item.Path.size() + 1;
   if (!AllocMemory(((clip->Items.size()+1) * sizeof(STRING)) + str_len, MEM::NO_CLEAR|MEM::CALLER, &list)) {
      Args->Files    = list;
      Args->Flags    = clip->Flags;
      Args->Datatype = clip->Datatype;

      auto dest = (char *)list + ((clip->Items.size() + 1) * sizeof(STRING));
      for (auto &item : clip->Items) {
         *list++ = dest;
         CopyMemory(item.Path.c_str(), dest, item.Path.size() + 1);
         dest += item.Path.size() + 1;
      }
      *list = NULL;

      return ERR_Okay;
   }
   else return ERR_AllocMemory;
}

//********************************************************************************************************************

static ERROR CLIPBOARD_Init(objClipboard *Self, APTR Void)
{
   if ((Self->Flags & CPF::HISTORY_BUFFER) != CPF::NIL) glHistoryLimit = MAX_CLIPS;

   // Create a directory under temp: to store clipboard data

   CreateFolder("clipboard:", PERMIT::READ|PERMIT::WRITE);

   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR CLIPBOARD_NewObject(objClipboard *Self, APTR Void)
{
   return ERR_Okay;
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
AccessMemory: The clipboard memory data was not accessible.
-END-

*********************************************************************************************************************/

static ERROR CLIPBOARD_Remove(objClipboard *Self, struct clipRemove *Args)
{
   pf::Log log;

   if ((!Args) or (Args->Datatype IS CLIPTYPE::NIL)) return log.warning(ERR_NullArgs);

   log.branch("Datatype: $%x", LONG(Args->Datatype));

   for (auto it=glClips.begin(); it != glClips.end();) {
      if ((it->Datatype & Args->Datatype) != CLIPTYPE::NIL) {
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

-FIELD-
Flags: Optional flags.

-FIELD-
RequestHandler: Provides a hook for responding to drag and drop requests.

Applications can request data from a clipboard if it is in drag-and-drop mode by sending a `DATA::REQUEST` to the
Clipboard's DataFeed action.  Doing so will result in a callback to the function that is referenced in the
RequestHandler, which must be defined by the source application.  The RequestHandler function must follow this
template:

`ERROR RequestHandler(*Clipboard, OBJECTPTR Requester, LONG Item, BYTE Datatypes[4])`

The function will be expected to send a `DATA::RECEIPT` to the object referenced in the Requester paramter.  The
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

static ERROR add_clip(CLIPTYPE Datatype, const std::vector<ClipItem> &Items, CEF Flags)
{
   pf::Log log(__FUNCTION__);

   log.branch("Datatype: $%x, Flags: $%x, Total Items: %d", LONG(Datatype), LONG(Flags), LONG(Items.size()));

   if (Items.empty()) return ERR_Args;

   if ((Flags & CEF::EXTEND) != CEF::NIL) {
      // Search for an existing clip that matches the requested datatype
      for (auto it = glClips.begin(); it != glClips.end(); it++) {
         if (it->Datatype IS Datatype) {
            log.msg("Extending existing clip record for datatype $%x.", LONG(Datatype));

            auto clip = *it;
            clip.Items.insert(clip.Items.end(), Items.begin(), Items.end());

            // Move clip to the front of the queue.

            glClips.erase(it);
            glClips.insert(glClips.begin(), clip);
            return ERR_Okay;
         }
      }
   }

   // Remove any existing clips that match this datatype

   for (auto it = glClips.begin(); it != glClips.end(); ) {
      if (it->Datatype IS Datatype) it = glClips.erase(it);
      else it++;
   }

   if (LONG(glClips.size()) > glHistoryLimit) glClips.pop_back(); // Remove oldest clip if history buffer is full.

   glClips.emplace_front(Datatype, Flags & CEF::DELETE, Items);
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR add_clip(CSTRING String)
{
   pf::Log log(__FUNCTION__);
   log.branch();

   std::vector<ClipItem> items = { std::string("clipboard:") + glProcessID + "_text" + std::to_string(glCounter++) + ".000" };
   if (auto error = add_clip(CLIPTYPE::TEXT, items); !error) {
      pf::Create<objFile> file = { fl::Path(items[0].Path), fl::Flags(FL::WRITE|FL::NEW), fl::Permissions(PERMIT::READ|PERMIT::WRITE) };
      if (file.ok()) {
         file->write(String, StrLength(String), 0);
         return ERR_Okay;
      }
      else return log.warning(ERR_CreateFile);
   }
   else return log.warning(error);
}

//********************************************************************************************************************
// Called when the Windows clipboard holds new text.  We respond by copying this into our internal clipboard system.

#ifdef _WIN32
extern "C" void report_windows_clip_text(CSTRING String)
{
   pf::Log log("Clipboard");
   log.branch("Application has detected text on the clipboard.");

   add_clip(String);
   glLastClipID = winCurrentClipboardID();
}

//********************************************************************************************************************
// Called when the Windows clipboard holds new file references.  We store a direct reference to the file path.

extern "C" void report_windows_files(APTR Data, LONG CutOperation)
{
   pf::Log log("Clipboard");
   log.branch("Application has detected files on the clipboard.  Cut: %d", CutOperation);

   std::vector<ClipItem> items;
   char buffer[256];
   for (LONG i=0; winExtractFile(Data, i, buffer, sizeof(buffer)); i++) {
      items.push_back(std::string(buffer));
   }
   add_clip(CLIPTYPE::FILE, items, CutOperation ? CEF::DELETE : CEF::NIL);
   glLastClipID = winCurrentClipboardID();
}

//********************************************************************************************************************

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

   add_clip(CLIPTYPE::FILE, items, CutOperation ? CEF::DELETE : CEF::NIL);
   glLastClipID = winCurrentClipboardID();
}

//********************************************************************************************************************
// Called when the Windows clipboard holds new text in UTF-16 format.

extern "C" void report_windows_clip_utf16(UWORD *String)
{
   pf::Log log("Clipboard");
   log.branch("Application has detected unicode text on the clipboard.");

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

   add_clip(buffer.str().c_str());
   glLastClipID = winCurrentClipboardID();
}

//********************************************************************************************************************
// Intercept changes to the Windows clipboard.  If the history buffer is enabled then we need to pro-actively copy
// content from the clipboard.

extern "C" void win_clipboard_updated()
{
   pf::Log log(__FUNCTION__);
   log.branch();
   if (glHistoryLimit <= 1) return;
   winCopyClipboard();
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
      fl::Category(CCF::IO),
      fl::Actions(clClipboardActions),
      fl::Methods(clClipboardMethods),
      fl::Fields(clFields),
      fl::Size(sizeof(objClipboard)),
      fl::Path(MOD_PATH));

   LONG pid;
   if (!CurrentTask()->get(FID_ProcessID, &pid)) glProcessID = std::to_string(pid);

   return clClipboard ? ERR_Okay : ERR_AddClass;
}
