/*********************************************************************************************************************

*********************************************************************************************************************/

#define PRV_SCRIPT
#define PRV_FLUID
#define PRV_FLUID_MODULE
#include <parasol/main.h>
#include <parasol/modules/fluid.h>
#include <inttypes.h>
#include <mutex>

extern "C" {
 #include "lua.h"
 #include "lualib.h"
 #include "lauxlib.h"
 #include "lj_obj.h"
}

#include "hashes.h"
#include "defs.h"

/*********************************************************************************************************************
** Usage: proc = processing.new({ timeout=5.0, signals={ obj1, obj2, ... } })
**
** Creates a new processing object.
*/

static int processing_new(lua_State *Lua)
{
   fprocessing *fp;
   if ((fp = (fprocessing *)lua_newuserdata(Lua, sizeof(fprocessing)))) {
      luaL_getmetatable(Lua, "Fluid.processing");
      lua_setmetatable(Lua, -2);

      // Default configuration
      fp->Timeout = -1;
      fp->Signals = 0;

      if (not (fp->Signals = new (std::nothrow) std::list<ObjectSignal>)) {
         luaL_error(Lua, "Memory allocation failed.");
      }

      if (lua_istable(Lua, 1)) {
         lua_pushnil(Lua);  // Access first key for lua_next()
         while (lua_next(Lua, 1) != 0) {
            if (auto field_name = luaL_checkstring(Lua, -2)) {
               auto field_hash = StrHash(field_name, 0);

               switch (field_hash) {
                  case HASH_TIMEOUT:
                     fp->Timeout = lua_tonumber(Lua, -1);
                     break;

                  case HASH_SIGNALS: {
                     if (lua_istable(Lua, -1)) { // { obj1, obj2, ... }
                        lua_pushnil(Lua);
                        while (lua_next(Lua, -2)) {
                           struct object *obj;
                           if ((obj = (struct object *)get_meta(Lua, -1, "Fluid.obj"))) {
                              ObjectSignal sig = { .Object = obj->ObjectPtr };
                              fp->Signals->push_back(sig);
                           }
                           else {
                              luaL_error(Lua, "Expected object in signal list, got %s.", lua_typename(Lua, lua_type(Lua, -2)));
                              return 0;
                           }

                           lua_pop(Lua, 1); // Remove value, keep the key
                        }
                     }
                     else luaL_error(Lua, "The signals option requires a table of object references.");
                     break;
                  }

                  default:
                     luaL_error(Lua, "Unrecognised option '%s'", field_name);
               }
            }
            else luaL_error(Lua, "Unrecognised option.");

            lua_pop(Lua, 1);  // removes 'value'; keeps 'key' for the proceeding lua_next() iteration
         }
      }

      if (fp->Signals->empty()) { // Monitor the script for a signal if the client did not specify any objects
         ObjectSignal sig = { .Object = Lua->Script };
         fp->Signals->push_back(sig);
      }

      return 1;  // new userdatum is already on the stack
   }
   else luaL_error(Lua, "Failed to create new processing object.");

   return 0;
}

//********************************************************************************************************************
// Usage: err = proc.sleep([Seconds], [WakeOnSignal=true])
//
// Puts a process to sleep with message processing in the background.  Can be woken early with a signal.
// Setting seconds to zero will process outstanding messages and return immediately.
//
// NOTE: Can be called directly as an interface function or as a member of a processing object.

static int processing_sleep(lua_State *Lua)
{
   { // Always collect your garbage before going to sleep
      pf::Log log;
      log.traceBranch("Collecting garbage.");
      lua_gc(Lua, LUA_GCCOLLECT, 0);
   }

   pf::Log log;
   static std::recursive_mutex recursion; // Intentionally accessible to all threads

   ERROR error;
   LONG timeout;

   auto fp = (fprocessing *)get_meta(Lua, lua_upvalueindex(1), "Fluid.processing");
   if (fp) timeout = F2T(fp->Timeout * 1000.0);
   else timeout = -1;

   if (lua_type(Lua, 1) IS LUA_TNUMBER) timeout = F2T(lua_tonumber(Lua, 1) * 1000.0);
   if (timeout < 0) timeout = -1; // Wait indefinitely

   bool wake_on_signal;
   if (lua_type(Lua, 2) IS LUA_TBOOLEAN) wake_on_signal = lua_toboolean(Lua, 2);
   else wake_on_signal = true;

   log.branch("Timeout: %d, WakeOnSignal: %c", timeout, wake_on_signal ? 'Y' : 'N');

   // The Lua signal flag is always reset on entry just in case it has been polluted by prior activity.
   // All other objects can be pre-signalled legitimately.

   Lua->Script->BaseClass::Flags = Lua->Script->BaseClass::Flags & (~NF::SIGNALLED);

   if (wake_on_signal) {
      if ((fp) and (fp->Signals) and (not fp->Signals->empty())) {
         // Use custom signals provided by the client (or Fluid if no objects were specified).
         auto signal_list_c = std::make_unique<ObjectSignal[]>(fp->Signals->size() + 1);
         LONG i = 0;
         for (auto &entry : *fp->Signals) signal_list_c[i++] = entry;
         signal_list_c[i].Object = NULL;

         std::scoped_lock lock(recursion);
         error = WaitForObjects(PMF::NIL, timeout, signal_list_c.get());
      }
      else { // Default behaviour: Sleeping can be broken with a signal to the Fluid object.
         ObjectSignal signal_list_c[2];
         signal_list_c[0].Object   = Lua->Script;
         signal_list_c[1].Object   = NULL;

         std::scoped_lock lock(recursion);
         error = WaitForObjects(PMF::NIL, timeout, signal_list_c);
      }
   }
   else {
      std::scoped_lock lock(recursion);
      WaitTime(timeout / 1000, (timeout % 1000) * 1000);
      error = ERR_Okay;
   }

   lua_pushinteger(Lua, error);
   return 1;
}

//********************************************************************************************************************
// Usage: proc.signal()
//
// Signals the Fluid object.  Note that this is ineffective if the user provided a list of objects to monitor for signalling.

static int processing_signal(lua_State *Lua)
{
   Action(AC_Signal, Lua->Script, NULL);
   return 0;
}

//********************************************************************************************************************
// Internal: Processing index call

static int processing_get(lua_State *Lua)
{
   auto fp = (struct fprocessing *)lua_touserdata(Lua, 1);

   if (fp) {
      CSTRING fieldname;
      if ((fieldname = luaL_checkstring(Lua, 2))) {
         if (!StrCompare("sleep", fieldname, 0, STR::MATCH_CASE)) {
            lua_pushvalue(Lua, 1);
            lua_pushcclosure(Lua, &processing_sleep, 1);
            return 1;
         }
         else if (!StrCompare("signal", fieldname, 0, STR::MATCH_CASE)) {
            lua_pushvalue(Lua, 1);
            lua_pushcclosure(Lua, &processing_signal, 1);
            return 1;
         }
         else return luaL_error(Lua, "Unrecognised field name '%s'", fieldname);
      }
   }

   return 0;
}

/*********************************************************************************************************************
** Garbage collecter.
*/

static int processing_destruct(lua_State *Lua)
{
   auto fp = (fprocessing *)luaL_checkudata(Lua, 1, "Fluid.processing");
   if (fp->Signals) { delete fp->Signals; fp->Signals = NULL; }
   return 0;
}

//********************************************************************************************************************
// Register the fprocessing interface.

static const luaL_Reg processinglib_functions[] = {
   { "new",   processing_new },
   { "sleep", processing_sleep },
   { NULL, NULL }
};

static const luaL_Reg processinglib_methods[] = {
   { "__index",    processing_get },
   { "__gc",       processing_destruct },
   { NULL, NULL }
};

void register_processing_class(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);
   log.trace("Registering processing interface.");

   luaL_newmetatable(Lua, "Fluid.processing");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable created earlier
   lua_settable(Lua, -3);   // metatable.__index = metatable
   luaL_openlib(Lua, NULL, processinglib_methods, 0);

   luaL_openlib(Lua, "processing", processinglib_functions, 0);
}
