/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
FileView: The FileView class provides a GUI-based gateway into the file system.

The FileView class is designed for developing graphical interfaces that require user interactive file lists.  This
makes the FileView particularly useful for creating file dialog boxes and file managers.  The class supports a number
of features including the filtering of file names using wildcards (for example, '*.fluid') and you may preset the initial
directory path.

The FileView class does not draw its own graphics.  In order to display the file-list, it must be linked to a
@View object via the #View field.

For an existing example of accepted usage, please refer to the FileDialog Fluid script.

-END-

*****************************************************************************/

//#define DEBUG
#define PRV_FILEVIEW
#include <parasol/modules/xml.h>
#include <parasol/modules/widget.h>
#include <parasol/modules/surface.h>
#include "../defs.h"

#include "fileview_shortcut.c"

#define TITLE_RENAME    "Rename"
#define TITLE_CREATEDIR "Create New Directory"
#define TITLE_DELETE    "Confirm Deletion"

static OBJECTPTR clFileView = NULL;
static FIELD FID_NewName;
static UBYTE glRenameReplace = TRUE, glShowDocs = TRUE, glShowHidden = FALSE, glShowSystem = FALSE;

static ERROR fileview_timer(objFileView *, LARGE, LARGE) __attribute__((unused));

#ifdef _WIN32

#define EXTERNAL_CLIP

INLINE void SUB_TIMER(DOUBLE Interval, TIMER *Timer)
{
   FUNCTION callback;
   SET_FUNCTION_STDC(callback, &fileview_timer);
   SubscribeTimer(Interval, &callback, Timer);
}

#define UNSUB_TIMER(a) UpdateTimer(a, 0)

#else
#define EXTERNAL_CLIP
#define SUB_TIMER(a,b)
#define UNSUB_TIMER(a)
#endif

static ERROR GET_Selection(objFileView *, STRING *);
static ERROR GET_SelectionPath(objFileView *, STRING *);
static ERROR FILEVIEW_CreateShortcut(objFileView *, struct fvCreateShortcut *);
static ERROR FILEVIEW_ParentDir(objFileView *, APTR Void);
static void event_volume_created(OBJECTID *, evAssignCreated *, LONG InfoSize);
static void event_volume_deleted(OBJECTID *, evAssignDeleted *, LONG InfoSize);

static const struct FieldArray clFields[];

static void add_file_item(objFileView *, objXML *, struct FileInfo *);
static void check_docview(objFileView *);
static void convert_permissions(LONG, STRING);
static BYTE delete_item(struct XMLTag *, objView *, CSTRING);
static void error_dialog(objFileView *, CSTRING, CSTRING);
static STRING extract_filename(struct XMLTag *);
static ERROR path_watch(objFile *, CSTRING Path, LARGE Custom, LONG Flags) __attribute__ ((unused));
static struct XMLTag * find_tag(CSTRING, struct XMLTag *);
static void key_event(objFileView *, evKey *, LONG);
static void load_prefs(void);
static ERROR open_files(objFileView *, LONG *, CSTRING);
static ERROR paste_to(objFileView *, CSTRING, LONG);
static ERROR rename_file_item(objFileView *, CSTRING, CSTRING);
static void strip_extension(STRING);
static void report_event(objFileView *, LONG Event);
static void response_delete(objDialog *, LONG Response);
static void response_createdir(objDialog *, LONG Response);
static void response_rename(objDialog *, LONG Response);
CSTRING get_file_icon(CSTRING Path);
static OBJECTID parent_window(OBJECTID SurfaceID);

//****************************************************************************

static ERROR FILEVIEW_ActionNotify(objFileView *Self, struct acActionNotify *Args)
{
   if (!Args) return PostError(ERR_NullArgs);
   if (Args->Error != ERR_Okay) return ERR_Okay;

   if (Args->ActionID IS AC_DragDrop) {
      // This notification is received when something is dropped onto the view's surface. We respond by sending a data
      // request to the source.  Refer to scrStartCursorDrag() for more information.

      struct acDragDrop *drag = (struct acDragDrop *)Args->Args;
      if (!drag) return PostError(ERR_NullArgs);

      FMSG("~","Item dropped onto view, highlighted %d - requesting files from source %d", Self->View->HighlightTag, drag->SourceID);

      Self->DragToTag = Self->View->HighlightTag;

      // Send the source an item request

      struct dcRequest request;
      request.Item          = drag->Item;
      request.Preference[0] = DATA_FILE;
      request.Preference[1] = 0;

      struct acDataFeed dc;
      dc.ObjectID = Self->Head.UniqueID;
      dc.Datatype = DATA_REQUEST;
      dc.Buffer   = &request;
      dc.Size     = sizeof(request);
      if (!ActionMsg(AC_DataFeed, drag->SourceID, &dc)) {
         // The source will return a DATA_RECEIPT for the items that we've asked for (see the DataFeed action).

      }

      STEP();
   }
   else if (Args->ActionID IS AC_Free) {
      if ((Self->EventCallback.Type IS CALL_SCRIPT) AND (Self->EventCallback.Script.Script->UniqueID IS Args->ObjectID)) {
         Self->EventCallback.Type = CALL_NONE;
      }
   }
   else if (Args->ActionID IS AC_Focus) {
      if (!Self->prvKeyEvent) {
         FUNCTION callback;
         SET_FUNCTION_STDC(callback, &key_event);
         SubscribeEvent(EVID_IO_KEYBOARD_KEYPRESS, &callback, Self, &Self->prvKeyEvent);
      }
   }
   else if (Args->ActionID IS AC_LostFocus) {
      if (Self->prvKeyEvent) { UnsubscribeEvent(Self->prvKeyEvent); Self->prvKeyEvent = NULL; }
   }

   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
Activate: Opens the currently selected file or directory.

If the user has selected a file or a directory, calling this method will 'open' the selection accordingly.  For
directories, this means that the FileView object will go to the selected path and read the directory contents,
consequently updating the FileView.  If the selection is a file, the FileView will send an Activate message to each
child that you have initialised to the FileView object.

If no selection has been made in the #View object, the Activate action will do nothing.

-ERRORS-
Okay:
AllocMemory:
ExclusiveDenied: Access to the View object was denied.
GetField:        The View does not support a Selection field, or no selection has been made by the user.
ListChildren:

*****************************************************************************/

static ERROR FILEVIEW_Activate(objFileView *Self, APTR Void)
{
   LONG i, tagindex;
   ERROR error;

   // Note: Activate notification is silent when the user is simply switching directories.

   LogBranch(NULL);

   objXML *xml = Self->View->XML;
   if ((!GetLong(Self->View, FID_ActiveTag, &tagindex)) AND (tagindex != -1)) {

      // Check if the selected item is a directory

      struct XMLTag *tag = xml->Tags[tagindex];
      CSTRING name = extract_filename(tag);
      for (i=0; (name[i]) AND (name[i] != ':') AND (name[i] != '/'); i++);

      if (!StrMatch("dir", tag->Attrib->Name)) {
         // Respond by switching to the new directory
         STRING str;
         LONG size = StrLength(Self->Path) + StrLength(name) + 2;
         if (!AllocMemory(size, MEM_NO_CLEAR|MEM_STRING, &str, NULL)) {
            STRING copy = str;
            if (Self->Path[0] != ':') copy += StrCopy(Self->Path, copy, COPY_ALL);
            copy += StrCopy(name, copy, COPY_ALL);
            if ((copy[-1] != ':') AND (copy[-1] != '/')) {
               *copy++ = '/';
               *copy++ = 0;
            }
            SetString(Self, FID_Path, str);
            FreeMemory(str);
            error = ERR_Okay|ERF_Notified;
         }
         else error = PostError(ERR_AllocMemory);
      }
      else if (!StrMatch("parent", tag->Attrib->Name)) {
         // Go to the parent directory
         error = FILEVIEW_ParentDir(Self, NULL) | ERF_Notified;
      }
      else {
         LONG tags[2];
         tags[1] = -1;

         switch (Self->Auto) {
            case FVA_OPEN:
               if (!GetLong(Self->View, FID_ActiveTag, &tags[0])) open_files(Self, tags, "Open");
               break;

            case FVA_EDIT:
               if (!GetLong(Self->View, FID_ActiveTag, &tags[0])) open_files(Self, tags, "Edit");
               break;

            case FVA_VIEW:
               if (!GetLong(Self->View, FID_ActiveTag, &tags[0])) open_files(Self, tags, "View");
               break;
         }

         if (Self->Feedback.Type IS CALL_STDC) {
            void (*routine)(objFileView *);
            routine = Self->Feedback.StdC.Routine;

            if (Self->Feedback.StdC.Context) {
               OBJECTPTR context = SetContext(Self->Feedback.StdC.Context);
               routine(Self);
               SetContext(context);
            }
            else routine(Self);
         }
         else if (Self->Feedback.Type IS CALL_SCRIPT) {
            OBJECTPTR script;
            if ((script = Self->Feedback.Script.Script)) {
               const struct ScriptArg args[] = {
                  { "FileView", FD_OBJECTPTR, { .Address = Self } }
               };
               scCallback(script, Self->Feedback.Script.ProcedureID, args, ARRAYSIZE(args));
            }
         }

         error = ERR_Okay;
      }
   }
   else error = ERR_GetField;

   LogBack();
   return error;
}

/*****************************************************************************
-ACTION-
Clear: Clears the View.
-END-
*****************************************************************************/

static ERROR FILEVIEW_Clear(objFileView *Self, APTR Void)
{
   acClear(Self->View);
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
CopyFiles: Posts all currently selected files to the clipboard as a 'copy' operation.

The CopyFiles method will copy all user-selected files and directories from the view to the clipboard.  If there are no
files selected, the method does nothing.

-ERRORS-
Okay: The cut operation was processed.
NothingDone: The user has not selected any files to copy.
AccessObject: Failed to access either the clipboard or the view object.
NoSupport: No clipboard is present in the system to support the copy operation.

*****************************************************************************/

static ERROR FILEVIEW_CopyFiles(objFileView *Self, APTR Void)
{
   // Do nothing if we are at the root level

   if ((!Self->Path[0]) OR (Self->Path[0] IS ':')) return ERR_Okay;

   LogBranch(NULL);

   struct XMLTag **taglist;
   LONG *tags;
   ERROR error;
   if ((!GetFields(Self->View, FID_SelectedTags|TPTR, &tags,
                                FID_Tags|TPTR,          &taglist,
                                TAGEND)) AND (tags)) {

      objClipboard *clipboard;
      if (!CreateObject(ID_CLIPBOARD, 0, &clipboard, TAGEND)) {
         // Clear any existing file clip records

         ActionTags(MT_ClipDelete, clipboard, CLIPTYPE_FILE);

         // Add all selected files to the clipboard, one at a time

         LONG i;
         UBYTE buffer[512];
         LONG j = StrCopy(Self->Path, buffer, sizeof(buffer));
         for (i=0; tags[i] != -1; i++) {
            StrCopy(extract_filename(taglist[tags[i]]), buffer+j, sizeof(buffer)-j);
            ActionTags(MT_ClipAddFile, clipboard, CLIPTYPE_FILE, buffer, CEF_EXTEND);
         }

         acFree(clipboard);
      }

      error = ERR_Okay;
   }
   else error = ERR_NothingDone;

   LogBack();
   return error;
}

/*****************************************************************************

-METHOD-
CopyFilesTo: Copies selected files to a destination path.

This method copies all selected files in the fileview to a destination directory of your choosing.  If no files are
selected in the fileview, this method does nothing.  The CopyFilesTo method may return immediately following the
initial call and then copy the files in the background.  If a failure occurs, the user will be notified with an error
dialog.

-INPUT-
cstr Dest: The folder to target for the copy operation.

-ERRORS-
Okay
NullArgs

*****************************************************************************/

static ERROR FILEVIEW_CopyFilesTo(objFileView *Self, struct fvCopyFilesTo *Args)
{
   if ((!Args) OR (!Args->Dest) OR (!Args->Dest[0])) return PostError(ERR_NullArgs);

   LogBranch(NULL);

   ERROR error;
   if (!(error = FILEVIEW_CopyFiles(Self, NULL))) {
      error = paste_to(Self, Args->Dest, 0);
   }

   viewSelectNone(Self->View);

   LogBack();
   return error;
}

/*****************************************************************************

-METHOD-
CreateDir: Creates a new directory in the current path of the file view.

This method creates a dialog box that prompts the user for a directory name.  If the user types in a valid directory
name then the directory will be created in the current path of the file view.  The user may cancel the process by
closing the dialog window at any time.

-ERRORS-
Okay
CreateObject

*****************************************************************************/

static ERROR FILEVIEW_CreateDir(objFileView *Self, APTR Void)
{
   OBJECTPTR dialog;
   ERROR error;

   LogBranch("Path: %s", Self->Path);

   if ((!Self->Path[0]) OR (Self->Path[0] IS ':')) {
      // Create a new shortcut/volume at the root level

      return FILEVIEW_CreateShortcut(Self, NULL);
   }
   else if (!CreateObject(ID_DIALOG, NF_INTEGRAL, &dialog,
         FID_Image|TSTR,    "icons:folders/folder_new(48)",
         FID_Type|TLONG,    DT_REQUEST,
         FID_Options|TSTR,  "cancel;okay",
         FID_Title|TSTR,    TITLE_CREATEDIR,
         FID_Flags|TLONG,   DF_INPUT|DF_INPUT_REQUIRED|DF_MODAL,
         FID_String|TSTR,   "Please enter the name of the new directory that you wish to create.",
         FID_PopOver|TLONG, Self->WindowID,
         TAGEND)) {

      acSetVar(dialog, "Dir", Self->Path);

      SetFunctionPtr(dialog, FID_Feedback, &response_createdir);

      acShow(dialog);
      error = ERR_Okay;
   }
   else error = ERR_CreateObject;

   LogBack();
   return error;
}

/*****************************************************************************

-METHOD-
CreateShortcut: Prompts the user with a dialog to create a new shortcut.

This method creates a dialog box that allows the user to create a new shortcut. The user will be required to name the
shortcut and specify the path to which the shortcut is connected to.

The user may cancel the process by closing the dialog window at any time.

-INPUT-
cstr Message: A message to display in the user dialog (overrides the default message - optional).
cstr Shortcut: Name of the shortcut (optional).
cstr Path: Path to the folder that the shortcut connects to (optional).

-ERRORS-
Okay
CreateObject
AllocMemory
-END-

*****************************************************************************/

static ERROR FILEVIEW_CreateShortcut(objFileView *Self, struct fvCreateShortcut *Args)
{
   LogBranch(NULL);
    
   STRING scriptfile;
   ERROR error;
   if (!AllocMemory(glNewShortcutScriptLength+1, MEM_STRING|MEM_NO_CLEAR, &scriptfile, NULL)) {
      CopyMemory(glNewShortcutScript, scriptfile, glNewShortcutScriptLength);
      scriptfile[glNewShortcutScriptLength] = 0;

      OBJECTPTR script;
      if (!CreateObject(ID_SCRIPT, NF_INTEGRAL, &script,
            FID_String|TSTR, scriptfile,
            TAGEND)) {
         char value[40];
         IntToStr(Self->WindowID, value, sizeof(value));
         acSetVar(script, "PopOver", value);

         if (Args) {
            if (Args->Message)  acSetVar(script, "Message", Args->Message);
            if (Args->Shortcut) acSetVar(script, "Shortcut", Args->Shortcut);
            if (Args->Path)     acSetVar(script, "Path", Args->Path);
         }

         error = acActivate(script);

         acFree(script);
      }
      else error = ERR_CreateObject;

      FreeMemory(scriptfile);
   }
   else error = ERR_AllocMemory;

   LogBack();
   return error;
}

/*****************************************************************************

-METHOD-
CutFiles: Posts all currently selected files to the clipboard as a 'cut' operation.

The CutFiles method will post all selected files and directories from the view to the clipboard.  If there are no files
selected, the method does nothing.

-ERRORS-
Okay
-END-

*****************************************************************************/

static ERROR FILEVIEW_CutFiles(objFileView *Self, APTR Void)
{
   // Do nothing if we are at the root level

   if ((!Self->Path[0]) OR (Self->Path[0] IS ':')) return ERR_Okay;

   LogBranch(NULL);

   struct XMLTag **taglist;
   LONG *tags;
   if ((!GetFields(Self->View, FID_SelectedTags|TPTR, &tags,
                                FID_Tags|TPTR,         &taglist,
                                TAGEND)) AND (tags)) {

      objClipboard *clipboard;
      if (!CreateObject(ID_CLIPBOARD, 0, &clipboard, TAGEND)) {
         // Clear any existing file clips

         ActionTags(MT_ClipDelete, clipboard, CLIPTYPE_FILE);

         // Add all selected files to the clipboard

         char buffer[300];
         LONG j = StrCopy(Self->Path, buffer, sizeof(buffer));
         LONG i;
         for (i=0; tags[i] != -1; i++) {
            StrCopy(extract_filename(taglist[tags[i]]), buffer+j, sizeof(buffer)-j);
            ActionTags(MT_ClipAddFile, clipboard, CLIPTYPE_FILE, buffer, CEF_EXTEND|CEF_DELETE);
         }

         acFree(clipboard);
      }
   }

   LogBack();
   return ERR_Okay;
}

//****************************************************************************

static ERROR FILEVIEW_DataFeed(objFileView *Self, struct acDataFeed *Args)
{
   LONG j;

   // Pass XML information on the XML object.  Then refresh the display to include the new information.

   if (!Args) return PostError(ERR_NullArgs);

   if (Args->DataType IS DATA_XML) {
      return Action(AC_DataFeed, Self->View, Args);
   }
   else if (Args->Datatype IS DATA_REQUEST) {
      // We are responsible for a drag-n-drop and the target is now requesting data from us as the source.  Send a list
      // of the selected files to the requesting object.

      struct dcRequest *request = (struct dcRequest *)Args->Buffer;

      LogMsg("Received request from object %d, item %d, dragto %d", Args->ObjectID, request->Item, Self->DragToTag);

      // Do nothing if we are at the root level

      if ((!Self->Path[0]) OR (Self->Path[0] IS ':')) {
         error_dialog(Self, "Drag and Drop Failure","Drop and drop for file system drives is not supported.");
         return ERR_Okay;
      }

      // Drag and drop within the same fileview is only supported if the item is dropped onto a folder.

      if (Args->ObjectID IS Self->Head.UniqueID) {
         if (Self->DragToTag != -1) {
            struct XMLTag *tag = Self->View->XML->Tags[Self->DragToTag];
            if (StrMatch("dir", tag->Attrib->Name) != ERR_Okay) {
               LogMsg("User did not drag & drop to a folder.");
               return ERR_Okay;
            }
         }
      }

      if ((request->Preference[0]) AND (request->Preference[0] != DATA_FILE)) {
         // The fileview only supports the file datatype
         return PostError(ERR_NoSupport);
      }

      LONG total_items;
      LONG *items;

      if (!GetFieldArray(Self->View, FID_DragItems, &items, &total_items)) {
         LONG xmlsize = 80 + (total_items * 300);
         STRING xml;
         if (!AllocMemory(xmlsize, MEM_STRING|MEM_NO_CLEAR, &xml, NULL)) { // Temporary buffer for holding the XML
            LONG pos = StrFormat(xml, xmlsize, "<receipt totalitems=\"%d\" id=\"%d\">", total_items, request->Item);

            LONG i;
            for (i=0; i < total_items; i++) {
               pos += StrCopy("<file path=\"", xml+pos, xmlsize-pos);

               for (j=0; Self->Path[j] AND (pos < xmlsize); j++) {
                  if (Self->Path[j] IS '&') pos += StrCopy("&amp;", xml+pos, xmlsize-pos);
                  else if (Self->Path[j] IS '<') pos += StrCopy("&lt;", xml+pos, xmlsize-pos);
                  else if (Self->Path[j] IS '>') pos += StrCopy("&gt;", xml+pos, xmlsize-pos);
                  else xml[pos++] = Self->Path[j];
               }

               STRING name = extract_filename(Self->View->XML->Tags[items[i]]);

               for (j=0; name[j] AND (pos < xmlsize); j++) {
                  if (name[j] IS '&') pos += StrCopy("&amp;", xml+pos, xmlsize-pos);
                  else if (name[j] IS '<') pos += StrCopy("&lt;", xml+pos, xmlsize-pos);
                  else if (name[j] IS '>') pos += StrCopy("&gt;", xml+pos, xmlsize-pos);
                  else xml[pos++] = name[j];
               }

               pos += StrCopy("\"/>", xml+pos, xmlsize-pos);
            }

            pos += StrCopy("</receipt>", xml+pos, xmlsize-pos);

            struct acDataFeed dc = {
               .ObjectID = Self->Head.UniqueID,
               .Datatype = DATA_RECEIPT,
               .Buffer   = xml,
               .Size     = pos+1
            };
            ActionMsg(AC_DataFeed, Args->ObjectID, &dc);

            FreeMemory(xml);

            return ERR_Okay;
         }
         else return PostError(ERR_AllocMemory);
      }
      else return PostError(ERR_NoData);
   }
   else if (Args->Datatype IS DATA_RECEIPT) {
      LONG i;
      STRING path;

      LogMsg("Received item receipt from object %d", Args->ObjectID);

      if (!Self->DragClip) {
         if (CreateObject(ID_CLIPBOARD, 0, &Self->DragClip,
               FID_Cluster|TLONG, 0, // Create a clipboard with a new file cluster
               TAGEND) != ERR_Okay) {
            return ERR_CreateObject;
         }
      }
      else {
         // Clear any existing file clip records
         ActionTags(MT_ClipDelete, Self->DragClip, CLIPTYPE_FILE);
      }

      STRING dest = StrClone(Self->Path);

      // If the item is being dropped onto a folder, the destination path will be our Path + the folder name.

      if (Self->DragToTag != -1) {
         struct XMLTag *tag = Self->View->XML->Tags[Self->DragToTag];
         if (!StrMatch("dir", tag->Attrib->Name)) {
            path = extract_filename(tag);
            if (!AllocMemory(StrLength(Self->Path) + StrLength(path) + 1, MEM_STRING|MEM_NO_CLEAR, &dest, NULL)) {
               i = StrCopy(Self->Path, dest, COPY_ALL);
               i += StrCopy(path, dest+i, COPY_ALL);
            }
            else return ERR_AllocMemory;
         }
      }

      // Do nothing if the destination is the root level

      if ((!dest[0]) OR (dest[0] IS ':')) {
         LogMsg("Doing nothing - at the root level.");
         FreeMemory(dest);
         return ERR_Okay;
      }

      objStorageDevice *dev_dest;
      if (!CreateObject(ID_STORAGEDEVICE, NF_INTEGRAL, &dev_dest,
            FID_Volume|TSTR, dest,
            TAGEND)) {
         LONG count = 0;

         objXML *xml;
         if (!CreateObject(ID_XML, NF_INTEGRAL, &xml,
               FID_Statement|TSTR, Args->Buffer,
               TAGEND)) {

            for (i=0; i < xml->TagCount; i++) {
               struct XMLTag *tag = xml->Tags[i];
               if (!StrMatch("file", tag->Attrib->Name)) {
                  // If the file is being dragged within the same device, it will be moved instead of copied.

                  if ((path = XMLATTRIB(tag, "path"))) {

                     LONG flags = 0;
                     objStorageDevice *dev_src;
                     if (!CreateObject(ID_STORAGEDEVICE, NF_INTEGRAL, &dev_src,
                           FID_Volume|TSTR, path,
                           TAGEND)) {

                        STRING src_device_id, dest_device_id;

                        if ((!GetString(dev_src, FID_DeviceID, &src_device_id)) AND
                            (!GetString(dev_dest, FID_DeviceID, &dest_device_id)) AND
                            (!StrMatch(src_device_id, dest_device_id))) {
                           flags |= CEF_DELETE;
                        }
                        else if ((dev_src->DeviceFlags IS dev_dest->DeviceFlags) AND
                                 (dev_src->BytesFree IS dev_dest->BytesFree) AND
                                 (dev_src->BytesUsed IS dev_dest->BytesUsed)) {
                           flags |= CEF_DELETE;
                        }

                        acFree(dev_src);
                     }

                     if (StrMatch(path, dest) != ERR_Okay) { // Source and destination must be different
                        if (flags & CEF_DELETE) LogMsg("MOVE '%s' TO %s'", path, dest);
                        else LogMsg("COPY '%s' to '%s'", path, dest);

                        ActionTags(MT_ClipAddFile, Self->DragClip, CLIPTYPE_FILE, path, CEF_EXTEND|flags);
                        count++;
                     }
                  }
               }
            }

            acFree(xml);
         }

         acFree(dev_dest);

         if (count) paste_to(Self, dest, Self->DragClip->ClusterID);
      }

      FreeMemory(dest);

      return ERR_Okay;
   }
   else return PostError(ERR_NoSupport);
}

/*****************************************************************************

-METHOD-
DeleteFiles: Deletes all currently selected files, following user confirmation.

This method simplifies the process of deleting selected files for the user.  The method will pop-up a dialog box to ask
the user if the selected files should be deleted.  If the user responds positively, the method will proceed to delete
all of the requested files.  Error dialogs may pop-up if any problems occur during the deletion process.

If no files are selected, this method does nothing.

This method will return a failure code if the creation of the initial dialog box fails.

-ERRORS-
Okay
CreateObject: Failed to create the dialog box.
AccessObject: Failed to access the referenced View object.

*****************************************************************************/

static ERROR FILEVIEW_DeleteFiles(objFileView *Self, APTR Void)
{
   LONG *tags, i;

   LogBranch("Path: %s", Self->Path);

   struct XMLTag **taglist;
   ERROR error;
   if ((!GetFields(Self->View, FID_SelectedTags|TPTR, &tags,
                               FID_Tags|TPTR,         &taglist,
                               TAGEND)) AND (tags)) {
      UBYTE buffer[512], intstr[20];

      for (i=0; tags[i] != -1; i++);

      if (i > 1) StrFormat(buffer, sizeof(buffer), "Are you sure that you want to delete the %d selected items?", i);
      else {
          i = StrFormat(buffer, sizeof(buffer), "Are you sure that you want to delete the '%s", extract_filename(taglist[tags[0]]));

          if (buffer[i-1] IS '/') StrCopy("' folder?", buffer+i-1, sizeof(buffer)-i);
          else if (buffer[i-1] IS ':') StrCopy("' shortcut?", buffer+i-1, sizeof(buffer)-i);
          else StrCopy("' file?", buffer+i, sizeof(buffer)-i);
      }

      OBJECTPTR dialog, config;
      if (!CreateObject(ID_DIALOG, NF_INTEGRAL, &dialog,
            FID_Image|TSTR,    "icons:tools/eraser(48)",
            FID_Type|TLONG,    DT_QUESTION,
            FID_Options|TSTR,  "CANCEL:No; YESALL:Yes",
            FID_Title|TSTR,    TITLE_DELETE,
            FID_String|TSTR,   buffer,
            FID_PopOver|TLONG, Self->WindowID,
            FID_Flags|TLONG,   DF_MODAL,
            TAGEND)) {

         // Create a Config object that will store all of the files that we are going to delete.  When the user
         // responds to the dialog box positively, we'll use the config object's content to determine what we're going
         // to delete (see the response support action).

         if (!CreateObject(ID_CONFIG, 0, &config, // Do not use NF_INTEGRAL
               FID_Owner|TLONG, dialog->UniqueID,
               TAGEND)) {

            for (i=0; tags[i] != -1; i++) {
               StrFormat(buffer, sizeof(buffer), "%s%s", Self->Path, extract_filename(taglist[tags[i]]));
               IntToStr(i, intstr, sizeof(intstr));
               cfgWriteValue(config, "DELETEFILES", intstr, buffer);
            }
         }

         SetFunctionPtr(dialog, FID_Feedback, &response_delete);

         acShow(dialog);

         error = ERR_Okay;
      }
      else error = ERR_CreateObject;
   }
   else error = ERR_Okay;

   LogBack();
   return error;
}

/*****************************************************************************

-METHOD-
EditFiles: Runs the edit command for all currently selected files.

This method simplifies the process of editing selected files for the user. The routine utilises the Run class with the
EDIT mode option to launch the files in their respected editors.

If no files are selected, this method does nothing.

-ERRORS-
Okay: The process executed successfully.
AccessObject: Failed to access the referenced View object.
-END-

*****************************************************************************/

static ERROR FILEVIEW_EditFiles(objFileView *Self, APTR Void)
{
   LogBranch(NULL);

   LONG *tags;
   if (!GetPointer(Self->View, FID_SelectedTags, &tags)) {
      open_files(Self, tags, "Edit");
   }

   LogBack();
   return ERR_Okay;
}

//****************************************************************************

static ERROR FILEVIEW_Free(objFileView *Self, APTR Void)
{
   if (Self->prvKeyEvent)   { UnsubscribeEvent(Self->prvKeyEvent); Self->prvKeyEvent = NULL; }
   if (Self->DragClip)      { acFree(Self->DragClip); Self->DragClip = NULL; }
   if (Self->DeleteClip)    { acFree(Self->DeleteClip); Self->DeleteClip = NULL; }
   if (Self->Watch)         { acFree(Self->Watch); Self->Watch = NULL; }
   if (Self->SelectionPath) { FreeMemory(Self->SelectionPath); Self->SelectionPath = NULL; }
   if (Self->RootPath)      { FreeMemory(Self->RootPath); Self->RootPath = NULL; }
   if (Self->Doc)           { acFree(Self->Doc); Self->Doc = NULL; }
   if (Self->Dir)           { CloseDir(Self->Dir); Self->Dir = NULL; }
   if (Self->DeviceInfo)    { acFree(Self->DeviceInfo); Self->DeviceInfo = NULL; }
   if (Self->View)          UnsubscribeAction(Self->View, 0);

   OBJECTPTR object;
   if ((Self->FocusID) AND (!AccessObject(Self->FocusID, 5000, &object))) {
      UnsubscribeAction(object, 0);
      ReleaseObject(object);
   }

   if (Self->VolumeCreatedHandle) { UnsubscribeEvent(Self->VolumeCreatedHandle); Self->VolumeCreatedHandle = NULL; }
   if (Self->VolumeDeletedHandle) { UnsubscribeEvent(Self->VolumeDeletedHandle); Self->VolumeDeletedHandle = NULL; }

   return ERR_Okay;
}

//****************************************************************************

static ERROR FILEVIEW_Init(objFileView *Self, APTR Void)
{
   if ((!Self->View) OR (Self->View->Head.ClassID != ID_VIEW)) {
      OBJECTID id = GetOwner(Self);
      while ((id) AND (GetClassID(id) != ID_VIEW)) {
         id = GetOwnerID(id);
      }
      if (!id) return PostError(ERR_FieldNotSet);
      else {
         MEMINFO info;
         if (!MemoryIDInfo(id, &info)) {
            if (!(Self->View = info.Start)) return PostError(ERR_FieldNotSet);
         }
      }
   }

   if (!Self->FocusID) GetLong(Self->View, FID_Surface, &Self->FocusID);
   else if (GetClassID(Self->FocusID) != ID_SURFACE) return PostError(ERR_FieldNotSet);

   OBJECTPTR object;
   if (!AccessObject(Self->FocusID, 5000, &object)) {
      SubscribeActionTags(object, AC_Focus, AC_LostFocus, TAGEND);
      ReleaseObject(object);
   }

   SubscribeActionTags(Self->View,
      AC_DragDrop,
      TAGEND);

   Self->View->DragSourceID = Self->Head.UniqueID;

   FUNCTION call;

   SET_FUNCTION_STDC(call, event_volume_created);
   SubscribeEvent(EVID_FILESYSTEM_ASSIGN_CREATED, &call, &Self->Head.UniqueID, &Self->VolumeCreatedHandle);

   SET_FUNCTION_STDC(call, event_volume_deleted);
   SubscribeEvent(EVID_FILESYSTEM_ASSIGN_DELETED, &call, &Self->Head.UniqueID, &Self->VolumeDeletedHandle);

   check_docview(Self);

   Self->Flags |= FVF_TOTAL_REFRESH;
   acRefresh(Self);

   if (!Self->Watch) {
      if ((!Self->Path[0]) OR (Self->Path[0] IS ':')) {
         // From the root view we will listen to events from the Assign system, so no timer or file monitor is needed.
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

         // Resort to using a timer if configuring a folder watch failed.
         if (error) SUB_TIMER(Self->RefreshRate * 1000, &Self->Timer);
      }
   }

   Self->WindowID = parent_window(Self->View->Layout->SurfaceID);

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
MoveFilesTo: Moves selected files to a destination path.

This method moves all selected files in the fileview to a destination directory of your choosing.  If no files are
selected in the fileview, this method does nothing.  The MoveFilesTo method may return immediately following the
initial call and then move the files in the background.  If a failure occurs, the user will be notified with an error
dialog.

-INPUT-
cstr Dest: The directory that you want to copy the selected files to.

-ERRORS-
Okay
NullArgs
-END-

*****************************************************************************/

static ERROR FILEVIEW_MoveFilesTo(objFileView *Self, struct fvMoveFilesTo *Args)
{
   if ((!Args) OR (!Args->Dest) OR (!Args->Dest[0])) return ERR_NullArgs;

   ERROR error;
   if (!(error = FILEVIEW_CutFiles(Self, NULL))) {
      error = paste_to(Self, Args->Dest, 0);
   }

   viewSelectNone(Self->View);

   return error;
}

//****************************************************************************

static ERROR FILEVIEW_NewObject(objFileView *Self, APTR Void)
{
   Self->RefreshRate = 0.25;
   Self->ShowHidden = glShowHidden;
   Self->ShowSystem = glShowSystem;
   Self->ShowDocs   = glShowDocs;
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
OpenFiles: Runs the open command for all currently selected files.

This method simplifies the process of opening selected files for the user.  The routine utilises the Run class with the
OPEN mode option to launch the files in their respected editors.

If no files are selected, this method does nothing.

-ERRORS-
Okay: The process executed successfully.
AccessObject: Failed to access the referenced View object.
-END-

*****************************************************************************/

static ERROR FILEVIEW_OpenFiles(objFileView *Self, APTR Void)
{
   LogBranch(NULL);

   LONG *tags;
   if (!GetPointer(Self->View, FID_SelectedTags, &tags)) {
      open_files(Self, tags, "Open");
   }

   LogBack();
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
ParentDir: Jumps to the parent directory of a FileView's current path.

When this method is called, the FileView will jump to the parent directory of the current path.  For instance, if
the user is currently analysing the 'system:classes/' directory, the view will change to the 'system:' path.

If the current path is already at the root level ':', then this method will return immediately as there is nothing
above root.

-ERRORS-
Okay:   The method executed successfully, or the path is already set to root level.
Memory: The method ran out of memory during normal processing.

*****************************************************************************/

static ERROR FILEVIEW_ParentDir(objFileView *Self, APTR Void)
{
   LONG i;

   if ((!Self->Path[0]) OR (Self->Path[0] IS ':')) return ERR_Okay;
   if ((Self->RootPath) AND (!StrMatch(Self->RootPath, Self->Path))) return ERR_Okay;

   LONG mark = 0;
   for (i=0; (Self->Path[i] != 0); i++) {
      if ((Self->Path[i] IS '/') OR (Self->Path[i] IS ':')) mark = i;
   }

   if (mark) i = mark;

   if (!Self->Path[i]) {
      return SetString(Self, FID_Path, NULL);
   }
   else {
      i--;
      while (i > 0) {
         if ((Self->Path[i] IS '/') OR (Self->Path[i] IS ':')) {
            STRING newpath;
            if ((newpath = StrClone(Self->Path))) {
               newpath[i+1] = 0;
               SetString(Self, FID_Path, newpath);
               FreeMemory(newpath);
               return ERR_Okay;
            }
            else return ERR_Memory;
         }
         i--;
      }

      return SetString(Self, FID_Path, NULL);
   }
}

/*****************************************************************************

-METHOD-
PasteFiles: Pastes files from the clipboard to the current path of the file view.

Call the PasteFiles method to copy files from the clipboard into the current path of the file view.  The file view will
automatically refresh itself so that pasted files are immediately displayed.

-ERRORS-
Okay

*****************************************************************************/

static ERROR FILEVIEW_PasteFiles(objFileView *Self, APTR Void)
{
   LogBranch(NULL);
   ERROR error = paste_to(Self, Self->Path, 0);
   LogBack();
   return error;
}

/*****************************************************************************

-ACTION-
Refresh: Refreshes a FileView's directory list.

When the Refresh action is called on a FileView object, the directory that the FileView is monitoring will be read from
scratch, and the list of directories and files will be subsequently refreshed.

-ERRORS-
Okay:         The refresh was successful.
FieldNotSet:  The View field is not set.
CreateObject: An error occurred while creating a File object.

*****************************************************************************/

static ERROR FILEVIEW_Refresh(objFileView *Self, APTR Void)
{
   UBYTE buffer[sizeof(Self->Path)];
   STRING str, colstr;
   LONG flags;
   ERROR error;

   // If monitoring is active, there is no need for manual refreshes

   if (Self->Watch) {
      if (!(Self->Flags & FVF_TOTAL_REFRESH)) return ERR_Okay;
   }

   // If the Refresh action is called by the developer under normal circumstances, we will just refresh the file view
   // via the timer system.

   if (!(Self->Flags & FVF_TOTAL_REFRESH)) {
      // Increase the rate of refresh to quickly scan the whole directory.  The Timer action will reset the timer on completion.

      LogMsg("Path: '%s'", Self->Path);

      Self->ResetTimer = TRUE;
      SUB_TIMER(0.02, &Self->Timer);

      // Reset the directory scanner so that it starts from the beginning

      CloseDir(Self->Dir);
      Self->Dir = NULL;
      return ERR_Okay;
   }

   Self->Flags &= ~FVF_TOTAL_REFRESH; // Turn off the total-refresh option if it has been used

   drwForbidDrawing();

restart:
   if (Self->Refresh) {
      LogMsg("Recursion detected, aborting request.");
      drwPermitDrawing();
      return ERR_Okay|ERF_Notified; // Do not allow a 'refresh within a refresh'
   }

   LogBranch("Path: '%s'", Self->Path);

   if (!Self->View) {
      drwPermitDrawing();
      LogBack();
      return PostError(ERR_FieldNotSet);
   }

   objXML *xml = Self->View->XML;

   if (!(Self->View->Flags & VWF_NO_ICONS)) Self->Qualify = TRUE;
   else Self->Qualify = FALSE;

   if (Self->Dir) { CloseDir(Self->Dir); Self->Dir = NULL; }

   // Get a new file list

   if (Self->DeviceInfo) { acFree(Self->DeviceInfo); Self->DeviceInfo = NULL; }

   if ((Self->Path[0] != ':') AND (Self->Path[0])) {
      CreateObject(ID_STORAGEDEVICE, NF_INTEGRAL, &Self->DeviceInfo,
         FID_Volume|TSTR, Self->Path,
         TAGEND);
   }

   flags = RDF_READ_ALL|RDF_QUALIFY|RDF_TAGS;
   if (Self->Flags & FVF_NO_FILES) flags &= ~RDF_FILE; // Do not read files

   Self->Refresh = TRUE;

   acClear(Self->View);

   // Define new column settings (if necessary)

   if ((Self->Path[0] IS ':') OR (!Self->Path[0])) {
      str = "default(text:Name, len:210, showicons); freespace(text:Free Space, len:90, type:bytesize, rightalign); totalsize(text:Total Size, len:90, type:bytesize, rightalign)";
   }
#ifdef __linux__
   else str = "default(text:Name, len:210, showicons); size(text:Size, len:90, type:bytesize, rightalign); date(text:Date, len:100, type:date); permissions(text:Permissions,len:90); owner(text:Owner,len:60); group(text:Group,len:60)";
#else
   else str = "default(text:Name, len:210, showicons); size(text:Size, len:90, type:bytesize, rightalign); date(text:Date, len:120, type:date)";
#endif

   if ((!GetString(Self->View, FID_Columns, &colstr)) AND (colstr)) {
      if (StrMatch(colstr, str) != ERR_Okay) SetString(Self->View, FID_Columns, str);
   }
   else SetString(Self->View, FID_Columns, str);

   if ((Self->Flags & FVF_SHOW_PARENT) AND (Self->Path[0] != ':') AND (Self->Path[0])) {
      BYTE show = TRUE;
      if ((Self->RootPath) AND (!StrMatch(Self->RootPath, Self->Path))) {
         show = FALSE;
      }

      if (show) {
         const char strparent[] = "<parent icon=\"folders/parent\" sort=\"\001...\" insensitive>...</dir>";
         acDataXML(xml, strparent);
      }
   }

   // If the device is removable, change the refresh timer so that we don't test the device so often.   This is useful
   // because some badly written device drivers and hardware may persistently test themselves when there is no media in
   // the drive.

   if ((Self->DeviceInfo) AND (!Self->Watch) AND (Self->Path[0]) AND (Self->Path[0] != ':')) {
      if ((Self->DeviceInfo->DeviceFlags & DEVICE_REMOVEABLE)) {
         if (!(Self->DeviceInfo->DeviceFlags & DEVICE_WRITE)) {
            // Read only device - do not refresh
            if (Self->Timer) { UNSUB_TIMER(Self->Timer); Self->Timer = 0; }
         }
         else if (Self->DeviceInfo->DeviceFlags & (DEVICE_FLOPPY_DISK|DEVICE_USB)) {
            // Slow device - do not refresh
            if (Self->Timer) { UNSUB_TIMER(Self->Timer); Self->Timer = 0; }
         }
         else SUB_TIMER(2.0, &Self->Timer); // Write/Read access to removable device
      }
      else SUB_TIMER(Self->RefreshRate, &Self->Timer);
   }

   LARGE msgtime = PreciseTime();
   LARGE systime = msgtime;
   BYTE dirchange = FALSE;
   StrCopy(Self->Path, buffer, sizeof(buffer));
   struct DirInfo *dirinfo;
   if (!(error = OpenDir(Self->Path, flags, &dirinfo))) {
      AdjustLogLevel(2);

      while (!ScanDir(dirinfo)) {
         // Some drives that have slow read access (like floppies) will take a long time to feed us information during
         // this loop.  For this reason we call ProcessMessages() to prevent the application from becoming frozen.
         // The path check is in case the user changes the path during the ProcessMessages() call.

         if (StrMatch(Self->Path, buffer) != ERR_Okay) {
            dirchange = TRUE;
            break;
         }

         add_file_item(Self, xml, dirinfo->Info);
         LARGE currenttime = PreciseTime();

         if (currenttime - msgtime > 20LL * 1000LL) {
            msgtime = currenttime;
            drwPermitDrawing();
            ProcessMessages(0, 0);
            drwForbidDrawing();
         }

         // Tell the view to update its display with the most recently added XML content every 200ms or so

         if (currenttime - systime > 200LL * 1000LL) {
            systime = currenttime;
            acRefresh(Self->View);

            drwPermitDrawing();
            ActionMsg(MT_DrwInvalidateRegion, Self->View->Layout->SurfaceID, NULL);
            drwForbidDrawing();
         }
      }

      AdjustLogLevel(-2);

      CloseDir(dirinfo);
   }
   else LogMsg("Failed to open \"%s\", \"%s\"", Self->Path, GetErrorMsg(error));

   if (dirchange) {
      LogMsg("Directory change within-refresh detected.");
      acClear(Self->View);
   }
   else {
      acSort(Self->View);
      acRefresh(Self->View);
   }

   Self->Refresh = FALSE;

   LogBack();

   if (dirchange) goto restart; //DelayMsg(AC_Refresh, Self->Head.UniqueID, NULL);

   drwPermitDrawing();

   ActionMsg(MT_DrwInvalidateRegion, Self->View->Layout->SurfaceID, NULL);

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
RenameFile: Renames the most recently selected item in the file view.

This method simplifies the process of renaming the most recently selected file for the user.  The method will pop-up an
input box to ask the user for the new file name.  If the user responds positively, the method will proceed to rename
the file.

If no file is selected, this method does nothing.

-ERRORS-
Okay:
AccessObject: Failed to access the referenced View object.
-END-

*****************************************************************************/

static ERROR FILEVIEW_RenameFile(objFileView *Self, APTR Void)
{
   if ((!Self->Path[0]) OR (Self->Path[0] IS ':')) return ERR_Okay;

   STRING selection;
   if (GET_Selection(Self, &selection) != ERR_Okay) return ERR_Okay;
   if ((!selection) OR (!selection[0])) return ERR_Okay;

   LogBranch("%s", selection);

   OBJECTPTR dialog;
   ERROR error;
   if (!CreateObject(ID_DIALOG, NF_INTEGRAL, &dialog,
         FID_Image|TSTR,     "icons:tools/edit(48)",
         FID_Options|TSTR,   "cancel;okay",
         FID_Type|TLONG,     DT_REQUEST,
         FID_Title|TSTR,     TITLE_RENAME,
         FID_Flags|TLONG,    DF_INPUT|DF_INPUT_REQUIRED|DF_MODAL,
         FID_String|TSTR,    "Please enter the new name for the selected file or directory.",
         FID_UserInput|TSTR, selection,
         FID_PopOver|TLONG,  Self->WindowID,
         TAGEND)) {

      if (!GET_SelectionPath(Self, &selection)) {
         SetVar(dialog, "Src", selection);
         SetFunctionPtr(dialog, FID_Feedback, &response_rename);
         acShow(dialog);
         error = ERR_Okay;
      }
      else error = PostError(ERR_Failed);
   }
   else error = PostError(ERR_CreateObject);

   LogBack();
   return error;
}

/*****************************************************************************

-METHOD-
ViewFiles: Runs the view command for all currently selected files.

This method simplifies the process of viewing selected files for the user.  The routine utilises the Run class with the
VIEW mode option to launch the files in their respected viewers.

If no files are selected, this method does nothing.

-ERRORS-
Okay: The process executed successfully.
AccessObject: Failed to access the referenced View object.
-END-

*****************************************************************************/

static ERROR FILEVIEW_ViewFiles(objFileView *Self, APTR Void)
{
   LogBranch(NULL);

   LONG *tags;
   if (!GetPointer(Self->View, FID_SelectedTags, &tags)) {
      open_files(Self, tags, "View");
   }

   LogBack();
   return ERR_Okay;
}

//****************************************************************************

#include "fileview_fields.c"
#include "fileview_functions.c"
#include "fileview_def.c"

static const struct FieldArray clFields[] = {
   { "RefreshRate", FDF_DOUBLE|FDF_RI,     0, NULL, NULL },
   { "View",        FDF_OBJECT|FDF_RI,    ID_VIEW, NULL, SET_View },
   { "Flags",       FDF_LONGFLAGS|FDF_RW, (MAXINT)&clFileViewFlags, NULL, NULL },
   { "Focus",       FDF_OBJECTID|FDF_RI,  0, NULL, NULL },
   { "RootPath",    FDF_STRING|FDF_RW,    0, NULL, SET_RootPath },
   { "Auto",        FDF_LONG|FDF_LOOKUP|FDF_RW, (MAXINT)&clFileViewAuto, NULL, NULL },
   { "Window",      FDF_OBJECTID|FDF_R,   0, NULL, NULL },
   { "ShowHidden",  FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "ShowSystem",  FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "ShowDocs",    FDF_LONG|FDF_RW,      0, NULL, NULL },
   { "EventMask",   FDF_LONGFLAGS|FDF_RW, (MAXINT)&clFileViewEventMask, NULL, NULL },
   // Virtual fields
   { "EventCallback", FDF_FUNCTIONPTR|FDF_RW, 0, GET_EventCallback, SET_EventCallback },
   { "Feedback",      FDF_FUNCTIONPTR|FDF_RW, 0, GET_Feedback, SET_Feedback },
   { "Filter",        FDF_STRING|FDF_RW, 0, GET_Filter,    SET_Filter },
   { "BytesFree",     FDF_LARGE|FDF_R,   0, GET_BytesFree, NULL },
   { "BytesUsed",     FDF_LARGE|FDF_R,   0, GET_BytesUsed, NULL },
   { "ByteSize",      FDF_LARGE|FDF_R,   0, GET_ByteSize,  NULL },
   { "Path",          FDF_STRING|FDF_RW, 0, GET_Path,  SET_Path },
   { "Location",      FDF_SYNONYM|FDF_STRING|FDF_RW, 0, GET_Path,  SET_Path },
   { "Selection",     FDF_STRING|FDF_R,  0, GET_Selection, NULL },
   { "SelectionFile", FDF_STRING|FDF_R,  0, GET_SelectionFile, NULL },
   { "SelectionPath", FDF_STRING|FDF_R,  0, GET_SelectionPath, NULL },
   END_FIELD
};

//****************************************************************************

ERROR init_fileview(void)
{
   FID_NewName = StrHash("NewName", FALSE);

   load_prefs();

   return CreateObject(ID_METACLASS, 0, &clFileView,
      FID_ClassVersion|TFLOAT, 1.0,
      FID_Name|TSTR,      "FileView",
      FID_Category|TLONG, CCF_TOOL,
      FID_Actions|TPTR,   clFileViewActions,
      FID_Methods|TARRAY, clFileViewMethods,
      FID_Fields|TARRAY,  clFields,
      FID_Size|TLONG,     sizeof(objFileView),
      FID_Path|TSTR,      MOD_PATH,
      TAGEND);
}

void free_fileview(void)
{
   if (clFileView) { acFree(clFileView); clFileView = NULL; }
}

