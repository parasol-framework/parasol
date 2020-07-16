/*****************************************************************************

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

******************************************************************************

-MODULE-
Fluid: Fluid is a customised scripting language for the Script class.

Fluid is a custom scripting language for Parasol developers.  It is implemented on the backbone of LuaJIT, a
high performance version of the Lua scripting language.  It supports garbage collection, dynamic typing and a byte-code
interpreter for compiled code.  We chose to support Lua due to its extensive popularity amongst game developers, a
testament to its low overhead, speed and lightweight processing when compared to common scripting languages.

Fluid files use the file extensions .lua and .fluid.  Ideally, scripts should start with the comment '-- $FLUID' near
the start of the document so that it can be correctly identified by the Fluid class.

For more information on the Fluid syntax, please refer to the official Fluid Reference Manual.

-END-

*****************************************************************************/

//#define DEBUG

#define RMSG(a...) //MSG(a) // Enable if you want to debug results returned from functions, actions etc

#define PRV_SCRIPT
#define PRV_FLUID
#define PRV_FLUID_MODULE
#include <parasol/main.h>
#include <parasol/modules/document.h>
#include <parasol/modules/xml.h>
#include <parasol/modules/window.h>
#include <parasol/modules/display.h>
#include <parasol/modules/fluid.h>

#include <inttypes.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lj_obj.h"

#include "hashes.h"

#define SIZE_READ 1024

#define VER_FLUID 1.0

MODULE_COREBASE;
static struct DisplayBase *DisplayBase;
//static struct SurfaceBase *SurfaceBase;
static OBJECTPTR modDisplay = NULL; // Required by fluid_input.c
static OBJECTPTR modFluid = NULL;
//static OBJECTPTR modSurface = NULL; // Required by fluid_widget.c
static OBJECTPTR clFluid = NULL;
static struct ActionTable *glActions = NULL;
static UBYTE glLocale[4] = "eng";
static struct KeyStore *glActionLookup = NULL;

#include "defs.h"

static CSTRING load_include_struct(lua_State *, CSTRING, CSTRING);
static CSTRING load_include_constant(lua_State *, CSTRING, CSTRING);

//****************************************************************************

FDEF argsSetVariable[] = { { "Error", FD_ERROR }, { "Script", FD_OBJECTPTR }, { "Name", FD_STR }, { "Type", FD_LONG }, { "Variable", FD_TAGS }, { 0, 0 } };

// These test calls are used to check that the dynamic assembler function calls are working as expected.

#ifdef DEBUG
static void flTestCall1(void);
static LONG flTestCall2(void);
static STRING flTestCall3(void);
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
   { flSetVariable, "SetVariable", argsSetVariable },
   #ifdef DEBUG
   { flTestCall1,   "TestCall1", argsTestCall1 },
   { flTestCall2,   "TestCall2", argsTestCall2 },
   { flTestCall3,   "TestCall3", argsTestCall3 },
   { flTestCall4,   "TestCall4", argsTestCall4 },
   { flTestCall5,   "TestCall5", argsTestCall5 },
   { flTestCall6,   "TestCall6", argsTestCall6 },
   { flTestCall7,   "TestCall7", argsTestCall7 },
   #endif
   { NULL, NULL, NULL }
};

#ifdef DEBUG

static void flTestCall1(void)
{
   LogF("TestCall1","No parameters.");
}

static LONG flTestCall2(void)
{
   LogF("TestCall2","Returning 0xdedbeef / %d", 0xdedbeef);
   return 0xdedbeef;
}

static STRING flTestCall3(void)
{
   LogF("TestCall3","Returning 'hello world'");
   return "hello world";
}

static void flTestCall4(LONG Long, LARGE Large)
{
   LogF("TestCall4","Received long %d / $%.8x", Long, Long);
   LogF("TestCall4","Received large " PF64() " / $%.8x%.8x", Large, (ULONG)Large, (ULONG)(Large>>32));
}

static LONG flTestCall5(LONG LongA, LONG LongB, LONG LongC, LONG LongD, LONG LongE, LARGE LongF)
{
   LogF("TestCall5","Received ints: %d, %d, %d, %d, %d, " PF64(), LongA, LongB, LongC, LongD, LongE, LongF);
   LogF("TestCall5","Received ints: $%.8x, $%.8x, $%.8x, $%.8x, $%.8x, $%.8x", LongA, LongB, LongC, LongD, LongE, (LONG)LongF);
   return LongF;
}

static LARGE flTestCall6(LONG long1, LARGE large1, LARGE large2, LONG long2, LARGE large3, DOUBLE float1)
{
   LogF("TestCall6","Received %d, " PF64() ", %d, %d, %d", long1, large1, (LONG)large2, (LONG)long2, (LONG)large3);
   LogF("TestCall6","Received double %f", float1);
   LogF("TestCall6","Returning " PF64(), large2);
   return large2;
}

static void flTestCall7(STRING a, STRING b, STRING c)
{
   LogF("TestCall7","Received string pointers %p, %p, %p", a, b, c);
   LogF("TestCall7","As '%s', '%s', '%s'", a, b, c);
}
#endif

//****************************************************************************

static struct references * alloc_references(void)
{
   struct references *list;
   if (!AllocMemory(sizeof(struct references), MEM_DATA|MEM_NO_CLEAR, &list, NULL)) {
      list->Index = 0;
      return list;
   }
   else return NULL;
}

static LONG get_ptr_ref(struct references *Ref, CPTR Address)
{
   LONG i;
   for (i=0; i < Ref->Index; i++) {
      if (Address IS Ref->List[i].Address) return Ref->List[i].Ref;
   }
   return 0;
}

static void set_ptr_ref(struct references *Ref, CPTR Address, LONG Resource)
{
   if (Ref->Index < ARRAYSIZE(Ref->List)-1) {
      LONG i = Ref->Index++;
      Ref->List[i].Address = Address;
      Ref->List[i].Ref = Resource;
   }
}

static void free_references(lua_State *Lua, struct references *Ref)
{
   LONG i;
   for (i=0; i < Ref->Index; i++) {
      luaL_unref(Lua, LUA_REGISTRYINDEX, Ref->List[i].Ref);
   }
   FreeResource(Ref);
}

//****************************************************************************

static APTR get_meta(lua_State *Lua, LONG Arg, STRING MetaTable)
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

INLINE CSTRING check_bom(CSTRING Value)
{
   if (((UBYTE)Value[0] IS 0xef) AND ((UBYTE)Value[1] IS 0xbb) AND ((UBYTE)Value[2] IS 0xbf)) Value += 3; // UTF-8 BOM
   else if (((UBYTE)Value[0] IS 0xfe) AND ((UBYTE)Value[1] IS 0xff)) Value += 2; // UTF-16 BOM big endian
   else if (((UBYTE)Value[0] IS 0xff) AND ((UBYTE)Value[1] IS 0xfe)) Value += 2; // UTF-16 BOM little endian
   return Value;
}

/*****************************************************************************
** Returns a pointer to an object (if the object exists).
*/

static APTR access_object(struct object *Object)
{
   if (Object->AccessCount) {
      Object->AccessCount++;
      return Object->prvObject;
   }

   if (!Object->ObjectID) return NULL; // Object reference is dead

   if (!Object->prvObject) {
      ERROR error;
      FMSG("access_obj()","Locking object #%d.", Object->ObjectID);
      if (!(error = AccessObject(Object->ObjectID, 5000, &Object->prvObject))) {
         Object->Locked = TRUE;
      }
      else if (error IS ERR_DoesNotExist) {
         FMSG("access_obj","Object #%d has been terminated.", Object->ObjectID);
         Object->prvObject = NULL;
         Object->ObjectID = 0;
      }
   }
   else if (CheckObjectExists(Object->ObjectID, NULL) != ERR_True) {
      FMSG("access_obj()","Object #%d has been terminated.", Object->ObjectID);
      Object->prvObject = NULL;
      Object->ObjectID = 0;
   }

   if (Object->prvObject) Object->AccessCount++;
   return Object->prvObject;
}

static void release_object(struct object *Object)
{
   FMSG("release_obj()","#%d Current Locked: %d, Accesses: %d", Object->ObjectID, Object->Locked, Object->AccessCount);

   if (Object->AccessCount > 0) {
      Object->AccessCount--;
      if (!Object->AccessCount) {
         if (Object->Locked) {
            ReleaseObject(Object->prvObject);
            Object->Locked = FALSE;
            Object->prvObject = NULL;
         }
      }
   }
}

//****************************************************************************
// Build the Includes keystore for the active Lua state.  Content is based on glIncludes (dir scan of config:defs/)

INLINE struct KeyStore * GET_INCLUDES(objScript *Script)
{
   struct prvFluid *prv = Script->Head.ChildPrivate;
   if (!prv->Includes) prv->Includes = VarNew(64, 0);
   return prv->Includes;
}

//****************************************************************************
// Automatically load the include file for the given metaclass, if it has not been loaded already.

static void auto_load_include(lua_State *Lua, objMetaClass *MetaClass)
{
   CSTRING module_name;
   ERROR error;
   if (!(error = GetString(MetaClass, FID_Module, (STRING *)&module_name))) {
      FMSG("auto_load_include()","Class: %s, Module: %s", MetaClass->ClassName, module_name);

      LONG *current_state;
      struct KeyStore *inc = GET_INCLUDES(Lua->Script);
      if ((VarGet(inc, module_name, &current_state, NULL) != ERR_Okay) OR (current_state[0] != 1)) {
         LONG new_state = 1;
         VarSet(inc, module_name, &new_state, sizeof(new_state)); // Mark the module as processed.

         CSTRING idl;
         if ((!(error = GetString(MetaClass, FID_IDL, (STRING *)&idl))) AND (idl)) {
            MSG("Parsing IDL for module %s", module_name);

            while ((idl) AND (*idl)) {
               if ((idl[0] IS 's') AND (idl[1] IS '.')) idl = load_include_struct(Lua, idl+2, module_name);
               else if ((idl[0] IS 'c') AND (idl[1] IS '.')) idl = load_include_constant(Lua, idl+2, module_name);
               else idl = StrNextLine(idl);
            }
         }
         else FMSG("auto_load_include","No IDL defined for %s", module_name);
      }
      else FMSG("auto_load_include","Module %s is marked as loaded.", module_name);
   }
   else LogF("@auto_load_include","Failed to get module name from class '%s', \"%s\"", MetaClass->ClassName, GetErrorMsg(error));
}

//****************************************************************************

static ERROR CMDInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   CoreBase = argCoreBase;

   GetPointer(argModule, FID_Master, &modFluid);

   //if (LoadModule("surface", MODVERSION_SURFACE, &modSurface, &SurfaceBase) != ERR_Okay) return ERR_InitModule;

   ActionList(&glActions, NULL); // Get the global action table from the Core

   // Create a lookup table for converting named actions to IDs.

   if ((glActionLookup = VarNew(0, 0))) {
      ACTIONID action_id;
      for (action_id=1; glActions[action_id].Name; action_id++) {
         VarSet(glActionLookup, glActions[action_id].Name, &action_id, sizeof(ACTIONID));
      }
   }

   // Get the user's language for translation purposes

   CSTRING str;
   if (!StrReadLocale("Language", &str)) {
      StrCopy(str, glLocale, sizeof(glLocale));
   }

   return create_fluid();
}

static ERROR CMDExpunge(void)
{
   if (glActionLookup) { FreeResource(glActionLookup); glActionLookup = NULL; }
   if (clFluid)        { acFree(clFluid); clFluid = NULL; }
   if (modDisplay)     { acFree(modDisplay); modDisplay = NULL; }
   //if (modSurface)     { acFree(modSurface); modSurface = NULL; }
   return ERR_Okay;
}

static ERROR CMDOpen(OBJECTPTR Module)
{
   SetPointer(Module, FID_FunctionList, JumpTableV1);
   return ERR_Okay;
}

/*****************************************************************************

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

*****************************************************************************/

static ERROR flSetVariable(objScript *Script, CSTRING Name, LONG Type, ...)
{
   struct prvFluid *prv;
   va_list list;

   if ((!Script) OR (Script->Head.ClassID != ID_FLUID) OR (!Name) OR (!*Name)) return LogError(ERH_Function, ERR_Args);

   LogF("SetVariable","Script: %d, Name: %s, Type: $%.8x", Script->Head.UniqueID, Name, Type);

   if (!(prv = Script->Head.ChildPrivate)) return LogError(ERH_Function, ERR_ObjectCorrupt);

   va_start(list, Type);

   if (Type & FD_STRING)       lua_pushstring(prv->Lua, va_arg(list, STRING));
   else if (Type & FD_POINTER) lua_pushlightuserdata(prv->Lua, va_arg(list, APTR));
   else if (Type & FD_LONG)    lua_pushinteger(prv->Lua, va_arg(list, LONG));
   else if (Type & FD_LARGE)   lua_pushnumber(prv->Lua, va_arg(list, LARGE));
   else if (Type & FD_DOUBLE)  lua_pushnumber(prv->Lua, va_arg(list, DOUBLE));
   else {
      va_end(list);
      return LogError(ERH_Function, ERR_FieldTypeMismatch);
   }

   lua_setglobal(prv->Lua, Name);

   va_end(list);
   return ERR_Okay;
}

//****************************************************************************

static void hook_debug(lua_State *Lua, lua_Debug *Info)
{
   if (Info->event IS LUA_HOOKCALL) {
      if (lua_getinfo(Lua, "nSl", Info)) {
         if (Info->name) LogF("LuaCall","%s: %s.%s(), Line: %d", Info->what, Info->namewhat, Info->name, Lua->Script->CurrentLine + Lua->Script->LineOffset);
      }
      else LogErrorMsg("lua_getinfo() failed.");
   }
   else if (Info->event IS LUA_HOOKRET) {
      //LogBack();
   }
   else if (Info->event IS LUA_HOOKTAILRET) {
      //LogBack();
   }
   else if (Info->event IS LUA_HOOKLINE) {
      Lua->Script->CurrentLine = Info->currentline - 1; // Our line numbers start from zero
      if (Lua->Script->CurrentLine < 0) Lua->Script->CurrentLine = 0; // Just to be certain :-)
/*
      if (lua_getinfo(Lua, "nSl", Info)) {
         LogF("LuaLine","Line %d: %s: %s", Info->currentline, Info->what, Info->name);
      }
      else LogErrorMsg("lua_getinfo() failed.");
*/
   }
}

//****************************************************************************
// Builds an ordered Lua array from a fixed list of values.  Guaranteed to always return a table, empty or not.
// Works with primitives only, for structs please use make_struct_[ptr|serial]_table() because the struct name
// will be required.

static void make_table(lua_State *Lua, LONG Type, LONG Elements, CPTR Data)
{
   LONG i;

   FMSG("make_table()","Type: $%.8x, Elements: %d, Data: %p", Type, Elements, Data);

   if (Elements < 0) {
      if (!Data) Elements = 0;
      else {
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
            default:         i = 0; break;
         }

         Elements = i;
      }
   }

   lua_createtable(Lua, Elements, 0); // Create a new table on the stack.
   if (!Data) return;

   switch(Type & (FD_DOUBLE|FD_LARGE|FD_FLOAT|FD_POINTER|FD_OBJECT|FD_STRING|FD_LONG|FD_WORD|FD_BYTE)) {
      case FD_STRING:  for (i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); lua_pushstring(Lua, ((CSTRING *)Data)[i]); lua_settable(Lua, -3); } break;
      case FD_OBJECT:  for (i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); push_object(Lua, ((APTR *)Data)[i]); lua_settable(Lua, -3); } break;
      case FD_POINTER: for (i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); lua_pushlightuserdata(Lua, ((APTR *)Data)[i]); lua_settable(Lua, -3); } break;
      case FD_FLOAT:   for (i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); lua_pushnumber(Lua, ((FLOAT *)Data)[i]); lua_settable(Lua, -3); } break;
      case FD_DOUBLE:  for (i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); lua_pushnumber(Lua, ((DOUBLE *)Data)[i]); lua_settable(Lua, -3); } break;
      case FD_LARGE:   for (i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); lua_pushnumber(Lua, ((LARGE *)Data)[i]); lua_settable(Lua, -3); } break;
      case FD_LONG:    for (i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); lua_pushinteger(Lua, ((LONG *)Data)[i]); lua_settable(Lua, -3); } break;
      case FD_WORD:    for (i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); lua_pushinteger(Lua, ((WORD *)Data)[i]); lua_settable(Lua, -3); } break;
      case FD_BYTE:    for (i=0; i < Elements; i++) { lua_pushinteger(Lua, i+1); lua_pushinteger(Lua, ((BYTE *)Data)[i]); lua_settable(Lua, -3); } break;
   }
}

//****************************************************************************
// Create a Lua array from a list of structure pointers.

static void make_struct_ptr_table(lua_State *Lua, CSTRING StructName, LONG Elements, CPTR *Values)
{
   FMSG("make_struct_ptr_table()","%s, Elements: %d, Values: %p", StructName, Elements, Values);

   if (Elements < 0) {
      LONG i;
      for (i=0; Values[i]; i++);
      Elements = i;
   }

   lua_createtable(Lua, Elements, 0); // Create a new table on the stack.
   if (!Values) return;

   struct prvFluid *prv = Lua->Script->Head.ChildPrivate;
   struct structentry *def;

   if (!KeyGet(prv->Structs, STRUCTHASH(StructName), &def, NULL)) {
      LONG i;
      struct references *ref;
      if ((ref = alloc_references())) {
         for (i=0; i < Elements; i++) {
            lua_pushinteger(Lua, i+1);
            if (struct_to_table(Lua, ref, def, Values[i]) != ERR_Okay) lua_pushnil(Lua);
            lua_settable(Lua, -3);
         }
         free_references(Lua, ref);
      }
   }
   else LogErrorMsg("Failed to find struct '%s'", StructName);
}

//****************************************************************************
// Create a Lua array from a serialised list of structures.

static void make_struct_serial_table(lua_State *Lua, CSTRING StructName, LONG Elements, CPTR Data)
{
   FMSG("make_struct_serial_table()","%s, Elements: %d, Values: %p", StructName, Elements, Data);

   if (Elements < 0) Elements = 0; // The total number of structs is a hard requirement.

   lua_createtable(Lua, Elements, 0); // Create a new table on the stack.
   if (!Data) return;

   struct prvFluid *prv = Lua->Script->Head.ChildPrivate;
   struct structentry *def;

   if (!KeyGet(prv->Structs, STRUCTHASH(StructName), &def, NULL)) {
      LONG i;
      struct references *ref;
      if ((ref = alloc_references())) {
         for (i=0; i < Elements; i++) {
            lua_pushinteger(Lua, i+1);
            if (struct_to_table(Lua, ref, def, Data) != ERR_Okay) lua_pushnil(Lua);
            #ifdef _LP64
               Data += ALIGN64(def->Size);
            #else
               Data += def->Size;
            #endif
            lua_settable(Lua, -3);
         }
         free_references(Lua, ref);
      }
   }
   else LogErrorMsg("Failed to find struct '%s'", StructName);
}

//****************************************************************************
// The TypeName can be in the format 'Struct:Arg' without causing any issues.

static void make_any_table(lua_State *Lua, LONG Type, CSTRING TypeName, LONG Elements, CPTR Values)
{
   if (Type & FD_STRUCT) {
      if (Type & FD_POINTER) make_struct_ptr_table(Lua, TypeName, Elements, (CPTR *)Values);
      else make_struct_serial_table(Lua, TypeName, Elements, Values);
   }
   else make_table(Lua, Type, Elements, Values);
}

//****************************************************************************

static void get_line(objScript *Self, LONG Line, STRING Buffer, LONG Size)
{
   CSTRING str;

   if ((str = Self->String)) {
      LONG i;
      for (i=0; i < Line; i++) {
         if (!(str = StrNextLine(str))) {
            Buffer[0] = 0;
            return;
         }
      }

      while ((*str IS ' ') OR (*str IS '\t')) str++;

      for (i=0; i < Size-1; i++) {
         if ((*str IS '\n') OR (*str IS '\r') OR (!*str)) break;
         Buffer[i] = *str++;
      }
      Buffer[i] = 0;
   }
   else Buffer[0] = 0;
}

//****************************************************************************

static ERROR load_include(objScript *Script, CSTRING IncName)
{
   LogF("~load_include()","Definition: %s", IncName);

   struct prvFluid *prv = Script->Head.ChildPrivate;

   // For security purposes, check the validity of the include name.

   LONG i;
   for (i=0; IncName[i]; i++) {
      if ((IncName[i] >= 'a') AND (IncName[i] <= 'z')) continue;
      if ((IncName[i] >= 'A') AND (IncName[i] <= 'Z')) continue;
      if ((IncName[i] >= '0') AND (IncName[i] <= '9')) continue;
      break;
   }

   if ((IncName[i]) OR (i >= 32)) {
      LogF("load_include","Invalid module name; only alpha-numeric names are permitted with max 32 chars.");
      LogBack();
      return ERR_Syntax;
   }

   // Check that the include file hasn't already been processed.

   struct KeyStore *inc = GET_INCLUDES(Script);

   {
      LONG *state;
      if ((!VarGet(inc, IncName, &state, NULL)) AND (state[0] == 1)) {
         FMSG("load_include","Include file '%s' has already been loaded.", IncName);
         LogBack();
         return ERR_Okay;
      }
   }

   ERROR error = ERR_Okay;

   AdjustLogLevel(1);

      if (!StrMatch("core", IncName)) { // The Core module's IDL is accessible from the RES_CORE_IDL resource.
         CSTRING idl;
         if ((idl = GetResourcePtr(RES_CORE_IDL))) {
            while ((idl) AND (*idl)) {
               if ((idl[0] IS 's') AND (idl[1] IS '.')) idl = load_include_struct(prv->Lua, idl+2, IncName);
               else if ((idl[0] IS 'c') AND (idl[1] IS '.')) idl = load_include_constant(prv->Lua, idl+2, IncName);
               else idl = StrNextLine(idl);
            }

            LONG state = 1;
            VarSet(inc, IncName, &state, sizeof(state)); // Mark the file as loaded.
         }
         else error = ERR_Failed;
      }
      else { // The IDL for standard modules is retrievable from the IDL string of a loaded module object.
         OBJECTPTR module;
         if (!CreateObject(ID_MODULE, NF_INTEGRAL, &module, FID_Name|TSTR, IncName, TAGEND)) {
            CSTRING idl;
            if ((!(error = GetString(module, FID_IDL, (STRING *)&idl))) AND (idl)) {
               while ((idl) AND (*idl)) {
                  if ((idl[0] IS 's') AND (idl[1] IS '.')) idl = load_include_struct(prv->Lua, idl+2, IncName);
                  else if ((idl[0] IS 'c') AND (idl[1] IS '.')) idl = load_include_constant(prv->Lua, idl+2, IncName);
                  else idl = StrNextLine(idl);
               }

               LONG state = 1;
               VarSet(inc, IncName, &state, sizeof(state)); // Mark the file as loaded.
            }
            else LogErrorMsg("No IDL for module %s", IncName);

            acFree(module);
         }
         else error = ERR_CreateObject;
      }

   AdjustLogLevel(-1);

   LogBack();
   return error;
}

//****************************************************************************
// Format: s.Name:typeField,...

static CSTRING load_include_struct(lua_State *Lua, CSTRING Line, CSTRING Source)
{
   char name[80];
   LONG i;
   for (i=0; (Line[i] >= 0x20) AND (Line[i] != ':') AND (i < sizeof(name)-1); i++) name[i] = Line[i];
   name[i] = 0;

   if (Line[i] IS ':') {
      Line += i + 1;

      LONG j;
      for (j=0; (Line[j] != '\n') AND (Line[j] != '\r') AND (Line[j]); j++);

      if ((Line[j] IS '\n') OR (Line[j] IS '\r')) {
         char linebuf[j + 1];
         CopyMemory(Line, linebuf, j);
         linebuf[j] = 0;
         make_struct(Lua, name, linebuf);
         while ((Line[j] IS '\n') OR (Line[j] IS '\r')) j++;
         return Line + j;
      }
      else {
         make_struct(Lua, name, Line);
         return Line + j;
      }
   }
   else {
      LogErrorMsg("Malformed struct name in %s.", Source);
      return StrNextLine(Line);
   }
}

//****************************************************************************

static CSTRING load_include_constant(lua_State *Lua, CSTRING Line, CSTRING Source)
{
   char name[80];
   LONG i;
   for (i=0; (*Line > 0x20) AND (*Line != ':') AND (i < sizeof(name)-1); i++) name[i] = *Line++;

   if (*Line != ':') {
      LogErrorMsg("Malformed const name in %s.", Source);
      return StrNextLine(Line);
   }
   else Line++;

   LONG p;
   if (name[0]) {
      name[i++] = '_';
      p = i;
   }
   else p = 0;

   while (*Line > 0x20) {
      LONG n;
      for (n=0; (*Line > 0x20) AND (*Line != '=') AND (p+n < sizeof(name)-1); n++) name[p+n] = *Line++;
      if (p+n >= sizeof(name)-1) {
         LogErrorMsg("The constant name '%s' in '%s' is too long.", name, Source);
         break;
      }
      name[p+n] = 0;

      if (Line[0] != '=') {
         LogErrorMsg("Malformed const definition, expected '=' after name '%s'", name);
         break;
      }
      Line++;

      char value[200];
      for (n=0; (n < sizeof(value)-1) AND (*Line > 0x20) AND (*Line != ','); n++) value[n] = *Line++;
      value[n] = 0;

      if (n > 0) {
         LONG dt = StrDatatype(value);
         if (dt IS STT_NUMBER) {
            lua_pushinteger(Lua, StrToInt(value));
         }
         else if (dt IS STT_FLOAT) {
            lua_pushnumber(Lua, StrToFloat(value));
         }
         else if (dt IS STT_HEX) {
            lua_pushnumber(Lua, StrToHex(value)); // Using pushnumber() so that 64-bit hex is supported.
         }
         else if (value[0] IS '\"') {
            if (value[n-1] IS '\"') lua_pushlstring(Lua, value+1, n-2);
            else lua_pushlstring(Lua, value, n);
         }
         else lua_pushlstring(Lua, value, n);

         lua_setglobal(Lua, name);
      }

      if (*Line IS ',') Line++;
   }

   return StrNextLine(Line);
}

/*****************************************************************************
** Bytecode read & write callbacks.  Returning 1 will stop processing.
*/

static int code_writer_id(lua_State *Lua, CPTR Data, size_t Size, void *FileID)
{
   if (Size <= 0) return 0; // Ignore bad size requests

   if (!acWriteID((OBJECTID)(MAXINT)FileID, (APTR)Data, Size)) {
      return 0;
   }
   else {
      LogErrorMsg("Failed writing %d bytes.", (int)Size);
      return 1;
   }
}

static int code_writer(lua_State *Lua, CPTR Data, size_t Size, void *File)
{
   if (Size <= 0) return 0; // Ignore bad size requests

   LONG result;
   if (!acWrite(File, (APTR)Data, Size, &result)) {
      if (result != Size) {
         LogErrorMsg("Wrote %d bytes instead of %d.", result, (int)Size);
         return 1;
      }
      else return 0;
   }
   else {
      LogErrorMsg("Failed writing %d bytes.", (int)Size);
      return 1;
   }
}

//****************************************************************************
// Callback for lua_load() to read data from File objects.

static const char * code_reader(lua_State *Lua, void *Handle, size_t *Size)
{
   struct code_reader_handle *handle = Handle;
   LONG result;
   if (!acRead(handle->File, handle->Buffer, SIZE_READ, &result)) {
      *Size = result;
      return handle->Buffer;
   }
   else return NULL;
}

//****************************************************************************

#ifdef DEBUG

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

//****************************************************************************

#include "fluid_array.c"
#include "fluid_class.c"
#include "fluid_input.c"
#include "fluid_objects.c"
#include "fluid_number.c"
#include "fluid_struct.c"
#include "fluid_thread.c"
#include "fluid_module.c"
#include "fluid_functions.c"
//#include "fluid_widget.c"

//****************************************************************************

PARASOL_MOD(CMDInit, NULL, CMDOpen, CMDExpunge, VER_FLUID)
