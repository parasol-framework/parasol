
#define PRV_SCRIPT
#define PRV_FLUID
#define PRV_FLUID_MODULE
#include <parasol/main.h>
#include <parasol/modules/fluid.h>
#include <parasol/strings.hpp>
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

//********************************************************************************************************************
// check() is the equivalent of an assert() for error codes.  Any major error code will be converted to an
// exception containing a readable string for the error code.  It is most powerful when used in conjunction with
// the catch() function, which will apply the line number of the exception to the result.  The error code will
// also be propagated to the Script object's Error field.
//
// This function also serves a dual purpose in that it can be used to raise exceptions when an error condition needs to
// be propagated.

int fcmd_check(lua_State *Lua)
{
   if (lua_type(Lua, 1) IS LUA_TNUMBER) {
      ERR error = ERR(lua_tointeger(Lua, 1));
      if (int(error) >= int(ERR::ExceptionThreshold)) {
         auto prv = (prvFluid *)Lua->Script->ChildPrivate;
         prv->CaughtError = error;
         luaL_error(Lua, GetErrorMsg(error));
      }
   }
   return 0;
}

//********************************************************************************************************************
// raise() will raise an error immediately from an error code.  Unlike check(), all codes have coverage, including
// minor codes.  The error code will also be propagated to the Script object's Error field.

int fcmd_raise(lua_State *Lua)
{
   if (lua_type(Lua, 1) IS LUA_TNUMBER) {
      ERR error = ERR(lua_tointeger(Lua, 1));
      auto prv = (prvFluid *)Lua->Script->ChildPrivate;
      prv->CaughtError = error;
      luaL_error(Lua, GetErrorMsg(error));
   }
   return 0;
}

//********************************************************************************************************************
// Use catch() to switch on exception handling for functions that return an error code other than ERR::Okay, as well as
// normal exceptions that would otherwise be caught by pcall().  Areas affected include obj.new(); any module function
// that returns an ERROR; any method or action called on an object.
//
// The caught error code is returned by default, or if no exception handler is defined then the entire exception table
// is returned.
//
// Be aware that the scope of the catch will extend into any sub-routines that are called.  Mis-use of catch() can be
// confusing for this reason, and pcall() is more appropriate when broad exception handling is desired.
//
// catch() is most useful for creating small code segments that limit any failures to their own scope.
//
//   err, result = catch(function()
//      // Code to execute
//      return 'success'
//   end,
//   function(Exception)
//      // Exception handler
//      print("Code: " .. nz(Exception.code,"LUA") .. ", Message: " .. Exception.message)
//   end)
//
// As above, but the handler is only called if certain codes are raised.  Any mismatched errors will throw to the parent code.
//
//   err, result = catch(function()
//      // Code to execute
//      return 'success'
//   end,
//   { ERR::Failed, ERR::Terminate }, // Errors to filter for
//   fuction(Exception) // Exception handler for the filtered errors
//   end)
//
// To silently ignore exceptions, or to receive the thrown exception details as a table result:
//
//   local exception, result, ... = catch(function()
//      // Code to execute
//      return result, ...
//   end)
//
// Errors that are NOT treated as exceptions are Okay, False, LimitedSuccess, Cancelled, NothingDone, Continue, Skip,
// Retry, DirEmpty.

int fcmd_catch_handler(lua_State *Lua)
{
   lua_Debug ar;
   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   if (lua_getstack(Lua, 2, &ar)) {
      lua_getinfo(Lua, "nSl", &ar);
      // ar.currentline, ar.name, ar.source, ar.short_src, ar.linedefined, ar.lastlinedefined, ar.what
      prv->ErrorLine = ar.currentline;
   }
   else prv->ErrorLine = -1;

   return 1; // Return 1 to rethrow the exception table, no need to re-push the value
}

int fcmd_catch(lua_State *Lua)
{
   auto prv = (prvFluid *)Lua->Script->ChildPrivate;

   if (lua_gettop(Lua) >= 2) {
      auto type = lua_type(Lua, 1);
      if (type IS LUA_TFUNCTION) {
         int catch_filter = 0;
         type = lua_type(Lua, 2);

         int a = 2;
         if (type IS LUA_TTABLE) {
            // First argument is a list of error codes to filter on, second argument is the exception handler.
            lua_pushvalue(Lua, a++);
            catch_filter = luaL_ref(Lua, LUA_REGISTRYINDEX);
            type = lua_type(Lua, a);
         }

         if (type IS LUA_TFUNCTION) {
            BYTE caught_by_filter = FALSE;
            prv->Catch++; // Flag to convert ERR results to exceptions.
            prv->CaughtError = ERR::Okay;
            lua_pushcfunction(Lua, fcmd_catch_handler);
            lua_pushvalue(Lua, 1); // Parameter #1 is the function to call.
            int result_top = lua_gettop(Lua);
            if (lua_pcall(Lua, 0, LUA_MULTRET, -2)) { // An exception was raised!
               prv->Catch--;

               if ((prv->CaughtError >= ERR::ExceptionThreshold) and (catch_filter)) { // Apply error code filtering
                  lua_rawgeti(Lua, LUA_REGISTRYINDEX, catch_filter);
                  lua_pushnil(Lua);  // First key
                  while ((!caught_by_filter) and (lua_next(Lua, -2) != 0)) { // Iterate over each table key
                     // -1 is the value and -2 is the key.
                     if (lua_tointeger(Lua, -1) IS int(prv->CaughtError)) {
                        caught_by_filter = TRUE;
                        lua_pop(Lua, 1); // Pop the key because we're going to break the loop early.
                     }
                     lua_pop(Lua, 1); // Removes 'value'; keeps 'key' for next iteration
                  }
                  lua_pop(Lua, 1); // Pop the catch_filter
               }
               else caught_by_filter = TRUE;

               if (catch_filter) luaL_unref(Lua, LUA_REGISTRYINDEX, catch_filter);

               if (caught_by_filter) {
                  lua_pushvalue(Lua, a); // For lua_call()

                  // Build an exception table: { code=123, message="Description" }

                  lua_newtable(Lua);
                  lua_pushstring(Lua, "code");
                  if (prv->CaughtError >= ERR::ExceptionThreshold) lua_pushinteger(Lua, int(prv->CaughtError));
                  else lua_pushnil(Lua);
                  lua_settable(Lua, -3);

                  lua_pushstring(Lua, "message");
                  if (lua_type(Lua, -4) IS LUA_TSTRING) {
                     lua_pushvalue(Lua, -4); // This is the error exception string returned by pcall()
                  }
                  else if (prv->CaughtError != ERR::Okay) lua_pushstring(Lua, GetErrorMsg(prv->CaughtError));
                  else lua_pushstring(Lua, "<No message>");
                  lua_settable(Lua, -3);

                  lua_pushstring(Lua, "line");
                  lua_pushinteger(Lua, prv->ErrorLine);
                  lua_settable(Lua, -3);

                  lua_call(Lua, 1, 0); // nargs, nresults

                  lua_pop(Lua, 1); // Pop the error message.
               }
               else luaL_error(Lua, lua_tostring(Lua, -1)); // Rethrow the message

               lua_pushinteger(Lua, prv->CaughtError != ERR::Okay ? int(prv->CaughtError) : int(ERR::Exception));
               return 1;
            }
            else { // pcall() was successful
               prv->Catch--;
               if (catch_filter) luaL_unref(Lua, LUA_REGISTRYINDEX, catch_filter);
               lua_pushinteger(Lua, int(ERR::Okay));
               int result_count = lua_gettop(Lua) - result_top + 1;
               lua_insert(Lua, -result_count); // Push the error code in front of any other results
               return result_count;
            }
         }
         else {
            if (catch_filter) luaL_unref(Lua, LUA_REGISTRYINDEX, catch_filter);
            luaL_argerror(Lua, 2, "Expected function.");
         }
      }
      else luaL_argerror(Lua, 1, "Expected function.");
   }
   else { // In single-function mode, exceptions are returned as a result.
      auto type = lua_type(Lua, 1);
      if (type IS LUA_TFUNCTION) {
         prv->Catch++; // Indicate to other routines that errors must be converted to exceptions.
         prv->CaughtError = ERR::Okay;

         lua_pushcfunction(Lua, fcmd_catch_handler);
         lua_pushvalue(Lua, 1); // Parameter #1 is the function to call.
         auto result_top = lua_gettop(Lua);
         if (lua_pcall(Lua, 0, LUA_MULTRET, -2)) {
            prv->Catch--;

            // -1 is the pcall() error string result
            // -2 is fcmd_catch_handler()
            // -3 is the function we called.

            lua_remove(Lua, -2); // Pop the handler
            lua_remove(Lua, -2); // Pop the function

            // Return an exception table: { code=123, message="Description", line=123 }

            lua_newtable(Lua); // +1 stack
            lua_pushstring(Lua, "code");
            if (prv->CaughtError >= ERR::ExceptionThreshold) lua_pushinteger(Lua, int(prv->CaughtError));
            else lua_pushnil(Lua); // Distinguish Lua exceptions by setting the code to nil.
            lua_settable(Lua, -3);

            lua_pushstring(Lua, "message");
            lua_pushvalue(Lua, -3); // Temp duplicate of the reference to -3; the error message returned by pcall()
            lua_settable(Lua, -3);

            lua_pushstring(Lua, "line");
            lua_pushinteger(Lua, prv->ErrorLine);
            lua_settable(Lua, -3);

            lua_remove(Lua, -2); // Remove the error msg to balance the stack
            return 1;
         }
         else {
            prv->Catch--; // Successful call
            lua_pushnil(Lua); // Use nil to indicate that no exception occurred
            auto result_count = lua_gettop(Lua) - result_top + 1;
            lua_insert(Lua, -result_count); // Push the error code in front of any other results
            return result_count;
         }
      }
      else luaL_argerror(Lua, 1, "Expected function.");
   }

   return 0;
}

//********************************************************************************************************************
// The event callback will be called with the following prototype:
//
// function callback(EventID, Args)
//
// Where Args is a named array containing the event parameters.  If the event is not known to Fluid, then no Args will
// be provided.

static void receive_event(pf::Event *Info, int InfoSize, APTR CallbackMeta)
{
   auto Script = (objScript *)CurrentContext();
   auto prv = (prvFluid *)Script->ChildPrivate;
   if (!prv) return;

   pf::Log log(__FUNCTION__);
   log.trace("Received event $%.8x%.8x", (int)((Info->EventID>>32) & 0xffffffff), (int)(Info->EventID & 0xffffffff));

   lua_rawgeti(prv->Lua, LUA_REGISTRYINDEX, intptr_t(CallbackMeta));

   lua_pushnumber(prv->Lua, Info->EventID);
   if (lua_pcall(prv->Lua, 1, 0, 0)) {
      process_error(Script, "Event Subscription");
   }

   log.traceBranch("Collecting garbage.");
   lua_gc(prv->Lua, LUA_GCCOLLECT, 0); // Run the garbage collector
}

//********************************************************************************************************************
// Usage: unsubscribeEvent(handle)

int fcmd_unsubscribe_event(lua_State *Lua)
{
   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   if (!prv) return 0;

   if (auto handle = lua_touserdata(Lua, 1)) {
      pf::Log log("unsubscribe_event");
      if ((Lua->Script->Flags & SCF::LOG_ALL) != SCF::NIL) log.msg("Handle: %p", handle);

      for (auto it=prv->EventList.begin(); it != prv->EventList.end(); it++) {
         if (it->EventHandle IS handle) {
            luaL_unref(prv->Lua, LUA_REGISTRYINDEX, it->Function);
            prv->EventList.erase(it);
            return 0;
         }
      }

      log.warning("Failed to link an event to handle %p.", handle);
   }
   else luaL_argerror(Lua, 1, "No handle provided.");

   return 0;
}

//********************************************************************************************************************
// Usage: error, handle = subscribeEvent("group.subgroup.name", function)

int fcmd_subscribe_event(lua_State *Lua)
{
   CSTRING event;
   if (!(event = lua_tostring(Lua, 1))) {
      luaL_argerror(Lua, 1, "Event string expected.");
      return 0;
   }

   if (!lua_isfunction(Lua, 2)) {
      if (!lua_isnil(Lua, 2)) {
         luaL_argerror(Lua, 2, "Function or nil expected.");
         return 0;
      }
   }

   // Generate the event ID

   char group[60];
   uint32_t group_hash = 0, subgroup_hash = 0;
   for (int i=0; event[i]; i++) {
      if (event[i] IS '.') {
         int j;
         if ((size_t)i >= sizeof(group)) luaL_error(Lua, "Buffer overflow.");
         for (j=0; (j < i) and ((size_t)j < sizeof(group)-1); j++) group[j] = event[j];
         group[j] = 0;
         group_hash = strihash(group);
         event += i + 1;

         for (int i=0; event[i]; i++) {
            if (event[i] IS '.') {
               int j;
               if ((size_t)i >= sizeof(group)) luaL_error(Lua, "Buffer overflow.");
               for (j=0; (j < i) and ((size_t)j < sizeof(group)-1); j++) group[j] = event[j];
               group[j] = 0;
               subgroup_hash = strihash(group);
               event += i + 1;
               break;
            }
         }
         break;
      }
   }

   auto group_id = EVG::NIL;
   if ((group_hash) and (subgroup_hash)) {
      switch (group_hash) {
         case HASH_FILESYSTEM: group_id = EVG::FILESYSTEM; break;
         case HASH_NETWORK:    group_id = EVG::NETWORK; break;
         case HASH_USER:       group_id = EVG::USER; break;
         case HASH_SYSTEM:     group_id = EVG::SYSTEM; break;
         case HASH_GUI:        group_id = EVG::GUI; break;
         case HASH_DISPLAY:    group_id = EVG::DISPLAY; break;
         case HASH_IO:         group_id = EVG::IO; break;
         case HASH_HARDWARE:   group_id = EVG::HARDWARE; break;
         case HASH_AUDIO:      group_id = EVG::AUDIO; break;
         case HASH_POWER:      group_id = EVG::POWER; break;
         case HASH_CLASS:      group_id = EVG::CLASS; break;
         case HASH_APP:        group_id = EVG::APP; break;
      }
   }

   if (group_id IS EVG::NIL) {
      luaL_error(Lua, "Invalid group name '%s' in event string.", event);
      return 0;
   }

   EVENTID event_id = GetEventID(group_id, group, event);

   if (!event_id) {
      luaL_argerror(Lua, 1, "Failed to build event ID.");
      lua_pushinteger(Lua, int(ERR::Failed));
      return 1;
   }
   else {
      APTR handle;
      lua_settop(Lua, 2);
      auto client_function = luaL_ref(Lua, LUA_REGISTRYINDEX);
      if (auto error = SubscribeEvent(event_id, C_FUNCTION(receive_event, client_function), &handle); error IS ERR::Okay) {
         auto prv = (prvFluid *)Lua->Script->ChildPrivate;
         prv->EventList.emplace_back(client_function, event_id, handle);
         lua_pushlightuserdata(Lua, handle); // 1: Handle
         lua_pushinteger(Lua, int(error)); // 2: Error code
      }
      else {
         lua_pushnil(Lua); // Handle
         lua_pushinteger(Lua, int(error)); // Error code
      }
      return 2;
   }
}

//********************************************************************************************************************
// Usage: msg("Message")
// Prints a debug message, with no support for input parameters.  This is the safest way to call LogF().

int fcmd_msg(lua_State *Lua)
{
   int n = lua_gettop(Lua);  // number of arguments
   lua_getglobal(Lua, "tostring");
   for (int i=1; i <= n; i++) {
      lua_pushvalue(Lua, -1);  // function to be called (tostring)
      lua_pushvalue(Lua, i);   // value to pass to tostring
      lua_call(Lua, 1, 1);
      CSTRING s = lua_tostring(Lua, -1);  // get result
      if (!s) return luaL_error(Lua, LUA_QL("tostring") " must return a string to " LUA_QL("print"));

      {
         pf::Log log("Fluid");
         log.msg("%s", s);
      }

      lua_pop(Lua, 1);  // pop the string result
   }
   return 0;
}

//********************************************************************************************************************
// Usage: print(...)
// Prints a message to stderr.  On Android stderr is unavailable, so the message is printed in the debug output.

int fcmd_print(lua_State *Lua)
{
   int n = lua_gettop(Lua);  // number of arguments
   lua_getglobal(Lua, "tostring");
   for (int i=1; i <= n; i++) {
      lua_pushvalue(Lua, -1);  // function to be called
      lua_pushvalue(Lua, i);   // value to print
      lua_call(Lua, 1, 1);
      CSTRING s = lua_tostring(Lua, -1);  // get result
      if (!s) return luaL_error(Lua, LUA_QL("tostring") " must return a string to " LUA_QL("print"));

      #ifdef __ANDROID__
         {
            pf::Log log("Fluid");
            log.msg("%s", s);
         }
      #else
         fprintf(stderr, "%s", s);
      #endif

      lua_pop(Lua, 1);  // pop result
   }
   fprintf(stderr, "\n");
   return 0;
}

//********************************************************************************************************************
// Usage: include "File1","File2","File3",...

int fcmd_include(lua_State *Lua)
{
   if (!lua_isstring(Lua, 1)) {
      luaL_argerror(Lua, 1, "Include name(s) required.");
      return 0;
   }

   int top = lua_gettop(Lua);
   for (int n=1; n <= top; n++) {
      CSTRING include = lua_tostring(Lua, n);
      if (auto error = load_include(Lua->Script, include); error != ERR::Okay) {
         if (error IS ERR::FileNotFound) luaL_error(Lua, "Requested include file '%s' does not exist.", include);
         else luaL_error(Lua, "Failed to process include file: %s", GetErrorMsg(error));
         return 0;
      }
   }

   return 0;
}

//********************************************************************************************************************
// Usage: require 'Module'
//
// Loads a Fluid language file from "scripts:" and executes it.  Differs from loadFile() in that registration
// prevents multiple executions, and the volume restriction improves security.
//
// The module can opt to return a table that represents the interface.  This allows the user to avoid namespace
// conflicts that could occur if the interface would otherwise be accessed as a global.

int fcmd_require(lua_State *Lua)
{
   auto prv = (prvFluid *)Lua->Script->ChildPrivate;

   CSTRING error_msg = nullptr;
   ERR error = ERR::Okay;
   auto module = luaL_checkstringview(Lua, 1);   
   
   // For security purposes, check the validity of the module name.

   int slash_count = 0;

   bool local = false;
   if (module.starts_with("./")) { // Local modules are permitted if the name starts with "./" and otherwise adheres to path rules
      local = true;
      module.remove_prefix(2);
   }
   
   size_t i;
   for (i=0; i < module.size(); i++) {
      if ((module[i] >= 'a') and (module[i] <= 'z')) continue;
      if ((module[i] >= 'A') and (module[i] <= 'Z')) continue;
      if ((module[i] >= '0') and (module[i] <= '9')) continue;
      if ((module[i] IS '-') or (module[i] IS '_')) continue;
      if (module[i] IS '/') { slash_count++; continue; }
      break;
   }

   if ((i < module.size()) or (i >= 96) or (slash_count > 2)) {
      luaL_error(Lua, "Invalid module name; only alpha-numeric names are permitted with max 96 chars.");
      return 0;
   }

   // Check if the module is already loaded.

   std::string modkey("require.");
   modkey.append(module);

   lua_getfield(Lua, LUA_REGISTRYINDEX, modkey.c_str());
   auto mod_value = lua_type(Lua, -1);
   if (mod_value IS LUA_TTABLE) return 1; // Return the interface originally returned by the module
   else {
      auto loaded = lua_toboolean(Lua, -1);
      lua_pop(Lua, 1);
      if (loaded) return 0;
   }

   std::string path;
   if (local) path.assign(Lua->Script->get<CSTRING>(FID_WorkingPath));
   else path.assign("scripts:");
   path.append(module);
   path.append(".fluid");

   objFile::create file = { fl::Path(path), fl::Flags(FL::READ) };

   if (file.ok()) {
      std::unique_ptr<char[]> buffer(new char[SIZE_READ]);
      struct code_reader_handle handle = { *file, buffer.get() };
      if (!lua_load(Lua, &code_reader, &handle, module.data())) {
         prv->RequireCounter++; // Used by getExecutionState()
         auto result_top = lua_gettop(Lua);
         if (!lua_pcall(Lua, 0, LUA_MULTRET, 0)) { // Success, mark the module as loaded.
            auto results = lua_gettop(Lua) - result_top + 1;
            if (results > 0) {
               // If an interface table is returned, store it with the modkey for future require calls.
               int rtype = lua_type(Lua, -1);
               if (rtype == LUA_TTABLE) {
                  lua_pushvalue(Lua, -1); // Duplicate table on stack
                  lua_setfield(Lua, LUA_REGISTRYINDEX, modkey.c_str()); // Store & pop one copy
                  // Original table remains on stack for return
                  prv->RequireCounter--;
                  return 1;
               }
               else { // Discard non-table result, mark module as loaded
                  lua_pop(Lua, 1);
                  lua_pushboolean(Lua, 1);
                  lua_setfield(Lua, LUA_REGISTRYINDEX, modkey.c_str());
               }
            }
            else { // No return value; just mark as loaded
               lua_pushboolean(Lua, 1);
               lua_setfield(Lua, LUA_REGISTRYINDEX, modkey.c_str());
            }
         }
         else error_msg = lua_tostring(Lua, -1);
         prv->RequireCounter--;
      }
      else error_msg = lua_tostring(Lua, -1);
   }
   else {
      luaL_error(Lua, "Failed to open file '%s', may not exist.", path.c_str());
      return 0;
   }

   if (error_msg) luaL_error(Lua, error_msg);
   else if (error != ERR::Okay) luaL_error(Lua, GetErrorMsg(error));

   return 0;
}

//********************************************************************************************************************
// Usage: state = getExecutionState()
//
// Returns miscellaneous information about the code's current state of execution.

int fcmd_get_execution_state(lua_State *Lua)
{
   auto prv = (prvFluid *)Lua->Script->ChildPrivate;
   lua_newtable(Lua);
   lua_pushstring(Lua, "inRequire");
   lua_pushboolean(Lua, prv->RequireCounter ? TRUE : FALSE);
   lua_settable(Lua, -3);
   return 1;
}

//********************************************************************************************************************
// Usage: results = loadFile("Path")
//
// Loads a Fluid language file from any location and executes it.  Any return values from the script will be returned
// as-is.  Any error that occurs will be thrown with a descriptive string.

int fcmd_loadfile(lua_State *Lua)
{
   CSTRING error_msg = nullptr;
   int results = 0;
   ERR error = ERR::Okay;

   if (auto path = lua_tostring(Lua, 1)) {
      pf::Log log("loadfile");

      log.branch("%s", path);

      bool recompile = false;
      CSTRING src = path;

      #if 0
      int pathlen = strlen(path);
      char fbpath[pathlen+6];
      if (iequals(".fluid", path + pathlen - 6)) {
         // File is a .fluid.  Let's check if a .fb exists and is date-stamped for the same date as the .fluid version.
         // Note: The developer can also delete the .fluid file in favour of a .fb that is already present (for
         // production releases)

         StrCopy(path, fbpath, pathlen - 5);
         StrCopy(".fb", fbpath + pathlen - 6);

         log.msg("Checking for a compiled Fluid file: %s", fbpath);

         objFile::create fb_file = { fl::Path(fbpath) };
         if (fb_file.ok()) { // A compiled version exists.  Compare datestamps
            objFile::create src_file = { fl::Path(path) };
            if (src_file.ok()) {
               LARGE fb_ts, src_ts;
               fb_file->get(FID_TimeStamp, &fb_ts);
               src_file->get(FID_TimeStamp, &src_ts);

               if (fb_ts != src_ts) {
                  log.msg("Timestamp mismatch, will recompile the cached version.");
                  recompile = true;
                  error = ERR::Failed;
               }
               else src = fbpath;
            }
            else if (error IS ERR::FileNotFound) {
               src = fbpath; // Use the .fb if the developer removed the .fluid (typically done for production releases)
            }
         }
      }
      #endif

      objFile::create file = { fl::Path(src), fl::Flags(FL::READ) };
      if (file.ok()) {
         APTR buffer;
         if (AllocMemory(SIZE_READ, MEM::NO_CLEAR, &buffer) IS ERR::Okay) {
            struct code_reader_handle handle = { *file, buffer };

            // Check for the presence of a compiled header and skip it if present

            {
               int len, i;
               char header[256];
               if (file->read(header, sizeof(header), &len) IS ERR::Okay) {
                  if (pf::startswith(LUA_COMPILED, std::string_view(header, sizeof(header)))) {
                     recompile = false; // Do not recompile that which is already compiled
                     for (i=sizeof(LUA_COMPILED)-1; (i < len) and (header[i]); i++);
                     if (!header[i]) i++;
                     else i = 0;
                  }
                  else i = 0;
               }
               else i = 0;

               file->setPosition(i);
            }

            int i;
            for (i=strlen(path); i > 0; i--) { // Get the file name from the path
               if ((path[i-1] IS '\\') or (path[i-1] IS '/') or (path[i-1] IS ':')) break;
            }

            if (!lua_load(Lua, &code_reader, &handle, path+i)) {
               // TODO Code compilation not currently supported
            /*
               if (recompile) {
                  objFile::create cachefile = {
                     fl::Path(fbpath),
                     fl::Flags(FL::NEW|FL::WRITE),
                     fl::Permissions(PERMIT::USER_READ|PERMIT::USER_WRITE)
                  };

                  if (cachefile.ok()) {
                     const Proto *f;
                     struct DateTime *date;
                     f = clvalue(Lua->top + (-1))->l.p;
                     luaU_dump(Lua, f, &code_writer, cachefile, (Self->Flags & SCF_DEBUG) ? 0 : 1);
                     if (!file.obj->getPtr(FID_Date, &date)) {
                        cachefile->setDate(date);
                     }
                  }
               }
             */

               int result_top = lua_gettop(Lua);

               if (!lua_pcall(Lua, 0, LUA_MULTRET, 0)) {
                  results = lua_gettop(Lua) - result_top + 1;
               }
               else error_msg = lua_tostring(Lua, -1);
            }
            else error_msg = lua_tostring(Lua, -1);

            FreeResource(buffer);
         }
         else error = ERR::AllocMemory;
      }
      else error = ERR::DoesNotExist;

      if ((!error_msg) and (error != ERR::Okay)) error_msg = GetErrorMsg(error);
      if (error_msg) luaL_error(Lua, "Failed to load/parse file '%s', error: %s", path, error_msg);
   }
   else luaL_argerror(Lua, 1, "File path required.");

   return results;
}

//********************************************************************************************************************
// Usage: exec(Statement)

struct luaReader {
   CSTRING String;
   int Index;
   int Size;
};

static const char * code_reader_buffer(lua_State *, void *, size_t *);

int fcmd_exec(lua_State *Lua)
{
   int results = 0;

   size_t len;
   if (auto statement = lua_tolstring(Lua, 1, &len)) {
      CSTRING error_msg = nullptr;

      {
         pf::Log log("exec");
         log.branch();

         // Check for the presence of a compiled header and skip it if present

         if (pf::startswith(LUA_COMPILED, std::string_view(statement, len))) {
            size_t i;
            for (i=sizeof(LUA_COMPILED)-1; statement[i]; i++);
            statement += i + 1;
            len -= (i + 1);
         }

         struct luaReader lr = { statement, 0, int(len) };
         if (!lua_load(Lua, &code_reader_buffer, &lr, "exec")) {
            int result_top = lua_gettop(Lua);
            if (!lua_pcall(Lua, 0, LUA_MULTRET, 0)) {
               results = lua_gettop(Lua) - result_top + 1;
            }
            else error_msg = lua_tostring(Lua, -1);
         }
         else error_msg = lua_tostring(Lua, -1);
      }

      if (error_msg) luaL_error(Lua, error_msg);
   }
   else luaL_argerror(Lua, 1, "Fluid statement required.");

   return results;
}

//********************************************************************************************************************

const char * code_reader_buffer(lua_State *Lua, void *Source, size_t *ResultSize)
{
   struct luaReader *lr = (struct luaReader *)Source;
   *ResultSize = lr->Size - lr->Index;
   lr->Index += *ResultSize;
   return lr->String;
}

//********************************************************************************************************************
// Usage: arg = arg("Width", IfNullValue)
//
// NB: Arguments are set as variables and this is managed in the base Script class.

int fcmd_arg(lua_State *Lua)
{
   objScript *Self = Lua->Script;

   int args = lua_gettop(Lua);

   auto key = lua_tostring(Lua, 1);
   auto it = Self->Vars.find(key);
   if (it != Self->Vars.end()) {
      lua_pushstring(Lua, it->second.c_str());
      return 1;
   }

   if (args IS 2) return 1; // Return value 2 (top of the stack)
   else {
      lua_pushnil(Lua);
      return 1;
   }
}

//********************************************************************************************************************
// Returns the 2nd argument if the 1st argument is evaluated as nil, zero, an empty string, table or array.  Otherwise
// the 1st argument is returned.
//
// If the 2nd argument is not given, nil is returned if the 1st argument is evaluated as being empty, otherwise 1 is
// returned.
//
// Usage: result = nz(checkval, zeroval)
//
// 'nz' is short for 'nonzero' and its use can be described as 'if checkval is non zero then return checkval, else
// return zeroval'.

int fcmd_nz(lua_State *Lua)
{
   int args = lua_gettop(Lua);
   if ((args != 2) and (args != 1)) {
      luaL_error(Lua, "Expected 1 or 2 arguments, not %d.", args);
      return 0;
   }

   BYTE isnull = FALSE;
   int type = lua_type(Lua, 1);
   if (type IS LUA_TNUMBER) {
      if (lua_tonumber(Lua, 1)) isnull = FALSE;
      else isnull = TRUE;
   }
   else if (type IS LUA_TSTRING) {
      CSTRING str;
      if ((str = lua_tostring(Lua, 1))) {
         if (str[0]) isnull = FALSE;
         else isnull = TRUE;
      }
      else isnull = TRUE;
   }
   else if ((type IS LUA_TNIL) or (type IS LUA_TNONE)) {
      isnull = TRUE;
   }
   else if ((type IS LUA_TLIGHTUSERDATA) or (type IS LUA_TUSERDATA)) {
      if (lua_touserdata(Lua, 1)) isnull = FALSE;
      else isnull = TRUE;
   }
   else if (type IS LUA_TTABLE) {
      if (lua_objlen(Lua, 1) == 0) {
         lua_pushnil(Lua);
         if (lua_next(Lua, 1)) {
            isnull = FALSE;
            lua_pop(Lua, 2); // Remove discovered value and next key
         }
         else isnull = TRUE;
      }
      else isnull = FALSE;
   }

   if (args IS 2) {
      if (isnull) { // Return value 2 (top of the stack)
      }
      else { // Return value 1
         lua_pop(Lua, 1);
      }
   }
   else if (isnull) return 0;
   else lua_pushinteger(Lua, 1);

   return 1;
}
