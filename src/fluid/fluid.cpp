/*********************************************************************************************************************

This source code is placed in the public domain under no warranty from its authors.

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
#include <parasol/modules/regex.h>
#include <parasol/strings.hpp>

#include <inttypes.h>
#include <vector>
#include <iterator>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lj_obj.h"
#include "parser/parser.h"
#include "lj_bc.h"
#include "lj_array.h"
#include "lj_gc.h"

#include "hashes.h"

JUMPTABLE_CORE
JUMPTABLE_REGEX

#include "defs.h"

constexpr size_t MAX_MODULE_NAME_LENGTH = 32;
constexpr size_t MAX_STRING_PREFIX_LENGTH = 200;

OBJECTPTR modDisplay = nullptr; // Required by fluid_input.c
OBJECTPTR modFluid = nullptr;
OBJECTPTR modRegex = nullptr;
OBJECTPTR clFluid = nullptr;
OBJECTPTR glFluidContext = nullptr;
struct ActionTable *glActions = nullptr;
bool glPrintMsg = false;
JOF glJitOptions = JOF::NIL;
ankerl::unordered_dense::map<std::string_view, ACTIONID, CaseInsensitiveHashView, CaseInsensitiveEqualView> glActionLookup;
ankerl::unordered_dense::map<std::string_view, uint32_t> glStructSizes;

static struct MsgHandler *glMsgThread = nullptr; // Message handler for thread callbacks

[[nodiscard]] static CSTRING load_include_struct(lua_State *, CSTRING, CSTRING);
[[nodiscard]] static CSTRING load_include_constant(lua_State *, CSTRING, CSTRING);

constexpr auto HASH_TRACE_TOKENS         = pf::strhash("trace-tokens");
constexpr auto HASH_TRACE_EXPECT         = pf::strhash("trace-expect");
constexpr auto HASH_TRACE_BOUNDARY       = pf::strhash("trace-boundary");
constexpr auto HASH_TRACE_OPERATORS      = pf::strhash("trace-operators");
constexpr auto HASH_TRACE_REGISTERS      = pf::strhash("trace-registers");
constexpr auto HASH_TRACE_CFG            = pf::strhash("trace-cfg");
constexpr auto HASH_TRACE_ASSIGNMENTS    = pf::strhash("trace-assignments");
constexpr auto HASH_TRACE_VALUE_CATEGORY = pf::strhash("trace-value-category");
constexpr auto HASH_TRACE_TYPES          = pf::strhash("trace-types");
constexpr auto HASH_DIAGNOSE             = pf::strhash("diagnose");
constexpr auto HASH_DUMP_BYTECODE        = pf::strhash("dump-bytecode");
constexpr auto HASH_PROFILE              = pf::strhash("profile");
constexpr auto HASH_TRACE                = pf::strhash("trace");
constexpr auto HASH_TOP_TIPS             = pf::strhash("top-tips");
constexpr auto HASH_TIPS                 = pf::strhash("tips");
constexpr auto HASH_ALL_TIPS             = pf::strhash("all-tips");

#include "module_def.cpp"

//********************************************************************************************************************

APTR get_meta(lua_State *Lua, int Arg, CSTRING MetaTable)
{
   if (auto address = (struct object *)lua_touserdata(Lua, Arg)) {
      if (lua_getmetatable(Lua, Arg)) {  // does it have a metatable?
         lua_getfield(Lua, LUA_REGISTRYINDEX, MetaTable);  // get correct metatable
         if (lua_rawequal(Lua, -1, -2)) {  // does it have the correct mt?
            lua_pop(Lua, 2);
            return address;
         }
         lua_pop(Lua, 2);
      }
   }

   return nullptr;
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
   else if (not Object->UID) return nullptr; // Object reference is dead
   else if ((!Object->ObjectPtr) or (Object->Detached)) {
      // Detached objects are always accessed via UID, even if we have a pointer reference.
      if (auto error = AccessObject(Object->UID, 5000, &Object->ObjectPtr); error IS ERR::Okay) {
         Object->Locked = true;
      }
      else if (error IS ERR::DoesNotExist) {
         pf::Log log(__FUNCTION__);
         log.trace("Object #%d has been terminated.", Object->UID);
         Object->ObjectPtr = nullptr;
         Object->UID = 0;
      }
   }
   else Object->ObjectPtr->lock(); // 'soft' lock in case of threading involving private objects

   if (Object->ObjectPtr) Object->AccessCount++;
   return Object->ObjectPtr;
}

void release_object(struct object *Object)
{
   if (Object->AccessCount > 0) {
      if (--Object->AccessCount IS 0) {
         if (Object->Locked) {
            ReleaseObject(Object->ObjectPtr);
            Object->Locked = false;
            Object->ObjectPtr = nullptr;
         }
         else Object->ObjectPtr->unlock();
      }
   }
}

//********************************************************************************************************************
// Automatically load the include file for the given metaclass, if it has not been loaded already.

void auto_load_include(lua_State *Lua, objMetaClass *MetaClass)
{
   pf::Log log(__FUNCTION__);

   // Ensure that the base-class is loaded first, if applicable
   if (MetaClass->BaseClassID != MetaClass->ClassID) {
      if (auto base_class = FindClass(MetaClass->BaseClassID)) {
         auto_load_include(Lua, base_class);
      }
   }

   CSTRING module_name;
   if (auto error = MetaClass->get(FID_Module, module_name); error IS ERR::Okay) {
      log.trace("Class: %s, Module: %s", MetaClass->ClassName, module_name);

      auto prv = (prvFluid *)Lua->script->ChildPrivate;
      if (not prv->Includes.contains(module_name)) {
         prv->Includes.insert(module_name); // Mark the module as processed.

         OBJECTPTR mod;
         if ((error = MetaClass->get(FID_RootModule, mod)) IS ERR::Okay) {
            struct ModHeader *header;

            if (((error = mod->get(FID_Header, header)) IS ERR::Okay) and (header)) {
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

[[nodiscard]] static ERR MODInit(OBJECTPTR argModule, struct CoreBase *argCoreBase)
{
   pf::Log log;

   CoreBase = argCoreBase;

   glFluidContext = CurrentContext();
   glPrintMsg = GetResource(RES::LOG_LEVEL) >= 4;

   argModule->get(FID_Root, modFluid);

   ActionList(&glActions, nullptr); // Get the global action table from the Core

   // Create a lookup table for converting named actions to IDs.

   for (int action_id=1; glActions[action_id].Name; action_id++) {
      glActionLookup[glActions[action_id].Name] = AC(action_id);
   }

   FUNCTION call(CALL::STD_C);
   call.Routine = (APTR)msg_thread_script_callback;
   AddMsgHandler(MSGID::FLUID_THREAD_CALLBACK, &call, &glMsgThread);

   pf::vector<std::string> *pargs;
   auto task = CurrentTask();
   if ((task->get(FID_Parameters, pargs) IS ERR::Okay) and (pargs)) {
      pf::vector<std::string> &args = *pargs;
      for (int i=0; i < std::ssize(args); i++) {
         if (pf::startswith(args[i], "--jit-options")) {
            // Parse --jit-options [csv] parameter
            // Use in conjunction with --log-api to see the log messages.
            // These options are system-wide, alternatively you can set JitOptions in the Script object.
            std::string value;

            if (i + 1 < std::ssize(args)) {
               value = args[i + 1];
               i++;
            }

            if (not value.empty()) {
               // Split the CSV string and set appropriate global variables
               std::vector<std::string> options;
               pf::split(value, std::back_inserter(options), ',');

               glJitOptions = JOF::NIL;
               for (const auto &option : options) {
                  std::string trimmed = option;
                  pf::trim(trimmed);

                  auto hash = pf::strhash(trimmed);
                  if (hash IS HASH_TRACE_VALUE_CATEGORY)   glJitOptions |= JOF::TRACE_VALUE_CATEGORY;
                  else if (hash IS HASH_TRACE_ASSIGNMENTS) glJitOptions |= JOF::TRACE_ASSIGNMENTS;
                  else if (hash IS HASH_TRACE_OPERATORS) glJitOptions |= JOF::TRACE_OPERATORS;
                  else if (hash IS HASH_TRACE_REGISTERS) glJitOptions |= JOF::TRACE_REGISTERS;
                  else if (hash IS HASH_TRACE_BOUNDARY) glJitOptions |= JOF::TRACE_BOUNDARY;
                  else if (hash IS HASH_TRACE_TOKENS)  glJitOptions |= JOF::TRACE_TOKENS;
                  else if (hash IS HASH_TRACE_EXPECT)  glJitOptions |= JOF::TRACE_EXPECT;
                  else if (hash IS HASH_TRACE_CFG)     glJitOptions |= JOF::TRACE_CFG;
                  else if (hash IS HASH_TRACE_TYPES)   glJitOptions |= JOF::TRACE_TYPES;
                  else if (hash IS HASH_DIAGNOSE)      glJitOptions |= JOF::DIAGNOSE;
                  else if (hash IS HASH_DUMP_BYTECODE) glJitOptions |= JOF::DUMP_BYTECODE;
                  else if (hash IS HASH_PROFILE)       glJitOptions |= JOF::PROFILE;
                  else if (hash IS HASH_TRACE)         glJitOptions |= JOF::TRACE;
                  else if (hash IS HASH_TOP_TIPS)      glJitOptions |= JOF::TOP_TIPS;
                  else if (hash IS HASH_TIPS)          glJitOptions |= JOF::TIPS;
                  else if (hash IS HASH_ALL_TIPS)      glJitOptions |= JOF::ALL_TIPS;
                  else log.warning("Unknown JIT option \"%s\" specified.", trimmed.c_str());
               }

               log.msg("JIT options \"%s\" set to $%.8x", value.c_str(), (uint32_t)glJitOptions);

               if ((glJitOptions & (JOF::TRACE|JOF::PROFILE)) != JOF::NIL) {
                  if (GetResource(RES::LOG_LEVEL) < 5) {
                     // Automatically raise the log level to see JIT messages.  Helpful for AI
                     // agents that forget this requirement.
                     SetResource(RES::LOG_LEVEL, 5);
                  }
               }
            }
            else log.warning("No value for --jit-options");
         }
      }
   }

   return create_fluid();
}

static ERR MODExpunge(void)
{
   if (glMsgThread) { FreeResource(glMsgThread); glMsgThread = nullptr; }
   if (clFluid)     { FreeResource(clFluid); clFluid = nullptr; }
   if (modDisplay)  { FreeResource(modDisplay); modDisplay = nullptr; }
   if (modRegex)    { FreeResource(modRegex); modRegex = nullptr; }
   return ERR::Okay;
}

//********************************************************************************************************************

static ERR MODOpen(OBJECTPTR Module)
{
   Module->set(FID_FunctionList, glFunctions);
   return ERR::Okay;
}

//********************************************************************************************************************

#ifdef ENABLE_UNIT_TESTS
extern void indexing_unit_tests(int &, int &);
extern void vm_asm_unit_tests(int &, int &);
extern void jit_frame_unit_tests(int &, int &);
extern void parser_unit_tests(int &, int &);
extern void array_unit_tests(int &, int &);
#endif

static void MODTest(CSTRING Options, int *Passed, int *Total)
{
#ifdef ENABLE_UNIT_TESTS
   {
      pf::Log log("FluidTests");
      log.branch("Running indexing unit tests...");
      indexing_unit_tests(*Passed, *Total);
   }
   {
      pf::Log log("FluidTests");
      log.branch("Running parser unit tests...");
      parser_unit_tests(*Passed, *Total);
   }
   {
      pf::Log log("FluidTests");
      log.branch("Running VM assembly unit tests...");
      vm_asm_unit_tests(*Passed, *Total);
   }
   {
      pf::Log log("FluidTests");
      log.branch("Running JIT frame unit tests...");
      jit_frame_unit_tests(*Passed, *Total);
   }
   {
      pf::Log log("FluidTests");
      log.branch("Running array unit tests...");
      array_unit_tests(*Passed, *Total);
   }
#else
   pf::Log("FluidTests").warning("Unit tests are disabled in this build.");
#endif
}

//********************************************************************************************************************
// Bytecode names for debugging purposes

CSTRING const glBytecodeNames[] = {
#define BCNAME(name, ma, mb, mc, mt) #name,
   BCDEF(BCNAME)
#undef BCNAME
};

/*********************************************************************************************************************

-FUNCTION-
SetVariable: Sets any variable in a loaded Fluid script.

The SetVariable() function provides a method for setting global variables in a Fluid script prior to execution of that
script.  If the script is cached, the variable settings will be available on the next activation.

-INPUT-
obj(Script) Script: Pointer to a Fluid script.
cstr Name: The name of the variable to set.
int Type: A valid field type must be indicated, e.g. `FD_STRING`, `FD_POINTER`, `FD_INT`, `FD_DOUBLE`, `FD_INT64`.
tags Variable: A variable that matches the indicated `Type`.

-ERRORS-
Okay: The variable was defined successfully.
Args:
FieldTypeMismatch: A valid field type was not specified in the `Type` parameter.
ObjectCorrupt: Privately maintained memory has become inaccessible.
-END-

*********************************************************************************************************************/
namespace fl {
ERR SetVariable(objScript *Script, CSTRING Name, int Type, ...)
{
   pf::Log log(__FUNCTION__);
   prvFluid *prv;
   va_list list;

   if ((!Script) or (Script->classID() != CLASSID::FLUID) or (!Name) or (!*Name)) return log.warning(ERR::Args);

   log.branch("Script: %d, Name: %s, Type: $%.8x", Script->UID, Name, Type);

   if (not (prv = (prvFluid *)Script->ChildPrivate)) return log.warning(ERR::ObjectCorrupt);

   va_start(list, Type);

   if (Type & FD_STRING)       lua_pushstring(prv->Lua, va_arg(list, STRING));
   else if (Type & FD_POINTER) lua_pushlightuserdata(prv->Lua, va_arg(list, APTR));
   else if (Type & FD_INT)     lua_pushinteger(prv->Lua, va_arg(list, int));
   else if (Type & FD_INT64)   lua_pushnumber(prv->Lua, va_arg(list, int64_t));
   else if (Type & FD_DOUBLE)  lua_pushnumber(prv->Lua, va_arg(list, double));
   else {
      va_end(list);
      return log.warning(ERR::FieldTypeMismatch);
   }

   lua_setglobal(prv->Lua, Name);

   va_end(list);
   return ERR::Okay;
}
}
//********************************************************************************************************************

void hook_debug(lua_State *Lua, lua_Debug *Info)
{
   pf::Log log("Lua");

   if (Info->event IS LUA_HOOKCALL) {
      if (lua_getinfo(Lua, "nSl", Info)) {
         if (Info->name) log.msg("%s: %s.%s(), Line: %d", Info->what, Info->namewhat, Info->name, Lua->script->CurrentLine + Lua->script->LineOffset);
      }
      else log.warning("lua_getinfo() failed.");
   }
   else if (Info->event IS LUA_HOOKRET) { }
   else if (Info->event IS LUA_HOOKTAILRET) { }
   else if (Info->event IS LUA_HOOKLINE) {
      Lua->script->CurrentLine = Info->currentline - 1; // Our line numbers start from zero
      if (Lua->script->CurrentLine < 0) Lua->script->CurrentLine = 0; // Just to be certain :-)
/*
      if (lua_getinfo(Lua, "nSl", Info)) {
         log.msg("Line %d: %s: %s", Info->currentline, Info->what, Info->name);
      }
      else log.warning("lua_getinfo() failed.");
*/
   }
}

//********************************************************************************************************************
// Builds an array from a fixed list of values.  Guaranteed to always return an array, empty or not.
// Intended for primitives only, for structs please use make_struct_[ptr|serial]_table() because the struct name
// will be required.

void make_array(lua_State *Lua, AET Type, int Elements, CPTR Data, std::string_view StructName)
{
   pf::Log log(__FUNCTION__);

   log.traceBranch("Type: $%.8x, Elements: %d, Data: %p", int(Type), Elements, Data);

   if (Elements < 0) {
      if (not Data) Elements = 0;
      else {
         int i = 0;
         switch (Type) {
            case AET::CSTR:
            case AET::PTR:
            case AET::OBJECT:
               for (i=0; ((APTR *)Data)[i]; i++);
               break;
            case AET::FLOAT:
            case AET::INT32:
               for (i=0; ((int *)Data)[i]; i++);
               break;
            case AET::DOUBLE:
            case AET::INT64:
               for (i=0; ((int64_t *)Data)[i]; i++);
               break;
            case AET::INT16:
               for (i=0; ((int16_t *)Data)[i]; i++);
               break;
            case AET::BYTE:
               for (i=0; ((int8_t *)Data)[i]; i++);
               break;
            case AET::STRUCT: // Use make_struct_*() interfaces instead
            case AET::STR_GC:
            case AET::STR_CPP:
            default:
               log.warning("Unsupported type $%.8x", int(Type));
               lua_pushnil(Lua);
               return;
         }

         Elements = i;
      }
   }

   // lj_array_new() with ARRAY_CACHED handles all data copying internally, including string caching

   GCarray *array = lj_array_new(Lua, Elements, Type, (void *)Data, ARRAY_CACHED, StructName);

   // Push to the stack
   lj_gc_check(Lua);
   setarrayV(Lua, Lua->top++, array);
}

//********************************************************************************************************************
// Create a Lua array from a list of structure pointers.

void make_struct_ptr_array(lua_State *Lua, std::string_view StructName, int Elements, CPTR *Values)
{
   pf::Log log(__FUNCTION__);

   log.trace("%.*s, Elements: %d, Values: %p", int(StructName.size()), StructName.data(), Elements, Values);

   if (Elements < 0) {
      int i;
      for (i=0; Values[i]; i++);
      Elements = i;
   }

   auto prv = (prvFluid *)Lua->script->ChildPrivate;

   auto s_name = struct_name(StructName);
   if (not prv->Structs.contains(s_name)) luaL_error(Lua, "Failed to find struct '%.*s'", int(StructName.size()), StructName.data());

   GCarray *arr = lj_array_new(Lua, Elements, AET::TABLE);
   setarrayV(Lua, Lua->top++, arr); // Push to stack immediately to protect from GC during loop
   int arr_idx = lua_gettop(Lua);

   if (Values) {
      std::vector<lua_ref> ref;
      auto &sdef = prv->Structs[s_name];

      for (int i=0; i < Elements; i++) {
         if (struct_to_table(Lua, ref, sdef, Values[i]) IS ERR::Okay) {
            // Table is now on top of stack; retrieve arr from stack in case GC moved it
            arr = arrayV(Lua->base + arr_idx - 1);
            TValue *tv = Lua->top - 1;
            GCtab *tab = tabV(tv);
            setgcref(arr->get<GCRef>()[i], obj2gco(tab));
            lj_gc_objbarrier(Lua, arr, tab);
            Lua->top--;  // Pop the table
         }
         else {
            arr = arrayV(Lua->base + arr_idx - 1);
            setgcrefnull(arr->get<GCRef>()[i]);
         }
      }
   }
}

//********************************************************************************************************************
// Create an array from a serialised list of structures aligned to a 64-bit boundary.

void make_struct_serial_array(lua_State *Lua, std::string_view StructName, int Elements, CPTR Input)
{
   pf::Log log(__FUNCTION__);

   if (Elements < 0) Elements = 0; // The total number of structs is a hard requirement.

   auto prv = (prvFluid *)Lua->script->ChildPrivate;

   auto s_name = struct_name(StructName);
   if (not prv->Structs.contains(s_name)) luaL_error(Lua, "Failed to find struct '%.*s'", int(StructName.size()), StructName.data());

   GCarray *arr = lj_array_new(Lua, Elements, AET::TABLE);
   setarrayV(Lua, Lua->top++, arr); // Push to stack immediately to protect from GC during loop
   int arr_idx = lua_gettop(Lua);

   if (Input) {
      std::vector<lua_ref> ref;
      auto &sdef = prv->Structs[s_name];

      // 64-bit compilers don't always align structures to 64-bit, and it's difficult to compute alignment with
      // certainty.  It is essential that structures that are intended to be serialised into arrays are manually
      // padded to 64-bit so that the potential for mishap is eliminated.

      int def_size = ALIGN64(sdef.Size);
      char aligned = ((sdef.Size & 0x7) != 0) ? 'N': 'Y';
      if (aligned IS 'N') {
         log.msg("%.*s, Elements: %d, Values: %p, StructSize: %d, Aligned: %c", int(StructName.size()), StructName.data(), Elements, Input, def_size, aligned);
      }

      for (int i=0; i < Elements; i++) {
         if (struct_to_table(Lua, ref, sdef, Input) IS ERR::Okay) {
            // Table is now on top of stack; retrieve arr from stack in case GC moved it
            arr = arrayV(Lua->base + arr_idx - 1);
            TValue *tv = Lua->top - 1;
            GCtab *tab = tabV(tv);
            setgcref(arr->get<GCRef>()[i], obj2gco(tab));
            lj_gc_objbarrier(Lua, arr, tab);
            Lua->top--;  // Pop the table
         }
         else {
            arr = arrayV(Lua->base + arr_idx - 1);
            setgcrefnull(arr->get<GCRef>()[i]);
         }

         Input = (int8_t *)Input + def_size;
      }
   }
}

//********************************************************************************************************************
// The TypeName can be in the format 'Struct:Arg' without causing any issues.

void make_any_array(lua_State *Lua, int Flags, std::string_view TypeName, int Elements, CPTR Values)
{
   if (Flags & FD_STRUCT) {
      if (Flags & FD_POINTER) make_struct_ptr_array(Lua, TypeName, Elements, (CPTR *)Values);
      else make_struct_serial_array(Lua, TypeName, Elements, Values);
   }
   else make_array(Lua, ff_to_aet(Flags), Elements, Values, TypeName);
}

//********************************************************************************************************************

void get_line(objScript *Self, int Line, STRING Buffer, int Size)
{
   if (CSTRING str = Self->String) {
      int i;
      for (i=0; i < Line; i++) {
         if (not (str = next_line(str))) {
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

[[nodiscard]] ERR load_include(objScript *Script, CSTRING IncName)
{
   pf::Log log(__FUNCTION__);

   log.branch("Definition: %s", IncName);

   auto prv = (prvFluid *)Script->ChildPrivate;

   // For security purposes, check the validity of the include name.

   int i;
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
      log.trace("Include file '%s' already loaded.", IncName);
      return ERR::Okay;
   }

   ERR error = ERR::Okay;

   AdjustLogLevel(1);

      objModule::create module = { fl::Name(IncName) };
      if (module.ok()) {
         prv->Includes.insert(IncName); // Mark the file as loaded.

         OBJECTPTR root;
         if ((error = module->get(FID_Root, root)) IS ERR::Okay) {
            struct ModHeader *header;
            if ((((error = root->get(FID_Header, header)) IS ERR::Okay) and (header))) {
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

   int i;
   for (i=0; (Line[i] >= 0x20) and (Line[i] != ':'); i++);

   if (Line[i] IS ':') {
      std::string name(Line, i);
      Line += i + 1;

      int j;
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

[[nodiscard]] static constexpr int8_t datatype(std::string_view String) noexcept
{
   size_t i = 0;
   while ((i < String.size()) and (String[i] <= 0x20)) i++; // Skip white-space

   if ((String[i] IS '0') and (String[i+1] IS 'x')) {
      for (i+=2; i < String.size(); i++) {
         if (not std::isxdigit(String[i])) return 's';
      }
      return 'h';
   }

   bool is_number = true;
   bool is_float  = false;

   for (; (i < String.size()) and (is_number); i++) {
      if ((!std::isdigit(String[i])) and (String[i] != '.') and (String[i] != '-')) is_number = false;
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

   int i;
   for (i=0; (unsigned(Line[i]) > 0x20) and (Line[i] != ':'); i++);

   if (Line[i] != ':') {
      log.warning("Malformed const name in %s.", Source);
      return next_line(Line);
   }

   std::string prefix(Line, i);
   prefix.reserve(MAX_STRING_PREFIX_LENGTH);

   Line += i + 1;

   if (not prefix.empty()) prefix += '_';
   auto append_from = prefix.size();

   while (*Line > 0x20) {
      int n;
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

      //log.warning("%s = %s", prefix.c_str(), value.c_str());

      if (n > 0) {
         auto dt = datatype(value);
         if (dt IS 'i') lua_pushinteger(Lua, strtoll(value.c_str(), nullptr, 0));
         else if (dt IS 'f') lua_pushnumber(Lua, strtod(value.c_str(), nullptr));
         else if (dt IS 'h') {
            lua_pushnumber(Lua, strtoull(value.c_str(), nullptr, 0)); // Using pushnumber() so that 64-bit hex is supported.
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

   pf::ScopedObjectLock file((MAXINT)FileID);
   if (file.granted()) {
      if (acWrite(*file, (APTR)Data, Size) IS ERR::Okay) return 0;
   }
   log.warning("Failed writing %d bytes.", (int)Size);
   return 1;
}

int code_writer(lua_State *Lua, CPTR Data, size_t Size, OBJECTPTR File)
{
   pf::Log log(__FUNCTION__);

   if (Size <= 0) return 0; // Ignore bad size requests

   int result;
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
   int result;
   if (acRead(handle->File, handle->Buffer, SIZE_READ, &result) IS ERR::Okay) {
      *Size = result;
      return (CSTRING)handle->Buffer;
   }
   else return nullptr;
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

PARASOL_MOD(MODInit, nullptr, MODOpen, MODExpunge, MODTest, MOD_IDL, nullptr)
extern "C" struct ModHeader * register_fluid_module() { return &ModHeader; }
