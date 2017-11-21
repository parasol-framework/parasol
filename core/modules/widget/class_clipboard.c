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
#include <parasol/modules/widget.h>

#ifdef _WIN32
#define WINDOWS_WINDOWS_H
#include "platform/windows.c"
#endif

#include "defs.h"

static objMetaClass *clClipboard = NULL;

static const struct ActionArray clClipboardActions[];
static const struct MethodArray clClipboardMethods[];

static const struct FieldDef glDatatypes[] = {
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

static const struct FieldArray clFields[];

static ERROR add_clip(objClipboard *, LONG, CSTRING, LONG, CLASSID, LONG, LONG *);
static void free_clip(objClipboard *, struct ClipEntry *);
static LONG delete_feedback(struct FileFeedback *);
static LONG paste_feedback(struct FileFeedback *);
static void run_script(CSTRING, OBJECTID, CSTRING);
static ERROR CLIPBOARD_AddObjects(objClipboard *, struct clipAddObjects *);

//****************************************************************************

ERROR init_clipboard(void)
{
   MEMORYID memoryid = RPM_Clipboard;
   AllocMemory(sizeof(struct ClipHeader) + (MAX_CLIPS * sizeof(struct ClipEntry)), MEM_UNTRACKED|MEM_PUBLIC|MEM_RESERVED|MEM_NO_BLOCKING, NULL, &memoryid);

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

   // Initialise the windows clipboard handler.  This monitors the windows clipboard for new items and copies them into
   // our internal clipboard (the main cluster represented by RPM_Clipboard) when they appear.

   LogF("7","Initialise the Windows clipboard handler.");

   if (!winInit()) {
      // If this is the first initialisation of the clipboard module, we need to copy the current Windows clipboard
      // content into our clipboard.

      struct ClipHeader *clipboard;

      if (!AccessMemory(RPM_Clipboard, MEM_READ_WRITE, 3000, &clipboard)) {
         if (!clipboard->Init) {
            LogF("~","Populating clipboard for the first time.");
            clipboard->Init = TRUE;
            winCopyClipboard();
            LogBack();
         }
         ReleaseMemory(clipboard);
      }
   }
   else return PostError(ERR_SystemCall);

#endif

   return ERR_Okay;
}

void free_clipboard(void)
{
#ifdef _WIN32
   LogF("7","Terminating Windows clipboard resources.");
   winTerminate();
#endif

   if (clClipboard) { acFree(clClipboard); clClipboard = NULL; }
}

//****************************************************************************

static CSTRING GetDatatype(LONG Datatype)
{
   WORD i;
   for (i=0; glDatatypes[i].Name; i++) {
      if (Datatype IS glDatatypes[i].Value) return (CSTRING)glDatatypes[i].Name;
   }

   return "unknown";
}

//****************************************************************************

static ERROR CLIPBOARD_ActionNotify(objClipboard *Self, struct acActionNotify *Args)
{
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
MissingLoation: The Files argument was not correctly specified.
LimitedSuccess: The file item was successfully added to the internal clipboard, but could not be added to the host.
-END-

*****************************************************************************/

static ERROR CLIPBOARD_AddFile(objClipboard *Self, struct clipAddFile *Args)
{
   if (!Args) return PostError(ERR_NullArgs);
   if ((!Args->Path) OR (!Args->Path[0])) return PostError(ERR_MissingPath);

   LogBranch("Path: %s", Args->Path);

   ERROR error = add_clip(Self, Args->Datatype, Args->Path, Args->Flags & (CEF_DELETE|CEF_EXTEND), 0, 1, 0);

#ifdef _WIN32
   if (!error) {
      struct ClipHeader *header;
      struct ClipEntry *clips;
      STRING str, win, path;
      LONG j, i, winpos;

      if (!AccessMemory(Self->ClusterID, MEM_READ_WRITE, 3000, &header)) {
         clips = (struct ClipEntry *)(header + 1);

         if (!AccessMemory(clips->Files, MEM_READ_WRITE, 3000, &str)) {
            // Build a list of resolved path names in a new buffer that is suitable for passing to Windows.

            if (!AllocMemory(512 * clips->TotalItems, MEM_DATA|MEM_NO_CLEAR, &win, NULL)) {
               j = 0;
               winpos = 0;
               for (i=0; i < clips->TotalItems; i++) {
                  if (!ResolvePath(str+j, 0, &path)) {
                     winpos += StrCopy(path, win+winpos, 511) + 1;
                     FreeMemory(path);
                  }

                  while (str[j]) j++;
                  j++;
               }
               win[winpos++] = 0; // An extra null byte is required to terminate the list for Windows HDROP

               if (winAddClip(CLIPTYPE_FILE, win, winpos, (Args->Flags & CEF_DELETE) ? TRUE : FALSE)) {
                  error = ERR_LimitedSuccess;
               }

               FreeMemory(win);
            }

            ReleaseMemory(str);
         }

         ReleaseMemory(header);
      }
   }
#endif

   LogBack();
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
   if (!Args) return PostError(ERR_NullArgs);

   struct clipAddObjects add;
   OBJECTID objects[2];

   objects[0] = Args->ObjectID;
   objects[1] = 0;
   add.Objects = &Args->ObjectID;
   add.Flags   = Args->Flags;
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
   if ((!Args) OR (!Args->Objects) OR (!Args->Objects[0])) return PostError(ERR_NullArgs);

   LogBranch(NULL);

   // Use the SaveToObject action to save each object's data to the clipboard storage area.  The class ID for each
   // object is also recorded.

   OBJECTPTR object;
   char location[100];
   LONG counter;
   WORD i, total;

   CLASSID classid  = 0;
   LONG datatype = 0;
   OBJECTID *list = Args->Objects;
   for (total=0; list[total]; total++);

   if (!add_clip(Self, datatype, 0, Args->Flags & CEF_EXTEND, 0, total, &counter)) {
      for (i=0; list[i]; i++) {
         if (!AccessObject(list[i], 5000, &object)) {
            if (!classid) classid = object->ClassID;

            if (classid IS object->ClassID) {
               datatype = Args->Datatype;

               if (!datatype) {
                  if (object->ClassID IS ID_PICTURE) {
                  StrFormat(location, sizeof(location), "clipboard:image%d.%.3d", counter, i);
                  datatype = CLIPTYPE_IMAGE;
                  }
                  else if (object->ClassID IS ID_SOUND) {
                     StrFormat(location, sizeof(location), "clipboard:audio%d.%.3d", counter, i);
                     datatype = CLIPTYPE_AUDIO;
                  }
                  else {
                     StrFormat(location, sizeof(location), "clipboard:object%d.%.3d", counter, i);
                     datatype = CLIPTYPE_OBJECT;
                  }
               }
               else {
                  // Use the specified datatype
                  StrFormat(location, sizeof(location), "clipboard:%s%d.%.3d", GetDatatype(datatype), counter, i);
               }

               SaveObjectToFile(object, location, 0);
            }

            ReleaseObject(object);
         }
      }
   }

   LogBack();
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
   ERROR error;

   if ((!Args) OR (!Args->String)) return PostError(ERR_NullArgs);
   if (!Args->String[0]) return ERR_Okay;

   #ifdef _WIN32
   {
      // Copy text to the windows clipboard.  This requires that we convert from UTF-8 to UTF-16.  For consistency and
      // interoperability purposes, we interact with both the Windows and internal clipboards.

      LONG chars, i;
      CSTRING str;
      UWORD *utf16;

      error = ERR_Okay;
      chars = UTF8Length(Args->String);
      if (!AllocMemory((chars+1) * sizeof(WORD), MEM_DATA|MEM_NO_CLEAR, &utf16, NULL)) {
         str = Args->String;
         i = 0;
         while (*str) {
            utf16[i++] = UTF8ReadValue(str, NULL);
            str += UTF8CharLength(str);
         }
         utf16[i] = 0;
         error = winAddClip(CLIPTYPE_TEXT, utf16, (chars+1) * sizeof(WORD), FALSE);
         FreeMemory(utf16);
      }
      else error = ERR_AllocMemory;

      if (error) return PostError(error);
   }
   #endif

   LogBranch(NULL);

   LONG counter;
   if (!(error = add_clip(Self, CLIPTYPE_TEXT, 0, 0, 0, 1, &counter))) {
      char buffer[200];
      StrFormat(buffer, sizeof(buffer), "clipboard:text%d.000", counter);

      OBJECTPTR file;
      if (!CreateObject(ID_FILE, 0, &file,
            FID_Location|TSTR,     buffer,
            FID_Flags|TLONG,       FL_NEW|FL_WRITE,
            FID_Permissions|TLONG, PERMIT_READ|PERMIT_WRITE,
            TAGEND)) {

         acWrite(file, Args->String, StrLength(Args->String), 0);

         acFree(file);
         LogBack();
         return ERR_Okay;
      }
      else return StepError(0, ERR_CreateFile);
   }
   else return StepError(0, error);
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
      DeleteFile(path);
      CreateFolder(path, PERMIT_READ|PERMIT_WRITE);
      FreeMemory(path);
   }

   // Annihilate all historical clip information

   struct ClipEntry *clips;
   if (!AccessMemory(Self->ClusterID, MEM_READ_WRITE, 3000, &clips)) {
      ClearMemory(&clips, sizeof(struct ClipHeader) + (MAX_CLIPS * sizeof(struct ClipEntry)));
      ReleaseMemory(clips);
      return ERR_Okay;
   }
   else return PostError(ERR_AccessMemory);
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
   OBJECTPTR file;
   UBYTE buffer[200];
   LONG counter;

   if (!Args) return PostError(ERR_NullArgs);

   if (Args->DataType IS DATA_TEXT) {
      LogMsg("Copying text to the clipboard.");

      #ifdef _WIN32
      {
         // Copy text to the windows clipboard.  This requires that we convert
         // from UTF-8 to UTF-16.  For consistency and interoperability purposes,
         // we interact with both the Windows and internal clipboards.

         UWORD *utf16;

         ERROR error = ERR_Okay;
         LONG chars = UTF8Length((STRING)Args->Buffer);
         STRING str = (STRING)Args->Buffer;

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
            FreeMemory(utf16);
         }
         else error = ERR_AllocMemory;

         if (error) return PostError(error);
      }
      #endif

      if (!add_clip(Self, CLIPTYPE_TEXT, 0, 0, 0, 1, &counter)) {
         StrFormat(buffer, sizeof(buffer), "clipboard:text%d.000", counter);

         if (!CreateObject(ID_FILE, 0, &file,
               FID_Location|TSTR,  buffer,
               FID_Flags|TLONG,       FL_NEW|FL_WRITE,
               FID_Permissions|TLONG, PERMIT_READ|PERMIT_WRITE,
               TAGEND)) {

            if (acWrite(file, Args->Buffer, Args->Size, 0) != ERR_Okay) {
               acFree(file);
               return PostError(ERR_Write);
            }

            acFree(file);
            return ERR_Okay;
         }
         else return PostError(ERR_CreateObject);
      }
      else return PostError(ERR_Failed);
   }
   else LogErrorMsg("Unrecognised data type %d.", Args->DataType);

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
Delete: Remove items from the clipboard.

The Delete method will clear all items that match a specified datatype.  Clear multiple datatypes by combining flags
in the Datatype parameter.  To clear all content from the clipboard, use the #Clear() action instead of this method.

-INPUT-
int(CLIPTYPE) Datatype: The datatype(s) that will be deleted (datatypes may be logically-or'd together).

-ERRORS-
Okay
NullArgs
AccessMemory: The clipboard memory data was not accessible.
-END-

*****************************************************************************/

static ERROR CLIPBOARD_Delete(objClipboard *Self, struct clipDelete *Args)
{
   if ((!Args) OR (!Args->Datatype)) return PostError(ERR_NullArgs);

   LogBranch("Datatype: $%x", Args->Datatype);

   struct ClipHeader *header;

   if (!AccessMemory(Self->ClusterID, MEM_READ_WRITE, 3000, &header)) {
      struct ClipEntry *clips = (struct ClipEntry *)(header + 1);
      WORD i;
      for (i=0; i < MAX_CLIPS; i++) {
         if (clips[i].Datatype & Args->Datatype) {
            if (i IS 0) {
               #ifdef _WIN32
               winClearClipboard();
               #endif
            }
            free_clip(Self, &clips[i]);
         }
      }

      ReleaseMemory(header);
      LogBack();
      return ERR_Okay;
   }
   else return StepError(0, ERR_AccessMemory);
}

/*****************************************************************************

-METHOD-
DeleteFiles: Deletes all files that have been referenced in the clipboard.

The DeleteFiles method is used to implement file-delete operations for the CLIPTYPE_FILE datatype in a convenient way for
the developer.

A progress window will popup during the delete operation if it takes an extended period of time to complete (typically
over 0.5 seconds).  Any dialog windows will open on whatever surface you specify as the Target (this should be set to
either the Desktop object, or a screen that you have created).  If the Target is set to NULL, the routine will discover
the most appropriate space for opening new windows.  If the Target is set to -1, the routine will delete all files
without progress information or telling the user about encountered errors.

-INPUT-
oid Target: Set this parameter to a valid surface and the paste operation will open interactive windows (such as progress bars and user confirmations) during the paste operation.  This is usually set to the "Desktop" object, which is also the default if this argument is NULL.  Use a value of -1 if you don't want to provide feedback to the user.

-ERRORS-
Okay: The paste operation was performed successfully (this code is returned even if there are no files to be pasted).
Args: Invalid arguments were specified.
File: A non-specific file error occurred during the paste operation.
-END-

*****************************************************************************/

static ERROR CLIPBOARD_DeleteFiles(objClipboard *Self, struct clipDeleteFiles *Args)
{
   struct clipGetFiles get;
   struct rkFunction callback;
   OBJECTPTR dialog;
   STRING *list;
   ERROR error;
   WORD i;

   LogBranch(NULL);

   Self->Response = RSP_YES;

   Self->ProgressTime = PreciseTime();

   if (Args) Self->ProgressTarget = Args->TargetID;
   else Self->ProgressTarget = 0;

   if (Self->ProgressDialog) { acFree(Self->ProgressDialog); Self->ProgressDialog = NULL; }

   get.Datatype = CLIPTYPE_FILE;
   if (!Action(MT_ClipGetFiles, Self, &get)) {
      error = ERR_Okay;
      list = get.Files;
      for (i=0; list[i]; i++) {
         StrCopy(list[i], Self->LastFile, sizeof(Self->LastFile));

         SET_FUNCTION_STDC(callback, (APTR)&delete_feedback);
         FileFeedback(&callback, Self, 0);

         error = DeleteFile(list[i]);

         if (error IS ERR_Cancelled) {
            error = ERR_Okay;
            break;
         }
         else if (error) {
            if (Self->ProgressTarget != -1) {
               char buffer[256];

               if (error IS ERR_Permissions) {
                  StrFormat(buffer, sizeof(buffer), "You do not have the necessary permissions to delete this file:\n\n%s", Self->LastFile);
               }
               else StrFormat(buffer, sizeof(buffer), "An error occurred while deleting this file:\n\n%s\n\n%s.  Process cancelled.", Self->LastFile, GetErrorMsg(error));

               if (!CreateObject(ID_DIALOG, 0, &dialog,
                     FID_Owner|TLONG,  Self->Head.TaskID,
                     FID_Target|TLONG, Self->ProgressTarget,
                     FID_Type|TSTR,    "ERROR",
                     FID_Options|TSTR, "Okay",
                     FID_Title|TSTR,   "File Delete Failure",
                     FID_String|TSTR,  buffer,
                     FID_Flags|TSTR,   (Self->Flags & CLF_WAIT) ? "WAIT" : NULL,
                     TAGEND)) {
                  acShow(dialog);
               }
            }

            error = ERR_File;
            break;
         }
      }

      if (Self->ProgressDialog) {
         acFree(Self->ProgressDialog);
         Self->ProgressDialog = NULL;
      }

      FreeMemory(list);
      LogBack();
      return error;
   }
   else {
      LogMsg("There are no files to delete from the clipboard.");
      LogBack();
      return ERR_Okay;
   }
}

//****************************************************************************

static ERROR CLIPBOARD_Free(objClipboard *Self, APTR Void)
{
   if (Self->ProgressDialog) { acFree(Self->ProgressDialog); Self->ProgressDialog = NULL; }
   if (Self->ClusterAllocated) { FreeMemoryID(Self->ClusterID); Self->ClusterID = 0; }
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
GetFiles: Retrieve the most recently clipped data as a list of files.

To retrieve items from a clipboard, you must use the GetFiles method.  You must declare the types of data that you
support (or NULL if you recognise all datatypes) so that you are returned a recognisable data format.

The most recent clip is always returned, so if you support text, audio and image clips and the most recent clip is an
audio file, any text and image clips on the history buffer will be ignored by this routine. If you want to scan all
available clip items, set the Datatype parameter to NULL and repeatedly call this method with incremented Index numbers
until the error code ERR_OutOfRange is returned.

On success this method will return a list of files (terminated with a NULL entry) in the Files parameter.  Each file is
a readable clipboard entry - how you read it depends on the resulting Datatype.  Additionally, the IdentifyFile()
function could be used to find a class that supports the data.  The resulting Files array is a memory allocation that
must be freed with a call to ~Core.FreeMemory().

If this method returns the CEF_DELETE flag in the Flags parameter, you must delete the source files after successfully
copying out the data.  If you are not successful in your operation, do not proceed with the deletion or the user will
have lost the original data.  When cutting and pasting files within the file system, simply using the file system's
'move file' functionality may be useful for fast file transfer.

-INPUT-
int(CLIPTYPE) Datatype: The types of data that you recognise for retrieval are specified here (for example CLIPTYPE_TEXT, CLIPTYPE_AUDIO, CLIPTYPE_FILE...).  You may logical-OR multiple data types together.  This parameter will be updated to reflect the retrieved data type when the method returns.
int Index: If the Datatype parameter is NULL, this parameter may be set to the index of the clip item that you want to retrieve.
!array(str) Files: The resulting location(s) of the requested clip data are returned in this parameter; terminated with a NULL entry.  You are required to free the returned array with FreeMemory().
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
   if (!Args) return PostError(ERR_NullArgs);

   LogMsg("Datatype: $%.8x", Args->Datatype);

   Args->Files = NULL;

   // Find the first clipboard entry to match what has been requested

   struct ClipHeader *header;
   if (!AccessMemory(Self->ClusterID, MEM_READ_WRITE, 3000, &header)) {
      struct ClipEntry *clips = (struct ClipEntry *)(header + 1);

      UBYTE buffer[100];
      STRING files, str;
      ERROR error;
      WORD index, i, j;
      LONG len;

      if (!Args->Datatype) {
         // Retrieve the most recent clip item, or the one indicated in the Index parameter.

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

      if (clips[index].TotalItems < 1) {
         LogErrorMsg("No items are set for datatype $%x at clip index %d", clips[index].Datatype, index);
         ReleaseMemory(header);
         return ERR_NoData;
      }

      STRING *list = NULL;
      if (clips[index].Files) {
         if (!(error = AccessMemory(clips[index].Files, MEM_READ, 3000, &files))) {
            MEMINFO info;
            if (!MemoryIDInfo(clips[index].Files, &info)) {
               if (!AllocMemory(((clips[index].TotalItems+1) * sizeof(STRING)) + info.Size, MEM_DATA, &list, NULL)) {
                  CopyMemory(files, list + clips[index].TotalItems + 1, info.Size);
               }
               else {
                  ReleaseMemory(files);
                  ReleaseMemory(header);
                  return PostError(ERR_AllocMemory);
               }
            }
            ReleaseMemory(files);
         }
         else {
            LogErrorMsg("Failed to access file string #%d, error %d.", clips[index].Files, error);
            if (error IS ERR_MemoryDoesNotExist) clips[index].Files = 0;
            ReleaseMemory(header);
            return PostError(ERR_AccessMemory);
         }
      }
      else {
         if (clips[index].Datatype IS CLIPTYPE_FILE) {
            LogErrorMsg("File datatype detected, but no file list has been set.");
            ReleaseMemory(header);
            return ERR_Failed;
         }

         // Calculate the length of the file strings

         len = StrFormat(buffer, sizeof(buffer), "clipboard:%s%d.000", GetDatatype(clips[index].Datatype), clips[index].ID);
         len = ((len + 1) * clips[index].TotalItems) + 1;

         // Allocate the list array with some room for the strings at the end and then fill it with data.

         if (!AllocMemory(((clips[index].TotalItems+1) * sizeof(STRING)) + len, MEM_DATA|MEM_CALLER, &list, NULL)) {
            str = (STRING)(list + clips[index].TotalItems + 1);
            for (i=0; i < clips[index].TotalItems; i++) {
               if (i > 0) *str++ = '\0'; // Each file is separated with a NULL character
               StrFormat(str, len, "clipboard:%s%d.%.3d", GetDatatype(clips[index].Datatype), clips[index].ID, i);
               while (*str) str++;
            }
            *str = 0;
         }
         else {
            ReleaseMemory(header);
            return PostError(ERR_AllocMemory);
         }
      }

      // Setup the pointers in the string list

      str = (STRING)(list + clips[index].TotalItems + 1);
      j = 0;
      for (i=0; i < clips[index].TotalItems; i++) {
         LogMsg("Analysis: %d:%d: %s", i, j, str + j);
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
   else return PostError(ERR_AccessMemory);
}

/*****************************************************************************

-ACTION-
GetVar: Special field types are supported via unlisted fields.

The following unlisted field types are supported by the Clipboard class:

<fields>
<fld type="STRING" name="File(Datatype,Index)">Where Datatype is a recognised data format (e.g. TEXT) and Index is between 0 and the Items() field.  If you don't support multiple clipped items, use an index of zero.  On success, this field will return a file location that points to the clipped data.</>
<fld type="NUMBER" name="Items(Datatype)">Returns the total number of items available for the specified data type.</>
</fields>

-END-

*****************************************************************************/

static ERROR CLIPBOARD_GetVar(objClipboard *Self, struct acGetVar *Args)
{
   struct ClipHeader *header;
   struct ClipEntry *clip, *clips;
   char datatype[20], index[20];
   STRING files;
   WORD i, j, total;
   LONG value;

   if (!Args) return PostError(ERR_NullArgs);

   if ((!Args->Field) OR (!Args->Buffer) OR (Args->Size < 1)) {
      return PostError(ERR_Args);
   }

   if (!(Self->Head.Flags & NF_INITIALISED)) return PostError(ERR_Failed);

   Args->Buffer[0] = 0;

   if (!StrCompare("File(", Args->Field, 0, 0)) {
      // Extract the datatype

      for (j=0, i=6; (Args->Field[i]) AND (Args->Field[i] != ',') AND (Args->Field[i] != ')'); i++) datatype[j++] = Args->Field[i];
      datatype[j] = 0;

      // Convert the datatype string into its equivalent value

      value = 0;
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

      if (!AccessMemory(Self->ClusterID, MEM_READ_WRITE, 3000, &header)) {
         // Find the clip for the requested datatype

         clips = (struct ClipEntry *)(header + 1);

         clip = NULL;
         for (i=0; i < MAX_CLIPS; i++) {
            if (clips[i].Datatype IS value) {
               clip = &clips[i];
               break;
            }
         }

         if (clip) {
            i = StrToInt(index);
            if ((i >= 0) AND (i < clip->TotalItems)) {
               if (clip->Files) {
                  if (!AccessMemory(clip->Files, MEM_READ, 3000, &files)) {
                     // Find the file path that we're looking for

                     j = 0;
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
                     return PostError(ERR_AccessMemory);
                  }
               }
               else StrFormat(Args->Buffer, Args->Size, "clipboard:%s%d.%.3d", datatype, clip->ID, i);
            }
         }

         ReleaseMemory(header);
         return ERR_Okay;
      }
      else return PostError(ERR_AccessMemory);
   }
   else if (!StrCompare("Items(", Args->Field, 0, 0)) {
      // Extract the datatype
      for (j=0, i=6; (Args->Field[i]) AND (Args->Field[i] != ')'); i++) datatype[j++] = Args->Field[i];
      datatype[j] = 0;

      // Convert the datatype string into its equivalent value

      value = 0;
      for (i=0; glDatatypes[i].Name; i++) {
         if (!StrMatch(datatype, glDatatypes[i].Name)) {
            value = glDatatypes[i].Value;
            break;
         }
      }

      // Calculate the total number of items available for this datatype

      total = 0;
      if (value) {
         if (!AccessMemory(Self->ClusterID, MEM_READ, 3000, &header)) {
            clips = (struct ClipEntry *)(header + 1);
            for (i=0; i < MAX_CLIPS; i++) {
               if (clips[i].Datatype IS value) {
                  total = clips[i].TotalItems;
                  break;
               }
            }
            ReleaseMemory(header);
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
   if (!Self->ClusterID) {
      // Create a new grouping for this clipboard.  It will be possible for any other clipboard to attach itself to
      // this memory block if the ID is known.

      if (!AllocMemory(sizeof(struct ClipHeader) + (MAX_CLIPS * sizeof(struct ClipEntry)), MEM_PUBLIC|MEM_NO_BLOCKING, NULL, &Self->ClusterID)) {
         Self->ClusterAllocated = TRUE;
      }
      else return PostError(ERR_AllocMemory);
   }

   // Create a directory under temp: to store clipboard data

   CreateFolder("clipboard:", PERMIT_READ|PERMIT_WRITE);

   // Scan the clipboard: directory for existing clips.  If they exist, add them to our clipboard object.  This allows
   // the user to continue using clipboard data from the last time that the machine was powered on.

/*
   struct DirInfo *dir = NULL;
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

-METHOD-
PasteFiles: Performs the 'paste' operation on file items.

The PasteFiles method is used to implement file-paste operations for the CLIPTYPE_FILE datatype in a convenient way for the
developer.  The correct handling for both copy and cut operations is implemented for you.

You are required to specify a destination path for the file items.  This should be a directory (denoted by a trailing
slash or colon character), although you may specify a single target file if you wish.

Confirmation windows are automatically handled by this routine when duplicate file names or errors are encountered
during processing.  Any windows will open on whatever surface you specify as the Target (this should be set to either
the Desktop object, or a screen that you have created).  If the Target is set to NULL, the routine will discover the
most appropriate space for opening new windows. If the Target is set to -1, the routine will overwrite all files by
default and not inform the user if errors are encountered.

-INPUT-
cstr Dest: The destination path for the file items.  Must be identifiable as a directory location, otherwise the file(s) will be copied to a destination file.
oid Target: Set this parameter to a valid surface and the paste operation will open interactive windows (such as progress bars and user confirmations) during the paste operation.  This is usually set to the "Desktop" object, which is also the default if this argument is NULL.  Use a value of -1 if you don't want to provide feedback to the user.

-ERRORS-
Okay: The paste operation was performed successfully (this code is returned even if there are no files to be pasted).
Args
NullArgs
MissingPath
File: A non-specific file error occurred during the paste operation.
-END-

*****************************************************************************/

static CSTRING PasteConfirm = "STRING:\n\
   local clip = obj.find(arg('clipboard'))\n\
   local dlg = obj.new('dialog', {\n\
      name='PasteConfirm', type='attention'\n\
      buttons='cancel;no;yesall;yes', title='Confirmation Required'\n\
      flags='!WAIT', responseObject='[@scriptowner]', responseField='response'\n\
   })\n\
   dlg.string = 'You are about to overwrite this location - should it be replaced?\n\n' .. arg('file','NIL')\n\
   dlg.detach()\n\
   dlg.acShow()\n";

static ERROR CLIPBOARD_PasteFiles(objClipboard *Self, struct clipPasteFiles *Args)
{
   struct clipGetFiles get;
   struct rkFunction callback;
   OBJECTID dialogid;
   OBJECTPTR dialog;
   STRING *list;
   char dest[512];
   LONG type;
   ERROR error;
   WORD i, findex, dindex;

   if (!Args) return PostError(ERR_NullArgs);
   if ((!Args->Dest) OR (!Args->Dest[0])) return PostError(ERR_MissingPath);

   LogBranch("Dest: %s, Target: %d", Args->Dest, Args->TargetID);

   UBYTE remove_clip = FALSE;
   Self->Response = RSP_YES;
   Self->ProgressTime = PreciseTime();
   Self->ProgressTarget = Args->TargetID;

   if (Self->ProgressDialog) { acFree(Self->ProgressDialog); Self->ProgressDialog = NULL; }

   get.Datatype = CLIPTYPE_FILE;
   if (!Action(MT_ClipGetFiles, Self, &get)) {
      // Scan the file list and move or copy each file to the destination directory

      error = ERR_Okay;
      list = get.Files;
      for (i=0; list[i]; i++) {
         if (Self->Response != RSP_YES_ALL) Self->Response = RSP_YES; // Reset Response to YES default

         // Determine the index at which the source filename starts

         for (findex=0; list[i][findex]; findex++);
         if ((list[i][findex-1] IS '/') OR (list[i][findex-1] IS ':') OR (list[i][findex-1] IS '\\')) findex--;
         while ((findex > 0) AND (list[i][findex-1] != '/') AND (list[i][findex-1] != ':') AND (list[i][findex-1] != '\\')) findex--;

         // Copy the source's filename to the end of the destination path

         dindex = StrCopy(Args->Dest, dest, sizeof(dest)-1);
         if ((dest[dindex-1] IS '/') OR (dest[dindex-1] IS '\\') OR (dest[dindex-1] IS ':')) {
            StrCopy(list[i]+findex, dest+dindex, sizeof(dest)-dindex);
         }

         // Check if the destination already exists

         if ((Self->Response != RSP_YES_ALL) AND (Self->ProgressTarget != -1)) {
            if (!StrMatch(list[i], dest)) {
               if (get.Flags & CEF_DELETE) {
                  continue; // Do nothing for move operations when the source and destination are identical
               }
               else {
                  // The source and destination strings are exactly identical.  In this case, the destination will be
                  // "Copy of [Source File]"

                  if ((dest[dindex] >= 'A') AND (dest[dindex] <= 'Z')) dindex += StrCopy("Copy of ", dest+dindex, sizeof(dest)-dindex);
                  else dindex += StrCopy("copy of ", dest+dindex, sizeof(dest)-dindex);

                  dindex += StrCopy(list[i]+findex, dest+dindex, sizeof(dest)-dindex);
               }
            }

            // Request user confirmation

            STRING path;

            if (!ResolvePath(dest, RSF_NO_FILE_CHECK, &path)) { // Resolve to avoid multi-directory assign problems
               if ((!AnalysePath(path, &type)) AND ((type IS LOC_FILE) OR (type IS LOC_DIRECTORY))) {
                  Self->Response = RSP_CANCEL;
                  run_script(PasteConfirm, Self->ProgressTarget, dest);
                  if (!Self->ProgressDialog) Self->ProgressTime = PreciseTime(); // Reset the start time whenever a user dialog is presented
               }

               FreeMemory(path);
            }
         }

         if (Self->Response IS RSP_CANCEL) break;

         if ((Self->Response != RSP_YES) AND (Self->Response != RSP_YES_ALL)) {
            LogMsg("Skipping file %s, response %d", list[i], Self->Response);
            continue;
         }

         // Copy/Move the file

         while (dest[dindex]) dindex++;
         if ((dest[dindex-1] IS '/') OR (dest[dindex-1] IS '\\')) dest[dindex-1] = 0;

         SET_FUNCTION_STDC(callback, (APTR)&paste_feedback);
         FileFeedback(&callback, Self, 0);

         if (get.Flags & CEF_DELETE) {
            if (!(error = MoveFile(list[i], dest))) {
               remove_clip = TRUE;
            }
         }
         else error = CopyFile(list[i], dest);

         // If an error occurred, tell the user and stop the pasting process

         if (error IS ERR_Cancelled) {
            error = ERR_Okay;
            break;
         }
         else if (error) {
            LogErrorMsg("Error during paste operation [%d]: %s", error, GetErrorMsg(error));

            if (Self->ProgressTarget != -1) {
               char buffer[256];

               if (error IS ERR_OutOfSpace) {
                  StrFormat(buffer, sizeof(buffer), "There is not enough space in the destination drive to copy this file:\n\n%s", list[i]);
               }
               else if (error IS ERR_Permissions) {
                  StrFormat(buffer, sizeof(buffer), "You do not have the necessary permissions to copy this file:\n\n%s", list[i]);
               }
               else StrFormat(buffer, sizeof(buffer), "An error occurred while copying this file:\n\n%s\n\n%s.  Process cancelled.", list[i], GetErrorMsg(error));

               if (!CreateObject(ID_DIALOG, 0, &dialog,
                     FID_Owner|TLONG,  Self->Head.TaskID,
                     FID_Target|TLONG, Self->ProgressTarget,
                     FID_Type|TSTR,    "ERROR",
                     FID_Options|TSTR, "Okay",
                     FID_Title|TSTR,   "File Paste Failure",
                     FID_String|TSTR,  buffer,
                     FID_Flags|TSTR,   (Self->Flags & CLF_WAIT) ? "WAIT" : NULL,
                     TAGEND)) {
                  acShow(dialog);
               }
            }

            error = ERR_File;
            break;
         }
      }

      if (!FastFindObject("PasteConfirm", ID_DIALOG, &dialogid, 1, 0)) {
         acFreeID(dialogid);
      }

      if (Self->ProgressDialog) { acFree(Self->ProgressDialog); Self->ProgressDialog = NULL; }

      if (remove_clip) clipDelete(Self, CLIPTYPE_FILE); // For cut-paste operations

      FreeMemory(list);
      LogBack();
      return error;
   }
   else {
      LogMsg("There are no files to paste from the clipboard.");
      LogBack();
      return ERR_Okay;
   }
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
Response: Contains the last value received from a user request window.

If a clipboard operation requires a user confirmation request, this field will contain the response value.  The user
will typically be asked for a response prior to delete operations and in cases of existing file replacement.

For available response values please refer to the <class>Dialog</class> class.
-END-

*****************************************************************************/

static LONG delete_feedback(struct FileFeedback *Feedback)
{
   objClipboard *Self = (objClipboard *)Feedback->User;

   LONG len = StrLength(Feedback->Path);
   if (len >= (WORD)sizeof(Self->LastFile)) {
      Self->LastFile[0] = '.'; Self->LastFile[1] = '.'; Self->LastFile[2] = '.';
      StrCopy(Feedback->Path + len - (sizeof(Self->LastFile) - 4), Self->LastFile+3, sizeof(Self->LastFile)-3);
   }
   else StrCopy(Feedback->Path, Self->LastFile, sizeof(Self->LastFile));

   if (Self->ProgressTarget IS -1) return FFR_OKAY;

   if (Self->ProgressDialog) {
      if (Self->ProgressDialog->Response IS RSF_CANCEL) return FFR_ABORT;

      if (Self->ProgressDialog->Response IS RSF_CLOSED) {
         // If the window was closed then continue deleting files, don't bother the user with further messages.

         acFree(Self->ProgressDialog);
         Self->ProgressDialog = NULL;
         Self->ProgressTarget = -1;
         return FFR_OKAY;
      }
   }

   // If the file processing exceeds a set time period, popup a progress window

   if ((!Self->ProgressDialog) AND ((PreciseTime() - Self->ProgressTime) > 500000LL)) {
      if (!CreateObject(ID_DIALOG, NF_INTEGRAL, &Self->ProgressDialog,
            FID_Owner|TLONG,  Self->Head.UniqueID,
            FID_Target|TLONG, Self->ProgressTarget,
            FID_Title|TSTR,   "File Deletion Progress",
            FID_Image|TSTR,   "icons:tools/eraser(48)",
            FID_Options|TSTR, "Cancel",
            FID_String|TSTR,  "Deleting...",
            TAGEND)) {
         acShow(Self->ProgressDialog);
      }
   }

   if (!Self->ProgressDialog) return FFR_OKAY;

   STRING str = Feedback->Path;
   LONG i = len;
   while ((i > 0) AND (str[i-1] != '/') AND (str[i-1] != '\\') AND (str[i-1] != ':')) i--;

   char buffer[256];
   StrFormat(buffer, sizeof(buffer), "Deleting: %s", str+i);
   SetString(Self->ProgressDialog, FID_String, buffer);
   Self->ProgressTime = PreciseTime();

   ProcessMessages(0, 0);

   return FFR_OKAY;
}

//****************************************************************************

static LONG paste_feedback(struct FileFeedback *Feedback)
{
   char buffer[256];

   objClipboard *Self = (objClipboard *)Feedback->User;

   LONG len = StrLength(Feedback->Path);
   if (len >= (WORD)sizeof(Self->LastFile)) {
      Self->LastFile[0] = '.'; Self->LastFile[1] = '.'; Self->LastFile[2] = '.';
      StrCopy(Feedback->Path + len - (sizeof(Self->LastFile) - 4), Self->LastFile+3, sizeof(Self->LastFile)-3);
   }
   else StrCopy(Feedback->Path, Self->LastFile, sizeof(Self->LastFile));

   if (Self->ProgressTarget IS -1) return FFR_OKAY;

   if (Self->ProgressDialog) {
      if (Self->ProgressDialog->Response IS RSF_CANCEL) return FFR_ABORT;

      if (Self->ProgressDialog->Response IS RSF_CLOSED) {
         // If the window was closed then continue deleting files, don't bother the user with further messages.

         acFree(Self->ProgressDialog);
         Self->ProgressDialog = NULL;
         Self->ProgressTarget = -1;
         return FFR_OKAY;
      }
   }

   // If the file processing exceeds a set time period, popup a progress window

   if ((!Self->ProgressDialog) AND ((PreciseTime() - Self->ProgressTime) > 500000LL)) {
      if (!CreateObject(ID_DIALOG, NF_INTEGRAL, &Self->ProgressDialog,
            FID_Owner|TLONG,  Self->Head.UniqueID,
            FID_Target|TLONG, Self->ProgressTarget,
            FID_Title|TSTR,   "File Transfer Progress",
            FID_Image|TSTR,   "icons:tools/copy(48)",
            FID_Options|TSTR, "Cancel",
            FID_String|TSTR,  "Copying...\n\nPlease wait...",
            TAGEND)) {

         acShow(Self->ProgressDialog);
      }
   }

   if (!Self->ProgressDialog) return FFR_OKAY;

   if ((Feedback->Position IS 0) OR (Feedback->Size > 32768)) {
      STRING str = Feedback->Path;
      WORD i = len;
      while ((i > 0) AND (str[i-1] != '/') AND (str[i-1] != '\\') AND (str[i-1] != ':')) i--;

      if (Feedback->Position IS 0) {
         if (Feedback->Size >= 1048576) StrFormat(buffer, sizeof(buffer), "Copying: %s\n\n%.2f MB", str+i, (DOUBLE)Feedback->Size / 1048576.0);
         else StrFormat(buffer, sizeof(buffer), "Copying: %s\n\n%.2f KB", str+i, (DOUBLE)Feedback->Size / 1024.0);
         SetString(Self->ProgressDialog, FID_String, buffer);
         Self->ProgressTime = PreciseTime();

         ProcessMessages(0, 0);
      }
      else if (PreciseTime() - Self->ProgressTime > 50000LL) {
         if (Feedback->Size >= 1048576) StrFormat(buffer, sizeof(buffer), "Copying: %s\n\n%.2f / %.2f MB", str+i, (DOUBLE)Feedback->Position / 1048576.0, (DOUBLE)Feedback->Size / 1048576.0);
         else StrFormat(buffer, sizeof(buffer), "Copying: %s\n\n%.2f / %.2f KB", str+i, (DOUBLE)Feedback->Position / 1024.0, (DOUBLE)Feedback->Size / 1024.0);
         SetString(Self->ProgressDialog, FID_String, buffer);
         Self->ProgressTime = PreciseTime();

         ProcessMessages(0, 0);
      }
   }

   return FFR_OKAY;
}

//****************************************************************************

static void run_script(CSTRING Statement, OBJECTID Target, CSTRING File)
{
   if (Statement) {
      OBJECTPTR script;
      if (!CreateObject(ID_FLUID, NF_INTEGRAL, &script,
            FID_String|TSTR,  Statement,
            FID_Target|TLONG, Target,
            TAGEND)) {
         acSetVar(script, "File", File);
         acActivate(script);
         acFree(script);
      }
   }
}

//****************************************************************************

static void free_clip(objClipboard *Self, struct ClipEntry *Clip)
{
   if (Clip->TotalItems > 16384) Clip->TotalItems = 16384;

   if (Clip->Datatype != CLIPTYPE_FILE) {
      CSTRING datatype = GetDatatype(Clip->Datatype);

      LogBranch("Deleting %d clip files for datatype %s / %d.", Clip->TotalItems, datatype, Clip->Datatype);

      // Delete cached clipboard files

      WORD i;
      for (i=0; i < Clip->TotalItems; i++) {
         char buffer[200];
         StrFormat(buffer, sizeof(buffer), "clipboard:%s%d.%.3d", datatype, Clip->ID, i);
         DeleteFile(buffer);
      }
   }
   else LogBranch("Datatype: File");

   if (Clip->Files) { FreeMemoryID(Clip->Files); Clip->Files = 0; }

   ClearMemory(Clip, sizeof(struct ClipEntry));

   LogBack();
}

//****************************************************************************

static ERROR add_clip(objClipboard *Self, LONG Datatype, CSTRING File, LONG Flags, CLASSID ClassID, LONG TotalItems,
   LONG *Counter)
{
   struct ClipHeader *header;
   struct ClipEntry clip, tmp, *clips;
   STRING str;
   ERROR error;
   WORD i;

   LogBranch("Datatype: $%x, File: %s, Flags: $%x, Class: %d, Items: %d", Datatype, File, Flags, ClassID, TotalItems);

   Flags |= Self->Flags & CLF_HOST; // Automatically add the CLF_HOST flag

   ClearMemory(&clip, sizeof(clip));

   if (!TotalItems) {
      LogMsg("TotalItems parameter not specified.");
      LogBack();
      return ERR_NullArgs;
   }

   if (!AccessMemory(Self->ClusterID, MEM_READ_WRITE, 3000, &header)) {
      clips = (struct ClipEntry *)(header + 1);

      if (Flags & CEF_EXTEND) {
         // Search for an existing clip that matches the requested datatype
         for (i=0; i < MAX_CLIPS; i++) {
            if (clips[i].Datatype IS Datatype) {
               MSG("Found clip to extend.");

               error = ERR_Okay;

               // We have found a matching datatype.  Start by moving the clip to the front of the queue.

               CopyMemory(clips+i, &tmp, sizeof(struct ClipEntry)); // !!! We need to shift the list down, not swap entries !!!
               CopyMemory(clips, clips+i, sizeof(struct ClipEntry));
               CopyMemory(&tmp, clips, sizeof(struct ClipEntry));

               // Extend the existing clip with the new items/file

               if (File) {
                  if (clips->Files) {
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
                     LogErrorMsg("DATA_FILE datatype used, but a specific file path was not provided.");
                     error = ERR_Failed;
                  }
                  else clips->TotalItems += TotalItems; // Virtual file name
               }

               if (Counter) *Counter = clips->ID;

               ReleaseMemory(header);
               LogBack();
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
            LogBack();
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

      // Remove any existing clips that match this data type number

      for (i=0; i < MAX_CLIPS; i++) {
         if (clips[i].Datatype IS Datatype) free_clip(Self, &clips[i]);
      }

      // If an entry has to be deleted due to a full historical buffer, kill it off.

      if (clips[MAX_CLIPS-1].Datatype) {
         free_clip(Self, &clips[MAX_CLIPS-1]);
      }

      // Insert the new clip entry at start of the history buffer

      CopyMemory(clips, clips+1, MAX_CLIPS-1);
      CopyMemory(&clip, clips, sizeof(clip));

      ReleaseMemory(header);
      LogBack();
      return ERR_Okay;
   }
   else {
      LogBack();
      return ERR_AccessMemory;
   }
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
   objClipboard *clipboard;

   LogF("~Clipboard","Windows has received text on the clipboard.");

   if (!CreateObject(ID_CLIPBOARD, 0, &clipboard,
         FID_Flags|TLONG, CLF_HOST,
         TAGEND)) {
      clipAddText(clipboard, String);
      acFree(clipboard);
   }
   else PostError(ERR_CreateObject);

   LogBack();
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
   objClipboard *clipboard;
   char path[256];
   LONG i;

   LogF("~Clipboard:","Windows has received files on the clipboard.  Cut: %d", CutOperation);

   if (!CreateObject(ID_CLIPBOARD, 0, &clipboard,
         FID_Flags|TLONG, CLF_HOST,
         TAGEND)) {

      for (i=0; winExtractFile(Data, i, path, sizeof(path)); i++) {
         add_clip(clipboard, CLIPTYPE_FILE, path, (i ? CEF_EXTEND : 0) | (CutOperation ? CEF_DELETE : 0), 0, 1, 0);
      }

      acFree(clipboard);
   }
   else PostError(ERR_CreateObject);

   LogBack();
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
   objClipboard *clipboard;
   LONG i;

   LogF("~Clipboard:","Windows has received files on the clipboard.  Cut: %d", CutOperation);

   if (!CreateObject(ID_CLIPBOARD, 0, &clipboard,
         FID_Flags|TLONG, CLF_HOST,
         TAGEND)) {

      for (i=0; *Data; i++) {
         add_clip(clipboard, CLIPTYPE_FILE, Data, (i ? CEF_EXTEND : 0) | (CutOperation ? CEF_DELETE : 0), 0, 1, 0);
         while (*Data) Data++; // Go to next file path
         Data++; // Skip null byte
      }

      acFree(clipboard);
   }
   else PostError(ERR_CreateObject);

   LogBack();
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
   objClipboard *clipboard;
   STRING u8str;

   LogF("~Clipboard:","Windows has received unicode text on the clipboard.");

   if (!CreateObject(ID_CLIPBOARD, 0, &clipboard,
         FID_Flags|TLONG, CLF_HOST,
         TAGEND)) {
      LONG chars, u8len;
      u8len = 0;
      for (chars=0; String[chars]; chars++) {
         if (String[chars] < 128) u8len++;
         else if (String[chars] < 0x800) u8len += 2;
         else u8len += 3;
      }

      if (!AllocMemory(u8len+1, MEM_STRING|MEM_NO_CLEAR, &u8str, NULL)) {
         LONG i;
         UWORD value;
         i = 0;
         for (chars=0; String[chars]; chars++) {
            value = String[chars];
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
         FreeMemory(u8str);
      }
      acFree(clipboard);
   }
   else PostError(ERR_CreateObject);

   LogBack();
}
#endif

#include "class_clipboard_def.c"

static const struct FieldArray clFields[] = {
   { "Response", FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clClipboardResponse, NULL, NULL }, // NB: Is writeable so that our confirmation boxes can change it
   { "Flags",    FDF_LONGFLAGS|FDF_RI,       (MAXINT)&clClipboardFlags, NULL, NULL },
   { "Cluster",  FDF_LONG|FDF_RW,            0, NULL, NULL },
   END_FIELD
};
