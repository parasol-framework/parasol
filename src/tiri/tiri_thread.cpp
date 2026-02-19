/*********************************************************************************************************************

The thread interface provides support for the parallel execution of actions and methods against objects:

  thread.action(Object, Action, Callback, Key, Args...)

  thread.method(Object, Action, Callback, Key, Args...)

The script() method compiles a statement string and executes it in a separate script state.  The code may not share
variables with its creator, except via existing conventional means such as a KeyStore.

  thread.script(Statement, Callback)

*********************************************************************************************************************/

#define PRV_SCRIPT
#define PRV_TIRI
#define PRV_TIRI_MODULE
#include <kotuku/main.h>
#include <kotuku/modules/tiri.h>
#include <kotuku/strings.hpp>
#include <thread>
#include <cassert>

#include "lib.h"
#include "lauxlib.h"
#include "lj_obj.h"
#include "lj_object.h"
#include "hashes.h"
#include "defs.h"
#include "lj_proto_registry.h"

// Message payload for thread.script() callbacks

struct ThreadScriptMsg {
   int       Callback;  // Client callback reference
   int       ObjRef;    // Registry reference that pins the GCobject from GC collection
   objScript *Owner;    // The parent script that owns the registry references
};

//********************************************************************************************************************
// Callback following execution (executed by the main thread, not the child)

static void msg_thread_script_complete(ACTIONID ActionID, objScript *Script, ERR Error, int MsgKey, ThreadScriptMsg *Msg)
{
   pf::Log log("thread_callback");

   auto owner = Msg->Owner; // The parent script that owns the registry references
   auto prv = (prvTiri *)owner->ChildPrivate;

   if (Msg->Callback) {
      owner->callback(Msg->Callback, nullptr, 0, nullptr);
      luaL_unref(prv->Lua, LUA_REGISTRYINDEX, Msg->Callback); // Drop the procedure reference
   }

   // Unpin the GCobject from the registry and release the pin on the underlying object.

   int obj_ref = Msg->ObjRef;
   lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, obj_ref);
   auto gc_script = lua_toobject(prv->Lua, -1);
   lua_pop(prv->Lua, 1);
   luaL_unref(prv->Lua, LUA_REGISTRYINDEX, obj_ref);

   if (gc_script and gc_script->ptr) gc_script->ptr->unpin(true);
   delete Msg;
}

//********************************************************************************************************************
// Usage: thread.script(Script, Callback)
//
// Pins the Script object to prevent premature destruction, then executes it in its own thread.  The pin is
// released when the thread completes and the callback message is processed on the main thread.  No object lock
// is held across the thread boundary — acActivate() acquires its own lock internally via ScopedObjectAccess.

static int thread_script(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);

   GCobject *gc_script = lua_toobject(Lua, 1);
   if (gc_script->classptr->ClassID != CLASSID::SCRIPT) luaL_error(Lua, ERR::WrongClass);
   if (not gc_script->ptr) luaL_error(Lua, ERR::ObjectCorrupt);

   gc_script->ptr->pin(); // Prevent the object from being freed while the thread is running.

   int client_callback = 0;
   if (lua_isfunction(Lua, 2)) {
      lua_pushvalue(Lua, 2);
      client_callback = luaL_ref(Lua, LUA_REGISTRYINDEX);
   }

   // Pin the script in the registry so the GC cannot collect it while the thread is running.
   lua_pushvalue(Lua, 1);
   int obj_ref = luaL_ref(Lua, LUA_REGISTRYINDEX);

   auto msg = new ThreadScriptMsg { client_callback, obj_ref, Lua->script };
   auto callback = C_FUNCTION(msg_thread_script_complete, msg);

   if (AsyncAction(AC::Activate, gc_script->ptr, nullptr, &callback) != ERR::Okay) {
      gc_script->ptr->unpin(true);
      luaL_unref(Lua, LUA_REGISTRYINDEX, obj_ref);
      luaL_unref(Lua, LUA_REGISTRYINDEX, client_callback);
      delete msg;
      luaL_error(Lua, "Failed to run script in new thread.");
   }

   return 0;
}

//********************************************************************************************************************
// Usage: thread.action(Object, Action, Callback, Key, Args...)

static int thread_action(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);

   // Args: Object (1), Action (2), Callback (3), Key (4), Parameters...

   GCobject *obj_ref;
   if (not (obj_ref = lj_lib_checkobject(Lua, 1))) luaL_argerror(Lua, 1, "Object required.");

   auto type = lua_type(Lua, 2);
   AC action_id;
   CSTRING action = nullptr;
   if (type IS LUA_TSTRING) {
      action = lua_tostring(Lua, 2);
      if (auto it = glActionLookup.find(action); it != glActionLookup.end()) {
         action_id = it->second;
      }
      else luaL_argerror(Lua, 2, "Action name is not recognised (is it a method?)");
   }
   else if (type IS LUA_TNUMBER) action_id = AC(lua_tointeger(Lua, 2));
   else luaL_argerror(Lua, 2, "Action name required.");

   FUNCTION callback;
   auto key = lua_tointeger(Lua, 4);

   type = lua_type(Lua, 3); // Optional callback.
   if (type IS LUA_TSTRING) {
      lua_getglobal(Lua, lua_tostring(Lua, 3));
      callback = FUNCTION(Lua->script, luaL_ref(Lua, LUA_REGISTRYINDEX));
   }
   else if (type IS LUA_TFUNCTION) {
      lua_pushvalue(Lua, 3);
      callback = FUNCTION(Lua->script, luaL_ref(Lua, LUA_REGISTRYINDEX));
   }

   int arg_size = 0;
   const FunctionField *args = nullptr;
   ERR error = ERR::Okay;

   if ((glActions[int(action_id)].Args) and (glActions[int(action_id)].Size)) {
      arg_size = glActions[int(action_id)].Size;
      args = glActions[int(action_id)].Args;
   }

   log.trace("#%d/%p, Action: %s/%d, Args: %d", obj_ref->uid, obj_ref->ptr, action, int(action_id), arg_size);

   if (arg_size > 0) {
      auto arg_buffer = std::make_unique<int8_t[]>(arg_size+8); // +8 for overflow protection in build_args()
      int result_count;

      if ((error = build_args(Lua, args, arg_size, arg_buffer.get(), &result_count)) IS ERR::Okay) {
         callback.Meta = (APTR)key;
         if (obj_ref->ptr) {
            error = AsyncAction(action_id, obj_ref->ptr, arg_buffer.get(), &callback);
         }
         else if (!result_count) {
            if (auto obj = access_object(obj_ref)) {
               error = AsyncAction(action_id, obj, arg_buffer.get(), &callback);
               release_object(obj_ref);
            }
         }
         else log.warning("Actions that return results have not been tested/supported for release of resources.");
      }
      else {
         luaL_unref(Lua, LUA_REGISTRYINDEX, callback.ProcedureID);
         luaL_error(Lua, "Argument build failure for %s.", glActions[int(action_id)].Name);
         return 0;
      }
   }
   else { // No parameters.
      callback.Meta = (APTR)key;
      if (auto obj = access_object(obj_ref)) {
         error = AsyncAction(action_id, obj, nullptr, &callback);
         release_object(obj_ref);
      }
      else error = log.warning(ERR::AccessObject);
   }

   if (error != ERR::Okay) {
      if (callback.defined()) luaL_unref(Lua, LUA_REGISTRYINDEX, callback.ProcedureID);
      luaL_error(Lua, error);
   }

   return 0;
}

//********************************************************************************************************************
// Usage: error = thread.method(Object, Method, Callback, Key, Args...)

static int thread_method(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);

   #ifndef NDEBUG
   auto stack_top = lua_gettop(Lua);
   #endif

   // Args: Object (1), Action (2), Callback (3), Key (4), Parameters...

   if (auto obj_ref = lj_lib_checkobject(Lua, 1)) {
      if (auto method = luaL_checkstring(Lua, 2)) {
         MethodEntry *table;
         int total_methods, i;

         // TODO: We should be using a hashmap here.

         if ((obj_ref->classptr->get(FID_Methods, table, total_methods) IS ERR::Okay) and (table)) {
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
                  callback = FUNCTION(Lua->script, luaL_ref(Lua, LUA_REGISTRYINDEX));
               }
               else if (type IS LUA_TFUNCTION) {
                  lua_pushvalue(Lua, 3);
                  callback = FUNCTION(Lua->script, luaL_ref(Lua, LUA_REGISTRYINDEX));
               }
               else callback.Type = CALL::NIL;
               callback.Meta = APTR(lua_tointeger(Lua, 4));

               if (argsize > 0) {
                  auto argbuffer = std::make_unique<int8_t[]>(argsize+8); // +8 for overflow protection in build_args()
                  int resultcount;

                  // Remove the first 4 required arguments so that the user's custom parameters are left on the stack.
                  lua_rotate(Lua, 1, -4);
                  lua_pop(Lua, 4);
                  if ((error = build_args(Lua, args, argsize, argbuffer.get(), &resultcount)) IS ERR::Okay) {
                     if (obj_ref->ptr) {
                        error = AsyncAction(action_id, obj_ref->ptr, argbuffer.get(), &callback);
                     }
                     else {
                        if (!resultcount) {
                           if ((obj = access_object(obj_ref))) {
                              error = AsyncAction(action_id, obj, argbuffer.get(), &callback);
                              release_object(obj_ref);
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
                  if (obj_ref->ptr) {
                     error = AsyncAction(action_id, obj_ref->ptr, nullptr, &callback);
                  }
                  else if ((obj = access_object(obj_ref))) {
                     error = AsyncAction(action_id, obj, nullptr, &callback);
                     release_object(obj_ref);
                  }
                  else error = log.warning(ERR::AccessObject);
               }

               if (error != ERR::Okay) {
                  if (callback.defined()) luaL_unref(Lua, LUA_REGISTRYINDEX, callback.ProcedureID);
                  luaL_error(Lua, error);
               }

               #ifndef NDEBUG
               // After the lua_rotate/lua_pop of 4 args, the stack should have shrunk accordingly.
               // For the no-args path the stack is unchanged; for the args path it lost 4 entries.
               if (argsize > 0) assert(lua_gettop(Lua) IS stack_top - 4);
               else assert(lua_gettop(Lua) IS stack_top);
               #endif

               return 0;
            }
         }

         luaL_error(Lua, "No '%s' method for class %s.", method, obj_ref->classptr->ClassName);
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

   luaL_newmetatable(Lua, "Tiri.thread");
   lua_pushstring(Lua, "Tiri.thread");
   lua_setfield(Lua, -2, "__name");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable created earlier
   lua_settable(Lua, -3);   // metatable.__index = metatable

   luaL_openlib(Lua, nullptr, threadlib_methods, 0);
   luaL_openlib(Lua, "thread", threadlib_functions, 0);

   // Register thread interface prototypes for compile-time type inference
   reg_iface_prototype("thread", "action", {}, { TiriType::Any, TiriType::Any, TiriType::Func, TiriType::Num });
   reg_iface_prototype("thread", "method", {}, { TiriType::Any, TiriType::Str, TiriType::Func, TiriType::Num });
   reg_iface_prototype("thread", "script", {}, { TiriType::Object, TiriType::Func });
}
