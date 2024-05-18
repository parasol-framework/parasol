/*********************************************************************************************************************

This source code is placed in the public domain under no warranty from its
authors.

NOTE REGARDING LUAJIT PATCHES:
  Search for 'PARASOL PATCHED IN' to discover what we've inserted into the code.
  Packages removed from ljamalg.c are: lib_io lib_os lib_package lib_jit
  Modify src/Makefile with the following changes:
    Enable LUAJIT_ENABLE_LUA52COMPAT
    Enable LUAJIT_DISABLE_FFI because FFI is not permitted.
    Switch from BUILDMODE=mixed to BUILDMODE=static
  Optional changes to src/Makefile:
    Enable LUAJIT_USE_SYSMALLOC temporarily if you need to figure out memory management and overflow issues.
    Enable LUAJIT_USE_GDBJIT temporarily if debugging with GDB.

**********************************************************************************************************************

-MODULE-
Fluid: Fluid is a customised scripting language for the Script class.

Fluid is a custom scripting language for Parasol developers.  It is implemented on the backbone of LuaJIT, a
high performance version of the Lua scripting language.  It supports garbage collection, dynamic typing and a byte-code
interpreter for compiled code.  We chose to support Lua due to its extensive popularity amongst game developers, a
testament to its low overhead, speed and lightweight processing when compared to common scripting languages.

Fluid files use the file extensions `.lua` and `.fluid`.  Ideally, scripts should start with the comment '-- $FLUID' near
the start of the document so that it can be correctly identified by the Fluid class.

For more information on the Fluid syntax, please refer to the official Fluid Reference Manual.

-END-

*********************************************************************************************************************/

#ifdef _DEBUG
#undef DEBUG
#endif

#define PRV_SCRIPT
#define PRV_FLUID
#define PRV_FLUID_MODULE
#include <parasol/main.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/display.h>
#include <parasol/modules/fluid.h>

#include <inttypes.h>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lj_obj.h"
}

#include "hashes.h"

JUMPTABLE_CORE

#include "defs.h"

OBJECTPTR modDisplay = NULL; // Required by fluid_input.c
OBJECTPTR modFluid = NULL;
OBJECTPTR clFluid = NULL;
struct ActionTable *glActions = NULL;
std::map<std::string, ACTIONID, CaseInsensitiveMap> glActionLookup;
std::unordered_map<std::string, ULONG> glStructSizes;

static CSTRING load_include_struct(lua_State *, CSTRING, CSTRING);
static CSTRING load_include_constant(lua_State *, CSTRING, CSTRING);
static ERR flSetVariable(objScript *, CSTRING, LONG, ...);

//********************************************************************************************************************

FDEF argsSetVariable[] = { { "Error", FD_ERROR }, { "Script", FD_OBJECTPTR }, { "Name", FD_STR }, { "Type", FD_LONG }, { "Variable", FD_TAGS }, { 0, 0 } };

// These test calls are used to check that the dynamic assembler function calls are working as expected.

#ifdef _DEBUG
static void flTestCall1(void);
static LONG flTestCall2(void);
static CSTRING flTestCall3(void);
static void flTestCall4(LONG, LARGE);
static LONG flTestCall5(LONG, LONG, LONG, LONG, LONG, LARGE);
static LARGE flTestCall6(LONG, LARGE, LARGE, LONG, LARGE, DOUBLE);
static void flTestCall7(STRING a, STRING b, STRING c);

FDEF argsTestCall1[]   = { { "Void", FD_VOID }, { 0, 0 } };
FDEF argsTestCall2[]   = { { "Result", FD_LONG }, { 0, 0 } };
FDEF argsTestCall3[]   = { { "Result", FD_STR }, { 0, 0 } };
FDEF argsTestCall4[]   = { { "Void", FD_VOID }, { "Long", FD_LONG }, { "Large", FD_LARGE }, { 0, 0 } };
FDEF argsTestCall5[]   = { { "Result", FD_LONG }, { "LA", FD_LONG }, { "LB", FD_LONG }, { "LC", FD_LONG }, { "LD", FD_LONG }, { "LE", FD_LONG }, { "LF", FD_LARGE }, { 0, 0 } };
FDEF argsTestCall6[]   = { { "Result", FD_LARGE }, { "LA", FD_LONG }, { "LLA", FD_LARGE }, { "LLB", FD_LARGE }, { "LB", FD_LONG }, { "LLC", FD_LARGE }, { "DA", FD_DOUBLE }, { "LB", FD_LARGE }, { 0, 0 } };
FDEF argsTestCall7[]   = { { "Void", FD_VOID }, { "StringA", FD_STRING }, { "StringB", FD_STRING }, { "StringC", FD_STRING }, { 0, 0 } };
#endif

static const struct Function JumpTableV1[] = {
   { (APTR)flSetVariable, "SetVariable", argsSetVariable },
   #ifdef _DEBUG
   { (APTR)flTestCall1,   "TestCall1", argsTestCall1 },
   { (APTR)flTestCall2,   "TestCall2", argsTestCall2 },
   { (APTR)flTestCall3,   "TestCall3", argsTestCall3 },
   { (APTR)flTestCall4,   "TestCall4", argsTestCall4 },
   { (APTR)flTestCall5,   "TestCall5", argsTestCall5 },
   { (APTR)flTestCall6,   "TestCall6", argsTestCall6 },
   { (APTR)flTestCall7,   "TestCall7", argsTestCall7 },
   #endif
   { NULL, NULL, NULL }
};

#ifdef _DEBUG

static void flTestCall1(void)
{
   pf::Log log(__FUNCTION__);
   log.msg("No parameters.");
}

static LONG flTestCall2(void)
{
   pf::Log log(__FUNCTION__);
   log.msg("Returning 0xdedbeef / %d", 0xdedbeef);
   return 0xdedbeef;
}

static CSTRING flTestCall3(void)
{
   pf::Log log(__FUNCTION__);
   log.msg("Returning 'hello world'");
   return "hello world";
}

static void flTestCall4(LONG Long, LARGE Large)
{
   pf::Log log(__FUNCTION__);
   log.msg("Received long %d / $%.8x", Long, Long);
   log.msg("Received large %" PF64 " / $%.8x%.8x", Large, (ULONG)Large, (ULONG)(Large>>32));
}

static LONG flTestCall5(LONG LongA, LONG LongB, LONG LongC, LONG LongD, LONG LongE, LARGE LongF)
{
   pf::Log log(__FUNCTION__);
   log.msg("Received ints: %d, %d, %d, %d, %d, %" PF64, LongA, LongB, LongC, LongD, LongE, LongF);
   log.msg("Received ints: $%.8x, $%.8x, $%.8x, $%.8x, $%.8x, $%.8x", LongA, LongB, LongC, LongD, LongE, (LONG)LongF);
   return LongF;
}

static LARGE flTestCall6(LONG long1, LARGE large1, LARGE large2, LONG long2, LARGE large3, DOUBLE float1)
{
   pf::Log log(__FUNCTION__);
   log.msg("Received %d, %" PF64 ", %d, %d, %d", long1, large1, (LONG)large2, (LONG)long2, (LONG)large3);
   log.msg("Received double %f", float1);
   log.msg("Returning %" PF64, large2);
   return large2;
}

static void flTestCall7(STRING a, STRING b, STRING c)
{
   pf::Log log(__FUNCTION__);
   log.msg("Received string pointers %p, %p, %p", a, b, c);
   log.msg("As '%s', '%s', '%s'", a, b, c);
}
#endif

//********************************************************************************************************************

CSTRING next_line(CSTRING String)
{
   if (!String) return NULL;

   while ((*String) and (*String != '\n') and (*String != '\r')) String++;
   while (*String IS '\r') String++;
   if (*String IS '\n') String++;
   while (*String IS '\r') String++;
   if (*String) return String;
   else return NULL;
}

//********************************************************************************************************************

APTR get_meta(lua_State *Lua, LONG Arg, CSTRING MetaTable)
{
   APTR address;
   if ((address = (struct object *)lua_touserdata(Lua, Arg))) {
      if (lua_getmetatable(Lua, Arg)) {  // does it have a metatable?
         lua_getfield(Lua, LUA_REGISTRYINDEX, MetaTable);  // get correct metatable
         if (lua_rawequal(Lua, -1, -2)) {  // does it have the correct mt?
            lua_pop(Lua, 2);
            return address;
         }
         lua_pop(Lua, 2);
      }
   }

   return NULL;
}

//********************************************************************************************************************
// Returns a pointer to an object (if the object exists).  To guarantee safety, object access always utilises the ID
// so that we don't run into issues if the object has been collected.

OBJECTPTR access_object(struct object *Object)
{
   if (Object->AccessCount) {
      Object->AccessCount++;
      return Object->ObjectPtr;
   }
   else if (!Object->UID) return NULL; // Object reference is dead
   else if (!Object->ObjectPtr) { // If not pointer defined then treat the object as detached.
      if (auto error = AccessObject(Object->UID, 5000, &Object->ObjectPtr); error IS ERR::Okay) {
         Object->Locked = true;
      }
      else if (error IS ERR::DoesNotExist) {
         pf::Log log(__FUNCTION__);
         log.trace("Object #%d has been terminated.", Object->UID);
         Object->ObjectPtr = NULL;
         Object->UID = 0;
      }
   }

   if (Object->ObjectPtr) Object->AccessCount++;
   return Object->ObjectPtr;
}

void release_object(struct object *Object)
{
   if (Object->AccessCount > 0) {
      Object->AccessCount--;
      if ((!Object->AccessCount) and (Object->Locked)) {
         ReleaseObject(Object->ObjectPtr);
         Object->Locked = false;
         Object->ObjectPtr = NULL;
      }
   }
}

//********************************************************************************************************************
// Automatically load the include file for the given metaclass, if it has not been loaded already.

void auto_load_include(lua_State *Lua, objMetaClass *MetaClass)
{
   pf::Log log(__FUNCTION__);

   CSTRING module_name;
   ERR error;
   if ((error = MetaClass->get(FID_Module, (STRING *)&module_name)) IS ERR::Okay) {
      log.trace("Class: %s, Module: %s", MetaClass->ClassName, module_name);

      auto prv = (prvFluid *)Lua->Script->ChildPrivate;
      if (!prv->Includes.contains(module_name)) {
         prv->Includes.insert(module_name); // Mark the module as processed.

         OBJECTPTR mod;
         if ((error = MetaClass->getPtr(FID_RootModule, &mod)) IS ERR::Okay) {
            struct ModHeader *header;

            if (((error = mod->getPtr(FID_Header, &header)) IS ERR::Okay) and (header)) {
               if (auto structs = header->StructDefs) {
                  for (auto &s : structs[0]) {
                     glStructSizes[s.first] = s.second;
                  }
               }

               if (auto idl = header->Definitions) {
                  log.trace("Parsing IDL for module %s", module_name);

                  while ((idl) and (*idl)) {
                     if ((idl[0] IS 's') and (idl[1] IS '.')) idl = load_include_struct(Lua, idl+2, module_name);
                     else if ((idl[0] IS 'c') and (idl[1] IS '.')) idl = load_include_constant(Lua, idl+2, module_name);
                     else idl = next_line(idl);
                  }
               }
               else log.trace("No IDL defined for %s", module_name);
            }
         }
      }
      else log.trace("Module %s is marked as loaded.", module_name);
   }
   else log.traceWarning("Failed to get module name from class '%s', \"%s\"", MetaClass->ClassName, GetErrorMsg(error));
}

//********************************************************************************************************************

static ERR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   argModule->getPtr(FID_Root, &modFluid);

   ActionList(&glActions, NULL); // Get the global action table from the Core

   // Create a lookup table for converting named actions to IDs.

   for (ACTIONID action_id=1; glActions[action_id].Name; action_id++) {
      glActionLookup[glActions[action_id].Name] = action_id;
   }

   return create_fluid();
}

static ERR CMDExpunge(void)
{
   if (clFluid)    { FreeResource(clFluid); clFluid = NULL; }
   if (modDisplay) { FreeResource(modDisplay); modDisplay = NULL; }
   return ERR::Okay;
}

static ERR CMDOpen(OBJECTPTR Module)
{
   Module->set(FID_FunctionList, JumpTableV1);
   return ERR::Okay;
}

/*********************************************************************************************************************

-FUNCTION-
SetVariable: Sets any variable in a loaded Fluid script.

The SetVariable() function provides a method for setting global variables in a Fluid script prior to execution of that
script.  If the script is cached, the variable settings will be available on the next activation.

-INPUT-
obj Script: Pointer to a Fluid script.
cstr Name: The name of the variable to set.
int(FD) Type: A valid field type must be indicated, e.g. FD_STRING, FD_POINTER, FD_LONG, FD_DOUBLE, FD_LARGE.
tags Variable: A variable that matches the indicated Type.

-ERRORS-
Okay: The variable was defined successfully.
Args:
FieldTypeMismatch: A valid field type was not specified in the Type parameter.
ObjectCorrupt: Privately maintained memory has become inaccessible.
-END-

*********************************************************************************************************************/

static ERR flSetVariable(objScript *Script, CSTRING Name, LONG Type, ...)
{
   pf::Log log(__FUNCTION__);
   prvFluid *prv;
   va_list list;

   if ((!Script) or (Script->classID() != ID_FLUID) or (!Name) or (!*Name)) return log.warning(ERR::Args);

   log.branch("Script: %d, Name: %s, Type: $%.8x", Script->UID, Name, Type);

   if (!(prv = (prvFluid *)Script->ChildPrivate)) return log.warning(ERR::ObjectCorrupt);

   va_start(list, Type);

   if (Type & FD_STRING)       lua_pushstring(prv->Lua, va_arg(list, STRING));
   else if (Type & FD_POINTER) lua_pushlightuserdata(prv->Lua, va_arg(list, APTR));
   else if (Type & FD_LONG)    lua_pushinteger(prv->Lua, va_arg(list, LONG));
   else if (Type & FD_LARGE)   lua_pushnumber(prv->Lua, va_arg(list, LARGE));
   else if (Type & FD_DOUBLE)  lua_pushnumber(prv->Lua, va_arg(list, DOUBLE));
   else {
      va_end(list);
      return log.warning(ERR::FieldTypeMismatch);
   }

   lua_setglobal(prv->Lua, Name);

   va_end(list);
   return ERR::Okay;
}

//********************************************************************************************************************

void hook_debug(lua_State *Lua, lua_Debug *Info)
{
   pf::Log log("Lua");

   if (Info->event IS LUA_HOOKCALL) {
      if (lua_getinfo(Lua, "nSl", Info)) {
         if (Info->name) log.msg("%s: %s.%s(), Line: %d", Info->what, Info->namewhat, Info->name, Lua->Script->CurrentLine + Lua->Script->LineOffset);
      }
      else log.warning("lua_getinfo() failed.");
   }
   else if (Info->event IS LUA_HOOKRET) { }
   else if (Info->event IS LUA_HOOKTAILRET) { }
   else if (Info->event IS LUA_HOOKLINE) {
      Lua->Script->CurrentLine = Info->currentline - 1; // Our line numbers start from zero
      if (Lua->Script->CurrentLine < 0) Lua->Script->CurrentLine = 0; // Just to be certain :-)
/*
      if (lua_getinfo(Lua, "nSl", Info)) {
         log.msg("Line %d: %s: %s", Info->currentline, Info->what, Info->name);
      }
      else log.warning("lua_getinfo() failed.");
*/
   }
}

//********************************************************************************************************************
// Builds an ordered Lua array from a fixed list of values.  Guaranteed to always return a table, empty or not.
// Works with primitives only, for structs please use make_struct_[ptr|serial]_table() because the struct name
// will be required.

void make_table(lua_State *Lua, LONG Type, LONG Elements, CPTR Data)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Type: $%.8x, Elements: %d, Data: %p", Type, Elements, Data);

   if (Elements < 0) {
      if (!Data) Elements = 0;
      else {
         LONG i = 0;
         switch (Type & (FD_DOUBLE|FD_LARGE|FD_FLOAT|FD_POINTER|FD_OBJECT|FD_STRING|FD_LONG|FD_WORD|FD_BYTE)) {
            case FD_STRING:
            case FD_OBJECT:
            case FD_POINTER: for (i=0; ((APTR *)Data)[i]; i++); break;
            case FD_FLOAT:   for (i=0; ((FLOAT *)Data)[i]; i++); break;
            case FD_DOUBLE:  for (i=0; ((DOUBLE *)Data)[i]; i++); break;
            case FD_LARGE:   for (i=0; ((LARGE *)Data)[i]; i++); break;
            case FD_LONG:    for (i=0; ((LONG *)Data)[i]; i++); break;
            case FD_WORD:    for (i=0; ((WORD *)Data)[i]; i++); break;
            case FD_BYTE:    for (i=0; ((BYTE *)Data)[i]; i++); break;
            default:
               log.warning("Unsupported type $%.8x", Type);
               lua_pushnil(Lua);
               return;
         }

         Elements = i;
      }
   }

   lua_createtable(Lua, Elements, 0); // Create a new table on the stack.
   if (!Data) return;

   switch(Type & (FD_DOUBLE|FD_LARGE|FD_FLOAT|FD_POINTER|FD_OBJECT|FD_STRING|FD_LONG|FD_WORD|FD_BYTE)) {
      case FD_STRING:  for (LONG i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); lua_pushstring(Lua, ((CSTRING *)Data)[i]); lua_settable(Lua, -3); } break;
      case FD_OBJECT:  for (LONG i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); push_object(Lua, ((OBJECTPTR *)Data)[i]); lua_settable(Lua, -3); } break;
      case FD_POINTER: for (LONG i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); lua_pushlightuserdata(Lua, ((APTR *)Data)[i]); lua_settable(Lua, -3); } break;
      case FD_FLOAT:   for (LONG i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); lua_pushnumber(Lua, ((FLOAT *)Data)[i]); lua_settable(Lua, -3); } break;
      case FD_DOUBLE:  for (LONG i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); lua_pushnumber(Lua, ((DOUBLE *)Data)[i]); lua_settable(Lua, -3); } break;
      case FD_LARGE:   for (LONG i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); lua_pushnumber(Lua, ((LARGE *)Data)[i]); lua_settable(Lua, -3); } break;
      case FD_LONG:    for (LONG i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); lua_pushinteger(Lua, ((LONG *)Data)[i]); lua_settable(Lua, -3); } break;
      case FD_WORD:    for (LONG i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); lua_pushinteger(Lua, ((WORD *)Data)[i]); lua_settable(Lua, -3); } break;
      case FD_BYTE:    for (LONG i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); lua_pushinteger(Lua, ((BYTE *)Data)[i]); lua_settable(Lua, -3); } break;
   }
}

//********************************************************************************************************************
// Create a Lua array from a list of structure pointers.

void make_struct_ptr_table(lua_State *Lua, CSTRING StructName, LONG Elements, CPTR *Values)
{
   pf::Log log(__FUNCTION__);

   log.trace("%s, Elements: %d, Values: %p", StructName, Elements, Values);

   if (Elements < 0) {
      LONG i;
      for (i=0; Values[i]; i++);
      Elements = i;
   }

   lua_createtable(Lua, Elements, 0); // Create a new table on the stack.
   if (!Values) return;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;

   auto s_name = struct_name(StructName);
   if (prv->Structs.contains(s_name)) {
      std::vector<lua_ref> ref;
      for (LONG i=0; i < Elements; i++) {
         lua_pushinteger(Lua, i+1);
         if (struct_to_table(Lua, ref, prv->Structs[s_name], Values[i]) != ERR::Okay) lua_pushnil(Lua);
         lua_settable(Lua, -3);
      }
   }
   else log.warning("Failed to find struct '%s'", StructName);
}

//********************************************************************************************************************
// Create a Lua array from a serialised list of structures.

void make_struct_serial_table(lua_State *Lua, CSTRING StructName, LONG Elements, CPTR Data)
{
   pf::Log log(__FUNCTION__);

   if (Elements < 0) Elements = 0; // The total number of structs is a hard requirement.

   lua_createtable(Lua, Elements, 0); // Create a new table on the stack.
   if (!Data) return;

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   auto s_name = struct_name(StructName);
   if (prv->Structs.contains(s_name)) {
      auto def = &prv->Structs[s_name];

      // 64-bit compilers don't always align structures to 64-bit, and it's difficult to compute alignment with
      // certainty.  It is essential that structures that are intended to be serialised into arrays are manually
      // padded to 64-bit so that the potential for mishap is eliminated.

      #ifdef _LP64
      LONG def_size = ALIGN64(def->Size);
      #else
      LONG def_size = ALIGN32(def->Size);
      #endif

      char aligned = ((def->Size & 0x7) != 0) ? 'N': 'Y';
      if (aligned IS 'N') {
         log.msg("%s, Elements: %d, Values: %p, StructSize: %d, Aligned: %c", StructName, Elements, Data, def_size, aligned);
      }

      std::vector<lua_ref> ref;

      for (LONG i=0; i < Elements; i++) {
         lua_pushinteger(Lua, i+1);
         if (struct_to_table(Lua, ref, *def, Data) != ERR::Okay) lua_pushnil(Lua);
         Data = (BYTE *)Data + def_size;
         lua_settable(Lua, -3);
      }
   }
   else log.warning("Failed to find struct '%s'", StructName);
}

//********************************************************************************************************************
// The TypeName can be in the format 'Struct:Arg' without causing any issues.

void make_any_table(lua_State *Lua, LONG Type, CSTRING TypeName, LONG Elements, CPTR Values)
{
   if (Type & FD_STRUCT) {
      if (Type & FD_POINTER) make_struct_ptr_table(Lua, TypeName, Elements, (CPTR *)Values);
      else make_struct_serial_table(Lua, TypeName, Elements, Values);
   }
   else make_table(Lua, Type, Elements, Values);
}

//********************************************************************************************************************

void get_line(objScript *Self, LONG Line, STRING Buffer, LONG Size)
{
   CSTRING str;

   if ((str = Self->String)) {
      LONG i;
      for (i=0; i < Line; i++) {
         if (!(str = next_line(str))) {
            Buffer[0] = 0;
            return;
         }
      }

      while ((*str IS ' ') or (*str IS '\t')) str++;

      for (i=0; i < Size-1; i++) {
         if ((*str IS '\n') or (*str IS '\r') or (!*str)) break;
         Buffer[i] = *str++;
      }
      Buffer[i] = 0;
   }
   else Buffer[0] = 0;
}

//********************************************************************************************************************

ERR load_include(objScript *Script, CSTRING IncName)
{
   pf::Log log(__FUNCTION__);

   log.branch("Definition: %s", IncName);

   auto prv = (prvFluid *)Script->ChildPrivate;

   // For security purposes, check the validity of the include name.

   LONG i;
   for (i=0; IncName[i]; i++) {
      if ((IncName[i] >= 'a') and (IncName[i] <= 'z')) continue;
      if ((IncName[i] >= 'A') and (IncName[i] <= 'Z')) continue;
      if ((IncName[i] >= '0') and (IncName[i] <= '9')) continue;
      break;
   }

   if ((IncName[i]) or (i >= 32)) {
      log.msg("Invalid module name; only alpha-numeric names are permitted with max 32 chars.");
      return ERR::Syntax;
   }

   if (prv->Includes.contains(IncName)) {
      log.trace("Include file '%s' has already been loaded.", IncName);
      return ERR::Okay;
   }

   ERR error = ERR::Okay;

   AdjustLogLevel(1);

      objModule::create module = { fl::Name(IncName) };
      if (module.ok()) {
         prv->Includes.insert(IncName); // Mark the file as loaded.

         OBJECTPTR root;
         if ((error = module->getPtr(FID_Root, &root)) IS ERR::Okay) {
            struct ModHeader *header;
            if ((((error = root->getPtr(FID_Header, &header)) IS ERR::Okay) and (header))) {
               if (auto structs = header->StructDefs) {
                  for (auto &s : structs[0]) glStructSizes[s.first] = s.second;
               }

               if (auto idl = header->Definitions) {
                  log.trace("Parsing IDL for module %s", IncName);

                  while ((idl) and (*idl)) {
                     if ((idl[0] IS 's') and (idl[1] IS '.')) idl = load_include_struct(prv->Lua, idl+2, IncName);
                     else if ((idl[0] IS 'c') and (idl[1] IS '.')) idl = load_include_constant(prv->Lua, idl+2, IncName);
                     else idl = next_line(idl);
                  }
               }
               else log.trace("No IDL defined for %s", IncName);
            }
         }
      }
      else error = ERR::CreateObject;

   AdjustLogLevel(-1);
   return error;
}

//********************************************************************************************************************
// Format: s.Name:typeField,...

static CSTRING load_include_struct(lua_State *Lua, CSTRING Line, CSTRING Source)
{
   pf::Log log("load_include");
   LONG i;
   for (i=0; (Line[i] >= 0x20) and (Line[i] != ':'); i++);

   if (Line[i] IS ':') {
      std::string name(Line, i);
      Line += i + 1;

      LONG j;
      for (j=0; (Line[j] != '\n') and (Line[j] != '\r') and (Line[j]); j++);

      if ((Line[j] IS '\n') or (Line[j] IS '\r')) {
         std::string linebuf(Line, j);
         make_struct(Lua, name, linebuf.c_str());
         while ((Line[j] IS '\n') or (Line[j] IS '\r')) j++;
         return Line + j;
      }
      else {
         make_struct(Lua, name, Line);
         return Line + j;
      }
   }
   else {
      log.warning("Malformed struct name in %s.", Source);
      return next_line(Line);
   }
}

//********************************************************************************************************************

static BYTE datatype(std::string_view String)
{
   LONG i = 0;
   while ((String[i]) and (String[i] <= 0x20)) i++; // Skip white-space

   if ((String[i] IS '0') and (String[i+1] IS 'x')) {
      for (i+=2; String[i]; i++) {
         if (((String[i] >= '0') and (String[i] <= '9')) or
             ((String[i] >= 'A') and (String[i] <= 'F')) or
             ((String[i] >= 'a') and (String[i] <= 'f')));
         else return 's';
      }
      return 'h';
   }

   bool is_number = true;
   bool is_float  = false;

   for (; (String[i]) and (is_number); i++) {
      if (((String[i] < '0') or (String[i] > '9')) and (String[i] != '.') and (String[i] != '-')) is_number = false;
      if (String[i] IS '.') is_float = true;
   }

   if ((is_float) and (is_number)) return 'f';
   else if (is_number) return 'i';
   else return 's';
}

//********************************************************************************************************************

static CSTRING load_include_constant(lua_State *Lua, CSTRING Line, CSTRING Source)
{
   pf::Log log("load_include");

   LONG i;
   for (i=0; (unsigned(Line[i]) > 0x20) and (Line[i] != ':'); i++);

   if (Line[i] != ':') {
      log.warning("Malformed const name in %s.", Source);
      return next_line(Line);
   }

   std::string prefix(Line, i);
   prefix.reserve(200);

   Line += i + 1;

   if (!prefix.empty()) prefix += '_';
   auto append_from = prefix.size();

   while (*Line > 0x20) {
      LONG n;
      for (n=0; (Line[n] > 0x20) and (Line[n] != '='); n++);

      if (Line[n] != '=') {
         log.warning("Malformed const definition, expected '=' after name '%s'", prefix.c_str());
         break;
      }

      prefix.erase(append_from);
      prefix.append(Line, n);
      Line += n + 1;

      for (n=0; (Line[n] > 0x20) and (Line[n] != ','); n++);
      std::string value(Line, n);
      Line += n;

      if (n > 0) {
         auto dt = datatype(value);
         if (dt IS 'i') {
            lua_pushinteger(Lua, strtoll(value.c_str(), NULL, 0));
         }
         else if (dt IS 'f') {
            lua_pushnumber(Lua, strtod(value.c_str(), NULL));
         }
         else if (dt IS 'h') {
            lua_pushnumber(Lua, strtoull(value.c_str(), NULL, 0)); // Using pushnumber() so that 64-bit hex is supported.
         }
         else if (value[0] IS '\"') {
            if (value[n-1] IS '\"') lua_pushlstring(Lua, value.c_str()+1, n-2);
            else lua_pushlstring(Lua, value.c_str(), n);
         }
         else lua_pushlstring(Lua, value.c_str(), n);

         lua_setglobal(Lua, prefix.c_str());
      }

      if (*Line IS ',') Line++;
   }

   return next_line(Line);
}

//********************************************************************************************************************
// Bytecode read & write callbacks.  Returning 1 will stop processing.

int code_writer_id(lua_State *Lua, CPTR Data, size_t Size, void *FileID)
{
   pf::Log log("code_writer");

   if (Size <= 0) return 0; // Ignore bad size requests

   if (acWrite((OBJECTID)(MAXINT)FileID, (APTR)Data, Size) IS ERR::Okay) {
      return 0;
   }
   else {
      log.warning("Failed writing %d bytes.", (LONG)Size);
      return 1;
   }
}

int code_writer(lua_State *Lua, CPTR Data, size_t Size, OBJECTPTR File)
{
   pf::Log log(__FUNCTION__);

   if (Size <= 0) return 0; // Ignore bad size requests

   LONG result;
   if (acWrite(File, (APTR)Data, Size, &result) IS ERR::Okay) {
      if ((size_t)result != Size) {
         log.warning("Wrote %d bytes instead of %d.", result, (int)Size);
         return 1;
      }
      else return 0;
   }
   else {
      log.warning("Failed writing %d bytes.", (int)Size);
      return 1;
   }
}

//********************************************************************************************************************
// Callback for lua_load() to read data from File objects.

CSTRING code_reader(lua_State *Lua, void *Handle, size_t *Size)
{
   auto handle = (code_reader_handle *)Handle;
   LONG result;
   if (acRead(handle->File, handle->Buffer, SIZE_READ, &result) IS ERR::Okay) {
      *Size = result;
      return (CSTRING)handle->Buffer;
   }
   else return NULL;
}

//********************************************************************************************************************

#ifdef _DEBUG

static void stack_dump(lua_State *L) __attribute__ ((unused));

static void stack_dump(lua_State *L)
{
   int i;
   int top = lua_gettop(L);
   for (i=1; i <= top; i++) {  // repeat for each level
      int t = lua_type(L, i);
      switch (t) {
         case LUA_TSTRING:  printf("'%s'", lua_tostring(L, i)); break;
         case LUA_TBOOLEAN: printf(lua_toboolean(L, i) ? "true" : "false"); break;
         case LUA_TNUMBER:  printf("%g", lua_tonumber(L, i)); break;
         default:           printf("%s", lua_typename(L, t)); break;
      }
      printf("  ");  // put a separator
   }
   printf("\n");  // end the listing
}

#endif

//********************************************************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, MOD_IDL, NULL)
extern "C" struct ModHeader * register_fluid_module() { return &ModHeader; }
