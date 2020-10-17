/*****************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

******************************************************************************

-CLASS-
Module: Manages the loading of system libraries.

The Module class is used to load and maintain the modules that are installed on the user's system.  A number of modules
are available in the core platform as standard, which you can use in the development of your programs.
Examples of existing modules can be found in both the "modules:" folder.

To load a module for the purpose of utilising its API functions, you will need to create a module object and initialise
it.  The following code segment illustrates:

<pre>
objModule *modDisplay;
struct DisplayBase *DisplayBase;

if (!CreateObject(ID_MODULE, 0, &amp;modDisplay,
      FID_Name|TSTR, "display",
      TAGEND)) {
   GetPointer(modDisplay, FID_ModBase, &amp;DisplayBase);
}
</pre>

Post-initialisation there is very little that you need to do with the object besides reading its function base from the
#ModBase field.  Keep in mind that you must not free the module object until you are finished with the
functions that it provides.

A list of officially recognised modules that export function tables can be found in the Module Index manual. If you
would like to learn more about modules in general, refer to the Module Interfaces manual.  If you would like to write a
new module, please read the Module Development Guide.
-END-

*****************************************************************************/

#ifdef __unix__
#include <dlfcn.h>
#endif

#include "../defs.h"
#include <parasol/main.h>

#ifdef _WIN32
#define DLLCALL // __declspec(dllimport)
#define WINAPI  __stdcall
#define HKEY_LOCAL_MACHINE 0x80000002
#define KEY_READ 0x20019
DLLCALL APTR WINAPI LoadLibraryA(STRING);
DLLCALL LONG WINAPI FreeLibrary(APTR);
DLLCALL APTR WINAPI GetProcAddress(APTR, STRING);
#endif

#ifdef DLL
APTR LoadLibraryA(STRING);
void * GetProcAddress(APTR, STRING);
int FreeLibrary(APTR);
#endif

static struct ModuleMaster glCoreMaster;
static struct ModHeader glCoreHeader;

static LONG cmp_mod_names(CSTRING, CSTRING);
static struct ModuleMaster * check_resident(objModule *, CSTRING);
static ERROR intercepted_master(struct ModuleMaster *, APTR);
static void free_module(MODHANDLE handle);

//****************************************************************************

static ERROR GET_Actions(objModule *, struct ActionEntry **);
static ERROR GET_IDL(objModule *, CSTRING *);
static ERROR GET_Name(objModule *, CSTRING *);

static ERROR SET_Header(objModule *, struct ModHeader *);
static ERROR SET_Name(objModule *, CSTRING);

static const struct FieldDef clFlags[] = {
   { "LinkLibrary", MOF_LINK_LIBRARY },
   { "Static",      MOF_STATIC },
   { NULL, 0 }
};

static const struct FieldArray glModuleFields[] = {
   { "Version",      FDF_DOUBLE|FDF_RI,  0, NULL, NULL },
   { "FunctionList", FDF_POINTER|FDF_RW, 0, NULL, NULL },
   { "ModBase",      FDF_POINTER|FDF_R,  0, NULL, NULL },
   { "Master",       FDF_POINTER|FDF_R,  0, NULL, NULL },
   { "Header",       FDF_POINTER|FDF_RI, 0, NULL, SET_Header },
   { "Flags",        FDF_LONG|FDF_RI,    (MAXINT)&clFlags, NULL, NULL },
   // Virtual fields
   { "Actions",      FDF_POINTER|FDF_R, 0, GET_Actions, NULL },
   { "Name",         FDF_STRING|FDF_RI, 0, GET_Name, SET_Name },
   { "IDL",          FDF_STRING|FDF_R, 0, GET_IDL, NULL },
   END_FIELD
};

static ERROR MODULE_GetVar(objModule *, struct acGetVar *);
static ERROR MODULE_Init(objModule *, APTR);
static ERROR MODULE_Free(objModule *, APTR);
static ERROR MODULE_SetVar(objModule *, struct acSetVar *);

static const struct ActionArray glModuleActions[] = {
   { AC_Free,   MODULE_Free },
   { AC_GetVar, MODULE_GetVar },
   { AC_Init,   MODULE_Init },
   { AC_SetVar, MODULE_SetVar },
   { 0, NULL }
};

//****************************************************************************

static ERROR MODULE_ResolveSymbol(objModule *, struct modResolveSymbol *);

static const struct FunctionField argsResolveSymbol[] = { { "Name", FD_STR }, { "Address", FD_PTR|FD_RESULT }, { NULL, 0 } };

static const struct MethodArray glModuleMethods[] = {
   { MT_ModResolveSymbol, MODULE_ResolveSymbol, "ResolveSymbol", argsResolveSymbol, sizeof(struct modResolveSymbol) },
   { 0, NULL, NULL, 0 }
};

//****************************************************************************

static ERROR GET_MMActions(struct ModuleMaster *, struct ActionEntry **);

static const struct FieldArray glModuleMasterFields[] = {
   // Virtual fields
   { "Actions", FDF_POINTER|FDF_R, 0, GET_MMActions, NULL },
   END_FIELD
};

static ERROR MODULEMASTER_Free(struct ModuleMaster *, APTR);

static const struct ActionArray glModuleMasterActions[] = {
   { AC_Free, MODULEMASTER_Free },
   // The following actions are program dependent
   { AC_ActionNotify,     intercepted_master },
   { AC_Clear,            intercepted_master },
   { AC_DataFeed,         intercepted_master },
   { AC_Deactivate,       intercepted_master },
   { AC_Draw,             intercepted_master },
   { AC_Flush,            intercepted_master },
   { AC_Focus,            intercepted_master },
   { AC_GetVar,           intercepted_master },
   { AC_Hide,             intercepted_master },
   { AC_Lock,             intercepted_master },
   { AC_LostFocus,        intercepted_master },
   { AC_Move,             intercepted_master },
   { AC_MoveToBack,       intercepted_master },
   { AC_MoveToFront,      intercepted_master },
   { AC_NewChild,         intercepted_master },
   { AC_NewOwner,         intercepted_master },
   { AC_Query,            intercepted_master },
   { AC_Read,             intercepted_master },
   { AC_Rename,           intercepted_master },
   { AC_Reset,            intercepted_master },
   { AC_Resize,           intercepted_master },
   { AC_SaveImage,        intercepted_master },
   { AC_SaveToObject,     intercepted_master },
   { AC_Scroll,           intercepted_master },
   { AC_Seek,             intercepted_master },
   { AC_SetVar,           intercepted_master },
   { AC_Show,             intercepted_master },
   { AC_Unlock,           intercepted_master },
   { AC_Write,            intercepted_master },
   { AC_Clipboard,        intercepted_master },
   { AC_Refresh,          intercepted_master },
   { AC_Disable,          intercepted_master },
   { AC_Enable,           intercepted_master },
   { AC_Redimension,      intercepted_master },
   { AC_MoveToPoint,      intercepted_master },
   { AC_ScrollToPoint,    intercepted_master },
   { AC_Custom,           intercepted_master },
   { 0, NULL }
};

//****************************************************************************

ERROR add_module_class(void)
{
   if (CreateObject(ID_METACLASS, 0, (OBJECTPTR *)&ModuleClass,
         FID_BaseClassID|TLONG,    ID_MODULE,
         FID_ClassVersion|TFLOAT,  VER_MODULE,
         FID_Name|TSTR,            "Module",
         FID_Category|TLONG,       CCF_SYSTEM,
         FID_FileExtension|TSTR,   "*.mod|*.so|*.dll",
         FID_FileDescription|TSTR, "System Module",
         FID_Actions|TPTR,         glModuleActions,
         FID_Methods|TARRAY,       glModuleMethods,
         FID_Fields|TARRAY,        glModuleFields,
         FID_Size|TLONG,           sizeof(objModule),
         FID_Path|TSTR,            "modules:core",
         TAGEND) != ERR_Okay) return ERR_AddClass;

   if (CreateObject(ID_METACLASS, 0, (OBJECTPTR *)&ModuleMasterClass,
         FID_BaseClassID|TLONG,   ID_MODULEMASTER,
         FID_ClassVersion|TFLOAT, 1.0,
         FID_Name|TSTR,           "ModuleMaster",
         FID_Category|TLONG,      CCF_SYSTEM,
         FID_Actions|TPTR,        glModuleMasterActions,
         FID_Fields|TARRAY,       glModuleMasterFields,
         FID_Size|TLONG,          sizeof(struct ModuleMaster),
         FID_Path|TSTR,           "modules:core",
         TAGEND) != ERR_Okay) return ERR_AddClass;

   return ERR_Okay;
}

//****************************************************************************
// Action interception routine.

static ERROR intercepted_master(struct ModuleMaster *Self, APTR Args)
{
   if (Self->prvActions[tlContext->Action].PerformAction) {
      return Self->prvActions[tlContext->Action].PerformAction((OBJECTPTR)Self, Args);
   }
   else return ERR_NoSupport;
}

//****************************************************************************

static ERROR GET_MMActions(struct ModuleMaster *Self, struct ActionEntry **Value)
{
   *Value = Self->prvActions;
   return ERR_Okay;
}

//****************************************************************************

ERROR MODULEMASTER_Free(struct ModuleMaster *Self, APTR Void)
{
   if (Self->Table) Self->Table->Master = NULL; // Remove the DLL's reference to the master.

   // Note that the order in which we perform the following actions is very important.

   if (Self->CoreBase) { FreeResource(Self->CoreBase); Self->CoreBase = NULL; }

   // Free the module's segment/code area

   if ((Self->NoUnload IS FALSE) AND (!(Self->Flags & MHF_STATIC))) {
      free_module(Self->LibraryBase);
      Self->LibraryBase = NULL;
   }

   // Patch the gap

   if (!thread_lock(TL_GENERIC, 200)) {
      if (Self->Prev) Self->Prev->Next = Self->Next;
      else glModuleList = Self->Next;
      if (Self->Next) Self->Next->Prev = Self->Prev;
      thread_unlock(TL_GENERIC);
   }

   return ERR_Okay;
}

//****************************************************************************
// This action sends a CLOSE command to the module, then frees the personally assigned module structure.  Note that the
// module code will be left resident in memory as it belongs to the ModuleMaster, not the Module.  See Expunge()
// in the Core for further details.

static ERROR MODULE_Free(objModule *Self, APTR Void)
{
   // Call the Module's Close procedure

   if (Self->Master) {
      if (Self->Master->OpenCount > 0) Self->Master->OpenCount--;
      if (Self->Master->Close)         Self->Master->Close((OBJECTPTR)Self);
      Self->Master = NULL;
   }

   if (Self->prvMBMemory) { FreeResource(Self->prvMBMemory); Self->prvMBMemory = NULL; }
   if (Self->Vars) { FreeResource(Self->Vars); Self->Vars = NULL; }
   return ERR_Okay;
}

/*****************************************************************************
-ACTION-
GetVar: Module parameters can be retrieved through this action.
-END-
*****************************************************************************/

static ERROR MODULE_GetVar(objModule *Self, struct acGetVar *Args)
{
   if ((!Args) OR (!Args->Buffer) OR (!Args->Field)) return PostError(ERR_NullArgs);
   if (Args->Size < 2) return PostError(ERR_Args);
   if (!Self->Vars) return ERR_UnsupportedField;

   CSTRING arg = VarGetString(Self->Vars, Args->Field);

   if (arg) {
      StrCopy(arg, Args->Buffer, Args->Size);
      return ERR_Okay;
   }
   else {
      Args->Buffer[0] = 0;
      return ERR_UnsupportedField;
   }
}

//****************************************************************************

static ERROR MODULE_Init(objModule *Self, APTR Void)
{
   #define AF_MODULEMASTER 0x0001
   #define AF_SEGMENT      0x0002
   OBJECTPTR Task = 0;
   ERROR error = ERR_Failed;
   LONG i, len;
   WORD ext, aflags = 0;
   char name[60];

   DEBUG_LINE

   if (!Self->Name[0]) return PostError(ERR_FieldNotSet);

   // Check if the module is resident.  If not, we need to load and prepare the module for a shared environment.

   APTR context = NULL;

   i = StrLength(Self->Name);
   while ((i > 0) AND (Self->Name[i-1] != ':') AND (Self->Name[i-1] != '/') AND (Self->Name[i-1] != '\\')) i--;
   StrCopy(Self->Name+i, name, sizeof(name));

   MSG("Finding module %s (%s)", Self->Name, name);

   struct ModuleMaster *master;
   struct ModHeader *table;
   if ((master = check_resident(Self, name))) {
      Self->Master = master;
   }
   else if (!NewPrivateObject(ID_MODULEMASTER, NF_NO_TRACK, (APTR)&master)) {
      char path[300];

      DEBUG_LINE

      if (!AccessObject(SystemTaskID, 5000, &Task)) {
         SetOwner((OBJECTPTR)master, (OBJECTPTR)Task);
         ReleaseObject(Task);
      }

      DEBUG_LINE

      master->Next = glModuleList; // Insert the ModuleMaster at the start of the chain.
      if (glModuleList) glModuleList->Prev = master;
      glModuleList = master;

      aflags |= AF_MODULEMASTER;

      context = SetContext((OBJECTPTR)master);

      StrCopy(name, master->LibraryName, sizeof(master->LibraryName));

      if (Self->Header) {
         // If the developer has specified a module header, then the module code is memory-resident and not to be
         // loaded from storage.
         table = Self->Header;
      }
      else {
         for (i=0; (Self->Name[i]) AND (Self->Name[i] != ':'); i++);
         path[0] = 0;

         if ((Self->Name[0] IS '/') OR (Self->Name[i] IS ':')) {
            MSG("Module location is absolute.");
            StrCopy(Self->Name, path, sizeof(path));

            STRING volume;
            if (!ResolvePath(path, RSF_APPROXIMATE, &volume)) {
               StrCopy(volume, path, sizeof(path));
               FreeResource(volume);
            }
            else {
               LogErrorMsg("Failed to resolve the path of module '%s'", Self->Name);
               error = ERR_ResolvePath;
               goto exit;
            }
         }

         // Scan the module database to find the location(s) of the module.  If the module is not registered, we will
         // resort to looking in the modules: folder.

         if (!path[0]) {
            struct ModuleItem *item;
            ULONG hashname = StrHash(name, FALSE);

            if ((item = find_module(hashname))) {
               StrCopy((CSTRING)(item + 1), path, sizeof(path));

               STRING volume;
               if (!ResolvePath(path, RSF_APPROXIMATE, &volume)) {
                  StrCopy(volume, path, sizeof(path));
                  FreeResource(volume);
               }
               else {
                  LogErrorMsg("Found registered module %s, but failed to resolve path '%s'", name, path);
                  error = ERR_ResolvePath;
                  goto exit;
               }
            }
            else LogMsg("Module '%s' #%.8x is not registered in the database.", name, hashname);
         }

         DEBUG_LINE

         if (!path[0]) {
            // If the file system module hasn't been loaded yet, we have to manually calculate the location of the
            // module.

            #ifdef __unix__
               if (glModulePath[0]) { // If no specific module path is defined, default to the system path and tack on the modules/ suffix.
                  i = StrCopy(glModulePath, path, sizeof(path));
               }
               else i = StrFormat(path, sizeof(path), "%slib/parasol/", glRootPath);

               if (Self->Flags & MOF_LINK_LIBRARY) i += StrCopy("lib/", path+i, sizeof(path-i));

               #ifdef __ANDROID__
                  if ((Self->Name[0] IS 'l') AND (Self->Name[1] IS 'i') AND (Self->Name[2] IS 'b'));
                  else for (j=0; "lib"[j]; j++) path[i++] = "lib"[j]; // Packaged Android modules have to begin with 'lib'
               #endif

               StrCopy(Self->Name, path+i, sizeof(path)-i);

            #elif _WIN32
               if (glModulePath[0]) {
                  i = StrCopy(glModulePath, path, sizeof(path)-32);
                  if (path[i-1] != '\\') path[i++] = '\\';
               }
               else if (glSystemPath[0]) {
                  i = StrCopy(glSystemPath, path, sizeof(path)-32);
                  if (path[i-1] != '\\') path[i++] = '\\';
                  i += StrCopy("lib\\", path+i, sizeof(path)-i);
               }
               else {
                  const char modlocation[] = "lib\\";
                  i = StrCopy(glRootPath, path, sizeof(path));
                  if (path[i-1] != '\\') path[i++] = '\\';
                  i += StrCopy(modlocation, path+i, sizeof(path)-i);
               }

               if (Self->Flags & MOF_LINK_LIBRARY) i += StrCopy("lib\\", path+i, sizeof(path-i));
               StrCopy(Self->Name, path+i, sizeof(path)-i);
            #endif
         }

         // Deal with the file extensions

         len = StrLength(path);
         ext = len;
         while ((ext > 0) AND (path[ext] != '.') AND (path[ext] != ':') AND (path[ext] != '\\') AND (path[ext] != '/')) ext--;

         if (path[ext] IS '.') {
            if (StrMatch(".dll", path+ext) != ERR_Okay) { len=ext; ext = -1; }
            else if (StrMatch(".so", path+ext) != ERR_Okay) { len=ext; ext = -1; }
         }
         else ext = -1;

         if (ext IS -1) {
            if (len < sizeof(path)-12) {
                ext = len;

                #ifdef __unix__
                   path[ext] = '.'; path[ext+1] = 's'; path[ext+2] = 'o'; path[ext+3] = 0;
                #elif _WIN32
                   path[ext] = '.'; path[ext+1] = 'd'; path[ext+2] = 'l'; path[ext+3] = 'l'; path[ext+4] = 0;
                #elif __APPLE__
                   // OSX uses .dylib but is compatible with .so
                   path[ext] = '.'; path[ext+1] = 's'; path[ext+2] = 'o'; path[ext+3] = 0;
                #else
                   #error What is the module extension for this machine type (.so/.mod/...)?
                #endif
            }
            else {
               error = ERR_BufferOverflow;
               goto exit;
            }
         }
         else ext = 0;

         MSG("Loading module \"%s\".", path);

         // Open the module file.  Note that we will dlclose() the module in the expunge sequence of the Core (see core.c).

         table = NULL;

         #ifdef __unix__

            // RTLD_LAZY needs to be used in case the module wants to have the ability to link to
            // symbolically linked libraries (e.g. the Network module does this to dynamically load
            // SSL support).
            //
            // RTLD_GLOBAL is needed only for symbolically linked libraries in case one is dependent on
            // other libraries.  SSL is an example of this as the libssl library is dependent
            // on symbols found in libcrypto, therefore libcrypto needs RTLD_GLOBAL.

            DEBUG_LINE

            if ((master->LibraryBase = dlopen(path, (Self->Flags & MOF_LINK_LIBRARY) ? (RTLD_LAZY|RTLD_GLOBAL) : RTLD_LAZY))) {
               aflags |= AF_SEGMENT;

               DEBUG_LINE

               if (!(Self->Flags & MOF_LINK_LIBRARY)) {
                  if (!(table = dlsym(master->LibraryBase, "ModHeader"))) {
                     LogErrorMsg("The 'ModHeader' structure is missing from module %s.", path);
                     goto exit;
                  }
               }
            }
            else {

               #ifdef DLL
                  if (ext) {
                     path[ext] = '.'; path[ext+1] = 'd'; path[ext+2] = 'l'; path[ext+3] = 'l'; path[ext+4] = 0;
                  }

                  MSG("Attempting to load module as a DLL.");
                  if ((master->LibraryBase = LoadLibraryA(path))) {

                     MSG("Identified module as a DLL.");
                     master->DLL = TRUE;
                     aflags |= AF_SEGMENT;
                     if (!(Self->Flags & MOF_LINK_LIBRARY)) {
                        if (!(table = GetProcAddress(master->LibraryBase, "ModHeader"))) {
                           if (!(table = GetProcAddress(master->LibraryBase, "_ModHeader"))) {
                              LogErrorMsg("The 'ModHeader' structure is missing from module %s.", path);
                              goto exit;
                           }
                        }
                     }

                     MSG("Retrieved ModHeader: $%.8x", table);
                  }
               #endif

               LogErrorMsg("%s: %s", name, (CSTRING)dlerror());
               error = ERR_NoSupport;
               goto exit;
            }

         #elif _WIN32

            if ((master->LibraryBase = winLoadLibrary(path))) {
               aflags |= AF_SEGMENT;

               if (!(Self->Flags & MOF_LINK_LIBRARY)) {
                  if (!(table = winGetProcAddress(master->LibraryBase, "ModHeader"))) {
                     if (!(table = winGetProcAddress(master->LibraryBase, "_ModHeader"))) {
                        LogErrorMsg("The 'ModHeader' structure is missing from module %s.", path);
                        goto exit;
                     }
                  }
               }
            }
            else {
               UBYTE msg[100];
               LogF("!", "Failed to load DLL '%s' (call: winLoadLibrary(): %s).", path, winFormatMessage(0, msg, sizeof(msg)));
               error = ERR_Read;
               goto exit;
            }

         #else
            #error This system needs support for the loading of module/exe files.
         #endif
      }

      DEBUG_LINE

      // The module version fields can give clues as to whether the table is corrupt or not.

      if (table) {
         if ((table->ModVersion > 500) OR (table->ModVersion < 0)) {
            LogErrorMsg("Corrupt module version number %d for module '%s'", (LONG)master->ModVersion, path);
            goto exit;
         }
         else if ((table->HeaderVersion < MODULE_HEADER_V1) OR (table->HeaderVersion > MODULE_HEADER_V1 + 256)) {
            LogErrorMsg("Invalid module header $%.8x", table->HeaderVersion);
            goto exit;
         }
      }

      master->OpenCount  = 0;
      master->Version    = 1;
      Self->Master = master;

      if (table) {
         // First, check if the module has already been loaded and is resident in a way that we haven't caught.
         // This shouldn't happen, but can occur for reasons such as the module being loaded from a path that differs
         // to the original. We resolve it by unloading the module and reverting to ModuleMaster referenced in the
         // Master field.

         if (table->HeaderVersion >= MODULE_HEADER_V2) {
            if (table->Master) {
               LogF("8","Module already loaded as #%d, reverting to original ModuleMaster object.", table->Master->Head.UniqueID);

               SetContext(context);
               context = NULL;

               free_module(master->LibraryBase);
               master->LibraryBase = NULL;
               acFree(&master->Head);

               Self->Master = table->Master;
               master = table->Master;
               goto open_module;
            }

            table->Master = master;
         }

         if (!table->Init) { PostError(ERR_ModuleMissingInit); goto exit; }
         if (!table->Name) { PostError(ERR_ModuleMissingName); goto exit; }

         master->Header = table;
         Self->FunctionList = table->DefaultList;
         Self->Version      = table->ModVersion;
         master->Table      = table;
         master->Name       = table->Name;
         master->ModVersion = table->ModVersion;
         master->Init       = table->Init;
         master->Open       = table->Open;
         master->Expunge    = table->Expunge;
         master->Flags      = table->Flags;

#ifdef DEBUG
         if (master->Name) { // Give the master object a nicer name for debug output.
            UBYTE mmname[30];
            mmname[0] = 'm';
            mmname[1] = 'm';
            mmname[2] = '_';
            for (i=0; i < sizeof(mmname)-4; i++) mmname[i+3] = master->Name[i];
            mmname[i+3] = 0;
            SetName((OBJECTPTR)master, mmname);
         }
#endif
      }

      // INIT

      DEBUG_LINE

      if (master->Init) {
         // Build a Core base for the module to use
         struct CoreBase *modkb;
         if ((modkb = build_jump_table(master->Table->Flags, glFunctions, NULL))) {
            master->CoreBase = modkb;
            fix_core_table(modkb, table->CoreVersion);

            FMSG("~","Initialising the module.");

            error = master->Init((OBJECTPTR)Self, modkb);

            if (error) {
               STEP();
               goto exit;
            }

            STEP();
         }
      }
      else if (Self->Flags & MOF_LINK_LIBRARY) {
         LogMsg("Loaded link library '%s'", Self->Name);
      }
      else {
         PostError(ERR_ModuleMissingInit);
         goto exit;
      }

      SetContext(context);
      context = NULL;
   }
   else {
      error = PostError(ERR_NewObject);
      goto exit;
   }

open_module:
   // If the STATIC option is set then the loaded module must not be removed when the Module object is freed.  This is
   // typically used for symbolic linked libraries.

   if (Self->Flags & MOF_STATIC) master->Flags |= MHF_STATIC;

   // At this stage the module is 100% resident and it is not possible to reverse the process.  Because of this, if an
   // error occurs we must not try to free any resident allocations from memory.

   aflags = aflags & ~(AF_MODULEMASTER|AF_SEGMENT);

   // OPEN

   if (master->Open) {
      MSG("Opening %s module.", Self->Name);
      if (master->Open((OBJECTPTR)Self) != ERR_Okay) {
         PostError(ERR_ModuleOpenFailed);
         goto exit;
      }
   }

   if (master->Table) master->Close = master->Table->Close;
   master->OpenCount++;

   DEBUG_LINE

   // Open() should have set the Self->FunctionList for us, but if it is null we will have to grab the default
   // function list.

   if (!Self->FunctionList) {
      if (master->Header) {
         Self->FunctionList = master->Header->DefaultList;
      }
      else if (!(Self->Flags & MOF_LINK_LIBRARY)) error = PostError(ERR_EntryMissingHeader);
   }

   // Build the jump table for the program

   if (Self->FunctionList) {
      if ((Self->ModBase = build_jump_table(MHF_STRUCTURE, Self->FunctionList, NULL)) IS NULL) {
         goto exit;
      }
      Self->prvMBMemory = Self->ModBase;
   }

   // Some DLL's like wsock2 can change the exception handler - we don't want that, so reset our exception handler
   // just in case.

   #ifdef _WIN32
      winSetUnhandledExceptionFilter(NULL);
   #endif

   MSG("Module has been successfully initialised.");
   error = ERR_Okay;

exit:
   DEBUG_LINE

   if (error) { // Free allocations if an error occurred

      if (!(error & ERF_Notified)) LogMsg("\"%s\" failed: %s", Self->Name, GetErrorMsg(error));
      error &= ~(ERF_Notified|ERF_Delay);

      if (aflags & AF_MODULEMASTER) {
         if (master->Expunge) {
            LogMsg("Expunging...");
            master->Expunge();
         }

         acFree(&master->Head);
         Self->Master = NULL;
      }
   }

   if (context) SetContext(context);
   return error;
}

/*****************************************************************************

-METHOD-
ResolveSymbol: Resolves the symbol names in loaded link libraries to address pointers.

This method will convert symbol names to their respective address pointers.  The module code must have been successfully
loaded into memory or an ERR_FieldNotSet error will be returned.  If the symbol was not found then ERR_NotFound is
returned.

-INPUT-
cstr Name: The name of the symbol to resolve.
&ptr Address: The address of the symbol will be returned in this parameter.

-ERRORS-
Okay
NullArgs
FieldNotSet: The module has not been successfully initialised.
NotFound: The symbol was not found.
NoSupport: The host platform does not support this method.

*****************************************************************************/

static ERROR MODULE_ResolveSymbol(objModule *Self, struct modResolveSymbol *Args)
{
   if ((!Args) OR (!Args->Name)) return PostError(ERR_NullArgs);

#ifdef _WIN32
   if ((!Self->Master) OR (!Self->Master->LibraryBase)) return ERR_FieldNotSet;

   if ((Args->Address = winGetProcAddress(Self->Master->LibraryBase, Args->Name))) {
      return ERR_Okay;
   }
   else {
      LogMsg("Failed to resolve '%s' in %s module.", Args->Name, Self->Master->Name);
      return ERR_NotFound;
   }
#elif __unix__
   if ((!Self->Master) OR (!Self->Master->LibraryBase)) return ERR_FieldNotSet;

   if ((Args->Address = dlsym(Self->Master->LibraryBase, Args->Name))) {
      return ERR_Okay;
   }
   else {
      LogMsg("Failed to resolve '%s' in %s module.", Args->Name, Self->Master->Name);
      return ERR_NotFound;
   }
#else
   #warning Platform not supported.
   return ERR_NoSupport;
#endif
}

/*****************************************************************************
-ACTION-
SetVar: Passes variable parameters to loaded modules.
-END-
*****************************************************************************/

static ERROR MODULE_SetVar(objModule *Self, struct acSetVar *Args)
{
   if ((!Args) OR (!Args->Field)) return ERR_NullArgs;
   if (!Args->Field[0]) return ERR_EmptyString;

   if (!Self->Vars) {
      if (!(Self->Vars = VarNew(0, 0))) return LogError(0, ERR_AllocMemory);
   }

   return VarSetString(Self->Vars, Args->Field, Args->Value);
}

/*****************************************************************************

-FIELD-
Actions: Used to gain direct access to a module's actions.

This field provides direct access to the actions of a module.  You can use it in the development of a class or function
based module so that your code can hook into the action system.  This allows you to create a module that blends in
seamlessly with the system's object oriented design.

The Actions field itself points to a list of action routines that are arranged into a lookup table, sorted by action ID.
You can hook into an action simply by writing to its index in the table with a pointer to the routine that you want to
use for that action.  For example:

<pre>
APTR *actions;

GetPointer(Module, FID_Actions, &actions);
actions[AC_ActionNotify] = MODULE_ActionNotify;
</pre>

The synopsis of the routines that you use for hooking into the action list must match `ERROR MODULE_ActionNotify(OBJECTPTR Module, APTR Args)`

It is recommended that you refer to the Action Support Guide before hooking into any action that you have not written
code for before.
-END-

*****************************************************************************/

static ERROR GET_Actions(objModule *Self, struct ActionEntry **Value)
{
   if (Self->Master) {
      *Value = Self->Master->prvActions;
      return ERR_Okay;
   }
   else return PostError(ERR_FieldNotSet);
}

/*****************************************************************************

-FIELD-
Flags: Optional flags.
Lookup: MOF

-FIELD-
IDL: Returns a compressed IDL string from the module, if available.

*****************************************************************************/

static ERROR GET_IDL(objModule *Self, CSTRING *Value)
{
   if ((Self->Master) AND (Self->Master->Header)) {
      *Value = Self->Master->Header->Definitions;
      MSG("No IDL for module %s", Self->Name);
      return ERR_Okay;
   }
   else return PostError(ERR_NotInitialised);
}

/*****************************************************************************
-FIELD-
FunctionList: Refers to a list of public functions exported by the module.

After initialisation, the FunctionList will refer to an array of public functions that are exported by the module.  The
FunctionList array consists of Function structs, which are in the following format:

<pre>
struct Function {
   APTR Address;  // Address of the function routine.
   CSTRING Name;  // Name of the function.
   const struct FunctionField *Args; // Descriptor array for the function arguments.
};
</pre>

-FIELD-
Header: For internal usage only.
Status: private

Setting the module Table prior to initialisation allows 'fake' modules to be created that reside in memory rather
than on disk.

*****************************************************************************/

static ERROR SET_Header(objModule *Self, struct ModHeader *Value)
{
   if (!Value) return ERR_Failed;
   Self->Header = Value;
   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Master: For internal use only.
Status: private

A loaded module can only reside once in memory, irrespective of how many module objects are used to represent it.  The
Master field refers to a ModuleMaster object that reflects this single instance of the module.

-FIELD-
ModBase: The Module's function base (jump table) must be read from this field.

Initialising a module will create a jump table that is referenced in the ModBase field.  The jump table contains
vectors that point to all functions that are published by the module.

The jump table is unique to the instance of the module. This allows each module to use a different function model
between versions, without losing backwards compatibility.  When a module is opened, it can check the requested
#Version and return a custom-built jump table to the program.  Thus if a function were changed in a
future module version, older programs would be re-routed to a routine that provides backwards compatibility to
the newer function model.

By default, jump tables are arranged as an array of function pointers accessible through a well defined structure.
The template for making calls is `FunctionBase-&gt;FunctionCall()`

Header files will normally include macros to simplify the function call:

<pre>#define FunctionCall      FunctionBase-&gt;FunctionCall</pre>

The jump table is invalid once the module is destroyed.

-FIELD-
Name: The name of the module.

This string pointer specifies the name of the Module.  This name will be used to load the module from the "modules:"
folder, so this field actually reflects part of the module file name.  It is also possible to specify
sub-directories before the module name itself - this could become more common in module loading in future.

It is critical that file extensions do not appear in the Name string, e.g. "screen.dll" as not all systems
may use a ".dll" extension.

*****************************************************************************/

static ERROR GET_Name(objModule *Self, CSTRING *Value)
{
   *Value = Self->Name;
   return ERR_Okay;
}

static ERROR SET_Name(objModule *Self, CSTRING Name)
{
   if (!Name) return ERR_Okay;

   WORD i;
   for (i=0; (Name[i]) AND (i < sizeof(Self->Name)-1); i++) {
      if ((Name[i] >= 'A') AND (Name[i] <= 'Z')) Self->Name[i] = Name[i] - 'A' + 'a';
      else Self->Name[i] = Name[i];
   }
   Self->Name[i] = 0;

   return ERR_Okay;
}

/*****************************************************************************

-FIELD-
Version: Minimum required version number.

When opening a module, the value that you insert in the Version field will reflect the minimum version and revision
number required of the module file.  If the module's version number is less than the version that you specify, then
the initialisation procedure will be aborted.

The Version field is also useful for ensuring that the function base returned by a module matches your program's
expectations.  For instance, if you write your program against a 1.0 version of a module but the user's machine has a
2.0 version installed, there could be incompatibilities.  By specifying the required version number, the module can
provide backwards-compatible functionality for your software.

After initialisation, the Version field will be updated to reflect the actual version of the Module.
-END-

*****************************************************************************/

//****************************************************************************
// Builds jump tables that link programs to modules.

APTR build_jump_table(LONG JumpType, const struct Function *FList, LONG MemFlags)
{
   void **functions;

   if ((!JumpType) OR (!FList)) LogErrorMsg("JumpTable() Invalid arguments.");

   if (JumpType & MHF_STRUCTURE) {
      LONG size = 0;
      LONG i;
      for (i=0; FList[i].Address; i++) size += sizeof(APTR);

      FMSG("JumpTable:","%d functions have been detected in the function list.",i);

      if (!AllocMemory(size + sizeof(APTR), MEM_NO_CLEAR|MemFlags, (APTR)&functions, NULL)) {
         for (i=0; FList[i].Address; i++) functions[i] = FList[i].Address;
         functions[i] = NULL;
         return functions;
      }
      else LogError(ERH_Module, ERR_AllocMemory);
   }

   return NULL;
}

//****************************************************************************
// This special routine will compare strings up to a '.' extension or null character.

static LONG cmp_mod_names(CSTRING String1, CSTRING String2)
{
   if ((String1) AND (String2)) {
      // Skip past any : or / folder characters

      WORD i = 0;
      while (String1[i]) {
         if ((String1[i] IS ':') OR (String1[i] IS '/')) {
            String1 += i + 1;  // Increment string's position
            i = 0;             // Reset the counter
         }
         else i++;
      }

      i = 0;
      while (String2[i] != 0) {
         if ((String2[i] IS ':') OR (String2[i] IS '/')) {
            String2 += i + 1;
            i = 0;
         }
         else i++;
      }

      // Loop until String1 reaches termination

      while ((*String1 != '.') AND (*String1 != 0)) {
         char ch1 = *String1;
         char ch2 = *String2;
         if ((ch2 IS '.') OR (ch2 IS 0)) return FALSE;
         if ((ch1 >= 'a') AND (ch1 <= 'z')) ch1 -= 0x20;
         if ((ch2 >= 'a') AND (ch2 <= 'z')) ch2 -= 0x20;
         if (ch1 != ch2) return FALSE;
         String1++; String2++;
      }

      // If we get this far then both strings match up to the end of String1, so now we need to check if String2 has
      // also terminated at the same point.

      if ((*String2 IS '.') OR (*String2 IS 0)) return TRUE;
   }

   return FALSE;
}

//****************************************************************************
// Searches the system for a ModuleMaster header that matches the Module details.  The module must have been
// loaded into memory in order for this function to return successfully.

static struct ModuleMaster * check_resident(objModule *Self, CSTRING ModuleName)
{
   struct ModuleMaster *master;
   static BYTE kminit = FALSE;

   if (!ModuleName) return NULL;

   FMSG("check_resident()","Module Name: %s", ModuleName);

   if (!StrMatch("core", ModuleName)) {
      LogMsg("Self-reference to the Core detected.");
      if (kminit IS FALSE) {
         kminit = TRUE;
         ClearMemory(&glCoreMaster, sizeof(glCoreMaster));
         ClearMemory(&glCoreHeader, sizeof(glCoreHeader));
         glCoreMaster.Name        = "Core";
         glCoreMaster.Version     = 1;
         glCoreMaster.OpenCount   = 1;
         glCoreMaster.ModVersion  = VER_CORE;
         glCoreMaster.Table       = &glCoreHeader;
         glCoreMaster.Header      = &glCoreHeader;
         glCoreHeader.DefaultList = glFunctions;
         glCoreHeader.Definitions = glIDL;

         Self->FunctionList = glFunctions;
      }
      return &glCoreMaster;
   }
   else if ((master = glModuleList)) {
      while (master) {
         if (cmp_mod_names(master->Name, ModuleName) IS TRUE) {
            MSG("Entry for module \"%s\" (\"%s\") found.", ModuleName, master->Name);
            return master;
         }
         master = master->Next;
      }
   }

   return NULL;
}

//****************************************************************************
// Search the module database (loaded from disk).

struct ModuleItem * find_module(ULONG Hash)
{
   if (glModules) {
      LONG *offsets = (LONG *)(glModules + 1);

      FMSG("find_module()","Scanning %d modules for %x", glModules->Total, Hash);

      LONG floor = 0;
      LONG ceiling = glModules->Total;
      while (floor < ceiling) {
         LONG i = (floor + ceiling)>>1;
         struct ModuleItem *item = (struct ModuleItem *)((APTR)glModules + offsets[i]);
         if (item->Hash < Hash) floor = i + 1;
         else if (item->Hash > Hash) ceiling = i;
         else return item;
      }
   }
   else FMSG("find_module()","glModules not defined.");

   return NULL;
}

//****************************************************************************

static void free_module(MODHANDLE handle)
{
   if (!handle) return;

   FMSG("free_module()","%p", handle);

   #ifdef __unix__
      #ifdef DLL
         if (Self->DLL) FreeLibrary(handle);
         else dlclose(handle);
      #else
         dlclose(handle);
      #endif
   #elif _WIN32
      winFreeLibrary(handle);
   #else
      #error You need to write machine specific code to expunge modules.
   #endif
}

