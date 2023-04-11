/*********************************************************************************************************************

The thread interface provides support for the parallel execution of actions and methods against objects:

  thread.action(Object, Action, Callback, Key, Args...)

  thread.method(Object, Action, Callback, Key, Args...)

The script() method compiles a statement string and executes it in a separate script state.  The code may not share
variables with its creator, except via existing conventional means such as a KeyStore.

  thread.script(Statement, Callback)

*********************************************************************************************************************/

#define PRV_SCRIPT
#define PRV_FLUID
#define PRV_FLUID_MODULE
#include <parasol/main.h>
#include <parasol/modules/fluid.h>

extern "C" {
#include "lauxlib.h"
#include "lj_obj.h"
}

#include "hashes.h"
#include "defs.h"

static ERROR thread_script_entry(objThread *);
static ERROR thread_script_callback(objThread *);

struct thread_callback {
   LONG callbackID;
   objScript *threadScript;
   OBJECTID mainScriptID;
};

//********************************************************************************************************************
// Usage: thread.script(Statement, Callback)

static int thread_script(lua_State *Lua)
{
   CSTRING statement;

   if (!(statement = luaL_checkstring(Lua, 1))) {
      luaL_argerror(Lua, 1, "Script statement required.");
      return 0;
   }

   if (auto thread = objThread::create::untracked(fl::Flags(THF_AUTO_FREE), fl::Routine((CPTR)thread_script_entry))) {
      if (auto script = objScript::create::global(fl::Owner(thread->UID), fl::Statement(statement))) {
         if (lua_isfunction(Lua, 2)) {
            lua_pushvalue(Lua, 2);
            thread_callback cb = {
               .callbackID   = luaL_ref(Lua, LUA_REGISTRYINDEX),
               .threadScript = script,
               .mainScriptID = Lua->Script->UID
            };
            thSetData(thread, &cb, sizeof(cb));

            thread->set(FID_Callback, (CPTR)&thread_script_callback);
         }

         if (thread->activate()) {
            luaL_error(Lua, "Failed to execute thread");
         }
      }
      else luaL_error(Lua, "Failed to create script for threaded execution.");
   }
   else luaL_error(Lua, "Failed to create new Thread object.");

   return 0;
}

//********************************************************************************************************************
// Execute the script statement within the context of the child thread.

static ERROR thread_script_entry(objThread *Thread)
{
   thread_callback *cb;
   if (!Thread->getPtr(FID_Data, &cb)) {
      acActivate(cb->threadScript);
      FreeResource(cb->threadScript);
   }
   return ERR_Okay;
}

//********************************************************************************************************************
// Callback following execution (within the context of the main thread, not the child)

static ERROR thread_script_callback(objThread *Thread)
{
   pf::Log log("thread");
   thread_callback *cb;

   if ((!Thread->getPtr(FID_Data, &cb)) and (cb)) {
      objScript *script;
      if (!AccessObject(cb->mainScriptID, 4000, &script)) {
         auto prv = (prvFluid *)script->ChildPrivate;
         if (!prv) return log.warning(ERR_ObjectCorrupt);
         scCallback(script, cb->callbackID, NULL, 0, NULL);
         luaL_unref(prv->Lua, LUA_REGISTRYINDEX, cb->callbackID);
         ReleaseObject(script);
      }
   }

   return ERR_Okay;
}

//********************************************************************************************************************
// Usage: error = thread.action(Object, Action, Callback, Key, Args...)

static int thread_action(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);

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

   LONG action_id;
   auto it = glActionLookup.find(action);
   if (it != glActionLookup.end()) action_id = it->second;
   else {
      luaL_argerror(Lua, 2, "Action name is not recognised (is it a method?)");
      return 0;
   }

   FUNCTION callback;
   LONG key = lua_tointeger(Lua, 4);

   LONG type = lua_type(Lua, 3); // Optional callback.
   if (type IS LUA_TSTRING) {
      lua_getglobal(Lua, lua_tostring(Lua, 3));
      callback = make_function_script(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
   }
   else if (type IS LUA_TFUNCTION) {
      lua_pushvalue(Lua, 3);
      callback = make_function_script(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
   }
   else callback.Type = 0;

   LONG argsize = 0;
   const FunctionField *args = NULL;
   ERROR error = ERR_Okay;

   if ((glActions[action_id].Args) and (glActions[action_id].Size)) {
      argsize = glActions[action_id].Size;
      args = glActions[action_id].Args;
   }

   log.trace("#%d/%p, Action: %s/%d, Key: %d, Args: %d", object->UID, object->ObjectPtr, action, action_id, key, argsize);

   if (argsize > 0) {
      BYTE argbuffer[argsize+8]; // +8 for overflow protection in build_args()
      LONG resultcount;

      if (!(error = build_args(Lua, args, argsize, argbuffer, &resultcount))) {
         if (object->ObjectPtr) {
            error = ActionThread(action_id, object->ObjectPtr, argbuffer, &callback, key);
         }
         else {
            if (!resultcount) {
               if (auto obj = access_object(object)) {
                  error = ActionThread(action_id, obj, argbuffer, &callback, key);
                  release_object(object);
               }
            }
            else log.warning("Actions that return results have not been tested/supported for release of resources.");
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
      if (object->ObjectPtr) {
         error = ActionThread(action_id, object->ObjectPtr, NULL, &callback, key);
      }
      else if (auto obj = access_object(object)) {
         error = ActionThread(action_id, obj, NULL, &callback, key);
         release_object(object);
      }
      else error = log.warning(ERR_AccessObject);
   }

   if ((error) and (callback.Type)) luaL_unref(Lua, LUA_REGISTRYINDEX, callback.Script.ProcedureID);
   lua_pushinteger(Lua, error);
   return 1;
}

//********************************************************************************************************************
// Usage: error = thread.method(Object, Action, Callback, Key, Args...)

static int thread_method(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);

   // Args: Object (1), Action (2), Callback (3), Key (4), Parameters...

   if (auto object = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj")) {
      if (auto method = luaL_checkstring(Lua, 2)) {
         MethodEntry *table;
         LONG total_methods, i;

         if ((!GetFieldArray(object->Class, FID_Methods, &table, &total_methods)) and (table)) {
            bool found = false;
            for (i=1; i < total_methods; i++) {
               if ((table[i].Name) and (!StrMatch(table[i].Name, method))) { found = true; break; }
            }

            if (found) {
               // If an obj.new() lock is still present, detach it first because ActionThread() is going to attempt to
               // lock the object with LockObject() and a timeout error will occur otherwise.

               auto args = table[i].Args;
               LONG argsize = table[i].Size;
               LONG action_id = table[i].MethodID;
               ERROR error;
               OBJECTPTR obj;
               FUNCTION callback;
               LONG key = lua_tointeger(Lua, 4);

               LONG type = lua_type(Lua, 3); // Optional callback.
               if (type IS LUA_TSTRING) {
                  lua_getglobal(Lua, (STRING)lua_tostring(Lua, 3));
                  callback = make_function_script(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
               }
               else if (type IS LUA_TFUNCTION) {
                  lua_pushvalue(Lua, 3);
                  callback = make_function_script(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
               }
               else callback.Type = 0;

               if (argsize > 0) {
                  BYTE argbuffer[argsize+8]; // +8 for overflow protection in build_args()
                  LONG resultcount;

                  lua_remove(Lua, 1); // Remove all 4 required arguments so that the user's custom parameters are then left on the stack
                  lua_remove(Lua, 1);
                  lua_remove(Lua, 1);
                  lua_remove(Lua, 1);
                  if (!(error = build_args(Lua, args, argsize, argbuffer, &resultcount))) {
                     if (object->ObjectPtr) {
                        error = ActionThread(action_id, object->ObjectPtr, argbuffer, &callback, key);
                     }
                     else {
                        if (!resultcount) {
                           if ((obj = access_object(object))) {
                              error = ActionThread(action_id, obj, argbuffer, &callback, key);
                              release_object(object);
                           }
                        }
                        else log.warning("Actions that return results have not been tested/supported for release of resources.");
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
                  if (object->ObjectPtr) {
                     error = ActionThread(action_id, object->ObjectPtr, NULL, &callback, key);
                  }
                  else if ((obj = access_object(object))) {
                     error = ActionThread(action_id, obj, NULL, &callback, key);
                     release_object(object);
                  }
                  else error = log.warning(ERR_AccessObject);
               }

               if ((error) and (callback.Type)) luaL_unref(Lua, LUA_REGISTRYINDEX, callback.Script.ProcedureID);
               lua_pushinteger(Lua, error);
               return 1;
            }
         }

         luaL_error(Lua, "No '%s' method for class %s.", method, object->Class->ClassName);
         return 0;
      }
      else luaL_argerror(Lua, 2, "Action name required.");
   }
   else luaL_argerror(Lua, 1, "Object required.");

   return 0;
}

//********************************************************************************************************************
// Register the thread interface.

static const luaL_Reg threadlib_functions[] = {
   { "action", thread_action },
   { "method", thread_method },
   { "script", thread_script },
   { NULL, NULL }
};

static const luaL_Reg threadlib_methods[] = {
   //{ "__index",    thread_get },
   //{ "__newindex", thread_set },
   { NULL, NULL }
};

void register_thread_class(lua_State *Lua)
{
   pf::Log log;

   log.trace("Registering thread interface.");

   luaL_newmetatable(Lua, "Fluid.thread");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable created earlier
   lua_settable(Lua, -3);   // metatable.__index = metatable

   luaL_openlib(Lua, NULL, threadlib_methods, 0);
   luaL_openlib(Lua, "thread", threadlib_functions, 0);
}
