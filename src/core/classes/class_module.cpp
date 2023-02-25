/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the
terms described in the LICENSE.TXT file that is distributed with this package.
Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
Module: Manages the loading of system libraries.

The Module class is used to load and maintain the modules that are installed on the user's system.  A number of modules
are available in the core platform as standard, which you can use in the development of your programs.
Examples of existing modules can be found in both the `modules:` folder.

To load a module and interact with its API, create a module object and initialise it.  The following code segment
illustrates in C++:

<pre>
DisplayBase *DisplayBase;
auto modDisplay = objModule::create::global(fl::Name("display"));
if (modDisplay) modDisplay->getPtr(FID_ModBase, &amp;DisplayBase);
</pre>

To do the same in Fluid:

<pre>
mGfx = mod.load('display')
</pre>

It is critical that the module object is permanently retained until the program no longer needs its functionality.
-END-

**********************************************************************************************************************/

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

static ModuleMaster glCoreMaster;
static ModHeader glCoreHeader;

static LONG cmp_mod_names(CSTRING, CSTRING);
static ModuleMaster * check_resident(extModule *, CSTRING);
static void free_module(MODHANDLE handle);

//********************************************************************************************************************

static ERROR GET_IDL(extModule *, CSTRING *);
static ERROR GET_Name(extModule *, CSTRING *);

static ERROR SET_Header(extModule *, ModHeader *);
static ERROR SET_Name(extModule *, CSTRING);

static const FieldDef clFlags[] = {
   { "LinkLibrary", MOF_LINK_LIBRARY },
   { "Static",      MOF_STATIC },
   { NULL, 0 }
};

static const FieldArray glModuleFields[] = {
   { "Version",      FDF_DOUBLE|FDF_RI,  0, NULL, NULL },
   { "FunctionList", FDF_POINTER|FDF_RW, 0, NULL, NULL },
   { "ModBase",      FDF_POINTER|FDF_R,  0, NULL, NULL },
   { "Master",       FDF_POINTER|FDF_R,  0, NULL, NULL },
   { "Header",       FDF_POINTER|FDF_RI, 0, NULL, (APTR)SET_Header },
   { "Flags",        FDF_LONG|FDF_RI,    (MAXINT)&clFlags, NULL, NULL },
   // Virtual fields
   { "Name",         FDF_STRING|FDF_RI, 0, (APTR)GET_Name, (APTR)SET_Name },
   { "IDL",          FDF_STRING|FDF_R,  0, (APTR)GET_IDL, NULL },
   END_FIELD
};

static ERROR MODULE_Init(extModule *, APTR);
static ERROR MODULE_Free(extModule *, APTR);

static const ActionArray glModuleActions[] = {
   { AC_Free,   (APTR)MODULE_Free },
   { AC_Init,   (APTR)MODULE_Init },
   { 0, NULL }
};

//********************************************************************************************************************

static ERROR MODULE_ResolveSymbol(extModule *, struct modResolveSymbol *);

static const FunctionField argsResolveSymbol[] = { { "Name", FD_STR }, { "Address", FD_PTR|FD_RESULT }, { NULL, 0 } };

static const MethodArray glModuleMethods[] = {
   { MT_ModResolveSymbol, (APTR)MODULE_ResolveSymbol, "ResolveSymbol", argsResolveSymbol, sizeof(struct modResolveSymbol) },
   { 0, NULL, NULL, 0 }
};

//********************************************************************************************************************

static const FieldArray glModuleMasterFields[] = {
   END_FIELD
};

static ERROR MODULEMASTER_Free(ModuleMaster *, APTR);

static const ActionArray glModuleMasterActions[] = {
   { AC_Free, (APTR)MODULEMASTER_Free },
   { 0, NULL }
};

//********************************************************************************************************************

extern "C" ERROR add_module_class(void)
{
   if (!(glModuleClass = extMetaClass::create::global(
      fl::BaseClassID(ID_MODULE),
      fl::ClassVersion(VER_MODULE),
      fl::Name("Module"),
      fl::Category(CCF_SYSTEM),
      fl::FileExtension("*.mod|*.so|*.dll"),
      fl::FileDescription("System Module"),
      fl::Actions(glModuleActions),
      fl::Methods(glModuleMethods),
      fl::Fields(glModuleFields),
      fl::Size(sizeof(extModule)),
      fl::Path("modules:core")))) return ERR_AddClass;

   if (!(glModuleMasterClass = extMetaClass::create::global(
      fl::BaseClassID(ID_MODULEMASTER),
      fl::ClassVersion(1.0),
      fl::Name("ModuleMaster"),
      fl::Flags(CLF_NO_OWNERSHIP),
      fl::Category(CCF_SYSTEM),
      fl::Actions(glModuleMasterActions),
      fl::Fields(glModuleMasterFields),
      fl::Size(sizeof(ModuleMaster)),
      fl::Path("modules:core")))) return ERR_AddClass;

   return ERR_Okay;
}

//********************************************************************************************************************

ERROR MODULEMASTER_Free(ModuleMaster *Self, APTR Void)
{
   if (Self->Table) Self->Table->Master = NULL; // Remove the DLL's reference to the master.

   // Note that the order in which we perform the following actions is very important.

   if (Self->CoreBase) { FreeResource(Self->CoreBase); Self->CoreBase = NULL; }

   // Free the module's segment/code area

   if ((Self->NoUnload IS FALSE) and (!(Self->Flags & MHF_STATIC))) {
      free_module(Self->LibraryBase);
      Self->LibraryBase = NULL;
   }

   ThreadLock lock(TL_GENERIC, 200);
   if (lock.granted()) { // Patch the gap
      if (Self->Prev) Self->Prev->Next = Self->Next;
      else glModuleList = Self->Next;

      if (Self->Next) Self->Next->Prev = Self->Prev;
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// This action sends a CLOSE command to the module, then frees the personally assigned module structure.  Note that the
// module code will be left resident in memory as it belongs to the ModuleMaster, not the Module.  See Expunge()
// in the Core for further details.

static ERROR MODULE_Free(extModule *Self, APTR Void)
{
   // Call the Module's Close procedure

   if (Self->Master) {
      if (Self->Master->OpenCount > 0) Self->Master->OpenCount--;
      if (Self->Master->Close)         Self->Master->Close(Self);
      Self->Master = NULL;
   }

   if (Self->prvMBMemory) { FreeResource(Self->prvMBMemory); Self->prvMBMemory = NULL; }
   return ERR_Okay;
}

//********************************************************************************************************************

static ERROR MODULE_Init(extModule *Self, APTR Void)
{
   pf::Log log;
   #define AF_MODULEMASTER 0x0001
   #define AF_SEGMENT      0x0002
   ERROR error = ERR_Failed;
   LONG i;
   WORD aflags = 0;
   char name[60];

   if (!Self->Name[0]) return log.warning(ERR_FieldNotSet);

   // Check if the module is resident.  If not, we need to load and prepare the module for a shared environment.

   OBJECTPTR context = NULL;

   i = StrLength(Self->Name);
   while ((i > 0) and (Self->Name[i-1] != ':') and (Self->Name[i-1] != '/') and (Self->Name[i-1] != '\\')) i--;
   StrCopy(Self->Name+i, name, sizeof(name));

   log.trace("Finding module %s (%s)", Self->Name, name);

   ModuleMaster *master;
   ModHeader *table;
   if ((master = check_resident(Self, name))) {
      Self->Master = master;
   }
   else if (!NewObject(ID_MODULEMASTER, NF::UNTRACKED, (OBJECTPTR *)&master)) {
      std::string path;

      master->Next = glModuleList; // Insert the ModuleMaster at the start of the chain.
      if (glModuleList) glModuleList->Prev = master;
      glModuleList = master;

      aflags |= AF_MODULEMASTER;

      context = SetContext(master);

      StrCopy(name, master->LibraryName, sizeof(master->LibraryName));

      if (Self->Header) {
         // If the developer has specified a module header, then the module code is memory-resident and not to be
         // loaded from storage.
         table = Self->Header;
      }
      else {
         for (i=0; (Self->Name[i]) and (Self->Name[i] != ':'); i++);

         if ((Self->Name[0] IS '/') or (Self->Name[i] IS ':')) {
            log.trace("Module location is absolute.");
            path = Self->Name;

            STRING volume;
            if (!ResolvePath(path.c_str(), RSF_APPROXIMATE, &volume)) {
               path = volume;
               FreeResource(volume);
            }
            else {
               log.warning("Failed to resolve the path of module '%s'", Self->Name);
               error = ERR_ResolvePath;
               goto exit;
            }
         }

         if (path.empty()) {
            #ifdef __unix__
               if (!glModulePath.empty()) { // If no specific module path is defined, default to the system path and tack on the modules/ suffix.
                  path = glModulePath;
               }
               else path = std::string(glRootPath) + "lib/parasol/";

               if (Self->Flags & MOF_LINK_LIBRARY) path += "lib/";

               #ifdef __ANDROID__
                  if ((Self->Name[0] IS 'l') and (Self->Name[1] IS 'i') and (Self->Name[2] IS 'b'));
                  else path += "lib"; // Packaged Android modules have to begin with 'lib'
               #endif

               path.append(Self->Name);

            #elif _WIN32
               if (!glModulePath.empty()) {
                  path = glModulePath;
                  if ((path.back() != '\\') and (path.back() != '/')) path.push_back('\\');
               }
               else if (glSystemPath[0]) {
                  path = glSystemPath;
                  if ((path.back() != '\\') and (path.back() != '/')) path.push_back('\\');
                  path += "lib\\";
               }
               else {
                  path = glRootPath;
                  if ((path.back() != '\\') and (path.back() != '/')) path.push_back('\\');
                  path += "lib\\";
               }

               if (Self->Flags & MOF_LINK_LIBRARY) path += "lib\\";
               path.append(Self->Name);
            #endif
         }

         // Deal with the file extensions

         if (path.ends_with(".dll"));
         else if (path.ends_with(".so"));
         else {
            #ifdef __unix__
               path.append(".so");
            #elif _WIN32
               path.append(".dll");
            #elif __APPLE__ // OSX uses .dylib but is compatible with .so
               path.append(".so");
            #else
               #error What is the module extension for this machine type (.so/.mod/...)?
            #endif
         }

         log.trace("Loading module \"%s\".", path.c_str());

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

            if ((master->LibraryBase = dlopen(path.c_str(), (Self->Flags & MOF_LINK_LIBRARY) ? (RTLD_LAZY|RTLD_GLOBAL) : RTLD_LAZY))) {
               aflags |= AF_SEGMENT;

               if (!(Self->Flags & MOF_LINK_LIBRARY)) {
                  if (!(table = (ModHeader *)dlsym(master->LibraryBase, "ModHeader"))) {
                     log.warning("The 'ModHeader' structure is missing from module %s.", path.c_str());
                     goto exit;
                  }
               }
            }
            else {
               log.warning("%s: %s", name, (CSTRING)dlerror());
               error = ERR_NoSupport;
               goto exit;
            }

         #elif _WIN32

            if ((master->LibraryBase = winLoadLibrary(path.c_str()))) {
               aflags |= AF_SEGMENT;

               if (!(Self->Flags & MOF_LINK_LIBRARY)) {
                  if (!(table = (ModHeader *)winGetProcAddress(master->LibraryBase, "ModHeader"))) {
                     if (!(table = (ModHeader *)winGetProcAddress(master->LibraryBase, "_ModHeader"))) {
                        log.warning("The 'ModHeader' structure is missing from module %s.", path.c_str());
                        goto exit;
                     }
                  }
               }
            }
            else {
               char msg[100];
               log.error("Failed to load DLL '%s' (call: winLoadLibrary(): %s).", path.c_str(), winFormatMessage(0, msg, sizeof(msg)));
               error = ERR_Read;
               goto exit;
            }

         #else
            #error This system needs support for the loading of module/exe files.
         #endif
      }

      // The module version fields can give clues as to whether the table is corrupt or not.

      if (table) {
         if ((table->ModVersion > 500) or (table->ModVersion < 0)) {
            log.warning("Corrupt module version number %d for module '%s'", (LONG)master->ModVersion, path.c_str());
            goto exit;
         }
         else if ((table->HeaderVersion < MODULE_HEADER_V1) or (table->HeaderVersion > MODULE_HEADER_V1 + 256)) {
            log.warning("Invalid module header $%.8x", table->HeaderVersion);
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
               log.debug("Module already loaded as #%d, reverting to original ModuleMaster object.", table->Master->UID);

               SetContext(context);
               context = NULL;

               free_module(master->LibraryBase);
               master->LibraryBase = NULL;
               acFree(master);

               Self->Master = table->Master;
               master = table->Master;
               goto open_module;
            }

            table->Master = master;
         }

         if (!table->Init) { log.warning(ERR_ModuleMissingInit); goto exit; }
         if (!table->Name) { log.warning(ERR_ModuleMissingName); goto exit; }

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
            char mmname[30];
            mmname[0] = 'm';
            mmname[1] = 'm';
            mmname[2] = '_';
            for (i=0; (size_t) i < sizeof(mmname)-4; i++) mmname[i+3] = master->Name[i];
            mmname[i+3] = 0;
            SetName(master, mmname);
         }
#endif
      }

      // INIT

      if (master->Init) {
         // Build a Core base for the module to use
         struct CoreBase *modkb;
         if ((modkb = (struct CoreBase *)build_jump_table(master->Table->Flags, glFunctions, 0))) {
            master->CoreBase = modkb;
            fix_core_table(modkb, table->CoreVersion);

            log.traceBranch("Initialising the module.");
            error = master->Init(Self, modkb);
            if (error) goto exit;
         }
      }
      else if (Self->Flags & MOF_LINK_LIBRARY) {
         log.msg("Loaded link library '%s'", Self->Name);
      }
      else {
         log.warning(ERR_ModuleMissingInit);
         goto exit;
      }

      SetContext(context);
      context = NULL;
   }
   else {
      error = log.warning(ERR_NewObject);
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
      log.trace("Opening %s module.", Self->Name);
      if (master->Open(Self) != ERR_Okay) {
         log.warning(ERR_ModuleOpenFailed);
         goto exit;
      }
   }

   if (master->Table) master->Close = master->Table->Close;
   master->OpenCount++;

   // Open() should have set the Self->FunctionList for us, but if it is null we will have to grab the default
   // function list.

   if (!Self->FunctionList) {
      if (master->Header) {
         Self->FunctionList = master->Header->DefaultList;
      }
      else if (!(Self->Flags & MOF_LINK_LIBRARY)) error = log.warning(ERR_EntryMissingHeader);
   }

   // Build the jump table for the program

   if (Self->FunctionList) {
      if ((Self->ModBase = build_jump_table(MHF_STRUCTURE, Self->FunctionList, 0)) IS NULL) {
         goto exit;
      }
      Self->prvMBMemory = Self->ModBase;
   }

   // Some DLL's like wsock2 can change the exception handler - we don't want that, so reset our exception handler
   // just in case.

   #ifdef _WIN32
      winSetUnhandledExceptionFilter(NULL);
   #endif

   log.trace("Module has been successfully initialised.");
   error = ERR_Okay;

exit:
   if (error) { // Free allocations if an error occurred

      if (!(error & ERF_Notified)) log.msg("\"%s\" failed: %s", Self->Name, GetErrorMsg(error));
      error &= ~(ERF_Notified|ERF_Delay);

      if (aflags & AF_MODULEMASTER) {
         if (master->Expunge) {
            log.msg("Expunging...");
            master->Expunge();
         }

         acFree(master);
         Self->Master = NULL;
      }
   }

   if (context) SetContext(context);
   return error;
}

/*********************************************************************************************************************

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

**********************************************************************************************************************/

static ERROR MODULE_ResolveSymbol(extModule *Self, struct modResolveSymbol *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Name)) return log.warning(ERR_NullArgs);

#ifdef _WIN32
   if ((!Self->Master) or (!Self->Master->LibraryBase)) return ERR_FieldNotSet;

   if ((Args->Address = winGetProcAddress(Self->Master->LibraryBase, Args->Name))) {
      return ERR_Okay;
   }
   else {
      log.msg("Failed to resolve '%s' in %s module.", Args->Name, Self->Master->Name);
      return ERR_NotFound;
   }
#elif __unix__
   if ((!Self->Master) or (!Self->Master->LibraryBase)) return ERR_FieldNotSet;

   if ((Args->Address = dlsym(Self->Master->LibraryBase, Args->Name))) {
      return ERR_Okay;
   }
   else {
      log.msg("Failed to resolve '%s' in %s module.", Args->Name, Self->Master->Name);
      return ERR_NotFound;
   }
#else
   #warning Platform not supported.
   return ERR_NoSupport;
#endif
}

/*********************************************************************************************************************

-FIELD-
Flags: Optional flags.
Lookup: MOF

-FIELD-
IDL: Returns a compressed IDL string from the module, if available.

**********************************************************************************************************************/

static ERROR GET_IDL(extModule *Self, CSTRING *Value)
{
   pf::Log log;

   if ((Self->Master) and (Self->Master->Header)) {
      *Value = Self->Master->Header->Definitions;
      log.trace("No IDL for module %s", Self->Name);
      return ERR_Okay;
   }
   else return log.warning(ERR_NotInitialised);
}

/*********************************************************************************************************************
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

**********************************************************************************************************************/

static ERROR SET_Header(extModule *Self, ModHeader *Value)
{
   if (!Value) return ERR_Failed;
   Self->Header = Value;
   return ERR_Okay;
}

/*********************************************************************************************************************

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

**********************************************************************************************************************/

static ERROR GET_Name(extModule *Self, CSTRING *Value)
{
   *Value = Self->Name;
   return ERR_Okay;
}

static ERROR SET_Name(extModule *Self, CSTRING Name)
{
   if (!Name) return ERR_Okay;

   LONG i;
   for (i=0; (Name[i]) and ((size_t)i < sizeof(Self->Name)-1); i++) {
      if ((Name[i] >= 'A') and (Name[i] <= 'Z')) Self->Name[i] = Name[i] - 'A' + 'a';
      else Self->Name[i] = Name[i];
   }
   Self->Name[i] = 0;

   return ERR_Okay;
}

/*********************************************************************************************************************

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

**********************************************************************************************************************/

//********************************************************************************************************************
// Builds jump tables that link programs to modules.

APTR build_jump_table(LONG JumpType, const Function *FList, LONG MemFlags)
{
   pf::Log log(__FUNCTION__);

   if ((!JumpType) or (!FList)) log.warning("JumpTable() Invalid arguments.");

   if (JumpType & MHF_STRUCTURE) {
      LONG size = 0;
      LONG i;
      for (i=0; FList[i].Address; i++) size += sizeof(APTR);

      log.trace("%d functions have been detected in the function list.", i);

      void **functions;
      if (!AllocMemory(size + sizeof(APTR), MEM_NO_CLEAR|MemFlags, (APTR *)&functions, NULL)) {
         LONG i;
         for (i=0; FList[i].Address; i++) functions[i] = FList[i].Address;
         functions[i] = NULL;
         return functions;
      }
      else log.warning(ERR_AllocMemory);
   }

   return NULL;
}

//********************************************************************************************************************
// This special routine will compare strings up to a '.' extension or null character.

static LONG cmp_mod_names(CSTRING String1, CSTRING String2)
{
   if ((String1) and (String2)) {
      // Skip past any : or / folder characters

      WORD i = 0;
      while (String1[i]) {
         if ((String1[i] IS ':') or (String1[i] IS '/')) {
            String1 += i + 1;  // Increment string's position
            i = 0;             // Reset the counter
         }
         else i++;
      }

      i = 0;
      while (String2[i] != 0) {
         if ((String2[i] IS ':') or (String2[i] IS '/')) {
            String2 += i + 1;
            i = 0;
         }
         else i++;
      }

      // Loop until String1 reaches termination

      while ((*String1 != '.') and (*String1 != 0)) {
         char ch1 = *String1;
         char ch2 = *String2;
         if ((ch2 IS '.') or (ch2 IS 0)) return FALSE;
         if ((ch1 >= 'a') and (ch1 <= 'z')) ch1 -= 0x20;
         if ((ch2 >= 'a') and (ch2 <= 'z')) ch2 -= 0x20;
         if (ch1 != ch2) return FALSE;
         String1++; String2++;
      }

      // If we get this far then both strings match up to the end of String1, so now we need to check if String2 has
      // also terminated at the same point.

      if ((*String2 IS '.') or (*String2 IS 0)) return TRUE;
   }

   return FALSE;
}

//********************************************************************************************************************
// Searches the system for a ModuleMaster header that matches the Module details.  The module must have been
// loaded into memory in order for this function to return successfully.

static ModuleMaster * check_resident(extModule *Self, CSTRING ModuleName)
{
   pf::Log log(__FUNCTION__);
   ModuleMaster *master;
   static BYTE kminit = FALSE;

   if (!ModuleName) return NULL;

   log.traceBranch("Module Name: %s", ModuleName);

   if (!StrMatch("core", ModuleName)) {
      log.msg("Self-reference to the Core detected.");
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
            log.trace("Entry for module \"%s\" (\"%s\") found.", ModuleName, master->Name);
            return master;
         }
         master = master->Next;
      }
   }

   return NULL;
}

//********************************************************************************************************************

static void free_module(MODHANDLE handle)
{
   pf::Log log(__FUNCTION__);

   if (!handle) return;

   log.traceBranch("%p", handle);

   #ifdef ANALYSIS_ENABLED
      // Library closure is disabled when code analysis is turned on so that code
      // addresses can be looked up correctly.
   #else
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
   #endif
}

