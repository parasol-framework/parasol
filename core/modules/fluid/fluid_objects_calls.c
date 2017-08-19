
//****************************************************************************
// Lua C closure executed via calls to obj.acName() or obj.mtName()

static int object_call(lua_State *Lua)
{
   struct object *object;
   if (!(object = get_meta(Lua, lua_upvalueindex(1), "Fluid.obj"))) {
      luaL_error(Lua, "object_call() expected object in upvalue.");
      return 0;
   }

   LONG action_id = lua_tointeger(Lua, lua_upvalueindex(2));

   FMSG("object_call()","#%d/%p, Action: %d", object->ObjectID, object->prvObject, action_id);

   ERROR error = ERR_Okay;
   LONG results = 0;
   BYTE release = FALSE;
   if (action_id >= 0) {
      if ((glActions[action_id].Args) AND (glActions[action_id].Size)) {
         UBYTE argbuffer[glActions[action_id].Size+8]; // +8 for overflow protection in build_args()

         LONG resultcount;
         if (!(error = build_args(Lua, glActions[action_id].Args, glActions[action_id].Size, argbuffer, &resultcount))) {
            if (object->DelayCall) {
               object->DelayCall = FALSE;
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

         // NB: Even if an error is returned, always get the results (any results parameters are nullified prior to
         // function entry and the action can return results legitimately even if an error code is returned - e.g.
         // quite common when returning ERR_Terminate).

         results = 1 + get_results(Lua, glActions[action_id].Args, argbuffer);

         if (release) release_object(object);
      }
      else {
         if (object->DelayCall) {
            object->DelayCall = FALSE;
            error = DelayMsg(action_id, object->ObjectID, NULL);
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
      struct MethodArray *methods = lua_touserdata(Lua, lua_upvalueindex(3));

      if ((methods->Args) AND (methods->Size)) {
         UBYTE argbuffer[methods->Size+8]; // +8 for overflow protection in build_args()

         LONG resultcount;
         if (!(error = build_args(Lua, methods->Args, methods->Size, argbuffer, &resultcount))) {
            if (object->DelayCall) {
               object->DelayCall = FALSE;
               error = DelayMsg(action_id, object->ObjectID, &argbuffer);
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

         results = 1 + get_results(Lua, methods->Args, argbuffer);

         if (release) release_object(object);
      }
      else {
         if (object->DelayCall) {
            object->DelayCall = FALSE;
            error = DelayMsg(action_id, object->ObjectID, NULL);
         }
         else if (object->prvObject) error = Action(action_id, object->prvObject, NULL);
         else error = ActionMsg(action_id, object->ObjectID, NULL);

         lua_pushinteger(Lua, error);
         results = 1;
      }
   }

   return results;
}

//****************************************************************************
// Build argument buffer for actions and methods.

static ERROR build_args(lua_State *Lua, const struct FunctionField *args, LONG ArgsSize, APTR argbuffer,
   LONG *ResultCount)
{
   struct memory *memory;
   struct array *farray;
   struct fstruct *fstruct;

   LONG top = lua_gettop(Lua);

   FMSG("~build_args()","%d, %p, Top: %d", ArgsSize, argbuffer, top);

   ClearMemory(argbuffer, ArgsSize);

   LONG i, n;
   LONG resultcount = 0;
   LONG j = 0;
   for (i=0,n=1; (args[i].Name) AND (j < ArgsSize) AND (top > 0); i++,n++,top--) {
      LONG type = lua_type(Lua, n);

      if (args[i].Type & FD_RESULT) resultcount = resultcount + 1;

      FMSG("build_args","Processing arg %s, type $%.8x", args[i].Name, args[i].Type);

      if ((args[i].Type & FD_BUFFER) OR (args[i+1].Type & FD_BUFSIZE)) {
         #ifdef _LP64
            j = ALIGN64(j);
         #endif
         if ((memory = get_meta(Lua, n, "Fluid.mem"))) {
            MSG("Arg: %s, Value: Buffer (Source is Memory)", args[i].Name);

            ((APTR *)(argbuffer + j))[0] = memory->Memory;
            j += sizeof(APTR);

            if (args[i+1].Type & FD_BUFSIZE) {
               // Buffer size is optional, so set the buffer size parameter by default.  The user can override it if
               // more arguments are specified in the function call.

               LONG memsize = memory->MemorySize;
               if (args[i+1].Type & FD_LONG)  ((LONG *)(argbuffer + j))[0] = memsize;
               else if (args[i+1].Type & FD_LARGE) ((LARGE *)(argbuffer + j))[0] = memsize;
               MSG("Preset buffer size of %d bytes.", memsize);
            }

            //n--; // Adjustment required due to successful get_meta()
         }
         else if ((fstruct = get_meta(Lua, n, "Fluid.struct"))) {
            MSG("Arg: %s, Value: Buffer (Source is a struct)", args[i].Name);

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
         else if ((farray = get_meta(Lua, n, "Fluid.array"))) {
            MSG("Arg: %s, Value: Buffer (Source is a array)", args[i].Name);

            ((APTR *)(argbuffer + j))[0] = farray->ptrPointer;
            j += sizeof(APTR);

            if (args[i+1].Type & FD_BUFSIZE) {
               // Buffer size is optional, so set the buffer size parameter by default.  The user can
               // override it if more arguments are specified in the function call.

               MSG("Advance setting of following BUFSIZE parameter to %d", farray->ArraySize);

               if (args[i+1].Type & FD_LONG) ((LONG *)(argbuffer + j))[0] = farray->ArraySize;
               else if (args[i+1].Type & FD_LARGE) ((LARGE *)(argbuffer + j))[0] = farray->ArraySize;
               else MSG("Cannot set BUFSIZE argument - unknown type.");
            }
            n--; // Adjustment required due to successful get_meta()
         }
         else if (type IS LUA_TSTRING) {
            MSG("Arg: %s, Value: Buffer (Source is String)", args[i].Name);

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
            STEP();
            luaL_argerror(Lua, n, "Cannot use a number as a buffer pointer.");
            return ERR_WrongType;
         }
         else {
            MSG("Arg: %s, Value: Buffer", args[i].Name);
            ((APTR *)(argbuffer + j))[0] = lua_touserdata(Lua, n);
            j += sizeof(APTR);
         }
      }
      else if (args[i].Type & FD_STR) {
         #ifdef _LP64
            j = ALIGN64(j);
         #endif
         if ((type IS LUA_TSTRING) OR (type IS LUA_TNUMBER)) {
            ((CSTRING *)(argbuffer + j))[0] = lua_tostring(Lua, n);
         }
         else if (type <= 0) {
            ((CSTRING *)(argbuffer + j))[0] = NULL;
         }
         else if ((type IS LUA_TUSERDATA) OR (type IS LUA_TLIGHTUSERDATA)) {
            luaL_error(Lua, "Arg #%d (%s) requires a string and not untyped pointer.", i, args[i].Name);
         }
         else luaL_error(Lua, "Arg #%d (%s) requires a string, got %s '%s'.", i, args[i].Name, lua_typename(Lua, type), lua_tostring(Lua, n));

         MSG("Arg: %s, Value: %s", args[i].Name, ((STRING *)(argbuffer + j))[0]);

         j += sizeof(STRING);
      }
      else if (args[i].Type & FD_PTR) {
         #ifdef _LP64
            j = ALIGN64(j);
         #endif
         if (args[i].Type & FD_OBJECT) {
            struct object *object;
            if ((object = get_meta(Lua, n, "Fluid.obj"))) {
               OBJECTPTR ptr_obj;
               if (object->prvObject) {
                  ((OBJECTPTR *)(argbuffer + j))[0] = object->prvObject;
               }
               else if ((ptr_obj = access_object(object))) {
                  ((OBJECTPTR *)(argbuffer + j))[0] = ptr_obj;
                  release_object(object);
               }
               else {
                  LogF("@","Unable to resolve object pointer for #%d.", object->ObjectID);
                  ((OBJECTPTR *)(argbuffer + j))[0] = NULL;
               }
            }
            else ((OBJECTPTR *)(argbuffer + j))[0] = NULL;
         }
         else if (args[i].Type & FD_FUNCTION) {
            if ((type IS LUA_TSTRING) OR (type IS LUA_TFUNCTION)) {
               FUNCTION *func;

               if (!AllocMemory(sizeof(FUNCTION), MEM_DATA, &func, NULL)) {
                  if (type IS LUA_TSTRING) {
                     lua_getglobal(Lua, lua_tostring(Lua, n));
                     SET_FUNCTION_SCRIPT(*func, &Lua->Script->Head, luaL_ref(Lua, LUA_REGISTRYINDEX));
                  }
                  else {
                     lua_pushvalue(Lua, n);
                     SET_FUNCTION_SCRIPT(*func, &Lua->Script->Head, luaL_ref(Lua, LUA_REGISTRYINDEX));
                  }

                  ((FUNCTION **)(argbuffer + j))[0] = func;

                  // The FUNCTION structure is freed when processing results
               }
               else luaL_error(Lua, "Memory allocation error.");
            }
            else luaL_error(Lua, "Arg #%d (%s) requires a string or function, got %s '%s'.", i, args[i].Name, lua_typename(Lua, type), lua_tostring(Lua, n));
         }
         else if (type IS LUA_TSTRING) {
            MSG("Arg: %s, Value: Pointer (Source is String)", args[i].Name);
            ((CSTRING *)(argbuffer + j))[0] = lua_tostring(Lua, n);
         }
         else if (type IS LUA_TNUMBER) {
            STEP();
            luaL_argerror(Lua, n, "Unable to convert number to a pointer.");
            return ERR_WrongType;
         }
         else {
            MSG("Arg: %s, Value: Pointer, SrcType: %s", args[i].Name, lua_typename(Lua, type));

            if ((memory = get_meta(Lua, n, "Fluid.mem"))) {
               ((APTR *)(argbuffer + j))[0] = memory->Memory;
               //n--; // Adjustment required due to successful get_meta()
            }
            else if ((fstruct = get_meta(Lua, n, "Fluid.struct"))) {
               ((APTR *)(argbuffer + j))[0] = fstruct->Data;
               //n--; // Adjustment required due to successful get_meta()
            }
            else ((APTR *)(argbuffer + j))[0] = lua_touserdata(Lua, n);
         }

         j += sizeof(APTR);
      }
      else if (args[i].Type & FD_LONG) {
         if ((type IS LUA_TUSERDATA) OR (type IS LUA_TLIGHTUSERDATA)) {
            struct object *object;
            if ((object = get_meta(Lua, n, "Fluid.obj"))) {
               ((LONG *)(argbuffer + j))[0] = object->ObjectID;
            }
            else luaL_argerror(Lua, n, "Unable to convert usertype to an integer.");
         }
         else if (type IS LUA_TBOOLEAN) ((LONG *)(argbuffer + j))[0] = lua_toboolean(Lua, n);
         else if (type != LUA_TNIL) ((LONG *)(argbuffer + j))[0] = lua_tointeger(Lua, n);
         else if (args[i].Type & FD_BUFSIZE); // Do not alter as the FD_BUFFER support would have managed it
         else ((LONG *)(argbuffer + j))[0] = 0;
         MSG("Arg: %s, Value: %d / $%.8x", args[i].Name, ((LONG *)(argbuffer + j))[0], ((LONG *)(argbuffer + j))[0]);
         j += sizeof(LONG);
      }
      else if (args[i].Type & FD_DOUBLE) {
         j = ALIGN64(j);
         ((DOUBLE *)(argbuffer + j))[0] = lua_tonumber(Lua, n);
         MSG("Arg: %s, Value: %.2f", args[i].Name, ((DOUBLE *)(argbuffer + j))[0]);
         j += sizeof(DOUBLE);
      }
      else if (args[i].Type & FD_LARGE) {
         j = ALIGN64(j);
         ((LARGE *)(argbuffer + j))[0] = lua_tonumber(Lua, n);
         MSG("Arg: %s, Value: " PF64(), args[i].Name, ((LARGE *)(argbuffer + j))[0]);
         j += sizeof(LARGE);
      }
      else {
         LogErrorMsg("Unsupported arg %s, flags $%.8x, aborting now.", args[i].Name, args[i].Type);
         STEP();
         return ERR_WrongType;
      }
   }

   // Finish counting the number of result types registered in the argument list

   for (; (args[i].Name); i++) {
      if (args[i].Type & FD_RESULT) resultcount = resultcount + 1;
   }

   FMSG("build_args","Processed %d args (%d bytes), detected %d result parameters.", i, j, resultcount);
   if (ResultCount) *ResultCount = resultcount;
   STEP();
   return ERR_Okay;
}

// Note: Please refer to process_results() in fluid_module.c for the 'official' take on result handling.

static LONG get_results(lua_State *Lua, const struct FunctionField *args, APTR ArgBuf)
{
   LONG i;

   RMSG("get_results(%p)", ArgBuf);

   LONG total = 0;
   LONG of = 0;
   for (i=0; args[i].Name; i++) {
      LONG type = args[i].Type;

      if (type & FD_ARRAY) { // Pointer to an array.
         if (sizeof(APTR) IS 8) of = ALIGN64(of);
         if (type & FD_RESULT) {
            APTR values = ((APTR *)ArgBuf + of)[0];
            RMSG("Result-Arg: %s, Array: %p", args[i].Name, values);

            LONG total_elements = -1; // Default of -1 will work for null-terminated arrays.
            if (args[i+1].Type & FD_ARRAYSIZE) {
               const APTR size_var = ((APTR *)(ArgBuf + of + sizeof(APTR)))[0];
               if (args[i+1].Type & FD_LONG) total_elements = ((LONG *)size_var)[0];
               else if (args[i+1].Type & FD_LARGE) total_elements = ((LARGE *)size_var)[0];
               else LogErrorMsg("Invalid arg %s, flags $%.8x", args[i+1].Name, args[i+1].Type);

               if (values) {
                  make_any_table(Lua, type, args[i].Name, total_elements, values);
                  if (type & FD_ALLOC) FreeMemory(values);
               }
               else lua_pushnil(Lua);
            }
            else {
               LogF("@get_results","Function parameter '%s' incorrectly defined.", args[i].Name);
               lua_pushnil(Lua);
            }
         }
         of += sizeof(APTR);
      }
      else if (type & FD_STR) {
         if (sizeof(APTR) IS 8) of = ALIGN64(of);
         if (type & FD_RESULT) {
            APTR val = ArgBuf + of;
            RMSG("Result-Arg: %s, Value: %.20s (String)", args[i].Name, ((STRING *)val)[0]);
            lua_pushstring(Lua, ((STRING *)val)[0]);
            if (type & FD_ALLOC) {
               APTR ptr = ((STRING *)val)[0];
               if (ptr) FreeMemory(ptr);
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
                  push_struct(Lua->Script, ptr_struct, args[i].Name, (type & FD_ALLOC) ? TRUE : FALSE);
               }
               else {
                  if (named_struct_to_table(Lua, args[i].Name, ptr_struct) != ERR_Okay) {
                     luaL_error(Lua, "Failed to create struct for %s, %p", args[i].Name, ptr_struct);
                     return total;
                  }
                  if (type & FD_ALLOC) FreeMemory(ptr_struct);
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
            if ((func = ((APTR *)(ArgBuf+of))[0])) {
               MSG("Removing function memory allocation %p", func);
               FreeMemory(func);
            }
         }
         else if (type & FD_RESULT) {
            if (type & FD_OBJECT) {
               OBJECTPTR obj = ((APTR *)(ArgBuf+of))[0];

               RMSG("Result-Arg: %s, Value: %p (Object)", args[i].Name, obj);

               if (obj) {
                  struct object *new_obj = push_object(Lua, obj);
                  new_obj->Detached = (type & FD_ALLOC) ? FALSE : TRUE;
               }
               else lua_pushnil(Lua);
            }
            else if (type & FD_RGB) {
               struct RGB8 *rgb = ((APTR *)(ArgBuf+of))[0];

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
            RMSG("Result-Arg: %s, Value: " PF64() " (Large)", args[i].Name, ((LARGE *)(ArgBuf+of))[0]);
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
         LogF("@get_results","Unsupported arg %s, flags $%x, aborting now.", args[i].Name, type);
         break;
      }
   }

   RMSG("get_results: Wrote %d args.", total);
   return total;
}
