
//********************************************************************************************************************
// Lua C closure executed via calls to obj.acName()

static int object_action_call_args(lua_State *Lua)
{
   auto object = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj");
   LONG action_id = lua_tointeger(Lua, lua_upvalueindex(2));
   bool release = false;

   BYTE argbuffer[glActions[action_id].Size+8]; // +8 for overflow protection in build_args()
   ERROR error = build_args(Lua, glActions[action_id].Args, glActions[action_id].Size, argbuffer, NULL);
   if (error) {
      luaL_error(Lua, "Argument build failure for %s.", glActions[action_id].Name);
      return 0;
   }

   LONG results = 1;
   if (object->DelayCall) {
      object->DelayCall = false;
      error = QueueAction(action_id, object->UID, argbuffer);
      lua_pushinteger(Lua, error);
   }
   else {
      if (object->ObjectPtr) error = Action(action_id, object->ObjectPtr, argbuffer);
      else if (auto obj = access_object(object)) {
         error = Action(action_id, obj, argbuffer);
         release = true;
      }
      else error = ERR_AccessObject;

      // NB: Even if an error is returned, always get the results (any results parameters are nullified prior to
      // function entry and the action can return results legitimately even if an error code is returned - e.g.
      // quite common when returning ERR_Terminate).

      lua_pushinteger(Lua, error);
      results += get_results(Lua, glActions[action_id].Args, argbuffer);
   }

   if (release) release_object(object);
   report_action_error(Lua, object, glActions[action_id].Name, error);
   return results;
}

// This variant is for actions that take no parameters.

static int object_action_call(lua_State *Lua)
{
   auto def = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj");
   LONG action_id = lua_tointeger(Lua, lua_upvalueindex(2));
   ERROR error;
   bool release = false;

   if (def->DelayCall) {
      def->DelayCall = false;
      error = QueueAction(action_id, def->UID);
   }
   else if (def->ObjectPtr) error = Action(action_id, def->ObjectPtr, NULL);
   else if (auto obj = access_object(def)) {
      error = Action(action_id, obj, NULL);
      release = true;
   }
   else error = ERR_AccessObject;

   lua_pushinteger(Lua, error);

   if (release) release_object(def);
   report_action_error(Lua, def, glActions[action_id].Name, error);
   return 1;
}

//********************************************************************************************************************
// Lua C closure executed via calls to obj.mtName()

static int object_method_call_args(lua_State *Lua)
{
   auto def = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj");
   auto method = (MethodEntry *)lua_touserdata(Lua, lua_upvalueindex(2));

   BYTE argbuffer[method->Size+8]; // +8 for overflow protection in build_args()
   LONG resultcount;
   ERROR error = build_args(Lua, method->Args, method->Size, argbuffer, &resultcount);
   if (error) {
      luaL_error(Lua, "Argument build failure for method %s.", method->Name);
      return 0;
   }

   LONG results = 1;
   bool release = false;
   if (def->DelayCall) {
      def->DelayCall = false;
      error = QueueAction(method->MethodID, def->UID, (APTR)&argbuffer);
      lua_pushinteger(Lua, error);
   }
   else {
      if (def->ObjectPtr) error = Action(method->MethodID, def->ObjectPtr, &argbuffer);
      else if (auto obj = access_object(def)) {
         error = Action(method->MethodID, obj, argbuffer);
         release = true;
      }
      else error = ERR_AccessObject;

      lua_pushinteger(Lua, error);

      if (!def->DelayCall) results += get_results(Lua, method->Args, (const BYTE *)argbuffer);
   }

   if (release) release_object(def);
   report_action_error(Lua, def, method->Name, error);
   return results;
}

// This variant is for methods that take no parameters.

static int object_method_call(lua_State *Lua)
{
   auto def = (object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj");
   ERROR error;
   bool release = false;
   auto method = (MethodEntry *)lua_touserdata(Lua, lua_upvalueindex(2));

   if (def->DelayCall) {
      def->DelayCall = false;
      error = QueueAction(method->MethodID, def->UID);
   }
   else if (def->ObjectPtr) error = Action(method->MethodID, def->ObjectPtr, NULL);
   else if (auto obj = access_object(def)) {
      error = Action(method->MethodID, obj, NULL);
      release = true;
   }
   else error = ERR_AccessObject;

   lua_pushinteger(Lua, error);

   if (release) release_object(def);
   report_action_error(Lua, def, method->Name, error);
   return 1;
}

//********************************************************************************************************************
// Build argument buffer for actions and methods.

ERROR build_args(lua_State *Lua, const FunctionField *args, LONG ArgsSize, BYTE *argbuffer, LONG *ResultCount)
{
   pf::Log log(__FUNCTION__);

   LONG top = lua_gettop(Lua);

   log.traceBranch("%d, %p, Top: %d", ArgsSize, argbuffer, top);

   ClearMemory(argbuffer, ArgsSize);

   LONG i, n;
   LONG resultcount = 0;
   LONG j = 0;
   for (i=0,n=1; (args[i].Name) and (j < ArgsSize) and (top > 0); i++,n++,top--) {
      LONG type = lua_type(Lua, n);

      if (args[i].Type & FD_RESULT) resultcount = resultcount + 1;

      //log.trace("Processing arg %s, type $%.8x", args[i].Name, args[i].Type);

      if ((args[i].Type & FD_BUFFER) or (args[i+1].Type & FD_BUFSIZE)) {
         #ifdef _LP64
            j = ALIGN64(j);
         #endif
         if (auto memory = (struct memory *)get_meta(Lua, n, "Fluid.mem")) {
            //log.trace("Arg: %s, Value: Buffer (Source is Memory)", args[i].Name);

            ((APTR *)(argbuffer + j))[0] = memory->Memory;
            j += sizeof(APTR);

            if (args[i+1].Type & FD_BUFSIZE) {
               // Buffer size is optional, so set the buffer size parameter by default.  The user can override it if
               // more arguments are specified in the function call.

               LONG memsize = memory->MemorySize;
               if (args[i+1].Type & FD_LONG)  ((LONG *)(argbuffer + j))[0] = memsize;
               else if (args[i+1].Type & FD_LARGE) ((LARGE *)(argbuffer + j))[0] = memsize;
               //log.trace("Preset buffer size of %d bytes.", memsize);
            }

            //n--; // Adjustment required due to successful get_meta()
         }
         else if (auto fstruct = (struct fstruct *)get_meta(Lua, n, "Fluid.struct")) {
            //log.trace("Arg: %s, Value: Buffer (Source is a struct)", args[i].Name);

            ((APTR *)(argbuffer + j))[0] = fstruct->Data;
            j += sizeof(APTR);

            if (args[i+1].Type & FD_BUFSIZE) {
               // Buffer size is optional, so set the buffer size parameter by default.
               // The user can override it if more arguments are specified in the function call.

               if (args[i+1].Type & FD_LONG) ((LONG *)(argbuffer + j))[0] = fstruct->AlignedSize;
               else if (args[i+1].Type & FD_LARGE) ((LARGE *)(argbuffer + j))[0] = fstruct->AlignedSize;
            }
            n--; // Adjustment required due to successful get_meta()
         }
         else if (auto farray = (struct array *)get_meta(Lua, n, "Fluid.array")) {
            //log.trace("Arg: %s, Value: Buffer (Source is a array)", args[i].Name);

            ((APTR *)(argbuffer + j))[0] = farray->ptrPointer;
            j += sizeof(APTR);

            if (args[i+1].Type & FD_BUFSIZE) {
               // Buffer size is optional, so set the buffer size parameter by default.  The user can
               // override it if more arguments are specified in the function call.

               //log.trace("Advance setting of following BUFSIZE parameter to %d", farray->ArraySize);

               if (args[i+1].Type & FD_LONG) ((LONG *)(argbuffer + j))[0] = farray->ArraySize;
               else if (args[i+1].Type & FD_LARGE) ((LARGE *)(argbuffer + j))[0] = farray->ArraySize;
               else log.trace("Cannot set BUFSIZE argument - unknown type.");
            }
            n--; // Adjustment required due to successful get_meta()
         }
         else if (type IS LUA_TSTRING) {
            //log.trace("Arg: %s, Value: Buffer (Source is String)", args[i].Name);
            size_t len;
            CSTRING str = lua_tolstring(Lua, n, &len);
            ((CSTRING *)(argbuffer + j))[0] = str;
            j += sizeof(APTR);

            if (args[i+1].Type & FD_BUFSIZE) {
               if (args[i+1].Type & FD_LONG) ((LONG *)(argbuffer + j))[0] = len;
               else if (args[i+1].Type & FD_LARGE) ((LARGE *)(argbuffer + j))[0] = len;
            }
         }
         else if (type IS LUA_TNUMBER) {
            luaL_argerror(Lua, n, "Cannot use a number as a buffer pointer.");
            return ERR_WrongType;
         }
         else {
            //log.trace("Arg: %s, Value: Buffer", args[i].Name);
            ((APTR *)(argbuffer + j))[0] = lua_touserdata(Lua, n);
            j += sizeof(APTR);
         }
      }
      else if (args[i].Type & FD_STR) {
         #ifdef _LP64
            j = ALIGN64(j);
         #endif
         if ((type IS LUA_TSTRING) or (type IS LUA_TNUMBER)) {
            ((CSTRING *)(argbuffer + j))[0] = lua_tostring(Lua, n);
         }
         else if (type <= 0) {
            ((CSTRING *)(argbuffer + j))[0] = NULL;
         }
         else if ((type IS LUA_TUSERDATA) or (type IS LUA_TLIGHTUSERDATA)) {
            luaL_error(Lua, "Arg #%d (%s) requires a string and not untyped pointer.", i, args[i].Name);
         }
         else luaL_error(Lua, "Arg #%d (%s) requires a string, got %s '%s'.", i, args[i].Name, lua_typename(Lua, type), lua_tostring(Lua, n));

         //log.trace("Arg: %s, Value: %s", args[i].Name, ((STRING *)(argbuffer + j))[0]);

         j += sizeof(STRING);
      }
      else if (args[i].Type & FD_PTR) {
         #ifdef _LP64
            j = ALIGN64(j);
         #endif
         if (args[i].Type & FD_OBJECT) {
            struct object *object;
            if ((object = (struct object *)get_meta(Lua, n, "Fluid.obj"))) {
               OBJECTPTR ptr_obj;
               if (object->ObjectPtr) {
                  ((OBJECTPTR *)(argbuffer + j))[0] = object->ObjectPtr;
               }
               else if ((ptr_obj = access_object(object))) {
                  ((OBJECTPTR *)(argbuffer + j))[0] = ptr_obj;
                  release_object(object);
               }
               else {
                  log.warning("Unable to resolve object pointer for #%d.", object->UID);
                  ((OBJECTPTR *)(argbuffer + j))[0] = NULL;
               }
            }
            else ((OBJECTPTR *)(argbuffer + j))[0] = NULL;
         }
         else if (args[i].Type & FD_FUNCTION) {
            if ((type IS LUA_TSTRING) or (type IS LUA_TFUNCTION)) {
               FUNCTION *func;

               if (!AllocMemory(sizeof(FUNCTION), MEM::DATA, &func)) {
                  if (type IS LUA_TSTRING) {
                     lua_getglobal(Lua, lua_tostring(Lua, n));
                     *func = make_function_script(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
                  }
                  else {
                     lua_pushvalue(Lua, n);
                     *func = make_function_script(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
                  }

                  ((FUNCTION **)(argbuffer + j))[0] = func;

                  // The FUNCTION structure is freed when processing results
               }
               else luaL_error(Lua, "Memory allocation error.");
            }
            else luaL_error(Lua, "Arg #%d (%s) requires a string or function, got %s '%s'.", i, args[i].Name, lua_typename(Lua, type), lua_tostring(Lua, n));
         }
         else if (type IS LUA_TSTRING) {
            //log.trace("Arg: %s, Value: Pointer (Source is String)", args[i].Name);
            ((CSTRING *)(argbuffer + j))[0] = lua_tostring(Lua, n);
         }
         else if (type IS LUA_TNUMBER) {
            luaL_argerror(Lua, n, "Unable to convert number to a pointer.");
            return ERR_WrongType;
         }
         else {
            //log.trace("Arg: %s, Value: Pointer, SrcType: %s", args[i].Name, lua_typename(Lua, type));

            if (auto memory = (struct memory *)get_meta(Lua, n, "Fluid.mem")) {
               ((APTR *)(argbuffer + j))[0] = memory->Memory;
               //n--; // Adjustment required due to successful get_meta()
            }
            else if (auto fstruct = (struct fstruct *)get_meta(Lua, n, "Fluid.struct")) {
               ((APTR *)(argbuffer + j))[0] = fstruct->Data;
               //n--; // Adjustment required due to successful get_meta()
            }
            else ((APTR *)(argbuffer + j))[0] = lua_touserdata(Lua, n);
         }

         j += sizeof(APTR);
      }
      else if (args[i].Type & FD_LONG) {
         if ((type IS LUA_TUSERDATA) or (type IS LUA_TLIGHTUSERDATA)) {
            if (auto obj = (struct object *)get_meta(Lua, n, "Fluid.obj")) {
               ((LONG *)(argbuffer + j))[0] = obj->UID;
            }
            else luaL_argerror(Lua, n, "Unable to convert usertype to an integer.");
         }
         else if (type IS LUA_TBOOLEAN) ((LONG *)(argbuffer + j))[0] = lua_toboolean(Lua, n);
         else if (type != LUA_TNIL) ((LONG *)(argbuffer + j))[0] = lua_tointeger(Lua, n);
         else if (args[i].Type & FD_BUFSIZE); // Do not alter as the FD_BUFFER support would have managed it
         else ((LONG *)(argbuffer + j))[0] = 0;
         //log.trace("Arg: %s, Value: %d / $%.8x", args[i].Name, ((LONG *)(argbuffer + j))[0], ((LONG *)(argbuffer + j))[0]);
         j += sizeof(LONG);
      }
      else if (args[i].Type & FD_DOUBLE) {
         j = ALIGN64(j);
         ((DOUBLE *)(argbuffer + j))[0] = lua_tonumber(Lua, n);
         //log.trace("Arg: %s, Value: %.2f", args[i].Name, ((DOUBLE *)(argbuffer + j))[0]);
         j += sizeof(DOUBLE);
      }
      else if (args[i].Type & FD_LARGE) {
         j = ALIGN64(j);
         ((LARGE *)(argbuffer + j))[0] = lua_tointeger(Lua, n);
         //log.trace("Arg: %s, Value: %" PF64, args[i].Name, ((LARGE *)(argbuffer + j))[0]);
         j += sizeof(LARGE);
      }
      else {
         log.warning("Unsupported arg %s, flags $%.8x, aborting now.", args[i].Name, args[i].Type);
         return ERR_WrongType;
      }
   }

   // Finish counting the number of result types registered in the argument list

   for (; (args[i].Name); i++) {
      if (args[i].Type & FD_RESULT) resultcount = resultcount + 1;
   }

   log.trace("Processed %d args (%d bytes), detected %d result parameters.", i, j, resultcount);
   if (ResultCount) *ResultCount = resultcount;
   return ERR_Okay;
}

// Note: Please refer to process_results() in fluid_module.c for the 'official' take on result handling.

static LONG get_results(lua_State *Lua, const FunctionField *args, const BYTE *ArgBuf)
{
   pf::Log log(__FUNCTION__);
   LONG i;

   RMSG("get_results(%p)", ArgBuf);

   LONG total = 0;
   LONG of = 0;
   for (i=0; args[i].Name; i++) {
      LONG type = args[i].Type;
      if (type & FD_ARRAY) { // Pointer to an array.
         if (sizeof(APTR) IS 8) of = ALIGN64(of);
         if (type & FD_RESULT) {
            LONG total_elements = -1;  // If -1, make_any_table() assumes the array is null terminated.
            if (args[i+1].Type & FD_ARRAYSIZE) {
               const APTR size_var = ((APTR *)(ArgBuf + of + sizeof(APTR)))[0];
               if (args[i+1].Type & FD_LONG) total_elements = ((LONG *)size_var)[0];
               else if (args[i+1].Type & FD_LARGE) total_elements = ((LARGE *)size_var)[0];
               else log.warning("Invalid parameter definition for '%s' of $%.8x", args[i+1].Name, args[i+1].Type);
            }

            CPTR values = ((APTR *)(ArgBuf + of))[0];
            if (values) {
               make_any_table(Lua, type, args[i].Name, total_elements, values);
               if (type & FD_ALLOC) FreeResource(values);
            }
            else lua_pushnil(Lua);
            total++;
         }
         of += sizeof(APTR);
      }
      else if (type & FD_STR) {
         if (sizeof(APTR) IS 8) of = ALIGN64(of);
         if (type & FD_RESULT) {
            CPTR val = ArgBuf + of;
            RMSG("Result-Arg: %s, Value: %.20s (String)", args[i].Name, ((STRING *)val)[0]);
            lua_pushstring(Lua, ((STRING *)val)[0]);
            if (type & FD_ALLOC) {
               APTR ptr = ((STRING *)val)[0];
               if (ptr) FreeResource(ptr);
            }
            total++;
         }
         of += sizeof(STRING);
      }
      else if (type & FD_STRUCT) { // Pointer to a struct
         if (sizeof(APTR) IS 8) of = ALIGN64(of);
         if (type & FD_RESULT) {
            APTR ptr_struct = ((APTR *)(ArgBuf + of))[0];
            RMSG("Result-Arg: %s, Struct: %p", args[i].Name, ptr_struct);
            if (ptr_struct) {
               if (type & FD_RESOURCE) {
                  push_struct(Lua->Script, ptr_struct, args[i].Name, (type & FD_ALLOC) ? true : false, false);
               }
               else {
                  if (named_struct_to_table(Lua, args[i].Name, ptr_struct) != ERR_Okay) {
                     luaL_error(Lua, "Failed to create struct for %s, %p", args[i].Name, ptr_struct);
                     return total;
                  }
                  if (type & FD_ALLOC) FreeResource(ptr_struct);
               }
            }
            else lua_pushnil(Lua);

            total++;
         }
         of += sizeof(APTR);
      }
      else if (type & FD_PTR) {
         if (sizeof(APTR) IS 8) of = ALIGN64(of);
         if (type & FD_FUNCTION) {
            FUNCTION *func;
            if ((func = (FUNCTION *)((APTR *)(ArgBuf+of))[0])) {
               log.trace("Removing function memory allocation %p", func);
               FreeResource(func);
            }
         }
         else if (type & FD_RESULT) {
            if (type & FD_OBJECT) {
               auto obj = (OBJECTPTR)((APTR *)(ArgBuf+of))[0];

               RMSG("Result-Arg: %s, Value: %p (Object)", args[i].Name, obj);

               if (obj) {
                  auto new_obj = push_object(Lua, obj);
                  new_obj->Detached = (type & FD_ALLOC) ? false : true;
               }
               else lua_pushnil(Lua);
            }
            else if (type & FD_RGB) {
               auto rgb = (RGB8 *)((APTR *)(ArgBuf+of))[0];

               if (rgb) { // This return type is untested
                  lua_newtable(Lua);
                  lua_pushstring(Lua, "r");
                  lua_pushnumber(Lua, rgb->Red);
                  lua_settable(Lua, -3);

                  lua_pushstring(Lua, "g");
                  lua_pushnumber(Lua, rgb->Green);
                  lua_settable(Lua, -3);

                  lua_pushstring(Lua, "b");
                  lua_pushnumber(Lua, rgb->Blue);
                  lua_settable(Lua, -3);

                  lua_pushstring(Lua, "a");
                  lua_pushnumber(Lua, rgb->Alpha);
                  lua_settable(Lua, -3);
               }
               else lua_pushnil(Lua);
            }
            else {
               RMSG("Result-Arg: %s, Value: %p (Pointer)", args[i].Name, ((APTR *)(ArgBuf+of))[0]);
               lua_pushlightuserdata(Lua, ((APTR *)(ArgBuf+of))[0]);
            }
            total++;
         }
         of += sizeof(APTR);
      }
      else if (type & FD_LONG) {
         if (type & FD_RESULT) {
            RMSG("Result-Arg: %s, Value: %d (Long)", args[i].Name, ((LONG *)(ArgBuf+of))[0]);
            lua_pushinteger(Lua, ((LONG *)(ArgBuf+of))[0]);
            total++;
         }
         of += sizeof(LONG);
      }
      else if (type & FD_DOUBLE) {
         of = ALIGN64(of);
         if (type & FD_RESULT) {
            RMSG("Result-Arg: %s, Offset: %d, Value: %.2f (Double)", args[i].Name, of, ((DOUBLE *)(ArgBuf+of))[0]);
            lua_pushnumber(Lua, ((DOUBLE *)(ArgBuf+of))[0]);
            total++;
         }
         of += sizeof(DOUBLE);
      }
      else if (type & FD_LARGE) {
         of = ALIGN64(of);
         if (type & FD_RESULT) {
            RMSG("Result-Arg: %s, Value: %" PF64 " (Large)", args[i].Name, ((LARGE *)(ArgBuf+of))[0]);
            lua_pushnumber(Lua, ((LARGE *)(ArgBuf+of))[0]);
            total++;
         }
         of += sizeof(LARGE);
      }
      else if (type & FD_TAGS) {
         // Tags come last and have no result
         break;
      }
      else {
         log.warning("Unsupported arg %s, flags $%x, aborting now.", args[i].Name, type);
         break;
      }
   }

   RMSG("get_results: Wrote %d args.", total);
   return total;
}
