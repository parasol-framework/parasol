
#define PRV_SCRIPT
#define PRV_FLUID
#define PRV_FLUID_MODULE
#include <parasol/main.h>
#include <parasol/modules/fluid.h>
#include <parasol/strings.hpp>
#include <inttypes.h>
#include <mutex>
#include <algorithm>
#include <string_view>
#include <vector>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lj_obj.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_gc.h"
#include "lj_debug.h"
#include "lj_array.h"
#include "lj_trace.h"
#include "parser/parser_diagnostics.h"

#include "hashes.h"
#include "defs.h"

static int lua_load(lua_State *Lua, class objFile *File, CSTRING SourceName)
{
   std::string buffer;
   auto filesize = File->get<int>(FID_Size);
   buffer.resize(filesize);
   File->read(buffer.data(), filesize);

   return lua_load(Lua, std::string_view(buffer.data(), buffer.size()), SourceName);
}

//********************************************************************************************************************
// lua_load() failures are handled here.  At least one parser diagnostic is expected - failure to produce a
// diagnostic requires further investigation and a fix to the parser code.

[[noreturn]] static void lua_load_failed(lua_State *Lua)
{
   if (Lua->parser_diagnostics and Lua->parser_diagnostics->has_errors()) {
      std::string msg;
      for (const auto &entry : Lua->parser_diagnostics->entries()) {
         if (not msg.empty()) msg += "\n";
         msg += entry.to_string(Lua->script->LineOffset);
      }
      luaL_error(Lua, "%s", msg.c_str());
   }
   else luaL_error(Lua, "Parsing failed but no diagnostics are available.");

   //error_msg = lua_tostring(Lua, -1); <- No longer considered appropriate (legacy)
}

//********************************************************************************************************************
// Native bytecode helpers for BC_CHECK and BC_RAISE opcodes.
// These are called from VM assembly after type checking and L->CaughtError is already set.
// All three functions are noreturn - they always throw an exception.

extern "C" LJ_NORET void lj_check_raise(lua_State *L, int32_t ErrorCode)
{
   // L->CaughtError is already set by the VM
   luaL_error(L, GetErrorMsg(ERR(ErrorCode)));
}

extern "C" LJ_NORET void lj_raise_with_msg(lua_State *L, int32_t ErrorCode, GCstr *Msg)
{
   // L->CaughtError is already set by the VM
   luaL_error(L, "%s", strdata(Msg));
}

extern "C" LJ_NORET void lj_raise_default(lua_State *L, int32_t ErrorCode)
{
   // L->CaughtError is already set by the VM
   luaL_error(L, GetErrorMsg(ERR(ErrorCode)));
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
   if (not prv) return;

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
   auto prv = (prvFluid *)Lua->script->ChildPrivate;
   if (not prv) return 0;

   if (auto handle = lua_touserdata(Lua, 1)) {
      pf::Log log("unsubscribe_event");
      if ((Lua->script->Flags & SCF::LOG_ALL) != SCF::NIL) log.msg("Handle: %p", handle);

      auto erased = std::erase_if(prv->EventList, [&](const auto& event) {
         if (event.EventHandle IS handle) {
            luaL_unref(prv->Lua, LUA_REGISTRYINDEX, event.Function);
            return true;
         }
         return false;
      });

      if (erased > 0) return 0;

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
   if (not (event = lua_tostring(Lua, 1))) {
      luaL_argerror(Lua, 1, "Event string expected.");
      return 0;
   }

   if (not lua_isfunction(Lua, 2)) {
      if (not lua_isnil(Lua, 2)) {
         luaL_argerror(Lua, 2, "Function or nil expected.");
         return 0;
      }
   }

   // Generate the event ID

   constexpr size_t GROUP_BUFFER_SIZE = 60;
   uint32_t group_hash = 0, subgroup_hash = 0;
   std::string_view event_view(event);
   std::string group_str, subgroup_str;

   auto first_dot = event_view.find('.');
   if (first_dot != std::string_view::npos) {
      auto group_part = event_view.substr(0, first_dot);
      if (group_part.size() >= GROUP_BUFFER_SIZE) luaL_error(Lua, "Buffer overflow.");

      group_str = std::string(group_part);
      group_hash = strihash(group_str.c_str());
      event_view.remove_prefix(first_dot + 1);

      auto second_dot = event_view.find('.');
      if (second_dot != std::string_view::npos) {
         auto subgroup_part = event_view.substr(0, second_dot);
         if (subgroup_part.size() >= GROUP_BUFFER_SIZE) luaL_error(Lua, "Buffer overflow.");

         subgroup_str = std::string(subgroup_part);
         subgroup_hash = strihash(subgroup_str.c_str());
         event_view.remove_prefix(second_dot + 1);
         event = event_view.data();
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

   EVENTID event_id = GetEventID(group_id, subgroup_str.c_str(), event);

   if (not event_id) {
      luaL_argerror(Lua, 1, "Failed to build event ID.");
      lua_pushinteger(Lua, int(ERR::Failed));
      return 1;
   }
   else {
      APTR handle;
      lua_settop(Lua, 2);
      auto client_function = luaL_ref(Lua, LUA_REGISTRYINDEX);
      if (auto error = SubscribeEvent(event_id, C_FUNCTION(receive_event, client_function), &handle); error IS ERR::Okay) {
         auto prv = (prvFluid *)Lua->script->ChildPrivate;
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
      if (not s) luaL_error(Lua, LUA_QL("tostring") " must return a string to " LUA_QL("print"));

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
      if (not s) luaL_error(Lua, LUA_QL("tostring") " must return a string to " LUA_QL("print"));

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
// Usage: include "Module1","Module2","Module3",...
// Loads the constants for a module without the overhead of creating a module object.

int fcmd_include(lua_State *Lua)
{
   if (not lua_isstring(Lua, 1)) {
      luaL_argerror(Lua, 1, "Include name(s) required.");
      return 0;
   }

   int top = lua_gettop(Lua);
   for (int n=1; n <= top; n++) {
      CSTRING include = lua_tostring(Lua, n);

      // For security purposes, check the validity of the include name.

      int i;
      for (i=0; include[i]; i++) {
         if ((include[i] >= 'a') and (include[i] <= 'z')) continue;
         if ((include[i] >= 'A') and (include[i] <= 'Z')) continue;
         if ((include[i] >= '0') and (include[i] <= '9')) continue;
         break;
      }

      if ((include[i]) or (i >= 32)) {
         luaL_error(Lua, "Invalid module name; only alpha-numeric names are permitted with max 32 chars.");
         return 0;
      }

      if (auto error = load_include(Lua->script, include); error != ERR::Okay) {
         if (error IS ERR::FileNotFound) luaL_error(Lua, "Requested include file '%s' does not exist.", include);
         else luaL_error(Lua, "Failed to process include file: %s", GetErrorMsg(error));
         return 0;
      }
   }

   return 0;
}

//********************************************************************************************************************
// Usage: require 'ScriptFile'
//
// Loads a Fluid language file from "scripts:" and executes it.  Differs from loadFile() in that registration
// prevents multiple executions, and the volume restriction improves security.
//
// The loaded script can opt to return a table that represents the interface.  This allows the user to avoid namespace
// conflicts that could occur if the interface would otherwise be accessed as a global.

int fcmd_require(lua_State *Lua)
{
   auto prv = (prvFluid *)Lua->script->ChildPrivate;

   CSTRING error_msg = nullptr;
   ERR error = ERR::Okay;
   auto module = lua_checkstringview(Lua, 1);

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
   if (local) path.assign(Lua->script->get<CSTRING>(FID_WorkingPath));
   else path.assign("scripts:");
   path.append(module);
   path.append(".fluid");

   objFile::create file = { fl::Path(path), fl::Flags(FL::READ) };

   if (file.ok()) {
      if (not lua_load(Lua, *file, module.data())) {
         prv->RequireCounter++; // Used by getExecutionState()
         auto result_top = lua_gettop(Lua);
         if (not lua_pcall(Lua, 0, LUA_MULTRET, 0)) { // Success, mark the module as loaded.
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
      else { lua_load_failed(Lua); return 0; }
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
// Returns miscellaneous information about the code's current state of execution.  Currently this function is
// considered to be internal until such time we add anything useful for production developers.

int fcmd_get_execution_state(lua_State *Lua)
{
   auto prv = (prvFluid *)Lua->script->ChildPrivate;
   lua_newtable(Lua);
   lua_pushstring(Lua, "inRequire");
   lua_pushboolean(Lua, prv->RequireCounter ? true : false);
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
               int64_t fb_ts, src_ts;
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
         // Check for the presence of a compiled header and skip it if present

         {
            int len, i;
            char header[256];
            if (file->read(header, sizeof(header), &len) IS ERR::Okay) {
               if (pf::startswith(LUA_COMPILED, std::string_view(header, sizeof(header)))) {
                  recompile = false; // Do not recompile that which is already compiled
                  for (i=sizeof(LUA_COMPILED)-1; (i < len) and (header[i]); i++);
                  if (not header[i]) i++;
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

         // Prefix chunk name with '@' (Lua convention for file-based chunks) for better debug output
         std::string chunk_name = std::string("@") + (path + i);

         if (not lua_load(Lua, *file, chunk_name.c_str())) {
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
                  if (not file.obj->getPtr(FID_Date, &date)) {
                     cachefile->setDate(date);
                  }
               }
            }
            */

            int result_top = lua_gettop(Lua);

            if (not lua_pcall(Lua, 0, LUA_MULTRET, 0)) {
               results = lua_gettop(Lua) - result_top + 1;
            }
            else error_msg = lua_tostring(Lua, -1);
         }
         else { lua_load_failed(Lua); return 0; }
      }
      else error = ERR::DoesNotExist;

      if ((not error_msg) and (error != ERR::Okay)) error_msg = GetErrorMsg(error);
      if (error_msg) luaL_error(Lua, "Failed to load/parse file '%s', error: %s", path, error_msg);
   }
   else luaL_argerror(Lua, 1, "File path required.");

   return results;
}

//********************************************************************************************************************
// Usage: exec(Statement)
//
// Executes a string statement within a pcall.  Returns results if there are any.  An exception will be raised if an
// error occurs during statement execution.

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

         if (not lua_load(Lua, std::string_view(statement, len), "exec")) {
            int result_top = lua_gettop(Lua);
            if (not lua_pcall(Lua, 0, LUA_MULTRET, 0)) {
               results = lua_gettop(Lua) - result_top + 1;
            }
            else error_msg = lua_tostring(Lua, -1);
         }
         else { lua_load_failed(Lua); return 0; }
      }

      if (error_msg) luaL_error(Lua, error_msg);
   }
   else luaL_argerror(Lua, 1, "Fluid statement required.");

   return results;
}

//********************************************************************************************************************
// Usage: arg = arg("Width", IfNullValue)
//
// NB: Arguments are set as variables and this is managed in the base Script class.

int fcmd_arg(lua_State *Lua)
{
   objScript *Self = Lua->script;

   int args = lua_gettop(Lua);

   auto key = lua_tostring(Lua, 1);
   if (auto it = Self->Vars.find(key); it != Self->Vars.end()) {
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
// Bytecode-level try-except runtime functions.
// These are called by the BC_TRYENTER and BC_TRYLEAVE handlers and by the error unwinding system.

#include "lj_err.h"
#include "lj_func.h"
#include "lj_frame.h"
#include "lj_state.h"

//********************************************************************************************************************
// Called by BC_TRYENTER to push an exception frame onto the try stack.
//
// Parameters:
//   L              - The lua_State pointer
//   Func          - The current Lua function (passed explicitly for JIT compatibility)
//   Base          - The current base pointer (passed explicitly for JIT compatibility)
//   TryBlockIndex - Index into the function's try_blocks array
//
// Note: Both Func and Base are passed explicitly rather than computed from L->base because in JIT-compiled
// code, L->base is not synchronized with the actual base (which is kept in a CPU register).
// The interpreter passes its BASE register value. The JIT passes REF_BASE which resolves to the actual base.

extern "C" void lj_try_enter(lua_State *L, GCfunc *Func, TValue *Base, uint16_t TryBlockIndex)
{
   // Keep the entirety of this function as simple as possible - no allocations, no throwing in production.

   lj_assertL(Func != nullptr, "lj_try_enter: Func is null");
   lj_assertL(isluafunc(Func), "lj_try_enter: Func is not a Lua function");
   lj_assertL(Base >= tvref(L->stack), "lj_try_enter: Base below stack start");
   lj_assertL(Base <= tvref(L->maxstack), "lj_try_enter: Base above maxstack");

   if (L->try_stack.depth >= LJ_MAX_TRY_DEPTH) lj_err_msg(L, ErrMsg::XNEST);  // "try blocks nested too deeply"

   pf::Log log(__FUNCTION__);
   log.trace("Entering try block %u: L->base=%p, Base(VM)=%p, L->top=%p, depth=%u", TryBlockIndex, L->base, Base, L->top, L->try_stack.depth);

   // Sync L->base with the passed Base pointer.  This is critical for JIT mode where L->base may be stale (the JIT keeps the
   // base in a CPU register). If an error occurs after this call, the error handling code uses L->base to walk frames - it
   // must be valid.  Note: Do NOT modify L->top here - it was synced by the VM before this call, and modifying it would
   // truncate the live stack.

   if (L->base != Base) {
      log.detail("L->base != Base; syncing L->base for try-enter");
      L->base = Base;
   }

   ptrdiff_t frame_base_offset = savestack(L, Base);
   TValue *safe_top = L->top;
   if (safe_top < Base) safe_top = Base;
   ptrdiff_t saved_top_offset = savestack(L, safe_top);
   lj_assertL(saved_top_offset >= frame_base_offset, "lj_try_enter: saved_top below base (top=%p base=%p)", safe_top, Base);

   // Note: We leave L->top at safe_top. In JIT mode, the JIT will restore state from snapshots if needed. In
   // interpreter mode, the VM will continue with the correct top. This ensures L->top is always valid if an
   // error occurs.

   GCproto *proto = funcproto(Func); // Retrieve for try metadata
   lj_assertL(TryBlockIndex < proto->try_block_count, "lj_try_enter: TryBlockIndex %u >= try_block_count %u", TryBlockIndex, proto->try_block_count);
   lj_assertL(proto->try_blocks != nullptr, "lj_try_enter: try_blocks is null");
   TryBlockDesc *block_desc = &proto->try_blocks[TryBlockIndex];

   TryFrame *try_frame = &L->try_stack.frames[L->try_stack.depth++];
   try_frame->try_block_index = TryBlockIndex;
   try_frame->frame_base      = frame_base_offset;
   try_frame->saved_top       = saved_top_offset;
   try_frame->saved_nactvar   = BCREG(block_desc->entry_slots);
   try_frame->func            = Func;
   try_frame->depth           = (uint8_t)L->try_stack.depth;
   try_frame->flags           = block_desc->flags;
   try_frame->catch_depth     = Base - tvref(L->stack) + 2;
}

//********************************************************************************************************************
// Called by BC_TRYLEAVE to pop an exception frame from the try stack.  Note that this operation is also replicated
// in the *.dasc files when JIT optimised, so it may be shadowed.

extern "C" void lj_try_leave(lua_State *L)
{
   pf::Log(__FUNCTION__).trace("Stack Depth: %d, Base: %p, Top: %p", L->try_stack.depth, L->base, L->top);

   // NB: The setup_try_handler() also decrements the depth, so the check prevents a repeat
   if (L->try_stack.depth > 0) L->try_stack.depth--;
}

//********************************************************************************************************************
// Check if a filter matches an error code.
// PackedFilter contains up to 4 16-bit error codes packed into a 64-bit integer.
// A filter of 0 means catch-all.

static bool filter_matches(uint64_t PackedFilter, ERR ErrorCode)
{
   if (PackedFilter IS 0) return true;  // Catch-all

   // Only ERR codes at or above ExceptionThreshold can match specific filters
   if (ErrorCode < ERR::ExceptionThreshold) return false;

   // Unpack and check each 16-bit code
   for (int shift = 0; shift < 64; shift += 16) {
      uint16_t filter_code = (PackedFilter >> shift) & 0xFFFF;
      if (filter_code IS 0) break;  // No more codes in this filter
      if (filter_code IS uint16_t(ErrorCode)) return true;
   }
   return false;
}

//********************************************************************************************************************
// Find a matching handler for the given error in the current try frame.
// Returns true if a handler was found, with handler PC and exception register set.

extern "C" bool lj_try_find_handler(lua_State *L, const TryFrame *Frame, ERR ErrorCode, const BCIns **HandlerPc,
   BCREG *ExceptionReg)
{
   lj_assertL(Frame != nullptr, "lj_try_find_handler: Frame is null");
   lj_assertL(HandlerPc != nullptr, "lj_try_find_handler: HandlerPc output is null");
   lj_assertL(ExceptionReg != nullptr, "lj_try_find_handler: ExceptionReg output is null");

   GCfunc *func = Frame->func;
   lj_assertL(func != nullptr, "lj_try_find_handler: Frame->func is null");
   if (not isluafunc(func)) return false;

   GCproto *proto = funcproto(func);
   lj_assertL(proto != nullptr, "lj_try_find_handler: proto is null for Lua function");
   if (not proto->try_blocks or Frame->try_block_index >= proto->try_block_count) return false;

   const TryBlockDesc *try_block = &proto->try_blocks[Frame->try_block_index];

   // A try block with no handlers (no except clause) silently swallows exceptions
   if (try_block->handler_count IS 0) return false;

   // Only access try_handlers if there are handlers to check
   lj_assertL(proto->try_handlers != nullptr, "lj_try_find_handler: try_handlers is null but handler_count > 0");

   // Validate handler indices are within bounds
   lj_assertL(try_block->first_handler + try_block->handler_count <= proto->try_handler_count,
      "lj_try_find_handler: handler indices out of bounds (first=%u, count=%u, total=%u)",
      try_block->first_handler, try_block->handler_count, proto->try_handler_count);

   for (uint8_t index = 0; index < try_block->handler_count; ++index) {
      const TryHandlerDesc *handler = &proto->try_handlers[try_block->first_handler + index];

      if (not filter_matches(handler->filter_packed, ErrorCode)) continue;

      // Validate handler PC is within bytecode bounds
      lj_assertL(handler->handler_pc < proto->sizebc,
         "lj_try_find_handler: handler_pc %u >= sizebc %u", handler->handler_pc, proto->sizebc);

      // Found a matching handler
      *HandlerPc = proto_bc(proto) + handler->handler_pc;
      *ExceptionReg = handler->exception_reg;
      return true;
   }

   return false;
}

//********************************************************************************************************************
// Build an exception table and place it in the specified register.
// The exception table has fields: code, message, line, trace, stackTrace

extern "C" void lj_try_build_exception_table(lua_State *L, ERR ErrorCode, CSTRING Message, int Line, BCREG ExceptionReg, CapturedStackTrace *Trace)
{
   if (ExceptionReg IS 0xff) { // No exception variable - just free the trace and return
      if (Trace) lj_debug_free_trace(L, Trace);
      return;
   }

   lj_assertL(L->base >= tvref(L->stack), "lj_try_build_exception_table: L->base below stack start");
   lj_assertL(L->base <= tvref(L->maxstack), "lj_try_build_exception_table: L->base above maxstack");

   TValue *target_slot = L->base + ExceptionReg;
   lj_assertL(target_slot >= tvref(L->stack), "lj_try_build_exception_table: target slot below stack start");
   lj_assertL(target_slot < tvref(L->maxstack), "lj_try_build_exception_table: target slot at or above maxstack");

   // Create exception table and store immediately at target_slot to root it.
   // This protects it from GC during subsequent allocations without modifying L->top.

   GCtab *t = lj_tab_new(L, 0, 5);
   lj_assertL(t != nullptr, "lj_try_build_exception_table: table allocation failed");
   settabV(L, target_slot, t);  // Root immediately - don't modify L->top

   TValue *slot;

   // Set e.code

   slot = lj_tab_setstr(L, t, lj_str_newlit(L, "code"));
   if (ErrorCode >= ERR::ExceptionThreshold) setintV(slot, int(ErrorCode));
   else setnilV(slot);

   // Set e.message

   slot = lj_tab_setstr(L, t, lj_str_newlit(L, "message"));
   if (Message) setstrV(L, slot, lj_str_newz(L, Message));
   else if (ErrorCode != ERR::Okay) setstrV(L, slot, lj_str_newz(L, GetErrorMsg(ErrorCode)));
   else setstrV(L, slot, lj_str_newlit(L, "<No message>"));

   // Set e.line

   slot = lj_tab_setstr(L, t, lj_str_newlit(L, "line"));
   setintV(slot, Line);

   // NB: We do not get the "trace" and "stackTrace" slots here because subsequent allocations (lj_array_new,
   // lj_tab_new, lj_str_new) can cause table t to be rehashed, which would invalidate any slot pointers.
   // We get the slots right before storing values into them.

   if (Trace and Trace->frame_count > 0) {
      // Build native array of frame tables: [{source, line, func}, ...]
      // The array is rooted in the exception table t (at the "trace" field) after creation.
      GCarray *frames = lj_array_new(L, Trace->frame_count, AET::TABLE);
      GCRef *frame_refs = (GCRef *)frames->arraydata();

      // Build formatted traceback string at the same time
      std::string traceback = "stack traceback:";

      for (uint16_t i = 0; i < Trace->frame_count; i++) {
         CapturedFrame *cf = &Trace->frames[i];

         // Create frame table - it will be rooted in the frames array immediately
         GCtab *frame = lj_tab_new(L, 0, 3);

         // Store table reference in array first (roots it for GC)
         setgcref(frame_refs[i], obj2gco(frame));

         TValue *frame_slot = lj_tab_setstr(L, frame, lj_str_newlit(L, "source"));
         if (cf->source) setstrV(L, frame_slot, cf->source);
         else setnilV(frame_slot);

         frame_slot = lj_tab_setstr(L, frame, lj_str_newlit(L, "line"));
         setintV(frame_slot, cf->line);

         frame_slot = lj_tab_setstr(L, frame, lj_str_newlit(L, "func"));
         if (cf->funcname) setstrV(L, frame_slot, cf->funcname);
         else setnilV(frame_slot);

         lj_gc_anybarriert(L, frame);

         // Build traceback string entry
         traceback += "\n\t";
         if (cf->source) traceback += strdata(cf->source);
         else traceback += "?";

         if (cf->line > 0) {
            traceback += ":";
            traceback += std::to_string(cf->line);
         }

         if (cf->funcname) {
            traceback += ": in function '";
            traceback += strdata(cf->funcname);
            traceback += "'";
         }
      }

      // Now that all allocations are done, get the slots and store values knowing that
      // the table won't be rehashed.

      slot = lj_tab_setstr(L, t, lj_str_newlit(L, "trace"));
      setarrayV(L, slot, frames);

      // Set stackTrace string - get slot first, then create string
      // (avoids allocation window where the string would be unrooted)
      TValue *stacktrace_slot = lj_tab_setstr(L, t, lj_str_newlit(L, "stackTrace"));
      setstrV(L, stacktrace_slot, lj_str_new(L, traceback.data(), traceback.size()));

      lj_debug_free_trace(L, Trace);
   }
   else {
      // Get slots right before storing nil values
      slot = lj_tab_setstr(L, t, lj_str_newlit(L, "trace"));
      TValue *stacktrace_slot = lj_tab_setstr(L, t, lj_str_newlit(L, "stackTrace"));
      setnilV(slot);
      setnilV(stacktrace_slot);
      if (Trace) lj_debug_free_trace(L, Trace);
   }

   lj_gc_anybarriert(L, t);  // Final barrier check
   // Note: t is already stored at target_slot (done at the start)
}
