/*********************************************************************************************************************
-CATEGORY-
Name: Files
-END-
*********************************************************************************************************************/

/*********************************************************************************************************************

-FUNCTION-
ResolvePath: Converts volume-based paths into absolute paths applicable to the host platform.

This function will convert a file path to its resolved form, according to the host system.  For example, a Linux
system might resolve `drive1:documents/readme.txt` to `/documents/readme.txt`.  A Windows system might
resolve the path to `c:\documents\readme.txt`.

The resulting path is guaranteed to be absolute, meaning the use of sequences such as `..`, `//` and `./` will be
eliminated.

If the path can be resolved to more than one file, the ResolvePath() method will attempt to guess the correct path by
checking the validity of each possible location.  For instance, if resolving a path of `user:document.txt`
and the `user:` volume refers to both `system:users/joebloggs/` and `system:users/default/`, the routine will check
both directories for the existence of the `document.txt` file to determine the correct location.  While helpful, this
can cause problems if the intent is to create a new file.  To circumvent this feature, use the `RSF_NO_FILE_CHECK`
setting in the Flags parameter.

When checking for the location of a file, ResolvePath() will only accept an exact file name match.  If the path must be
treated as an approximation (i.e. file extensions can be ignored) then use the `RSF_APPROXIMATE` flag to tell the
function to ignore extensions for the purpose of file name matching.

To resolve the location of executable programs on Unix systems, use the `RSF_PATH` flag.  This uses the PATH environment
variable to resolve the file name specified in the Path parameter.

The resolved path will be returned in the Result parameter as an allocated memory block.  It must be removed once it is
no longer required with FreeResource().

<types lookup="RSF"/>

If the path resolves to a virtual drive, it may not be possible to confirm whether the target file exists if the
virtual driver does not support this check.  This is common when working with network drives.

-INPUT-
cstr Path: The path to be resolved.
int(RSF) Flags: Optional flags.
!str Result: Must point to an empty STRING variable so that the resolved path can be stored.  If NULL, ResolvePath() will work as normal and return a valid error code without the result string.

-ERRORS-
Okay:            The path was resolved.
NullArgs:        Invalid arguments were specified.
AllocMemory:     The result string could not be allocated.
LockFailed:
Search:          The given volume does not exist.
FileNotFound:    The path was resolved, but the referenced file or folder does not exist (use RSF_NO_FILE_CHECK if you need to avoid this error code).
Loop:            The volume refers back to itself.

-END-

*********************************************************************************************************************/

static ERROR resolve(STRING, STRING, LONG);
static ERROR resolve_path_env(CSTRING RelativePath, STRING *Result);
static THREADVAR bool tlClassLoaded;

ERROR ResolvePath(CSTRING Path, LONG Flags, STRING *Result)
{
   parasol::Log log(__FUNCTION__);
   LONG i, loop;
   char src[MAX_FILENAME];
   char dest[MAX_FILENAME];

   log.traceBranch("%s, Flags: $%.8x", Path, Flags);

   if (!Path) return log.warning(ERR_NullArgs);

   if (Result) *Result = NULL;

   tlClassLoaded = false;

   if (Path[0] IS '~') {
      Flags |= RSF_APPROXIMATE;
      Path++;
   }

   if (!StrCompare("string:", Path, 7, 0)) {
      if ((*Result = StrClone(Path))) return ERR_Okay;
      else return log.warning(ERR_AllocMemory);
   }

   // Check if the Path argument contains a volume character.  If it does not, make a clone of the string and return it.

   bool resolved = FALSE;
#ifdef _WIN32
   if ((std::tolower(Path[0]) >= 'a') and (std::tolower(Path[0]) <= 'z') and (Path[1] IS ':')) {
      resolved = TRUE; // Windows drive letter reference discovered
      if ((Path[2] != '/') and (Path[2] != '\\')) {
         // Ensure that the path is correctly formed in order to pass test_path()
         src[0] = Path[0];
         src[1] = ':';
         src[2] = '\\';
         StrCopy(Path+2, src+3, sizeof(src)-3);
         Path = src;
      }
   }
   else if ((Path[0] IS '/') and (Path[1] IS '/')) resolved = TRUE; // UNC path discovered
   else if ((Path[0] IS '\\') and (Path[1] IS '\\')) resolved = TRUE; // UNC path discovered

#elif __unix__
   if ((Path[0] IS '/') or (Path[0] IS '\\')) resolved = TRUE;
#endif

   // Use the PATH environment variable to resolve the filename.  This can only be done if the path is relative
   // (ideally with no leading folder references).

   if ((!resolved) and (Flags & RSF_PATH)) {
      if (!resolve_path_env(Path, Result)) return ERR_Okay;
   }

   if (!resolved) {
      for (i=0; (Path[i]) and (Path[i] != ':') and (Path[i] != '/') and (Path[i] != '\\'); i++);
      if (Path[i] != ':') resolved = TRUE;
   }

   if (resolved) {
      if (Flags & RSF_APPROXIMATE) {
         StrCopy(Path, dest, sizeof(dest));
         if (!test_path(dest, RSF_APPROXIMATE)) Path = dest;
         else return ERR_FileNotFound;
      }
      else if (!(Flags & RSF_NO_FILE_CHECK)) {
         StrCopy(Path, dest, sizeof(dest));
         if (!test_path(dest, 0)) Path = dest;
         else return ERR_FileNotFound;
      }

      if (!Result) return ERR_Okay;
      else if ((*Result = cleaned_path(Path))) return ERR_Okay;
      else if ((*Result = StrClone(Path))) return ERR_Okay;
      else return log.warning(ERR_AllocMemory);
   }

   StrCopy(Path, src, sizeof(src)); // Copy the Path argument to our internal buffer

   // Keep looping until the volume is resolved

   dest[0] = 0;
   ThreadLock lock(TL_VOLUMES, 4000); // resolve() will be using glVolumes
   if (lock.granted()) {
      ERROR error = ERR_Failed;
      for (loop=10; loop > 0; loop--) {
         error = resolve(src, dest, Flags);

         if (error IS ERR_VirtualVolume) {
            log.trace("Detected virtual volume '%s'", dest);

            // If RSF_CHECK_VIRTUAL is set, return ERR_VirtualVolume for reserved volume names.

            if (!(Flags & RSF_CHECK_VIRTUAL)) error = ERR_Okay;

            if (Result) {
               if (Flags & RSF_APPROXIMATE) { // Ensure that the resolved path is accurate
                  if (test_path(dest, RSF_APPROXIMATE)) error = ERR_FileNotFound;
               }

               if (!(*Result = StrClone(dest))) error = ERR_AllocMemory;
            }

            break;
         }
         else if (error) break;
         else {
            #ifdef _WIN32 // UNC network path check
               if (((dest[0] IS '\\') and (dest[1] IS '\\')) or ((dest[0] IS '/') and (dest[1] IS '/'))) {
                  goto resolved_path;
               }
            #endif

            // Check if the path has been resolved by looking for a ':' character

            for (i=0; (dest[i]) and (dest[i] != ':') and (dest[i] != '/') and (dest[i] != '\\'); i++);

            #ifdef _WIN32
            if ((dest[i] IS ':') and (i > 1)) {
            #else
            if (dest[i] IS ':') {
            #endif
               // Copy the destination to the source buffer and repeat the resolution process.

               if (Flags & RSF_NO_DEEP_SCAN) return ERR_Failed;
               StrCopy(dest, src, sizeof(src));
               continue; // Keep resolving
            }
         }

#ifdef _WIN32
resolved_path:
#endif
         if (Result) {
            if (!(*Result = cleaned_path(dest))) {
               if (!(*Result = StrClone(dest))) error = ERR_AllocMemory;
            }
         }

         break;
      } // for()

      if (loop > 0) { // Note that loop starts at 10 and decrements to zero
         if ((!error) and (!dest[0])) error = ERR_Failed;
         return error;
      }
      else return ERR_Loop;
   }
   else return log.warning(ERR_LockFailed);
}

//********************************************************************************************************************
// For resolving file references via the host environment's PATH variable.  This will only work for relative paths.

#ifdef __unix__

static ERROR resolve_path_env(CSTRING RelativePath, STRING *Result)
{
   parasol::Log log("ResolvePath");
   struct stat64 info;
   CSTRING path;
   char src[512];

   // If a path to the file isn't available, scan the PATH environment variable. In Unix the separator is :

   if ((path = getenv("PATH")) and (path[0])) {
      LONG j = 0, k;
      while (path[j]) {
         for (k=0; (path[j+k]) and (path[j+k] != ':') and ((size_t)k < sizeof(src)-1); k++) {
            src[k] = path[j+k];
         }
         if ((k > 0) and (src[k-1] != '/')) src[k++] = '/';

         StrCopy(RelativePath, src+k, sizeof(src)-k);

         if (!stat64(src, &info)) {
            if (!S_ISDIR(info.st_mode)) { // Successfully identified file location
               if (Result) *Result = cleaned_path(src);
               return ERR_Okay;
            }
         }

         while (path[j+k] IS ':') k++; // Go to the next path in the list
         j += k;
      }
   }
   else log.trace("Failed to read PATH environment variable.");

   return ERR_NothingDone;
}

#elif _WIN32

static ERROR resolve_path_env(CSTRING RelativePath, STRING *Result)
{
   parasol::Log log("ResolvePath");
   struct stat64 info;
   CSTRING path;
   char src[512];

   // If a path to the file isn't available, scan the PATH environment variable. In Windows the separator is ;

   if ((path = getenv("PATH")) and (path[0])) {
      log.trace("Got PATH: %s", path);

      LONG j = 0, k;
      while (path[j]) {
         for (k=0; (path[j+k]) and (path[j+k] != ';') and ((size_t)k < sizeof(src)-1); k++) {
            src[k] = path[j+k];
         }
         j += k;
         if ((k > 0) and (src[k-1] != '/')) src[k++] = '/';

         StrCopy(RelativePath, src+k, sizeof(src)-k);

         if (!stat64(src, &info)) {
            if (!S_ISDIR(info.st_mode)) { // Successfully identified file location
               if (Result) *Result = cleaned_path(src);
               return ERR_Okay;
            }
         }

         while (path[j] IS ';') j++; // Go to the next path in the list
      }
   }
   else log.trace("Failed to read PATH environment variable.");

   return ERR_NothingDone;
}

#endif

/*********************************************************************************************************************
** Note: This function calls itself recursively.  For use by ResolvePath() only.
**
** Config  - The SystemVolumes configuration object.
** Source  - The file string that we are trying to resolve.
** Dest    - Buffer area; the resolved location will be stored here.
** Flags   - Optional RSF flags.
*/

static ERROR resolve_object_path(STRING, STRING, STRING, LONG);

static ERROR resolve(STRING Source, STRING Dest, LONG Flags)
{
   parasol::Log log("ResolvePath");
   char fullpath[MAX_FILENAME];
   char buffer[MAX_FILENAME];
   LONG j, k, pos, loop;
   ERROR error;

   if (get_virtual(Source)) {
      StrCopy(Source, Dest, COPY_ALL);
      return ERR_VirtualVolume;
   }

   for (pos=0; Source[pos] != ':'; pos++) {
      if (!Source[pos]) return log.warning(ERR_InvalidData);
   }
   pos++;

   Source[pos-1] = 0; // Remove the volume symbol for the string comparison

   if (glVolumes.contains(Source)) {
      StrCopy(glVolumes[Source]["Path"], fullpath, sizeof(fullpath));
   }
   else fullpath[0] = 0;

   if (!fullpath[0]) {
      log.msg("No matching volume for \"%s\".", Source);
      Source[pos-1] = ':'; // Put back the volume symbol
      return ERR_Search;
   }

   Source[pos-1] = ':'; // Restore the volume symbol

   // Handle the ":ObjectName" case

   if (fullpath[0] IS ':') return resolve_object_path(fullpath+1, Source, Dest, sizeof(fullpath)-1);

   log.traceBranch("%s, Resolved Path: %s, Flags: $%.8x", Source, fullpath, Flags);

   STRING path = fullpath;

   // Check if the EXT: reference is used.  If so, respond by loading the module or class that handles the volume.
   // The loaded code should replace the volume with the correct information for discovery on the next resolution phase.

   if (!StrCompare("EXT:", path, 4, STR_MATCH_CASE)) {
      StrCopy(Source, Dest, MAX_FILENAME); // Return an exact duplicate of the original source string

      if (get_virtual(Source)) {
         return ERR_VirtualVolume;
      }

      if (tlClassLoaded) { // Already attempted to load the module on a previous occasion - we must fail
         return ERR_Failed;
      }

      // An external reference can refer to a module for auto-loading (preferred) or a class name.

      objModule::create mod = { fl::Name(path + 4) };
      if (!mod.ok()) FindClass(ResolveClassName(path + 4));

      tlClassLoaded = true; // This setting will prevent recursion
      return ERR_VirtualVolume;
   }

   while (*path) {
      // Copy the resolved volume path to the destination buffer

      for (k=0; (*path) and (*path != '|') and (k < MAX_FILENAME-1);) {
         if (k > 0) {
            if ((*path IS '\\') and (path[1] IS '\\')) path++; // Eliminate dual slashes - with an exception for UNC paths
            else if ((*path IS '/') and (path[1] IS '/')) path++;
            else Dest[k++] = *path++;
         }
         else Dest[k++] = *path++;
      }

      if ((Dest[k-1] != '/') and (Dest[k-1] != '\\') and (k < MAX_FILENAME-1)) Dest[k++] = '/'; // Add a trailing slash if it is missing

      // Copy the rest of the source to the destination buffer

      j = pos;
      while ((Source[j] IS '/') or (Source[j] IS '\\')) j++;
      while ((Source[j]) and (k < MAX_FILENAME-1)) Dest[k++] = Source[j++];
      Dest[k++] = 0;

      // Fully resolve the path to a system folder before testing it (e.g. "scripts:" to "parasol:scripts/" to "c:\parasol\scripts\" will be resolved through this recursion).

      #ifdef _WIN32
         if ((Dest[1] IS ':') and ((Dest[2] IS '/') or (Dest[2] IS '\\'))) j = 0;
         else if ((Dest[0] IS '/') and (Dest[1] IS '/')) j = 0;
         else if ((Dest[0] IS '\\') and (Dest[1] IS '\\')) j = 0;
         else for (j=0; (Dest[j]) and (Dest[j] != ':') and (Dest[j] != '/'); j++);
      #else
         for (j=0; (Dest[j]) and (Dest[j] != ':') and (Dest[j] != '/'); j++);
      #endif

      error = -1;
      for (loop=10; loop > 0; loop--) {
         if ((Dest[j] IS ':') and (j > 1)) { // Remaining ':' indicates more path resolution is required.
            error = resolve(Dest, buffer, Flags);

            if (!error) {
               // Copy the result from buffer to Dest.
               for (j=0; buffer[j]; j++) Dest[j] = buffer[j];
               Dest[j] = 0;

               // Reexamine the result for the presence of a colon.
               for (LONG j=0; (Dest[j]) and (Dest[j] != ':') and (Dest[j] != '/'); j++);
            }
            else break; // Path not resolved or virtual volume detected.
         }
         else break;
      }

      if (loop <= 0) {
         log.warning("Infinite loop on path '%s'", Dest);
         return ERR_Loop;
      }

      if (!error) return ERR_Okay;

      // Return now if no file checking is to be performed

      if (Flags & RSF_NO_FILE_CHECK) {
         log.trace("No file check will be performed.");
         return ERR_Okay;
      }

      if (!test_path(Dest, Flags)) {
         log.trace("File found, path resolved successfully.");
         return ERR_Okay;
      }

      log.trace("File does not exist at %s", Dest);

      if (Flags & RSF_NO_DEEP_SCAN) {
         log.trace("No deep scanning - additional paths will not be checked.");
         break;
      }

      if (*path) path++;
   }

   log.trace("Resolved path but no matching file for %s\"%s\".", (Flags & RSF_APPROXIMATE) ? "~" : "", Source);
   return ERR_FileNotFound;
}

//********************************************************************************************************************
// For cases such as ":SystemIcons", we find the referenced object and ask it to resolve the path for us.  (In effect,
// the object will be used as a plugin for volume resolution).
//
// If the path is merely ":" or resolve_virtual() returns ERR_VirtualVolume, return the VirtualVolume error code to
// indicate that no further resolution is required.

static ERROR resolve_object_path(STRING Path, STRING Source, STRING Dest, LONG PathSize)
{
   parasol::Log log("ResolvePath");
   ERROR (*resolve_virtual)(OBJECTPTR, STRING, STRING, LONG);
   ERROR error = ERR_VirtualVolume;

   if (Path[0]) {
      OBJECTID volume_id;
      if (!FindObject(Path, 0, 0, &volume_id)) {
         OBJECTPTR object;
         if (!AccessObjectID(volume_id, 5000, &object)) {
            if ((!object->getPtr(FID_ResolvePath, &resolve_virtual)) and (resolve_virtual)) {
               error = resolve_virtual(object, Source, Dest, PathSize);
            }
            ReleaseObject(object);
         }
      }
   }

   if (error IS ERR_VirtualVolume) { // Return an exact duplicate of the original source string
      StrCopy(Source, Dest, COPY_ALL);
      return ERR_VirtualVolume;
   }
   else if (error != ERR_Okay) return log.warning(error);
   else return ERR_Okay;
}
