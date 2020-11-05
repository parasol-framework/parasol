
#include <ffi.h>

static int module_call(lua_State *);
static LONG process_results(struct prvFluid *, APTR resultsidx, const struct FunctionField *, LONG);

/*****************************************************************************
** Usage: module = mod.load("core")
*/

static int module_load(lua_State *Lua)
{
   CSTRING modname;
   if (!(modname = luaL_checkstring(Lua, 1))) {
      luaL_argerror(Lua, 1, "String expected for module name.");
      return 0;
   }

   LogF("~mod.new()", "Module: %s", modname);

   // Check if there is an include file with the same name as this module.

   ERROR error = load_include(Lua->Script, modname);
   if ((error != ERR_Okay) AND (error != ERR_FileNotFound)) {
      LogReturn();
      luaL_error(Lua, "Failed to load include file for the %s module.", modname);
      return 0;
   }

   OBJECTPTR module;
   if (!(error = CreateObject(ID_MODULE, 0, &module,
         FID_Name|TSTR, modname,
         TAGEND))) {
      struct module *mod = (struct module *)lua_newuserdata(Lua, sizeof(struct module));
      ClearMemory(mod, sizeof(struct module));

      luaL_getmetatable(Lua, "Fluid.mod");
      lua_setmetatable(Lua, -2);

      mod->Module = module;
      GetPointer(module, FID_FunctionList, &mod->Functions);

      LogReturn();
      return 1;  // new userdatum is already on the stack
   }
   else {
      LogReturn();
      luaL_error(Lua, "Failed to load the %s module.", modname);
      return 0;
   }
}

/*****************************************************************************
** Internal: Object garbage collector.
*/

static int module_destruct(lua_State *Lua)
{
   struct module *mod;

   if ((mod = (struct module *)luaL_checkudata(Lua, 1, "Fluid.mod"))) {
      acFree(mod->Module);
   }

   return 0;
}

/*****************************************************************************
** Internal: Prints the module name
*/

static int module_tostring(lua_State *Lua)
{
   struct module *mod;

   if ((mod = (struct module *)luaL_checkudata(Lua, 1, "Fluid.mod"))) {
      STRING name;
      if (!GetString(mod->Module, FID_Name, &name)) {
         lua_pushstring(Lua, name);
      }
      else lua_pushnil(Lua);
   }
   else lua_pushnil(Lua);

   return 1;
}

/*****************************************************************************
** Any Read accesses to the module object will pass through here.
*/

static int module_index(lua_State *Lua)
{
   struct module *mod;
   if ((mod = (struct module *)luaL_checkudata(Lua, 1, "Fluid.mod"))) {
      CSTRING function;
      if ((function = luaL_checkstring(Lua, 2))) {
         struct Function *list;
         if ((list = mod->Functions)) {
            LONG i;
            for (i=0; list[i].Name; i++) {
               CSTRING name = list[i].Name;
               if (!StrMatch(name, function)) {
                  // Function call stack management

                  lua_pushvalue(Lua, 1); // Arg1: Duplicate the module reference
                  lua_pushinteger(Lua, i); // Arg2: Index of the function that is being called
                  lua_pushcclosure(Lua, module_call, 2);
                  return 1;
               }
            }

            luaL_error(Lua, "Function %s not recognised.", function);
         }
         else luaL_error(Lua, "No exported function list for this module.", function);
      }
      else luaL_argerror(Lua, 2, "Expected function string.");
   }
   else luaL_argerror(Lua, 1, "Expected module.");

   return 0;
}

//****************************************************************************

#undef RMSG
#define RMSG(a...) MSG(a)
#define MAX_MODULE_ARGS 16

static int module_call(lua_State *Lua)
{
   objScript *Self = Lua->Script;
   struct prvFluid *prv;
   UBYTE buffer[256]; // +8 for overflow protection
   FUNCTION func;
   LONG i, type;

   if (!(prv = Self->Head.ChildPrivate)) return PostError(ERR_ObjectCorrupt);

   struct module *mod;
   if (!(mod = get_meta(Lua, lua_upvalueindex(1), "Fluid.mod"))) {
      luaL_error(Lua, "object_call() expected object in upvalue.");
      return 0;
   }

   UWORD index = lua_tointeger(Lua, lua_upvalueindex(2));

   if (!mod) { luaL_argerror(Lua, 1, "Expected module from module_index()."); return 0; }

   if (!mod->Functions) return 0;

   LONG nargs = lua_gettop(Lua);
   if (nargs > MAX_MODULE_ARGS-1) nargs = MAX_MODULE_ARGS-1;

   UBYTE *end = buffer + sizeof(buffer);

   FMSG("module_call()","%s() Index: %d, Args: %d", mod->Functions[index].Name, index, nargs);

   const struct FunctionField *args;
   if (!(args = mod->Functions[index].Args)) {
      void (*function)(void);
      function = mod->Functions[index].Address;
      function();
      return 0;
   }

   APTR function = mod->Functions[index].Address;
   ffi_cif cif;
   ffi_arg rc;
   ffi_type *fin[MAX_MODULE_ARGS];
   void * fptr[MAX_MODULE_ARGS];
   LONG in = 0;

   LONG j = 0;
   for (i=1; (args[i].Name) AND (j < sizeof(buffer)-8); i++) {
      LONG argtype = args[i].Type;

      //FMSG("module_call()","%s() Arg: %s, Offset: %d, Type: $%.8x (received %s)", mod->Functions[index].Name, args[i].Name, j, argtype, lua_typename(Lua, lua_type(Lua, i)));

      if (argtype & FD_RESULT) {
         // Result arguments are stored in the buffer with a pointer to an empty variable space (stored at the end of
         // the buffer)

         RMSG("Result for arg %d stored at %p", i, end);

         if (argtype & FD_BUFFER) {
            // User is required to supply an argument that will store a buffer result.  This is a different case to the
            // storage of type values.

            struct memory *memory;
            if ((memory = get_meta(Lua, i, "Fluid.mem"))) {
               ((APTR *)(buffer + j))[0] = memory->Address;
               fptr[in] = buffer + j;
               fin[in++] = &ffi_type_pointer;
               j += sizeof(APTR);

               if (args[i+1].Type & FD_BUFSIZE) {
                  if (args[i+1].Type & FD_LONG) {
                     ((LONG *)(buffer + j))[0] = memory->MemorySize;
                     fptr[in] = buffer + j;
                     fin[in++] = &ffi_type_sint32;
                     i++;
                     j += sizeof(LONG);
                  }
                  else if (args[i+1].Type & FD_LARGE) {
                     ((LARGE *)(buffer + j))[0] = memory->MemorySize;
                     fptr[in] = buffer + j;
                     fin[in++] = &ffi_type_sint64;
                     i++;
                     j += sizeof(LARGE);
                  }
                  else LogErrorMsg("Integer type unspecified for BUFSIZE argument in %s()", mod->Functions[index].Name);
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
            fptr[in] = buffer + j;
            fin[in++] = &ffi_type_pointer;
            j += sizeof(APTR);
         }
         else if (argtype & (FD_LONG)) {
            end -= sizeof(LONG);
            ((APTR *)(buffer + j))[0] = end;
            ((LONG *)end)[0] = 0;
            fptr[in] = buffer + j;
            fin[in++] = &ffi_type_pointer;
            j += sizeof(APTR);
         }
         else if (argtype & (FD_DOUBLE|FD_LARGE)) {
            end -= sizeof(LARGE);
            ((APTR *)(buffer + j))[0] = end;
            ((LARGE *)end)[0] = 0;
            fptr[in] = buffer + j;
            fin[in++] = &ffi_type_pointer;
            j += sizeof(APTR);
         }
         else {
            luaL_error(Lua, "Unrecognised arg %d type %d", i, argtype);
            return 0;
         }
      }
      else if (argtype & FD_VARTAGS) { // Variable tags expect the use of TDOUBLE, TLONG etc to specify the value type
         if (argtype & FD_PTR) { // Pointer to a taglist
            luaL_error(Lua, "Pointers to tag-lists are unsupported.");
            return 0;
         }
         else {
            LONG fixed_args = i-1;
            while ((i <= nargs) AND (j < sizeof(buffer)-20)) {
               if (lua_type(Lua, i) IS LUA_TNUMBER) { // Tags have to be expressed as numbers.
                  LARGE tag = lua_tonumber(Lua, i++);
                  if (tag IS TAGEND) break;

                  LONG value_type = lua_type(Lua, i);
                  if (value_type IS LUA_TNUMBER) tag |= TDOUBLE;
                  else if (value_type IS LUA_TBOOLEAN) tag |= TLONG;
                  else if (value_type IS LUA_TSTRING) tag |= TSTR;
                  else if (value_type IS LUA_TLIGHTUSERDATA) tag |= TPTR;
                  else if (value_type IS LUA_TUSERDATA) tag |= TPTR;
                  else luaL_error(Lua, "Unsupported type '%s' at arg %d", lua_typename(Lua, value_type), i);

                  ((LARGE *)(buffer + j))[0] = tag;
                  fptr[in] = buffer + j;
                  fin[in++] = &ffi_type_uint64;
                  j += sizeof(LARGE);

                  if (tag & TDOUBLE) {
                     ((DOUBLE *)(buffer + j))[0] = lua_tonumber(Lua, i++);
                     fptr[in] = buffer + j;
                     fin[in++] = &ffi_type_double;
                     j += sizeof(DOUBLE);
                  }
                  else if (tag & TLARGE) {
                     ((LARGE *)(buffer + j))[0] = F2I(lua_tonumber(Lua, i++));
                     fptr[in] = buffer + j;
                     fin[in++] = &ffi_type_sint64;
                     j += sizeof(LARGE);
                  }
                  else if (tag & TLONG) {
                     ((LONG *)(buffer + j))[0] = lua_tointeger(Lua, i++);
                     fptr[in] = buffer + j;
                     fin[in++] = &ffi_type_sint32;
                     j += sizeof(LONG);
                  }
                  else if (tag & TSTR) {
                     ((CSTRING *)(buffer + j))[0] = lua_tostring(Lua, i++);
                     fptr[in] = buffer + j;
                     fin[in++] = &ffi_type_pointer;
                     j += sizeof(CSTRING);
                  }
                  else if (tag & TPTR) {
                     ((APTR *)(buffer + j))[0] = lua_touserdata(Lua, i++); //lua_topointer?
                     fptr[in] = buffer + j;
                     fin[in++] = &ffi_type_pointer;
                     j += sizeof(APTR);
                  }
                  else {
                     LogF("@module_call","Unrecognised tag type $%.8x00000000 at arg %d", (ULONG)(tag>>32), i);
                     luaL_error(Lua, "Invalid tag type detected.");
                     return 0;
                  }
               }
               else luaL_error(Lua, "Expected number for tag definition, got %s", lua_typename(Lua, lua_type(Lua, i)));
            }

            // TAGEND comes last.
            ((LARGE *)(buffer + j))[0] = TAGEND;
            fptr[in] = buffer + j;
            fin[in++] = &ffi_type_uint64;

            LONG result = 1;
            if (args->Type & FD_LONG) {
               if (ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, fixed_args, in, &ffi_type_sint32, fin) IS FFI_OK) {
                  ffi_call(&cif, function, &rc, fptr);
                  lua_pushinteger(Lua, (LONG)rc);
                  return process_results(prv, buffer, args, result);
               }
            }
            else if (ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, fixed_args, in, &ffi_type_void, fin) IS FFI_OK) {
               ffi_call(&cif, function, &rc, fptr);
               return process_results(prv, buffer, args, 0);
            }

            luaL_error(Lua, "Failed to make variadic function call to module.");
            return 0;
         }
      }
      else if (argtype & FD_TAGS) {
         if (argtype & FD_PTR) { // Pointer to a tag-list
            luaL_error(Lua, "Pointers to tag-lists are unsupported.");
         }
         else {
            // NOTE: All numeric tags passed from Lua are treated as 32-bit integers, this is because Lua
            // cannot differentiate between floats and integers.
            //
            // ! It might be possible to add a workaround by introducing a numeric interface that returns numeric objects, e.g.
            //   num.float(n), num.int(n), num.double(n)
            //   obj.new(..., FID_Delay, num.float(1.3), FID_Label, "Hello")

            luaL_error(Lua, "Tag-lists are not supported at this time.");
/*
            while ((i <= nargs) AND (j < sizeof(buffer)-8)) {
               type = lua_type(Lua, i);
               FMSG("module_call:","Arg %d/%d, Type: %s, ToIndex: %d", i, nargs, lua_typename(Lua, type), j);
               switch (type) {
                  case LUA_TNUMBER:
                  case LUA_TBOOLEAN:
                     ((LONG *)(buffer + j))[0] = F2I(lua_tonumber(Lua, i));
                     j += sizeof(LONG);
                     break;

                  case LUA_TUSERDATA:
                  case LUA_TLIGHTUSERDATA:
                     ((APTR *)(buffer + j))[0] = lua_touserdata(Lua, i); //lua_topointer?
                     j += sizeof(APTR);
                     break;

                  case LUA_TSTRING:
                     ((CSTRING *)(buffer + j))[0] = lua_tostring(Lua, i);
                     j += sizeof(STRING);
                     break;

                  default:
                     luaL_error(Lua, "Lua type %d/%s at arg %d (of %d) not supported as a tag value.", type, lua_typename(Lua, type), i, nargs);
                     break;
               }

               i++;
            }

            // Add TAGEND just in case it's needed the developer forgot it

            ((LARGE *)(buffer + j))[0] = TAGEND;
            j += sizeof(LARGE);
*/
         }
         break; // Tags must always be the last entry of a function, so break to enforce this
      }
      else if (argtype & FD_FUNCTION) {
         type = lua_type(Lua, i);

         if (type IS LUA_TSTRING) { // Name of function to call
            lua_getglobal(Lua, lua_tostring(Lua, i));
            SET_FUNCTION_SCRIPT(func, &Self->Head, luaL_ref(Lua, LUA_REGISTRYINDEX));
            ((FUNCTION **)(buffer + j))[0] = &func;
         }
         else if (type IS LUA_TFUNCTION) { // Direct function reference
            lua_pushvalue(Lua, i);
            SET_FUNCTION_SCRIPT(func, &Self->Head, luaL_ref(Lua, LUA_REGISTRYINDEX));
            ((FUNCTION **)(buffer + j))[0] = &func;
         }
         else if ((type IS LUA_TNIL) OR (type IS LUA_TNONE)) {
            ((FUNCTION **)(buffer + j))[0] = NULL;
         }
         else {
            luaL_error(Lua, "Type mismatch, arg #%d (%s) expected function, got %s '%s'.", i, args[i].Name, lua_typename(Lua, lua_type(Lua, i)), lua_tostring(Lua, i));
            return 0;
         }

         fptr[in] = buffer + j;
         fin[in++] = &ffi_type_pointer;
         j += sizeof(FUNCTION *);
      }
      else if (argtype & FD_STR) {
         type = lua_type(Lua, i);

         if ((type IS LUA_TSTRING) OR (type IS LUA_TNUMBER) OR (type IS LUA_TBOOLEAN)) {
            ((CSTRING *)(buffer + j))[0] = lua_tostring(Lua, i);
         }
         else if (type <= 0) {
            ((CSTRING *)(buffer + j))[0] = NULL;
         }
         else if ((type IS LUA_TUSERDATA) OR (type IS LUA_TLIGHTUSERDATA)) {
            luaL_error(Lua, "Arg #%d (%s) requires a string and not untyped pointer.", i, args[i].Name, lua_typename(Lua, lua_type(Lua, i)), lua_tostring(Lua, i));
            return 0;
         }
         else {
            luaL_error(Lua, "Type mismatch, arg #%d (%s) expected string, got %s '%s'.", i, args[i].Name, lua_typename(Lua, lua_type(Lua, i)), lua_tostring(Lua, i));
            return 0;
         }

         fptr[in] = buffer + j;
         fin[in++] = &ffi_type_pointer;
         j += sizeof(STRING);
      }
      else if (argtype & FD_PTR) {
         struct memory *memory;
         struct fstruct *fstruct;
         struct object *obj;

         type = lua_type(Lua, i);
         if (type IS LUA_TSTRING) {
            // Lua strings need to be converted to C strings
            size_t strlen;
            ((CSTRING *)(buffer + j))[0] = lua_tolstring(Lua, i, &strlen);
            fptr[in] = buffer + j;
            fin[in++] = &ffi_type_pointer;
            j += sizeof(CSTRING);

            if (args[i+1].Type & FD_BUFSIZE) {
               if (args[i+1].Type & FD_LONG) {
                  ((LONG *)(buffer + j))[0] = strlen;
                  i++;
                  fptr[in] = buffer + j;
                  fin[in++] = &ffi_type_sint32;
                  j += sizeof(LONG);
               }
               else if (args[i+1].Type & FD_LARGE) {
                  ((LARGE *)(buffer + j))[0] = strlen;
                  i++;
                  fptr[in] = buffer + j;
                  fin[in++] = &ffi_type_sint64;
                  j += sizeof(LARGE);
               }
            }
         }
         else if ((memory = get_meta(Lua, i, "Fluid.mem"))) {
            ((APTR *)(buffer + j))[0] = memory->Address;
            fptr[in] = buffer + j;
            fin[in++] = &ffi_type_pointer;
            j += sizeof(APTR);

            if (args[i+1].Type & FD_BUFSIZE) {
               if (args[i+1].Type & FD_LONG) {
                  ((LONG *)(buffer + j))[0] = memory->MemorySize;
                  i++;
                  fptr[in] = buffer + j;
                  fin[in++] = &ffi_type_sint32;
                  j += sizeof(LONG);
               }
               else if (args[i+1].Type & FD_LARGE) {
                  ((LARGE *)(buffer + j))[0] = memory->MemorySize;
                  i++;
                  fptr[in] = buffer + j;
                  fin[in++] = &ffi_type_sint64;
                  j += sizeof(LARGE);
               }
            }
         }
         else if ((fstruct = get_meta(Lua, i, "Fluid.struct"))) {
            ((APTR *)(buffer + j))[0] = fstruct->Data;
            fptr[in] = buffer + j;
            fin[in++] = &ffi_type_pointer;
            j += sizeof(APTR);

            FMSG("module_call","Struct address %p inserted to arg offset %d", fstruct->Data, j);
            if (args[i+1].Type & FD_BUFSIZE) {
               if (args[i+1].Type & FD_LONG) {
                  ((LONG *)(buffer + j))[0] = fstruct->AlignedSize;
                  i++;
                  fptr[in] = buffer + j;
                  fin[in++] = &ffi_type_sint32;
                  j += sizeof(LONG);
               }
               else if (args[i+1].Type & FD_LARGE) {
                  ((LARGE *)(buffer + j))[0] = fstruct->AlignedSize;
                  i++;
                  fptr[in] = buffer + j;
                  fin[in++] = &ffi_type_sint64;
                  j += sizeof(LARGE);
               }
            }
         }
         else if ((obj = get_meta(Lua, i, "Fluid.obj"))) {
            OBJECTPTR ptr_obj;

            if (obj->prvObject) {
               ((OBJECTPTR *)(buffer + j))[0] = obj->prvObject;
            }
            else if ((ptr_obj = access_object(obj))) {
               ((OBJECTPTR *)(buffer + j))[0] = ptr_obj;
               release_object(obj);
            }
            else {
               LogF("@","Unable to resolve object pointer for #%d.", obj->ObjectID);
               ((OBJECTPTR *)(buffer + j))[0] = NULL;
            }

            fptr[in] = buffer + j;
            fin[in++] = &ffi_type_pointer;
            j += sizeof(APTR);
         }
         else {
            ((APTR *)(buffer + j))[0] = lua_touserdata(Lua, i); //lua_topointer?
            fptr[in] = buffer + j;
            fin[in++] = &ffi_type_pointer;
            j += sizeof(APTR);
         }
      }
      else if (argtype & FD_LONG) {
         if (argtype & FD_OBJECT) {
            struct object *obj;
            if ((obj = get_meta(Lua, i, "Fluid.obj"))) {
               ((LONG *)(buffer + j))[0] = obj->ObjectID;
            }
            else ((LONG *)(buffer + j))[0] = F2I(lua_tonumber(Lua, i));
         }
         else ((LONG *)(buffer + j))[0] = F2I(lua_tonumber(Lua, i));
         fptr[in] = buffer + j;
         fin[in++] = &ffi_type_sint32;
         j += sizeof(LONG);
      }
      else if (argtype & FD_DOUBLE) {
         ((DOUBLE *)(buffer + j))[0] = lua_tonumber(Lua, i);
         fptr[in] = buffer + j;
         fin[in++] = &ffi_type_double;
         j += sizeof(DOUBLE);
      }
      else if (argtype & FD_LARGE) {
         ((LARGE *)(buffer + j))[0] = lua_tonumber(Lua, i);
         fptr[in] = buffer + j;
         fin[in++] = &ffi_type_sint64;
         j += sizeof(LARGE);
      }
      else if (argtype & FD_PTRSIZE) {
         ((LONG *)(buffer + j))[0] = F2I(lua_tonumber(Lua, i));
         fptr[in] = buffer + j;
         fin[in++] = &ffi_type_sint32;
         j += sizeof(LONG);
      }
      else {
         LogF("@module_call_i32","%s() unsupported arg '%s', flags $%.8x, aborting now.", mod->Functions[index].Name, args[i].Name, argtype);
         return 0;
      }
   }

   // Call the function.  The method used for execution depends on the function's result type.

   LONG restype = args->Type;
   LONG result = 1;
   LONG total_args = i - 1;

   if (restype & FD_STR) {
      if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_pointer, fin) IS FFI_OK) {
         ffi_call(&cif, function, &rc, fptr);
         lua_pushstring(Lua, (CSTRING)rc);
      }
      else lua_pushnil(Lua);
   }
   else if (restype & FD_OBJECT) {
      if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_pointer, fin) IS FFI_OK) {
         ffi_call(&cif, function, &rc, fptr);
         if ((OBJECTPTR)rc) {
            struct object *obj = push_object(Lua, (OBJECTPTR)rc);
            if (restype & FD_ALLOC) obj->Detached = FALSE;
         }
         else lua_pushnil(Lua);
      }
      else lua_pushnil(Lua);
   }
   else if (restype & FD_PTR) {
      if (restype & FD_STRUCT) {
         if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_pointer, fin) IS FFI_OK) {
            ffi_call(&cif, function, &rc, fptr);
            APTR structptr = (APTR)rc;
            if (structptr) {
               ERROR error;
               if (restype & FD_RESOURCE) push_struct(Self, structptr, args->Name, (restype & FD_ALLOC) ? TRUE : FALSE);
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
      else if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_pointer, fin) IS FFI_OK) {
         ffi_call(&cif, function, &rc, fptr);
         if ((APTR)rc) lua_pushlightuserdata(Lua, (APTR)rc);
         else lua_pushnil(Lua);
      }
      else lua_pushnil(Lua);
   }
   else if (restype & (FD_LONG|FD_ERROR)) {
      if (restype & FD_UNSIGNED) {
         if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_uint32, fin) IS FFI_OK) {
            ffi_call(&cif, function, &rc, fptr);
            lua_pushnumber(Lua, (ULONG)rc);
         }
         else lua_pushnil(Lua);
      }
      else {
         if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_sint32, fin) IS FFI_OK) {
            ffi_call(&cif, function, &rc, fptr);
            lua_pushinteger(Lua, (LONG)rc);

            if ((prv->Catch) AND (restype & FD_ERROR) AND (rc >= ERR_ExceptionThreshold)) {
               prv->CaughtError = rc;
               luaL_error(prv->Lua, GetErrorMsg(rc));
            }
         }
         else lua_pushnil(Lua);
      }
   }
   else if (restype & FD_DOUBLE) {
      if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_double, fin) IS FFI_OK) {
         ffi_call(&cif, function, &rc, fptr);
         lua_pushnumber(Lua, (DOUBLE)rc);
      }
      else lua_pushnil(Lua);
   }
   else if (restype & FD_LARGE) {
      if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_sint64, fin) IS FFI_OK) {
         ffi_call(&cif, function, &rc, fptr);
         lua_pushnumber(Lua, (LARGE)rc);
      }
      else lua_pushnil(Lua);
   }
   else { // Void
      if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, total_args, &ffi_type_void, fin) IS FFI_OK) {
         ffi_call(&cif, function, &rc, fptr);
      }
      result = 0;
   }

   if (j >= sizeof(buffer)-8) {
      luaL_error(Lua, "Too many arguments - buffer overflow.");
      return 0;
   }

   return process_results(prv, buffer, args, result);
}

/*****************************************************************************
** This code looks for FD_RESULT arguments in the function's parameter list and converts them into multiple Fluid results.
*/

static LONG process_results(struct prvFluid *prv, APTR resultsidx, const struct FunctionField *args, LONG result)
{
   LONG i;
   APTR var;

   UBYTE *scan = resultsidx;
   for (i=1; args[i].Name; i++) {
      LONG argtype = args[i].Type;
      CSTRING argname = args[i].Name;

      if (argtype & FD_RESULT) {
         var = ((APTR *)scan)[0];
         RMSG("Result Arg %d @ %p", i, var);
      }
      else var = NULL;

      if (argtype & FD_ARRAY) {
         scan += sizeof(APTR);
         if (argtype & FD_RESULT) {
            if (var) {
               APTR values = ((APTR *)var)[0];
               LONG total_elements = -1; // If -1, make_any_table() assumes the array is null terminated.

               if (args[i+1].Type & FD_ARRAYSIZE) {
                  const APTR size_var = ((APTR *)scan)[0];
                  if (args[i+1].Type & FD_LONG) total_elements = ((LONG *)size_var)[0];
                  else if (args[i+1].Type & FD_LARGE) total_elements = ((LARGE *)size_var)[0];
                  else LogErrorMsg("Invalid arg %s, flags $%.8x", args[i+1].Name, args[i+1].Type);
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
               if ((argtype & FD_ALLOC) AND (((STRING *)var)[0])) FreeResource(((STRING *)var)[0]);
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
                  RMSG("Result-Arg: %s, Value: %p (Object)", argname, ((APTR *)var)[0]);
                  if (((APTR *)var)[0]) {
                     struct object *obj = push_object(prv->Lua, ((APTR *)var)[0]);
                     if (argtype & FD_ALLOC) obj->Detached = FALSE;
                  }
                  else lua_pushnil(prv->Lua);
               }
               else if (argtype & FD_STRUCT) {
                  if (((APTR *)var)[0]) {
                     if (argtype & FD_RESOURCE) {
                        // Resource structures are managed with direct data addresses.
                        push_struct(prv->Lua->Script, ((APTR *)var)[0], args[i].Name, (argtype & FD_ALLOC) ? TRUE : FALSE);
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
                     else LogErrorMsg("Invalid arg %s, flags $%.8x", args[i+1].Name, args[i+1].Type);
                  }
                  else {
                     struct MemInfo meminfo;
                     if (!MemoryPtrInfo(((APTR *)var)[0], &meminfo)) size = meminfo.Size;
                  }

                  if (size > 0) lua_pushlstring(prv->Lua, ((APTR *)var)[0], size);
                  else lua_pushnil(prv->Lua);

                  if (((APTR *)var)[0]) FreeResource(((APTR *)var)[0]);
               }
               else if (args[i+1].Type & FD_BUFSIZE) { // We can convert the data to a binary string rather than work with unsafe pointers.
                  LARGE size = 0;
                  const APTR size_var = ((APTR *)scan)[0];
                  if (args[i+1].Type & FD_LONG) size = ((LONG *)size_var)[0];
                  else if (args[i+1].Type & FD_LARGE) size = ((LARGE *)size_var)[0];
                  else LogErrorMsg("Invalid arg %s, flags $%.8x", args[i+1].Name, args[i+1].Type);

                  if (size > 0) lua_pushlstring(prv->Lua, ((APTR *)var)[0], size);
                  else lua_pushnil(prv->Lua);
               }
               else {
                  RMSG("Result-Arg: %s, Value: %p (Pointer)", argname, ((APTR *)var)[0]);
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
               RMSG("Result-Arg: %s, Value: " PF64() " (Large)", argname, ((LARGE *)var)[0]);
               lua_pushnumber(prv->Lua, ((LARGE *)var)[0]);
            }
            else lua_pushnil(prv->Lua);
            result++;
         }
         else scan += sizeof(LARGE);
      }
      else if (argtype & (FD_VARTAGS|FD_TAGS)) {
         // Tags come last and have no result
         break;
      }
      else {
         LogF("@process_results","Unsupported arg '%s', flags $%x, aborting now.", argname, argtype);
         break;
      }
   }

   RMSG("module_call: Wrote %d results.", result);

   return result;
}

/*****************************************************************************
** Register the module interface.
*/

static void register_module_class(lua_State *Lua)
{
   static const struct luaL_reg modlib_functions[] = {
      { "new",  module_load },
      { "load", module_load },
      { NULL, NULL}
   };

   static const struct luaL_reg modlib_methods[] = {
      { "__index",    module_index },
      { "__tostring", module_tostring },
      { "__gc",       module_destruct },
      { NULL, NULL }
   };

   MSG("Registering module interface.");

   luaL_newmetatable(Lua, "Fluid.mod");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable
   lua_settable(Lua, -3);   // metatable.__index = metatable

   luaL_openlib(Lua, NULL, modlib_methods, 0);
   luaL_openlib(Lua, "mod", modlib_functions, 0);
}
