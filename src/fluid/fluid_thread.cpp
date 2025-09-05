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
#include <thread>

extern "C" {
#include "lauxlib.h"
#include "lj_obj.h"
}

#include "hashes.h"
#include "defs.h"

//********************************************************************************************************************
// Usage: thread.script(Statement, Callback)

static int thread_script(lua_State *Lua)
{
   CSTRING statement;

   if (!(statement = luaL_checkstring(Lua, 1))) {
      luaL_argerror(Lua, 1, "Script statement required.");
      return 0;
   }

   if (auto script = objScript::create::global(fl::Statement(statement))) {
      FUNCTION callback;

      if (lua_isfunction(Lua, 2)) {
         lua_pushvalue(Lua, 2);
         callback = FUNCTION(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
         callback.MetaValue = Lua->Script->UID;
      }

      auto prv = (prvFluid *)Lua->Script->ChildPrivate;

      prv->Threads.emplace_back(std::make_unique<std::jthread>(std::jthread([](objScript *Script, FUNCTION Callback) {
         acActivate(Script);
         FreeResource(Script);

         // Client callback must be executed in the main thread; send a message to do that.
         if (Callback.isScript()) {
            SendMessage(MSGID::FLUID_THREAD_CALLBACK, MSF::ADD|MSF::WAIT, &Callback, sizeof(callback));
         }
      }, script, std::move(callback))));
   }
   else luaL_error(Lua, "Failed to create script for threaded execution.");

   return 0;
}

//********************************************************************************************************************
// Callback following execution (executed by the main thread, not the child)

ERR msg_thread_script_callback(APTR Custom, int MsgID, int MsgType, APTR Message, int MsgSize)
{
   auto cb = (FUNCTION *)Message;
   if (cb->isScript()) {
      if (ScopedObjectLock<objScript> script(cb->MetaValue, 4000); script.granted()) {
         script->callback(cb->ProcedureID, nullptr, 0, nullptr);
         auto prv = (prvFluid *)script->ChildPrivate;
         luaL_unref(prv->Lua, LUA_REGISTRYINDEX, cb->ProcedureID);
      }
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

   auto type = lua_type(Lua, 2);
   AC action_id;
   CSTRING action = nullptr;
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
      action_id = AC(lua_tointeger(Lua, 2));
   }
   else {
      luaL_argerror(Lua, 2, "Action name required.");
      return 0;
   }

   FUNCTION callback;
   auto key = lua_tointeger(Lua, 4);

   type = lua_type(Lua, 3); // Optional callback.
   if (type IS LUA_TSTRING) {
      lua_getglobal(Lua, lua_tostring(Lua, 3));
      callback = FUNCTION(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
   }
   else if (type IS LUA_TFUNCTION) {
      lua_pushvalue(Lua, 3);
      callback = FUNCTION(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
   }

   int arg_size = 0;
   const FunctionField *args = nullptr;
   ERR error = ERR::Okay;

   if ((glActions[int(action_id)].Args) and (glActions[int(action_id)].Size)) {
      arg_size = glActions[int(action_id)].Size;
      args = glActions[int(action_id)].Args;
   }

   log.trace("#%d/%p, Action: %s/%d, Args: %d", object->UID, object->ObjectPtr, action, int(action_id), arg_size);

   if (arg_size > 0) {
      auto arg_buffer = std::make_unique<int8_t[]>(arg_size+8); // +8 for overflow protection in build_args()
      int result_count;

      if ((error = build_args(Lua, args, arg_size, arg_buffer.get(), &result_count)) IS ERR::Okay) {
         callback.Meta = (APTR)key;
         if (object->ObjectPtr) {
            error = AsyncAction(action_id, object->ObjectPtr, arg_buffer.get(), &callback);
         }
         else {
            if (!result_count) {
               if (auto obj = access_object(object)) {
                  error = AsyncAction(action_id, obj, arg_buffer.get(), &callback);
                  release_object(object);
               }
            }
            else log.warning("Actions that return results have not been tested/supported for release of resources.");
         }
      }
      else {
         luaL_unref(Lua, LUA_REGISTRYINDEX, callback.ProcedureID);
         luaL_error(Lua, "Argument build failure for %s.", glActions[int(action_id)].Name);
         return 0;
      }
   }
   else {
      // No parameters.
      callback.Meta = (APTR)key;
      if (object->ObjectPtr) {
         error = AsyncAction(action_id, object->ObjectPtr, nullptr, &callback);
      }
      else if (auto obj = access_object(object)) {
         error = AsyncAction(action_id, obj, nullptr, &callback);
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
         int total_methods, i;

         // TODO: We should be using a hashmap here.

         if ((object->Class->get(FID_Methods, table, total_methods) IS ERR::Okay) and (table)) {
            bool found = false;
            for (i=1; i < total_methods; i++) {
               if ((table[i].Name) and (iequals(table[i].Name, method))) { found = true; break; }
            }

            if (found) {
               // If an obj.new() lock is still present, detach it first because AsyncAction() is going to attempt to
               // lock the object with LockObject() and a timeout error will occur otherwise.

               auto args = table[i].Args;
               int argsize = table[i].Size;
               AC action_id = table[i].MethodID;
               ERR error;
               OBJECTPTR obj;
               FUNCTION callback;

               int type = lua_type(Lua, 3); // Optional callback.
               if (type IS LUA_TSTRING) {
                  lua_getglobal(Lua, (STRING)lua_tostring(Lua, 3));
                  callback = FUNCTION(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
               }
               else if (type IS LUA_TFUNCTION) {
                  lua_pushvalue(Lua, 3);
                  callback = FUNCTION(Lua->Script, luaL_ref(Lua, LUA_REGISTRYINDEX));
               }
               else callback.Type = CALL::NIL;
               callback.Meta = APTR(lua_tointeger(Lua, 4));

               if (argsize > 0) {
                  auto argbuffer = std::make_unique<int8_t[]>(argsize+8); // +8 for overflow protection in build_args()
                  int resultcount;

                  lua_remove(Lua, 1); // Remove all 4 required arguments so that the user's custom parameters are then left on the stack
                  lua_remove(Lua, 1);
                  lua_remove(Lua, 1);
                  lua_remove(Lua, 1);
                  if ((error = build_args(Lua, args, argsize, argbuffer.get(), &resultcount)) IS ERR::Okay) {
                     if (object->ObjectPtr) {
                        error = AsyncAction(action_id, object->ObjectPtr, argbuffer.get(), &callback);
                     }
                     else {
                        if (!resultcount) {
                           if ((obj = access_object(object))) {
                              error = AsyncAction(action_id, obj, argbuffer.get(), &callback);
                              release_object(object);
                           }
                        }
                        else log.warning("Actions that return results have not been tested/supported for release of resources.");
                     }
                  }
                  else {
                     luaL_unref(Lua, LUA_REGISTRYINDEX, callback.ProcedureID);
                     luaL_error(Lua, "Argument build failure for %s.", glActions[int(action_id)].Name);
                     return 0;
                  }
               }
               else { // No parameters.
                  if (object->ObjectPtr) {
                     error = AsyncAction(action_id, object->ObjectPtr, nullptr, &callback);
                  }
                  else if ((obj = access_object(object))) {
                     error = AsyncAction(action_id, obj, nullptr, &callback);
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
   { nullptr, nullptr }
};

static const luaL_Reg threadlib_methods[] = {
   //{ "__index",    thread_get },
   //{ "__newindex", thread_set },
   { nullptr, nullptr }
};

void register_thread_class(lua_State *Lua)
{
   pf::Log log;

   log.trace("Registering thread interface.");

   luaL_newmetatable(Lua, "Fluid.thread");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable created earlier
   lua_settable(Lua, -3);   // metatable.__index = metatable

   luaL_openlib(Lua, nullptr, threadlib_methods, 0);
   luaL_openlib(Lua, "thread", threadlib_functions, 0);
}
