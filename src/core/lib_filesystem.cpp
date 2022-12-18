/*****************************************************************************

The source code of the Parasol Framework is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

-CATEGORY-
Name: Files
-END-

*****************************************************************************/

//#define DEBUG
#define PRV_FILESYSTEM
#define PRV_FILE
#define _LARGE_FILES

#ifdef _WIN32
typedef int HANDLE;
#endif

#if defined(__unix__) && !defined(_WIN32)
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

 #ifndef __linux__
  #include <sys/statvfs.h>
 #else
  #include <sys/vfs.h>
  #define statvfs statfs
  #define fstatvfs fstatfs
 #endif

 #include <sys/time.h>
 #include <sys/syscall.h>

 #ifdef __linux__
  #include <sys/inotify.h>
  //#include "inotify-syscalls.h"
  //#include "inotify.h"
 #endif
#endif

#include "defs.h"
#include <parasol/main.h>

#include <stdarg.h>
#include <errno.h>
#include <map>
#include <mutex>

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
 #undef NULL
 #define NULL 0
#endif // _WIN32

#ifdef __APPLE__
 #include <sys/param.h>
 #include <sys/mount.h>
 #define lseek64  lseek
 #define ftruncate64 ftruncate
#endif

//****************************************************************************

class CacheFileIndex {
public:
   std::string path;
   LARGE timestamp;
   LARGE size;

   CacheFileIndex(std::string Path, LARGE Timestamp, LARGE Size) {
      path      = Path;
      timestamp = Timestamp;
      size      = Size;
   }

   bool operator==(const CacheFileIndex &other) const {
      return (path == other.path && timestamp == other.timestamp && size == other.size);
   }
};

namespace std {
   template <>
   struct hash<CacheFileIndex>
   {
      std::size_t operator()(const CacheFileIndex& k) const {
         return ((std::hash<std::string>()(k.path)
            ^ (std::hash<LARGE>()(k.timestamp) << 1)) >> 1)
            ^ (std::hash<LARGE>()(k.size) << 1);
      }
   };
}

static std::unordered_map<CacheFileIndex, CacheFile *> glCache;
static std::mutex glCacheLock;

//****************************************************************************
// Called during shutdown

void free_file_cache(void)
{
   parasol::Log log(__FUNCTION__);

   log.branch();

   const std::lock_guard<std::mutex> lock(glCacheLock);

   for (auto it=glCache.begin(); it != glCache.end(); it = glCache.erase(it)) {
      FreeResource(it->second);
   }
}

//****************************************************************************
// Check if a Path refers to a virtual volume, and if so, return the matching virtual_drive definition.

static virtual_drive * get_virtual(CSTRING Path)
{
   LONG len;
   for (len=0; (Path[len]) and (Path[len] != ':'); len++); // The Path may end with a NULL terminator or a colon
   if ((size_t)len < sizeof(glVirtual[0].Name)) {
      for (LONG i=0; i < glVirtualTotal; i++) {
         if (glVirtual[i].Name[len] IS ':') {
            if (!StrCompare(glVirtual[i].Name, Path, len, 0)) return glVirtual+i;
         }
      }
   }
   return NULL;
}

//****************************************************************************

extern "C" LONG CALL_FEEDBACK(FUNCTION *Callback, FileFeedback *Feedback)
{
   if ((!Callback) or (!Feedback)) return FFR_OKAY;

   if (Callback->Type IS CALL_STDC) {
      auto routine = (LONG (*)(FileFeedback *))Callback->StdC.Routine;
      return routine(Feedback);
   }
   else if (Callback->Type IS CALL_SCRIPT) {
      OBJECTPTR script;
      if ((script = Callback->Script.Script)) {
         const ScriptArg args[] = {
            { "Size",     FD_LARGE,   { .Large   = Feedback->Size } },
            { "Position", FD_LARGE,   { .Large   = Feedback->Position } },
            { "Path",     FD_STRING,  { .Address = Feedback->Path } },
            { "Dest",     FD_STRING,  { .Address = Feedback->Dest } },
            { "FeedbackID", FD_LONG,  { .Long    = Feedback->FeedbackID } }
         };

         ERROR error;
         if (scCallback(script, Callback->Script.ProcedureID, args, ARRAYSIZE(args), &error)) error = ERR_Failed;

         if (!error) {
            CSTRING *results;
            LONG size;
            if ((!GetFieldArray(script, FID_Results, (APTR *)&results, &size)) and (size > 0)) {
               return StrToInt(results[0]);
            }
            else return FFR_OKAY;
         }
         else return FFR_OKAY;
      }
      else return FFR_OKAY;
   }
   else return FFR_OKAY;
}

/*****************************************************************************
** Cleans up path strings such as "../../myfile.txt".  Note that for Linux, the targeted file/folder has to exist or
** NULL will be returned.
**
** The Path must be resolved to the native OS format.
*/

static STRING cleaned_path(CSTRING Path)
{
#ifdef _WIN32
   char buffer[512];
   if (winGetFullPathName(Path, sizeof(buffer), buffer, NULL) > 0) {
      STRING p = StrClone(buffer);
      /*LONG i;
      for (i=0; p[i]; i++) {
         if (p[i] IS '/') p[i] = '\\';
      }*/
      return p;
   }
   else return NULL;
#else
   char *rp = realpath(Path, NULL);
   if (rp) {
      STRING p = StrClone(rp);
      free(rp);
      return p;
   }
   else return NULL;
#endif
}

/*****************************************************************************
** Returns a virtual_drive structure for all path types. Defaults to the host file system if no virtual drive was
** identified.
**
** The Path must be resolved before you call this function, this is necessary to solve cases where a volume is a
** shortcut to multiple paths for example.
*/

const virtual_drive * get_fs(CSTRING Path)
{
   if (Path[0] IS ':') return (const virtual_drive *)(&glVirtual[0]);

   LONG len;
   ULONG hash = 5381;
   for (len=0; (Path[len]) and (Path[len] != ':'); len++) {
      char c = Path[len];
      if ((c IS '/') or (c IS '\\')) return (const virtual_drive *)&glVirtual[0]; // If a slash is encountered early, the path belongs to the local FS
      if ((c >= 'A') and (c <= 'Z')) hash = (hash<<5) + hash + c - 'A' + 'a';
      else hash = (hash<<5) + hash + c;
   }

   // Determine ownership based on the volume name

   if ((size_t)len < sizeof(glVirtual[0].Name)) {
      for (LONG i=0; i < glVirtualTotal; i++) {
         if ((hash IS glVirtual[i].VirtualID) and (glVirtual[i].Name[len] IS ':')) {
            if (!StrCompare(glVirtual[i].Name, Path, len, 0)) return glVirtual+i;
         }
      }
   }

   return (const virtual_drive *)&glVirtual[0];
}

//****************************************************************************
// Assigned to a timer for the purpose of checking up on the expiry of cached files.

ERROR check_cache(OBJECTPTR Subscriber, LARGE Elapsed, LARGE CurrentTime)
{
   parasol::Log log(__FUNCTION__);

   log.branch("Scanning file cache for unused entries...");

   const std::lock_guard<std::mutex> lock(glCacheLock);
   for (auto it=glCache.begin(); it != glCache.end(); ) {
      if ((CurrentTime - it->second->LastUse >= 60LL * 1000000LL) and (it->second->Locks <= 0)) {
         log.msg("Removing expired cache file: %.80s", it->second->Path);
         FreeResource(it->second);
         it = glCache.erase(it);
      }
      else it++;
   }

   if (glCache.empty()) {
      glCacheTimer = 0;
      return ERR_Terminate;
   }
   else return ERR_Okay;
}

/*****************************************************************************

-FUNCTION-
AddInfoTag: Adds new tags to FileInfo structures.

This function adds file tags to FileInfo structures.  It is intended for use by the FileSystem module and related
drivers only.  Tags allow extended attributes to be associated with a file, for example the number of seconds of audio
in an MP3 file.

-INPUT-
struct(FileInfo) Info: Pointer to a valid FileInfo structure.
cstr Name: The name of the tag.
cstr Value: The value to associate with the tag name.  If NULL, any existing tag with a matching Name will be removed.

-ERRORS-
Okay:
NullArgs:
CreateResource: Failed to create a new keystore.

*****************************************************************************/

ERROR AddInfoTag(FileInfo *Info, CSTRING Name, CSTRING Value)
{
   if (!Info->Tags) {
      if (!(Info->Tags = VarNew(0, 0))) return ERR_CreateResource;
   }

   return VarSetString(Info->Tags, Name, Value);
}

/*****************************************************************************

-FUNCTION-
AnalysePath: Analyses paths to determine their type (file, folder or volume).

This function will analyse a path and determine the type of file that the path is referring to.  For instance, a path
of `user:documents/` would indicate a folder reference.  A path of `system:` would be recognised as a volume. A path
of `user:documents/copyright.txt` would be recognised as a file.

Ambiguous references are analysed to get the correct type - for example `user:documents/helloworld` could refer to a
folder or file, so the path is analysed to check the file type.  On exceptional occasions where the path could be
interpreted as either a folder or a file, preference is given to the folder.

File path approximation is supported if the Path is prefixed with a `~` character (e.g. `~pictures:photo` could be
matched to `photo.jpg` in the same folder).

To check if a volume name is valid, call ~ResolvePath() first and then pass the resulting path to this
function.

If the queried path does not exist, a fail code is returned.  This behaviour makes the AnalysePath() function a good
candidate for testing the validity of a path string.

-INPUT-
cstr Path: The path to analyse.
&int(LOC) Type: The result will be stored in the LONG variable referred to by this argument.  The return types are LOC_DIRECTORY, LOC_FILE and LOC_VOLUME.  You can set this argument to NULL if you are only interested in checking if the file exists.

-ERRORS-
Okay: The path was analysed and the result is stored in the Type variable.
NullArgs:
DoesNotExist:

*****************************************************************************/

ERROR AnalysePath(CSTRING Path, LONG *PathType)
{
   parasol::Log log(__FUNCTION__);

   if (PathType) *PathType = 0;
   if (!Path) return ERR_NullArgs;

   // Special volumes 'string:' and 'memory:' are considered to be file paths.

   if (!StrCompare("string:", Path, 7, 0)) {
      if (PathType) *PathType = LOC_FILE;
      return ERR_Okay;
   }

   log.traceBranch("%s", Path);

   LONG flags = 0;
   if (Path[0] IS '~') {
      flags |= RSF_APPROXIMATE;
      Path++;
   }

   LONG len = StrLength(Path);
   if (Path[len-1] IS ':') {
      parasol::ScopedObjectLock<objConfig> volumes(glVolumes, 8000);
      if (volumes.granted()) {
         ConfigGroups *groups;
         if (!GetPointer(glVolumes, FID_Data, &groups)) {
            for (auto& [group, keys] : groups[0]) {
               if ((!StrCompare(Path, keys["Name"].c_str(), len-1, 0)) and
                   (keys["Name"].size() IS (size_t)len-1)) {
                  if (PathType) *PathType = LOC_VOLUME;
                  return ERR_Okay;
               }
            }
         }
      }
      return ERR_DoesNotExist;
   }

   STRING test_path;
   if (!ResolvePath(Path, flags, &test_path)) {
      log.trace("Testing path type for '%s'", test_path);

      ERROR error;
      const virtual_drive *vd = get_fs(test_path);
      if (vd->TestPath) {
         if (!PathType) PathType = &len; // Dummy variable, helps to avoid bugs
         error = vd->TestPath(test_path, 0, PathType);
      }
      else error = ERR_NoSupport;

      FreeResource(test_path);
      return error;
   }
   else {
      log.trace("Path '%s' does not exist.", Path);
      return ERR_DoesNotExist;
   }
}

/*****************************************************************************

-FUNCTION-
CompareFilePaths: Checks if two file paths refer to the same physical file.

This function will test two file paths, checking if they refer to the same file in a storage device.  It uses a string
comparison on the resolved path names, then attempts a second test based on an in-depth analysis of file attributes if
the string comparison fails.  In the event of a match, ERR_Okay is returned.  All other error codes indicate a
mis-match or internal failure.

The targeted paths do not have to refer to an existing file or folder in order to match (i.e. match on string
comparison succeeds).

-INPUT-
cstr PathA: File location 1.
cstr PathB: File location 2.

-ERRORS-
Okay: The file paths refer to the same file.
False: The file paths refer to different files.
NullArgs
-END-

*****************************************************************************/

ERROR CompareFilePaths(CSTRING PathA, CSTRING PathB)
{
   if ((!PathA) or (!PathB)) return ERR_NullArgs;

   STRING path1, path2;
   ERROR error;
   if ((error = ResolvePath(PathA, RSF_NO_FILE_CHECK, &path1))) {
      return error;
   }

   if ((error = ResolvePath(PathB, RSF_NO_FILE_CHECK, &path2))) {
      FreeResource(path1);
      return error;
   }

   const virtual_drive *v1, *v2;
   v1 = get_fs(path1);
   v2 = get_fs(path2);

   if ((!v1->CaseSensitive) and (!v2->CaseSensitive)) {
      error = StrCompare(path1, path2, 0, STR_MATCH_LEN);
   }
   else error = StrCompare(path1, path2, 0, STR_MATCH_LEN|STR_MATCH_CASE);

   if (error) {
      if (v1 IS v2) {
         // Ask the virtual FS if the paths match

         if (v1->SameFile) {
            error = v1->SameFile(path1, path2);
         }
         else error = ERR_False; // Assume the earlier string comparison is good enough
      }
      else error = ERR_False;
   }

   FreeResource(path1);
   FreeResource(path2);
   return error;
}

//****************************************************************************

ERROR fs_samefile(CSTRING Path1, CSTRING Path2)
{
#ifdef __unix__
   struct stat64 stat1, stat2;

   if ((!stat64(Path1, &stat1)) and (!stat64(Path2, &stat2))) {
      if ((stat1.st_ino IS stat2.st_ino)
            and (stat1.st_dev IS stat2.st_dev)
            and (stat1.st_mode IS stat2.st_mode)
            and (stat1.st_uid IS stat2.st_uid)
            and (stat1.st_gid IS stat2.st_gid)) {
         return ERR_True;
      }
      else return ERR_False;
   }
   else return ERR_False;
#else
   return ERR_NoSupport;
#endif
}

/*****************************************************************************

-FUNCTION-
ResolveGroupID: Converts a group ID to its corresponding name.

This function converts group ID's obtained from the file system into their corresponding names.  If the group ID is
invalid then NULL will be returned.

-INPUT-
int Group: The group ID.

-RESULT-
cstr: The group name is returned, or NULL if the ID cannot be resolved.

*****************************************************************************/

CSTRING ResolveGroupID(LONG GroupID)
{
#ifdef __unix__

   static THREADVAR char group[40];
   struct group *info;
   LONG i;

   if ((info = getgrgid(GroupID))) {
      for (i=0; (info->gr_name[i]) and ((size_t)i < sizeof(group)-1); i++) group[i] = info->gr_name[i];
      group[i] = 0;
      return group;
   }
   else return NULL;

#else

   return NULL;

#endif
}

/*****************************************************************************

-FUNCTION-
ResolveUserID: Converts a user ID to its corresponding name.

This function converts user ID's obtained from the file system into their corresponding names.  If the user ID is
invalid then NULL will be returned.

-INPUT-
int User: The user ID.

-RESULT-
cstr: The user name is returned, or NULL if the ID cannot be resolved.

*****************************************************************************/

CSTRING ResolveUserID(LONG UserID)
{
#ifdef __unix__

   static THREADVAR char user[40];
   struct passwd *info;
   LONG i;

   if ((info = getpwuid(UserID))) {
      for (i=0; (info->pw_name[i]) and ((size_t)i < sizeof(user)-1); i++) user[i] = info->pw_name[i];
      user[i] = 0;
      return user;
   }
   else return NULL;

#else

   return NULL;

#endif
}

/*****************************************************************************

-FUNCTION-
CopyFile: Makes copies of folders and files.

This function is used to copy files and folders to new locations.  When copying folders it will do so
recursively, so as to copy all sub-folders and files within the location.

It is important that you are aware that different types of string formatting can give different results.  The following
examples illustrate:

Copying `parasol:makefile` to `parasol:documents` results in a file called `parasol:documents`.

Copying `parasol:makefile` to `parasol:documents/` results in a file called `parasol:documents/makefile`.

Copying `parasol:pictures/` to `parasol:documents/` results in a folder at `parasol:documents/pictures` and includes
a copy of all folders and files found within the pictures folder.

Copying `parasol:pictures/` to `parasol:documents` results in a folder at `parasol:documents` (if the documents folder
already exists, it receives additional content from the pictures folder).

This function will overwrite any destination file(s) that already exist.

The Source parameter should always clarify the type of location that is being copied.  For example if copying a
folder, a forward slash must terminate the string or it will be assumed that a file is the source.

The Callback parameter can be set with a function that matches this prototype:

`LONG Callback(struct FileFeedback *)`

For each file that is processed during the copy operation, a &FileFeedback structure is passed that describes the
source file and its target.  The callback must return a constant value that can potentially affect file processing.
Valid values are FFR_Okay (copy the file), FFR_Skip (do not copy the file) and FFR_Abort (abort the process completely
and return ERR_Cancelled as an error code).

-INPUT-
cstr Source: The source location.
cstr Dest:   The destination location.
ptr(func) Callback: Optional callback for receiving feedback during the operation.

-ERRORS-
Okay: The location was copied successfully.
Args:
Failed: A failure occurred during the copy process.

*****************************************************************************/

ERROR CopyFile(CSTRING Source, CSTRING Dest, FUNCTION *Callback)
{
   return fs_copy(Source, Dest, Callback, FALSE);
}

/*****************************************************************************

-FUNCTION-
CreateLink: Creates symbolic links on Unix file systems.

Use the CreateLink() function to create symbolic links on Unix file systems. The link connects a new file created at
From to an existing file referenced at To. The To link is allowed to be relative to the From location - for instance,
you can link `documents:myfiles/newlink.txt` to `../readme.txt` or `folder/readme.txt`. The `..` path component must be
used when making references to parent folders.

The permission flags for the link are inherited from the file that you are linking to.  If the file location referenced
at From already exists as a file or folder, the function will fail with an ERR_FileExists error code.

This function does not automatically create folders in circumstances where new folders are required to complete the
From link.  You will need to call ~CreateFolder() to ensure that the necessary paths exist beforehand.  If the
file referenced at To does not exist, the link will be created without error, but any attempts to open the link will
fail until the target file or folder exists.

-INPUT-
cstr From: The symbolic link will be created at the location specified here.
cstr To:   The file that you are linking to is specified here.

-ERRORS-
Okay: The link was created successfully.
NullArgs:
NoSupport: The file system or the host operating system does not support symbolic links.
NoPermission: The user does not have permission to create the link, or the file system is mounted read-only.
ResolvePath:
LowCapacity: There is no room on the device to create the new link.
Memory:
BufferOverflow: One or both of the provided arguments is too long.
FileExists: The location referenced at From already exists.

*****************************************************************************/

ERROR CreateLink(CSTRING From, CSTRING To)
{
#ifdef _WIN32

   return ERR_NoSupport;

#else

   parasol::Log log(__FUNCTION__);
   STRING src, dest;
   LONG err;

   if ((!From) or (!To)) return ERR_NullArgs;

   log.branch("From: %.40s, To: %s", From, To);

   if (!ResolvePath(From, RSF_NO_FILE_CHECK, &src)) {
      if (!ResolvePath(To, RSF_NO_FILE_CHECK, &dest)) {
         err = symlink(dest, src);
         FreeResource(dest);
         FreeResource(src);

         if (!err) return ERR_Okay;
         else return convert_errno(err, ERR_Failed);
      }
      else {
         FreeResource(src);
         return ERR_ResolvePath;
      }
   }
   else return ERR_ResolvePath;

   return ERR_Okay;

#endif
}

/*****************************************************************************

-FUNCTION-
DeleteFile: Deletes files and folders.

This function will delete a file or folder when given a valid file location.  The current user must have delete access
to the given file. When deleting folders, all content will be scanned and deleted recursively. Individual deletion
failures are ignored, although an error will be returned if the top-level folder still contains content on its deletion.

This function does not allow for the approximation of file names.  To approximate a file location, open it as a @File
object or use ~ResolvePath() first.

The Callback parameter can be set with a function that matches this prototype:

`LONG Callback(struct FileFeedback *)`

Prior to the deletion of any file, a &FileFeedback structure is passed that describes the file's location.  The
callback must return a constant value that can potentially affect file processing.  Valid values are FFR_Okay (delete
the file), FFR_Skip (do not delete the file) and FFR_Abort (abort the process completely and return ERR_Cancelled as an
error code).

-INPUT-
cstr Path: String referring to the file or folder to be deleted.  Folders must be denoted with a trailing slash.
ptr(func) Callback: Optional callback for receiving feedback during the operation.

-ERRORS-
Okay: The file or folder was deleted successfully.
NullArgs:
FileNotFound:
File: The location could not be opened for deletion.
NoSupport: The filesystem driver does not support deletion.

*****************************************************************************/

ERROR DeleteFile(CSTRING Path, FUNCTION *Callback)
{
   parasol::Log log(__FUNCTION__);

   if (!Path) return ERR_NullArgs;

   log.branch("%s", Path);

   LONG len = StrLength(Path);

   if (Path[len-1] IS ':') return DeleteVolume(Path);

   ERROR error;
   STRING resolve;
   if (!(error = ResolvePath(Path, 0, &resolve))) {
      const virtual_drive *vd = get_fs(resolve);
      if (vd->Delete) error = vd->Delete(resolve, NULL);
      else error = ERR_NoSupport;
      FreeResource(resolve);
   }

   return error;
}

/*****************************************************************************

-FUNCTION-
SetDefaultPermissions: Forces the user and group permissions to be applied to new files and folders.

By default, user, group and permission information for new files is inherited from the system defaults or from the file
source in copy operations.  This behaviour can be overridden with new default values on a global basis (affects all
threads).

To revert behaviour to the default, set the User and/or Group values to -1 and the Permissions value to zero.

-INPUT-
int User: User ID to apply to new files.
int Group: Group ID to apply to new files.
int(PERMIT) Permissions: Permission flags to be applied to new files.
-END-

*****************************************************************************/

void SetDefaultPermissions(LONG User, LONG Group, LONG Permissions)
{
   parasol::Log log(__FUNCTION__);

   glForceUID = User;
   glForceGID = Group;

   if (Permissions IS -1) {
      // Prevent improper permission settings
      log.warning("Permissions of $%.8x is illegal.", Permissions);
      Permissions = 0;
   }

   glDefaultPermissions = Permissions;
}

//****************************************************************************
// Internal function for getting information from files, particularly virtual volumes.  If you know that a path
// refers directly to the client's filesystem then you can revert to calling fs_getinfo() instead.

ERROR get_file_info(CSTRING Path, FileInfo *Info, LONG InfoSize)
{
   parasol::Log log(__FUNCTION__);
   LONG i, len;
   ERROR error;

   if ((!Path) or (!Path[0]) or (!Info) or (InfoSize <= 0)) return log.warning(ERR_Args);

   char NameBuffer[MAX_FILENAME];
   ClearMemory(Info, InfoSize);
   Info->Name = NameBuffer;

   // Check if the location is a volume with no file reference

   for (len=0; (Path[len]) and (Path[len] != ':'); len++);

   if ((Path[len] IS ':') and (!Path[len+1])) {
      const virtual_drive *vfs = get_fs(Path);

      Info->Flags = RDF_VOLUME;

      for (i=0; (i < MAX_FILENAME-1) and (Path[i]) and (Path[i] != ':'); i++) NameBuffer[i] = Path[i];
      LONG pos = i;
      NameBuffer[i] = 0;

      error = ERR_Okay;

      parasol::ScopedObjectLock<objConfig> volumes(glVolumes);
      if (volumes.granted()) {
         ConfigGroups *groups;
         if (!GetPointer(glVolumes, FID_Data, &groups)) {
            for (auto& [group, keys] : groups[0]) {
               if (!StrMatch(NameBuffer, keys["Name"].c_str())) {
                  if (keys.contains("Hidden")) {
                     if ((!StrMatch("Yes", keys["Hidden"].c_str())) or (!keys["Hidden"].compare("1"))) Info->Flags |= RDF_HIDDEN;
                  }

                  break;
               }
            }
         }
         else error = ERR_FileNotFound;
      }
      else error = ERR_AccessObject;

      if (pos < MAX_FILENAME-2) {
         NameBuffer[pos++] = ':';
         NameBuffer[pos] = 0;

         if (vfs->VirtualID != 0xffffffff) {
            Info->Flags |= RDF_VIRTUAL;
            if (vfs->GetInfo) error = vfs->GetInfo(Path, Info, InfoSize);
         }

         return error;
      }
      else return log.warning(ERR_BufferOverflow);
   }

   log.traceBranch("%s", Path);

   STRING path;
   if (!(error = ResolvePath(Path, 0, &path))) {
      const virtual_drive *vfs = get_fs(path);

      if (vfs->GetInfo) {
         if (vfs->VirtualID != 0xffffffff) Info->Flags |= RDF_VIRTUAL;

         if (!(error = vfs->GetInfo(path, Info, InfoSize))) {
            Info->TimeStamp = calc_timestamp(&Info->Modified);
         }
      }
      else log.warning(ERR_NoSupport);

      FreeResource(path);
   }

   return error;
}

/*****************************************************************************

-FUNCTION-
TranslateCmdRef: Converts program references into command-line format.
Status: Private

TBA

-INPUT-
cstr String: String to translate
!str Command: The resulting command string is returned in this parameter.

-ERRORS-
Okay
NullArgs
StringFormat
NoData
-END-

*****************************************************************************/

ERROR TranslateCmdRef(CSTRING String, STRING *Command)
{
   parasol::Log log(__FUNCTION__);

   if ((!String) or (!Command)) return ERR_NullArgs;

   if (StrCompare("[PROG:", String, sizeof("[PROG:")-1, 0) != ERR_Okay) return ERR_StringFormat;

   *Command = NULL;

   LONG i;
   char buffer[400];
   LONG cmdindex = sizeof("[PROG:") - 1;
   for (i=0; String[cmdindex] and (String[cmdindex] != ']'); i++) buffer[i] = String[cmdindex++];
   buffer[i] = 0;

   log.traceBranch("Command references program '%s'", buffer);

   if (String[cmdindex] IS ']') cmdindex++;
   while ((String[cmdindex]) and (String[cmdindex] <= 0x20)) cmdindex++;

   objConfig *cfgprog;
   ERROR error;
   if (!(error = CreateObject(ID_CONFIG, 0, (OBJECTPTR *)&cfgprog, FID_Path|TSTR, "config:software/programs.cfg", TAGEND))) {
      ConfigGroups *groups;
      if (!GetPointer(cfgprog, FID_Data, &groups)) {
         error = ERR_Failed;
         for (auto& [group, keys] : groups[0]) {
            if (!StrMatch(buffer, group.c_str())) {
               CSTRING cmd, args;
               if (!cfgReadValue(cfgprog, group.c_str(), "CommandFile", &cmd)) {
                  if (cfgReadValue(cfgprog, group.c_str(), "Args", &args)) args = "";
                  StrFormat(buffer, sizeof(buffer), "\"%s\" %s %s", cmd, args, String + cmdindex);

                  *Command = StrClone(buffer);
                  error = ERR_Okay;
               }
               else log.warning("CommandFile value not present for group %s", group.c_str());
               break;
            }
         }
      }
      else error = ERR_NoData;

      acFree(cfgprog);
   }

   return error;
}

/*****************************************************************************

-FUNCTION-
LoadFile: Loads files into a local cache for fast file processing.

The LoadFile() function loads complete files into memory and caches the content for use by other areas of the system
or application.

This function will first determine if the requested file has already been cached.  If this is true then the &CacheFile
structure is returned immediately.  Note that if the file was previously cached but then modified, this will be treated
as a cache miss and the file will be loaded into a new buffer.

File content will be loaded into a readable memory buffer that is referenced by the Data field of the
&CacheFile structure.  A hidden null byte is appended at the end of the buffer to assist the processing of text files.
Other pieces of information about the file can be derived from the &CacheFile meta data.

Calls to LoadFile() must be matched with a call to ~UnloadFile() to decrement the cache counter. When the counter
returns to zero, the file can be unloaded from the cache during the next resource collection phase.

-INPUT-
cstr Path: The location of the file to be cached.
int(LDF) Flags: Optional flags are specified here.
&resource(CacheFile) Cache: A pointer to a CacheFile structure is returned here if successful.

-ERRORS-
Okay: The file was cached successfully.
NullArgs:
AllocMemory:
Search: If LDF_CHECK_EXISTS is specified, this failure indicates that the file is not cached.
-END-

*****************************************************************************/

ERROR LoadFile(CSTRING Path, LONG Flags, CacheFile **Cache)
{
   parasol::Log log(__FUNCTION__);

   if ((!Path) or (!Cache)) return ERR_NullArgs;

   // Check if the file is already cached.  If it is, check that the file hasn't been written since the last time it was cached.

   STRING path;
   ERROR error;
   if ((error = ResolvePath(Path, RSF_APPROXIMATE, &path)) != ERR_Okay) return error;

   const std::lock_guard<std::mutex> lock(glCacheLock);

   log.branch("%.80s, Flags: $%.8x", path, Flags);

   OBJECTPTR file = NULL;
   if (!CreateObject(ID_FILE, 0, &file, FID_Path|TSTR, path, FID_Flags|TLONG, FL_READ|FL_FILE, TAGEND)) {
      LARGE timestamp, file_size;
      GetLarge(file, FID_Size, &file_size);
      GetLarge(file, FID_TimeStamp, &timestamp);

      CacheFileIndex index(path, timestamp, file_size);

      if (glCache.contains(index)) {
         acFree(file);
         FreeResource(path);

         auto cf = glCache[index];
         *Cache = cf;
         if (!(Flags & LDF_CHECK_EXISTS)) cf->Locks++;
         return ERR_Okay;
      }

      // If the client just wanted to check for the existence of the file, do not proceed in loading it.

      if (Flags & LDF_CHECK_EXISTS) {
         acFree(file);
         FreeResource(path);
         return ERR_Search;
      }

      LONG pathlen = StrLength(path) + 1;

      // An additional byte is allocated below so that a null terminator can be attached to the end of the buffer
      // (assists with text file processing).

      CacheFile *cache;
      if (!AllocMemory(sizeof(CacheFile) + pathlen + file_size + 1, MEM_NO_CLEAR|MEM_UNTRACKED, (APTR *)&cache, NULL)) {
         ClearMemory(cache, sizeof(CacheFile));
         cache->Path = (STRING)(cache + 1);
         cache->Data = cache->Path + pathlen;
         ((STRING)cache->Data)[file_size] = 0; // Null terminator is added to help with text file processing
         cache->Locks     = 1;
         cache->Size      = file_size;
         cache->TimeStamp = timestamp;
         cache->LastUse   = PreciseTime();

         CopyMemory(path, cache->Path, pathlen);
         FreeResource(path);

         if (file_size) {
            LONG result;
            error = acRead(file, cache->Data, file_size, &result);
            if ((!error) and (file_size != result)) error = ERR_Read;
         }

         if (!error) {
            glCache[index] = cache;
            *Cache = cache;
            acFree(file);

            if (!glCacheTimer) {
               parasol::SwitchContext context(CurrentTask());
               auto call = make_function_stdc(check_cache);
               SubscribeTimer(60, &call, &glCacheTimer);
            }

            return ERR_Okay;
         }

         FreeResource(cache);
      }
      else error = ERR_AllocMemory;
   }
   else error = ERR_CreateObject;

   if (file) acFree(file);
   FreeResource(path);
   return error;
}

/*****************************************************************************

-FUNCTION-
CreateFolder: Makes new folders.

This function creates new folders.  You are required to specify the full path of the new folder.  Standard
permission flags can be passed to determine the new permissions to set against the newly created Dir(s).  If no
permission flags are passed, only the current user will have access to the new folder (assuming that the file system
supports security settings on the given media).  This function will create multiple folders if the complete path
does not exist at the time of the call.

On Unix systems you can define the owner and group ID's for the new folder by calling the
~SetDefaultPermissions() function prior to CreateFolder().

-INPUT-
cstr Path: The location of the folder.
int(PERMIT) Permissions: Security permissions to apply to the created Dir(s).  Set to NULL if only the current user should have access.

-ERRORS-
Okay:
NullArgs:
FileExists: An identically named file or folder already exists at the Path.
NoSupport:  Virtual file system does not support folder creation.
Failed:

*****************************************************************************/

ERROR CreateFolder(CSTRING Path, LONG Permissions)
{
   parasol::Log log(__FUNCTION__);

   if ((!Path) or (!*Path)) return log.warning(ERR_NullArgs);

   if (glDefaultPermissions) Permissions = glDefaultPermissions;
   else if ((!Permissions) or (Permissions & PERMIT_INHERIT)) {
      Permissions |= get_parent_permissions(Path, NULL, NULL);
      if (!Permissions) Permissions = PERMIT_READ|PERMIT_WRITE|PERMIT_EXEC|PERMIT_GROUP_READ|PERMIT_GROUP_WRITE|PERMIT_GROUP_EXEC; // If no permissions are set, give current user full access
   }

   ERROR error;
   STRING resolve;
   if (!(error = ResolvePath(Path, RSF_NO_FILE_CHECK, &resolve))) {
      const virtual_drive *vd = get_fs(resolve);
      if (vd->CreateFolder) {
         error = vd->CreateFolder(resolve, Permissions);
      }
      else error = ERR_NoSupport;
      FreeResource(resolve);
   }

   return error;
}

/*****************************************************************************

-FUNCTION-
MoveFile: Moves folders and files to new locations.

This function is used to move files and folders to new locations.  It can also be used for renaming purposes and is
able to move data from one type of media to another.  When moving folders, any contents within the folder will
also be moved across to the new location.

It is important that you are aware that different types of string formatting can give different results.  The following
examples illustrate:

<pre>
<b>Source               Destination          Result</b>
parasol:makefile     parasol:documents    parasol:documents
parasol:makefile     parasol:documents/   parasol:documents/makefile
parasol:pictures/    parasol:documents/   parasol:documents/pictures
parasol:pictures/    parasol:documents    parasol:documents (Existing documents folder destroyed)
</>

This function will overwrite the destination location if it already exists.

The Source argument should always clarify the type of location that is being copied - e.g. if you are copying a
folder, you must specify a forward slash at the end of the string or the function will assume that you are moving a
file.

The Callback parameter can be set with a function that matches this prototype:

`LONG Callback(struct FileFeedback *)`

For each file that is processed during the move operation, a &FileFeedback structure is passed that describes the
source file and its target.  The callback must return a constant value that can potentially affect file processing.
Valid values are `FFR_Okay` (move the file), `FFR_Skip` (do not move the file) and FFR_Abort (abort the process completely
and return `ERR_Cancelled` as an error code).

-INPUT-
cstr Source: The source path.
cstr Dest:   The destination path.
ptr(func) Callback: Optional callback for receiving feedback during the operation.

-ERRORS-
Okay
NullArgs
Failed

*****************************************************************************/

ERROR MoveFile(CSTRING Source, CSTRING Dest, FUNCTION *Callback)
{
   parasol::Log log(__FUNCTION__);

   if ((!Source) or (!Dest)) return ERR_NullArgs;

   log.branch("%s to %s", Source, Dest);
   return fs_copy(Source, Dest, Callback, TRUE);
}

/******************************************************************************

-FUNCTION-
ReadFileToBuffer: Reads a file into a buffer.

This function provides a simple method for reading file content into a buffer.  In some cases this procedure may be
optimised for the host platform, which makes it the fastest way to read file content in simple cases.

File path approximation is supported if the Path is prefixed with a `~` character (e.g. `~pictures:photo` could be
matched to `photo.jpg` in the same folder).

-INPUT-
cstr Path: The path of the file.
buf(ptr) Buffer: Pointer to a buffer that will receive the file content.
bufsize BufferSize: The byte size of the Buffer.
&int Result: The total number of bytes read into the Buffer will be returned here (optional).

-ERRORS-
Okay
Args
NullArgs
OpenFile
InvalidPath
Read
File
-END-

******************************************************************************/

ERROR ReadFileToBuffer(CSTRING Path, APTR Buffer, LONG BufferSize, LONG *BytesRead)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("Path: %s, Buffer Size: %d", Path, BufferSize);

#if defined(__unix__) || defined(_WIN32)
   if ((!Path) or (BufferSize <= 0) or (!Buffer)) return ERR_Args;

   BYTE approx;
   if (*Path IS '~') {
      Path++;
      approx = TRUE;
   }
   else approx = FALSE;

   if (BytesRead) *BytesRead = 0;

   ERROR error;
   STRING res_path;
   if (!(error = ResolvePath(Path, RSF_CHECK_VIRTUAL | (approx ? RSF_APPROXIMATE : 0), &res_path))) {
      if (StrCompare("/dev/", res_path, 5, 0) != ERR_Okay) {
         LONG handle;
         if ((handle = open(res_path, O_RDONLY|O_NONBLOCK|O_LARGEFILE|WIN32OPEN, NULL)) != -1) {
            LONG result;
            if ((result = read(handle, Buffer, BufferSize)) IS -1) {
               error = ERR_Read;
               #ifdef __unix__
                  log.warning("read(%s, %p, %d): %s", Path, Buffer, BufferSize, strerror(errno));
               #endif
            }
            else if (BytesRead) *BytesRead = result;

            close(handle);
         }
         else {
            #ifdef __unix__
               log.warning("open(%s): %s", Path, strerror(errno));
            #endif
            error = ERR_OpenFile;
         }
      }
      else error = ERR_InvalidPath;

      FreeResource(res_path);
   }
   else if (error IS ERR_VirtualVolume) {
      extFile *file;

      if (!CreateObject(ID_FILE, 0, (OBJECTPTR *)&file,
            FID_Path|TSTR,   res_path,
            FID_Flags|TLONG, FL_READ|FL_FILE|(approx ? FL_APPROXIMATE : 0),
            TAGEND)) {

         if (!acRead(file, Buffer, BufferSize, BytesRead)) error = ERR_Okay;
         else error = ERR_Read;

         acFree(file);
      }
      else error = ERR_File;

      FreeResource(res_path);
      return error;
   }
   else error = ERR_FileNotFound;

   #ifdef DEBUG
      if (error) log.warning(error);
   #endif
   return error;

#else

   extFile *file;

   if (!CreateObject(ID_FILE, 0, (OBJECTPTR *)&file,
         FID_Path|TSTR,   Path,
         FID_Flags|TLONG, FL_READ|FL_FILE|(approx ? FL_APPROXIMATE : 0),
         TAGEND)) {

      LONG result;
      if (!acRead(file, Buffer, BufferSize, &result)) {
         if (BytesRead) *BytesRead = result;
         acFree(file);
         return ERR_Okay;
      }
      else {
         acFree(file);
         return ERR_Read;
      }
   }
   else return ERR_File;

#endif
}

/*****************************************************************************
** The Path passed to this function must be a completely resolved path.  Note that the Path argument needs to be a
** large buffer as this function will modify it.
*/

ERROR test_path(STRING Path, LONG Flags)
{
   parasol::Log log(__FUNCTION__);
   virtual_drive *vd;
   LONG len, type;
#ifdef _WIN32
   LONG j;
#elif __unix__
   struct stat64 info;
#endif

   if (!Path) return ERR_NullArgs;

   log.trace("%s", Path);

   if ((vd = get_virtual(Path))) {
      if (vd->TestPath) {
         if (!vd->TestPath(Path, Flags, &type)) {
            return ERR_Okay;
         }
         else return ERR_FileNotFound;
      }
      else return ERR_Okay;
   }

#ifdef _WIN32
   // Convert forward slashes to back slashes
   for (j=0; Path[j]; j++) if (Path[j] IS '/') Path[j] = '\\';
#endif

   len = StrLength(Path);
   if ((Path[len-1] IS '/') or (Path[len-1] IS '\\')) {
      // This code handles testing for folder locations

      #ifdef __unix__

         if (len IS 1) return ERR_Okay; // Do not lstat() the root '/' folder

         Path[len-1] = 0;
         LONG result = lstat64(Path, &info);
         Path[len-1] = '/';

         if (!result) return ERR_Okay;

      #elif _WIN32

         if (winCheckDirectoryExists(Path)) return ERR_Okay;
         else log.trace("Folder does not exist.");

      #else
         #error Require folder testing code for this platform.
      #endif
   }
   else {
      // This code handles testing for file locations

      if (Flags & RSF_APPROXIMATE) {
         if (!findfile(Path)) return ERR_Okay;
      }
#ifdef __unix__
      else if (!lstat64(Path, &info)) {
         if (S_ISDIR(info.st_mode)) {
            Path[len++] = '/';
            Path[len] = 0;
         }

         return ERR_Okay;
      }
#else
      else if (!access(Path, F_OK)) {
         return ERR_Okay;
      }
      //else log.trace("access() failed.");
#endif
   }

   return ERR_FileNotFound;
}

/*****************************************************************************

-FUNCTION-
SaveImageToFile: Saves an object's image to a destination file.

This function simplifies the process of saving object images to files.  You need to provide the object address and the
destination file location for the data to be saved.  If the destination file exists, it will be overwritten.

The object's class must support the #SaveToObject() action or this function will return ERR_NoSupport.
Sub-classes are supported if the image needs to be saved as a specific type of file (for example, to save a picture
object as a JPEG file, the Class must be set to ID_JPEG).

-INPUT-
obj Object: Pointer to the object that contains the image to be saved.
cstr Path: The destination file location.
cid Class: The sub-class to use when saving the image (optional).
int(PERMIT) Permissions: File permissions to use (optional).  If NULL, file is saved with user and group permissions of read/write.

-ERRORS-
Okay: The volume was successfully added.
Args: A valid Path argument was not provided.
CreateFile: Failed to create the file at the indicated destination.
NoSupport: The object does not support the SaveImage action.
-END-

*****************************************************************************/

ERROR SaveImageToFile(OBJECTPTR Object, CSTRING Path, CLASSID ClassID, LONG Permissions)
{
   parasol::Log log(__FUNCTION__);
   OBJECTPTR file;
   ERROR error;

   log.branch("Object: %d, Dest: %s", Object->UID, Path);

   if (!(error = CreateObject(ID_FILE, 0, (OBJECTPTR *)&file,
         FID_Path|TSTR,         Path,
         FID_Flags|TLONG,       FL_WRITE|FL_NEW,
         FID_Permissions|TLONG, Permissions,
         TAGEND))) {

      error = acSaveImage(Object, file->UID, ClassID);

      acFree(file);
      return error;
   }
   else return log.warning(ERR_CreateFile);
}

/*****************************************************************************

-FUNCTION-
SaveObjectToFile: Saves an object to a destination file.

This support function simplifies the process of saving objects to files.  A source Object must be provided and a
destination path for the data.  If the destination file exists, it will be overwritten.

The object's class must support the SaveToObject action or this function will return `ERR_NoSupport`.

-INPUT-
obj Object: Pointer to the source object that will be saved.
cstr Path: The destination file path.
int(PERMIT) Permissions: File permissions to use (optional).

-ERRORS-
Okay:
NullArgs:
CreateFile:
NoSupport: The object does not support the SaveToObject action.

*****************************************************************************/

ERROR SaveObjectToFile(OBJECTPTR Object, CSTRING Path, LONG Permissions)
{
   parasol::Log log(__FUNCTION__);

   if ((!Object) or (!Path)) return log.warning(ERR_NullArgs);

   log.branch("#%d to %s", Object->UID, Path);

   OBJECTPTR file;
   ERROR error;
   if (!(error = CreateObject(ID_FILE, 0, (OBJECTPTR *)&file,
         FID_Path|TSTRING,      Path,
         FID_Flags|TLONG,       FL_WRITE|FL_NEW,
         FID_Permissions|TLONG, Permissions,
         TAGEND))) {

      error = acSaveToObject(Object, file->UID, 0);

      acFree(file);
      return error;
   }
   else return ERR_CreateFile;
}

/*****************************************************************************

-FUNCTION-
UnloadFile: Unloads files from the file cache.

This function unloads cached files that have been previously loaded with the ~LoadFile() function.

-INPUT-
resource(CacheFile) Cache: A pointer to a CacheFile structure returned from LoadFile().
-END-

*****************************************************************************/

void UnloadFile(CacheFile *Cache)
{
   if (!Cache) return;

   parasol::Log log(__FUNCTION__);

   log.function("%.80s, Locks: %d", Cache->Path, Cache->Locks);

   const std::lock_guard<std::mutex> lock(glCacheLock);

   if (Cache->Locks > 0) Cache->Locks--;

   // Cache entries are never removed here because this determination is handled by check_cache().
}

//****************************************************************************
// NOTE: The argument passed as the folder must be a large buffer to compensate for the resulting filename.

#ifdef __unix__

struct olddirent {
   long d_ino;                 // inode number
   off_t d_off;                // offset to next dirent
   unsigned short d_reclen;    // length of this dirent
   char d_name[];              // file name (null-terminated)
};

ERROR findfile(STRING Path)
{
   parasol::Log log("FindFile");
   struct stat64 info;
   DIR *dummydir;
   LONG namelen, len;
   char save;

   if ((!Path) or (Path[0] IS ':')) return ERR_Args;

   // Return if the file exists at the specified Path and is not a folder

   if (lstat64(Path, &info) != -1) {
      if (!S_ISDIR(info.st_mode)) return ERR_Okay;
   }

   for (len=0; Path[len]; len++);
   while ((len > 0) and (Path[len-1] != ':') and (Path[len-1] != '/') and (Path[len-1] != '\\')) len--;
   for (namelen=0; Path[len+namelen]; namelen++);

   save = Path[len];
   Path[len] = 0;

   // Scan the files at the Path to find a similar filename (ignore the filename extension).

   // Note: We use getdents() instead of the opendir()/readdir() method due to glibc bugs surrounding readdir() [problem noticed around kernel 2.4+].

   log.trace("Scanning Path %s", Path);

#if 1

   struct dirent *entry;
   DIR *dir;

   if ((dir = opendir(Path))) {
      rewinddir(dir);
      Path[len] = save;

      while ((entry = readdir(dir))) {
         if ((entry->d_name[0] IS '.') and (entry->d_name[1] IS 0)) continue;
         if ((entry->d_name[0] IS '.') and (entry->d_name[1] IS '.') and (entry->d_name[2] IS 0)) continue;

         if ((!StrCompare(Path+len, entry->d_name, 0, 0)) and
             ((entry->d_name[namelen] IS '.') or (!entry->d_name[namelen]))) {
            StrCopy(entry->d_name, Path+len, COPY_ALL);

            // If it turns out that the Path is a folder, ignore it

            if ((dummydir = opendir(Path))) {
               closedir(dummydir);
               continue;
            }

            closedir(dir);
            return ERR_Okay;
         }
      }

      closedir(dir);
   }
   else Path[len] = save;

#else

   struct olddirent *entry;
   UBYTE buffer[8192];
   LONG dir, bytes;

   if ((dir = open(Path, O_RDONLY|O_NONBLOCK|O_LARGEFILE|WIN32OPEN, NULL)) != -1) {
      Path[len] = save;

      if ((bytes = syscall(SYS_getdents, dir, buffer, sizeof(buffer))) > 0) {
         for (entry = (struct olddirent *)buffer; (size_t)entry < (size_t)(buffer + bytes); entry = (struct olddirent *)(((BYTE *)entry) + entry->d_reclen)) {
            if ((entry->d_name[0] IS '.') and (entry->d_name[1] IS 0)) continue;
            if ((entry->d_name[0] IS '.') and (entry->d_name[1] IS '.') and (entry->d_name[2] IS 0)) continue;

            if ((!StrCompare(Path+len, entry->d_name, 0, NULL)) and
                ((entry->d_name[namelen] IS '.') or (!entry->d_name[namelen]))) {
               StrCopy(entry->d_name, Path+len, COPY_ALL);

               // If it turns out that the Path is a folder, ignore it

               if ((dummydir = opendir(Path))) {
                  closedir(dummydir);
                  continue;
               }

               close(dir);
               return ERR_Okay;
            }
         }
      }

      close(dir);
   }
   else Path[len] = save;

#endif

   return ERR_Search;
}

#elif _WIN32

ERROR findfile(STRING Path)
{
   if ((!Path) or (Path[0] IS ':')) return ERR_Args;

   // Find a file with the standard Path

   LONG filehandle;
   if ((filehandle = open(Path, O_RDONLY|O_LARGEFILE|WIN32OPEN, NULL)) != -1) {
      close(filehandle);
      return ERR_Okay;
   }

   // Find a file with an extension

   LONG len = StrLength(Path);
   Path[len] = '.';
   Path[len+1] = '*';
   Path[len+2] = 0;

   char buffer[130];
   APTR handle = NULL;
   if ((handle = winFindFile(Path, &handle, buffer))) {
      while ((len > 0) and (Path[len-1] != ':') and (Path[len-1] != '/') and (Path[len-1] != '\\')) len--;
      StrCopy(buffer, Path+len, COPY_ALL);
      winFindClose(handle);
      return ERR_Okay;
   }

   return ERR_Search;
}

#endif

//****************************************************************************

LONG convert_permissions(LONG Permissions)
{
   LONG flags = 0;

#ifdef __unix__
   if (Permissions & PERMIT_READ)         flags |= S_IRUSR;
   if (Permissions & PERMIT_WRITE)        flags |= S_IWUSR;
   if (Permissions & PERMIT_EXEC)         flags |= S_IXUSR;

   if (Permissions & PERMIT_GROUP_READ)   flags |= S_IRGRP;
   if (Permissions & PERMIT_GROUP_WRITE)  flags |= S_IWGRP;
   if (Permissions & PERMIT_GROUP_EXEC)   flags |= S_IXGRP;

   if (Permissions & PERMIT_OTHERS_READ)  flags |= S_IROTH;
   if (Permissions & PERMIT_OTHERS_WRITE) flags |= S_IWOTH;
   if (Permissions & PERMIT_OTHERS_EXEC)  flags |= S_IXOTH;

   if (Permissions & PERMIT_USERID)       flags |= S_ISUID;
   if (Permissions & PERMIT_GROUPID)      flags |= S_ISGID;
#else
   if (Permissions & PERMIT_ALL_READ)     flags |= S_IREAD;
   if (Permissions & PERMIT_ALL_WRITE)    flags |= S_IWRITE;
   if (Permissions & PERMIT_ALL_EXEC)     flags |= S_IEXEC;
#endif

   return flags;
}

//****************************************************************************

LONG convert_fs_permissions(LONG Permissions)
{
   LONG flags = 0;

#ifdef __unix__
   if (Permissions & S_IRUSR) flags |= PERMIT_READ;
   if (Permissions & S_IWUSR) flags |= PERMIT_WRITE;
   if (Permissions & S_IXUSR) flags |= PERMIT_EXEC;

   if (Permissions & S_IRGRP) flags |= PERMIT_GROUP_READ;
   if (Permissions & S_IWGRP) flags |= PERMIT_GROUP_WRITE;
   if (Permissions & S_IXGRP) flags |= PERMIT_GROUP_EXEC;

   if (Permissions & S_IROTH) flags |= PERMIT_OTHERS_READ;
   if (Permissions & S_IWOTH) flags |= PERMIT_OTHERS_WRITE;
   if (Permissions & S_IXOTH) flags |= PERMIT_OTHERS_EXEC;

   if (Permissions & S_ISGID) flags |= PERMIT_GROUPID;
   if (Permissions & S_ISUID) flags |= PERMIT_USERID;
#else
   if (Permissions & S_IREAD)  flags |= PERMIT_READ;
   if (Permissions & S_IWRITE) flags |= PERMIT_WRITE;
   if (Permissions & S_IEXEC)  flags |= PERMIT_EXEC;
#endif
   return flags;
}

//*****************************************************************************
// Strips the filename and calls CreateFolder() to create all paths leading up to the filename.

ERROR check_paths(CSTRING Path, LONG Permissions)
{
   parasol::Log log(__FUNCTION__);

   log.traceBranch("%s", Path);

   LONG i = StrLength(Path);

   {
      char path[i+1];
      CopyMemory(Path, path, i);

      while (i > 0) {
         if ((path[i-1] IS ':') or (path[i-1] IS '/') or (path[i-1] IS '\\')) {
            path[i] = 0;
            return CreateFolder(path, Permissions);
         }
         i--;
      }
   }

   return ERR_Failed;
}

//***************************************************************************
// This low level function is used for copying/moving/renaming files and folders.

ERROR fs_copy(CSTRING Source, CSTRING Dest, FUNCTION *Callback, BYTE Move)
{
   parasol::Log log(Move ? "MoveFile" : "CopyFile");
#ifdef __unix__
   struct stat64 stinfo;
   LONG parentpermissions, gid, uid;
   LONG i;
#endif
#ifdef _WIN32
   LONG handle;
#else
   LONG handle;
#endif
   STRING src, tmp;
   LONG permissions, dhandle, len;
   LONG srclen, result;
   char dest[2000];
   BYTE srcdir;
   ERROR error;

   if ((!Source) or (!Source[0]) or (!Dest) or (!Dest[0])) return log.warning(ERR_NullArgs);

   log.traceBranch("\"%s\" to \"%s\"", Source, Dest);

   extFile *srcfile = NULL;
   extFile *destfile = NULL;

   if ((error = ResolvePath(Source, 0, &src)) != ERR_Okay) {
      return ERR_FileNotFound;
   }

   if ((error = ResolvePath(Dest, RSF_NO_FILE_CHECK, &tmp)) != ERR_Okay) {
      FreeResource(src);
      return ERR_ResolvePath;
   }

   const virtual_drive *srcvirtual  = get_fs(src);
   const virtual_drive *destvirtual = get_fs(tmp);

   LONG destlen = StrCopy(tmp, dest, sizeof(dest));
   FreeResource(tmp);

   // Check if the source is a folder

   for (srclen=0; src[srclen]; srclen++);
   if ((src[srclen-1] IS '/') or (src[srclen-1] IS '\\')) srcdir = TRUE;
   else srcdir = FALSE;

   // If the destination is a folder, we need to copy the name of the source to create the new file or dir.

   if ((dest[destlen-1] IS '/') or (dest[destlen-1] IS '\\') or (dest[destlen-1] IS ':')) {
      len = srclen;
      if ((src[len-1] IS '/') or (src[len-1] IS '\\') or (src[len-1] IS ':')) len--;
      while ((len > 0) and (src[len-1] != '/') and (src[len-1] != '\\') and (src[len-1] != ':')) len--;

      while (((size_t)destlen < sizeof(dest)-1) and (src[len]) and (src[len] != '/') and (src[len] != '\\')) dest[destlen++] = src[len++];
      dest[destlen] = 0;
   }

   if ((size_t)destlen >= sizeof(dest)) {
      error = ERR_BufferOverflow;
      goto exit;
   }

   log.trace("Copy: %s TO %s", src, dest);

   if (!CompareFilePaths(src, dest)) {
      log.trace("The source and destination refer to the same location.");
      if (Move) return ERR_IdenticalPaths; // Move fails if source and dest are identical, since the source is not deleted
      else return ERR_Okay; // Copy succeeds if source and dest are identical
   }

   FileFeedback feedback;
   ClearMemory(&feedback, sizeof(feedback));
   if (Move) feedback.FeedbackID = FBK_MOVE_FILE;
   else feedback.FeedbackID = FBK_COPY_FILE;

   feedback.Path = src;
   feedback.Dest = dest;

   if ((srcvirtual->VirtualID != 0xffffffff) or (destvirtual->VirtualID != 0xffffffff)) {
      APTR data;
      LONG bufsize, result;

      log.trace("Using virtual copy routine.");

      // Open the source and destination

      if (!CreateObject(ID_FILE, 0, (OBJECTPTR *)&srcfile,
            FID_Path|TSTR,   Source,
            FID_Flags|TLONG, FL_READ,
            TAGEND)) {

         if ((Move) and (srcvirtual IS destvirtual)) {
            // If the source and destination use the same virtual volume, execute the move method.

            error = flMove(srcfile, Dest, NULL);
            goto exit;
         }

         if (!CreateObject(ID_FILE, 0, (OBJECTPTR *)&destfile,
               FID_Path|TSTR,         Dest,
               FID_Flags|TLONG,       FL_WRITE|FL_NEW,
               FID_Permissions|TLONG, srcfile->Permissions,
               TAGEND)) {
         }
         else {
            error = ERR_CreateFile;
            goto exit;
         }
      }
      else {
         error = ERR_FileNotFound;
         goto exit;
      }

      // Folder copy

      if (srcfile->Flags & FL_FOLDER) {
         char srcbuffer[2000];

         if (!(destfile->Flags & FL_FOLDER)) {
            // You cannot copy from a folder to a file
            error = ERR_Mismatch;
            goto exit;
         }

         srclen  = StrCopy(src, srcbuffer, sizeof(srcbuffer));

         // Check if the copy would cause recursion  - e.g. "/parasol/system/" to "/parasol/system/temp/".

         if (srclen <= destlen) {
            if (StrCompare(src, dest, srclen, 0) IS ERR_True) {
               log.warning("The requested copy would cause recursion.");
               error = ERR_Loop;
               goto exit;
            }
         }

         // Create the destination folder, then copy the source folder across using a recursive routine.

         if (glDefaultPermissions) CreateFolder(dest, glDefaultPermissions);
         else CreateFolder(dest, PERMIT_USER|PERMIT_GROUP);

         if (!(error = fs_copydir(srcbuffer, dest, &feedback, Callback, Move))) {
            // Delete the source if we are moving folders
            if (Move) error = DeleteFile(srcbuffer, NULL);
         }
         else log.warning("Folder copy process failed, error %d.", error);

         goto exit;
      }

      // Standard file copy

      feedback.Position = 0;

      // Use a reasonably small read buffer so that we can provide continuous feedback

      bufsize = ((Callback) and (Callback->Type)) ? 65536 : 65536 * 2;

      // This routine is designed to handle streams - where either the source is a stream or the destination is a stream.

      error = ERR_Okay;
      if (!AllocMemory(bufsize, MEM_DATA|MEM_NO_CLEAR, (APTR *)&data, NULL)) {
         #define STREAM_TIMEOUT (10000LL)

         LARGE time = (PreciseTime() / 1000LL);
         while (srcfile->Position < srcfile->Size) {
            error = acRead(srcfile, data, bufsize, &len);
            if (error) {
               log.warning("acRead() failed: %s", GetErrorMsg(error));
               break;
            }

            feedback.Position += len;

            if (len) {
               time = (PreciseTime()/1000LL);
            }
            else {
               log.msg("Failed to read any data, position " PF64() " / " PF64() ".", srcfile->Position, srcfile->Size);

               if ((PreciseTime() / 1000LL) - time > STREAM_TIMEOUT) {
                  log.warning("Timeout - stopped reading at offset " PF64() " of " PF64() "", srcfile->Position, srcfile->Size);
                  error = ERR_TimeOut;
                  break;
               }
            }

            // Write the data

            while (len > 0) {
               if ((error = acWrite(destfile, data, len, &result)) != ERR_Okay) {
                  error = ERR_Write;
                  break;
               }

               if (result) time = (PreciseTime() / 1000LL);
               else if ((PreciseTime() / 1000LL) - time > STREAM_TIMEOUT) {
                  log.warning("Timeout - failed to write remaining %d bytes.", len);
                  error = ERR_TimeOut;
                  break;
               }

               len -= result;
               if (destfile->Flags & FL_STREAM) {

               }
               else if (len > 0) {
                  log.warning("Out of space - wrote %d bytes, %d left.", result, len);
                  error = ERR_OutOfSpace;
                  break;
               }

               if (len > 0) ProcessMessages(0, 0);
            }

            if (error) break;

            if ((Callback) and (Callback->Type)) {
               if (feedback.Size < feedback.Position) feedback.Size = feedback.Position;

               result = CALL_FEEDBACK(Callback, &feedback);

               if (result IS FFR_ABORT) { error = ERR_Cancelled; break; }
               else if (result IS FFR_SKIP) break;
            }

            ProcessMessages(0, 0);
         }

         FreeResource(data);
      }
      else error = log.warning(ERR_AllocMemory);

      if ((Move) and (!error)) {
         flDelete(srcfile, 0);
      }

      goto exit;
   }

#ifdef __unix__
   // This code manages symbolic links

   if (srcdir) {
      src[srclen-1] = 0;
      result = lstat64(src, &stinfo);
      src[srclen-1] = '/';
   }
   else result = lstat64(src, &stinfo);

   if ((!result) and (S_ISLNK(stinfo.st_mode))) {
      BYTE linkto[512];

      if (srcdir) src[srclen-1] = 0;

      if ((i = readlink(src, linkto, sizeof(linkto)-1)) != -1) {
         linkto[i] = 0;

         if ((Callback) and (Callback->Type)) {
            result = CALL_FEEDBACK(Callback, &feedback);
            if (result IS FFR_ABORT) { error = ERR_Cancelled; goto exit; }
            else if (result IS FFR_SKIP) { error = ERR_Okay; goto exit; }
         }

         unlink(dest); // Remove any existing file first

         if (!symlink(linkto, dest)) error = ERR_Okay;
         else {
            // On failure, it may be possible that precursing folders need to be created for the link.  Do this here and then try
            // creating the link a second time.

            check_paths(dest, PERMIT_READ|PERMIT_WRITE|PERMIT_GROUP_READ|PERMIT_GROUP_WRITE);

            if (!symlink(linkto, dest)) error = ERR_Okay;
            else {
               log.warning("Failed to create link \"%s\"", dest);
               error = ERR_CreateFile;
            }
         }
      }
      else {
        log.warning("Failed to read link \"%s\"", src);
        error = ERR_Read;
      }

      if ((Move) and (!error)) { // Delete the source
         error = DeleteFile(src, NULL);
      }

      goto exit;
   }

   feedback.Size = stinfo.st_size;
#endif

   if (Move) {
      // Attempt to move the source to the destination using a simple rename operation.  If the rename fails,
      // the files may need to be copied across drives, so we don't abort in the case of failure.

      error = ERR_Okay;

      if ((Callback) and (Callback->Type)) {
         result = CALL_FEEDBACK(Callback, &feedback);
         if (result IS FFR_ABORT) { error = ERR_Cancelled; goto exit; }
         else if (result IS FFR_SKIP) goto exit;
      }

#ifdef _WIN32
      if (rename(src, dest)) {
         // failed - drop through to file copy
      }
      else goto exit; // success
#else
      if (rename(src, dest) IS -1) {
         // failed - drop through to file copy
      }
      else {
         LONG parent_uid, parent_gid;

         // Move successful.  Now assign the user and group id's from the parent folder to the file.

         parentpermissions = get_parent_permissions(dest, &parent_uid, &parent_gid) & (~PERMIT_ALL_EXEC);

         gid = -1;
         uid = -1;

         if (parentpermissions & PERMIT_USERID) uid = parent_uid;
         if (parentpermissions & PERMIT_GROUPID) gid = parent_gid;

         if (glForceGID != -1) gid = glForceGID;
         if (glForceUID != -1) uid = glForceUID;

         if ((uid != -1) or (gid != -1)) chown(dest, uid, gid);

         goto exit; // success
      }
#endif
   }

   if (srcdir) {
      char srcbuffer[2000];

      // The source location is expressed as a folder string.  Confirm that the folder exists before continuing.

      #ifdef _WIN32
         if (winCheckDirectoryExists(src));
         else {
            error = ERR_File;
            goto exit;
         }
      #else
         DIR *dirhandle;
         if ((dirhandle = opendir(src))) {
            closedir(dirhandle);
         }
         else {
            error = ERR_File;
            goto exit;
         }
      #endif

      srclen = StrCopy(src, srcbuffer, sizeof(srcbuffer));

      // Check if the copy would cause recursion  - e.g. "/parasol/system/" to "/parasol/system/temp/".

      if (srclen <= destlen) {
         if (StrCompare(src, dest, srclen, 0) IS ERR_True) {
            log.warning("The requested copy would cause recursion.");
            error = ERR_Loop;
            goto exit;
         }
      }

      // Create the destination folder, then copy the source folder across using a recursive routine.

      if (glDefaultPermissions) CreateFolder(dest, glDefaultPermissions);
      else {
#ifdef _WIN32
         CreateFolder(dest, PERMIT_USER|PERMIT_GROUP);
#else
         if (stat64(src, &stinfo) != -1) {
            CreateFolder(dest, convert_fs_permissions(stinfo.st_mode));
            chown(dest, (glForceUID != -1) ? glForceUID : stinfo.st_uid, (glForceGID != -1) ? glForceGID : stinfo.st_gid);
         }
         else {
            log.warning("stat64() failed for %s", src);
            CreateFolder(dest, PERMIT_USER|PERMIT_GROUP);
         }
#endif
      }

      if (!(error = fs_copydir(srcbuffer, dest, &feedback, Callback, Move))) {
         // Delete the source if we are moving folders
         if (Move) error = DeleteFile(srcbuffer, NULL);
      }
      else log.warning("Folder copy process failed, error %d.", error);

      goto exit;
   }

   if (!Move) { // (If Move is enabled, we would have already sent feedback during the earlier rename() attempt
      if ((Callback) and (Callback->Type)) {
         result = CALL_FEEDBACK(Callback, &feedback);
         if (result IS FFR_ABORT) { error = ERR_Cancelled; goto exit; }
         else if (result IS FFR_SKIP) { error = ERR_Okay; goto exit; }
      }
   }

   if ((handle = open(src, O_RDONLY|O_NONBLOCK|WIN32OPEN|O_LARGEFILE, NULL)) != -1) {

      // Get permissions of the source file to apply to the destination file

#ifdef _WIN32
      if (glDefaultPermissions) {
         if (glDefaultPermissions & PERMIT_INHERIT) {
            //LONG parentpermissions;
            //parentpermissions = get_parent_permissions(dest, NULL, NULL) & (~PERMIT_ALL_EXEC);
            //permissions = convert_permissions((parentpermissions & (~(PERMIT_USERID|PERMIT_GROUPID))) | glDefaultPermissions);
             permissions = S_IREAD|S_IWRITE;
         }
         else permissions = convert_permissions(glDefaultPermissions);
      }
      else permissions = S_IREAD|S_IWRITE;

      winFileInfo(src, &feedback.Size, NULL, NULL);
#else
      parentpermissions = get_parent_permissions(dest, NULL, NULL) & (~PERMIT_ALL_EXEC);
      if (glDefaultPermissions) {
         if (glDefaultPermissions & PERMIT_INHERIT) {
            permissions = convert_permissions((parentpermissions & (~(PERMIT_USERID|PERMIT_GROUPID))) | glDefaultPermissions);
         }
         else permissions = convert_permissions(glDefaultPermissions);
      }
      else {
         if (fstat64(handle, &stinfo) IS -1) permissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
         else permissions = stinfo.st_mode;
      }

      feedback.Size = stinfo.st_size;
#endif

      // Delete any existing destination file first so that we can give it new permissions.
      // This will also help when assessing the amount of free space on the destination device.

#if defined(__unix__) || defined(_WIN32)
      unlink(dest);
#else
      DeleteFile(dest, NULL);
#endif

      // Check if there is enough room to copy this file to the destination

      objStorageDevice *device;
      if (!CreateObject(ID_STORAGEDEVICE, 0, (OBJECTPTR *)&device,
            FID_Volume|TSTR, dest,
            TAGEND)) {
         if (device->BytesFree >= 0) {
            if (device->BytesFree - 1024LL <= feedback.Size) {
               close(handle);
               log.warning("Not enough space on device (" PF64() "/" PF64() " < " PF64() ")", device->BytesFree, device->DeviceSize, (LARGE)feedback.Size);
               error = ERR_OutOfSpace;
               acFree(device);
               goto exit;
            }
         }
         acFree(device);
      }

      if ((dhandle = open(dest, O_WRONLY|O_CREAT|O_TRUNC|O_LARGEFILE|WIN32OPEN, permissions)) IS -1) {
         // If the initial open failed, we may need to create preceding paths
         check_paths(dest, convert_fs_permissions(permissions));
         dhandle = open(dest, O_WRONLY|O_CREAT|O_TRUNC|O_LARGEFILE|WIN32OPEN, permissions);
      }

#ifdef __unix__
      // Set the owner and group for the file to match the original.  This only works if the user is root.  If the sticky bits are set on the parent folder,
      // do not override the group and user settings (the id's will be inherited from the parent folder instead).

      // fchown() ignores id's when they are set to -1.

      if (glForceGID != -1) gid = glForceGID;
      else gid = stinfo.st_gid;

      if (glForceUID != -1) uid = glForceUID;
      else uid = stinfo.st_uid;

      if (parentpermissions & PERMIT_GROUPID) gid = -1;
      if (parentpermissions & PERMIT_USERID) uid = -1;

      if ((uid != -1) or (gid != -1)) fchown(dhandle, uid, gid);

#endif

      feedback.Position = 0;

      if (dhandle != -1) {
         APTR data;

         // Use a reasonably small read buffer so that we can provide continuous feedback

         LONG bufsize = ((Callback) and (Callback->Type)) ? 65536 : 524288;
         error = ERR_Okay;
         if (!AllocMemory(bufsize, MEM_DATA|MEM_NO_CLEAR, (APTR *)&data, NULL)) {
            while ((len = read(handle, data, bufsize)) > 0) {
               LONG result;
               if ((result = write(dhandle, data, len)) IS -1) {
                  if (errno IS ENOSPC) error = log.warning(ERR_OutOfSpace);
                  else error = log.warning(ERR_Write);
                  break;
               }
               else if (result < len) {
                  log.warning("Wrote %d of %d bytes.", result, len);
                  error = ERR_OutOfSpace;
                  break;
               }


               if ((Callback) and (Callback->Type)) {
                  feedback.Position += len;
                  if (feedback.Size < feedback.Position) feedback.Size = feedback.Position;
                  result = CALL_FEEDBACK(Callback, &feedback);
                  if (result IS FFR_ABORT) { error = ERR_Cancelled; break; }
                  else if (result IS FFR_SKIP) break;
               }
            }

            if (len IS -1) {
               log.warning("Error reading source file.");
               error = ERR_Read;
            }

            FreeResource(data);
         }
         else error = log.warning(ERR_AllocMemory);

#ifdef __unix__
         // If the sticky bits were set, we need to set them again because Linux sneakily turns off those bits when a
         // file is written (for security reasons).

         if ((!error) and (permissions & (S_ISUID|S_ISGID))) {
            fchmod(dhandle, permissions);
         }
#endif

         close(dhandle);
      }
      else error = log.warning(ERR_CreateFile);

      close(handle);
   }
   else error = log.warning(ERR_FileNotFound);

   if ((Move) and (!error)) { // Delete the source
      error = DeleteFile(src, NULL);
   }

exit:
   if (srcfile) acFree(srcfile);
   if (destfile) acFree(destfile);
   FreeResource(src);
   return error;
}

//****************************************************************************
// Generic routine for copying folders, intended to be used in conjunction with fs_copy()

ERROR fs_copydir(STRING Source, STRING Dest, FileFeedback *Feedback, FUNCTION *Callback, BYTE Move)
{
   parasol::Log log("copy_file");

   const virtual_drive *vsrc = get_fs(Source);
   const virtual_drive *vdest = get_fs(Dest);

   // This is a recursive copier for folders

   LONG srclen, destlen;
   for (srclen=0; Source[srclen]; srclen++);
   for (destlen=0; Dest[destlen]; destlen++);

   if ((Source[srclen-1] != '/') and (Source[srclen-1] != '\\') and (Source[srclen-1] != ':')) {
      Source[srclen++] = '/';
      Source[srclen] = 0;
   }

   if ((Dest[destlen-1] != '/') and (Dest[destlen-1] != '\\') and (Dest[destlen-1] != ':')) {
      Dest[destlen++] = '/';
      Dest[destlen] = 0;
   }

   DirInfo *dir;
   ERROR error;
   if (!(error = OpenDir(Source, RDF_FILE|RDF_FOLDER|RDF_PERMISSIONS, &dir))) {
      while (!(error = ScanDir(dir))) {
         FileInfo *file = dir->Info;
         if (file->Flags & RDF_LINK) {
            if ((vsrc->ReadLink) and (vdest->CreateLink)) {
               StrCopy(file->Name, Source+srclen, COPY_ALL);
               StrCopy(file->Name, Dest+destlen, COPY_ALL);

               if ((Callback) and (Callback->Type)) {
                  Feedback->Path = Source;
                  Feedback->Dest = Dest;
                  LONG result = CALL_FEEDBACK(Callback, Feedback);
                  if (result IS FFR_ABORT) { error = ERR_Cancelled; break; }
                  else if (result IS FFR_SKIP) continue;
               }

               STRING link;
               if (!(error = vsrc->ReadLink(Source, &link))) {
                  DeleteFile(Dest, NULL);
                  error = vdest->CreateLink(Dest, link);
               }
            }
            else {
               log.warning("Cannot copy linked file to destination.");
               error = ERR_NoSupport;
            }
         }
         else if (file->Flags & RDF_FILE) {
            StrCopy(file->Name, Source+srclen, COPY_ALL);
            StrCopy(file->Name, Dest+destlen, COPY_ALL);

            AdjustLogLevel(1);
               error = fs_copy(Source, Dest, Callback, FALSE);
            AdjustLogLevel(-1);
         }
         else if (file->Flags & RDF_FOLDER) {
            StrCopy(file->Name, Dest+destlen, COPY_ALL);

            if ((Callback) and (Callback->Type)) {
               Feedback->Path = Source;
               Feedback->Dest = Dest;
               LONG result = CALL_FEEDBACK(Callback, Feedback);
               if (result IS FFR_ABORT) { error = ERR_Cancelled; break; }
               else if (result IS FFR_SKIP) continue;
            }

            AdjustLogLevel(1);
               error = CreateFolder(Dest, (glDefaultPermissions) ? glDefaultPermissions : file->Permissions);
#ifdef __unix__
               if (vdest->VirtualID IS 0xffffffff) {
                  chown(Dest, (glForceUID != -1) ? glForceUID : file->UserID, (glForceGID != -1) ? glForceGID : file->GroupID);
               }
#endif
               if (error IS ERR_FileExists) error = ERR_Okay;
            AdjustLogLevel(-1);

            // Copy everything under the folder to the destination

            if (!error) {
               StrCopy(file->Name, Source+srclen, COPY_ALL);
               fs_copydir(Source, Dest, Feedback, Callback, Move);
            }
         }
      }

      FreeResource(dir);

      Source[srclen] = 0;
      Dest[destlen]  = 0;
      return error;
   }
   else if (error IS ERR_DirEmpty) return ERR_Okay;
   else {
      log.msg("Folder list failed for \"%s\"", Source);
      return error;
   }
}

//****************************************************************************
// Gets the permissions of the parent folder.  Typically used for permission inheritance. NB: It is often wise to
// remove exec and suid flags returned from this function.

LONG get_parent_permissions(CSTRING Path, LONG *UserID, LONG *GroupID)
{
   parasol::Log log(__FUNCTION__);
   char folder[512];
   LONG i;

   // Make a copy of the location

   folder[0] = 0;
   for (i=0; (Path[i]) and ((size_t)i < sizeof(folder)); i++) folder[i] = Path[i];
   if (i > 0) {
      i--;
      if ((folder[i] IS '/') or (folder[i] IS '\\') or (folder[i] IS ':')) i--;
   }

   while (i > 0) {
      while ((i > 0) and (folder[i] != '/') and (folder[i] != '\\') and (folder[i] != ':')) i--;
      folder[i+1] = 0;

      FileInfo info;
      if ((i > 0) and (!get_file_info(folder, &info, sizeof(info)))) {
         //log.msg("%s [$%.8x]", Path, info.Permissions);
         if (UserID) *UserID = info.UserID;
         if (GroupID) *GroupID = info.GroupID;
         return info.Permissions;
      }
      i--;
   }

   //log.msg("%s [FAIL]", Path);
   return 0;
}

//****************************************************************************
// Strips trailing slashes from folder locations.

BYTE strip_folder(STRING Path)
{
   LONG i;
   for (i=0; Path[i]; i++);
   if (i > 1) {
      if ((Path[i-1] IS '/') or (Path[i-1] IS '\\')) {
         Path[i-1] = 0;
         return TRUE;
      }
   }
   return FALSE;
}

//****************************************************************************

ERROR fs_readlink(STRING Source, STRING *Link)
{
#ifdef __unix__
   char buffer[512];
   LONG i;

   if ((i = readlink(Source, buffer, sizeof(buffer)-1)) != -1) {
      buffer[i] = 0;
      *Link = StrClone(buffer);
      return ERR_Okay;
   }
   else return ERR_Failed;
#else
   return ERR_NoSupport;
#endif
}

//****************************************************************************

ERROR fs_createlink(CSTRING Target, CSTRING Link)
{
#ifdef __unix__
   if (symlink(Link, Target) IS -1) {
      return convert_errno(errno, ERR_CreateFile);
   }
   else return ERR_Okay;
#else
   return ERR_NoSupport;
#endif
}

//****************************************************************************
// NB: The path that is received is already resolved.

ERROR fs_delete(STRING Path, FUNCTION *Callback)
{
   ERROR error;
   LONG len;

   for (len=0; Path[len]; len++);
   if ((Path[len-1] IS '/') or (Path[len-1] IS '\\')) Path[len-1] = 0;

   #ifdef _WIN32
      FileFeedback feedback;
      char buffer[MAX_FILENAME];

      StrCopy(Path, buffer, sizeof(buffer));

      if ((Callback) and (Callback->Type)) {
         ClearMemory(&feedback, sizeof(feedback));
         feedback.FeedbackID = FBK_DELETE_FILE;
         feedback.Path = buffer;
      }

      error = delete_tree(buffer, sizeof(buffer), Callback, &feedback);
   #else
      if (!unlink(Path)) { // unlink() works if the folder is empty
         error = ERR_Okay;
      }
      else if (errno IS EISDIR) {
         FileFeedback feedback;
         char buffer[MAX_FILENAME];

         StrCopy(Path, buffer, sizeof(buffer));

         if ((Callback) and (Callback->Type)) {
            ClearMemory(&feedback, sizeof(feedback));
            feedback.FeedbackID = FBK_DELETE_FILE;
            feedback.Path = buffer;
         }

         error = delete_tree(buffer, sizeof(buffer), Callback, &feedback);
      }
      else error = convert_errno(errno, ERR_Failed);
   #endif

   return error;
}

//****************************************************************************

ERROR fs_scandir(DirInfo *Dir)
{
#ifdef __unix__
   struct dirent *de;
   struct stat64 info, link;
   struct tm *local;
   LONG j;

   char pathbuf[256];
   LONG path_end = StrCopy(Dir->prvResolvedPath, pathbuf, sizeof(pathbuf));
   if ((size_t)path_end >= sizeof(pathbuf)-12) return(ERR_BufferOverflow);
   if (pathbuf[path_end-1] != '/') pathbuf[path_end++] = '/';

   while ((de = readdir((DIR *)Dir->prvHandle))) {
      if ((de->d_name[0] IS '.') and (de->d_name[1] IS 0)) continue;
      if ((de->d_name[0] IS '.') and (de->d_name[1] IS '.') and (de->d_name[2] IS 0)) continue;

      StrCopy(de->d_name, pathbuf + path_end, sizeof(pathbuf) - path_end);

      FileInfo *file = Dir->Info;
      if (!(stat64(pathbuf, &info))) {
         if (S_ISDIR(info.st_mode)) {
            if (!(Dir->prvFlags & RDF_FOLDER)) continue;
            file->Flags |= RDF_FOLDER;
         }
         else {
            if (!(Dir->prvFlags & RDF_FILE)) continue;
            file->Flags |= RDF_FILE|RDF_SIZE|RDF_DATE|RDF_PERMISSIONS;
         }
      }
      else if (!(lstat64(pathbuf, &info))) {
         if (!(Dir->prvFlags & RDF_FILE)) continue;
         file->Flags |= RDF_FILE|RDF_SIZE|RDF_DATE|RDF_PERMISSIONS;
      }
      else continue;

      if (lstat64(pathbuf, &link) != -1) {
         if (S_ISLNK(link.st_mode)) file->Flags |= RDF_LINK;
      }

      j = StrCopy(de->d_name, file->Name, MAX_FILENAME);

      if ((file->Flags & RDF_FOLDER) and (Dir->prvFlags & RDF_QUALIFY)) {
         file->Name[j++] = '/';
         file->Name[j] = 0;
      }

      if (file->Flags & RDF_FILE) file->Size = info.st_size;
      else file->Size = 0;

      if (Dir->prvFlags & RDF_PERMISSIONS) {
         if (info.st_mode & S_IRUSR) file->Permissions |= PERMIT_READ;
         if (info.st_mode & S_IWUSR) file->Permissions |= PERMIT_WRITE;
         if (info.st_mode & S_IXUSR) file->Permissions |= PERMIT_EXEC;
         if (info.st_mode & S_IRGRP) file->Permissions |= PERMIT_GROUP_READ;
         if (info.st_mode & S_IWGRP) file->Permissions |= PERMIT_GROUP_WRITE;
         if (info.st_mode & S_IXGRP) file->Permissions |= PERMIT_GROUP_EXEC;
         if (info.st_mode & S_IROTH) file->Permissions |= PERMIT_OTHERS_READ;
         if (info.st_mode & S_IWOTH) file->Permissions |= PERMIT_OTHERS_WRITE;
         if (info.st_mode & S_IXOTH) file->Permissions |= PERMIT_OTHERS_EXEC;
         if (info.st_mode & S_ISUID) file->Permissions |= PERMIT_USERID;
         if (info.st_mode & S_ISGID) file->Permissions |= PERMIT_GROUPID;
         file->UserID = info.st_uid;
         file->GroupID = info.st_gid;
      }

      if (Dir->prvFlags & RDF_DATE) {
         local = localtime(&info.st_mtime);
         file->Modified.Year   = 1900 + local->tm_year;
         file->Modified.Month  = local->tm_mon + 1;
         file->Modified.Day    = local->tm_mday;
         file->Modified.Hour   = local->tm_hour;
         file->Modified.Minute = local->tm_min;
         file->Modified.Second = local->tm_sec;

         local = localtime(&info.st_ctime);
         file->Created.Year   = 1900 + local->tm_year;
         file->Created.Month  = local->tm_mon + 1;
         file->Created.Day    = local->tm_mday;
         file->Created.Hour   = local->tm_hour;
         file->Created.Minute = local->tm_min;
         file->Created.Second = local->tm_sec;
      }
      return ERR_Okay;
   }

#elif _WIN32

   BYTE dir, hidden, readonly, archive;
   LONG i;

   while (winScan(&Dir->prvHandle, Dir->prvResolvedPath, Dir->Info->Name, &Dir->Info->Size, &Dir->Info->Created, &Dir->Info->Modified, &dir, &hidden, &readonly, &archive)) {
      if (hidden)   Dir->Info->Flags |= RDF_HIDDEN;
      if (readonly) Dir->Info->Flags |= RDF_READ_ONLY;
      if (archive)  Dir->Info->Flags |= RDF_ARCHIVE;

      if (dir) {
         if (!(Dir->prvFlags & RDF_FOLDER)) { Dir->Info->Name[0] = 0; continue; }
         Dir->Info->Flags |= RDF_FOLDER;

         if (Dir->prvFlags & RDF_QUALIFY) {
            i = StrLength(Dir->Info->Name);
            Dir->Info->Name[i++] = '/';
            Dir->Info->Name[i] = 0;
         }
      }
      else {
         if (!(Dir->prvFlags & RDF_FILE)) { Dir->Info->Name[0] = 0; continue; }
         Dir->Info->Flags |= RDF_FILE|RDF_SIZE|RDF_DATE;
      }

      return ERR_Okay;
   }

#else
   #error Platform requires support for ScanDir();
#endif

   return ERR_DirEmpty;
}

//****************************************************************************

ERROR fs_opendir(DirInfo *Info)
{
   parasol::Log log(__FUNCTION__);

   log.trace("Resolve '%.40s'/ '%.40s'", Info->prvPath, Info->prvResolvedPath);

#ifdef __unix__

   if ((Info->prvHandle = opendir(Info->prvResolvedPath))) {
      rewinddir((DIR *)Info->prvHandle);
      return ERR_Okay;
   }
   else return ERR_InvalidPath;

#elif _WIN32

   if (Info->prvResolveLen < MAX_FILENAME-1) {
      STRING str = Info->prvResolvedPath;
      str[Info->prvResolveLen-1] = '*'; // The -1 is because the length includes the null terminator.
      str[Info->prvResolveLen++] = 0;

      Info->prvHandle = (WINHANDLE)-1; // No handle is required for windows until ScanDir() is called.  TODO: See winScan() - we should probably call FindFirstFile() here to ensure that the folder exists and initialise the search.
      return ERR_Okay;
   }
   else return log.warning(ERR_BufferOverflow);

#else
   #error Platform requires support for OpenDir()
#endif
}

//****************************************************************************

ERROR fs_closedir(DirInfo *Dir)
{
   parasol::Log log(__FUNCTION__);

   log.trace("Dir: %p, VirtualID: %d", Dir, Dir->prvVirtualID);

   if ((!Dir->prvVirtualID) or (Dir->prvVirtualID IS DEFAULT_VIRTUALID)) {
      #ifdef __unix__
         if (Dir->prvHandle) closedir((DIR *)Dir->prvHandle);
      #elif _WIN32
         if ((Dir->prvHandle != (WINHANDLE)-1) and (Dir->prvHandle)) {
            winFindClose(Dir->prvHandle);
         }
      #else
         #error Platform requires support for CloseDir()
      #endif
   }

   if (Dir->Info) {
      if (Dir->prvFlags & RDF_OPENDIR) {
         // OpenDir() allocates Dir->Info as part of the Dir structure, so no need for a FreeResource(Dir->Info) here.

         if (Dir->Info->Tags) { FreeResource(Dir->Info->Tags); Dir->Info->Tags = NULL; }
      }
      else {
         FileInfo *list = Dir->Info;
         while (list) {
            FileInfo *next = list->Next;
            if (list->Tags) { FreeResource(list->Tags); list->Tags = NULL; }
            FreeResource(list);
            list = next;
         }
         Dir->Info = NULL;
      }
   }

   return ERR_Okay;
}

//****************************************************************************

ERROR fs_rename(STRING CurrentPath, STRING NewPath)
{
   return ERR_NoSupport;
}

//****************************************************************************

ERROR fs_testpath(CSTRING Path, LONG Flags, LONG *Type)
{
   STRING str;
   LONG len, type;

   len = StrLength(Path);

   if (Path[len-1] IS ':') {
      if (!ResolvePath(Path, 0, &str)) {
         if (*Type) *Type = LOC_VOLUME;
         FreeResource(str);
         return ERR_Okay;
      }
      else return ERR_DoesNotExist;
   }

   #ifdef __unix__

      struct stat64 info;
      type = 0;
      if (!stat64(Path, &info)) {
         if (S_ISDIR(info.st_mode)) type = LOC_DIRECTORY;
         else type = LOC_FILE;
      }
      else if (!lstat64(Path, &info)) type = LOC_FILE; // The file is a broken symbolic link

   #elif _WIN32

      type = winTestLocation(Path, (Flags & RSF_CASE_SENSITIVE) ? TRUE : FALSE);

   #endif

   if (type) {
      if (Type) *Type = type;
      return ERR_Okay;
   }
   else return ERR_DoesNotExist;
}

//****************************************************************************

ERROR fs_getinfo(CSTRING Path, struct FileInfo *Info, LONG InfoSize)
{
   parasol::Log log(__FUNCTION__);

#ifdef __unix__
   // In order to tell if a folder is a symbolic link or not, we have to remove any trailing slash...

   char path_ref[256];
   LONG len = StrCopy(Path, path_ref, sizeof(path_ref));
   if ((size_t)len >= sizeof(path_ref)-1) return ERR_BufferOverflow;
   if ((path_ref[len-1] IS '/') or (path_ref[len-1] IS '\\')) path_ref[len-1] = 0;

   // Get the file info.  Use lstat64() and if it turns out that the file is a symbolic link, set the RDF_LINK flag
   // and then switch to stat64().

   struct stat64 info;
   if (lstat64(path_ref, &info) IS -1) return ERR_FileNotFound;

   Info->Flags = 0;

   if (S_ISLNK(info.st_mode)) {
      Info->Flags |= RDF_LINK;
      if (stat64(path_ref, &info) IS -1) {
         // We do not abort in the case of a broken link, just warn and treat it as an empty file
         log.warning("Broken link detected.");
      }
   }

   if (S_ISDIR(info.st_mode)) Info->Flags |= RDF_FOLDER|RDF_TIME|RDF_PERMISSIONS;
   else Info->Flags |= RDF_FILE|RDF_SIZE|RDF_TIME|RDF_PERMISSIONS;

   // Extract file/folder name

   LONG i = len;
   while ((i > 0) and (path_ref[i-1] != '/') and (path_ref[i-1] != '\\') and (path_ref[i-1] != ':')) i--;
   i = StrCopy(path_ref + i, Info->Name, MAX_FILENAME-2);

   if (Info->Flags & RDF_FOLDER) {
      Info->Name[i++] = '/';
      Info->Name[i] = 0;
   }

   Info->Tags = NULL;
   Info->Size = info.st_size;

   // Set file security information

   Info->Permissions = 0;
   if (info.st_mode & S_IRUSR) Info->Permissions |= PERMIT_READ;
   if (info.st_mode & S_IWUSR) Info->Permissions |= PERMIT_WRITE;
   if (info.st_mode & S_IXUSR) Info->Permissions |= PERMIT_EXEC;
   if (info.st_mode & S_IRGRP) Info->Permissions |= PERMIT_GROUP_READ;
   if (info.st_mode & S_IWGRP) Info->Permissions |= PERMIT_GROUP_WRITE;
   if (info.st_mode & S_IXGRP) Info->Permissions |= PERMIT_GROUP_EXEC;
   if (info.st_mode & S_IROTH) Info->Permissions |= PERMIT_OTHERS_READ;
   if (info.st_mode & S_IWOTH) Info->Permissions |= PERMIT_OTHERS_WRITE;
   if (info.st_mode & S_IXOTH) Info->Permissions |= PERMIT_OTHERS_EXEC;
   if (info.st_mode & S_ISUID) Info->Permissions |= PERMIT_USERID;
   if (info.st_mode & S_ISGID) Info->Permissions |= PERMIT_GROUPID;

   Info->UserID = info.st_uid;
   Info->GroupID = info.st_gid;

   // Get time information.  NB: The timestamp is calculated by the filesystem's GetFileInfo() manager, using
   // calc_timestamp().

   struct tm *local;
   if ((local = localtime(&info.st_mtime))) {
      Info->Modified.Year   = 1900 + local->tm_year;
      Info->Modified.Month  = local->tm_mon + 1;
      Info->Modified.Day    = local->tm_mday;
      Info->Modified.Hour   = local->tm_hour;
      Info->Modified.Minute = local->tm_min;
      Info->Modified.Second = local->tm_sec;
   }

#else
   BYTE dir;
   LONG i, len;

   Info->Flags = 0;
   if (!winFileInfo(Path, &Info->Size, &Info->Modified, &dir)) return ERR_File;

   // TimeStamp has to match that produced by GET_TimeStamp

   struct stat64 stats;
   struct tm *local;

   if (!stat64(Path, &stats)) {
      if ((local = localtime(&stats.st_mtime))) {
         Info->Modified.Year   = 1900 + local->tm_year;
         Info->Modified.Month  = local->tm_mon + 1;
         Info->Modified.Day    = local->tm_mday;
         Info->Modified.Hour   = local->tm_hour;
         Info->Modified.Minute = local->tm_min;
         Info->Modified.Second = local->tm_sec;
      }
   }

   for (len=0; Path[len]; len++);

   if ((Path[len-1] IS '/') or (Path[len-1] IS '\\')) Info->Flags |= RDF_FOLDER|RDF_TIME;
   else if (dir) Info->Flags |= RDF_FOLDER|RDF_TIME;
   else Info->Flags |= RDF_FILE|RDF_SIZE|RDF_TIME;

   // Extract the file name

   i = len;
   if ((Path[i-1] IS '/') or (Path[i-1] IS '\\')) i--;

   while ((i > 0) and (Path[i-1] != '/') and (Path[i-1] != '\\') and (Path[i-1] != ':')) i--;

   i = StrCopy(Path + i, Info->Name, MAX_FILENAME-2);

   if (Info->Flags & RDF_FOLDER) {
      if (Info->Name[i-1] IS '\\') Info->Name[i-1] = '/';
      else if (Info->Name[i-1] != '/') {
         Info->Name[i++] = '/';
         Info->Name[i] = 0;
      }
   }

   Info->Permissions = NULL;
   Info->UserID      = 0;
   Info->GroupID     = 0;
   Info->Tags        = NULL;

#endif

   return ERR_Okay;
}

//****************************************************************************

ERROR fs_getdeviceinfo(CSTRING Path, objStorageDevice *Info)
{
   parasol::Log log("GetDeviceInfo");

   STRING location;
   ERROR error;

   // Device information is stored in the SystemVolumes object

   {
      parasol::ScopedObjectLock<objConfig> volumes(glVolumes, 8000);
      if (volumes.granted()) {
         ULONG pathend;
         STRING resolve = NULL;
         location = NULL;

restart:
         for (pathend=0; (Path[pathend]) and (Path[pathend] != ':'); pathend++);

         ConfigGroups *groups;
         if (!GetPointer(glVolumes, FID_Data, &groups)) {
            for (auto& [group, keys] : groups[0]) {
               if (not keys.contains("Name")) continue;
               auto& name = keys["Name"];

               bool match = false;
               ULONG j;
               for (j=0; (j < (ULONG)name.size()) and (j < pathend); j++) {
                  if (std::tolower(Path[j]) != std::tolower(name[j])) break;
               }
               if ((j IS pathend) and ((j IS (ULONG)name.size()) or (name[j] IS ':'))) match = true;

               if (!match) continue;

               if (keys.contains("Path")) {
                  if (!keys["Path"].compare(0, 6, "EXT:")) Info->DeviceFlags |= DEVICE_SOFTWARE; // Virtual device
               }

               if (keys.contains("Device")) {
                  auto& device = keys["Device"];
                  if (!device.compare("disk"))     Info->DeviceFlags |= DEVICE_FLOPPY_DISK|DEVICE_REMOVABLE|DEVICE_READ|DEVICE_WRITE;
                  else if (!device.compare("hd"))  Info->DeviceFlags |= DEVICE_HARD_DISK|DEVICE_READ|DEVICE_WRITE;
                  else if (!device.compare("cd"))  Info->DeviceFlags |= DEVICE_COMPACT_DISC|DEVICE_REMOVABLE|DEVICE_READ;
                  else if (!device.compare("usb")) Info->DeviceFlags |= DEVICE_USB|DEVICE_REMOVABLE;
                  else log.warning("Device '%s' unrecognised.", device.c_str());
               }
            }
         }

         if (!Info->DeviceFlags) {
            // Unable to find a device reference for the volume, so try to resolve the path and try again.

            if (resolve) {
               // We've done what we can - drop through

               #ifdef _WIN32
                // On win32 we can get the drive information from the drive letter
                #warning TODO: Write Win32 code to discover the drive type in GetDeviceInfo().
               #endif

               location = resolve;
               resolve = NULL;
            }
            else {
               if (ResolvePath(Path, RSF_NO_FILE_CHECK, &resolve) != ERR_Okay) {
                  if (resolve) FreeResource(resolve);
                  return ERR_ResolvePath;
               }

               Path = resolve;
               goto restart;
            }
         }

         if (resolve) FreeResource(resolve);
      }
      else return log.warning(ERR_AccessObject);
   }

   // Assume that the device is read/write if the device type cannot be assessed

   if (!Info->DeviceFlags) Info->DeviceFlags |= DEVICE_READ|DEVICE_WRITE;

   // Calculate the amount of available disk space

#ifdef _WIN32

   LARGE bytes_avail, total_size;

   if (!location) error = ResolvePath(Path, RSF_NO_FILE_CHECK, &location);
   else error = ERR_Okay;

   if (!error) {
      if (!(winGetFreeDiskSpace(location[0], &bytes_avail, &total_size))) {
         log.msg("Failed to read location \"%s\" (from \"%s\")", location, Path);
         Info->BytesFree  = -1;
         Info->BytesUsed  = 0;
         Info->DeviceSize = -1;
         FreeResource(location);
         return ERR_Okay; // Even though the disk space calculation failed, we succeeded on resolving other device information
      }
      else {
         Info->BytesFree  = bytes_avail;
         Info->BytesUsed  = total_size - bytes_avail;
         Info->DeviceSize = total_size;
         FreeResource(location);
         return ERR_Okay;
      }
   }
   else error = ERR_ResolvePath;

   if (location) FreeResource(location);
   return log.warning(error);

#elif __unix__

   if (Info->DeviceFlags & DEVICE_HARD_DISK) {
      if (!location) {
         error = ResolvePath(Path, RSF_NO_FILE_CHECK, &location);
      }
      else error = ERR_Okay;

      if (!error) {
         struct statfs fstat;
         LONG result = statfs(location, &fstat);
         FreeResource(location);

         if (result != -1) {
            DOUBLE blocksize = (DOUBLE)fstat.f_bsize;
            Info->BytesFree  = ((DOUBLE)fstat.f_bavail) * blocksize;
            Info->DeviceSize = ((DOUBLE)fstat.f_blocks) * blocksize;
            Info->BytesUsed  = Info->DeviceSize - Info->BytesFree;

            /* statvfs()
            if (fstat.f_flag & ST_RDONLY) {
               Info->DeviceFlags &= ~DEVICE_WRITE;
            }
            */

            // Floating point corrections

            if (Info->BytesFree < 1)  Info->BytesFree = 0;
            if (Info->BytesUsed < 1)  Info->BytesUsed = 0;
            if (Info->DeviceSize < 1) Info->DeviceSize = 0;
            return ERR_Okay;
         }
         else return log.warning(convert_errno(errno, ERR_File));
      }
      else return log.warning(ERR_ResolvePath);
   }
   else {
      Info->BytesFree  = -1;
      Info->DeviceSize = -1;
      Info->BytesUsed  = 0;
      return ERR_Okay;
   }
#endif

   return ERR_NoSupport;
}

//****************************************************************************

ERROR fs_makedir(CSTRING Path, LONG Permissions)
{
   parasol::Log log(__FUNCTION__);

#ifdef __unix__

   LONG err, i;
   LONG len = StrLength(Path);

   // The 'executable' bit must be set for folders in order to have any sort of access to their content.  So, if
   // the read or write flags are set, we automatically enable the executable bit for that folder.

   Permissions |= PERMIT_EXEC; // At a minimum, ensure the owner has folder access initially
   if (Permissions & PERMIT_GROUP) Permissions |= PERMIT_GROUP_EXEC;
   if (Permissions & PERMIT_OTHERS) Permissions |= PERMIT_OTHERS_EXEC;

   log.branch("%s, Permissions: $%.8x %s", Path, Permissions, (glDefaultPermissions) ? "(forced)" : "");

   LONG secureflags = convert_permissions(Permissions);

   if (mkdir(Path, secureflags) IS -1) {
      char buffer[len+1];

      if (errno IS EEXIST) {
         log.msg("A folder or file already exists at \"%s\"", Path);
         return ERR_FileExists;
      }

      // This loop will go through the complete path attempting to create multiple folders.

      for (i=0; Path[i]; i++) {
         buffer[i] = Path[i];
         if ((i > 0) and (buffer[i] IS '/')) {
            buffer[i+1] = 0;

            log.msg("%s", buffer);

            if (((err = mkdir(buffer, secureflags)) IS -1) and (errno != EEXIST)) break;

            if (!err) {
               if ((glForceUID != -1) or (glForceGID != -1)) chown(buffer, glForceUID, glForceGID);
               if (secureflags & (S_ISUID|S_ISGID)) chmod(buffer, secureflags);
            }
         }
      }

      if (Path[i]) {
         log.warning("Failed to create folder \"%s\".", Path);
         return ERR_Failed;
      }
      else if (Path[i-1] != '/') {
         // If the path did not end with a slash, there is still one last folder to create
         buffer[i] = 0;
         log.msg("%s", buffer);
         if (((err = mkdir(buffer, secureflags)) IS -1) and (errno != EEXIST)) {
            log.warning("Failed to create folder \"%s\".", Path);
            return convert_errno(errno, ERR_SystemCall);
         }
         if (!err) {
            if ((glForceUID != -1) or (glForceGID != -1)) chown(buffer, glForceUID, glForceGID);
            if (secureflags & (S_ISUID|S_ISGID)) chmod(buffer, secureflags);
         }
      }
   }
   else {
      if ((glForceUID != -1) or (glForceGID != -1)) chown(Path, glForceUID, glForceGID);
      if (secureflags & (S_ISUID|S_ISGID)) chmod(Path, secureflags);
   }

   return ERR_Okay;

#elif _WIN32

   LONG len = StrLength(Path);
   ERROR error;
   LONG i;
   if ((error = winCreateDir(Path))) {
      char buffer[len+1];

      if (error IS ERR_FileExists) return ERR_FileExists;

      // This loop will go through the complete path attempting to create multiple folders.

      log.trace("Creating multiple folders.");

      for (i=0; Path[i]; i++) {
         buffer[i] = Path[i];
         if ((i >= 3) and (buffer[i] IS '\\')) {
            buffer[i+1] = 0;
            log.trace("%s", buffer);
            winCreateDir(buffer);
         }
      }

      if (Path[i]) {
         log.traceWarning("Failed to create folder \"%s\".", Path);
         return ERR_Failed;
      }
   }

   return ERR_Okay;
#endif
}

//****************************************************************************

#ifdef __ANDROID__
// The Android release does not keep an associations.cfg file.
ERROR load_datatypes(void)
{
   parasol::Log log(__FUNCTION__);
   if (!glDatatypes) {
      if (CreateObject(ID_CONFIG, NF_UNTRACKED, (OBJECTPTR *)&glDatatypes, TAGEND) != ERR_Okay) {
         return log.warning(ERR_CreateObject);
      }
   }

   return ERR_Okay;
}
#else
ERROR load_datatypes(void)
{
   parasol::Log log(__FUNCTION__);
   FileInfo info;
   static LARGE user_ts = 0;
   bool reload;

   log.traceBranch("");

   if (!glDatatypes) {
      reload = true;

      if (!get_file_info("config:users/associations.cfg", &info, sizeof(info))) {
         user_ts = info.TimeStamp;
      }
      else {
         return log.warning(ERR_FileDoesNotExist);
      }
   }
   else {
      reload = false;
      if (!get_file_info("config:users/associations.cfg", &info, sizeof(info))) {
         if (user_ts != info.TimeStamp) {
            user_ts = info.TimeStamp;
            reload = true;
         }
      }
   }

   if (reload) {
      objConfig *datatypes;

      if (CreateObject(ID_CONFIG, NF_UNTRACKED, (OBJECTPTR *)&datatypes,
            FID_Path|TSTR, "config:users/associations.cfg",
            FID_Flags|TLONG, CNF_OPTIONAL_FILES,
            TAGEND) != ERR_Okay) {
         return log.warning(ERR_CreateObject);
      }

      if (glDatatypes) acFree(glDatatypes);
      glDatatypes = datatypes;
   }

   return ERR_Okay;
}
#endif

//****************************************************************************
// Private function for deleting files and folders recursively.

#ifdef __unix__

ERROR delete_tree(STRING Path, LONG Size, FUNCTION *Callback, FileFeedback *Feedback)
{
   parasol::Log log(__FUNCTION__);
   struct dirent *direntry;
   LONG len;
   DIR *dummydir, *stream;
   struct stat64 info;
   LONG result;
   ERROR error;

   log.trace("Path: %s", Path);

   if ((Callback) and (Callback->Type)) {
      Feedback->Path = Path;
      result = CALL_FEEDBACK(Callback, Feedback);
      if (result IS FFR_ABORT) {
         log.trace("Feedback requested abort at file '%s'", Path);
         return ERR_Cancelled;
      }
      else if (result IS FFR_SKIP) {
         log.trace("Feedback requested skip at file '%s'", Path);
         return ERR_Okay;
      }
   }

   // Check if the folder is actually a symbolic link (we don't want to recurse into them)

   if (lstat64(Path, &info) != -1) {
      if (S_ISLNK(info.st_mode)) {
         if (unlink(Path)) {
            log.error("unlink() failed on symbolic link '%s'", Path);
            return convert_errno(errno, ERR_SystemCall);
         }
         else return ERR_Okay;
      }
   }

   if ((stream = opendir(Path))) {
      for (len=0; Path[len]; len++);
      Path[len] = '/';

      error = ERR_Okay;
      rewinddir(stream);
      while ((direntry = readdir(stream))) {
         if ((direntry->d_name[0] IS '.') and (direntry->d_name[1] IS 0));
         else if ((direntry->d_name[0] IS '.') and (direntry->d_name[1] IS '.') and (direntry->d_name[2] IS 0));
         else {
            StrCopy(direntry->d_name, Path+len+1, Size-len-1);
            if ((dummydir = opendir(Path))) {
               closedir(dummydir);
               if (delete_tree(Path, Size, Callback, Feedback) IS ERR_Cancelled) {
                  error = ERR_Cancelled;
                  break;
               }
            }
            else {
               // Delete a file within the folder
               if (unlink(Path)) {
                  log.error("unlink() failed on '%s'", Path);
                  error = convert_errno(errno, ERR_SystemCall);
                  break;
               }
            }
         }
      }
      closedir(stream);

      Path[len] = 0;

      if ((!error) and (rmdir(Path))) {
         log.error("rmdir(%s) error: %s", Path, strerror(errno));
         return convert_errno(errno, ERR_SystemCall);
      }

      return error;
   }
   else {
      log.error("Failed to open folder \"%s\" using opendir().", Path);
      return convert_errno(errno, ERR_SystemCall);
   }
}

#endif

//****************************************************************************

#include "fs_identify.cpp"
#include "fs_resolution.cpp"
#include "fs_folders.cpp"
#include "fs_volumes.cpp"
#include "fs_watch_path.cpp"

