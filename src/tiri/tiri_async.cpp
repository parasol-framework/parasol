/*********************************************************************************************************************

The async interface provides support for the parallel execution of actions and methods against objects:

  async.action(Object, Action, Callback, Key, Args...)

  async.method(Object, Action, Callback, Key, Args...)

The script() method is a simplified variant of async.action() for scripts, but there's some potential to add
additional functionality in the future.

  async.script(Script, Callback)

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

// Message payload for thread completion callbacks (used by script, action, and method)

struct ThreadMsg {
   int       Callback;  // Client callback reference
   int       ObjRef;    // Registry reference that pins the GCobject from GC collection
   objScript *Owner;    // The parent script that owns the registry references
   double    Key;       // Client-provided key value forwarded to the callback
};

//********************************************************************************************************************
// Callback following execution (executed by the main thread, not the child)
// Must follow the signature declared in AsyncAction() documentation.

static void msg_thread_complete(ACTIONID ActionID, OBJECTPTR Object, ERR Error, ThreadMsg *Msg)
{
   pf::Log log("thread_callback");

   auto prv = (prvTiri *)Msg->Owner->ChildPrivate;

   if (Msg->Callback != LUA_NOREF) {
      if ((Object) and (Object->baseClassID() IS CLASSID::SCRIPT)) {
         auto args = std::to_array<ScriptArg>({
            { "Object", Object, FD_OBJECTPTR }
         });
         Msg->Owner->callback(Msg->Callback, args.data(), int(args.size()), nullptr);
      }
      else {
         auto args = std::to_array<ScriptArg>({
            { "ActionID", int(ActionID) },
            { "Object",   Object, FD_OBJECTPTR },
            { "Error",    int(Error) },
            { "Key",      Msg->Key }
         });
         Msg->Owner->callback(Msg->Callback, args.data(), int(args.size()), nullptr);
      }
      luaL_unref(prv->Lua, LUA_REGISTRYINDEX, Msg->Callback); // Drop the procedure reference
   }

   // Unpin the GCobject from the registry and release the pin on the underlying object.

   lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, Msg->ObjRef);
   auto gc_script = lua_toobject(prv->Lua, -1);
   lua_pop(prv->Lua, 1);
   luaL_unref(prv->Lua, LUA_REGISTRYINDEX, Msg->ObjRef);

   if (gc_script and gc_script->ptr) gc_script->ptr->unpin(true);
   delete Msg;
}

//********************************************************************************************************************
// Usage: async.script(Script, Callback)
//
// Pins the Script object to prevent premature destruction, then executes it in its own thread.  The pin is
// released when the thread completes and the callback message is processed on the main thread.  No object lock
// is held across the thread boundary — acActivate() acquires its own lock internally via ScopedObjectAccess.

static int async_script(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);

   GCobject *gc_script = lua_toobject(Lua, 1);
   if (gc_script->classptr->ClassID != CLASSID::SCRIPT) luaL_error(Lua, ERR::WrongClass);
   if (not gc_script->ptr) luaL_error(Lua, ERR::ObjectCorrupt);

   gc_script->ptr->pin(); // Prevent the object from being freed while the thread is running.

   int client_callback = LUA_NOREF;
   if (lua_isfunction(Lua, 2)) {
      lua_pushvalue(Lua, 2);
      client_callback = luaL_ref(Lua, LUA_REGISTRYINDEX);
   }

   // Pin the script in the registry so the GC cannot collect it while the thread is running.
   lua_pushvalue(Lua, 1);
   int obj_ref = luaL_ref(Lua, LUA_REGISTRYINDEX);

   auto msg = new ThreadMsg { client_callback, obj_ref, Lua->script, 0 };
   auto callback = C_FUNCTION(msg_thread_complete, msg);

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
// Usage: async.action(Object, Action, Callback, Key, Args...)

static int async_action(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);

   // Args: Object (1), Action (2), Callback (3), Key (4), Parameters...

   GCobject *gc_obj;
   if (not (gc_obj = lj_lib_checkobject(Lua, 1))) luaL_argerror(Lua, 1, "Object required.");
   if (not gc_obj->ptr) luaL_error(Lua, ERR::ObjectCorrupt);

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

   int client_callback = LUA_NOREF;
   type = lua_type(Lua, 3); // Optional callback.
   if (type IS LUA_TSTRING) {
      lua_getglobal(Lua, lua_tostring(Lua, 3));
      client_callback = luaL_ref(Lua, LUA_REGISTRYINDEX);
   }
   else if (type IS LUA_TFUNCTION) {
      lua_pushvalue(Lua, 3);
      client_callback = luaL_ref(Lua, LUA_REGISTRYINDEX);
   }

   int arg_size = 0;
   const FunctionField *args = nullptr;

   if ((glActions[int(action_id)].Args) and (glActions[int(action_id)].Size)) {
      arg_size = glActions[int(action_id)].Size;
      args = glActions[int(action_id)].Args;
   }

   log.trace("#%d/%p, Action: %s/%d, Args: %d", gc_obj->uid, gc_obj->ptr, action, int(action_id), arg_size);

   // Pin the object and GCobject to prevent destruction while the thread is running.

   gc_obj->ptr->pin();

   lua_pushvalue(Lua, 1);
   int obj_ref = luaL_ref(Lua, LUA_REGISTRYINDEX);

   auto msg = new ThreadMsg { client_callback, obj_ref, Lua->script, lua_tonumber(Lua, 4) };
   auto callback = C_FUNCTION(msg_thread_complete, msg);

   ERR error = ERR::Okay;
   if (arg_size > 0) {
      auto arg_buffer = std::make_unique<int8_t[]>(arg_size+8); // +8 for overflow protection in build_args()
      int result_count;

      if ((error = build_args(Lua, args, arg_size, arg_buffer.get(), &result_count)) IS ERR::Okay) {
         if (!result_count) {
            error = AsyncAction(action_id, gc_obj->ptr, arg_buffer.get(), &callback);
         }
         else {
            gc_obj->ptr->unpin(true);
            luaL_unref(Lua, LUA_REGISTRYINDEX, obj_ref);
            luaL_unref(Lua, LUA_REGISTRYINDEX, client_callback);
            delete msg;
            luaL_error(Lua, "Actions that return results are not yet supported.");
         }
      }
      else {
         gc_obj->ptr->unpin(true);
         luaL_unref(Lua, LUA_REGISTRYINDEX, obj_ref);
         luaL_unref(Lua, LUA_REGISTRYINDEX, client_callback);
         delete msg;
         luaL_error(Lua, "Argument build failure for %s.", glActions[int(action_id)].Name);
      }
   }
   else { // No parameters.
      error = AsyncAction(action_id, gc_obj->ptr, nullptr, &callback);
   }

   if (error != ERR::Okay) {
      gc_obj->ptr->unpin(true);
      luaL_unref(Lua, LUA_REGISTRYINDEX, obj_ref);
      luaL_unref(Lua, LUA_REGISTRYINDEX, client_callback);
      delete msg;
      luaL_error(Lua, error);
   }

   return 0;
}

//********************************************************************************************************************
// Usage: error = async.method(Object, Method, Callback, Key, Args...)

static int async_method(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);

   auto gc_obj = lj_lib_checkobject(Lua, 1);
   if (not gc_obj->ptr) luaL_error(Lua, ERR::ObjectCorrupt);
   auto method = luaL_checkstring(Lua, 2);

   MethodEntry *table;
   int total_methods, i;

   // TODO: We should be using a hashmap here.

   if ((gc_obj->classptr->get(FID_Methods, table, total_methods) IS ERR::Okay) and (table)) {
      bool found = false;
      for (i=1; i < total_methods; i++) {
         if ((table[i].Name) and (iequals(table[i].Name, method))) { found = true; break; }
      }

      if (found) {
         auto args      = table[i].Args;
         auto argsize   = table[i].Size;
         auto action_id = table[i].MethodID;

         int client_callback = LUA_NOREF;
         int type = lua_type(Lua, 3); // Optional callback.
         if (type IS LUA_TSTRING) {
            lua_getglobal(Lua, (STRING)lua_tostring(Lua, 3));
            client_callback = luaL_ref(Lua, LUA_REGISTRYINDEX);
         }
         else if (type IS LUA_TFUNCTION) {
            lua_pushvalue(Lua, 3);
            client_callback = luaL_ref(Lua, LUA_REGISTRYINDEX);
         }

         // Pin the object and GCobject to prevent destruction while the thread is running.

         gc_obj->ptr->pin();

         lua_pushvalue(Lua, 1);
         int obj_ref = luaL_ref(Lua, LUA_REGISTRYINDEX);

         auto msg = new ThreadMsg { client_callback, obj_ref, Lua->script, lua_tonumber(Lua, 4) };
         auto callback = C_FUNCTION(msg_thread_complete, msg);

         ERR error = ERR::Okay;
         if (argsize > 0) {
            auto argbuffer = std::make_unique<int8_t[]>(argsize+8); // +8 for overflow protection in build_args()
            int resultcount;

            // Remove the first 4 required arguments so that the user's custom parameters are left on the stack.
            lua_rotate(Lua, 1, -4);
            lua_pop(Lua, 4);
            if ((error = build_args(Lua, args, argsize, argbuffer.get(), &resultcount)) IS ERR::Okay) {
               if (!resultcount) {
                  error = AsyncAction(action_id, gc_obj->ptr, argbuffer.get(), &callback);
               }
               else {
                  gc_obj->ptr->unpin(true);
                  luaL_unref(Lua, LUA_REGISTRYINDEX, obj_ref);
                  luaL_unref(Lua, LUA_REGISTRYINDEX, client_callback);
                  delete msg;
                  luaL_error(Lua, "Methods that return results are not yet supported.");
               }
            }
            else {
               gc_obj->ptr->unpin(true);
               luaL_unref(Lua, LUA_REGISTRYINDEX, obj_ref);
               luaL_unref(Lua, LUA_REGISTRYINDEX, client_callback);
               delete msg;
               luaL_error(Lua, "Argument build failure for %s.", glActions[int(action_id)].Name);
            }
         }
         else { // No parameters.
            error = AsyncAction(action_id, gc_obj->ptr, nullptr, &callback);
         }

         if (error != ERR::Okay) {
            gc_obj->ptr->unpin(true);
            luaL_unref(Lua, LUA_REGISTRYINDEX, obj_ref);
            luaL_unref(Lua, LUA_REGISTRYINDEX, client_callback);
            delete msg;
            luaL_error(Lua, error);
         }

         return 0;
      }
   }

   luaL_error(Lua, "No '%s' method for class %s.", method, gc_obj->classptr->ClassName);
   return 0;
}

//********************************************************************************************************************
// Register the async interface.

static const luaL_Reg asynclib_functions[] = {
   { "action", async_action },
   { "method", async_method },
   { "script", async_script },
   { nullptr, nullptr }
};

static const luaL_Reg asynclib_methods[] = {
   //{ "__index",    async_get },
   //{ "__newindex", async_set },
   { nullptr, nullptr }
};

void register_async_class(lua_State *Lua)
{
   pf::Log log;

   log.trace("Registering async interface.");

   luaL_newmetatable(Lua, "Tiri.async");
   lua_pushstring(Lua, "Tiri.async");
   lua_setfield(Lua, -2, "__name");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable created earlier
   lua_settable(Lua, -3);   // metatable.__index = metatable

   luaL_openlib(Lua, nullptr, asynclib_methods, 0);
   luaL_openlib(Lua, "async", asynclib_functions, 0);

   // Register async interface prototypes for compile-time type inference
   reg_iface_prototype("async", "action", {}, { TiriType::Any, TiriType::Any, TiriType::Func, TiriType::Num });
   reg_iface_prototype("async", "method", {}, { TiriType::Any, TiriType::Str, TiriType::Func, TiriType::Num });
   reg_iface_prototype("async", "script", {}, { TiriType::Object, TiriType::Func });
}
