
//****************************************************************************
// Lua C closure executed via calls to obj.acName() or obj.mtName()

static int object_call(lua_State *Lua)
{
   parasol::Log log(__FUNCTION__);

   struct object *object;
   if (!(object = (struct object *)get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_error(Lua, "object_call() expected object in upvalue.");
      return 0;
   }

   LONG action_id = lua_tointeger(Lua, lua_upvalueindex(2));

   log.traceBranch("#%d/%p, Action: %d", object->ObjectID, object->prvObject, action_id);

   ERROR error = ERR_Okay;
   LONG results = 0;
   BYTE release = FALSE;
   CSTRING action_name = NULL;
   if (action_id >= 0) {
      action_name = glActions[action_id].Name;
      if ((glActions[action_id].Args) and (glActions[action_id].Size)) {
         BYTE argbuffer[glActions[action_id].Size+8]; // +8 for overflow protection in build_args()

         LONG resultcount;
         if (!(error = build_args(Lua, glActions[action_id].Args, glActions[action_id].Size, argbuffer, &resultcount))) {
            if (object->DelayCall) {
               error = DelayMsg(action_id, object->ObjectID, argbuffer);
            }
            else if (object->prvObject) {
               error = Action(action_id, object->prvObject, argbuffer);
            }
            else {
               // If the action returns results, we need to execute locally to pick up any results (e.g. string
               // pointers).  Otherwise, we can execute via messaging.

               if (resultcount > 0) {
                  OBJECTPTR obj;
                  if ((obj = access_object(object))) {
                     error = Action(action_id, obj, argbuffer);
                     release = TRUE;
                  }
               }
               else error = ActionMsg(action_id, object->ObjectID, argbuffer);
            }
         }
         else {
            luaL_error(Lua, "Argument build failure for %s.", glActions[action_id].Name);
            return 0;
         }

         lua_pushinteger(Lua, error);
         results = 1;

         // NB: Even if an error is returned, always get the results (any results parameters are nullified prior to
         // function entry and the action can return results legitimately even if an error code is returned - e.g.
         // quite common when returning ERR_Terminate).

         if (!object->DelayCall) results += get_results(Lua, glActions[action_id].Args, argbuffer);
         else object->DelayCall = FALSE;

         if (release) release_object(object);
      }
      else {
         if (object->DelayCall) {
            object->DelayCall = FALSE;
            error = DelayMsg(action_id, object->ObjectID);
         }
         else if (object->prvObject) error = Action(action_id, object->prvObject, NULL);
         else error = ActionMsg(action_id, object->ObjectID, NULL);

         lua_pushinteger(Lua, error);
         results = 1;
      }

      if (action_id IS AC_Free) {
         // Mark the object as unusable if it's been explicitly terminated.
         ClearMemory(object, sizeof(struct object));
      }
   }
   else { // Method
      auto methods = (MethodArray *)lua_touserdata(Lua, lua_upvalueindex(3));
      action_name = methods->Name;

      if ((methods->Args) and (methods->Size)) {
         BYTE argbuffer[methods->Size+8]; // +8 for overflow protection in build_args()

         LONG resultcount;
         if (!(error = build_args(Lua, methods->Args, methods->Size, argbuffer, &resultcount))) {
            if (object->DelayCall) {
               error = DelayMsg(action_id, object->ObjectID, (APTR)&argbuffer);
            }
            else if (object->prvObject) error = Action(action_id, object->prvObject, &argbuffer);
            else {
               // If the method returns results, we need to execute locally to pick up any results (e.g. string
               // pointers).  Otherwise, we can execute via messaging.

               if (resultcount > 0) {
                  OBJECTPTR obj;
                  if ((obj = access_object(object))) {
                     error = Action(action_id, obj, argbuffer);
                     release = TRUE;
                  }
               }
               else error = ActionMsg(action_id, object->ObjectID, argbuffer);
            }
         }
         else {
            luaL_error(Lua, "Argument build failure for method %s.", methods->Name);
            return 0;
         }

         lua_pushinteger(Lua, error);
         results = 1;

         if (!object->DelayCall) results += get_results(Lua, methods->Args, (const BYTE *)argbuffer);
         else object->DelayCall = FALSE;

         if (release) release_object(object);
      }
      else {
         if (object->DelayCall) {
            object->DelayCall = FALSE;
            error = DelayMsg(action_id, object->ObjectID);
         }
         else if (object->prvObject) error = Action(action_id, object->prvObject, NULL);
         else error = ActionMsg(action_id, object->ObjectID, NULL);

         lua_pushinteger(Lua, error);
         results = 1;
      }
   }

   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   if ((error >= ERR_ExceptionThreshold) and (prv->Catch)) {
      char msg[180];
      CSTRING error_msg = GetErrorMsg(error);
      prv->CaughtError = error;
      if (!action_name) action_name = "Unnamed";
      snprintf(msg, sizeof(msg), "%s.%s() failed: %s", object->Class->ClassName, action_name, error_msg);
      luaL_error(prv->Lua, msg);
   }

   return results;
}

//****************************************************************************
// Build argument buffer for actions and methods.

ERROR build_args(lua_State *Lua, const FunctionField *args, LONG ArgsSize, BYTE *argbuffer, LONG *ResultCount)
{
   parasol::Log log(__FUNCTION__);
   struct memory *memory;
   struct array *farray;
   struct fstruct *fstruct;

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
         if ((memory = (struct memory *)get_meta(Lua, n, "Fluid.mem"))) {
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
         else if ((fstruct = (struct fstruct *)get_meta(Lua, n, "Fluid.struct"))) {
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
         else if ((farray = (struct array *)get_meta(Lua, n, "Fluid.array"))) {
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
               if (object->prvObject) {
                  ((OBJECTPTR *)(argbuffer + j))[0] = object->prvObject;
               }
               else if ((ptr_obj = access_object(object))) {
                  ((OBJECTPTR *)(argbuffer + j))[0] = ptr_obj;
                  release_object(object);
               }
               else {
                  log.warning("Unable to resolve object pointer for #%d.", object->ObjectID);
                  ((OBJECTPTR *)(argbuffer + j))[0] = NULL;
               }
            }
            else ((OBJECTPTR *)(argbuffer + j))[0] = NULL;
         }
         else if (args[i].Type & FD_FUNCTION) {
            if ((type IS LUA_TSTRING) or (type IS LUA_TFUNCTION)) {
               FUNCTION *func;

               if (!AllocMemory(sizeof(FUNCTION), MEM_DATA, &func, NULL)) {
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

            if ((memory = (struct memory *)get_meta(Lua, n, "Fluid.mem"))) {
               ((APTR *)(argbuffer + j))[0] = memory->Memory;
               //n--; // Adjustment required due to successful get_meta()
            }
            else if ((fstruct = (struct fstruct *)get_meta(Lua, n, "Fluid.struct"))) {
               ((APTR *)(argbuffer + j))[0] = fstruct->Data;
               //n--; // Adjustment required due to successful get_meta()
            }
            else ((APTR *)(argbuffer + j))[0] = lua_touserdata(Lua, n);
         }

         j += sizeof(APTR);
      }
      else if (args[i].Type & FD_LONG) {
         if ((type IS LUA_TUSERDATA) or (type IS LUA_TLIGHTUSERDATA)) {
            struct object *obj;
            if ((obj = (struct object *)get_meta(Lua, n, "Fluid.obj"))) {
               ((LONG *)(argbuffer + j))[0] = obj->ObjectID;
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
         ((LARGE *)(argbuffer + j))[0] = lua_tonumber(Lua, n);
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
   parasol::Log log(__FUNCTION__);
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
                  push_struct(Lua->Script, ptr_struct, args[i].Name, (type & FD_ALLOC) ? TRUE : FALSE, FALSE);
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
                  struct object *new_obj = push_object(Lua, obj);
                  new_obj->Detached = (type & FD_ALLOC) ? FALSE : TRUE;
               }
               else lua_pushnil(Lua);
            }
            else if (type & FD_RGB) {
               RGB8 *rgb = (RGB8 *)((APTR *)(ArgBuf+of))[0];

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
