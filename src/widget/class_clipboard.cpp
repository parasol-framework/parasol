/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

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

*****************************************************************************/

#define PRV_CLIPBOARD
#define PRV_WIDGET_MODULE
#include <parasol/modules/widget.h>

#ifdef _WIN32
#define WINDOWS_WINDOWS_H
#include "platform/windows.c"
#endif

#include "defs.h"

static objMetaClass *clClipboard = NULL;

static const FieldDef glDatatypes[] = {
   { "data",   CLIPTYPE_DATA },
   { "audio",  CLIPTYPE_AUDIO },
   { "image",  CLIPTYPE_IMAGE },
   { "file",   CLIPTYPE_FILE },
   { "object", CLIPTYPE_OBJECT },
   { "text",   CLIPTYPE_TEXT },
   { NULL, 0 }
};

#define MAX_CLIPS 10     // Maximum number of clips stored in the historical buffer

struct ClipHeader {
   LONG Counter;
#ifdef _WIN32
   LONG LastID;
   UBYTE Init:1;
#endif
};

struct ClipEntry {
   LONG     Datatype;    // The type of data clipped
   LONG     Flags;       // CEF_DELETE may be set for the 'cut' operation
   CLASSID  ClassID;     // Class ID that is capable of managing the clip data, if it originated from an object
   MEMORYID Files;       // List of file locations, separated with semi-colons, referencing all the data in this clip entry
   LONG     FilesLen;    // Complete byte-length of the Files string
   UWORD    ID;          // Unique identifier for the clipboard entry
   WORD     TotalItems;  // Total number of items in the clip-set
};

static ERROR add_clip(MEMORYID, LONG, CSTRING, LONG, CLASSID, LONG, LONG *);
static void free_clip(ClipEntry *);
static ERROR CLIPBOARD_AddObjects(objClipboard *, struct clipAddObjects *);

//****************************************************************************

static CSTRING GetDatatype(LONG Datatype)
{
   for (WORD i=0; glDatatypes[i].Name; i++) {
      if (Datatype IS glDatatypes[i].Value) return (CSTRING)glDatatypes[i].Name;
   }

   return "unknown";
}

//****************************************************************************

static ERROR CLIPBOARD_ActionNotify(objClipboard *Self, struct acActionNotify *Args)
{
   if (Args->Error != ERR_Okay) return ERR_Okay;

   if (Args->ActionID IS AC_Free) {
      if ((Self->RequestHandler.Type IS CALL_SCRIPT) AND (Self->RequestHandler.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->RequestHandler.Type = CALL_NONE;
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

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

*****************************************************************************/

static ERROR CLIPBOARD_AddFile(objClipboard *Self, struct clipAddFile *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);
   if ((!Args->Path) OR (!Args->Path[0])) return log.warning(ERR_MissingPath);

   log.branch("Cluster: %d, Path: %s", Self->ClusterID, Args->Path);

   ERROR error = add_clip(Self->ClusterID, Args->Datatype, Args->Path, Args->Flags & (CEF_DELETE|CEF_EXTEND), 0, 1, 0);

#ifdef _WIN32
   // Add the file to the host clipboard
   if ((!(Self->Flags & CLF_DRAG_DROP)) AND (!error)) {
      parasol::ScopedAccessMemory<ClipHeader> header(Self->ClusterID, MEM_READ_WRITE, 3000);
      if (header.granted()) {
         auto clips = (ClipEntry *)(header.ptr + 1);
         parasol::ScopedAccessMemory<char> str(clips->Files, MEM_READ_WRITE, 3000);
         if (str.granted()) {
            // Build a list of resolved path names in a new buffer that is suitable for passing to Windows.

            STRING win;
            if (!AllocMemory(512 * clips->TotalItems, MEM_DATA|MEM_NO_CLEAR, &win, NULL)) {
               LONG j = 0, winpos = 0;
               for (LONG i=0; i < clips->TotalItems; i++) {
                  STRING path;
                  if (!ResolvePath(str.ptr+j, 0, &path)) {
                     winpos += StrCopy(path, win+winpos, 511) + 1;
                     FreeResource(path);
                  }

                  while (str.ptr[j]) j++;
                  j++;
               }
               win[winpos++] = 0; // An extra null byte is required to terminate the list for Windows HDROP

               if (winAddClip(CLIPTYPE_FILE, win, winpos, (Args->Flags & CEF_DELETE) ? TRUE : FALSE)) {
                  error = ERR_LimitedSuccess;
               }

               FreeResource(win);
            }
         }
      }
   }
#endif

   return error;
}

/*****************************************************************************

-METHOD-
AddObject: Extract data from an object and add it to the clipboard.

This method is a simple implementation of the #AddObjects() method and is intended primarily for script usage.
Please see the #AddObjects() method for details on adding objects to the clipboard.

-INPUT-
int(CLIPTYPE) Datatype: The type of data that you want the object data to be recognised as, or NULL for automatic recognition.
oid Object: The object containing the data to add.
int(CEF) Flags: Optional flags.

-ERRORS-
Okay: The object was added to the clipboard.
NullArgs

*****************************************************************************/

static ERROR CLIPBOARD_AddObject(objClipboard *Self, struct clipAddObject *Args)
{
   if (!Args) return ERR_NullArgs;

   OBJECTID objects[2] = { Args->ObjectID, 0 };
   struct clipAddObjects add = { .Objects = objects, .Flags = Args->Flags };
   return CLIPBOARD_AddObjects(Self, &add);
}

/*****************************************************************************

-METHOD-
AddObjects: Extract data from objects and add it all to the clipboard.

Data can be saved to the clipboard directly from an object if the object's class supports the SaveToObject action.  The
clipboard will ask that the object save its data directly to a cache file, completely removing the need for you to save
the object data to an interim file for the clipboard.

Certain classes are recognised by the clipboard system and will be added to the correct datatype automatically (for
instance, Picture objects will be put into the CLIPTYPE_IMAGE data category).  If an object's class is not recognised by
the clipboard system then the data will be stored in the CLIPTYPE_OBJECT category to signify that there is a class in the
system that recognises the data.  If you want to over-ride any aspect of this behaviour, you need to force the Datatype
parameter with one of the available CLIPTYPE* types.

This method supports groups of objects in a single clip, thus requires you to pass an array of object ID's, terminated
with a NULL entry.

Optional flags that may be passed to this method are the same as those specified in the #AddFile() method.  The
CEF_DELETE flag has no effect on objects.

This method should always be called directly and not messaged to the clipboard, unless you are able to guarantee that
the source objects are shared.

-INPUT-
int(CLIPTYPE) Datatype: The type of data that you want the object data to be recognised as, or NULL for automatic recognition.
ptr(oid) Objects: Array of shared object ID's to add to the clipboard.
int(CEF) Flags: Optional flags.

-ERRORS-
Okay: The objects were added to the clipboard.
Args
-END-

*****************************************************************************/

static ERROR CLIPBOARD_AddObjects(objClipboard *Self, struct clipAddObjects *Args)
{
   parasol::Log log;

   if ((!Args) OR (!Args->Objects) OR (!Args->Objects[0])) return log.warning(ERR_NullArgs);

   log.branch();

   // Use the SaveToObject action to save each object's data to the clipboard storage area.  The class ID for each
   // object is also recorded.

   OBJECTID *list = Args->Objects;
   WORD total;
   for (total=0; list[total]; total++);

   LONG counter;
   CLASSID classid = 0;
   LONG datatype = 0;
   if (!add_clip(Self->ClusterID, datatype, 0, Args->Flags & CEF_EXTEND, 0, total, &counter)) {
      for (LONG i=0; list[i]; i++) {
         parasol::ScopedObjectLock<Head> object(list[i], 5000);
         if (object.granted()) {
            if (!classid) classid = object.obj->ClassID;

            if (classid IS object.obj->ClassID) {
               char path[100];

               datatype = Args->Datatype;
               if (!datatype) {
                  if (object.obj->ClassID IS ID_PICTURE) {
                     StrFormat(path, sizeof(path), "clipboard:image%d.%.3d", counter, i);
                     datatype = CLIPTYPE_IMAGE;
                  }
                  else if (object.obj->ClassID IS ID_SOUND) {
                     StrFormat(path, sizeof(path), "clipboard:audio%d.%.3d", counter, i);
                     datatype = CLIPTYPE_AUDIO;
                  }
                  else {
                     StrFormat(path, sizeof(path), "clipboard:object%d.%.3d", counter, i);
                     datatype = CLIPTYPE_OBJECT;
                  }
               }
               else { // Use the specified datatype
                  StrFormat(path, sizeof(path), "clipboard:%s%d.%.3d", GetDatatype(datatype), counter, i);
               }

               SaveObjectToFile(object.obj, path, 0);
            }
         }
      }
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
AddText: Adds a block of text to the clipboard.

Text can be added to the clipboard using the AddText method.  This is the simplest way of passing text to the clipboard,
although passing text through the data feed system may also be convenient in certain circumstances. Text is passed
to the clipboard via the String parameter and it must be terminated with a null byte.

-INPUT-
cstr String: The text to add to the clipboard.

-ERRORS-
Okay
Args
File
-END-

*****************************************************************************/

static ERROR CLIPBOARD_AddText(objClipboard *Self, struct clipAddText *Args)
{
   parasol::Log log;

   if ((!Args) OR (!Args->String)) return log.warning(ERR_NullArgs);
   if (!Args->String[0]) return ERR_Okay;

#ifdef _WIN32
   if (!(Self->Flags & CLF_DRAG_DROP)) {
      // Copy text to the windows clipboard.  This requires that we convert from UTF-8 to UTF-16.  For consistency
      // and interoperability purposes, we interact with both the Windows and internal clipboards.

      UWORD *utf16;
      ERROR error = ERR_Okay;
      LONG chars = UTF8Length(Args->String);
      if (!AllocMemory((chars+1) * sizeof(WORD), MEM_DATA|MEM_NO_CLEAR, &utf16, NULL)) {
         CSTRING str = Args->String;
         LONG i = 0;
         while (*str) {
            utf16[i++] = UTF8ReadValue(str, NULL);
            str += UTF8CharLength(str);
         }
         utf16[i] = 0;
         error = winAddClip(CLIPTYPE_TEXT, utf16, (chars+1) * sizeof(WORD), FALSE);
         FreeResource(utf16);
      }
      else error = ERR_AllocMemory;

      if (error) return log.warning(error);
   }
#endif

   log.branch();

   ERROR error;
   LONG counter;
   if (!(error = add_clip(Self->ClusterID, CLIPTYPE_TEXT, 0, 0, 0, 1, &counter))) {
      char buffer[200];
      StrFormat(buffer, sizeof(buffer), "clipboard:text%d.000", counter);

      OBJECTPTR file;
      if (!CreateObject(ID_FILE, 0, &file,
            FID_Path|TSTR,         buffer,
            FID_Flags|TLONG,       FL_NEW|FL_WRITE,
            FID_Permissions|TLONG, PERMIT_READ|PERMIT_WRITE,
            TAGEND)) {

         acWrite(file, Args->String, StrLength(Args->String), 0);

         acFree(file);
         return ERR_Okay;
      }
      else return log.warning(ERR_CreateFile);
   }
   else return log.warning(error);
}

/*****************************************************************************
-ACTION-
Clear: Destroys all cached data that is stored in the clipboard.
-END-
*****************************************************************************/

static ERROR CLIPBOARD_Clear(objClipboard *Self, APTR Void)
{
   // Delete the clipboard directory and all content

   STRING path;
   if (!ResolvePath("clipboard:", RSF_NO_FILE_CHECK, &path)) {
      DeleteFile(path, NULL);
      CreateFolder(path, PERMIT_READ|PERMIT_WRITE);
      FreeResource(path);
   }

   // Annihilate all historical clip information

   parasol::ScopedAccessMemory<ClipEntry> clips(Self->ClusterID, MEM_READ_WRITE, 3000);
   if (clips.granted()) {
      ClearMemory(&clips.ptr, sizeof(ClipHeader) + (MAX_CLIPS * sizeof(ClipEntry)));
      return ERR_Okay;
   }
   else return ERR_AccessMemory;
}

/*****************************************************************************
-ACTION-
DataFeed: This action can be used to place data in a clipboard.

Data can be sent to a clipboard object via the DataFeed action. Currently, only the DATA_TEXT type is supported.
All data that is sent to a clipboard object through this action will replace any stored information that matches the
given data type.
-END-
*****************************************************************************/

static ERROR CLIPBOARD_DataFeed(objClipboard *Self, struct acDataFeed *Args)
{
   parasol::Log log;
   OBJECTPTR file;
   char buffer[200];
   LONG counter;

   if (!Args) return log.warning(ERR_NullArgs);

   if (Args->DataType IS DATA_TEXT) {
      log.msg("Copying text to the clipboard.");

      #ifdef _WIN32
      if (!(Self->Flags & CLF_DRAG_DROP)) {
         // Copy text to the windows clipboard.  This requires a conversion from UTF-8 to UTF-16.  For consistency
         // and interoperability purposes, we interact with both the Windows and internal clipboards.

         UWORD *utf16;

         ERROR error = ERR_Okay;
         LONG chars = UTF8Length((CSTRING)Args->Buffer);
         CSTRING str = (STRING)Args->Buffer;

         LONG bytes = 0;
         for (chars=0; (str[bytes]) AND (bytes < Args->Size); chars++) {
            for (++bytes; (bytes < Args->Size) AND ((str[bytes] & 0xc0) IS 0x80); bytes++);
         }

        if (!AllocMemory((chars+1) * sizeof(WORD), MEM_DATA|MEM_NO_CLEAR, &utf16, NULL)) {
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

      if (!add_clip(Self->ClusterID, CLIPTYPE_TEXT, 0, 0, 0, 1, &counter)) {
         StrFormat(buffer, sizeof(buffer), "clipboard:text%d.000", counter);

         if (!CreateObject(ID_FILE, 0, &file,
               FID_Path|TSTR,         buffer,
               FID_Flags|TLONG,       FL_NEW|FL_WRITE,
               FID_Permissions|TLONG, PERMIT_READ|PERMIT_WRITE,
               TAGEND)) {

            if (acWrite(file, Args->Buffer, Args->Size, 0) != ERR_Okay) {
               acFree(file);
               return log.warning(ERR_Write);
            }

            acFree(file);
            return ERR_Okay;
         }
         else return log.warning(ERR_CreateObject);
      }
      else return log.warning(ERR_Failed);
   }
   else if ((Args->DataType IS DATA_REQUEST) AND (Self->Flags & CLF_DRAG_DROP))  {
      if (Self->RequestHandler.Type) {
         auto request = (struct dcRequest *)Args->Buffer;
         log.branch("Data request from #%d received for item %d, datatype %d", Args->ObjectID, request->Item, request->Preference[0]);

         ERROR error;
         if (Self->RequestHandler.Type IS CALL_STDC) {
            auto routine = (ERROR (*)(objClipboard *, OBJECTID, LONG, BYTE *))Self->RequestHandler.StdC.Routine;
            parasol::SwitchContext ctx(Self->RequestHandler.StdC.Context);
            error = routine(Self, Args->ObjectID, request->Item, request->Preference);
         }
         else if (Self->RequestHandler.Type IS CALL_SCRIPT) {
            OBJECTPTR script;
            if ((script = Self->RequestHandler.Script.Script)) {
               const ScriptArg args[] = {
                  { "Clipboard", FD_OBJECTPTR,     { .Address = Self } },
                  { "Requester", FD_OBJECTID,      { .Long = Args->ObjectID } },
                  { "Item",      FD_LONG,          { .Long = request->Item } },
                  { "Datatypes", FD_ARRAY|FD_BYTE, { .Address = request->Preference } },
                  { "Size",      FD_LONG|FD_ARRAYSIZE, { .Long = ARRAYSIZE(request->Preference) } }
               };
               if (scCallback(script, Self->RequestHandler.Script.ProcedureID, args, ARRAYSIZE(args), &error)) error = ERR_Terminate;
            }
            else error = ERR_Terminate;
         }
         else error = log.warning(ERR_FieldNotSet);

         if (error IS ERR_Terminate) Self->RequestHandler.Type = 0;

         return ERR_Okay;
      }
      else return ERR_NoSupport;
   }
   else log.warning("Unrecognised data type %d.", Args->DataType);

   return ERR_Okay;
}

/*****************************************************************************

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

*****************************************************************************/

static ERROR CLIPBOARD_Remove(objClipboard *Self, struct clipRemove *Args)
{
   parasol::Log log;

   if ((!Args) OR (!Args->Datatype)) return log.warning(ERR_NullArgs);

   log.branch("Cluster: %d, Datatype: $%x", Self->ClusterID, Args->Datatype);

   ClipHeader *header;

   if (!AccessMemory(Self->ClusterID, MEM_READ_WRITE, 3000, &header)) {
      auto clips = (ClipEntry *)(header + 1);
      for (WORD i=0; i < MAX_CLIPS; i++) {
         if (clips[i].Datatype & Args->Datatype) {
            if (i IS 0) {
               #ifdef _WIN32
               winClearClipboard();
               #endif
            }
            free_clip(&clips[i]);
         }
      }

      ReleaseMemory(header);
      return ERR_Okay;
   }
   else return log.warning(ERR_AccessMemory);
}

//****************************************************************************

static ERROR CLIPBOARD_Free(objClipboard *Self, APTR Void)
{
   if (Self->ClusterAllocated) { FreeResourceID(Self->ClusterID); Self->ClusterID = 0; }
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
GetFiles: Retrieve the most recently clipped data as a list of files.

This method returns a list of items that are on the clipboard.  The caller must declare the types of data that it
supports (or zero if all datatypes are recognised).

The most recently clipped datatype is always returned.  To scan for all available clip items, set the Datatype
parameter to zero and repeatedly call this method with incremented Index numbers until the error code ERR_OutOfRange
is returned.

On success this method will return a list of files (terminated with a NULL entry) in the Files parameter.  Each file is
a readable clipboard entry - how the client reads it depends on the resulting Datatype.  Additionally, the
IdentifyFile() function could be used to find a class that supports the data.  The resulting Files array is a memory
allocation that must be freed with a call to ~Core.FreeResource().

If this method returns the CEF_DELETE flag in the Flags parameter, the client must delete the source files after
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

*****************************************************************************/

static ERROR CLIPBOARD_GetFiles(objClipboard *Self, struct clipGetFiles *Args)
{
   parasol::Log log;

   if (!Args) return log.warning(ERR_NullArgs);

   log.branch("Cluster: %d, Datatype: $%.8x", Self->ClusterID, Args->Datatype);

   Args->Files = NULL;

   // Find the first clipboard entry to match what has been requested

   ClipHeader *header;
   if (!AccessMemory(Self->ClusterID, MEM_READ_WRITE, 3000, &header)) {
      auto clips = (ClipEntry *)(header + 1);

      WORD index, i;

      if (!Args->Datatype) { // Retrieve the most recent clip item, or the one indicated in the Index parameter.
         if ((Args->Index < 0) OR (Args->Index >= MAX_CLIPS)) {
            ReleaseMemory(header);
            return ERR_OutOfRange;
         }

         index = Args->Index;
      }
      else {
         for (index=0; index < MAX_CLIPS; index++) {
            if (Args->Datatype & clips[index].Datatype) break;
         }
      }

      if (index >= MAX_CLIPS) {
         log.warning("No clips available for datatype $%x", Args->Datatype);
         ReleaseMemory(header);
         return ERR_NoData;
      }
      else if (clips[index].TotalItems < 1) {
         log.warning("No items are allocated to datatype $%x at clip index %d", clips[index].Datatype, index);
         ReleaseMemory(header);
         return ERR_NoData;
      }

      ERROR error;
      STRING files;
      CSTRING *list = NULL;
      if (clips[index].Files) {
         if (!(error = AccessMemory(clips[index].Files, MEM_READ, 3000, &files))) {
            MEMINFO info;
            if (!MemoryIDInfo(clips[index].Files, &info)) {
               if (!AllocMemory(((clips[index].TotalItems+1) * sizeof(STRING)) + info.Size, MEM_DATA|MEM_CALLER, &list, NULL)) {
                  CopyMemory(files, list + clips[index].TotalItems + 1, info.Size);
               }
               else {
                  ReleaseMemory(files);
                  ReleaseMemory(header);
                  return ERR_AllocMemory;
               }
            }
            ReleaseMemory(files);
         }
         else {
            log.warning("Failed to access file string #%d, error %d.", clips[index].Files, error);
            if (error IS ERR_MemoryDoesNotExist) clips[index].Files = 0;
            ReleaseMemory(header);
            return ERR_AccessMemory;
         }
      }
      else {
         if (clips[index].Datatype IS CLIPTYPE_FILE) {
            log.warning("File datatype detected, but no file list has been set.");
            ReleaseMemory(header);
            return ERR_Failed;
         }

         // Calculate the length of the file strings

         char buffer[100];
         LONG len = StrFormat(buffer, sizeof(buffer), "clipboard:%s%d.000", GetDatatype(clips[index].Datatype), clips[index].ID);
         len = ((len + 1) * clips[index].TotalItems) + 1;

         // Allocate the list array with some room for the strings at the end and then fill it with data.

         if (!AllocMemory(((clips[index].TotalItems+1) * sizeof(STRING)) + len, MEM_DATA|MEM_CALLER, &list, NULL)) {
            STRING str = (STRING)(list + clips[index].TotalItems + 1);
            for (i=0; i < clips[index].TotalItems; i++) {
               if (i > 0) *str++ = '\0'; // Each file is separated with a NULL character
               StrFormat(str, len, "clipboard:%s%d.%.3d", GetDatatype(clips[index].Datatype), clips[index].ID, i);
               while (*str) str++;
            }
            *str = 0;
         }
         else {
            ReleaseMemory(header);
            return ERR_AllocMemory;
         }
      }

      // Setup the pointers in the string list

      CSTRING str = (CSTRING)(list + clips[index].TotalItems + 1);
      LONG j = 0;
      for (i=0; i < clips[index].TotalItems; i++) {
         list[i] = str + j;
         while (str[j]) j++;  // Go to the end of the string to get to the next entry
         j++;
      }
      list[i] = NULL;

      // Results

      Args->Datatype = clips[index].Datatype;
      Args->Files    = list;
      Args->Flags    = clips[index].Flags;

      ReleaseMemory(header);
      return ERR_Okay;
   }
   else return ERR_AccessMemory;
}

/*****************************************************************************

-ACTION-
GetVar: Special field types are supported as variables.

The following variable field types are supported by the Clipboard class:

<fields>
<fld type="STRING" name="File(Datatype,Index)">Where Datatype is a recognised data format (e.g. TEXT) and Index is between 0 and the Items() field.  If you don't support multiple clipped items, use an index of zero.  On success, this field will return a file location that points to the clipped data.</>
<fld type="NUMBER" name="Items(Datatype)">Returns the total number of items available for the specified data type.</>
</fields>

-END-

*****************************************************************************/

static ERROR CLIPBOARD_GetVar(objClipboard *Self, struct acGetVar *Args)
{
   parasol::Log log;
   char datatype[20], index[20];
   WORD i, j;

   if (!Args) return log.warning(ERR_NullArgs);

   if ((!Args->Field) OR (!Args->Buffer) OR (Args->Size < 1)) {
      return log.warning(ERR_Args);
   }

   if (!(Self->Head.Flags & NF_INITIALISED)) return log.warning(ERR_Failed);

   Args->Buffer[0] = 0;

   if (!StrCompare("File(", Args->Field, 0, 0)) {
      // Extract the datatype

      for (j=0, i=6; (Args->Field[i]) AND (Args->Field[i] != ',') AND (Args->Field[i] != ')'); i++) datatype[j++] = Args->Field[i];
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
      while ((Args->Field[i]) AND (Args->Field[i] <= 0x20)) i++;

      for (j=0; (Args->Field[i]) AND (Args->Field[i] != ')'); i++) index[j++] = Args->Field[i];
      index[j] = 0;

      ClipHeader *header;
      if (!AccessMemory(Self->ClusterID, MEM_READ_WRITE, 3000, &header)) {
         // Find the clip for the requested datatype

         auto clips = (ClipEntry *)(header + 1);

         ClipEntry *clip = NULL;
         for (i=0; i < MAX_CLIPS; i++) {
            if (clips[i].Datatype IS value) {
               clip = &clips[i];
               break;
            }
         }

         if (clip) {
            LONG i = StrToInt(index);
            if ((i >= 0) AND (i < clip->TotalItems)) {
               if (clip->Files) {
                  STRING files;
                  if (!AccessMemory(clip->Files, MEM_READ, 3000, &files)) {
                     // Find the file path that we're looking for

                     LONG j = 0;
                     while ((i) AND (j < clip->FilesLen)) {
                        for (; files[j]; j++);
                        if (j < clip->FilesLen) j++; // Skip null byte separator
                        i--;
                     }

                     // Copy the discovered path into the result buffer

                     StrCopy(files+j, Args->Buffer, Args->Size);

                     ReleaseMemory(files);
                  }
                  else {
                     ReleaseMemory(header);
                     return log.warning(ERR_AccessMemory);
                  }
               }
               else StrFormat(Args->Buffer, Args->Size, "clipboard:%s%d.%.3d", datatype, clip->ID, i);
            }
         }

         ReleaseMemory(header);
         return ERR_Okay;
      }
      else return log.warning(ERR_AccessMemory);
   }
   else if (!StrCompare("Items(", Args->Field, 0, 0)) {
      // Extract the datatype
      for (j=0, i=6; (Args->Field[i]) AND (Args->Field[i] != ')'); i++) datatype[j++] = Args->Field[i];
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
         parasol::ScopedAccessMemory<ClipHeader> header(Self->ClusterID, MEM_READ, 3000);
         if (header.granted()) {
            auto clips = (ClipEntry *)(header.ptr + 1);
            for (WORD i=0; i < MAX_CLIPS; i++) {
               if (clips[i].Datatype IS value) {
                  total = clips[i].TotalItems;
                  break;
               }
            }
         }
      }

      IntToStr(total, Args->Buffer, Args->Size);
      return ERR_Okay;
   }
   else return ERR_NoSupport;
}

//****************************************************************************

static ERROR CLIPBOARD_Init(objClipboard *Self, APTR Void)
{
   parasol::Log log;

   if ((!Self->ClusterID) OR (Self->Flags & CLF_DRAG_DROP)) {
      // Create a new grouping for this clipboard.  It will be possible for any other clipboard to attach
      // itself to this memory block if the ID is known.

      if (!AllocMemory(sizeof(ClipHeader) + (MAX_CLIPS * sizeof(ClipEntry)), MEM_PUBLIC|MEM_NO_BLOCKING, NULL, &Self->ClusterID)) {
         Self->ClusterAllocated = TRUE;
      }
      else return log.warning(ERR_AllocMemory);
   }

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

//****************************************************************************

static ERROR CLIPBOARD_NewObject(objClipboard *Self, APTR Void)
{
   Self->ClusterID = RPM_Clipboard;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Cluster: Identifies a unique cluster of items targeted by a clipboard object.

By default, all clipboard objects will operate on a global cluster of clipboard entries.  This global cluster is used
by all applications, so a cut operation in application 1 would transfer selected items during a paste operation to
application 2.

If the Cluster field is set to zero prior to initialisation, a unique cluster will be assigned to that clipboard object.
The ID of that cluster can be read from the Cluster field at any time and used in the creation of new clipboard objects.
By sharing the ID with other applications, a private clipboard can be created that does not impact on the user's cut
and paste operations.

-FIELD-
Flags: Optional flags.

-FIELD-
RequestHandler: Provides a hook for responding to drag and drop requests.

Applications can request data from a clipboard if it is in drag-and-drop mode by sending a DATA_REQUEST to the
Clipboard's DataFeed action.  Doing so will result in a callback to the function that is referenced in the
RequestHandler, which must be defined by the source application.  The RequestHandler function must follow this
template:

`ERROR RequestHandler(*Clipboard, OBJECTID Requester, LONG Item, BYTE Datatypes[4])`

The function will be expected to send a DATA_RECEIPT to the object referenced in the Requester paramter.  The
receipt must provide coverage for the referenced Item and use one of the indicated Datatypes as the data format.
If this cannot be achieved then ERR_NoSupport should be returned by the function.

*****************************************************************************/

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
      if (Self->RequestHandler.Type IS CALL_SCRIPT) SubscribeAction(Self->RequestHandler.Script.Script, AC_Free);
   }
   else Self->RequestHandler.Type = CALL_NONE;
   return ERR_Okay;
}

//****************************************************************************

static void free_clip(ClipEntry *Clip)
{
   parasol::Log log(__FUNCTION__);

   if (Clip->TotalItems > 16384) Clip->TotalItems = 16384;

   if (Clip->Datatype != CLIPTYPE_FILE) {
      CSTRING datatype = GetDatatype(Clip->Datatype);

      log.branch("Deleting %d clip files for datatype %s / %d.", Clip->TotalItems, datatype, Clip->Datatype);

      // Delete cached clipboard files

      for (WORD i=0; i < Clip->TotalItems; i++) {
         char buffer[200];
         StrFormat(buffer, sizeof(buffer), "clipboard:%s%d.%.3d", datatype, Clip->ID, i);
         DeleteFile(buffer, NULL);
      }
   }
   else log.branch("Datatype: File");

   if (Clip->Files) { FreeResourceID(Clip->Files); Clip->Files = 0; }

   ClearMemory(Clip, sizeof(ClipEntry));
}

//****************************************************************************

static ERROR add_clip(MEMORYID ClusterID, LONG Datatype, CSTRING File, LONG Flags,
   CLASSID ClassID, LONG TotalItems, LONG *Counter)
{
   parasol::Log log(__FUNCTION__);

   log.branch("Datatype: $%x, File: %s, Flags: $%x, Class: %d, Total Items: %d", Datatype, File, Flags, ClassID, TotalItems);

   ClipEntry clip, tmp;
   ClearMemory(&clip, sizeof(clip));

   if (!TotalItems) {
      log.msg("TotalItems parameter not specified.");
      return ERR_NullArgs;
   }

   ClipHeader *header;
   if (!AccessMemory(ClusterID, MEM_READ_WRITE, 3000, &header)) {
      auto clips = (ClipEntry *)(header + 1);

      if (Flags & CEF_EXTEND) {
         // Search for an existing clip that matches the requested datatype
         for (WORD i=0; i < MAX_CLIPS; i++) {
            if (clips[i].Datatype IS Datatype) {
               log.msg("Extending existing clip record for datatype $%x.", Datatype);

               ERROR error = ERR_Okay;

               // We have found a matching datatype.  Start by moving the clip to the front of the queue.

               CopyMemory(clips+i, &tmp, sizeof(ClipEntry)); // TODO We need to shift the list down, not swap entries
               CopyMemory(clips, clips+i, sizeof(ClipEntry));
               CopyMemory(&tmp, clips, sizeof(ClipEntry));

               // Extend the existing clip with the new items/file

               if (File) {
                  if (clips->Files) {
                     STRING str;
                     if (!AccessMemory(clips->Files, MEM_READ_WRITE, 3000, &str)) {
                        MEMINFO meminfo;
                        if (!MemoryIDInfo(clips->Files, &meminfo)) {
                           if (!ReallocMemory(str, meminfo.Size + StrLength(File) + 1, &str, &clips->Files)) {
                              StrCopy(File, str + meminfo.Size, COPY_ALL);
                              clips->TotalItems += TotalItems;
                           }
                           else error = ERR_ReallocMemory;
                        }
                        else error = ERR_MemoryInfo;

                        ReleaseMemory(str);
                     }
                     else error = ERR_AccessMemory;
                  }
               }
               else {
                  if (Datatype IS DATA_FILE) {
                     log.warning("DATA_FILE datatype used, but a specific file path was not provided.");
                     error = ERR_Failed;
                  }
                  else clips->TotalItems += TotalItems; // Virtual file name
               }

               if (Counter) *Counter = clips->ID;

               ReleaseMemory(header);
               return error;
            }
         }
      }

      // If a file string was specified, copy it to the clip entry

      if (File) {
         STRING str;
         LONG len = StrLength(File) + 1;
         if (!AllocMemory(len, MEM_STRING|MEM_NO_CLEAR|MEM_PUBLIC|MEM_UNTRACKED, &str, &clip.Files)) {
            CopyMemory(File, str, len);
            ReleaseMemory(str);
         }
         else {
            ReleaseMemory(header);
            return ERR_AllocMemory;
         }
      }

      // Set the clip details

      clip.Datatype   = Datatype;
      clip.Flags      = Flags & CEF_DELETE;
      clip.ClassID    = ClassID;
      clip.TotalItems = TotalItems;
      clip.ID         = ++header->Counter;
      if (Counter) *Counter = clip.ID;

      // Remove any existing clips that match this datatype

      for (WORD i=0; i < MAX_CLIPS; i++) {
         if (clips[i].Datatype IS Datatype) free_clip(&clips[i]);
      }

      // Remove the oldest clip if the history buffer is full.

      if (clips[MAX_CLIPS-1].Datatype) {
         free_clip(&clips[MAX_CLIPS-1]);
      }

      // Insert the new clip entry at start of the history buffer

      CopyMemory(clips, clips+1, MAX_CLIPS-1);
      CopyMemory(&clip, clips, sizeof(clip));

      ReleaseMemory(header);
      return ERR_Okay;
   }
   else return ERR_AccessMemory;
}

//****************************************************************************
// Called when the windows clipboard holds new text.  We respond by copying this into our internal clipboard system.

#ifdef _WIN32
#ifdef  __cplusplus
extern "C" void report_windows_clip_text(CSTRING String)
#else
void report_windows_clip_text(CSTRING String)
#endif
{
   parasol::Log log("Clipboard");
   objClipboard *clipboard;

   log.branch("Windows has received text on the clipboard.");

   if (!CreateObject(ID_CLIPBOARD, 0, &clipboard, FID_Flags|TLONG, CLF_HOST, TAGEND)) {
      clipAddText(clipboard, String);
      acFree(clipboard);
   }
   else log.warning(ERR_CreateObject);
}
#endif

//****************************************************************************
// Called when the windows clipboard holds new file references.

#ifdef _WIN32
#ifdef  __cplusplus
extern "C" void report_windows_files(APTR Data, LONG CutOperation)
#else
void report_windows_files(APTR Data, LONG CutOperation)
#endif
{
   parasol::Log log("Clipboard");

   log.branch("Windows has received files on the clipboard.  Cut: %d", CutOperation);

   APTR lock;
   if (!AccessMemory(RPM_Clipboard, MEM_READ_WRITE, 3000, &lock)) {
      char path[256];
      for (LONG i=0; winExtractFile(Data, i, path, sizeof(path)); i++) {
         add_clip(RPM_Clipboard, CLIPTYPE_FILE, path, (i ? CEF_EXTEND : 0) | (CutOperation ? CEF_DELETE : 0), 0, 1, 0);
      }
      ReleaseMemory(lock);
   }
}
#endif

//****************************************************************************

#ifdef _WIN32
#ifdef  __cplusplus
extern "C" void report_windows_hdrop(STRING Data, LONG CutOperation)
#else
void report_windows_hdrop(STRING Data, LONG CutOperation)
#endif
{
   parasol::Log log("Clipboard");

   log.branch("Windows has received files on the clipboard.  Cut: %d", CutOperation);

   APTR lock;
   if (!AccessMemory(RPM_Clipboard, MEM_READ_WRITE, 3000, &lock)) {
      for (LONG i=0; *Data; i++) {
         add_clip(RPM_Clipboard, CLIPTYPE_FILE, Data, (i ? CEF_EXTEND : 0) | (CutOperation ? CEF_DELETE : 0), 0, 1, 0);
         while (*Data) Data++; // Go to next file path
         Data++; // Skip null byte
      }
      ReleaseMemory(lock);
   }
}
#endif

//****************************************************************************
// Called when the windows clipboard holds new text in UTF-16 format.

#ifdef _WIN32
#ifdef  __cplusplus
extern "C" void report_windows_clip_utf16(UWORD *String)
#else
void report_windows_clip_utf16(UWORD *String)
#endif
{
   parasol::Log log("Clipboard");

   log.branch("Windows has received unicode text on the clipboard.");

   objClipboard *clipboard;
   if (!CreateObject(ID_CLIPBOARD, 0, &clipboard, FID_Flags|TLONG, CLF_HOST, TAGEND)) {
      LONG u8len = 0;
      for (LONG chars=0; String[chars]; chars++) {
         if (String[chars] < 128) u8len++;
         else if (String[chars] < 0x800) u8len += 2;
         else u8len += 3;
      }

      STRING u8str;
      if (!AllocMemory(u8len+1, MEM_STRING|MEM_NO_CLEAR, &u8str, NULL)) {
         LONG i = 0;
         for (LONG chars=0; String[chars]; chars++) {
            auto value = String[chars];
            if (value < 128) {
               u8str[i++] = (UBYTE)value;
            }
            else if (value < 0x800) {
               u8str[i+1] = (value & 0x3f) | 0x80;
               value  = value>>6;
               u8str[i] = value | 0xc0;
               i += 2;
            }
            else {
               u8str[i+2] = (value & 0x3f)|0x80;
               value  = value>>6;
               u8str[i+1] = (value & 0x3f)|0x80;
               value  = value>>6;
               u8str[i] = value | 0xe0;
               i += 3;
            }
         }
         u8str[i] = 0;

         clipAddText(clipboard, u8str);
         FreeResource(u8str);
      }
      acFree(clipboard);
   }
   else log.warning(ERR_CreateObject);
}
#endif

#include "class_clipboard_def.c"

static const FieldArray clFields[] = {
   { "Flags",            FDF_LONGFLAGS|FDF_RI,       (MAXINT)&clClipboardFlags, NULL, NULL },
   { "Cluster",          FDF_LONG|FDF_RW,            0, NULL, NULL },
   { "RequestHandler",   FDF_FUNCTIONPTR|FDF_RW,     0, (APTR)GET_RequestHandler, (APTR)SET_RequestHandler },
   END_FIELD
};

//****************************************************************************

ERROR init_clipboard(void)
{
   MEMORYID memoryid = RPM_Clipboard;
   AllocMemory(sizeof(ClipHeader) + (MAX_CLIPS * sizeof(ClipEntry)), MEM_UNTRACKED|MEM_PUBLIC|MEM_RESERVED|MEM_NO_BLOCKING, NULL, &memoryid);

   if (CreateObject(ID_METACLASS, 0, &clClipboard,
         FID_BaseClassID|TLONG,   ID_CLIPBOARD,
         FID_ClassVersion|TFLOAT, VER_CLIPBOARD,
         FID_Name|TSTR,           "Clipboard",
         FID_Category|TLONG,      CCF_IO,
         FID_Actions|TPTR,        clClipboardActions,
         FID_Methods|TARRAY,      clClipboardMethods,
         FID_Fields|TARRAY,       clFields,
         FID_Size|TLONG,          sizeof(objClipboard),
         FID_Path|TSTR,           MOD_PATH,
         TAGEND)) {
      return ERR_AddClass;
   }

#ifdef _WIN32

   // If this is the first initialisation of the clipboard module, we need to copy the current Windows clipboard
   // content into our clipboard.

   ClipHeader *clipboard;
   if (!AccessMemory(RPM_Clipboard, MEM_READ_WRITE, 3000, &clipboard)) {
      if (!clipboard->Init) {
         parasol::Log log;
         log.branch("Populating clipboard for the first time from the Windows host.");

         if (!winInit()) {
            clipboard->Init = TRUE;
            winCopyClipboard();
         }
         else log.warning(ERR_SystemCall);
      }
      ReleaseMemory(clipboard);
   }

#endif

   return ERR_Okay;
}

void free_clipboard(void)
{
#ifdef _WIN32
   parasol::Log log(__FUNCTION__);
   log.extmsg("Terminating Windows clipboard resources.");
   winTerminate();
#endif

   if (clClipboard) { acFree(clClipboard); clClipboard = NULL; }
}
