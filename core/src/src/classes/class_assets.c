/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
FileAssets: For Android systems only.  The FileAssets sub-class provides access to the assets folder in the currently running Android project.
-END-

*****************************************************************************/

//#define DEBUG

#define __system__
#define PRV_FILE
#define PRV_FILESYSTEM

#include <android/asset_manager_jni.h>
#include <android/asset_manager.h>
#include <unistd.h>

#define VER_FileAssets 1.0

static OBJECTPTR glAssetClass = NULL;

extern struct CoreBase *CoreBase;
static AAssetManager *glAssetManager = NULL;
static BYTE glAssetManagerFree = FALSE;

#define LEN_ASSETS 7 // "assets:" length

static ERROR ASSET_Delete(objFile *, APTR);
static ERROR ASSET_Free(objFile *, APTR);
static ERROR ASSET_Init(objFile *, APTR);
static ERROR ASSET_Move(objFile *, struct mtFileMove *);
static ERROR ASSET_Read(objFile *, struct acRead *);
static ERROR ASSET_Rename(objFile *, struct acRename *);
static ERROR ASSET_Seek(objFile *, struct acSeek *);
static ERROR ASSET_Write(objFile *, struct acWrite *);

static ERROR GET_Permissions(objFile *, APTR *);
static ERROR GET_Size(objFile *, LARGE *);

static ERROR SET_Permissions(objFile *, APTR);

static const struct FieldArray clFields[] = {
   { "Permissions", FDF_LONG|FDF_RW, 0, GET_Permissions, SET_Permissions },
   { "Size",        FDF_LARGE|FDF_R, 0, GET_Size,        NULL },
   END_FIELD
};

static const struct ActionArray clActions[] = {
   { AC_Free,   ASSET_Free },
   { AC_Init,   ASSET_Init },
   { AC_Move,   ASSET_Move },
   { AC_Read,   ASSET_Read },
   { AC_Rename, ASSET_Rename },
   { AC_Seek,   ASSET_Seek },
   { AC_Write,  ASSET_Write },
   { 0, NULL }
};

struct prvFileAsset {
   AAsset *Asset;
   AAssetDir *Dir;
   AAssetManager *Mgr;
};

static const struct MethodArray clMethods[] = {
   { MT_FileDelete, ASSET_Delete, "Delete", NULL, 0 },
   { MT_FileMove,   ASSET_Move, "Move", NULL, 0 },
   { 0, NULL, NULL, NULL, 0 }
};

static ERROR close_dir(struct DirInfo *);
static ERROR open_dir(struct DirInfo *);
static ERROR get_info(CSTRING Path, struct FileInfo *Info, LONG InfoSize);
static ERROR read_dir(CSTRING, struct DirInfo **, LONG);
static ERROR scan_dir(struct DirInfo *);
static ERROR test_path(CSTRING, LONG *);
static AAssetManager * get_asset_manager(void);

//****************************************************************************

ERROR add_asset_class(void)
{
   struct OpenInfo *openinfo;
   CSTRING classname;
   LONG i;

   LogF("~add_asset_class()","");

   if (!(openinfo = GetResourcePtr(RES_OPENINFO))) {
      LogErrorMsg("No OpenInfo structure set during Core initialisation.");
      LogBack();
      return ERR_Failed;
   }

   classname = NULL;
   if ((openinfo->Flags & OPF_OPTIONS) AND (openinfo->Options)) {
      for (i=0; openinfo->Options[i].Tag != TAGEND; i++) {
         switch (openinfo->Options[i].Tag) {
            case TOI_ANDROID_CLASS: {
               classname = openinfo->Options[i].Value.String;
               break;
            }

            case TOI_ANDROID_ASSETMGR: {
               glAssetManager = openinfo->Options[i].Value.Pointer;
               break;
            }
         }
      }
   }

   if (glAssetManager) {
      // Asset manager has been pre-allocated during JNI initialisation.
      glAssetManagerFree = FALSE;
   }
   else {

      JNIEnv *env = GetResourcePtr(RES_JNI_ENV);
      glAssetManagerFree = TRUE;

      if ((!env) OR (!classname)) {
         LogErrorMsg("Android env and class name must be defined when opening the Core.");
         LogBack();
         return ERR_Failed;
      }

      jclass glActivityClass = ((*env)->FindClass)(env, classname);
      if (glActivityClass) {
         jfieldID fidAssetManager = (*env)->GetStaticFieldID(env, glActivityClass, "assetManager", "Landroid/content/res/AssetManager;");
         if (fidAssetManager) {
            glAssetManager = (*env)->GetStaticObjectField(env, glActivityClass, fidAssetManager);
            if (glAssetManager) {
               glAssetManager = (*env)->NewGlobalRef(env, glAssetManager); // This call is required to prevent the Java GC from collecting the reference.
            }
            else { FMSG("@add_asset_class","Failed to get assetManager field."); LogBack(); return ERR_SystemCall; }
         }
         else { FMSG("@add_asset_class","Failed to get assetManager field ID."); LogBack(); return ERR_SystemCall; }
      }
      else { FMSG("@add_asset_class","Failed to get Java class %s", classname); LogBack(); return ERR_SystemCall; }
   }

   // Create the assets: control class

   if (CreateObject(ID_METACLASS, 0, &glAssetClass,
         FID_BaseClassID|TLONG, ID_FILE,
         FID_SubClassID|TLONG,  ID_FILEASSETS,
         FID_Name|TSTRING,      "FileAssets",
         FID_Actions|TPTR,      clActions,
         FID_Methods|TARRAY,    clMethods,
         FID_Fields|TARRAY,     clFields,
         FID_Path|TSTR,         "modules:filesystem",
         TAGEND) != ERR_Okay) {
      LogBack();
      return ERR_CreateObject;
   }

   // Create the 'assets' virtual volume

   VirtualVolume("assets", VAS_OPEN_DIR,  &open_dir,
                           VAS_SCAN_DIR,  &scan_dir,
                           VAS_CLOSE_DIR, &close_dir,
                           VAS_TEST_PATH, &test_path,
                           VAS_GET_INFO,  &get_info,
                           0);

   LogBack();
   return ERR_Okay;
}

//****************************************************************************

void free_asset_class(void)
{
   if ((glAssetManager) AND (glAssetManagerFree)) {
      JNIEnv *env = GetResourcePtr(RES_JNI_ENV);
      if (env) (*env)->DeleteGlobalRef(env, glAssetManager);
   }

   VirtualAssign("assets", VAS_DEREGISTER, TAGEND);

   if (glAssetClass) { acFree(glAssetClass); glAssetClass = NULL; }
}

//****************************************************************************

static ERROR ASSET_Delete(objFile *Self, APTR Void)
{
   return ERR_NoSupport; // Asset files cannot be deleted.
}

//****************************************************************************

static ERROR ASSET_Free(objFile *Self, APTR Void)
{
   return ERR_Okay;
}

//****************************************************************************

static ERROR ASSET_Init(objFile *Self, APTR Void)
{
   struct prvFileAsset *prv;

   if (!Self->Path) return ERR_FieldNotSet;

   MSG("Path: %s", Self->Path);

   if (StrCompare("assets:", Self->Path, LEN_ASSETS, 0) != ERR_Okay) return ERR_NoSupport;

   if (Self->Flags & (FL_NEW|FL_WRITE)) return PostError(ERR_ReadOnly);

   // Allocate private structure

   if (!AllocMemory(sizeof(struct prvFileAsset), Self->Head.MemFlags, &Self->Head.ChildPrivate, NULL)) {
      LONG len;
      for (len=0; Self->Path[len]; len++);

      if (Self->Path[len-1] IS ':') {
         return ERR_Okay;
      }
      else if (Self->Path[len-1] IS '/') {
         // Check that the folder exists.

         UBYTE dirpath[len];

         StrCopy(Self->Path+LEN_ASSETS, dirpath, COPY_ALL);

         MSG("Checking that path exists for '%s'", dirpath);

         AAssetDir *dir;
         if ((dir = AAssetManager_openDir(get_asset_manager(), dirpath))) {
            // Folder exists, close it and return OK
            AAssetDir_close(dir);
            return ERR_Okay;
         }
         else {
            FreeResource(Self->Head.ChildPrivate);
            Self->Head.ChildPrivate = NULL;
            return ERR_DoesNotExist;
         }
      }
      else {
         prv = Self->Head.ChildPrivate;

         // Check that the location exists / open the file.

         AAssetManager *mgr = get_asset_manager();
         if (mgr) {
            if ((prv->Asset = AAssetManager_open(mgr, Self->Path+LEN_ASSETS, AASSET_MODE_RANDOM))) { // AASSET_MODE_UNKNOWN, AASSET_MODE_RANDOM
               return ERR_Okay;
            }
            else {
               LogF("@FileAssets", "Failed to open asset file \"%s\"", Self->Path+LEN_ASSETS);
            }
         }

         FreeResource(Self->Head.ChildPrivate);
         Self->Head.ChildPrivate = NULL;
         return ERR_Failed;
      }
   }
   else return PostError(ERR_AllocMemory);
}

//****************************************************************************

static ERROR ASSET_Move(objFile *Self, struct mtFileMove *Args)
{
   return ERR_NoSupport; // Assets cannot be moved
}

//****************************************************************************

static ERROR ASSET_Read(objFile *Self, struct acRead *Args)
{
   struct prvFileAsset *prv;
   if (!(prv = Self->Head.ChildPrivate)) return PostError(ERR_ObjectCorrupt);
   if (!(Self->Flags & FL_READ)) return PostError(ERR_FileReadFlag);

   Args->Result = AAsset_read(prv->Asset, Args->Buffer, Args->Length);

   if (Args->Result != Args->Length) {
      if (Args->Result IS -1) {
         LogMsg("Failed to read %d bytes from the file.", Args->Length);
         Args->Result = 0;
         return ERR_Failed;
      }

      // Return ERR_Okay even though not all data was read, because this was not due to a failure.

      LogF("5Read()","%d of the intended %d bytes were read from the file.", Args->Result, Args->Length);
      Self->Position += Args->Result;
      return ERR_Okay;
   }
   else {
      Self->Position += Args->Result;
      return ERR_Okay;
   }
}

//****************************************************************************

static ERROR ASSET_Rename(objFile *Self, struct acRename *Args)
{
   return ERR_NoSupport; // Assets cannot be renamed.
}

//****************************************************************************

static ERROR ASSET_Seek(objFile *Self, struct acSeek *Args)
{
   struct prvFileAsset *prv;
   LONG method;

   if (!(prv = Self->Head.ChildPrivate)) return PostError(ERR_ObjectCorrupt);

   if (Args->Position IS POS_START) method = SEEK_SET;
   else if (Args->Position IS POS_END) method = SEEK_END;
   else if (Args->Position IS POS_CURRENT) method = SEEK_CUR;
   else return PostError(ERR_Args);

   off_t offset = AAsset_seek(prv->Asset, Args->Offset, method);
   if (offset != -1) Self->Position = offset;
   else return ERR_Failed;

   return ERR_Okay;
}

//****************************************************************************
// Assets are read-only.  If writing to an asset is required, the developer should copy the file to the cache or other
// storage area and modify it there.

static ERROR ASSET_Write(objFile *Self, struct acWrite *Args)
{
   return ERR_NoSupport; // Writing to assets is disallowed
}

//****************************************************************************

static ERROR GET_Permissions(objFile *Self, APTR *Value)
{
   *Value = NULL;
   return ERR_Okay;
}

static ERROR SET_Permissions(objFile *Self, APTR Value)
{
   return ERR_Okay;
}

//****************************************************************************

static ERROR GET_Size(objFile *Self, LARGE *Value)
{
   struct prvFileAsset *prv;

   if (!(prv = Self->Head.ChildPrivate)) return PostError(ERR_ObjectCorrupt);

   if (prv->Asset) {
      *Value = AAsset_getLength(prv->Asset);
      if (*Value >= 0) return ERR_Okay;
      else return ERR_Failed;
   }
   else return ERR_Failed; // Either the file is a folder or hasn't been opened.
}

//****************************************************************************
// Open the assets: volume for scanning.

static ERROR open_dir(struct DirInfo *Dir)
{
   AAssetManager *mgr;
   LONG len;

   FMSG("open_dir()","%s", Dir->prvResolvedPath);

   if (!(mgr = get_asset_manager())) {
      return PostError(ERR_SystemCall);
   }

   // openDir() doesn't like trailing slashes, this code will handle such circumstances.

   for (len=0; Dir->prvResolvedPath[len]; len++);
   if (Dir->prvResolvedPath[len-1] != '/') Dir->prvHandle = AAssetManager_openDir(mgr, Dir->prvResolvedPath+LEN_ASSETS);
   else {
      char path[len];
      len = len - LEN_ASSETS - 1;
      CopyMemory(Dir->prvResolvedPath+LEN_ASSETS, path, len);
      path[len] = 0;
      Dir->prvHandle = AAssetManager_openDir(mgr, path);
   }

   if (Dir->prvHandle) {
      return ERR_Okay;
   }
   else return ERR_InvalidPath;
}

//****************************************************************************
// Scan the next entry in the folder.

static ERROR scan_dir(struct DirInfo *Dir)
{
   CSTRING filename;
   AAssetManager *mgr;

   FMSG("scan_dir()","Asset file scan on %s", Dir->prvResolvedPath);

   if (!(mgr = get_asset_manager())) {
      return PostError(ERR_SystemCall);
   }

   while ((filename = AAssetDir_getNextFileName(Dir->prvHandle))) {
      if (Dir->prvFlags & RDF_FILE) {
         AAsset *asset;
         if ((asset = AAssetManager_open(mgr, Dir->prvResolvedPath+LEN_ASSETS, AASSET_MODE_UNKNOWN))) {
            Dir->Info->Flags |= RDF_FILE;
            if (Dir->prvFlags & RDF_SIZE) Dir->Info->Size = AAsset_getLength(asset);
            AAsset_close(asset);
            StrCopy(filename, Dir->Info->Name, MAX_FILENAME);

            Dir->prvIndex++;
            Dir->prvTotal++;
            return ERR_Okay;
         }
      }

      if (Dir->prvFlags & RDF_FOLDER) {
         AAssetDir *dir;
         if ((dir = AAssetManager_openDir(mgr, Dir->prvResolvedPath+LEN_ASSETS))) {
            Dir->Info->Flags |= RDF_FOLDER;
            AAssetDir_close(dir);

            StrCopy(filename, Dir->Info->Name, MAX_FILENAME);

            Dir->prvIndex++;
            Dir->prvTotal++;
            return ERR_Okay;
         }
      }
   }

   return ERR_DirEmpty;
}

//****************************************************************************
// Close the assets: volume.

static ERROR close_dir(struct DirInfo *Dir)
{
   // Note: FreeResource() will take care of memory dealloactions, we only need to be concerned with deallocation of any
   // open handles.

   if (Dir->prvHandle) {
      AAssetDir_close(Dir->prvHandle);
      Dir->prvHandle = NULL;
   }

   return ERR_Okay;
}

//****************************************************************************

static ERROR get_info(CSTRING Path, struct FileInfo *Info, LONG InfoSize)
{
   BYTE dir;
   LONG i, len;

   // We need to open the file in order to retrieve its size.

   AAssetManager *mgr = get_asset_manager();
   dir = FALSE;
   if (mgr) {
      AAsset *asset;
      AAssetDir *assetdir;
      if (!StrCompare("assets:", Path, LEN_ASSETS, 0)) { // Just a sanity check - the Path is always meant to be resolved.
         if ((asset = AAssetManager_open(mgr, Path+LEN_ASSETS, AASSET_MODE_UNKNOWN))) {
            Info->Size = AAsset_getLength(asset);
            AAsset_close(asset);
         }
         else if ((assetdir = AAssetManager_openDir(mgr, Path+LEN_ASSETS))) {
            if (AAssetDir_getNextFileName(assetdir)) dir = TRUE;
            AAssetDir_close(assetdir);
         }
      }
      else return ERR_NoSupport;
   }
   else return ERR_SystemCall;

   Info->Flags = 0;
   Info->Time.Year   = 2013;
   Info->Time.Month  = 1;
   Info->Time.Day    = 1;
   Info->Time.Hour   = 0;
   Info->Time.Minute = 0;
   Info->Time.Second = 0;

   for (len=0; Path[len]; len++);

   if ((Path[len-1] IS '/') OR (Path[len-1] IS '\\')) Info->Flags |= RDF_FOLDER;
   else if (dir) Info->Flags |= RDF_FOLDER;
   else Info->Flags |= RDF_FILE|RDF_SIZE;

   // Extract the file name

   i = len;
   if ((Path[i-1] IS '/') OR (Path[i-1] IS '\\')) i--;
   while ((i > 0) AND (Path[i-1] != '/') AND (Path[i-1] != '\\') AND (Path[i-1] != ':')) i--;
   i = StrCopy(Path + i, Info->Name, MAX_FILENAME-2);

   if (Info->Flags & RDF_FOLDER) {
      if (Info->Name[i-1] IS '\\') Info->Name[i-1] = '/';
      else if (Info->Name[i-1] != '/') {
         Info->Name[i++] = '/';
         Info->Name[i] = 0;
      }
   }

   Info->Permissions = 0;
   Info->UserID      = 0;
   Info->GroupID     = 0;
   Info->Tags        = NULL;
   return ERR_Okay;
}

//****************************************************************************
// Test an assets: location.

static ERROR test_path(CSTRING Path, LONG Flags, LONG *Type)
{
   AAssetManager *mgr;
   AAsset *asset;
   AAssetDir *dir;
   LONG len;

   FMSG("test_path()","%s", Path);

   if (!(mgr = get_asset_manager())) {
      return ERR_SystemCall;
   }

   for (len=0; Path[len]; len++);  // Check if the reference is explicitly defined as a folder.
   if (Path[len-1] != '/') {
      if ((asset = AAssetManager_open(mgr, Path+LEN_ASSETS, AASSET_MODE_UNKNOWN))) {
         FMSG("test_path","Path identified as a file.");
         *Type = LOC_FILE;
         AAsset_close(asset);
         return ERR_Okay;
      }

      dir = AAssetManager_openDir(mgr, Path+LEN_ASSETS);
   }
   else {
      // openDir() doesn't like trailing slashes, so we'll have to remove it.
      char path[len];
      len = len - LEN_ASSETS - 1;
      CopyMemory(Path+LEN_ASSETS, path, len);
      path[len] = 0;

      dir = AAssetManager_openDir(mgr, path);
   }

   // Testing a folder for its existance requires that it contains at least one file.
   // This is because openDir() has been observed as succeeding even when the path doesn't exist.

   if (dir) {
      if (AAssetDir_getNextFileName(dir)) {
         FMSG("test_path","Path identified as a folder.");
         *Type = LOC_DIRECTORY;
         AAssetDir_close(dir);
         return ERR_Okay;
      }
      else AAssetDir_close(dir);
   }

   FMSG("test_path","Path '%s' does not exist.", Path + LEN_ASSETS);
   return ERR_DoesNotExist;
}

//****************************************************************************
// Read the entire folder in one function call.

#if 0
static ERROR read_dir(CSTRING Path, struct DirInfo **Result, LONG Flags)
{
   struct DirInfo *dirinfo;
   AAssetDir *dir;
   AAssetManager *mgr;
   LONG len;

   FMSG("~read_dir()","Path: %s, Flags: $%.8x", Path, Flags);

   if (!(mgr = get_asset_manager())) {
      STEP();
      return PostError(ERR_SystemCall);
   }

   // openDir() doesn't like trailing slashes, this code will handle such circumstances.

   for (len=0; Path[len]; len++);
   if (Path[len-1] != '/') dir = AAssetManager_openDir(mgr, Path+LEN_ASSETS);
   else {
      char path[len];
      len = len - LEN_ASSETS - 1;
      CopyMemory(Path+LEN_ASSETS, path, len);
      path[len] = 0;
      dir = AAssetManager_openDir(mgr, path);
   }

   if (!dir) {
      STEP();
      return ERR_InvalidPath;
   }

   if (AllocMemory(sizeof(struct DirInfo), MEM_DATA, &dirinfo, NULL)) {
      AAssetDir_close(dir);
      STEP();
      return ERR_AllocMemory;
   }

   const char *filename;
   struct FileInfo *entry, *current;
   UBYTE assetpath[300];
   LONG i;

   // Read folder structure

   current = NULL;
   dirinfo->Total = 0;
   ERROR error = ERR_Okay;
   LONG insert = StrCopy(Path+LEN_ASSETS, assetpath, sizeof(assetpath)-2);
   if (assetpath[insert-1] != '/') assetpath[insert++] = '/';
   while ((filename = AAssetDir_getNextFileName(dir)) AND (!error)) {
      entry = NULL;

      StrCopy(filename, assetpath+insert, sizeof(assetpath)-insert-1);
      if (insert >= sizeof(assetpath)-1) {
         error = ERR_BufferOverflow;
         break;
      }

      AAsset *asset;
      if ((asset = AAssetManager_open(mgr, assetpath, AASSET_MODE_UNKNOWN))) {
         if (Flags & RDF_FILE) {
            LONG size = sizeof(struct FileInfo) + StrLength(filename) + 2;
            if (!AllocMemory(size, MEM_DATA, &entry, NULL)) {
               entry->Flags = RDF_FILE;

               if (Flags & RDF_PERMISSIONS) {
                  entry->Flags |= RDF_PERMISSIONS;
                  entry->Permissions = PERMIT_READ|PERMIT_GROUP_READ|PERMIT_OTHERS_READ;
               }

               if (Flags & RDF_SIZE) {
                  entry->Flags |= RDF_SIZE;
                  entry->Size = AAsset_getLength(asset);
               }

               if (Flags & RDF_DATE) {
                  entry->Time.Year = 2013;
                  entry->Time.Month = 1;
                  entry->Time.Day = 1;
               }

               entry->Name = (STRING)(entry + 1);
               StrCopy(filename, entry->Name, COPY_ALL);

               dirinfo->Total++;
            }
            else error = ERR_AllocMemory;
         }
         AAsset_close(asset);
      }
      else {
         if (Flags & RDF_FOLDER) {
            LONG size = sizeof(struct FileInfo) + StrLength(filename) + 2;
            if (!AllocMemory(size, MEM_DATA, &entry, NULL)) {
               entry->Flags = RDF_FOLDER;

               if (Flags & RDF_PERMISSIONS) {
                  entry->Flags |= RDF_PERMISSIONS;
                  entry->Permissions = PERMIT_READ|PERMIT_GROUP_READ|PERMIT_OTHERS_READ;
               }

               entry->Name = (STRING)(entry + 1);
               i = StrCopy(filename, entry->Name, COPY_ALL);
               if (Flags & RDF_QUALIFY) { entry->Name[i++] = '/'; entry->Name[i++] = 0; }

               dirinfo->Total++;
            }
            else error = ERR_AllocMemory;
         }
      }

      // Insert entry into the linked list

      if (entry) {
         if (!dirinfo->Info) dirinfo->Info = entry;
         if (current) current->Next = entry;
         current = entry;
      }
   }

   AAssetDir_close(dir);

   FMSG("read_dir","Found %d files, error code %d", dirinfo->Total, error);

   if (error) {
      // Remove all allocations.

      struct FileInfo *list = dirinfo->Info;
      while (list) {
         struct FileInfo *next = list->Next;
         FreeResource(list);
         list = next;
      }

      if (Result) *Result = NULL;
      FreeResource(dirinfo);
      STEP();
      return error;
   }
   else {
      if (Result) *Result = dirinfo;
      STEP();
      return ERR_Okay;
   }
}
#endif

//****************************************************************************

static AAssetManager * get_asset_manager(void)
{
   FMSG("get_asset_manager()","Native Access: %d", glAssetManagerFree);

   if (glAssetManagerFree) {
      return AAssetManager_fromJava(GetResourcePtr(RES_JNI_ENV), glAssetManager);
   }
   else return glAssetManager;
}
