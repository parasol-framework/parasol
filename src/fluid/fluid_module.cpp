
#define PRV_SCRIPT
#define PRV_FLUID
#define PRV_FLUID_MODULE
#include <parasol/main.h>
#include <parasol/modules/fluid.h>
#include <parasol/strings.hpp>
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

template<class... Args> void RMSG(Args...) {
   //log.msg(Args)
}

constexpr int MAX_MODULE_ARGS = 16;

static int module_call(lua_State *);
static int process_results(prvFluid *, APTR, const FunctionField *);

//********************************************************************************************************************
// Usage: module = mod.load('core')

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

   auto error = load_include(Lua->Script, modname);
   if ((error != ERR::Okay) and (error != ERR::FileNotFound)) {
      log.debranch();
      luaL_error(Lua, "Failed to load include file for the %s module.", modname);
      return 0;
   }

   if (auto loaded_mod = objModule::create::global(fl::Name(modname))) {
      auto mod = (module *)lua_newuserdata(Lua, sizeof(module));
      clearmem(mod, sizeof(struct module));

      luaL_getmetatable(Lua, "Fluid.mod");
      lua_setmetatable(Lua, -2);

      mod->Module = loaded_mod;
      loaded_mod->get(FID_FunctionList, mod->Functions);
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
      CSTRING name;
      if (mod->Module->get(FID_Name, name) IS ERR::Okay) {
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
            for (int i=0; list[i].Name; i++) {
               if (pf::iequals(list[i].Name, function)) { // Function call stack management
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
   int i;

   auto prv = (prvFluid *)Self->ChildPrivate;
   if (!prv) {
      log.warning(ERR::ObjectCorrupt);
      return 0;
   }

   auto mod = (module *)get_meta(Lua, lua_upvalueindex(1), "Fluid.mod");
   if (!mod) {
      luaL_error(Lua, "module_call() expected module in upvalue.");
      return 0;
   }

   if (!mod->Functions) return 0;

   auto index = lua_tointeger(Lua, lua_upvalueindex(2));
   int nargs = lua_gettop(Lua);
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
   FUNCTION func;
   ffi_cif cif;
   ffi_arg rc;
   ffi_type *arg_types[MAX_MODULE_ARGS];
   void * arg_values[MAX_MODULE_ARGS];
   int in = 0;

   int j = 0;
   for (i=1; (args[i].Name) and ((size_t)j < sizeof(buffer)-8); i++) {
      int argtype = args[i].Type;

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
                  if (args[i+1].Type & FD_INT) {
                     ((int *)(buffer + j))[0] = mem->ArraySize;
                     arg_values[in]  = buffer + j;
                     arg_types[in++] = &ffi_type_sint32;
                     i++;
                     j += sizeof(int);
                  }
                  else if (args[i+1].Type & FD_INT64) {
                     ((int64_t *)(buffer + j))[0] = mem->ArraySize;
                     arg_values[in]  = buffer + j;
                     arg_types[in++] = &ffi_type_sint64;
                     i++;
                     j += sizeof(int64_t);
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
         else if (argtype & FD_STR) { // FD_RESULT
            if (argtype & FD_CPP) {
               // Special case; we provide a std::string that will be used as a buffer for storing the result.
               ((std::string **)(buffer + j))[0] = new std::string;
               arg_values[in]  = buffer + j;
               arg_types[in++] = &ffi_type_pointer;
               j += sizeof(APTR);
            }
            else {
               end -= sizeof(APTR);
               ((APTR *)(buffer + j))[0] = end;
               ((APTR *)end)[0] = nullptr;
               arg_values[in]  = buffer + j;
               arg_types[in++] = &ffi_type_pointer;
               j += sizeof(APTR);
            }
         }
         else if (argtype & (FD_PTR|FD_ARRAY)) { // FD_RESULT
            end -= sizeof(APTR);
            ((APTR *)(buffer + j))[0] = end;
            ((APTR *)end)[0] = nullptr;
            arg_values[in]  = buffer + j;
            arg_types[in++] = &ffi_type_pointer;
            j += sizeof(APTR);
         }
         else if (argtype & FD_INT) { // FD_RESULT
            end -= sizeof(int);
            ((APTR *)(buffer + j))[0] = end;
            ((int *)end)[0] = 0;
            arg_values[in]  = buffer + j;
            arg_types[in++] = &ffi_type_pointer;
            j += sizeof(APTR);
         }
         else if (argtype & (FD_DOUBLE|FD_INT64)) { // FD_RESULT
            end -= sizeof(int64_t);
            ((APTR *)(buffer + j))[0] = end;
            ((int64_t *)end)[0] = 0;
            arg_values[in]  = buffer + j;
            arg_types[in++] = &ffi_type_pointer;
            j += sizeof(APTR);
         }
         else {
            luaL_error(Lua, "Unrecognised arg %d type %d", i, argtype);
            return 0;
         }
      }
      else if (argtype & FD_FUNCTION) {
         if (func.defined()) { // Is the function reserve already used?
            luaL_error(Lua, "Multiple function arguments are not supported.");
            return 0;
         }

         switch(lua_type(Lua, i)) {
            case LUA_TSTRING: { // Name of function to call
               lua_getglobal(Lua, lua_tostring(Lua, i));
               func = FUNCTION(Self, luaL_ref(Lua, LUA_REGISTRYINDEX));
               ((FUNCTION **)(buffer + j))[0] = &func;
               break;
            }

            case LUA_TFUNCTION: { // Direct function reference
               lua_pushvalue(Lua, i);
               func = FUNCTION(Self, luaL_ref(Lua, LUA_REGISTRYINDEX));
               ((FUNCTION **)(buffer + j))[0] = &func;
               break;
            }

            case LUA_TNIL:
            case LUA_TNONE:
               ((FUNCTION **)(buffer + j))[0] = nullptr;
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

         if (argtype & FD_CPP) { // std::string_view (enforced, cannot be nullptr)
            size_t len;
            auto str = lua_tolstring(Lua, i, &len);
            ((std::string_view **)(buffer + j))[0] = new std::string_view(str, len);
         }
         else if ((type IS LUA_TSTRING) or (type IS LUA_TNUMBER) or (type IS LUA_TBOOLEAN)) {
            ((CSTRING *)(buffer + j))[0] = lua_tostring(Lua, i);
         }
         else if (type <= 0) {
            ((CSTRING *)(buffer + j))[0] = nullptr;
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
         j += sizeof(APTR);
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
                  if (args[i+1].Type & FD_INT) {
                     end -= sizeof(int);
                     ((int *)end)[0] = mem->Total;
                     ((APTR *)(buffer + j))[0] = end;
                     arg_values[in] = buffer + j;
                     arg_types[in++] = &ffi_type_pointer;
                     j += sizeof(APTR);
                     i++;
                  }
                  else if (args[i+1].Type & FD_INT64) {
                     end -= sizeof(int64_t);
                     ((int64_t *)end)[0] = mem->Total;
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
                  if (args[i+1].Type & FD_INT) {
                     ((int *)(buffer + j))[0] = mem->Total;
                     arg_values[in] = buffer + j;
                     arg_types[in++] = &ffi_type_sint32;
                     j += sizeof(int);
                     i++;
                  }
                  else if (args[i+1].Type & FD_INT64) {
                     ((int64_t *)(buffer + j))[0] = mem->Total;
                     arg_values[in] = buffer + j;
                     arg_types[in++] = &ffi_type_sint64;
                     j += sizeof(int64_t);
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
               if (args[i+1].Type & FD_INT) {
                  ((int *)(buffer + j))[0] = strlen;
                  i++;
                  arg_values[in] = buffer + j;
                  arg_types[in++] = &ffi_type_sint32;
                  j += sizeof(int);
               }
               else if (args[i+1].Type & FD_INT64) {
                  ((int64_t *)(buffer + j))[0] = strlen;
                  i++;
                  arg_values[in] = buffer + j;
                  arg_types[in++] = &ffi_type_sint64;
                  j += sizeof(int64_t);
               }
            }
         }
         else if (auto array = (struct array *)get_meta(Lua, i, "Fluid.array")) {
            ((APTR *)(buffer + j))[0] = array->ptrVoid;
            arg_values[in] = buffer + j;
            arg_types[in++] = &ffi_type_pointer;
            j += sizeof(APTR);

            if (args[i+1].Type & FD_BUFSIZE) {
               if (args[i+1].Type & FD_INT) {
                  ((int *)(buffer + j))[0] = array->ArraySize;
                  i++;
                  arg_values[in] = buffer + j;
                  arg_types[in++] = &ffi_type_sint32;
                  j += sizeof(int);
               }
               else if (args[i+1].Type & FD_INT64) {
                  ((int64_t *)(buffer + j))[0] = array->ArraySize;
                  i++;
                  arg_values[in] = buffer + j;
                  arg_types[in++] = &ffi_type_sint64;
                  j += sizeof(int64_t);
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
               if (args[i+1].Type & FD_INT) {
                  ((int *)(buffer + j))[0] = fstruct->AlignedSize;
                  i++;
                  arg_values[in] = buffer + j;
                  arg_types[in++] = &ffi_type_sint32;
                  j += sizeof(int);
               }
               else if (args[i+1].Type & FD_INT64) {
                  ((int64_t *)(buffer + j))[0] = fstruct->AlignedSize;
                  i++;
                  arg_values[in] = buffer + j;
                  arg_types[in++] = &ffi_type_sint64;
                  j += sizeof(int64_t);
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
               ((OBJECTPTR *)(buffer + j))[0] = nullptr;
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
      else if (argtype & FD_INT) {
         if (argtype & FD_OBJECT) {
            if (auto obj = (object *)get_meta(Lua, i, "Fluid.obj")) {
               ((int *)(buffer + j))[0] = obj->UID;
            }
            else ((int *)(buffer + j))[0] = lua_tointeger(Lua, i);
         }
         else if (argtype & FD_UNSIGNED) ((uint32_t *)(buffer + j))[0] = lua_tointeger(Lua, i);
         else ((int *)(buffer + j))[0] = lua_tointeger(Lua, i);
         arg_values[in] = buffer + j;
         arg_types[in++] = &ffi_type_sint32;
         j += sizeof(int);
      }
      else if (argtype & FD_DOUBLE) {
         ((double *)(buffer + j))[0] = lua_tonumber(Lua, i);
         arg_values[in] = buffer + j;
         arg_types[in++] = &ffi_type_double;
         j += sizeof(double);
      }
      else if (argtype & FD_INT64) {
         ((int64_t *)(buffer + j))[0] = lua_tointeger(Lua, i);
         arg_values[in] = buffer + j;
         arg_types[in++] = &ffi_type_sint64;
         j += sizeof(int64_t);
      }
      else if (argtype & FD_PTRSIZE) {
         ((int *)(buffer + j))[0] = lua_tointeger(Lua, i);
         arg_values[in] = buffer + j;
         arg_types[in++] = &ffi_type_sint32;
         j += sizeof(int);
      }
      else if (argtype & (FD_TAGS|FD_VARTAGS)) {
         luaL_error(Lua, "Functions using tags are not supported.");
         return 0;
      }
      else {
         log.warning("%s() unsupported arg '%s', flags $%.8x, aborting now.", mod->Functions[index].Name, args[i].Name, argtype);
         return 0;
      }
   }

   // Call the function.  The method used for execution depends on the function's result type.

   int restype = args->Type;
   int result = 1;
   int total_args = i - 1;

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
               ERR error;
               // A structure marked as a resource will be returned as an accessible struct pointer.  This is typically
               // needed when a struct's use is beyond informational and can be passed to other functions.
               //
               // Otherwise, the default behaviour is to convert the struct's content to a regular Lua table.
               if (restype & FD_RESOURCE) push_struct(Self, structptr, args->Name, (restype & FD_ALLOC) ? TRUE : FALSE, TRUE);
               else if ((error = named_struct_to_table(Lua, args->Name, structptr)) != ERR::Okay) {
                  if (error IS ERR::Search) {
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
   else if (restype & (FD_INT|FD_ERROR)) {
      if (restype & FD_UNSIGNED) {
         if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_uint32, arg_types) IS FFI_OK) {
            ffi_call(&cif, (void (*)())function, &rc, arg_values);
            lua_pushnumber(Lua, (uint32_t)rc);
         }
         else lua_pushnil(Lua);
      }
      else {
         if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_sint32, arg_types) IS FFI_OK) {
            ffi_call(&cif, (void (*)())function, &rc, arg_values);
            lua_pushinteger(Lua, (int)rc);

            if ((prv->Catch) and (restype & FD_ERROR) and (rc >= int(ERR::ExceptionThreshold))) {
               prv->CaughtError = ERR(rc);
               luaL_error(prv->Lua, GetErrorMsg(ERR(rc)));
            }
         }
         else lua_pushnil(Lua);
      }
   }
   else if (restype & FD_DOUBLE) {
      if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_double, arg_types) IS FFI_OK) {
         ffi_call(&cif, (void (*)())function, &rc, arg_values);
         lua_pushnumber(Lua, (double)rc);
      }
      else lua_pushnil(Lua);
   }
   else if (restype & FD_INT64) {
      if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_sint64, arg_types) IS FFI_OK) {
         ffi_call(&cif, (void (*)())function, &rc, arg_values);
         lua_pushnumber(Lua, (int64_t)rc);
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

   return process_results(prv, buffer, args) + result;
}

//********************************************************************************************************************
// Convert FD_RESULT parameters to the equivalent Fluid result value.
// Also takes care of any cleanup code for dynamically allocated values.

static int process_results(prvFluid *prv, APTR resultsidx, const FunctionField *args)
{
   pf::Log log(__FUNCTION__);

   auto scan = (UBYTE *)resultsidx;
   int results = 0;
   for (int i=1; args[i].Name; i++) {
      const auto argtype = args[i].Type;

      if ((argtype & FD_ARRAY) and (!(argtype & FD_BUFFER))) {
         if (argtype & FD_RESULT) {
            auto var = ((APTR *)scan)[0];
            scan += sizeof(APTR);
            if (var) {
               const auto argname = args[i].Name;
               APTR values = ((APTR *)var)[0];
               int total_elements = -1; // If -1, make_any_table() assumes the array is null terminated.

               if (args[i+1].Type & FD_ARRAYSIZE) {
                  CPTR size_var = ((APTR *)scan)[0];
                  if (args[i+1].Type & FD_INT) total_elements = ((int *)size_var)[0];
                  else if (args[i+1].Type & FD_INT64) total_elements = ((int64_t *)size_var)[0];
                  else log.warning("Invalid arg %s, flags $%.8x", args[i+1].Name, args[i+1].Type);
               }

               if (values) {
                  make_any_table(prv->Lua, argtype, argname, total_elements, values);
                  if (argtype & FD_ALLOC) FreeResource(values);
               }
               else lua_pushnil(prv->Lua);
            }
            else lua_pushnil(prv->Lua);
            results++;
         }
         else scan += sizeof(APTR);
      }
      else if (argtype & FD_STR) {
         if (argtype & FD_RESULT) {
            if (auto var = ((APTR *)scan)[0]) {
               if (argtype & FD_CPP) { // std::string variant
                  auto str_result = (std::string *)var;
                  lua_pushlstring(prv->Lua, str_result->data(), str_result->size());
                  delete str_result;
               }
               else {
                  lua_pushstring(prv->Lua, ((STRING *)var)[0]);
                  if ((argtype & FD_ALLOC) and (((STRING *)var)[0])) FreeResource(((STRING *)var)[0]);
               }
            }
            else lua_pushnil(prv->Lua);
            results++;
         }
         else if (argtype & FD_CPP) { // Delete dynamically created std::string_view
            delete ((std::string_view **)scan)[0];
         }
         scan += sizeof(APTR);
      }
      else if (argtype & (FD_PTR|FD_BUFFER|FD_STRUCT)) {
         if (argtype & FD_RESULT) {
            auto var = ((APTR *)scan)[0];
            scan += sizeof(APTR);
            if (var) {
               if (argtype & FD_OBJECT) {
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
                        if (named_struct_to_table(prv->Lua, args[i].Name, ((APTR *)var)[0]) != ERR::Okay) lua_pushnil(prv->Lua);
                        if (argtype & FD_ALLOC) FreeResource(((APTR *)var)[0]);
                     }
                  }
                  else lua_pushnil(prv->Lua);
               }
               else if (argtype & FD_ALLOC) { // The result is a memory allocation.  Convert it to a binary 'string' of fixed length
                  int64_t size = 0;
                  if (args[i+1].Type & FD_BUFSIZE) {
                     const APTR size_var = ((APTR *)scan)[0];
                     if (args[i+1].Type & FD_INT) size = ((int *)size_var)[0];
                     else if (args[i+1].Type & FD_INT64) size = ((int64_t *)size_var)[0];
                     else log.warning("Invalid arg %s, flags $%.8x", args[i+1].Name, args[i+1].Type);
                  }
                  else {
                     MemInfo meminfo;
                     if (MemoryIDInfo(GetMemoryID(((APTR *)var)[0]), &meminfo) IS ERR::Okay) size = meminfo.Size;
                  }

                  if (size > 0) lua_pushlstring(prv->Lua, ((CSTRING *)var)[0], size);
                  else lua_pushnil(prv->Lua);

                  if (((APTR *)var)[0]) FreeResource(((APTR *)var)[0]);
               }
               else if (args[i+1].Type & FD_BUFSIZE) { // We can convert the data to a binary string rather than work with unsafe pointers.
                  int64_t size = 0;
                  CPTR size_var = ((APTR *)scan)[0];
                  if (args[i+1].Type & FD_INT) size = ((int *)size_var)[0];
                  else if (args[i+1].Type & FD_INT64) size = ((int64_t *)size_var)[0];
                  else log.warning("Invalid arg %s, flags $%.8x", args[i+1].Name, args[i+1].Type);

                  if (size > 0) lua_pushlstring(prv->Lua, ((CSTRING *)var)[0], size);
                  else lua_pushnil(prv->Lua);
               }
               else lua_pushlightuserdata(prv->Lua, ((APTR *)var)[0]);
            }
            else lua_pushnil(prv->Lua);
            results++;
         }
         else scan += sizeof(APTR);
      }
      else if (argtype & FD_INT) {
         if (argtype & FD_RESULT) {
            if (auto var = ((APTR *)scan)[0]) lua_pushinteger(prv->Lua, ((int *)var)[0]);
            else lua_pushnil(prv->Lua);
            scan += sizeof(APTR);
            results++;
         }
         else scan += sizeof(int);
      }
      else if (argtype & FD_DOUBLE) {
         if (argtype & FD_RESULT) {
            if (auto var = ((APTR *)scan)[0]) lua_pushnumber(prv->Lua, ((double *)var)[0]);
            else lua_pushnil(prv->Lua);
            scan += sizeof(APTR);
            results++;
         }
         else scan += sizeof(double);
      }
      else if (argtype & FD_INT64) {
         if (argtype & FD_RESULT) {
            if (auto var = ((APTR *)scan)[0]) lua_pushnumber(prv->Lua, ((int64_t *)var)[0]);
            else lua_pushnil(prv->Lua);
            scan += sizeof(APTR);
            results++;
         }
         else scan += sizeof(int64_t);
      }
      else {
         log.warning("Unsupported arg '%s', flags $%x, aborting now.", args[i].Name, argtype);
         return results;
      }
   }

   return results;
}

//********************************************************************************************************************
// Register the module interface.

void register_module_class(lua_State *Lua)
{
   pf::Log log;

   static const struct luaL_Reg modlib_functions[] = {
      { "new",  module_load },
      { "load", module_load },
      { nullptr, nullptr}
   };

   static const struct luaL_Reg modlib_methods[] = {
      { "__index",    module_index },
      { "__tostring", module_tostring },
      { "__gc",       module_destruct },
      { nullptr, nullptr }
   };

   log.trace("Registering module interface.");

   luaL_newmetatable(Lua, "Fluid.mod");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable
   lua_settable(Lua, -3);   // metatable.__index = metatable

   luaL_openlib(Lua, nullptr, modlib_methods, 0);
   luaL_openlib(Lua, "mod", modlib_functions, 0);
}
