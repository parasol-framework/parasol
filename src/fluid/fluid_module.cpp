
#define PRV_SCRIPT
#define PRV_FLUID
#define PRV_FLUID_MODULE
#include <parasol/main.h>
#include <parasol/modules/fluid.h>
#include <inttypes.h>

extern "C" {
 #include "lua.h"
 #include "lualib.h"
 #include "lauxlib.h"
 #include "lj_obj.h"
}

#include "hashes.h"
#include "defs.h"

#include <ffi.h>

#undef RMSG
#define RMSG(a...) //log.msg(a)
#define MAX_MODULE_ARGS 16

static int module_call(lua_State *);
static LONG process_results(prvFluid *, APTR, const FunctionField *, LONG);

//********************************************************************************************************************
// Usage: module = mod.load("core")

static int module_load(lua_State *Lua)
{
   CSTRING modname;
   if (!(modname = luaL_checkstring(Lua, 1))) {
      luaL_argerror(Lua, 1, "String expected for module name.");
      return 0;
   }

   pf::Log log(__FUNCTION__);
   log.branch("Module: %s", modname);

   // Check if there is an include file with the same name as this module.

   ERROR error = load_include(Lua->Script, modname);
   if ((error != ERR_Okay) and (error != ERR_FileNotFound)) {
      log.debranch();
      luaL_error(Lua, "Failed to load include file for the %s module.", modname);
      return 0;
   }

   if (auto loaded_mod = objModule::create::global(fl::Name(modname))) {
      auto mod = (module *)lua_newuserdata(Lua, sizeof(module));
      ClearMemory(mod, sizeof(struct module));

      luaL_getmetatable(Lua, "Fluid.mod");
      lua_setmetatable(Lua, -2);

      mod->Module = loaded_mod;
      loaded_mod->getPtr(FID_FunctionList, &mod->Functions);
      return 1;  // new userdatum is already on the stack
   }
   else {
      log.debranch();
      luaL_error(Lua, "Failed to load the %s module.", modname);
      return 0;
   }
}

//********************************************************************************************************************
// Internal: Object garbage collector.

static int module_destruct(lua_State *Lua)
{
   if (auto mod = (module *)luaL_checkudata(Lua, 1, "Fluid.mod")) {
      FreeResource(mod->Module);
   }

   return 0;
}

//********************************************************************************************************************
// Internal: Prints the module name

static int module_tostring(lua_State *Lua)
{
   if (auto mod = (struct module *)luaL_checkudata(Lua, 1, "Fluid.mod")) {
      STRING name;
      if (!mod->Module->get(FID_Name, &name)) {
         lua_pushstring(Lua, name);
      }
      else lua_pushnil(Lua);
   }
   else lua_pushnil(Lua);

   return 1;
}

//********************************************************************************************************************
// Any Read accesses to the module object will pass through here.

static int module_index(lua_State *Lua)
{
   if (auto mod = (module *)luaL_checkudata(Lua, 1, "Fluid.mod")) {
      if (auto function = luaL_checkstring(Lua, 2)) {
         if (auto list = mod->Functions) {
            for (LONG i=0; list[i].Name; i++) {
               CSTRING name = list[i].Name;
               if (!StrMatch(name, function)) { // Function call stack management
                  lua_pushvalue(Lua, 1); // Arg1: Duplicate the module reference
                  lua_pushinteger(Lua, i); // Arg2: Index of the function that is being called
                  lua_pushcclosure(Lua, module_call, 2);
                  return 1;
               }
            }

            luaL_error(Lua, "Call to function %s() not recognised.", function);
         }
         else luaL_error(Lua, "No exported function list for this module.", function);
      }
      else luaL_argerror(Lua, 2, "Expected function string.");
   }
   else luaL_argerror(Lua, 1, "Expected module.");

   return 0;
}

//********************************************************************************************************************

static int module_call(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);
   objScript *Self = Lua->Script;
   UBYTE buffer[256]; // +8 for overflow protection
   LONG i;

   auto prv = (prvFluid *)Self->ChildPrivate;
   if (!prv) {
      log.warning(ERR_ObjectCorrupt);
      return 0;
   }

   auto mod = (module *)get_meta(Lua, lua_upvalueindex(1), "Fluid.mod");
   if (!mod) {
      luaL_error(Lua, "module_call() expected module in upvalue.");
      return 0;
   }

   if (!mod->Functions) return 0;

   auto index = lua_tointeger(Lua, lua_upvalueindex(2));
   LONG nargs = lua_gettop(Lua);
   if (nargs > MAX_MODULE_ARGS-1) nargs = MAX_MODULE_ARGS-1;

   UBYTE *end = buffer + sizeof(buffer);

   log.trace("%s() Index: %d, Args: %d", mod->Functions[index].Name, index, nargs);

   const FunctionField *args;
   if (!(args = mod->Functions[index].Args)) {
      auto function = (void (*)(void))mod->Functions[index].Address;
      function();
      return 0;
   }

   APTR function = mod->Functions[index].Address;
   FUNCTION func = { .Type = 0 };
   ffi_cif cif;
   ffi_arg rc;
   ffi_type *arg_types[MAX_MODULE_ARGS];
   void * arg_values[MAX_MODULE_ARGS];
   LONG in = 0;

   LONG j = 0;
   for (i=1; (args[i].Name) and ((size_t)j < sizeof(buffer)-8); i++) {
      LONG argtype = args[i].Type;

      //log.trace("%s() Arg: %s, Offset: %d, Type: $%.8x (received %s)", mod->Functions[index].Name, args[i].Name, j, argtype, lua_typename(Lua, lua_type(Lua, i)));

      if (argtype & FD_RESULT) {
         // Result arguments are stored in the buffer with a pointer to an empty variable space (stored at the end of
         // the buffer)

         RMSG("Result for arg %d stored at %p", i, end);

         if (argtype & FD_BUFFER) {
            // The client must supply an argument that will store a buffer result.  This is a different case to the
            // storage of type values.  Buffers can be combined with FD_ARRAY to store more than one element.

            if (argtype & FD_CPP) {
               luaL_error(Lua, "No support for calls utilising C++ arrays.");
               return 0;
            }

            if (auto mem = (array *)get_meta(Lua, i, "Fluid.array")) {
               ((APTR *)(buffer + j))[0] = mem->ptrVoid;
               arg_values[in] = buffer + j;
               arg_types[in++] = &ffi_type_pointer;
               j += sizeof(APTR);

               if (args[i+1].Type & (FD_BUFSIZE|FD_ARRAYSIZE)) {
                  if (args[i+1].Type & FD_LONG) {
                     ((LONG *)(buffer + j))[0] = mem->ArraySize;
                     arg_values[in]  = buffer + j;
                     arg_types[in++] = &ffi_type_sint32;
                     i++;
                     j += sizeof(LONG);
                  }
                  else if (args[i+1].Type & FD_LARGE) {
                     ((LARGE *)(buffer + j))[0] = mem->ArraySize;
                     arg_values[in]  = buffer + j;
                     arg_types[in++] = &ffi_type_sint64;
                     i++;
                     j += sizeof(LARGE);
                  }
                  else log.warning("Integer type unspecified for BUFSIZE argument in %s()", mod->Functions[index].Name);
               }
               else {
                  luaL_error(Lua, "Function '%s' is not compatible with Fluid.", mod->Functions[index].Name);
                  return 0;
               }
            }
            else {
               luaL_error(Lua, "A memory buffer is required in arg #%d.", i);
               return 0;
            }
         }
         else if (argtype & (FD_STR|FD_PTR|FD_ARRAY)) {
            end -= sizeof(APTR);
            ((APTR *)(buffer + j))[0] = end;
            ((APTR *)end)[0] = NULL;
            arg_values[in]  = buffer + j;
            arg_types[in++] = &ffi_type_pointer;
            j += sizeof(APTR);
         }
         else if (argtype & (FD_LONG)) {
            end -= sizeof(LONG);
            ((APTR *)(buffer + j))[0] = end;
            ((LONG *)end)[0] = 0;
            arg_values[in]  = buffer + j;
            arg_types[in++] = &ffi_type_pointer;
            j += sizeof(APTR);
         }
         else if (argtype & (FD_DOUBLE|FD_LARGE)) {
            end -= sizeof(LARGE);
            ((APTR *)(buffer + j))[0] = end;
            ((LARGE *)end)[0] = 0;
            arg_values[in]  = buffer + j;
            arg_types[in++] = &ffi_type_pointer;
            j += sizeof(APTR);
         }
         else {
            luaL_error(Lua, "Unrecognised arg %d type %d", i, argtype);
            return 0;
         }
      }
      else if (argtype & (FD_TAGS|FD_VARTAGS)) {
         luaL_error(Lua, "Functions using tags are not supported.");
         return 0;
      }
      else if (argtype & FD_FUNCTION) {
         if (func.Type) { // Is the function reserve already used?
            luaL_error(Lua, "Multiple function arguments are not supported.");
            return 0;
         }

         switch(lua_type(Lua, i)) {
            case LUA_TSTRING: { // Name of function to call
               lua_getglobal(Lua, lua_tostring(Lua, i));
               func = make_function_script(Self, luaL_ref(Lua, LUA_REGISTRYINDEX));
               ((FUNCTION **)(buffer + j))[0] = &func;
               break;
            }

            case LUA_TFUNCTION: { // Direct function reference
               lua_pushvalue(Lua, i);
               func = make_function_script(Self, luaL_ref(Lua, LUA_REGISTRYINDEX));
               ((FUNCTION **)(buffer + j))[0] = &func;
               break;
            }

            case LUA_TNIL:
            case LUA_TNONE:
               ((FUNCTION **)(buffer + j))[0] = NULL;
               break;

            default:
               luaL_error(Lua, "Type mismatch, arg #%d (%s) expected function, got %s '%s'.", i, args[i].Name, lua_typename(Lua, lua_type(Lua, i)), lua_tostring(Lua, i));
               return 0;
         }

         arg_values[in]  = buffer + j;
         arg_types[in++] = &ffi_type_pointer;
         j += sizeof(FUNCTION *);
      }
      else if (argtype & FD_STR) {
         auto type = lua_type(Lua, i);

         if ((type IS LUA_TSTRING) or (type IS LUA_TNUMBER) or (type IS LUA_TBOOLEAN)) {
            ((CSTRING *)(buffer + j))[0] = lua_tostring(Lua, i);
         }
         else if (type <= 0) {
            ((CSTRING *)(buffer + j))[0] = NULL;
         }
         else if ((type IS LUA_TUSERDATA) or (type IS LUA_TLIGHTUSERDATA)) {
            luaL_error(Lua, "Arg #%d (%s) requires a string and not untyped pointer.", i, args[i].Name, lua_typename(Lua, lua_type(Lua, i)), lua_tostring(Lua, i));
            return 0;
         }
         else {
            luaL_error(Lua, "Type mismatch, arg #%d (%s) expected string, got %s '%s'.", i, args[i].Name, lua_typename(Lua, lua_type(Lua, i)), lua_tostring(Lua, i));
            return 0;
         }

         arg_values[in] = buffer + j;
         arg_types[in++] = &ffi_type_pointer;
         j += sizeof(STRING);
      }
      else if (argtype & FD_ARRAY) {
         if (argtype & FD_CPP) {
            luaL_error(Lua, "No support for calls utilising C++ arrays.");
            return 0;
         }

         if (auto mem = (array *)get_meta(Lua, i, "Fluid.array")) {
            arg_values[in] = &mem->ptrVoid;
            arg_types[in++] = &ffi_type_pointer;
            j += sizeof(APTR); // Dummy increment

            if (args[i+1].Type & (FD_BUFSIZE|FD_ARRAYSIZE)) {
               if (args[i+1].Type & FD_RESULT) {
                  if (args[i+1].Type & FD_LONG) {
                     end -= sizeof(LONG);
                     ((LONG *)end)[0] = mem->Total;
                     ((APTR *)(buffer + j))[0] = end;
                     arg_values[in] = buffer + j;
                     arg_types[in++] = &ffi_type_pointer;
                     j += sizeof(APTR);
                     i++;
                  }
                  else if (args[i+1].Type & FD_LARGE) {
                     end -= sizeof(LARGE);
                     ((LARGE *)end)[0] = mem->Total;
                     ((APTR *)(buffer + j))[0] = end;
                     arg_values[in] = buffer + j;
                     arg_types[in++] = &ffi_type_pointer;
                     j += sizeof(APTR);
                     i++;
                  }
                  else {
                     luaL_error(Lua, "Function '%s' is not compatible with Fluid.", mod->Functions[index].Name);
                     return 0;
                  }
               }
               else {
                  if (args[i+1].Type & FD_LONG) {
                     ((LONG *)(buffer + j))[0] = mem->Total;
                     arg_values[in] = buffer + j;
                     arg_types[in++] = &ffi_type_sint32;
                     j += sizeof(LONG);
                     i++;
                  }
                  else if (args[i+1].Type & FD_LARGE) {
                     ((LARGE *)(buffer + j))[0] = mem->Total;
                     arg_values[in] = buffer + j;
                     arg_types[in++] = &ffi_type_sint64;
                     j += sizeof(LARGE);
                     i++;
                  }
                  else {
                     luaL_error(Lua, "Function '%s' is not compatible with Fluid.", mod->Functions[index].Name);
                     return 0;
                  }
               }
            }
            else {
               luaL_error(Lua, "Function '%s' is not compatible with Fluid.", mod->Functions[index].Name);
               return 0;
            }
         }
         else {
            luaL_error(Lua, "Type mismatch, arg #%d (%s) expected array, got '%s'.", i, args[i].Name, lua_typename(Lua, lua_type(Lua, i)));
            return 0;
         }
      }
      else if (argtype & FD_PTR) {
         if (lua_type(Lua, i) IS LUA_TSTRING) {
            // Lua strings need to be converted to C strings
            size_t strlen;
            ((CSTRING *)(buffer + j))[0] = lua_tolstring(Lua, i, &strlen);
            arg_values[in] = buffer + j;
            arg_types[in++] = &ffi_type_pointer;
            j += sizeof(CSTRING);

            if (args[i+1].Type & FD_BUFSIZE) {
               if (args[i+1].Type & FD_LONG) {
                  ((LONG *)(buffer + j))[0] = strlen;
                  i++;
                  arg_values[in] = buffer + j;
                  arg_types[in++] = &ffi_type_sint32;
                  j += sizeof(LONG);
               }
               else if (args[i+1].Type & FD_LARGE) {
                  ((LARGE *)(buffer + j))[0] = strlen;
                  i++;
                  arg_values[in] = buffer + j;
                  arg_types[in++] = &ffi_type_sint64;
                  j += sizeof(LARGE);
               }
            }
         }
         else if (auto memory = (struct memory *)get_meta(Lua, i, "Fluid.mem")) {
            ((APTR *)(buffer + j))[0] = memory->Address;
            arg_values[in] = buffer + j;
            arg_types[in++] = &ffi_type_pointer;
            j += sizeof(APTR);

            if (args[i+1].Type & FD_BUFSIZE) {
               if (args[i+1].Type & FD_LONG) {
                  ((LONG *)(buffer + j))[0] = memory->MemorySize;
                  i++;
                  arg_values[in] = buffer + j;
                  arg_types[in++] = &ffi_type_sint32;
                  j += sizeof(LONG);
               }
               else if (args[i+1].Type & FD_LARGE) {
                  ((LARGE *)(buffer + j))[0] = memory->MemorySize;
                  i++;
                  arg_values[in] = buffer + j;
                  arg_types[in++] = &ffi_type_sint64;
                  j += sizeof(LARGE);
               }
            }
         }
         else if (auto fstruct = (struct fstruct *)get_meta(Lua, i, "Fluid.struct")) {
            ((APTR *)(buffer + j))[0] = fstruct->Data;
            arg_values[in] = buffer + j;
            arg_types[in++] = &ffi_type_pointer;
            j += sizeof(APTR);

            log.trace("Struct address %p inserted to arg offset %d", fstruct->Data, j);
            if (args[i+1].Type & FD_BUFSIZE) {
               if (args[i+1].Type & FD_LONG) {
                  ((LONG *)(buffer + j))[0] = fstruct->AlignedSize;
                  i++;
                  arg_values[in] = buffer + j;
                  arg_types[in++] = &ffi_type_sint32;
                  j += sizeof(LONG);
               }
               else if (args[i+1].Type & FD_LARGE) {
                  ((LARGE *)(buffer + j))[0] = fstruct->AlignedSize;
                  i++;
                  arg_values[in] = buffer + j;
                  arg_types[in++] = &ffi_type_sint64;
                  j += sizeof(LARGE);
               }
            }
         }
         else if (auto obj = (object *)get_meta(Lua, i, "Fluid.obj")) {
            if (obj->ObjectPtr) {
               ((OBJECTPTR *)(buffer + j))[0] = obj->ObjectPtr;
            }
            else if (auto ptr_obj = (OBJECTPTR)access_object(obj)) {
               ((OBJECTPTR *)(buffer + j))[0] = ptr_obj;
               release_object(obj);
            }
            else {
               log.warning("Unable to resolve object pointer for #%d.", obj->UID);
               ((OBJECTPTR *)(buffer + j))[0] = NULL;
            }

            arg_values[in] = buffer + j;
            arg_types[in++] = &ffi_type_pointer;
            j += sizeof(APTR);
         }
         else {
            ((APTR *)(buffer + j))[0] = lua_touserdata(Lua, i); //lua_topointer?
            arg_values[in] = buffer + j;
            arg_types[in++] = &ffi_type_pointer;
            j += sizeof(APTR);
         }
      }
      else if (argtype & FD_LONG) {
         if (argtype & FD_OBJECT) {
            if (auto obj = (object *)get_meta(Lua, i, "Fluid.obj")) {
               ((LONG *)(buffer + j))[0] = obj->UID;
            }
            else ((LONG *)(buffer + j))[0] = lua_tointeger(Lua, i);
         }
         else if (argtype & FD_UNSIGNED) ((ULONG *)(buffer + j))[0] = lua_tointeger(Lua, i);
         else ((LONG *)(buffer + j))[0] = lua_tointeger(Lua, i);
         arg_values[in] = buffer + j;
         arg_types[in++] = &ffi_type_sint32;
         j += sizeof(LONG);
      }
      else if (argtype & FD_DOUBLE) {
         ((DOUBLE *)(buffer + j))[0] = lua_tonumber(Lua, i);
         arg_values[in] = buffer + j;
         arg_types[in++] = &ffi_type_double;
         j += sizeof(DOUBLE);
      }
      else if (argtype & FD_LARGE) {
         ((LARGE *)(buffer + j))[0] = lua_tointeger(Lua, i);
         arg_values[in] = buffer + j;
         arg_types[in++] = &ffi_type_sint64;
         j += sizeof(LARGE);
      }
      else if (argtype & FD_PTRSIZE) {
         ((LONG *)(buffer + j))[0] = lua_tointeger(Lua, i);
         arg_values[in] = buffer + j;
         arg_types[in++] = &ffi_type_sint32;
         j += sizeof(LONG);
      }
      else {
         log.warning("%s() unsupported arg '%s', flags $%.8x, aborting now.", mod->Functions[index].Name, args[i].Name, argtype);
         return 0;
      }
   }

   // Call the function.  The method used for execution depends on the function's result type.

   LONG restype = args->Type;
   LONG result = 1;
   LONG total_args = i - 1;

   if (restype & FD_STR) {
      if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_pointer, arg_types) IS FFI_OK) {
         ffi_call(&cif, (void (*)())function, &rc, arg_values);
         lua_pushstring(Lua, (CSTRING)rc);
      }
      else lua_pushnil(Lua);
   }
   else if (restype & FD_OBJECT) {
      if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_pointer, arg_types) IS FFI_OK) {
         ffi_call(&cif, (void (*)())function, &rc, arg_values);
         if ((OBJECTPTR)rc) {
            object *obj = push_object(Lua, (OBJECTPTR)rc);
            if (restype & FD_ALLOC) obj->Detached = false;
         }
         else lua_pushnil(Lua);
      }
      else lua_pushnil(Lua);
   }
   else if (restype & FD_PTR) {
      if (restype & FD_STRUCT) {
         if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_pointer, arg_types) IS FFI_OK) {
            ffi_call(&cif, (void (*)())function, &rc, arg_values);
            if (auto structptr = (APTR)rc) {
               ERROR error;
               // A structure marked as a resource will be returned as an accessible struct pointer.  This is typically
               // needed when a struct's use is beyond informational and can be passed to other functions.
               //
               // Otherwise, the default behaviour is to convert the struct's content to a regular Lua table.
               if (restype & FD_RESOURCE) push_struct(Self, structptr, args->Name, (restype & FD_ALLOC) ? TRUE : FALSE, TRUE);
               else if ((error = named_struct_to_table(Lua, args->Name, structptr)) != ERR_Okay) {
                  if (error IS ERR_Search) {
                     // Unknown structs are returned as pointers - this is mainly to indicate that there is a value
                     // and not a nil.
                     lua_pushlightuserdata(Lua, (APTR)structptr);
                  }
                  else {
                     luaL_error(Lua, "Failed to resolve struct %s, error: %s", args->Name, GetErrorMsg(error));
                     return 0;
                  }
               }
            }
            else lua_pushnil(Lua);
         }
         else lua_pushnil(Lua);
      }
      else if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_pointer, arg_types) IS FFI_OK) {
         ffi_call(&cif, (void (*)())function, &rc, arg_values);
         if ((APTR)rc) lua_pushlightuserdata(Lua, (APTR)rc);
         else lua_pushnil(Lua);
      }
      else lua_pushnil(Lua);
   }
   else if (restype & (FD_LONG|FD_ERROR)) {
      if (restype & FD_UNSIGNED) {
         if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_uint32, arg_types) IS FFI_OK) {
            ffi_call(&cif, (void (*)())function, &rc, arg_values);
            lua_pushnumber(Lua, (ULONG)rc);
         }
         else lua_pushnil(Lua);
      }
      else {
         if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_sint32, arg_types) IS FFI_OK) {
            ffi_call(&cif, (void (*)())function, &rc, arg_values);
            lua_pushinteger(Lua, (LONG)rc);

            if ((prv->Catch) and (restype & FD_ERROR) and (rc >= ERR_ExceptionThreshold)) {
               prv->CaughtError = rc;
               luaL_error(prv->Lua, GetErrorMsg(rc));
            }
         }
         else lua_pushnil(Lua);
      }
   }
   else if (restype & FD_DOUBLE) {
      if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_double, arg_types) IS FFI_OK) {
         ffi_call(&cif, (void (*)())function, &rc, arg_values);
         lua_pushnumber(Lua, (DOUBLE)rc);
      }
      else lua_pushnil(Lua);
   }
   else if (restype & FD_LARGE) {
      if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_sint64, arg_types) IS FFI_OK) {
         ffi_call(&cif, (void (*)())function, &rc, arg_values);
         lua_pushnumber(Lua, (LARGE)rc);
      }
      else lua_pushnil(Lua);
   }
   else { // Void
      if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_void, arg_types) IS FFI_OK) {
         ffi_call(&cif, (void (*)())function, &rc, arg_values);
      }
      result = 0;
   }

   if ((size_t)j >= sizeof(buffer)-8) {
      luaL_error(Lua, "Too many arguments - buffer overflow.");
      return 0;
   }

   return process_results(prv, buffer, args, result);
}

//********************************************************************************************************************
// This code looks for FD_RESULT arguments in the function's parameter list and converts them into multiple Fluid results.

static LONG process_results(prvFluid *prv, APTR resultsidx, const FunctionField *args, LONG result)
{
   pf::Log log(__FUNCTION__);
   LONG i;
   APTR var;

   auto scan = (UBYTE *)resultsidx;
   for (i=1; args[i].Name; i++) {
      LONG argtype = args[i].Type;
      CSTRING argname = args[i].Name;

      if (argtype & FD_RESULT) {
         var = ((APTR *)scan)[0];
         RMSG("Result Arg %d @ %p", i, var);
      }
      else var = NULL;

      if ((argtype & FD_ARRAY) and (!(argtype & FD_BUFFER))) {
         scan += sizeof(APTR);
         if (argtype & FD_RESULT) {
            if (var) {
               APTR values = ((APTR *)var)[0];
               LONG total_elements = -1; // If -1, make_any_table() assumes the array is null terminated.

               if (args[i+1].Type & FD_ARRAYSIZE) {
                  CPTR size_var = ((APTR *)scan)[0];
                  if (args[i+1].Type & FD_LONG) total_elements = ((LONG *)size_var)[0];
                  else if (args[i+1].Type & FD_LARGE) total_elements = ((LARGE *)size_var)[0];
                  else log.warning("Invalid arg %s, flags $%.8x", args[i+1].Name, args[i+1].Type);
               }

               if (values) {
                  make_any_table(prv->Lua, argtype, argname, total_elements, values);
                  if (argtype & FD_ALLOC) FreeResource(values);
               }
               else lua_pushnil(prv->Lua);
            }
            else lua_pushnil(prv->Lua);
            result++;
         }
      }
      else if (argtype & FD_STR) {
         scan += sizeof(APTR);
         if (argtype & FD_RESULT) {
            if (var) {
               RMSG("Result-Arg: %s, Value: %.20s (String)", argname, ((STRING *)var)[0]);
               lua_pushstring(prv->Lua, ((STRING *)var)[0]);
               if ((argtype & FD_ALLOC) and (((STRING *)var)[0])) FreeResource(((STRING *)var)[0]);
            }
            else lua_pushnil(prv->Lua);
            result++;
         }
      }
      else if (argtype & (FD_PTR|FD_BUFFER|FD_STRUCT)) {
         scan += sizeof(APTR);
         if (argtype & FD_RESULT) {
            if (var) {
               if (argtype & FD_OBJECT) {
                  RMSG("Result-Arg: %s, Value: %p (Object)", argname, ((OBJECTPTR *)var)[0]);
                  if (((APTR *)var)[0]) {
                     object *obj = push_object(prv->Lua, ((OBJECTPTR *)var)[0]);
                     if (argtype & FD_ALLOC) obj->Detached = false;
                  }
                  else lua_pushnil(prv->Lua);
               }
               else if (argtype & FD_STRUCT) {
                  if (((APTR *)var)[0]) {
                     if (argtype & FD_RESOURCE) {
                        // Resource structures are managed with direct data addresses.
                        push_struct(prv->Lua->Script, ((APTR *)var)[0], args[i].Name, (argtype & FD_ALLOC) ? TRUE : FALSE, TRUE);
                     }
                     else {
                        if (named_struct_to_table(prv->Lua, args[i].Name, ((APTR *)var)[0]) != ERR_Okay) lua_pushnil(prv->Lua);
                        if (argtype & FD_ALLOC) FreeResource(((APTR *)var)[0]);
                     }
                  }
                  else lua_pushnil(prv->Lua);
               }
               else if (argtype & FD_ALLOC) { // The result is a memory allocation.  Convert it to a binary 'string' of fixed length
                  LARGE size = 0;
                  if (args[i+1].Type & FD_BUFSIZE) {
                     const APTR size_var = ((APTR *)scan)[0];
                     if (args[i+1].Type & FD_LONG) size = ((LONG *)size_var)[0];
                     else if (args[i+1].Type & FD_LARGE) size = ((LARGE *)size_var)[0];
                     else log.warning("Invalid arg %s, flags $%.8x", args[i+1].Name, args[i+1].Type);
                  }
                  else {
                     MemInfo meminfo;
                     if (!MemoryIDInfo(GetMemoryID(((APTR *)var)[0]), &meminfo)) size = meminfo.Size;
                  }

                  if (size > 0) lua_pushlstring(prv->Lua, ((CSTRING *)var)[0], size);
                  else lua_pushnil(prv->Lua);

                  if (((APTR *)var)[0]) FreeResource(((APTR *)var)[0]);
               }
               else if (args[i+1].Type & FD_BUFSIZE) { // We can convert the data to a binary string rather than work with unsafe pointers.
                  LARGE size = 0;
                  CPTR size_var = ((APTR *)scan)[0];
                  if (args[i+1].Type & FD_LONG) size = ((LONG *)size_var)[0];
                  else if (args[i+1].Type & FD_LARGE) size = ((LARGE *)size_var)[0];
                  else log.warning("Invalid arg %s, flags $%.8x", args[i+1].Name, args[i+1].Type);

                  if (size > 0) lua_pushlstring(prv->Lua, ((CSTRING *)var)[0], size);
                  else lua_pushnil(prv->Lua);
               }
               else {
                  RMSG("Result-Arg: %s, Value: %p (Pointer)", argname, ((CSTRING *)var)[0]);
                  lua_pushlightuserdata(prv->Lua, ((APTR *)var)[0]);
               }
            }
            else lua_pushnil(prv->Lua);
            result++;
         }
      }
      else if (argtype & FD_LONG) {
         if (argtype & FD_RESULT) {
            scan += sizeof(APTR);
            if (var) {
               RMSG("Result-Arg: %s, Value: %d (Long)", argname, ((LONG *)var)[0]);
               lua_pushinteger(prv->Lua, ((LONG *)var)[0]);
            }
            else lua_pushnil(prv->Lua);
            result++;
         }
         else scan += sizeof(LONG);
      }
      else if (argtype & FD_DOUBLE) {
         if (argtype & FD_RESULT) {
            scan += sizeof(APTR);
            if (var) {
               RMSG("Result-Arg: %s, Value: %.2f (Double)", argname, ((DOUBLE *)var)[0]);
               lua_pushnumber(prv->Lua, ((DOUBLE *)var)[0]);
            }
            else lua_pushnil(prv->Lua);
            result++;
         }
         else scan += sizeof(DOUBLE);
      }
      else if (argtype & FD_LARGE) {
         if (argtype & FD_RESULT) {
            scan += sizeof(APTR);
            if (var) {
               RMSG("Result-Arg: %s, Value: %" PF64 " (Large)", argname, ((LARGE *)var)[0]);
               lua_pushnumber(prv->Lua, ((LARGE *)var)[0]);
            }
            else lua_pushnil(prv->Lua);
            result++;
         }
         else scan += sizeof(LARGE);
      }
      else {
         log.warning("Unsupported arg '%s', flags $%x, aborting now.", argname, argtype);
         break;
      }
   }

   RMSG("module_call: Wrote %d results.", result);

   return result;
}

//********************************************************************************************************************
// Register the module interface.

void register_module_class(lua_State *Lua)
{
   pf::Log log;

   static const struct luaL_Reg modlib_functions[] = {
      { "new",  module_load },
      { "load", module_load },
      { NULL, NULL}
   };

   static const struct luaL_Reg modlib_methods[] = {
      { "__index",    module_index },
      { "__tostring", module_tostring },
      { "__gc",       module_destruct },
      { NULL, NULL }
   };

   log.trace("Registering module interface.");

   luaL_newmetatable(Lua, "Fluid.mod");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable
   lua_settable(Lua, -3);   // metatable.__index = metatable

   luaL_openlib(Lua, NULL, modlib_methods, 0);
   luaL_openlib(Lua, "mod", modlib_functions, 0);
}
