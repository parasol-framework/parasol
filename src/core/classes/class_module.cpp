/*********************************************************************************************************************

The source code of the Parasol project is made publicly available under the terms described in the LICENSE.TXT file
that is distributed with this package.  Please refer to it for further information on licensing.

**********************************************************************************************************************

-CLASS-
Module: Manages the loading of system libraries.

The Module class is used to load and maintain the modules that are installed on the user's system.  A number of modules
are available in the core platform as standard, which you can use in the development of your programs.  Examples of
existing modules can be found in both the `modules:` folder.

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

static STRUCTS glStructures = {
   { "ActionArray",         sizeof(ActionArray) },
   { "ActionEntry",         sizeof(ActionEntry) },
   //{ "ActionTable",         sizeof(ActionTable) },
   { "CacheFile",           sizeof(CacheFile) },
   { "ChildEntry",          sizeof(ChildEntry) },
   { "ClipRectangle",       sizeof(ClipRectangle) },
   { "ColourFormat",        sizeof(ColourFormat) },
   { "CompressedItem",      sizeof(CompressedItem) },
   { "CompressionFeedback", sizeof(CompressionFeedback) },
   { "DateTime",            sizeof(DateTime) },
   { "DirInfo",             sizeof(DirInfo) },
   { "Edges",               sizeof(Edges) },
   { "FRGB",                sizeof(FRGB) },
   { "Field",               sizeof(Field) },
   { "FieldArray",          sizeof(FieldArray) },
   { "FieldDef",            sizeof(FieldDef) },
   { "FileFeedback",        sizeof(FileFeedback) },
   { "FileInfo",            sizeof(FileInfo) },
   { "Function",            sizeof(Function) },
   { "FunctionField",       sizeof(FunctionField) },
   { "HSV",                 sizeof(HSV) },
   { "InputEvent",          sizeof(InputEvent) },
   { "MemInfo",             sizeof(MemInfo) },
   { "Message",             sizeof(Message) },
   { "MethodEntry",         sizeof(MethodEntry) },
   { "ModHeader",           sizeof(struct ModHeader) },
   { "MsgHandler",          sizeof(MsgHandler) },
   { "ObjectSignal",        sizeof(ObjectSignal) },
   { "RGB16",               sizeof(RGB16) },
   { "RGB32",               sizeof(RGB32) },
   { "RGB8",                sizeof(RGB8) },
   { "RGBPalette",          sizeof(RGBPalette) },
   { "ResourceManager",     sizeof(ResourceManager) },
   { "SystemState",         sizeof(SystemState) },
   { "ThreadActionMessage", sizeof(ThreadActionMessage) },
   { "ThreadMessage",       sizeof(ThreadMessage) },
   { "Variable",            sizeof(Variable) },
   { "dcAudio",             sizeof(dcAudio) },
   { "dcDeviceInput",       sizeof(dcDeviceInput) },
   { "dcKeyEntry",          sizeof(dcKeyEntry) },
   { "dcRequest",           sizeof(dcRequest) },
   { "pfBase64Decode",      sizeof(pfBase64Decode) },
   { "pfBase64Encode",      sizeof(pfBase64Encode) }
};

#include "../idl.h"

static RootModule glCoreRoot;
struct ModHeader glCoreHeader(NULL, NULL, NULL, NULL, glIDL, &glStructures, "core");

static bool cmp_mod_names(CSTRING, CSTRING);
static RootModule * check_resident(extModule *, CSTRING);
static void free_module(MODHANDLE handle);

//********************************************************************************************************************

static ERR GET_Name(extModule *, CSTRING *);

static ERR SET_Header(extModule *, struct ModHeader *);
static ERR SET_Name(extModule *, CSTRING);

static const FieldDef clFlags[] = {
   { "LinkLibrary", MOF::LINK_LIBRARY },
   { "Static",      MOF::STATIC },
   { NULL, 0 }
};

static const FieldArray glModuleFields[] = {
   { "FunctionList", FDF_POINTER|FDF_RW },
   { "ModBase",      FDF_POINTER|FDF_R },
   { "Root",         FDF_POINTER|FDF_R },
   { "Header",       FDF_POINTER|FDF_RI, NULL, SET_Header },
   { "Flags",        FDF_LONG|FDF_RI, NULL, NULL, &clFlags },
   // Virtual fields
   { "Name",         FDF_STRING|FDF_RI, GET_Name, SET_Name },
   END_FIELD
};

static ERR MODULE_Init(extModule *, APTR);
static ERR MODULE_Free(extModule *, APTR);

static const ActionArray glModuleActions[] = {
   { AC_Free, MODULE_Free },
   { AC_Init, MODULE_Init },
   { 0, NULL }
};

//********************************************************************************************************************

#ifndef PARASOL_STATIC
static ERR load_mod(extModule *Self, RootModule *Root, struct ModHeader **Table)
{
   pf::Log log(__FUNCTION__);
   std::string path;
   LONG i;

   for (i=0; (Self->Name[i]) and (Self->Name[i] != ':'); i++);

   if ((Self->Name[0] IS '/') or (Self->Name[i] IS ':')) {
      log.trace("Module location is absolute.");
      path.assign(Self->Name);

      STRING volume;
      if (ResolvePath(path.c_str(), RSF::APPROXIMATE, &volume) IS ERR::Okay) {
         path.assign(volume);
         FreeResource(volume);
      }
      else {
         log.warning("Failed to resolve the path of module '%s'", Self->Name);
         return ERR::ResolvePath;
      }
   }

   if (path.empty()) {
      #ifdef __unix__
         if (!glModulePath.empty()) { // If no specific module path is defined, default to the system path and tack on the modules/ suffix.
            path.assign(glModulePath);
            if (path.back() != '/') path.push_back('/');
         }
         else path = glRootPath + "lib/parasol/";

         if ((Self->Flags & MOF::LINK_LIBRARY) != MOF::NIL) path += "lib/";

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
         else if (!glSystemPath.empty()) {
            path = glSystemPath;
            if ((path.back() != '\\') and (path.back() != '/')) path.push_back('\\');
            path += "lib\\";
         }
         else {
            path = glRootPath;
            if ((path.back() != '\\') and (path.back() != '/')) path.push_back('\\');
            path += "lib\\";
         }

         if ((Self->Flags & MOF::LINK_LIBRARY) != MOF::NIL) path += "lib\\";
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

   #ifdef __unix__

      // RTLD_LAZY needs to be used in case the module wants to have the ability to link to
      // symbolically linked libraries (e.g. the Network module does this to dynamically load
      // SSL support).
      //
      // RTLD_GLOBAL is needed only for symbolically linked libraries in case one is dependent on
      // other libraries.  SSL is an example of this as the libssl library is dependent
      // on symbols found in libcrypto, therefore libcrypto needs RTLD_GLOBAL.

      if ((Root->LibraryBase = dlopen(path.c_str(), ((Self->Flags & MOF::LINK_LIBRARY) != MOF::NIL) ? (RTLD_LAZY|RTLD_GLOBAL) : RTLD_LAZY))) {
         if ((Self->Flags & MOF::LINK_LIBRARY) IS MOF::NIL) {
            if (!(*Table = (struct ModHeader *)dlsym(Root->LibraryBase, "ModHeader"))) {
               log.warning("The 'ModHeader' structure is missing from module %s.", path.c_str());
               return ERR::NotFound;
            }
         }
      }
      else {
         log.warning("%s: %s", Self->Name, (CSTRING)dlerror());
         return ERR::NoSupport;
      }

   #elif _WIN32

      if ((Root->LibraryBase = winLoadLibrary(path.c_str()))) {
         if ((Self->Flags & MOF::LINK_LIBRARY) IS MOF::NIL) {
            if (!(*Table = (struct ModHeader *)winGetProcAddress(Root->LibraryBase, "ModHeader"))) {
               if (!(*Table = (struct ModHeader *)winGetProcAddress(Root->LibraryBase, "_ModHeader"))) {
                  log.warning("The 'ModHeader' structure is missing from module %s.", path.c_str());
                  return ERR::NotFound;
               }
            }
         }
      }
      else {
         char msg[100];
         log.error("Failed to load DLL '%s' (call: winLoadLibrary(): %s).", path.c_str(), winFormatMessage(0, msg, sizeof(msg)));
         return ERR::Read;
      }

   #else
      #error This system needs support for the loading of module/exe files.
   #endif

   return ERR::Okay;
}
#endif

//********************************************************************************************************************

ERR ROOTMODULE_Free(RootModule *Self, APTR Void)
{
   if (Self->Table) Self->Table->Root = NULL; // Remove the DLL's reference to the master.

   // Note that the order in which we perform the following actions is very important.

   if (Self->CoreBase) { FreeResource(Self->CoreBase); Self->CoreBase = NULL; }

   // Free the module's segment/code area

   if ((Self->NoUnload IS FALSE) and ((Self->Flags & MHF::STATIC) IS MHF::NIL)) {
      free_module(Self->LibraryBase);
      Self->LibraryBase = NULL;
   }

   if (auto lock = std::unique_lock{glmGeneric, 200ms}) {
      // Patch the gap
      if (Self->Prev) Self->Prev->Next = Self->Next;
      else glModuleList = Self->Next;

      if (Self->Next) Self->Next->Prev = Self->Prev;
   }

   return ERR::Okay;
}

static ERR ROOTMODULE_GET_Header(RootModule *Self, struct ModHeader **Value)
{
   *Value = Self->Header;
   return ERR::Okay;
}

//********************************************************************************************************************
// This action sends a CLOSE command to the module, then frees the personally assigned module structure.  Note that the
// module code will be left resident in memory as it belongs to the RootModule, not the Module.  See Expunge()
// in the Core for further details.

static ERR MODULE_Free(extModule *Self, APTR Void)
{
   // Call the Module's Close procedure

   if (Self->Root) {
      if (Self->Root->OpenCount > 0) Self->Root->OpenCount--;
      if (Self->Root->Close)         Self->Root->Close(Self);
      Self->Root = NULL;
   }

   if (Self->prvMBMemory) { FreeResource(Self->prvMBMemory); Self->prvMBMemory = NULL; }
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR MODULE_Init(extModule *Self, APTR Void)
{
   pf::Log log;
   ERR error = ERR::Failed;
   LONG i;
   bool root_mod = false;
   char name[60];

   if (!Self->Name[0]) return log.warning(ERR::FieldNotSet);

   // Check if the module is resident.  If not, we need to load and prepare the module for a shared environment.

   OBJECTPTR context = NULL;

   i = StrLength(Self->Name);
   while ((i > 0) and (Self->Name[i-1] != ':') and (Self->Name[i-1] != '/') and (Self->Name[i-1] != '\\')) i--;
   StrCopy(Self->Name+i, name, sizeof(name));

   log.trace("Finding module %s (%s)", Self->Name, name);

   RootModule *master;
   struct ModHeader *table = NULL;
   if ((master = check_resident(Self, name))) {
      Self->Root = master;
   }
   else if (NewObject(ID_ROOTMODULE, NF::UNTRACKED, (OBJECTPTR *)&master) IS ERR::Okay) {
      master->Next = glModuleList; // Insert the RootModule at the start of the chain.
      if (glModuleList) glModuleList->Prev = master;
      glModuleList = master;

      root_mod = true;

      context = SetContext(master);

      StrCopy(name, master->LibraryName, sizeof(master->LibraryName));

      if (Self->Header) {
         // If the developer has specified a module header, then the module code is memory-resident and not to be
         // loaded from storage.
         table = Self->Header;
      }
      else {
         #ifdef PARASOL_STATIC
         auto it = glStaticModules.find(Self->Name);
         if (it != glStaticModules.end()) table = it->second;
         else {
            log.warning("Unable to find module '%s' from %d static modules.", Self->Name, LONG(glStaticModules.size()));
            error = ERR::NotFound;
            goto exit;
         }
         #else
         if ((error = load_mod(Self, master, &table)) != ERR::Okay) goto exit;
         #endif
      }

      master->OpenCount  = 0;
      master->Version    = 1;
      Self->Root = master;

      if (table) {
         if (!table->Init) { log.warning(ERR::ModuleMissingInit); goto exit; }
         if (!table->Name) { log.warning(ERR::ModuleMissingName); goto exit; }

         master->Header     = table;
         master->Table      = table;
         master->Name       = table->Name;
         master->Init       = table->Init;
         master->Open       = table->Open;
         master->Expunge    = table->Expunge;
         master->Flags      = table->Flags;
      }

      // INIT

      if (master->Init) {
         #ifdef PARASOL_STATIC
            error = master->Init(Self, NULL);
         #else
            // Build a Core base for the module to use
            if (auto modkb = (struct CoreBase *)build_jump_table(glFunctions)) {
               master->CoreBase = modkb;
               log.traceBranch("Initialising the module.");
               error = master->Init(Self, modkb);
            }
         #endif
         if (error != ERR::Okay) goto exit;
      }
      else if ((Self->Flags & MOF::LINK_LIBRARY) != MOF::NIL) {
         log.msg("Loaded link library '%s'", Self->Name);
      }
      else {
         log.warning(ERR::ModuleMissingInit);
         goto exit;
      }

      SetContext(context);
      context = NULL;
   }
   else {
      error = log.warning(ERR::NewObject);
      goto exit;
   }

   // If the STATIC option is set then the loaded module must not be removed when the Module object is freed.  This is
   // typically used for symbolic linked libraries.

   if ((Self->Flags & MOF::STATIC) != MOF::NIL) master->Flags |= MHF::STATIC;

   // At this stage the module is 100% resident and it is not possible to reverse the process.  Because of this, if an
   // error occurs we must not try to free any resident allocations from memory.

   root_mod = false;

   if (master->Open) {
      log.trace("Opening %s module.", Self->Name);
      if (master->Open(Self) != ERR::Okay) {
         log.warning(ERR::ModuleOpenFailed);
         goto exit;
      }
   }

   if (master->Table) master->Close = master->Table->Close;
   master->OpenCount++;

   // Build the jump table for the program

   #ifndef PARASOL_STATIC
   if (Self->FunctionList) {
      if (!(Self->ModBase = build_jump_table(Self->FunctionList))) {
         goto exit;
      }
      Self->prvMBMemory = Self->ModBase;
   }
   #endif

   // Some DLL's like wsock2 can change the exception handler - we don't want that, so reset our exception handler
   // just in case.

   #ifdef _WIN32
      winSetUnhandledExceptionFilter(NULL);
   #endif

   log.trace("Module has been successfully initialised.");
   error = ERR::Okay;

exit:
   if (error != ERR::Okay) { // Free allocations if an error occurred

      if ((error & ERR::Notified) IS ERR::Okay) log.msg("\"%s\" failed: %s", Self->Name, GetErrorMsg(error));
      error &= ~(ERR::Notified);

      if (root_mod) {
         if (master->Expunge) master->Expunge();
         FreeResource(master);
         Self->Root = NULL;
      }
   }

   if (context) SetContext(context);
   return error;
}

/*********************************************************************************************************************

-METHOD-
ResolveSymbol: Resolves the symbol names in loaded link libraries to address pointers.

This method will convert symbol names to their respective address pointers.  The module code must have been successfully
loaded into memory or an ERR::FieldNotSet error will be returned.  If the symbol was not found then ERR::NotFound is
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

static ERR MODULE_ResolveSymbol(extModule *Self, struct modResolveSymbol *Args)
{
   pf::Log log;

   if ((!Args) or (!Args->Name)) return log.warning(ERR::NullArgs);

#ifdef _WIN32
   #ifdef PARASOL_STATIC
   if ((Args->Address = winGetProcAddress(NULL, Args->Name))) {
   #else
   if ((!Self->Root) or (!Self->Root->LibraryBase)) return ERR::FieldNotSet;
   if ((Args->Address = winGetProcAddress(Self->Root->LibraryBase, Args->Name))) {
   #endif
      return ERR::Okay;
   }
   else {
      log.msg("Failed to resolve '%s' in %s module.", Args->Name, Self->Root->Name);
      return ERR::NotFound;
   }
#elif __unix__
   #ifdef PARASOL_STATIC
   if ((Args->Address = dlsym(RTLD_DEFAULT, Args->Name))) {
   #else
   if ((!Self->Root) or (!Self->Root->LibraryBase)) return ERR::FieldNotSet;
   if ((Args->Address = dlsym(Self->Root->LibraryBase, Args->Name))) {
   #endif
      return ERR::Okay;
   }
   else {
      log.msg("Failed to resolve '%s' in %s module.", Args->Name, Self->Root->Name);
      return ERR::NotFound;
   }
#else
   #warning Platform not supported.
   return ERR::NoSupport;
#endif
}

/*********************************************************************************************************************

-FIELD-
Flags: Optional flags.
Lookup: MOF

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

static ERR SET_Header(extModule *Self, struct ModHeader *Value)
{
   if (!Value) return ERR::Failed;
   Self->Header = Value;
   return ERR::Okay;
}

/*********************************************************************************************************************

-FIELD-
Root: For internal use only.
Status: private

A loaded module can only reside once in memory, irrespective of how many module objects are used to represent it.  The
Root field refers to a RootModule object that reflects this single instance of the module.

-FIELD-
ModBase: The Module's function base (jump table) must be read from this field.

Initialising a module will create a jump table that is referenced in the ModBase field.  The jump table contains
vectors that point to all functions that are published by the module.  This is considered an internal feature that
is hidden by system headers.

If the module is unloaded at any time then the jump table becomes invalid.

-FIELD-
Name: The name of the module.

This string pointer specifies the name of the Module.  This name will be used to load the module from the `modules:`
folder, so this field actually reflects part of the module file name.  It is also possible to specify
sub-directories before the module name itself - this could become more common in module loading in future.

It is critical that file extensions do not appear in the Name string, e.g. `screen.dll` as not all systems
may use a `.dll` extension.

**********************************************************************************************************************/

static ERR GET_Name(extModule *Self, CSTRING *Value)
{
   *Value = Self->Name;
   return ERR::Okay;
}

static ERR SET_Name(extModule *Self, CSTRING Name)
{
   if (!Name) return ERR::Okay;

   LONG i;
   for (i=0; (Name[i]) and ((size_t)i < sizeof(Self->Name)-1); i++) {
      if ((Name[i] >= 'A') and (Name[i] <= 'Z')) Self->Name[i] = Name[i] - 'A' + 'a';
      else Self->Name[i] = Name[i];
   }
   Self->Name[i] = 0;

   return ERR::Okay;
}

//********************************************************************************************************************
// Builds jump tables that link programs to modules.

#ifndef PARASOL_STATIC
APTR build_jump_table(const Function *FList)
{
   if (!FList) return NULL;

   pf::Log log(__FUNCTION__);

   LONG size;
   for (size=0; FList[size].Address; size++);

   log.trace("%d functions have been detected in the function list.", size);

   void **functions;
   if (AllocMemory((size+1) * sizeof(APTR), MEM::NO_CLEAR|MEM::UNTRACKED, (APTR *)&functions, NULL) IS ERR::Okay) {
      for (LONG i=0; i < size; i++) functions[i] = FList[i].Address;
      functions[size] = NULL;
      return functions;
   }
   else log.warning(ERR::AllocMemory);
   return NULL;
}
#endif

//********************************************************************************************************************
// Compare strings up to a '.' extension or null character.

static bool cmp_mod_names(CSTRING String1, CSTRING String2)
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
         if ((ch2 IS '.') or (ch2 IS 0)) return false;
         if ((ch1 >= 'a') and (ch1 <= 'z')) ch1 -= 0x20;
         if ((ch2 >= 'a') and (ch2 <= 'z')) ch2 -= 0x20;
         if (ch1 != ch2) return false;
         String1++; String2++;
      }

      // If we get this far then both strings match up to the end of String1, so now we need to check if String2 has
      // also terminated at the same point.

      if ((*String2 IS '.') or (*String2 IS 0)) return true;
   }

   return false;
}

//********************************************************************************************************************
// Searches the system for a RootModule header that matches the Module details.  The module must have been
// loaded into memory in order for this function to return successfully.

static RootModule * check_resident(extModule *Self, CSTRING ModuleName)
{
   pf::Log log(__FUNCTION__);
   RootModule *master;
   static bool kminit = false;

   if (!ModuleName) return NULL;

   log.traceBranch("Module Name: %s", ModuleName);

   if (StrMatch("core", ModuleName) IS ERR::Okay) {
      log.msg("Self-reference to the Core detected.");
      if (!kminit) {
         kminit = true;
         ClearMemory(&glCoreRoot, sizeof(glCoreRoot));
         glCoreRoot.Class       = glRootModuleClass;
         glCoreRoot.Name        = "Core";
         glCoreRoot.OpenCount   = 1;
         glCoreRoot.Table       = &glCoreHeader;
         glCoreRoot.Header      = &glCoreHeader;
      }
      Self->FunctionList = glFunctions;
      return &glCoreRoot;
   }
   else if ((master = glModuleList)) {
      while (master) {
         if (cmp_mod_names(master->Name, ModuleName)) {
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
         dlclose(handle);
      #elif _WIN32
         winFreeLibrary(handle);
      #else
         #error You need to write machine specific code to expunge modules.
      #endif
   #endif
}

//********************************************************************************************************************

static const FunctionField argsResolveSymbol[] = { { "Name", FD_STR }, { "Address", FD_PTR|FD_RESULT }, { NULL, 0 } };

static const MethodEntry glModuleMethods[] = {
   { MT_ModResolveSymbol, (APTR)MODULE_ResolveSymbol, "ResolveSymbol", argsResolveSymbol, sizeof(struct modResolveSymbol) },
   { 0, NULL, NULL, NULL, 0 }
};

//********************************************************************************************************************

static const FieldArray glRootModuleFields[] = {
   { "Header", FDF_POINTER|FDF_RI, ROOTMODULE_GET_Header },
   END_FIELD
};

static const ActionArray glRootModuleActions[] = {
   { AC_Free, ROOTMODULE_Free },
   { 0, NULL }
};

//********************************************************************************************************************

extern "C" ERR add_module_class(void)
{
   if (!(glModuleClass = extMetaClass::create::global(
      fl::BaseClassID(ID_MODULE),
      fl::ClassVersion(VER_MODULE),
      fl::Name("Module"),
      fl::Category(CCF::SYSTEM),
      fl::FileExtension("*.mod|*.so|*.dll"),
      fl::FileDescription("System Module"),
      fl::Actions(glModuleActions),
      fl::Methods(glModuleMethods),
      fl::Fields(glModuleFields),
      fl::Size(sizeof(extModule)),
      fl::Path("modules:core")))) return ERR::AddClass;

   if (!(glRootModuleClass = extMetaClass::create::global(
      fl::BaseClassID(ID_ROOTMODULE),
      fl::ClassVersion(1.0),
      fl::Name("RootModule"),
      fl::Flags(CLF::NO_OWNERSHIP),
      fl::Category(CCF::SYSTEM),
      fl::Actions(glRootModuleActions),
      fl::Fields(glRootModuleFields),
      fl::Size(sizeof(RootModule)),
      fl::Path("modules:core")))) return ERR::AddClass;

   return ERR::Okay;
}

