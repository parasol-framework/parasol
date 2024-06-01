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
#include <parasol/strings.hpp>

extern "C" {
#include "lauxlib.h"
#include "lj_obj.h"
}

#include "hashes.h"
#include "defs.h"

static ERR thread_script_entry(objThread *);
static ERR thread_script_callback(OBJECTID);

struct thread_callback {
   objScript *threadScript;
   LONG     callbackID;
   OBJECTID mainScriptID;
};

std::unordered_map<OBJECTID, thread_callback> glThreadCB;

//********************************************************************************************************************
// Usage: thread.script(Statement, Callback)

static int thread_script(lua_State *Lua)
{
   pf::Log log;
   CSTRING statement;

   if (!(statement = luaL_checkstring(Lua, 1))) {
      luaL_argerror(Lua, 1, "Script statement required.");
      return 0;
   }

   if (auto thread = objThread::create::untracked(fl::Flags(THF::AUTO_FREE), fl::Routine((CPTR)thread_script_entry))) {
      if (auto script = objScript::create::global(fl::Owner(thread->UID), fl::Statement(statement))) {
         if (lua_isfunction(Lua, 2)) {
            lua_pushvalue(Lua, 2);

            glThreadCB[thread->UID] = {
               .threadScript = script,
               .callbackID   = luaL_ref(Lua, LUA_REGISTRYINDEX),
               .mainScriptID = Lua->Script->UID
            };

            thread->set(FID_Callback, (CPTR)&thread_script_callback);
         }

         if (thread->activate() != ERR::Okay) {
            luaL_error(Lua, "Failed to execute thread");
         }

         // The thread is not manually removed because we've used AUTO_FREE and the script is owned by it.
      }
      else luaL_error(Lua, "Failed to create script for threaded execution.");
   }
   else luaL_error(Lua, "Failed to create new Thread object.");

   return 0;
}

//********************************************************************************************************************
// Execute the script statement within the context of the child thread.

static ERR thread_script_entry(objThread *Thread)
{
   if (auto it = glThreadCB.find(Thread->UID); it != glThreadCB.end()) {
      thread_callback &cb = it->second;
      acActivate(cb.threadScript);
      FreeResource(cb.threadScript);
   }
   return ERR::Okay;
}

//********************************************************************************************************************
// Callback following execution (executed by the main thread, not the child)

static ERR thread_script_callback(OBJECTID ThreadID)
{
   if (auto it = glThreadCB.find(ThreadID); it != glThreadCB.end()) {
      thread_callback &cb = it->second;
      if (ScopedObjectLock<objScript> script(cb.mainScriptID, 4000); script.granted()) {
         auto prv= (prvFluid *)script->ChildPrivate;
         sc::Callback(*script, cb.callbackID, NULL, 0, NULL);
         luaL_unref(prv->Lua, LUA_REGISTRYINDEX, cb.callbackID);
      }
      glThreadCB.erase(it);
   }

   return ERR::Okay;
}

//********************************************************************************************************************
// Usage: thread.action(Object, Action, Callback, Key, Args...)

static int thread_action(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);

   // Args: Object (1), Action (2), Callback (3), Key (4), Parameters...

   struct object *object;
   if (!(object = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj"))) {
      luaL_argerror(Lua, 1, "Object required.");
      return 0;
   }

   LONG type = lua_type(Lua, 2);
   ACTIONID action_id;
   CSTRING action = NULL;
   if (type IS LUA_TSTRING) {
      action = lua_tostring(Lua, 2);
      if (auto it = glActionLookup.find(action); it != glActionLookup.end()) {
         action_id = it->second;
      }
      else {
         luaL_argerror(Lua, 2, "Action name is not recognised (is it a method?)");
         return 0;
      }
   }
   else if (type IS LUA_TNUMBER) {
      action_id = lua_tointeger(Lua, 2);
   }
   else {
      luaL_argerror(Lua, 2, "Action name required.");
      return 0;
   }

   FUNCTION callback;
   LONG key = lua_tointeger(Lua, 4);

   type = lua_type(Lua, 3); // Optional callback.
   if (type IS LUA_TSTRING) {
      lua_getglobal(Lua, lua_tostring(Lua, 3));
      callback = FUNCTION(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
   }
   else if (type IS LUA_TFUNCTION) {
      lua_pushvalue(Lua, 3);
      callback = FUNCTION(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
   }

   LONG argsize = 0;
   const FunctionField *args = NULL;
   ERR error = ERR::Okay;

   if ((glActions[action_id].Args) and (glActions[action_id].Size)) {
      argsize = glActions[action_id].Size;
      args = glActions[action_id].Args;
   }

   log.trace("#%d/%p, Action: %s/%d, Key: %d, Args: %d", object->UID, object->ObjectPtr, action, action_id, key, argsize);

   if (argsize > 0) {
      auto argbuffer = std::make_unique<BYTE[]>(argsize+8); // +8 for overflow protection in build_args()
      LONG resultcount;

      if ((error = build_args(Lua, args, argsize, argbuffer.get(), &resultcount)) IS ERR::Okay) {
         if (object->ObjectPtr) {
            error = ActionThread(action_id, object->ObjectPtr, argbuffer.get(), &callback, key);
         }
         else {
            if (!resultcount) {
               if (auto obj = access_object(object)) {
                  error = ActionThread(action_id, obj, argbuffer.get(), &callback, key);
                  release_object(object);
               }
            }
            else log.warning("Actions that return results have not been tested/supported for release of resources.");
         }
      }
      else {
         luaL_unref(Lua, LUA_REGISTRYINDEX, callback.ProcedureID);
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
      else error = log.warning(ERR::AccessObject);
   }

   if (error != ERR::Okay) {
      if (callback.defined()) luaL_unref(Lua, LUA_REGISTRYINDEX, callback.ProcedureID);
      luaL_error(Lua, "Failed with error %s", GetErrorMsg(error));
   }

   return 0;
}

//********************************************************************************************************************
// Usage: error = thread.method(Object, Method, Callback, Key, Args...)

static int thread_method(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);

   // Args: Object (1), Action (2), Callback (3), Key (4), Parameters...

   if (auto object = (struct object *)luaL_checkudata(Lua, 1, "Fluid.obj")) {
      if (auto method = luaL_checkstring(Lua, 2)) {
         MethodEntry *table;
         LONG total_methods, i;

         // TODO: We should be using a hashmap here.

         if ((GetFieldArray(object->Class, FID_Methods, (APTR *)&table, &total_methods) IS ERR::Okay) and (table)) {
            bool found = false;
            for (i=1; i < total_methods; i++) {
               if ((table[i].Name) and (iequals(table[i].Name, method))) { found = true; break; }
            }

            if (found) {
               // If an obj.new() lock is still present, detach it first because ActionThread() is going to attempt to
               // lock the object with LockObject() and a timeout error will occur otherwise.

               auto args = table[i].Args;
               LONG argsize = table[i].Size;
               LONG action_id = table[i].MethodID;
               ERR error;
               OBJECTPTR obj;
               FUNCTION callback;
               LONG key = lua_tointeger(Lua, 4);

               LONG type = lua_type(Lua, 3); // Optional callback.
               if (type IS LUA_TSTRING) {
                  lua_getglobal(Lua, (STRING)lua_tostring(Lua, 3));
                  callback = FUNCTION(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
               }
               else if (type IS LUA_TFUNCTION) {
                  lua_pushvalue(Lua, 3);
                  callback = FUNCTION(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
               }
               else callback.Type = CALL::NIL;

               if (argsize > 0) {
                  auto argbuffer = std::make_unique<BYTE[]>(argsize+8); // +8 for overflow protection in build_args()
                  LONG resultcount;

                  lua_remove(Lua, 1); // Remove all 4 required arguments so that the user's custom parameters are then left on the stack
                  lua_remove(Lua, 1);
                  lua_remove(Lua, 1);
                  lua_remove(Lua, 1);
                  if ((error = build_args(Lua, args, argsize, argbuffer.get(), &resultcount)) IS ERR::Okay) {
                     if (object->ObjectPtr) {
                        error = ActionThread(action_id, object->ObjectPtr, argbuffer.get(), &callback, key);
                     }
                     else {
                        if (!resultcount) {
                           if ((obj = access_object(object))) {
                              error = ActionThread(action_id, obj, argbuffer.get(), &callback, key);
                              release_object(object);
                           }
                        }
                        else log.warning("Actions that return results have not been tested/supported for release of resources.");
                     }
                  }
                  else {
                     luaL_unref(Lua, LUA_REGISTRYINDEX, callback.ProcedureID);
                     luaL_error(Lua, "Argument build failure for %s.", glActions[action_id].Name);
                     return 0;
                  }
               }
               else { // No parameters.
                  if (object->ObjectPtr) {
                     error = ActionThread(action_id, object->ObjectPtr, NULL, &callback, key);
                  }
                  else if ((obj = access_object(object))) {
                     error = ActionThread(action_id, obj, NULL, &callback, key);
                     release_object(object);
                  }
                  else error = log.warning(ERR::AccessObject);
               }

               if (error != ERR::Okay) {
                  if (callback.defined()) luaL_unref(Lua, LUA_REGISTRYINDEX, callback.ProcedureID);
                  luaL_error(Lua, "Failed with error %s", GetErrorMsg(error));
               }

               return 0;
            }
         }

         luaL_error(Lua, "No '%s' method for class %s.", method, object->Class->ClassName);
      }
      else luaL_argerror(Lua, 2, "Method name required.");
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
