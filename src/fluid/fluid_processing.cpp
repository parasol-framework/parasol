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
               auto field_hash = strihash(field_name);

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
// Puts a process to sleep with message processing in the background.  Can be woken early with a signal (i.e.
// proc.signal()).
//
// Lua's internal signal flag is always reset on entry in case it has been polluted by prior activity.  This behaviour
// can be disabled by setting the third argument to false.
//
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

   ERR error;
   int timeout;

   auto fp = (fprocessing *)get_meta(Lua, lua_upvalueindex(1), "Fluid.processing");
   if (fp) timeout = F2T(fp->Timeout * 1000.0);
   else timeout = -1;

   if (lua_type(Lua, 1) IS LUA_TNUMBER) timeout = F2T(lua_tonumber(Lua, 1) * 1000.0);
   if (timeout < 0) timeout = -1; // Wait indefinitely

   bool wake_on_signal;
   if (lua_type(Lua, 2) IS LUA_TBOOLEAN) wake_on_signal = lua_toboolean(Lua, 2);
   else if (!timeout) wake_on_signal = false; // We don't want to intercept signals if just processing messages
   else wake_on_signal = true;

   bool reset_state = true;
   if (lua_type(Lua, 3) IS LUA_TBOOLEAN) reset_state = lua_toboolean(Lua, 3);

   log.branch("Timeout: %d, WakeOnSignal: %c", timeout, wake_on_signal ? 'Y' : 'N');

   if (wake_on_signal) {
      if ((fp) and (fp->Signals) and (not fp->Signals->empty())) {
         // Use custom signals provided by the client (or Fluid if no objects were specified).
         auto signal_list_c = std::make_unique<ObjectSignal[]>(fp->Signals->size() + 1);
         int i = 0;
         for (auto &entry : *fp->Signals) signal_list_c[i++] = entry;
         signal_list_c[i].Object = nullptr;

         std::scoped_lock lock(recursion);
         error = WaitForObjects(PMF::NIL, timeout, signal_list_c.get());
      }
      else { // Default behaviour: Sleeping can be broken with a signal to the Fluid object.
         ObjectSignal signal_list_c[2];
         signal_list_c[0].Object   = Lua->Script;
         signal_list_c[1].Object   = nullptr;

         std::scoped_lock lock(recursion);
         error = WaitForObjects(PMF::NIL, timeout, signal_list_c);
      }
   }
   else {
      std::scoped_lock lock(recursion);
      WaitTime(timeout / 1000.0); // Convert milliseconds to seconds
      error = ERR::Okay;
   }

   lua_pushinteger(Lua, int(error));
   return 1;
}

//********************************************************************************************************************
// Usage: proc.signal() or processing.signal()
//
// Signals the Fluid object.  Note that this is ineffective if the user provided a list of objects to monitor for signalling.

static int processing_signal(lua_State *Lua)
{
   Action(AC::Signal, Lua->Script, nullptr);
   return 0;
}

//********************************************************************************************************************
// Usage: processing.flush()
//
// Flushes any pending signals from the Fluid object.

static int processing_flush(lua_State *Lua)
{
   Lua->Script->Object::Flags = Lua->Script->Object::Flags & (~NF::SIGNALLED);
   return 0;
}

//********************************************************************************************************************
// Usage: task = processing.task()
//
// Returns a Fluid object that references the current task.

static int processing_task(lua_State *Lua)
{
   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   object *obj = push_object(prv->Lua, CurrentTask());
   obj->Detached = true;  // External reference
   return 1;
}

//********************************************************************************************************************
// Internal: Processing index call - for objects returned from processing.new() only.

static int processing_get(lua_State *Lua)
{
   if (auto fieldname = luaL_checkstring(Lua, 2)) {
      if (std::string_view("sleep") IS fieldname) {
         lua_pushvalue(Lua, 1);
         lua_pushcclosure(Lua, &processing_sleep, 1);
         return 1;
      }
      else if (std::string_view("signal") IS fieldname) {
         lua_pushvalue(Lua, 1);
         lua_pushcclosure(Lua, &processing_signal, 1);
         return 1;
      }
      else if (std::string_view("flush") IS fieldname) {
         Lua->Script->Object::Flags = Lua->Script->Object::Flags & (~NF::SIGNALLED);
         if (auto fp = (fprocessing *)get_meta(Lua, lua_upvalueindex(1), "Fluid.processing")) {
            for (auto &entry : *fp->Signals) {
               entry.Object->Flags &= (~NF::SIGNALLED);
            }
         }
         return 0;
      }
      else return luaL_error(Lua, "Unrecognised index '%s'", fieldname);
   }

   return 0;
}

//********************************************************************************************************************
// Call a function on the next message processing cycle.
//
// Usage: processing.delayedCall(function() ... end)

static MsgHandler *delayed_call_handle;

static ERR msg_handler(APTR Meta, int MsgID, int MsgType, APTR Message, int MsgSize)
{
   if (MsgSize != sizeof(int)) return pf::Log(__FUNCTION__).warning(ERR::Args);

   auto lua = (lua_State *)Meta;
   auto prv = (prvFluid *)lua->Script->ChildPrivate;
   int ref = *(int *)Message;
   lua_rawgeti(lua, LUA_REGISTRYINDEX, ref); // Get the function from the registry
   luaL_unref(lua, LUA_REGISTRYINDEX, ref); // Remove it
   if (lua_pcall(prv->Lua, 0, 0, 0)) {
      process_error(lua->Script, "delayedCall()");
   }
   return ERR::Okay;
}

static int processing_delayed_call(lua_State *Lua)
{
   static MSGID msgid = MSGID::NIL;
   if (msgid IS MSGID::NIL) {
      msgid = MSGID(AllocateID(IDTYPE::MESSAGE));
      auto func = C_FUNCTION(msg_handler, Lua);
      if (AddMsgHandler(msgid, &func, &delayed_call_handle) != ERR::Okay) {
         luaL_error(Lua, "Failed to register handler for delayedCall().");
      }
   }

   if (lua_type(Lua, 1) IS LUA_TFUNCTION) {
      int ref = luaL_ref(Lua, LUA_REGISTRYINDEX);
      SendMessage(msgid, MSF::NIL, &ref, sizeof(ref));
   }
   else luaL_error(Lua, "Expected a function to register as a message hook.");
   return 0;
}

//********************************************************************************************************************
// Garbage collector.

static int processing_destruct(lua_State *Lua)
{
   auto fp = (fprocessing *)luaL_checkudata(Lua, 1, "Fluid.processing");
   if (fp->Signals) { delete fp->Signals; fp->Signals = nullptr; }
   return 0;
}

//********************************************************************************************************************
// Register the processing interface.

static const luaL_Reg processinglib_functions[] = {
   { "new",     processing_new },
   { "sleep",   processing_sleep },
   { "signal",  processing_signal },
   { "task",    processing_task },
   { "flush",   processing_flush },
   { "delayedCall", processing_delayed_call },
   { nullptr, nullptr }
};

static const luaL_Reg processinglib_methods[] = {
   { "__index",    processing_get },
   { "__gc",       processing_destruct },
   { nullptr, nullptr }
};

void register_processing_class(lua_State *Lua)
{
   pf::Log log(__FUNCTION__);
   log.trace("Registering processing interface.");

   luaL_newmetatable(Lua, "Fluid.processing");
   lua_pushstring(Lua, "Fluid.processing");
   lua_setfield(Lua, -2, "__name");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable created earlier
   lua_settable(Lua, -3);   // metatable.__index = metatable
   luaL_openlib(Lua, nullptr, processinglib_methods, 0);

   luaL_openlib(Lua, "processing", processinglib_functions, 0);
}
