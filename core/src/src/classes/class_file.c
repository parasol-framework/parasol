/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
File: Enables access to the file system.

The File class provides extensive support for file management and I/O.  The class supports the notion of individual
file compression and file finding capabilities.  Since all File objects are tracked, there is no chance of the system
leaving locked files behind after a program exits.  Folder management is also integrated into this class to ease the
management of both file types.

To read or write to a file, set the #Path of the file as well as the correct I/O file flags before initialisation.
See the #Flags field for information on the available I/O flags.  Functionality for read and write operations is
provided through the #Read() and #Write() actions.  The #Seek() action can be used to change the read/write position
in a file.
-END-

*****************************************************************************/

#define PRV_FILE
#define PRV_FILESYSTEM
#define _LARGEFILE64_SOURCE
#define _TIME64_T
#define _LARGE_TIME_API
#include "../defs.h"

#ifdef __linux__
#define _GNU_SOURCE
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef __ANDROID__
#include <sys/statvfs.h>
#else
#include <sys/vfs.h>
#define statvfs statfs
#define fstatvfs fstatfs
#endif

#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/vfs.h>

#include <sys/inotify.h>
//#include "inotify-syscalls.h"
//#include "inotify.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>

#define open64   open
#define lseek64  lseek
//#define fstat64  fstat
//#define stat64   stat
#undef NULL
#define NULL 0

#endif // _WIN32

#include <parasol/main.h>

LONG feedback_delete(struct FileFeedback *);
void path_monitor(HOSTHANDLE, objFile *);

static ERROR FILE_Init(objFile *, APTR);
static ERROR FILE_Watch(objFile *, struct flWatch *);

static ERROR SET_Path(objFile *, CSTRING);
static ERROR SET_Size(objFile *, LARGE);

static const struct FieldDef FileFlags[];
static const struct FieldDef PermissionFlags[];
static const struct FieldArray FileFields[];
static const struct ActionArray clFileActions[];
static const struct MethodArray clFileMethods[];

static ERROR GET_ResolvedPath(objFile *, CSTRING *);

static ERROR  set_permissions(objFile *, LONG);

//****************************************************************************

ERROR add_file_class(void)
{
   extern objMetaClass *glFileClass;
   return CreateObject(ID_METACLASS, 0, (OBJECTPTR *)&glFileClass,
      FID_ClassVersion|TFLOAT, VER_FILE,
      FID_Name|TSTR,      "File",
      FID_Category|TLONG, CCF_SYSTEM,
      FID_Flags|TLONG,    CLF_PRIVATE_ONLY,
      FID_Actions|TPTR,   clFileActions,
      FID_Methods|TARRAY, clFileMethods,
      FID_Fields|TARRAY,  FileFields,
      FID_Size|TLONG,     sizeof(objFile),
      FID_Path|TSTR,      "modules:core",
      TAGEND);
}

/*****************************************************************************
-ACTION-
Activate: Opens the file.  Performed automatically if NEW, READ or WRITE flags were specified on initialisation.
-END-
*****************************************************************************/

static ERROR FILE_Activate(objFile *Self, APTR Void)
{
   if (Self->Handle != -1) return ERR_Okay;
   if (!(Self->Flags & (FL_NEW|FL_READ|FL_WRITE))) return PostError(ERR_NothingDone);

   // Setup the open flags.  Note that for new files, the owner will always have read/write/delete permissions by
   // default.  Extra flags can be set through the Permissions field.  If the user wishes to turn off his access to
   // the created file then he must do so after initialisation.

   LONG openflags = 0;
   if (Self->Flags & FL_NEW) openflags |= O_CREAT|O_TRUNC;

   CSTRING path;
   if (GET_ResolvedPath(Self, &path)) return ERR_ResolvePath;

#ifdef __unix__
   LONG secureflags = S_IRUSR|S_IWUSR|convert_permissions(Self->Permissions);

   // Opening /dev/ files from Parasol is disallowed because it can cause problems

   if (Self->Flags & FL_DEVICE) {
      openflags |= O_NOCTTY; // Prevent device from becoming the controlling terminal
   }
   else if (!StrCompare("/dev/", path, 0, 0)) {
      LogErrorMsg("Opening devices not permitted without the DEVICE flag.");
      return ERR_NoPermission;
   }
#else
   LONG secureflags = S_IRUSR|S_IWUSR;
#endif

   if ((Self->Flags & (FL_READ|FL_WRITE)) IS (FL_READ|FL_WRITE)) {
      LogMsg("Open \"%s\" [RW]", path);
      openflags |= O_RDWR;
   }
   else if (Self->Flags & FL_READ) {
      LogMsg("Open \"%s\" [R]", path);
      openflags |= O_RDONLY;
   }
   else if (Self->Flags & FL_WRITE) {
      LogMsg("Open \"%s\" [W|%s]", path, (Self->Flags & FL_NEW) ? "New" : "Existing");
      openflags |= O_RDWR;
   }
   else LogMsg("Open \"%s\" [-]", path);

   #ifdef __unix__
      // Set O_NONBLOCK to stop the task from being halted in the event that we accidentally try to open a pipe like a
      // FIFO file.  This can happen when scanning the /dev/ folder and can cause tasks to hang.

      openflags |= O_NONBLOCK;
   #endif

   #ifdef _WIN32
      if (Self->Flags & FL_NEW) {
         // Make sure that we'll be able to recreate the file from new if it already exists and is marked read-only.

         chmod(path, S_IRUSR|S_IWUSR);
      }
   #endif

   if ((Self->Handle = open(path, openflags|WIN32OPEN|O_LARGEFILE, secureflags)) IS -1) {
      LONG err = errno;

      if (Self->Flags & FL_NEW) {
         // Attempt to create the necessary directories that might be required for this new file.

         if (check_paths(path, Self->Permissions) IS ERR_Okay) {
            Self->Handle = open(path, openflags|WIN32OPEN|O_LARGEFILE, secureflags);
         }

         if (Self->Handle IS -1) {
            LogErrorMsg("New file error \"%s\"", path);
            if (err IS EACCES) return PostError(ERR_NoPermission);
            else if (err IS ENAMETOOLONG) return PostError(ERR_BufferOverflow);
            else return ERR_CreateFile;
         }
      }
      else if ((errno IS EROFS) AND (Self->Flags & FL_READ)) {
         // Drop requested access rights to read-only and try again
         LogErrorMsg("Reverting to read-only access for this read-only file.");
         openflags = O_RDONLY;
         Self->Flags &= ~FL_WRITE;
         Self->Handle = open(path, openflags|WIN32OPEN|O_LARGEFILE, secureflags);
      }
      else if (Self->Flags & FL_LINK) {
         // The file is a broken symbolic link (i.e. refers to a file that no longer exists).  Even
         // though we won't be able to get a valid handle for the link, we'll allow the initialisation
         // to continue because the user may want to delete the symbolic link or get some information
         // about it.
      }

      if ((Self->Handle IS -1) AND (!(Self->Flags & FL_LINK))) {
         switch(errno) {
            case EACCES: return PostError(ERR_NoPermission);
            case EEXIST: return PostError(ERR_FileExists);
            case EINVAL: return PostError(ERR_Args);
            case ENOENT: return PostError(ERR_FileNotFound);
            default:
               LogErrorMsg("Could not open \"%s\", error: %s", path, strerror(errno));
               return ERR_Failed;
         }
      }
   }

   // File size management

   if (Self->Handle != -1) {
      if (Self->Flags & FL_NEW);
      else if ((Self->Size = lseek64(Self->Handle, 0, SEEK_END)) != -1) {  // Get the size of the file.  Could be zero if the file is a stream.
         lseek64(Self->Handle, 0, SEEK_SET);
      }
      else {
         // lseek64() can fail if the file is special
         Self->Size = 0;
      }
   }

   if (Self->Flags & FL_NEW) {
      if (Self->Permissions) set_permissions(Self, Self->Permissions);
   }

   // If the BUFFER flag is set, load the entire file into RAM and treat it as a read/write memory buffer.

   if (Self->Flags & FL_BUFFER) return flBufferContent(&Self->Head);

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
BufferContent: Reads all file content into a local memory buffer.

File content may be buffered at any time by calling the BufferContent method.  This will allocate a buffer that matches
the current file size and the file's content will be read into that buffer.  The BUFFER flag is set in the file object
and a pointer to the content is referenced in the file's Buffer field.  Standard file operations such as read, write
and seek have the same effect when a file is in buffer mode.

Once a file has been buffered, the original file handle and any locks on that file are returned to the system.
Physical operations on the file object such as delete, rename and attribute settings no longer have meaning when
applied to a buffered file.  It is not possible to drop the buffer and return the file object to its original state
once buffering has been enabled.

-ERRORS-
Okay: The file content was successfully buffered.
AllocMemory:
Read: Failed to read the file content.

*****************************************************************************/

static ERROR FILE_BufferContent(objFile *Self, APTR Void)
{
   LONG len;

   if (Self->Buffer) return ERR_Okay;

   acSeek(Self, 0, SEEK_START);

   if (!Self->Size) {
      // If the file has no size, it could be a stream (or simply empty).  This routine handles this situation.

      char ch;
      if (!acRead(Self, &ch, 1, &len)) {
         Self->Flags |= FL_STREAM;
         // Allocate a 1 MB memory block, read the stream into it, then reallocate the block to the correct size.

         UBYTE *buffer;
         if (!AllocMemory(1024 * 1024, MEM_NO_CLEAR, (APTR *)&buffer, NULL)) {
            acSeekStart(Self, 0);
            acRead(Self, buffer, 1024 * 1024, &len);
            if (len > 0) {
               if (!AllocMemory(len, Self->Head.MemFlags|MEM_NO_CLEAR, (APTR *)&Self->Buffer, NULL)) {
                  CopyMemory(buffer, Self->Buffer, len);
                  Self->Size = len;
               }
            }
            FreeMemory(buffer);
         }
      }
   }
   else {
      // Allocate buffer and load file content.  A NULL byte is added so that there is some safety in the event that
      // the file content is treated as a string.

      UBYTE *buffer;
      if (!AllocMemory(Self->Size+1, Self->Head.MemFlags|MEM_NO_CLEAR, (APTR *)&buffer, NULL)) {
         buffer[Self->Size] = 0;
         if (!acRead(Self, buffer, Self->Size, &len)) {
            Self->Buffer = buffer;
         }
         else {
            FreeMemory(buffer);
            return PostError(ERR_Read);
         }
      }
      else return PostError(ERR_AllocMemory);
   }

   // If the file was empty, allocate a 1-byte memory block for the Buffer field, in order to satisfy condition tests.

   if (!Self->Buffer) {
      if (AllocMemory(1, Self->Head.MemFlags, (APTR *)&Self->Buffer, NULL) != ERR_Okay) {
         return PostError(ERR_AllocMemory);
      }
   }

   LogMsg("File content now buffered in a " PF64() " byte memory block.", Self->Size);

   close(Self->Handle);
   Self->Handle = -1;
   Self->Position = 0;
   Self->Flags |= FL_BUFFER;
   return ERR_Okay;
}

/****************************************************************************
-ACTION-
DataFeed: Data can be streamed to any file as a method of writing content.

Streaming data of any type to a file will result in the content being written to the file at the current seek
#Position.

****************************************************************************/

static ERROR FILE_DataFeed(objFile *Self, struct acDataFeed *Args)
{
   if ((!Args) OR (!Args->Buffer)) return PostError(ERR_NullArgs);

   if (Args->Size) return acWrite(Self, Args->Buffer, Args->Size, NULL);
   else return acWrite(Self, Args->Buffer, StrLength(Args->Buffer), NULL);
}

/****************************************************************************

-METHOD-
Copy: Copies the data of a file to another location.

This method is used to copy the data of a file object to another location.  All of the data will be copied, effectively
creating a clone of the original file information.  The file object must have been initialised with the FL_READ flag,
or the copy operation will not work (this restriction does not apply to directories).  If a matching file name already
exists at the destination path, it will be over-written with the new data.

The #Position field will be reset as a result of calling this method.

When copying directories with this method, the entire folder structure (i.e. all of the folder contents) will be
copied to the new location.  If an error occurs when copying a sub-folder or file, the procedure will be aborted
and an error code will be returned.

-INPUT-
cstr Dest: The destination file path for the copy operation.

-ERRORS-
Okay: The file data was copied successfully.
NullArgs:
Args:
FieldNotSet: The Path field has not been set in the file object.
Failed:
Read: Data could not be read from the source path.
Write: Data could not be written to the destination path.
ResolvePath:
Loop: Performing the copy would cause infinite recursion.
AllocMemory:

****************************************************************************/

static ERROR FILE_Copy(objFile *Self, struct flCopy *Args)
{
   return CopyFile(Self->Path, Args->Dest);
}

/****************************************************************************

-METHOD-
Delete: Deletes a file from its source location.

This method is used to delete files from their source location.  If used on a folder, all of the folder's
contents will be deleted in the call.   Once a file is deleted, the object effectively becomes unusable.  For this
reason, file deletion should normally be followed up with a call to the Free action.

This method supports file feedback if you have called the FileFeedback() function earlier.  Feedback is sent for each
file or folder just before it is about to be removed by the routine.  Individual files can be skipped, or the entire
process aborted via the feedback mechanism.

-INPUT-
int(FDL) Flags: If set to FDL_FEEDBACK, a dialog window will popup to give user feedback during deletion.

-ERRORS-
Okay:  File deleted successfully.
Failed: The deletion attempt failed (specific condition not available).
MissingPath:
ResolvePath:
NoPermission: The user does not have the necessary permissions to delete the file.
ReadOnly: The file is on a read-only filesystem.
Locked: The file is in use.
BufferOverflow: The file path string is too long.

****************************************************************************/

static ERROR FILE_Delete(objFile *Self, struct flDelete *Args)
{
   if ((!Self->Path) OR (!*Self->Path)) return PostError(ERR_MissingPath);

   if ((Args) AND (Args->Flags & 0x1)) {
      struct rkFunction callback;
      SET_FUNCTION_STDC(callback, &feedback_delete);
      FileFeedback(&callback, Self, 0); // See feedback_delete() for more info

      ERROR error = FILE_Delete(Self, NULL);

      tlFeedback.Type = CALL_NONE;

      if (Self->ProgressDialog) { acFree(Self->ProgressDialog); Self->ProgressDialog = NULL; }

      return error;
   }

   if ((Self->Stream) AND (!(Self->Flags & FL_LINK))) {
      LogBranch("Delete Folder: %s", Self->Path);

      // Check if the Path is a volume

      LONG len = StrLength(Self->Path);

      if (Self->Path[len-1] IS ':') {
         if (!DeleteVolume(Self->Path)) {
            #ifdef __unix__
               closedir(Self->Stream);
            #endif
            Self->Stream = NULL;
            LogBack();
            return ERR_Okay;
         }
         else {
            LogBack();
            return ERR_DeleteFile;
         }
      }

      // Delete the folder and its contents

      CSTRING path;
      if (!GET_ResolvedPath(Self, &path)) {
         BYTE buffer[512];

         #ifdef __unix__
            closedir(Self->Stream);
         #endif
         Self->Stream = NULL;

         len = StrCopy(path, buffer, sizeof(buffer));
         if ((buffer[len-1] IS '/') OR (buffer[len-1] IS '\\')) buffer[len-1] = 0;

         struct FileFeedback fb;
         ClearMemory(&fb, sizeof(fb));
         if (tlFeedback.Type) {
            fb.FeedbackID = FBK_DELETE_FILE;
            fb.Path       = buffer;
            fb.User       = tlFeedbackData;
         }

         ERROR error;
         if (!(error = delete_tree(buffer, sizeof(buffer), &fb)));
         else if (error != ERR_Cancelled) LogErrorMsg("Failed to delete folder \"%s\"", buffer);

         LogBack();
         return error;
      }
      else {
         LogError(0, ERR_ResolvePath);
         LogBack();
         return ERR_ResolvePath;
      }
   }
   else {
      LogBranch("Delete File: %s", Self->Path);

      CSTRING path;
      if (!GET_ResolvedPath(Self, &path)) {
         BYTE buffer[512];
         LONG len = StrCopy(path, buffer, sizeof(buffer));
         if ((buffer[len-1] IS '/') OR (buffer[len-1] IS '\\')) buffer[len-1] = 0;

         if (Self->Handle != -1) { close(Self->Handle); Self->Handle = -1; }

         // Unlinking the file deletes it

         if (!unlink(buffer)) {
            LogBack();
            return ERR_Okay;
         }
         else {
            LogErrorMsg("unlink() failed on file \"%s\": %s", buffer, strerror(errno));
            LogBack();
            return convert_errno(errno, ERR_Failed);
         }
      }
      else {
         LogError(0, ERR_ResolvePath);
         LogBack();
         return ERR_ResolvePath;
      }
   }
}

//****************************************************************************

static ERROR FILE_Free(objFile *Self, APTR Void)
{
   if (Self->prvWatch) Action(MT_FlWatch, &Self->Head, NULL);

#ifdef _WIN32
   STRING path = NULL;
   if (Self->Flags & FL_RESET_DATE) {
      // If we have to reset the date, get the file path
      MSG("Resetting the file date.");
      ResolvePath(Self->Path, 0, &path);
   }
#endif

   if (Self->prvIcon) { FreeMemory(Self->prvIcon); Self->prvIcon = NULL; }
   if (Self->ProgressDialog) { acFree(Self->ProgressDialog); Self->ProgressDialog = NULL; }
   if (Self->prvLine) { FreeMemory(Self->prvLine); Self->prvLine = NULL; }
   if (Self->Path)    { FreeMemory(Self->Path); Self->Path = NULL; }
   if (Self->prvList) { CloseDir(Self->prvList); Self->prvList = NULL; }
   if (Self->prvResolvedPath) { FreeMemory(Self->prvResolvedPath); Self->prvResolvedPath = NULL; }
   if (Self->prvLink) { FreeMemory(Self->prvLink); Self->prvLink = NULL; }
   if (Self->Buffer)  { FreeMemory(Self->Buffer); Self->Buffer = NULL; }

   if (Self->Handle != -1) {
      if (close(Self->Handle) IS -1) {
         #ifdef __unix__
            LogErrorMsg("Unix filesystem error: %s", strerror(errno));
         #endif
      }
      Self->Handle = -1;
   }

   if (Self->Stream) {
      #ifdef __unix__
         closedir(Self->Stream);
      #endif
      Self->Stream = NULL;
   }

#ifdef _WIN32
   if ((Self->Flags & FL_RESET_DATE) AND (path)) {
      winResetDate(path);
      FreeMemory(path);
   }
#endif

   return ERR_Okay;
}

/*****************************************************************************

-ACTION-
Init: Initialises a file.

This action will prepare a file or folder at the given #Path for use.

To create a new file from scratch, specify the NEW flag.  This will overwrite any file that exists at the target path.

To read and write data to the file, specify the READ and/or WRITE modes in the #Flags field prior to initialisation.
If a file is read-only and the WRITE and READ flags are set in combination, the WRITE flag will be dropped and
initialisation will continue as normal.

If neither of the NEW, READ or WRITE flags are specified, the file object is prepared and queried from disk (if it
exists), but will not be opened.  It will be necessary to #Activate() the file in order to open it.

The File class supports RAM based file buffering - this is activated by using the BUFFER flag and setting the Size
field to the desired buffer size.  A file path is not required unless the buffer needs to be filled with content on
initialisation.  Because buffered files exist virtually, their functionality is restricted to read/write access.

Strings can also be loaded into file buffers for read/write access.  This is achieved by specifying the
#Path `string:Data\0`, where Data is a sequence of characters to be loaded into a virtual memory space.

-ERRORS-
Okay:
MissingPath:
SetField: An error occurred while updating the Path field.
FileNotFound:
ResolvePath:
Search: The file could not be found.
NoPermission: Permission was denied when accessing or creating the file.

*****************************************************************************/

static ERROR FILE_Init(objFile *Self, APTR Void)
{
   LONG j, len;
   ERROR error;

   // If the BUFFER flag is set then the file will be located in RAM.  Very little initialisation is needed for this.
   // If a path has been specified, we'll load the entire file into memory.  Please see the end of this
   // initialisation routine for more info.

   if ((Self->Flags & FL_BUFFER) AND (!Self->Path)) {
      if (Self->Size < 0) Self->Size = 0;
      Self->Flags |= FL_READ|FL_WRITE;
      if (!Self->Buffer) {
         // Allocate buffer if none specified.  An extra byte is allocated for a NULL byte on the end, in case the file
         // content is treated as a string.

         if (AllocMemory((Self->Size < 1) ? 1 : Self->Size+1, Self->Head.MemFlags|MEM_NO_CLEAR, (APTR *)&Self->Buffer, NULL) != ERR_Okay) {
            return PostError(ERR_AllocMemory);
         }
         ((BYTE *)Self->Buffer)[Self->Size] = 0;
      }
      return ERR_Okay;
   }

   if (!Self->Path) return PostError(ERR_MissingPath);

   if (glDefaultPermissions) Self->Permissions = glDefaultPermissions;

   if (!StrCompare("string:", Self->Path, 7, 0)) {
      Self->Size = StrLength(Self->Path + 7);

      if (Self->Size > 0) {
         if (!AllocMemory(Self->Size, Self->Head.MemFlags, (APTR *)&Self->Buffer, NULL)) {
            Self->Flags |= FL_READ|FL_WRITE;
            CopyMemory(Self->Path + 7, Self->Buffer, Self->Size);
            return ERR_Okay;
         }
         else return PostError(ERR_AllocMemory);
      }
      else return PostError(ERR_Failed);
   }

   if ((!Self->Permissions) OR (Self->Permissions & PERMIT_INHERIT)) {
      struct FileInfo info;
      UBYTE namebuf[MAX_FILENAME];

      // If the file already exists, pull the permissions from it.  Otherwise use a default set of permissions (if
      // possible, inherit permissions from the file's folder).

      if ((Self->Flags & FL_NEW) AND (get_file_info(Self->Path, &info, sizeof(info), namebuf, sizeof(namebuf)) IS ERR_Okay)) {
         LogMsg("Using permissions of the original file.");
         Self->Permissions |= info.Permissions;
      }
      else {
#ifdef __unix__
         Self->Permissions |= get_parent_permissions(Self->Path, NULL, NULL) & (PERMIT_ALL_READ|PERMIT_ALL_WRITE);
         if (!Self->Permissions) Self->Permissions = PERMIT_READ|PERMIT_WRITE|PERMIT_GROUP_READ|PERMIT_GROUP_WRITE;
         else LogMsg("Inherited permissions: $%.8x", Self->Permissions);
#else
         Self->Permissions = PERMIT_READ|PERMIT_WRITE|PERMIT_GROUP_READ|PERMIT_GROUP_WRITE;
#endif
       }
   }

   // Do not do anything if the File is used as a static object in a script

   if ((Self->Static) AND ((!Self->Path) OR (!Self->Path[0]))) return ERR_Okay;

   if (Self->Path[0] IS ':') {
      MSG("Root folder initialised.");
      return ERR_Okay;
   }

   // If the FL_FOLDER flag was set after the Path field was set, we may need to reset the Path field so
   // that the trailing folder slash is added to it.

retrydir:
   if (Self->Flags & FL_FOLDER) {
      ULONG len = StrLength(Self->Path);
      if (len > 512) return PostError(ERR_BufferOverflow);

      if ((Self->Path[len-1] != '/') AND (Self->Path[len-1] != '\\') AND (Self->Path[len-1] != ':')) {
         UBYTE buffer[len+1];
         for (j=0; j < len; j++) buffer[j] = Self->Path[j];
         buffer[j] = 0;
         if (SetString(Self, FID_Path, buffer) != ERR_Okay) {
            return PostError(ERR_SetField);
         }
      }
   }

   if (Self->Stream) {
      MSG("Folder stream already set.");
      return ERR_Okay;
   }

   // Use RSF_CHECK_VIRTUAL to cause failure if the volume name is reserved by a support class.  By doing this we can
   // return ERR_UseSubClass and a support class can then initialise the file instead.

   LONG resolveflags = 0;
   if (Self->Flags & FL_NEW) resolveflags |= RSF_NO_FILE_CHECK;
   if (Self->Flags & FL_APPROXIMATE) resolveflags |= RSF_APPROXIMATE;

   if ((error = ResolvePath(Self->Path, resolveflags|RSF_CHECK_VIRTUAL, &Self->prvResolvedPath)) != ERR_Okay) {
      if (error IS ERR_VirtualVolume) {
         // For virtual volumes, update the path to ensure that the volume name is referenced in the path string.
         // Then return ERR_UseSubClass to have support delegated to the correct File sub-class.
         if (StrMatch(Self->Path, Self->prvResolvedPath) != ERR_Okay) {
            SET_Path(Self, Self->prvResolvedPath);
         }
         MSG("ResolvePath() reports virtual volume, will delegate to sub-class...");
         return ERR_UseSubClass;
      }
      else {
         // The file may path may actually be a folder.  Add a / and retest to see if this is the case.

         if (!(Self->Flags & FL_FOLDER)) {
            Self->Flags |= FL_FOLDER;
            goto retrydir;
         }

         LogMsg("File not found \"%s\".", Self->Path);
         return ERR_FileNotFound;
      }
   }

   len = StrLength(Self->prvResolvedPath);

   // Check if ResolvePath() resolved the path from a file string to a folder

   if ((!(Self->prvType & STAT_FOLDER)) AND (Self->prvResolvedPath[len-1] IS '/') AND (!(Self->Flags & FL_FOLDER))) {
      Self->Flags |= FL_FOLDER;
      goto retrydir;
   }

#ifdef __unix__
   // Establishing whether or not the path is a link is required on initialisation.
   struct stat64 info;
   if (Self->prvResolvedPath[len-1] IS '/') Self->prvResolvedPath[len-1] = 0; // For lstat64() symlink we need to remove the slash
   if (lstat64(Self->prvResolvedPath, &info) != -1) { // Prefer to get a stat on the link rather than the file it refers to
      if (S_ISLNK(info.st_mode)) Self->Flags |= FL_LINK;
   }
#endif

   if (Self->prvType & STAT_FOLDER) { // Open the folder
      if (Self->Flags & FL_FILE) { // Check if the user expected the source to be a file, not a folder
         return PostError(ERR_ExpectedFile);
      }

      Self->Flags |= FL_FOLDER;

      acQuery(&Self->Head);

      #ifdef __unix__
         if ((Self->Stream = opendir(Self->prvResolvedPath))) return ERR_Okay;
      #elif _WIN32
         // Note: The CheckDiretoryExists() function does not return a true handle, just a code of 1 to indicate that the folder is present.

         if ((Self->Stream = winCheckDirectoryExists(Self->prvResolvedPath))) return ERR_Okay;
      #else
         #error Require folder open or folder marking code.
      #endif

      if (Self->Flags & FL_NEW) {
         LogMsg("Making dir \"%s\", Permissions: $%.8x", Self->prvResolvedPath, Self->Permissions);
         if (!CreateFolder(Self->prvResolvedPath, Self->Permissions)) {
            #ifdef __unix__
               if (!(Self->Stream = opendir(Self->prvResolvedPath))) {
                  LogErrorMsg("Failed to open the folder after creating it.");
               }
            #elif _WIN32
               if (!(Self->Stream = winCheckDirectoryExists(Self->prvResolvedPath))) {
                  LogErrorMsg("Failed to open the folder after creating it.");
               }
            #else
               #error Require folder open or folder marking code.
            #endif

            return ERR_Okay;
         }
         else return PostError(ERR_CreateFile);
      }
      else {
         LogErrorMsg("Could not open folder \"%s\", %s.", Self->prvResolvedPath, strerror(errno));
         return ERR_File;
      }
   }
   else {
      Self->Flags |= FL_FILE;

      // Automatically open the file if access is required on initialisation.

      if (Self->Flags & (FL_NEW|FL_READ|FL_WRITE)) {
         ERROR error = acActivate(&Self->Head);
         if (!error) error = acQuery(&Self->Head);
         return error;
      }
      else return acQuery(&Self->Head);
   }
}

/*****************************************************************************

-METHOD-
Move: Moves a file to a new location.

This method is used to move the data of a file to another location.  If the file object represents a folder, then
the folder and all of its contents will be moved.  The file object must have been initialised with the FL_READ flag,
or the move operation will not work (this restriction does not apply to directories).  If a file already exists at the
destination path then it will be over-written with the new data.

The #Position field will be reset as a result of calling this method.

-INPUT-
cstr Dest: The desired path for the file.

-ERRORS-
Okay: The File was moved successfully.
NullArgs
Args
FieldNotSet: The Path field has not been set in the file object.
Failed

*****************************************************************************/

static ERROR FILE_MoveFile(objFile *Self, struct flMove *Args)
{
   if ((!Args) OR (!Args->Dest)) return PostError(ERR_NullArgs);
   if (!Self->Path) return PostError(ERR_FieldNotSet);

   STRING src   = Self->Path;
   CSTRING dest = Args->Dest;

   LONG len, i, j;
   for (len=0; dest[len]; len++);
   if (len <= 1) return PostError(ERR_Args);

   LogMsg("%s to %s", src, dest);

   if ((dest[len-1] IS '/') OR (dest[len-1] IS '\\') OR (dest[len-1] IS ':')) {
      // If a trailing slash has been specified, we are moving the file into a folder, rather than to a direct path.

      for (i=0; src[i]; i++);
      i--;
      if (src[i] IS ':') {
         LogErrorMsg("Moving volumes is illegal.");
         return ERR_Failed;
      }
      else if ((src[i] IS '/') OR (src[i] IS '\\')) i--;

      while ((i > 0) AND (src[i] != ':') AND (src[i] != '/') AND (src[i] != '\\')) {
         i--;
         len++;
      }

      STRING newpath;
      if (!AllocMemory(len + 1, MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, (APTR *)&newpath, NULL)) {
         for (j=0; dest[j]; j++) newpath[j] = dest[j];
         i++;
         while ((src[i]) AND (src[i] != '/') AND (src[i] != '\\')) newpath[j++] = src[i++];
         newpath[j] = 0;

         #ifdef _WIN32
            if (Self->Handle != -1) { close(Self->Handle); Self->Handle = -1; }
         #endif

         ERROR error;
         if (!(error = fs_copy(src, newpath, TRUE))) {
            FreeMemory(Self->Path);
            Self->Path = newpath;
         }
         else {
            LogErrorMsg("Failed to move %s to %s", src, newpath);
            FreeMemory(newpath);
         }
         return error;
      }
      else return PostError(ERR_AllocMemory);
   }
   else {
      STRING newpath;
      if (!AllocMemory(len+1, MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, (APTR *)&newpath, NULL)) {
         for (i=0; dest[i]; i++) newpath[i] = dest[i];
         newpath[i] = 0;

         #ifdef _WIN32
            if (Self->Handle != -1) { close(Self->Handle); Self->Handle = -1; }
         #endif

         ERROR error;
         if (!(error = fs_copy(src, newpath, TRUE))) {
            FreeMemory(Self->Path);
            Self->Path = newpath;
            return ERR_Okay;
         }
         else {
            FreeMemory(newpath);
            return PostError(error);
         }
      }
      else return PostError(ERR_AllocMemory);
   }
}

//****************************************************************************

static ERROR FILE_NewObject(objFile *Self, APTR Void)
{
   Self->Handle = -1;
   Self->Permissions = PERMIT_READ|PERMIT_WRITE|PERMIT_GROUP_READ|PERMIT_GROUP_WRITE;
   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
Next: Retrieve meta information describing the next indexed file in the folder list.

If a file object represents a folder, calling the Next() method will retrieve meta information about the next file
in the folder's index.  This information will be returned as a new File object that is partially initialised (the file
will not be opened, but information such as size, timestamps and permissions will be retrievable).

If desired, the resulting file object can be opened by setting the READ or WRITE bits in #Flags and then
calling the #Activate() action.

It is the responsibility of the caller to free the resulting File object once it is no longer required.

The file index can be reset by calling the #Reset() action.

-INPUT-
&!obj(File) File: A pointer to a new File object will be returned in this parameter if the call is successful.

-ERRORS-
Okay
Args
NullArgs
DirEmpty: The index has reached the end of the file list.

****************************************************************************/

static ERROR FILE_NextFile(objFile *Self, struct flNext *Args)
{
   if (!Args) return PostError(ERR_NullArgs);
   if (!(Self->Flags & FL_FOLDER)) return PostError(ERR_ExpectedFolder);

   if (!Self->prvList) {
      LONG flags = RDF_QUALIFY;

      if (Self->Flags & FL_EXCLUDE_FOLDERS) flags |= RDF_FILE;
      else if (Self->Flags & FL_EXCLUDE_FILES) flags |= RDF_FOLDER;
      else flags |= RDF_FILE|RDF_FOLDER;

      ERROR error = OpenDir(Self->Path, flags, &Self->prvList);
      if (error) return error;
   }

   ERROR error;
   if (!(error = ScanDir(Self->prvList))) {
      objFile *file;
      LONG folder_len = StrLength(Self->Path);
      LONG name_len = StrLength(Self->prvList->Info->Name);

      {
         UBYTE path[folder_len + name_len + 2];
         CopyMemory(Self->Path, path, folder_len);
         CopyMemory(Self->prvList->Info->Name, path + folder_len, name_len);
         path[folder_len + name_len] = 0;

         if (!CreateObject(ID_FILE, 0, (OBJECTPTR *)&file,
               FID_Path|TSTR, path,
               TAGEND)) {

            Args->File = file;
            return ERR_Okay;
         }
         else return PostError(ERR_CreateObject);
      }
   }
   else {
      // Automatically close the list in the event of an error and repurpose the return code.  Subsequent
      // calls to Next() will start from the start of the file index.
      CloseDir(Self->prvList);
      Self->prvList = NULL;
   }

   return error;
}

/*****************************************************************************
-ACTION-
Query: Read a file's meta information from source.
-END-
*****************************************************************************/

static ERROR FILE_Query(objFile *Self, APTR Void)
{
#ifdef _WIN32


   return ERR_Okay;
#else
   return ERR_Okay;
#endif
}

/*****************************************************************************

-ACTION-
Read: Reads data from a file.

Reads data from a file into the given buffer.  Increases the value in the #Position field by the amount of
bytes read from the file data.  The FL_READ bit in the #Flags field must have been set on file initialisation,
or the call will fail.

It is possible for this call to report success even in the event that no data has been read from the file.  This is
typical when reading from streamed file sources.

-ERRORS-
Okay: The file information was read into the buffer.
Args
NullArgs
FileReadFlag: The FL_READ flag was not specified on initialisation.
Failed: The file object refers to a folder, or the object is corrupt.
-END-

****************************************************************************/

static ERROR FILE_Read(objFile *Self, struct acRead *Args)
{
   if ((!Args) OR (!Args->Buffer)) return PostError(ERR_NullArgs);
   else if (Args->Length == 0) return ERR_Okay;
   else if (Args->Length < 0) return ERR_OutOfRange;

   if (!(Self->Flags & FL_READ)) return PostError(ERR_FileReadFlag);

   if (Self->Buffer) {
      if (Self->Flags & FL_LOOP) {
         // In loop mode, we must make the file buffer appear to be of infinite length in terms of the read/write
         // position marker.

         BYTE *dest = Args->Buffer;
         LONG len, readlen;
         for (readlen=Args->Length; readlen > 0; readlen -= len) {
            len = Self->Size - (Self->Position % Self->Size); // Calculate amount of space ahead of us.
            if (len > readlen) len = readlen; // Restrict the length of the read operation to the length of the destination.

            CopyMemory(Self->Buffer + (Self->Position % Self->Size), dest, len);
            dest += len;
            Self->Position += len;
         }
/*
         readlen = Self->Position % Self->Size;
         for (len=0; len < Args->Length; len++) {
            ((BYTE *)Args->Buffer)[len] = Self->Buffer[readlen++];
            if (readlen >= Self->Size) readlen = 0;
         }
         Self->Position += Args->Length;
*/
         Args->Result = Args->Length;
         return ERR_Okay;
      }
      else {
         if (Self->Position + Args->Length > Self->Size) Args->Result = Self->Size - Self->Position;
         else Args->Result = Args->Length;
         CopyMemory(Self->Buffer + Self->Position, Args->Buffer, Args->Result);
         Self->Position += Args->Result;
         return ERR_Okay;
      }
   }

   if (Self->prvType & STAT_FOLDER) return PostError(ERR_ExpectedFile);

   if (Self->Handle IS -1) return ERR_NotInitialised;

   Args->Result = read(Self->Handle, Args->Buffer, (LONG)Args->Length);

   if (Args->Result != Args->Length) {
      if (Args->Result IS -1) {
         LogMsg("Failed to read %d bytes from the file.", Args->Length);
         Args->Result = 0;
         return ERR_SystemCall;
      }

      // Return ERR_Okay because even though not all data was read, this was not due to a failure.

      LogF("5Read()","%d of the intended %d bytes were read from the file.", Args->Result, Args->Length);
      Self->Position += Args->Result;
      return ERR_Okay;
   }
   else {
      Self->Position += Args->Result;
      return ERR_Okay;
   }
}

/*****************************************************************************

-METHOD-
ReadLine: Reads the next line from the file.

Reads one line from the file into an internal buffer, which is returned in the Result argument.  Reading a line will
increase the #Position field by the amount of bytes read from the file.  You must have set the FL_READ bit in
the #Flags field when you initialised the file, or the call will fail.

The line buffer is managed internally, so there is no need for you to free the result string.  This method returns
ERR_NoData when it runs out of information to read from the file.

-INPUT-
&str Result: The resulting string is returned in this parameter.

-ERRORS-
Okay: The file information was read into the buffer.
Args
FileReadFlag: The FL_READ flag was not specified on initialisation.
Failed: The file object refers to a folder.
ObjectCorrupt: The internal file handle is missing.
BufferOverflow: The line is too long for the read routine (4096 byte limit).
NoData: There is no more data left to read.

****************************************************************************/

static ERROR FILE_ReadLine(objFile *Self, struct flReadLine *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   if (!(Self->Flags & FL_READ)) return PostError(ERR_FileReadFlag);

   LONG i, len;
   UBYTE line[4096];
   LONG pos = Self->Position;
   if (Self->Buffer) {
      len = 0;
      i = Self->Position;
      while ((i < Self->Size) AND (len < sizeof(line)-1)) {
         line[len] = Self->Buffer[i++];
         if (line[len] IS '\n') {
            break; // Break once a line-feed is encountered
         }
         len++;
      }
      line[len] = 0;
      Self->Position = i;
   }
   else {
      if (Self->prvType & STAT_FOLDER) return PostError(ERR_ExpectedFile);
      if (Self->Handle IS -1) return PostError(ERR_ObjectCorrupt);

      // Read the line

      LONG result;
      LONG bytes = 256;
      len = 0;
      while ((result = read(Self->Handle, line+len, bytes)) > 0) {
         for (i=0; i < result; i++) {
            if (line[len] IS '\n') break;
            if (++len >= sizeof(line)) {
               // Buffer overflow
               lseek64(Self->Handle, Self->Position, SEEK_SET); // Reset the file position back to normal
               return PostError(ERR_BufferOverflow);
            }
         }
         if (line[len] IS '\n') break;

         if (len + bytes > sizeof(line)) bytes = sizeof(line) - len;
      }

      Self->Position += len;

      if (line[len] IS '\n') {
         Self->Position++; // Add 1 to skip the line feed
         lseek64(Self->Handle, Self->Position, SEEK_SET); // Reset the file position to the start of the next line
      }

      line[len] = 0;
   }

   if (Self->Position IS pos) return ERR_NoData;

   if (Self->prvLineLen >= len+1) {
      CopyMemory(line, Self->prvLine, len+1);
      Args->Result = Self->prvLine;
      return ERR_Okay;
   }
   else {
      if (Self->prvLine) { FreeMemory(Self->prvLine); Self->prvLine = NULL; }
      Self->prvLine    = StrClone(line);
      Self->prvLineLen = len + 1;
      Args->Result = Self->prvLine;
      return ERR_Okay;
   }
}

/*****************************************************************************
-ACTION-
Rename: Changes the name of a file.
-END-
*****************************************************************************/

static ERROR FILE_Rename(objFile *Self, struct acRename *Args)
{
   LONG namelen, i, j;
   STRING new;

   if ((!Args) OR (!Args->Name)) return PostError(ERR_NullArgs);
   for (namelen=0; Args->Name[namelen]; namelen++);
   if (!namelen) return PostError(ERR_Args);

   if (!Self->Path) return PostError(ERR_FieldNotSet);

   LogBranch("%s to %s", Self->Path, Args->Name);

   for (i=0; Self->Path[i]; i++);

   if ((Self->prvType & STAT_FOLDER) OR (Self->Flags & FL_FOLDER)) {
      if (Self->Path[i-1] IS ':') { // Renaming a volume
         if (!AllocMemory(namelen+2, MEM_STRING|Self->Head.MemFlags, (APTR *)&new, NULL)) {
            for (i=0; (Args->Name[i]) AND (Args->Name[i] != ':') AND (Args->Name[i] != '/') AND (Args->Name[i] != '\\'); i++) new[i] = Args->Name[i];
            new[i] = 0;
            if (!RenameVolume(Self->Path, new)) {
               new[i++] = ':';
               new[i++] = 0;
               FreeMemory(Self->Path);
               Self->Path = new;
               LogBack();
               return ERR_Okay;
            }
            else {
               FreeMemory(new);
               LogBack();
               return PostError(ERR_Failed);
            }
         }
         else {
            LogBack();
            return PostError(ERR_AllocMemory);
         }
      }
      else {
         // We are renaming a folder
         for (--i; (i > 0) AND (Self->Path[i-1] != ':') AND (Self->Path[i-1] != '/') AND (Self->Path[i-1] != '\\'); i--);

         if (!AllocMemory(i+namelen+2, MEM_STRING|Self->Head.MemFlags, (APTR *)&new, NULL)) {
            for (j=0; j < i; j++) new[j] = Self->Path[j];

            for (i=0; (Args->Name[i]) AND (Args->Name[i] != '/') AND (Args->Name[i] != '\\') AND (Args->Name[i] != ':'); i++) {
               new[j++] = Args->Name[i];
            }

            if (!fs_copy(Self->Path, new, TRUE)) {
               // Add the trailing slash
               if (new[j-1] != '/') new[j++] = '/';
               new[j] = 0;

               FreeMemory(Self->Path);
               Self->Path = new;
               LogBack();
               return ERR_Okay;
            }
            else {
               FreeMemory(new);
               LogBack();
               return PostError(ERR_Failed);
            }
         }
         else {
            LogBack();
            return PostError(ERR_AllocMemory);
         }
      }
   }
   else { // We are renaming a file
      while ((i > 0) AND (Self->Path[i-1] != ':') AND (Self->Path[i-1] != '/') AND (Self->Path[i-1] != '\\')) i--;
      if (!AllocMemory(i+namelen+1, MEM_STRING|Self->Head.MemFlags, (APTR *)&new, NULL)) {
         // Generate the new path, then rename the file

         for (j=0; j < i; j++) new[j] = Self->Path[j];
         for (i=0; Args->Name[i]; i++);
         while ((i > 0) AND (Args->Name[i] != '/') AND (Args->Name[i] != '\\') AND (Args->Name[i] != ':')) i--;
         if ((Args->Name[i] IS '/') OR (Args->Name[i] IS '\\') OR (Args->Name[i] IS ':')) i++;
         while (Args->Name[i]) new[j++] = Args->Name[i++];
         new[j] = 0;

         #ifdef _WIN32
            if (Self->Handle != -1) { close(Self->Handle); Self->Handle = -1; }
         #endif

         if (!fs_copy(Self->Path, new, TRUE)) {
            FreeMemory(Self->Path);
            Self->Path = new;
            LogBack();
            return ERR_Okay;
         }
         else {
            FreeMemory(new);
            LogBack();
            return PostError(ERR_Failed);
         }
      }
      else {
         LogBack();
         return PostError(ERR_AllocMemory);
      }
   }
}

/*****************************************************************************
-ACTION-
Reset: If the file represents a folder, the file list index is reset by this action.
-END-
*****************************************************************************/

static ERROR FILE_Reset(objFile *Self, APTR Void)
{
   if (Self->Flags & FL_FOLDER) {
      if (Self->prvList) { CloseDir(Self->prvList); Self->prvList = NULL; }
   }

   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
Seek: Seeks to a new read/write position within a file.
-END-
*****************************************************************************/

static ERROR FILE_Seek(objFile *Self, struct acSeek *Args)
{
   LARGE oldpos = Self->Position;

   // Set the new setting for the Self->Position field

   if (Args->Position IS SEEK_START) {
      Self->Position = (LARGE)Args->Offset;
   }
   else if (Args->Position IS SEEK_END) {
      LARGE filesize;
      GetLarge(Self, FID_Size, &filesize);
      Self->Position = filesize - (LARGE)Args->Offset;
   }
   else if (Args->Position IS SEEK_CURRENT) {
      Self->Position = Self->Position + (LARGE)Args->Offset;
   }
   else return PostError(ERR_Args);

   // Make sure we are greater than zero, otherwise set as zero

   if (Self->Position < 0) Self->Position = 0;

   if (Self->Buffer) {
      if (Self->Flags & FL_LOOP) return ERR_Okay; // In loop mode, the position marker can legally be above the buffer size
      else if (Self->Position > Self->Size) Self->Position = Self->Size;
      return ERR_Okay;
   }

   if (Self->Handle IS -1) return PostError(ERR_ObjectCorrupt);

   LARGE ret;
   if ((ret = lseek64(Self->Handle, Self->Position, SEEK_SET)) != Self->Position) {
      LogErrorMsg("Failed to Seek to new position of " PF64() " (return " PF64() ").", Self->Position, ret);
      Self->Position = oldpos;
      return ERR_SystemCall;
   }

   return ERR_Okay;
}

/*****************************************************************************

-METHOD-
SetDate: Sets the date on a file.

The SetDate method provides a convenient way to set the date and time information for a file object.  Date information
is set in a human readable year, month, day, hour, minute and second format for your convenience.

Depending on the filesystem type, multiple forms of datestamp may be supported.  The default datestamp, FDT_MODIFIED
defines the time at which the file data was last altered.  Other types include the date on which the file was created
and the date it was last archived (backed up).  The following types are supported by the Type argument:

<types lookup="FDT"/>

If the specified datestamp is not supported by the filesystem, ERR_NoSupport is returned by this method.

-INPUT-
int Year: Year (-ve for BC, +ve for AD).
int Month: Month (1 - 12)
int Day: Day (1 - 31)
int Hour: Hour (0 - 23
int Minute: Minute (0 - 59)
int Second: Second (0 - 59)
int(FDT) Type: The type of date to set (filesystem dependent).

-ERRORS-
Okay
NullArgs
ResolvePath
SystemCall
NoSupport: The platform does not support file date setting.

*****************************************************************************/

static ERROR FILE_SetDate(objFile *Self, struct flSetDate *Args)
{
   if (!Args) return PostError(ERR_NullArgs);

   LogMsg("%d/%d/%d %.2d:%.2d:%.2d", Args->Day, Args->Month, Args->Year, Args->Hour, Args->Minute, Args->Second);

   #ifdef _WIN32
      CSTRING path;
      if (!GET_ResolvedPath(Self, &path)) {
         if (winSetFileTime(path, Args->Year, Args->Month, Args->Day, Args->Hour, Args->Minute, Args->Second)) {
            Self->Flags |= FL_RESET_DATE;
            return ERR_Okay;
         }
         else return PostError(ERR_SystemCall);
      }
      else return ERR_ResolvePath;

   #elif __unix__

      CSTRING path;
      if (!GET_ResolvedPath(Self, &path)) {
         struct tm time;
         time.tm_year  = Args->Year - 1900;
         time.tm_mon   = Args->Month - 1;
         time.tm_mday  = Args->Day;
         time.tm_hour  = Args->Hour;
         time.tm_min   = Args->Minute;
         time.tm_sec   = Args->Second;
         time.tm_isdst = -1;
         time.tm_wday  = 0;
         time.tm_yday  = 0;

         struct timeval filetime[2];
         if ((filetime[0].tv_sec = mktime(&time)) != -1) {
            filetime[0].tv_usec = 0;
            filetime[1] = filetime[0];

            if (utimes(path, filetime) != -1) {
               Self->Flags |= FL_RESET_DATE;
               return ERR_Okay;
            }
            else {
               LogErrorMsg("Failed to set the file date.");
               return PostError(ERR_SystemCall);
            }
         }
         else return PostError(ERR_SystemCall);
      }
      else return ERR_ResolvePath;

   #else
      return ERR_NoSupport;
   #endif
}

/*****************************************************************************

-METHOD-
StartStream: Starts streaming data from a file source.

If a file object is a stream (indicated by the STREAM flag), the StartStream method should be used for reading or
writing data to the file object.  Although it is possible to call the Read and Write actions on streamed files, they
will be limited to returning only the amount of data that is cached locally (if any), or writing as much as buffers
will allow in software.

A single file object can support read or write streams (pass FL_READ or FL_WRITE in the Flags parameter).  However,
only one of the two can be active at any time.  To switch between read and write modes, the stream must be stopped with
the #StopStream() method and then restarted with StartStream.

A stream can be limited by setting the Length parameter to a non-zero value.

If the StartStream request is successful, the file object will return action notifications to the Subscriber to
indicate activity on the file stream.  When reading from a stream, AC_Write notifications will be received to indicate
that new data has been written to the file cache.  The Buffer parameter of the reported acWrite structure may refer to
a private address that contains the data that was received from the stream and the Result indicates the amount of new
data available.

When writing to a stream, AC_Read notifications will be received to indicate that the stream is ready to accept more
data.  The Result parameter will indicate the maximum amount of data that should be written to the stream using the
#Write() action.

A stream can be cancelled at any time by calling #StopStream().

-INPUT-
oid Subscriber: Reference to an object that will receive streamed data notifications.
int(FL) Flags: Use FL_READ for incoming data, FL_WRITE for outgoing data.
int Length: Limits the total amount of data to be streamed.

-ERRORS-
Okay
Args
NoSupport: The file is not streamed.

*****************************************************************************/

static ERROR FILE_StartStream(objFile *Self, struct flStartStream *Args)
{
   if ((!Args) OR (!Args->SubscriberID)) return PostError(ERR_NullArgs);

   // Streaming from standard files is pointless - it's the virtual drives that provide streaming features.

   return ERR_NoSupport;
}

/*****************************************************************************

-METHOD-
StopStream: Stops streaming data from a file source.

This method terminates data streaming from a file (instantiated by the #StartStream() method).  Any resources
related to the streaming process will be deallocated.

-ERRORS-
Okay
Args
NoSupport: The file is not streamed.

*****************************************************************************/

static ERROR FILE_StopStream(objFile *Self, APTR Void)
{
   return ERR_NoSupport;
}

/*****************************************************************************

-METHOD-
Watch: Monitors files and folders for file system events.

The WatchFile() function configures event based reporting for changes to any file or folder in the file system.
The capabilities of this method are dependent on the host platform, with Windows and Linux systems being able to
support most of the current feature set.

The path that will be monitored is determined by the File object's #Path field.  Both files and folders
are supported as targets.

The optional MFF Flags are used to filter events to those that are desired for monitoring.

The client must provide a Callback that will be triggered when a monitored event is triggered.  The Callback must
follow the format `ERROR Routine(*File, STRING Path, LARGE Custom, LONG Flags)`

Each event will be delivered in the sequence that they are originally raised.  The Flags parameter will reflect the
specific event that has occurred.  The Custom parameter is identical to the Custom argument originally passed to this
method.  The Path is a string that is relative to the File's #Path field.

If the callback routine returns ERR_Terminate, the watch will be disabled.  It is also possible to disable an existing
watch by calling this method with no parameters, or by setting the Flags parameter to 0.

-INPUT-
ptr(func) Callback: The routine that will be called when a file change is triggered by the system.
large Custom: A custom 64-bit value that will passed to the Callback routine as a parameter.
int(MFF) Flags: Filter events to those indicated in these flags.

-ERRORS-
Okay
Args
NullArgs

*****************************************************************************/

static ERROR FILE_Watch(objFile *Self, struct flWatch *Args)
{
   LogF("~","%s, Flags: $%.8x", Self->Path, (Args) ? Args->Flags : 0);

   // Drop any previously configured watch.

   if (Self->prvWatch) {
      LONG v;
      for (v=0; v < glVirtualTotal; v++) {
         if (glVirtual[v].VirtualID IS Self->prvWatch->VirtualID) {
            if (glVirtual[v].IgnoreFile) glVirtual[v].IgnoreFile(Self);
            break;
         }
      }
      if (v IS glVirtualTotal) LogErrorMsg("Failed to find virtual volume ID #%d", Self->prvWatch->VirtualID);

      FreeMemory(Self->prvWatch);
      Self->prvWatch = NULL;
   }

   if ((!Args) OR (!Args->Callback) OR (!Args->Flags)) {
      LogBack();
      return ERR_Okay;
   }

#ifdef __linux__ // Initialise inotify if not done already.
   if (glInotify IS -1) {
      ERROR error;
      if ((glInotify = inotify_init()) != -1) {
         fcntl(glInotify, F_SETFL, fcntl(glInotify, F_GETFL)|O_NONBLOCK);
         error = RegisterFD(glInotify, RFD_READ, (APTR)path_monitor, NULL);
      }
      else error = PostError(ERR_SystemCall);

      if (error) { LogBack(); return error; }
   }
#endif

   CSTRING resolve;
   ERROR error;
   if (!(error = GET_ResolvedPath(Self, &resolve))) {
      const struct virtual_drive *virtual = get_fs(resolve);

      if (virtual->WatchPath) {
         #ifdef _WIN32
         if (!AllocMemory(sizeof(struct rkWatchPath) + winGetWatchBufferSize(), MEM_DATA, (APTR *)&Self->prvWatch, NULL)) {
         #else
         if (!AllocMemory(sizeof(struct rkWatchPath), MEM_DATA, (APTR *)&Self->prvWatch, NULL)) {
         #endif
            Self->prvWatch->VirtualID = virtual->VirtualID;
            Self->prvWatch->Routine   = *Args->Callback;
            Self->prvWatch->Flags     = Args->Flags;
            Self->prvWatch->Custom    = Args->Custom;

            error = virtual->WatchPath(Self);
         }
         else error = ERR_AllocMemory;
      }
      else error = ERR_NoSupport;
   }

   LogBack();
   return error;
}

/*****************************************************************************

-ACTION-
Write: Writes data to a file.

Writes data from the provided buffer into the file, then updates the #Position field to reflect the new
read/write position.  You must have set the FL_WRITE bit in the #Flags field when you initialised the file, or
the call will fail.

-ERRORS-
Okay: All of the data was written to the file.
Args:
NullArgs:
ReallocMemory:
ExpectedFile:
ObjectCorrupt:
FileWriteFlag: The FL_WRITE flag was not specified when initialising the file.
LimitedSuccess: Only some of the data was written to the file.  Check the Result parameter to see how much data was written.
-END-

*****************************************************************************/

static ERROR FILE_Write(objFile *Self, struct acWrite *Args)
{
   if (!Args) return PostError(ERR_NullArgs);
   if (Args->Length <= 0) return ERR_Args;

   if (!(Self->Flags & FL_WRITE)) return PostError(ERR_FileWriteFlag);

   if (Self->Buffer) {
      if (Self->Flags & FL_LOOP) {
         // In loop mode, we must make the file buffer appear to be of infinite length in terms of the read/write
         // position marker.

         const char *src = Args->Buffer;
         LONG len, writelen;
         for (writelen=Args->Length; writelen > 0; writelen -= len) {
            len = Self->Size - (Self->Position % Self->Size); // Calculate amount of space ahead of us.
            if (len > writelen) len = writelen; // Restrict the length to the requested amount to write.

            CopyMemory(src, Self->Buffer + (Self->Position % Self->Size), len);
            src += len;
            Self->Position += len;
         }

/*
         writelen = Self->Position % Self->Size;
         for (len=0; len < Args->Length; len++) {
            Self->Buffer[writelen++] = ((BYTE *)Args->Buffer)[len];
            if (writelen >= Self->Size) writelen = 0;
         }
         Self->Position += Args->Length;
*/

         Args->Result = Args->Length;
         return ERR_Okay;
      }
      else {
         if (Self->Position + Args->Length > Self->Size) {
            // Increase the size of the buffer to cater for the write.  A null byte (not included in the official size)
            // is always placed at the end.
            if (!ReallocMemory(Self->Buffer, Self->Position + Args->Length + 1, (APTR *)&Self->Buffer, NULL)) {
               Self->Size = Self->Position + Args->Length;
               Self->Buffer[Self->Size] = 0;
            }
            else return PostError(ERR_ReallocMemory);
         }

         Args->Result = Args->Length;

         CopyMemory(Args->Buffer, Self->Buffer + Self->Position, Args->Result);

         Self->Position += Args->Result;
         return ERR_Okay;
      }
   }

   if ((Self->prvType & STAT_FOLDER) OR (Self->Flags & FL_FOLDER)) return PostError(ERR_ExpectedFile);

   if (Self->Handle IS -1) return PostError(ERR_ObjectCorrupt);

   // If no buffer was supplied then we will write out null values to a limit indicated by the Length field.

   if (!Args->Buffer) {
      LONG i;
      UBYTE nullbyte = 0;

      Args->Result = 0;
      for (i=0; i < Args->Length; i++) {
         LONG result = write(Self->Handle, &nullbyte, 1);
         if (result IS -1) break;
         else {
            Self->Position += result;
            Args->Result += result;
         }
      }

      if (Self->Position > Self->Size) Self->Size = Self->Position;
   }
   else {
      Args->Result = write(Self->Handle, Args->Buffer, Args->Length);

      if (Args->Result > -1) {
         Self->Position += Args->Result;
         if (Self->Position > Self->Size) Self->Size = Self->Position;
      }
      else Args->Result = 0;
   }

   if (Args->Result != Args->Length) {
      LogF("5","%d of the intended %d bytes were written to the file.", Args->Result, Args->Length);
      return ERR_LimitedSuccess;
   }

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Buffer: Points to the internal data buffer if the file content is held in memory.

If a file has been created with an internal buffer (by setting the BUFFER flag on creation), this field will point to
the address of that buffer.  The size of the buffer will match the #Size field.

*****************************************************************************/

static ERROR GET_Buffer(objFile *Self, APTR *Value, LONG *Elements)
{
   *Value = Self->Buffer;
   *Elements = Self->Size;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Created: The creation date stamp of the file.

The Date field allows you to obtain the time at which the file was last date-stamped, or you can use it to set a new
file date.  By default, the 'modification date' is targeted by this field's support routine.  Please note that if the
file is open for writing, then date-stamped, then written; the file system driver will normally change the date stamp
to the time at which the file was last modified.

To simplify time management, information is read and set via a &DateTime structure.

*****************************************************************************/

static ERROR GET_Created(objFile *Self, struct DateTime **Value)
{
   *Value = 0;

   if (Self->Handle != -1) {
      struct stat64 stats;
      if (!fstat64(Self->Handle, &stats)) {
         // Timestamp has to match that produced by fs_getinfo()

         struct tm *local;
         #ifdef _WIN32
         if ((local = _localtime64(&stats.st_mtime))) {
         #else
         if ((local = localtime(&stats.st_mtime))) {
         #endif
            Self->prvCreated.Year   = 1900 + local->tm_year;
            Self->prvCreated.Month  = local->tm_mon + 1;
            Self->prvCreated.Day    = local->tm_mday;
            Self->prvCreated.Hour   = local->tm_hour;
            Self->prvCreated.Minute = local->tm_min;
            Self->prvCreated.Second = local->tm_sec;

            *Value = &Self->prvCreated;
            return ERR_Okay;
         }
         else return PostError(ERR_SystemCall);
      }
      else return PostError(ERR_SystemCall);
   }
   else {
      CSTRING path;
      ERROR error;
      if (!GET_ResolvedPath(Self, &path)) {
         BYTE buffer[512];
         LONG len = StrCopy(path, buffer, sizeof(buffer));
         if ((buffer[len-1] IS '/') OR (buffer[len-1] IS '\\')) buffer[len-1] = 0;

         struct stat64 stats;
         if (!stat64(buffer, &stats)) {
            // Timestamp has to match that produced by fs_getinfo()

            struct tm *local;
            #ifdef _WIN32
            if ((local = _localtime64(&stats.st_mtime))) {
            #else
            if ((local = localtime(&stats.st_mtime))) {
            #endif
               Self->prvCreated.Year   = 1900 + local->tm_year;
               Self->prvCreated.Month  = local->tm_mon + 1;
               Self->prvCreated.Day    = local->tm_mday;
               Self->prvCreated.Hour   = local->tm_hour;
               Self->prvCreated.Minute = local->tm_min;
               Self->prvCreated.Second = local->tm_sec;

               *Value = &Self->prvCreated;
               error = ERR_Okay;
            }
            else error = PostError(ERR_SystemCall);
         }
         else error = PostError(ERR_SystemCall);
      }
      else error = PostError(ERR_ResolvePath);

      return error;
   }
}

/*****************************************************************************
-FIELD-
Date: The 'last modified' date stamp on the file.

The Date field reflects the time at which the file was last modified.  It can also be used to set a new modification
date.  Please note that if the file is open for writing, then date-stamped, then modified; the file system driver
will overwrite the previously defined date stamp with the time at which the file was last written.

Information is read and set using a standard &DateTime structure.

*****************************************************************************/

static ERROR GET_Date(objFile *Self, struct DateTime **Value)
{
   *Value = 0;

   if (Self->Handle != -1) {
      struct stat64 stats;
      ERROR error;
      if (!fstat64(Self->Handle, &stats)) {
         // Timestamp has to match that produced by fs_getinfo()

         struct tm *local;
         #ifdef _WIN32
         if ((local = _localtime64(&stats.st_mtime))) {
         #else
         if ((local = localtime(&stats.st_mtime))) {
         #endif
            Self->prvModified.Year   = 1900 + local->tm_year;
            Self->prvModified.Month  = local->tm_mon + 1;
            Self->prvModified.Day    = local->tm_mday;
            Self->prvModified.Hour   = local->tm_hour;
            Self->prvModified.Minute = local->tm_min;
            Self->prvModified.Second = local->tm_sec;

            *Value = &Self->prvModified;
            error = ERR_Okay;
         }
         else error = PostError(ERR_SystemCall);
      }
      else error = PostError(ERR_SystemCall);

      return error;
   }
   else {
      CSTRING path;
      ERROR error;
      if (!GET_ResolvedPath(Self, &path)) {
         BYTE buffer[512];
         LONG len = StrCopy(path, buffer, sizeof(buffer));
         if ((buffer[len-1] IS '/') OR (buffer[len-1] IS '\\')) buffer[len-1] = 0;

         struct stat64 stats;
         if (!stat64(buffer, &stats)) {
            // Timestamp has to match that produced by fs_getinfo()

            struct tm *local;
            #ifdef _WIN32
            if ((local = _localtime64(&stats.st_mtime))) {
            #else
            if ((local = localtime(&stats.st_mtime))) {
            #endif
               Self->prvModified.Year   = 1900 + local->tm_year;
               Self->prvModified.Month  = local->tm_mon + 1;
               Self->prvModified.Day    = local->tm_mday;
               Self->prvModified.Hour   = local->tm_hour;
               Self->prvModified.Minute = local->tm_min;
               Self->prvModified.Second = local->tm_sec;

               *Value = &Self->prvModified;
               error = ERR_Okay;
            }
            else error = PostError(ERR_SystemCall);
         }
         else error = PostError(ERR_SystemCall);
      }
      else error = PostError(ERR_ResolvePath);

      return error;
   }
}

ERROR SET_Date(objFile *Self, struct DateTime *Date)
{
   if (!Date) return PostError(ERR_NullArgs);

#ifdef _WIN32
   CSTRING path;
   if (!GET_ResolvedPath(Self, &path)) {
      if (winSetFileTime(path, Date->Year, Date->Month, Date->Day, Date->Hour, Date->Minute, Date->Second)) {
         Self->Flags |= FL_RESET_DATE;
         return ERR_Okay;
      }
      else return PostError(ERR_SystemCall);
   }
   else return PostError(ERR_ResolvePath);

#elif __unix__

   CSTRING path;
   time_t datetime;
   struct utimbuf utm;
   if (!GET_ResolvedPath(Self, &path)) {
      struct tm time;
      time.tm_year  = Date->Year - 1900;
      time.tm_mon   = Date->Month - 1;
      time.tm_mday  = Date->Day;
      time.tm_hour  = Date->Hour;
      time.tm_min   = Date->Minute;
      time.tm_sec   = Date->Second;
      time.tm_isdst = -1;
      time.tm_wday  = 0;
      time.tm_yday  = 0;

      if ((datetime = mktime(&time)) != -1) {
         utm.modtime = datetime;
         utm.actime  = datetime;

         if (utime(path, &utm) != -1) {
            Self->Flags |= FL_RESET_DATE;
            return ERR_Okay;
         }
         else return PostError(ERR_SystemCall);
      }
      else return PostError(ERR_SystemCall);
   }
   else return ERR_ResolvePath;

#else
   return ERR_NoSupport;
#endif
}

/*****************************************************************************

-FIELD-
Flags: File flags and options.

-FIELD-
Group: Retrieve or change the group ID of a file.

The group ID assigned to a file can be read from this field.  The ID is retrieved from the file system in real time in
case the ID has been changed after initialisation of the file object.

You can also change the group ID of a file by writing an integer value to this field.

If the file system does not support group ID's, ERR_NoSupport is returned.

*****************************************************************************/

static ERROR GET_Group(objFile *Self, LONG *Value)
{
#ifdef __unix__
   struct stat64 info;

   if (fstat64(Self->Handle, &info) IS -1) {
      return ERR_FileNotFound;
   }

   *Value = info.st_gid;
   return ERR_Okay;
#else
   return ERR_NoSupport;
#endif
}

static ERROR SET_Group(objFile *Self, LONG Value)
{
#ifdef __unix__
   if (Self->Head.Flags & NF_INITIALISED) {
      LogMsg("Changing group to #%d", Value);
      if (!fchown(Self->Handle, -1, Value)) {
         return ERR_Okay;
      }
      else return PostError(convert_errno(errno, ERR_Failed));
   }
   else return PostError(ERR_NotInitialised);
#else
   return ERR_NoSupport;
#endif
}

/****************************************************************************
-FIELD-
Handle: The native system handle for the file opened by the file object.

This field returns the native file system handle for the file opened by the file object.  The native handle may be an
integer or pointer value in 32 or 64-bit format.  In order to manage this issue in a multi-platform manner, the value
is returned as a 64-bit integer.

****************************************************************************/

static ERROR GET_Handle(objFile *Self, LARGE *Value)
{
   *Value = Self->Handle;
   return ERR_Okay;
}

/****************************************************************************
-FIELD-
Icon: A path to an icon image that is suitable for representing the file in a user interface.

This field returns the name of the best icon to use when representing the file to the user, for instance in a file
list.  The icon style is determined by analysing the File's #Path.

The resulting string is returned in the format `icons:category/name` and can be opened with the @Picture class.

****************************************************************************/

static ERROR GET_Icon(objFile *Self, CSTRING *Value)
{
   if (Self->prvIcon) {
      *Value = Self->prvIcon;
      return ERR_Okay;
   }

   OBJECTPTR context = SetContext(&Self->Head);

   if ((!Self->Path) OR (!Self->Path[0])) {
      *Value = Self->prvIcon = StrClone("icons:filetypes/empty");
      SetContext(context);
      return ERR_Okay;
   }

   // If the location is a volume, look the icon up in the SystemVolumes object

   LONG i;
   for (i=0; (Self->Path[i]) AND (Self->Path[i] != ':'); i++);

   if ((Self->Path[i] IS ':') AND (!Self->Path[i+1])) {
      char icon[40] = "icons:folders/folder";

      if (!AccessPrivateObject((OBJECTPTR)glVolumes, 8000)) {
         struct ConfigEntry *entries;
         if ((entries = glVolumes->Entries)) {
            UBYTE volume[40];
            if (i >= sizeof(volume)) i = sizeof(volume) - 1;
            volume[CharCopy(Self->Path, volume, i)] = 0;

            for (i=0; i < glVolumes->AmtEntries; i++) {
               if ((!StrMatch("Name", entries[i].Key)) AND (!StrMatch(volume, entries[i].Data))) {
                  while ((i > 0) AND (!StrMatch(entries[i].Section, entries[i-1].Section))) i--;

                  STRING section = entries[i].Section;
                  for (; i < glVolumes->AmtEntries; i++) {
                     if (StrMatch(entries[i].Section, section)) break; // Check if section has ended
                     if (!StrMatch("Icon", entries[i].Key)) {
                        StrCopy("icons:", icon, sizeof(icon));
                        StrCopy(entries[i].Data, icon+sizeof("icons:")-1, sizeof(icon)-(sizeof("icons:")-1));
                        goto volume_found;
                     }
                  }
               }
            }
volume_found:
            ReleasePrivateObject((OBJECTPTR)glVolumes);
         }
      }

      *Value = Self->prvIcon = StrClone(icon);
      SetContext(context);
      return ERR_Okay;
   }

   struct FileInfo info;
   UBYTE fileinfo[MAX_FILENAME];
   BYTE link = FALSE;
   if (!get_file_info(Self->Path, &info, sizeof(info), fileinfo, sizeof(fileinfo))) {
      if (info.Flags & RDF_LINK) link = TRUE;

      if (info.Flags & RDF_VIRTUAL) {
         STRING *tags;

         // Virtual drives can specify custom icons, even for folders
         for (tags=info.Tags; *tags; tags++) {
            if (!StrCompare("ICON:", *tags, 0, 0)) {
               *Value = Self->prvIcon = StrClone(tags[0]+5);
               SetContext(context);
               return ERR_Okay;
            }
         }
      }

      if (info.Flags & RDF_FOLDER) {
         if (link) *Value = Self->prvIcon = StrClone("icons:folders/folder+overlays/link");
         else *Value = Self->prvIcon = StrClone("icons:folders/folder");
         SetContext(context);
         return ERR_Okay;
      }
   }

   while (Self->Path[i]) i++;
   if ((Self->Path[i-1] IS '/') OR (Self->Path[i-1] IS '\\')) {
      if (link) *Value = Self->prvIcon = StrClone("icons:folders/folder+overlays/link");
      else *Value = Self->prvIcon = StrClone("icons:folders/folder");
      SetContext(context);
      return ERR_Okay;
   }

   // Load the file association data files.  Information is merged between the global association file and the user's
   // personal association file.

   if (!glDatatypes) {
      if (load_datatypes() != ERR_Okay) {
         if (link) *Value = Self->prvIcon = StrClone("icons:filetypes/empty+overlays/link");
         else *Value = Self->prvIcon = StrClone("icons:filetypes/empty");
         SetContext(context);
         return ERR_Okay;
      }
   }

   struct ConfigEntry *entries;
   char icon[80] = "";
   if ((entries = glDatatypes->Entries)) {
      // Scan file extensions first, because this saves us from having to open and read the file content.

      LONG k;
      for (k=i; (k > 0) AND (Self->Path[k-1] != ':') AND (Self->Path[k-1] != '/') AND (Self->Path[k-1] != '\\'); k--);

      if (Self->Path[k]) {
         LONG j;
         for (j=0; j < glDatatypes->AmtEntries; j++) {
            if (StrMatch(entries[j].Key, "Match") != ERR_Okay) continue;

            if (!StrCompare(entries[j].Data, Self->Path+k, 0, STR_WILDCARD)) {
               CSTRING str;
               if (!cfgReadValue(glDatatypes, entries[j].Section, "Icon", &str)) StrCopy(str, icon, sizeof(icon));
               break;
            }
         }
      }

      // Use IdentifyFile() to see if this file can be associated with a class

      if (!icon[0]) {
         char classname[40], mastername[40];
         classname[0]  = 0;
         mastername[0] = 0;

         CLASSID class_id, subclass_id;
         if (!IdentifyFile(Self->Path, NULL, 0, &class_id, &subclass_id, NULL)) {
            if (!subclass_id) subclass_id = class_id;

            struct ClassHeader *classes;

            if ((classes = glClassDB)) {
               LONG *offsets = CL_OFFSETS(classes);
               for (i=0; i < classes->Total; i++) {
                  struct ClassItem *item = ((APTR)classes) + offsets[i];
                  if (item->ClassID IS subclass_id) {
                     StrCopy(item->Name, classname, sizeof(classname));
                  }
                  else if (item->ClassID IS class_id) {
                     StrCopy(item->Name, mastername, sizeof(mastername));
                  }
               }
            }
         }

         // Scan class names

         if ((classname[0]) OR (mastername[0])) {
            LONG j;
            for (j=0; j < glDatatypes->AmtEntries; j++) {
               if (!StrMatch(entries[j].Key, "Class")) {
                  CSTRING str;
                  if (!StrMatch(entries[j].Data, classname)) {
                     if (!cfgReadValue(glDatatypes, entries[j].Section, "Icon", &str)) StrCopy(str, icon, sizeof(icon));
                     break;
                  }
                  else if (!StrMatch(entries[j].Data, mastername)) {
                     if (!cfgReadValue(glDatatypes, entries[j].Section, "Icon", &str)) StrCopy(str, icon, sizeof(icon));
                     // Don't break - keep searching in case there is a sub-class reference
                  }
               }
            }
         }
      }
   }

   if (!icon[0]) {
      if (link) *Value = Self->prvIcon = StrClone("icons:filetypes/empty+overlays/link");
      else *Value = Self->prvIcon = StrClone("icons:filetypes/empty");
      SetContext(context);
      return ERR_Okay;
   }

   if (StrCompare("icons:", icon, 6, 0) != ERR_Okay) {
      CopyMemory(icon, icon+6, sizeof(icon) - 6);
      for (i=0; i < 6; i++) icon[i] = "icons:"[i];
   }

   if (link) {
      for (i=0; icon[i]; i++);
      StrCopy("+overlays/link", icon + i, sizeof(icon)-i);
   }

   *Value = Self->prvIcon = StrClone(icon);
   SetContext(context);
   return ERR_Okay;
}

/****************************************************************************
-FIELD-
Link: Returns the link path for symbolically linked files.

If a file represents a symbolic link (indicated by the SYMLINK flag setting) then reading the Link field will return
the link path.  No assurance is made as to the validity of the path.  If the path is not absolute, then the parent
folder containing the link will need to be taken into consideration when calculating the path that the link refers to.

****************************************************************************/

static ERROR GET_Link(objFile *Self, STRING *Value)
{
#ifdef __unix__
   STRING path;
   UBYTE buffer[512];

   if (Self->prvLink) { // The link has already been read previously, just re-use it
      *Value = Self->prvLink;
      return ERR_Okay;
   }

   *Value = NULL;
   if (Self->Flags & FL_LINK) {
      if (!ResolvePath(Self->Path, 0, &path)) {
         LONG i;
         for (i=0; path[i]; i++);
         if (path[i-1] IS '/') path[i-1] = 0;
         if (((i = readlink(path, buffer, sizeof(buffer)-1)) > 0) AND (i < sizeof(buffer)-1)) {
            buffer[i] = 0;
            Self->prvLink = StrClone(buffer);
            *Value = Self->prvLink;
         }
         FreeMemory(path);

         if (*Value) return ERR_Okay;
         else return ERR_Failed;
      }
      else return ERR_ResolvePath;
   }

   return ERR_Failed;
#else
   return ERR_NoSupport;
#endif
}

static ERROR SET_Link(objFile *Self, STRING Value)
{
#ifdef __unix__
   //symlink().
#endif
   return ERR_NoSupport;
}

/****************************************************************************
-FIELD-
Path: Specifies the location of a file or folder.

This field is required for initialisation and must either be in the format of a universal path string, or a path
that is compatible with the host system.  The standard format for a universal path is `volume:folder/file`, for
instance `parasol:system/classes.bin`.

To reference a folder in a way that is distinct from a file, use a trailing slash as in `volume:folder/`.

Referencing a `volume:` is optional.  In the event that a volume is not defined, the current working path is used
as the point of origin.

The accepted method for referencing parent folders is `../`, which can be repeated for as many parent folders as needed
to traverse the folder hierarchy.

****************************************************************************/

static ERROR GET_Path(objFile *Self, STRING *Value)
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

static ERROR SET_Path(objFile *Self, CSTRING Value)
{
   if (Self->Head.Flags & NF_INITIALISED) return PostError(ERR_Immutable);

   if (Self->Stream) {
      #ifdef __unix__
         closedir(Self->Stream);
      #endif
      Self->Stream = NULL;
   }
   else if (Self->Handle != -1) {
      close(Self->Handle);
      Self->Handle = -1;
   }

   if (Self->Path) { FreeMemory(Self->Path); Self->Path = NULL; }
   if (Self->prvResolvedPath) { FreeMemory(Self->prvResolvedPath); Self->prvResolvedPath = NULL; }

   LONG i, j, len;
   if ((Value) AND (*Value)) {
      if (StrCompare("string:", Value, 7, 0) != ERR_Okay) {
         for (len=0; (Value[len]) AND (Value[len] != '|'); len++) {
            if (Value[len] IS ';') {
               LogErrorMsg("Warning - use of ; is obsolete as a separator, use | in path %s", Value);
            }
         }
      }
      else for (len=0; Value[len]; len++);

      // Note: An extra byte is allocated in case the FL_FOLDER flag is set
      if (!AllocMemory(len+2, MEM_STRING|MEM_NO_CLEAR|Self->Head.MemFlags, (APTR *)&Self->Path, NULL)) {
         // If the path is set to ':' then this is the equivalent of asking for a folder list of all volumes in
         // the system.  No further initialisation is necessary in such a case.

         if ((Value[0] IS ':') AND (!Value[1])) {
            Self->Path[0] = ':';
            Self->Path[1] = 0;
            Self->prvType |= STAT_FOLDER;
            return ERR_Okay;
         }

         // Copy the path across and skip any trailing colons at the start.  We also eliminate any double slashes,
         // e.g. "drive1:documents//tutorials/"

         for (j=0; Value[j] IS ':'; j++);
         if (!StrCompare("string:", Value, 7, 0)) {
            i = StrCopy(Value, Self->Path, COPY_ALL);
         }
         else {
            i = 0;
            while ((Value[j]) AND (Value[j] != '|')) {
               if ((Value[j] IS '\\') AND (Value[j+1] IS '\\')) {
                  #ifdef _WIN32
                     // Double slash is okay for UNC paths
                     if (!j) Self->Path[i++] = Value[j++];
                     else j++;
                  #else
                     j++;
                  #endif
               }
               else if ((Value[j] IS '/') AND (Value[j+1] IS '/')) {
                  #ifdef _WIN32
                     // Double slash is okay for UNC paths
                     if (!j) Self->Path[i++] = Value[j++];
                     else j++;
                  #else
                     j++;
                  #endif
               }
               else Self->Path[i++] = Value[j++];
            }
            Self->Path[i] = 0;
         }

         // Check if the path is a folder/volume or a file

         for (i=0; Self->Path[i]; i++);

         if ((Self->Path[i-1] IS ':') OR (Self->Path[i-1] IS '/') OR (Self->Path[i-1] IS '\\')) {
            Self->prvType |= STAT_FOLDER;
         }
         else if (Self->Flags & FL_FOLDER) {
            Self->Path[i++] = '/';
            Self->Path[i] = 0;
            Self->prvType |= STAT_FOLDER;
         }
      }
      else return PostError(ERR_AllocMemory);
   }

   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Permissions: Manages the permissions of a file.
Lookup: PERMIT
-END-
*****************************************************************************/

static ERROR GET_Permissions(objFile *Self, LONG *Value)
{
   *Value = 0;

#ifdef __unix__

   // Always read permissions straight off the disk rather than returning an internal field, because some other
   // process could always have changed the permission flags.

   CSTRING path;
   if (!GET_ResolvedPath(Self, &path)) {
      LONG i = StrLength(path);
      while ((i >= 0) AND (path[i] != '/') AND (path[i] != ':') AND (path[i] != '\\')) i--;
      if (path[i+1] IS '.') Self->Permissions = PERMIT_HIDDEN;
      else Self->Permissions = 0;

      if (Self->Handle != -1) {
         struct stat64 info;
         if (fstat64(Self->Handle, &info) != -1) {
            Self->Permissions |= convert_fs_permissions(info.st_mode);
         }
         else return convert_errno(errno, ERR_SystemCall);
      }
      else if (Self->Stream) {
         struct stat64 info;
         if (stat64(path, &info) != -1) {
            Self->Permissions |= convert_fs_permissions(info.st_mode);
         }
         else return convert_errno(errno, ERR_SystemCall);
      }

      *Value = Self->Permissions;
      return ERR_Okay;
   }
   else return ERR_ResolvePath;

#elif _WIN32

   CSTRING path;
   if (!GET_ResolvedPath(Self, &path)) {
      winGetAttrib(path, Value); // Supports PERMIT_HIDDEN/ARCHIVE/OFFLINE/READ/WRITE
      return ERR_Okay;
   }
   else return ERR_ResolvePath;

#endif

   return ERR_NoSupport;
}

static ERROR SET_Permissions(objFile *Self, LONG Value)
{
   if (!(Self->Head.Flags & NF_INITIALISED)) {
      Self->Permissions = Value;
      return ERR_Okay;
   }
   else return set_permissions(Self, Value);
}

//****************************************************************************

static ERROR set_permissions(objFile *Self, LONG Permissions)
{
#ifdef __unix__

   if (Self->Handle != -1) {
      LONG flags = 0;
      if (Permissions & PERMIT_READ)  flags |= S_IRUSR;
      if (Permissions & PERMIT_WRITE) flags |= S_IWUSR;
      if (Permissions & PERMIT_EXEC)  flags |= S_IXUSR;

      if (Permissions & PERMIT_GROUP_READ)  flags |= S_IRGRP;
      if (Permissions & PERMIT_GROUP_WRITE) flags |= S_IWGRP;
      if (Permissions & PERMIT_GROUP_EXEC)  flags |= S_IXGRP;

      if (Permissions & PERMIT_OTHERS_READ)  flags |= S_IROTH;
      if (Permissions & PERMIT_OTHERS_WRITE) flags |= S_IWOTH;
      if (Permissions & PERMIT_OTHERS_EXEC)  flags |= S_IXOTH;

      LONG err = fchmod(Self->Handle, flags);

      // Note that you need to be root to set the UID/GID flags, so we do it in this subsequent fchmod() call.

      if ((err != -1) AND (Permissions & (PERMIT_USERID|PERMIT_GROUPID))) {
         if (Permissions & PERMIT_USERID)  flags |= S_ISUID;
         if (Permissions & PERMIT_GROUPID) flags |= S_ISGID;
         err = fchmod(Self->Handle, flags);
      }

      if (err != -1) {
         Self->Permissions = Permissions;
         return ERR_Okay;
      }
      else return convert_errno(errno, ERR_SystemCall);
   }
   else if (Self->Stream) {
      // File represents a folder

      CSTRING path;
      if (!GET_ResolvedPath(Self, &path)) {
         LONG flags = 0;
         if (Permissions & PERMIT_READ)  flags |= S_IRUSR;
         if (Permissions & PERMIT_WRITE) flags |= S_IWUSR;
         if (Permissions & PERMIT_EXEC)  flags |= S_IXUSR;

         if (Permissions & PERMIT_GROUP_READ)  flags |= S_IRGRP;
         if (Permissions & PERMIT_GROUP_WRITE) flags |= S_IWGRP;
         if (Permissions & PERMIT_GROUP_EXEC)  flags |= S_IXGRP;

         if (Permissions & PERMIT_OTHERS_READ)  flags |= S_IROTH;
         if (Permissions & PERMIT_OTHERS_WRITE) flags |= S_IWOTH;
         if (Permissions & PERMIT_OTHERS_EXEC)  flags |= S_IXOTH;

         if (Permissions & PERMIT_GROUPID) flags |= S_ISGID;
         if (Permissions & PERMIT_USERID)  flags |= S_ISUID;

         if (chmod(path, flags) != -1) {
            Self->Permissions = Permissions;
            return ERR_Okay;
         }
         else return PostError(convert_errno(errno, ERR_SystemCall));
      }
      else return PostError(ERR_ResolvePath);
   }
   else return PostError(ERR_InvalidHandle);

#elif _WIN32

   LogF("~set_permissions()","$%.8x", Permissions);

   CSTRING path;
   if (!GET_ResolvedPath(Self, &path)) {
      ERROR error;
      if (winSetAttrib(path, Permissions)) {
         error = PostError(ERR_Failed);
      }
      else error = ERR_Okay;

      LogBack();
      return error;
   }
   else return StepError(0, ERR_ResolvePath);

#else

   return ERR_NoSupport;

#endif
}

/*****************************************************************************
-FIELD-
Position: The current read/write byte position in a file.

This field indicates the current byte position of an open file (this affects read and write operations).  Writing to
this field performs a #Seek() operation.

The Position will always remain at zero if the file object represents a folder.

*****************************************************************************/

static ERROR SET_Position(objFile *Self, LARGE Value)
{
   if (Self->Head.Flags & NF_INITIALISED) {
      return acSeekStart(Self, Value);
   }
   else {
      Self->Position = Value;
      return ERR_Okay;
   }
}

/*****************************************************************************
-FIELD-
ResolvedPath: Returns a resolved copy of the Path string.

The ResolvedPath will return a resolved copy of the #Path string.  The resolved path will be in a format that is native
to the host platform.  Please refer to the ~ResolvePath() function for further information.

*****************************************************************************/

static ERROR GET_ResolvedPath(objFile *Self, CSTRING *Value)
{
   if (!Self->Path) return ERR_FieldNotSet;

   if (!Self->prvResolvedPath) {
      LONG flags = 0;
      if (Self->Flags & FL_APPROXIMATE) flags |= RSF_APPROXIMATE;
      else flags |= RSF_NO_FILE_CHECK;

      ERROR error;
      if ((error = ResolvePath(Self->Path, flags, &Self->prvResolvedPath)) != ERR_Okay) {
         return ERR_ResolvePath;
      }
   }

   *Value = Self->prvResolvedPath;
   return ERR_Okay;
}

/*****************************************************************************
-FIELD-
Size: The byte size of a file.

The current byte size of a file is indicated by this field.  If the file object represents a folder, the Size value
will be set to zero.  You can also truncate a file by setting the Size; this will result in the current read/write
position being set to the end of the file.

*****************************************************************************/

static ERROR GET_Size(objFile *Self, LARGE *Size)
{
   if (Self->Flags & FL_FOLDER) {
      *Size = 0;
      return ERR_Okay;
   }

   if (Self->Handle != -1) {
      struct stat64 stats;
      if (!fstat64(Self->Handle, &stats)) {
         *Size = stats.st_size;
         return ERR_Okay;
      }
      else return convert_errno(errno, ERR_SystemCall);
   }

   CSTRING path;
   ERROR error;
   if (!(error = GET_ResolvedPath(Self, &path))) {
      struct stat64 stats;
      if (!stat64(path, &stats)) {
         *Size = stats.st_size;
         MSG("The file size is " PF64(), *Size);
         return ERR_Okay;
      }
      else return convert_errno(errno, ERR_SystemCall);
   }
   else return PostError(ERR_ResolvePath);
}

static ERROR SET_Size(objFile *Self, LARGE Size)
{
   if (Size IS Self->Size) return ERR_Okay;
   if (Size < 0) return PostError(ERR_OutOfRange);

   if (Self->Buffer) {
      if (Self->Head.Flags & NF_INITIALISED) return ERR_NoSupport;
      else Self->Size = Size;

      if (Self->Position > Self->Size) acSeekStart(Self, Size);
      return ERR_Okay;
   }

   if (!(Self->Head.Flags & NF_INITIALISED)) {
      Self->Size = Size;
      if (Self->Position > Self->Size) acSeekStart(Self, Size);
      return ERR_Okay;
   }

#ifdef _WIN32
   CSTRING path;

   if (!GET_ResolvedPath(Self, &path)) {
      if (winSetEOF(path, Size)) {
         acSeek(Self, 0.0, SEEK_END);
         Self->Size = Size;
         if (Self->Position > Self->Size) acSeekStart(Self, Size);
         return ERR_Okay;
      }
      else {
         LogErrorMsg("Failed to set file size to " PF64(), Size);
         return ERR_SystemCall;
      }
   }
   else return PostError(ERR_ResolvePath);

#elif __unix__

   #ifdef __ANDROID__
   #warning Support for ftruncate64() required for Android build.
   if (!ftruncate(Self->Handle, Size)) {
   #else
   if (!ftruncate64(Self->Handle, Size)) {
   #endif
      Self->Size = Size;
      if (Self->Position > Self->Size) acSeekStart(Self, Size);
      return ERR_Okay;
   }
   else {
      // Some filesystem drivers do not support truncation for the purpose of
      // enlarging files.  In this case, we have to write to the end of the file.

      LogErrorMsg("" PF64() " bytes, ftruncate: %s", Size, strerror(errno));

      if (Size > Self->Size) {
         CSTRING path;

         // Seek past the file boundary and write a single byte to expand the file.  Yes, it's legal and works.

         ERROR error;
         if (!(error = GET_ResolvedPath(Self, &path))) {
            struct statfs fstat;
            if (statfs(path, &fstat) != -1) {
               if (Size < (LARGE)fstat.f_bavail * (LARGE)fstat.f_bsize) {
                  LogMsg("Attempting to use the write-past-boundary method.");

                  if (lseek64(Self->Handle, Size - 1, SEEK_SET) != -1) {
                     BYTE c = 0;
                     if (write(Self->Handle, &c, 1) IS 1) {
                        lseek64(Self->Handle, Self->Position, SEEK_SET);
                        Self->Size = Size;
                        if (Self->Position > Self->Size) acSeekStart(Self, Size);
                        return ERR_Okay;
                     }
                     else return convert_errno(errno, ERR_SystemCall);
                  }
                  else return convert_errno(errno, ERR_SystemCall);
               }
               else return PostError(ERR_OutOfSpace);
            }
            else return convert_errno(errno, ERR_SystemCall);
         }
         else return ERR_ResolvePath;
      }
      else return ERR_Failed;
   }
#else
   MSG("No support for truncating file sizes on this platform.");
   return PostError(ERR_NoSupport);
#endif
}

/*****************************************************************************

-FIELD-
Static: Set to TRUE if a file object should be static.

This field applies when a file object has been created in an object script.  By default, a file object will
auto-terminate when a closing tag is received.  If the object must remain live, set this field to TRUE.

-FIELD-
Target: Specifies a surface ID to target for user feedback and dialog boxes.

User feedback can be enabled for certain file operations by setting the Target field to a valid surface ID (for
example, the Desktop object) or zero for the default target for new windows.  This field is set to -1 by default, in
order to disable this feature.

If set correctly, operations such as file deletion or copying will pop-up a progress box after a certain amount of time
has elapsed during the operation.  The dialog box will also provide the user with a Cancel option to terminate the
process early.

-FIELD-
TimeStamp: The last modification time set on a file, represented as a 64-bit integer.

The TimeStamp field is a 64-bit representation of the last modification date/time set on a file.  It is not guaranteed
that the value represents seconds from the epoch, so it should only be used for purposes such as sorting, or
for comparison to the time stamps of other files.  For a parsed time structure, refer to the #Date field.

*****************************************************************************/

static ERROR GET_TimeStamp(objFile *Self, LARGE *Value)
{
   *Value = 0;

   if (Self->Handle != -1) {
      struct stat64 stats;
      if (!fstat64(Self->Handle, &stats)) {
         // Timestamp has to match that produced by fs_getinfo()

         struct tm *local;
         #ifdef _WIN32
         if ((local = _localtime64(&stats.st_mtime))) {
         #else
         if ((local = localtime(&stats.st_mtime))) {
         #endif
            struct DateTime datetime;
            datetime.Year   = 1900 + local->tm_year;
            datetime.Month  = local->tm_mon + 1;
            datetime.Day    = local->tm_mday;
            datetime.Hour   = local->tm_hour;
            datetime.Minute = local->tm_min;
            datetime.Second = local->tm_sec;

            *Value = calc_timestamp(&datetime);
            return ERR_Okay;
         }
         else return convert_errno(errno, ERR_SystemCall);
      }
      else return convert_errno(errno, ERR_SystemCall);
   }
   else {
      CSTRING path;
      ERROR error;
      if (!(error = GET_ResolvedPath(Self, &path))) {
         struct stat64 stats;
         if (!stat64(path, &stats)) {

            struct tm *local;
            #ifdef _WIN32
            if ((local = _localtime64(&stats.st_mtime))) {
            #else
            if ((local = localtime(&stats.st_mtime))) {
            #endif
               struct DateTime datetime;
               datetime.Year   = 1900 + local->tm_year;
               datetime.Month  = local->tm_mon + 1;
               datetime.Day    = local->tm_mday;
               datetime.Hour   = local->tm_hour;
               datetime.Minute = local->tm_min;
               datetime.Second = local->tm_sec;

               *Value = calc_timestamp(&datetime);
               return ERR_Okay;
            }
            else return convert_errno(errno, ERR_SystemCall);

         }
         else return convert_errno(errno, ERR_SystemCall);
      }
      else return PostError(ERR_ResolvePath);
   }
}

/*****************************************************************************
-FIELD-
User: Retrieve or change the user ID of a file.

The user ID assigned to a file can be read from this field.  The ID is retrieved from the file system in real time in
case the ID has been changed after initialisation of the file object.

You can also change the user ID of a file by writing an integer value to this field.  This can only be done
post-initialisation or an error code will be returned.

If the filesystem does not support user ID's, ERR_NoSupport is returned.
-END-
*****************************************************************************/

static ERROR GET_User(objFile *Self, LONG *Value)
{
#ifdef __unix__
   struct stat64 info;

   if (fstat64(Self->Handle, &info) IS -1) {
      return ERR_FileNotFound;
   }

   *Value = info.st_uid;
   return ERR_Okay;
#else
   return ERR_NoSupport;
#endif
}

static ERROR SET_User(objFile *Self, LONG Value)
{
#ifdef __unix__
   if (Self->Head.Flags & NF_INITIALISED) {
      LogMsg("Changing user to #%d", Value);
      if (!fchown(Self->Handle, Value, -1)) {
         return ERR_Okay;
      }
      else return PostError(convert_errno(errno, ERR_Failed));
   }
   else return PostError(ERR_Failed);
#else
   return ERR_NoSupport;
#endif
}

//****************************************************************************

static const struct FieldDef PermissionFlags[] = {
   { "Read",         PERMIT_READ },
   { "Write",        PERMIT_WRITE },
   { "Exec",         PERMIT_EXEC },
   { "Executable",   PERMIT_EXEC },
   { "Delete",       PERMIT_DELETE },
   { "Hidden",       PERMIT_HIDDEN },
   { "Archive",      PERMIT_ARCHIVE },
   { "Password",     PERMIT_PASSWORD },
   { "UserID",       PERMIT_USERID },
   { "GroupID",      PERMIT_GROUPID },
   { "OthersRead",   PERMIT_OTHERS_READ },
   { "OthersWrite",  PERMIT_OTHERS_WRITE },
   { "OthersExec",   PERMIT_OTHERS_EXEC },
   { "OthersDelete", PERMIT_OTHERS_DELETE },
   { "GroupRead",    PERMIT_GROUP_READ },
   { "GroupWrite",   PERMIT_GROUP_WRITE },
   { "GroupExec",    PERMIT_GROUP_EXEC },
   { "GroupDelete",  PERMIT_GROUP_DELETE },
   { "AllRead",      PERMIT_ALL_READ },
   { "AllWrite",     PERMIT_ALL_WRITE },
   { "AllExec",      PERMIT_ALL_EXEC },
   { "UserRead",     PERMIT_READ },
   { "UserWrite",    PERMIT_WRITE },
   { "UserExec",     PERMIT_EXEC },
   { NULL, 0 }
};

#include "class_file_def.c"

static const struct FieldArray FileFields[] = {
   { "Position", FDF_LARGE|FDF_RW,     0, NULL, SET_Position },
   { "Flags",    FDF_LONGFLAGS|FDF_RI, (MAXINT)&clFileFlags, NULL, NULL },
   { "Static",   FDF_LONG|FDF_RI,      0, NULL, NULL },
   { "Target",   FDF_OBJECTID|FDF_RW,  ID_SURFACE, NULL, NULL },
   { "Buffer",   FDF_ARRAY|FDF_BYTE|FDF_R,  0, GET_Buffer, NULL },
   // Virtual fields
   { "Date",        FDF_POINTER|FDF_STRUCT|FDF_RW, (MAXINT)"DateTime", GET_Date, SET_Date },
   { "Created",     FDF_POINTER|FDF_STRUCT|FDF_RW, (MAXINT)"DateTime", GET_Created, NULL },
   { "Handle",      FDF_LARGE|FDF_R,      0, GET_Handle, NULL },
   { "Icon",        FDF_STRING|FDF_R, 0, GET_Icon, NULL },
   { "Path",        FDF_STRING|FDF_RI,    0, GET_Path, SET_Path },
   { "Permissions", FDF_LONGFLAGS|FDF_RW, (MAXINT)&PermissionFlags, GET_Permissions, SET_Permissions },
   { "ResolvedPath", FDF_STRING|FDF_R,    0, GET_ResolvedPath, NULL },
   { "Size",        FDF_LARGE|FDF_RW,     0, GET_Size, SET_Size },
   { "TimeStamp",   FDF_LARGE|FDF_R,      0, GET_TimeStamp, NULL },
   { "Link",        FDF_STRING|FDF_RW,    0, GET_Link, SET_Link },
   { "User",        FDF_LONG|FDF_RW,      0, GET_User, SET_User },
   { "Group",       FDF_LONG|FDF_RW,      0, GET_Group, SET_Group },
   // Synonyms
   { "Src",         FDF_STRING|FDF_SYNONYM|FDF_RI,    0, GET_Path, SET_Path },
   { "Location",    FDF_STRING|FDF_SYNONYM|FDF_RI, 0, GET_Path, SET_Path },
   END_FIELD
};
