/*****************************************************************************

The thread interface provides support for the parallel execution of actions and methods against objects.

thread.action(Object, Action, Callback, Key, Args...)

thread.method(Object, Action, Callback, Key, Args...)

thread.routine(Function, Callback, Key, Args...)
   Currently not implemented - would create a new lua_State and execute the provided Function.

*****************************************************************************/

//****************************************************************************
// Usage: error = thread.action(Object, Action, Callback, Key, Args...)

static int thread_action(lua_State *Lua)
{
   // Args: Object (1), Action (2), Callback (3), Key (4), Parameters...

   struct object *object;
   if (!(object = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Object required.");
      return 0;
   }

   CSTRING action;
   if (!(action = luaL_checkstring(Lua, 2))) {
      luaL_argerror(Lua, 2, "Action name required.");
      return 0;
   }

   // Convert the action name to its equivalent ID.

   ACTIONID *ptr;
   LONG action_id;
   if (!VarGet(glActionLookup, action, &ptr, NULL)) {
      action_id = ptr[0];
   }
   else {
      luaL_argerror(Lua, 2, "Action name is not recognised (is it a method?)");
      return 0;
   }

   // If an obj.new() lock is still present, detach it first because ActionThread() is going to attempt to lock the
   // object with AccessPrivateObject() and a timeout error will occur otherwise.

   if ((object->NewLock) AND (!object->Detached)) {
      object->Detached = TRUE;
      object->NewLock = FALSE;
      release_object(object);
   }

   FUNCTION callback;
   LONG key = lua_tointeger(Lua, 4);

   LONG type = lua_type(Lua, 3); // Optional callback.
   if (type IS LUA_TSTRING) {
      lua_getglobal(Lua, lua_tostring(Lua, 3));
      SET_FUNCTION_SCRIPT(callback, &Lua->Script->Head, luaL_ref(Lua, LUA_REGISTRYINDEX));
   }
   else if (type IS LUA_TFUNCTION) {
      lua_pushvalue(Lua, 3);
      SET_FUNCTION_SCRIPT(callback, &Lua->Script->Head, luaL_ref(Lua, LUA_REGISTRYINDEX));
   }
   else callback.Type = 0;

   LONG argsize = 0;
   const struct FunctionField *args = NULL;
   OBJECTPTR obj;
   ERROR error = ERR_Okay;

   if ((glActions[action_id].Args) AND (glActions[action_id].Size)) {
      argsize = glActions[action_id].Size;
      args = glActions[action_id].Args;
   }

   FMSG("thread_action","#%d/%p, Action: %s/%d, Key: %d, Args: %d", object->ObjectID, object->prvObject, action, action_id, key, argsize);

   if (argsize > 0) {
      UBYTE argbuffer[argsize+8]; // +8 for overflow protection in build_args()
      LONG resultcount;

      if (!(error = build_args(Lua, args, argsize, argbuffer, &resultcount))) {
         if (object->prvObject) {
            error = ActionThread(action_id, object->prvObject, argbuffer, &callback, key);
         }
         else {
            if (!resultcount) {
               if ((obj = access_object(object))) {
                  error = ActionThread(action_id, obj, argbuffer, &callback, key);
                  release_object(object);
               }
            }
            else LogErrorMsg("Actions that return results have not been tested/supported for release of resources.");
         }
      }
      else {
         luaL_unref(Lua, LUA_REGISTRYINDEX, callback.Script.ProcedureID);
         luaL_error(Lua, "Argument build failure for %s.", glActions[action_id].Name);
         return 0;
      }
   }
   else {
      // No parameters.
      if (object->prvObject) {
         error = ActionThread(action_id, object->prvObject, NULL, &callback, key);
      }
      else if ((obj = access_object(object))) {
         error = ActionThread(action_id, obj, NULL, &callback, key);
         release_object(object);
      }
      else error = PostError(ERR_AccessObject);
   }

   if ((error) AND (callback.Type)) luaL_unref(Lua, LUA_REGISTRYINDEX, callback.Script.ProcedureID);
   lua_pushinteger(Lua, error);
   return 1;
}

//****************************************************************************
// Usage: error = thread.method(Object, Action, Callback, Key, Args...)

static int thread_method(lua_State *Lua)
{
   struct object *object;
   CSTRING method;

   // Args: Object (1), Action (2), Callback (3), Key (4), Parameters...

   if ((object = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj"))) {
      if ((method = luaL_checkstring(Lua, 2))) {
         struct rkMetaClass *class;
         struct MethodArray *table;
         LONG total_methods, i;

         if (!(class = FindClass(object->ClassID))) {
            luaL_error(Lua, "Failed to resolve class %d", object->ClassID);
         }

         if ((!GetFieldArray(class, FID_Methods, &table, &total_methods)) AND (table)) {
            BYTE found = FALSE;
            for (i=1; i < total_methods+1; i++) {
               if ((table[i].Name) AND (!StrMatch(table[i].Name, method))) { found = TRUE; break; }
            }

            if (found) {
               // If an obj.new() lock is still present, detach it first because ActionThread() is going to attempt to
               // lock the object with AccessPrivateObject() and a timeout error will occur otherwise.

               if ((object->NewLock) AND (!object->Detached)) {
                  object->Detached = TRUE;
                  object->NewLock = FALSE;
                  release_object(object);
               }

               const struct FunctionField *args = table[i].Args;
               LONG argsize = table[i].Size;
               LONG action_id = table[i].MethodID;
               ERROR error;
               OBJECTPTR obj;
               FUNCTION callback;
               LONG key = lua_tointeger(Lua, 4);

               LONG type = lua_type(Lua, 3); // Optional callback.
               if (type IS LUA_TSTRING) {
                  lua_getglobal(Lua, (STRING)lua_tostring(Lua, 3));
                  SET_FUNCTION_SCRIPT(callback, &Lua->Script->Head, luaL_ref(Lua, LUA_REGISTRYINDEX));
               }
               else if (type IS LUA_TFUNCTION) {
                  lua_pushvalue(Lua, 3);
                  SET_FUNCTION_SCRIPT(callback, &Lua->Script->Head, luaL_ref(Lua, LUA_REGISTRYINDEX));
               }
               else callback.Type = 0;

               if (argsize > 0) {
                  UBYTE argbuffer[argsize+8]; // +8 for overflow protection in build_args()
                  LONG resultcount;

                  lua_remove(Lua, 1);
                  lua_remove(Lua, 1);
                  lua_remove(Lua, 1);
                  lua_remove(Lua, 1);
                  if (!(error = build_args(Lua, args, argsize, argbuffer, &resultcount))) {
                     if (object->prvObject) {
                        error = ActionThread(action_id, object->prvObject, argbuffer, &callback, key);
                     }
                     else {
                        if (!resultcount) {
                           if ((obj = access_object(object))) {
                              error = ActionThread(action_id, obj, argbuffer, &callback, key);
                              release_object(object);
                           }
                        }
                        else LogErrorMsg("Actions that return results have not been tested/supported for release of resources.");
                     }
                  }
                  else {
                     luaL_unref(Lua, LUA_REGISTRYINDEX, callback.Script.ProcedureID);
                     luaL_error(Lua, "Argument build failure for %s.", glActions[action_id].Name);
                     return 0;
                  }
               }
               else {
                  // No parameters.
                  if (object->prvObject) {
                     error = ActionThread(action_id, object->prvObject, NULL, &callback, key);
                  }
                  else if ((obj = access_object(object))) {
                     error = ActionThread(action_id, obj, NULL, &callback, key);
                     release_object(object);
                  }
                  else error = PostError(ERR_AccessObject);
               }

               if ((error) AND (callback.Type)) luaL_unref(Lua, LUA_REGISTRYINDEX, callback.Script.ProcedureID);
               lua_pushinteger(Lua, error);
               return 1;
            }
         }

         luaL_error(Lua, "No '%s' method for class %s.", method, class->ClassName);
         return 0;
      }
      else luaL_argerror(Lua, 2, "Action name required.");
   }
   else luaL_argerror(Lua, 1, "Object required.");

   return 0;
}

/*****************************************************************************
** Register the thread interface.
*/

static const struct luaL_reg threadlib_functions[] = {
   { "action", thread_action },
   { "method", thread_method },
   { NULL, NULL }
};

static const struct luaL_reg threadlib_methods[] = {
   //{ "__index",    thread_get },
   //{ "__newindex", thread_set },
   { NULL, NULL }
};

static void register_thread_class(lua_State *Lua)
{
   MSG("Registering thread interface.");

   luaL_newmetatable(Lua, "Fluid.thread");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable created earlier
   lua_settable(Lua, -3);   // metatable.__index = metatable

   luaL_openlib(Lua, NULL, threadlib_methods, 0);
   luaL_openlib(Lua, "thread", threadlib_functions, 0);
}
